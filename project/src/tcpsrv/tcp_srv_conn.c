/******************************************************************************
 * FileName: tcp_srv_conn.c
 * TCP сервачек для ESP8266
 * pvvx ver1.0 20/12/2014
 * Перекинут на RTL871X pvvx 2017
 ******************************************************************************/
#include "user_config.h"
#include "autoconf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/memp.h"
#include "lwip/ip_addr.h"
#include "flash_eep.h"
#include "tcpsrv/tcp_srv_conn.h"
#include "rtl8195a/rtl_libc.h"
#include "esp_comp.h"

#ifdef CONFIG_DEBUG_LOG
#define DEBUGSOO 2	// уровень вывода отладочной инфы по умолчанию = 2, =1 только error
#else
#define DEBUGSOO 0
#endif
// Lwip funcs - http://www.ecoscentric.com/ecospro/doc/html/ref/lwip.html

#define TCP_SRV_BSSDATA_ATTR
#define TCP_SRV_RODATA_ATTR
#define TCP_SRV_CODE_ATTR

#define ts_printf(...) rtl_printf(__VA_ARGS__)
#define system_get_free_heap_size xPortGetFreeHeapSize

#define os_free(p) vPortFree(p)
#define os_malloc(p) pvPortMalloc(p)
#define os_zalloc(p) pvPortZalloc(p)
#define os_realloc(p,s) pvPortReAlloc(p,s)
/*
extern __rtl_memsetw_v1_00(void *, uint32, size_t);

void *pvPortZalloc( size_t xWantedSize )
{
	void *pvReturn = pvPortMalloc(xWantedSize);
	if(pvReturn != NULL) __rtl_memsetw_v1_00(pvReturn, 0, (xWantedSize + 3)>>2);
	return pvReturn;
}
*/

TCP_SERV_CFG *phcfg TCP_SRV_BSSDATA_ATTR; // = NULL; // начальный указатель в памяти на структуры открытых сервачков

#if DEBUGSOO > 0
const uint8 txt_tcpsrv_NULL_pointer[] TCP_SRV_RODATA_ATTR = "tcpsrv: NULL pointer!\n";
const uint8 txt_tcpsrv_already_initialized[] TCP_SRV_RODATA_ATTR = "tcpsrv: already initialized!\n";
const uint8 txt_tcpsrv_out_of_mem[] TCP_SRV_RODATA_ATTR = "tcpsrv: out of mem!\n";
#endif

#define mMIN(a, b)  ((a<b)?a:b)
// пред.описание...
static void tcpsrv_list_delete(TCP_SERV_CONN * ts_conn) TCP_SRV_CODE_ATTR;
static void tcpsrv_disconnect_successful(TCP_SERV_CONN * ts_conn) TCP_SRV_CODE_ATTR;
static void tcpsrv_server_close(TCP_SERV_CONN * ts_conn) TCP_SRV_CODE_ATTR;
static err_t tcpsrv_poll(void *arg, struct tcp_pcb *pcb) TCP_SRV_CODE_ATTR;
static void tcpsrv_error(void *arg, err_t err) TCP_SRV_CODE_ATTR;
static err_t tcpsrv_connected(void *arg, struct tcp_pcb *tpcb, err_t err) TCP_SRV_CODE_ATTR;
static err_t tcpsrv_client_connect(TCP_SERV_CONN * ts_conn) TCP_SRV_CODE_ATTR;
static void tcpsrv_client_reconnect(TCP_SERV_CONN * ts_conn) TCP_SRV_CODE_ATTR;

#ifndef LWIP_DEBUG
#include "lwip/init.h"
#if (LWIP_VERSION != 0x010401ff)
#error "Only LwIP version 1.4.1 !"
#endif
static const char *err_strerr[] = {
           "Ok",                    /* ERR_OK          0  */
           "Out of memory error",   /* ERR_MEM        -1  */
           "Buffer error",          /* ERR_BUF        -2  */
           "Timeout",               /* ERR_TIMEOUT    -3  */
           "Routing problem",       /* ERR_RTE        -4  */
           "Operation in progress", /* ERR_INPROGRESS -5  */
           "Illegal value",         /* ERR_VAL        -6  */
           "Operation would block", /* ERR_WOULDBLOCK -7  */
           "Address in use",        /* ERR_USE        -8  */
           "Already connected",     /* ERR_ISCONN     -9  */
           "Connection aborted",    /* ERR_ABRT       -10 */
           "Connection reset",      /* ERR_RST        -11 */
           "Connection closed",     /* ERR_CLSD       -12 */
           "Not connected",         /* ERR_CONN       -13 */
           "Illegal argument",      /* ERR_ARG        -14 */
           "Low-level netif error", /* ERR_IF         -15 */
};
#endif
static const char srvContenErrX[] = "?";
/******************************************************************************
 * FunctionName : tspsrv_error_msg
 * Description  : строка ошибки по номеру
 * Parameters   : LwIP err
 * Returns      : указатель на строку ошибки
 *******************************************************************************/
char * tspsrv_error_msg(err_t err)
{
	if((err > -16) && (err < 1)) {
		return lwip_strerr(err);
	}
	else return srvContenErrX;
}

extern const char * const tcp_state_str[];
/******************************************************************************
 * FunctionName : tspsrv_tcp_state_msg
 * Description  : строка tcp_state по номеру
 * Parameters   : LwIP tcp_state
 * Returns      : указатель на строку
 *******************************************************************************/
char * tspsrv_tcp_state_msg(enum tcp_state state)
{
	if(state > TIME_WAIT && state < CLOSED) return srvContenErrX;
	return tcp_state_str[state];
}
/******************************************************************************
 * FunctionName : tspsrv_tcp_state_msg
 * Description  : строка tcp_state по номеру
 * Parameters   : LwIP tcp_state
 * Returns      : указатель на строку
 *******************************************************************************/
static char *msg_srvconn_state[] = {
      "NONE",
      "CLOSEWAIT",
      "LISTEN",
      "CONNECT",
      "CLOSED"
};
char * tspsrv_srvconn_state_msg(enum srvconn_state state)
{
	if(state > SRVCONN_CLOSED && state < SRVCONN_NONE) return srvContenErrX;
	return msg_srvconn_state[state];
}
//#endif
/******************************************************************************
 * FunctionName : tcpsrv_print_remote_info
 * Description  : выводит remote_ip:remote_port [conn_count] ts_printf("srv x.x.x.x:x [n] ")
 * Parameters   : TCP_SERV_CONN * ts_conn
 * Returns      : none
 *******************************************************************************/
void TCP_SRV_CODE_ATTR tcpsrv_print_remote_info(TCP_SERV_CONN *ts_conn) {
//#if DEBUGSOO > 0
	uint16 port;
	if(ts_conn->pcb != NULL) port = ts_conn->pcb->local_port;
	else port = ts_conn->pcfg->port;
	ts_printf("srv[%u] %d.%d.%d.%d:%d [%d] ", port,
			ts_conn->remote_ip.b[0], ts_conn->remote_ip.b[1],
			ts_conn->remote_ip.b[2], ts_conn->remote_ip.b[3],
			ts_conn->remote_port, ts_conn->pcfg->conn_count);
//#endif
}
/******************************************************************************
 * Default call back functions
 ******************************************************************************/
//------------------------------------------------------------------------------
void TCP_SRV_CODE_ATTR tcpsrv_disconnect_calback_default(TCP_SERV_CONN *ts_conn) {
	ts_conn->pcb = NULL;
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	ts_printf("disconnect\n");
#endif
}
//------------------------------------------------------------------------------
err_t TCP_SRV_CODE_ATTR tcpsrv_listen_default(TCP_SERV_CONN *ts_conn) {
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	ts_printf("listen\n");
#endif
	return ERR_OK;
}
//------------------------------------------------------------------------------
err_t TCP_SRV_CODE_ATTR tcpsrv_connected_default(TCP_SERV_CONN *ts_conn) {
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	ts_printf("connected\n");
#endif
	return ERR_OK;
}
//------------------------------------------------------------------------------
err_t TCP_SRV_CODE_ATTR tcpsrv_sent_callback_default(TCP_SERV_CONN *ts_conn) {
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	ts_printf("sent_cb\n");
#endif
	return ERR_OK;
}
//------------------------------------------------------------------------------
err_t TCP_SRV_CODE_ATTR tcpsrv_received_data_default(TCP_SERV_CONN *ts_conn) {
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	ts_printf("received, buffer %d bytes\n", ts_conn->sizei);
#endif
	return ERR_OK;
}
/******************************************************************************
 * FunctionName : tcpsrv_check_max_tm_tcp_pcb
 * Description  : Ограничение неактивных pcb в списках lwip до MAX_TIME_WAIT_PCB
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
static void TCP_SRV_CODE_ATTR tcpsrv_check_max_tm_tcp_pcb(void)
{
	struct tcp_pcb *pcb;
	int i = 0;
	for (pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) i++;
#if DEBUGSOO > 4
	ts_printf("tcpsrv: check %d tm pcb\n", i);
#endif
	while(i-- > MAX_TIME_WAIT_PCB) {
		struct tcp_pcb *inactive = NULL;
		for (pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
			inactive = pcb;
		}
#if DEBUGSOO > 3
		ts_printf("tcpsrv: kill %d tm pcb\n", i);
#endif
		if(inactive != NULL) {
			tcp_pcb_remove(&tcp_tw_pcbs, inactive);
			memp_free(MEMP_TCP_PCB, inactive);
		}
	}
}
/******************************************************************************
 * FunctionName : find_tcp_pcb
 * Description  : поиск pcb в списках lwip
 * Parameters   : TCP_SERV_CONN * ts_conn
 * Returns      : *pcb or NULL
 *******************************************************************************/
struct tcp_pcb * TCP_SRV_CODE_ATTR find_tcp_pcb(TCP_SERV_CONN * ts_conn) {
	struct tcp_pcb *pcb = ts_conn->pcb;
	if (pcb) {
		uint16 remote_port = ts_conn->remote_port;
		uint16 local_port = ts_conn->pcfg->port;
		uint32 ip = ts_conn->remote_ip.dw;
		if ((pcb->remote_port == remote_port) && (pcb->local_port == local_port)
				&& (pcb->remote_ip.addr == ip))
			return pcb;
		for (pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
			if ((pcb->remote_port == remote_port) && (pcb->local_port == local_port)
					&& (pcb->remote_ip.addr == ip))
				return pcb;
		};
		for (pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
			if ((pcb->remote_port == remote_port) && (pcb->local_port == local_port)
					&& (pcb->remote_ip.addr == ip))
				return pcb;
		};
	}
	return NULL;
}
/******************************************************************************
 * FunctionName : tcpsrv_disconnect
 * Description  : disconnect
 * Parameters   : TCP_SERV_CONN * ts_conn
 * Returns      : none
 *******************************************************************************/
void TCP_SRV_CODE_ATTR tcpsrv_disconnect(TCP_SERV_CONN * ts_conn) {
	if (ts_conn == NULL || ts_conn->state == SRVCONN_CLOSEWAIT) return; // уже закрывается
	ts_conn->pcb = find_tcp_pcb(ts_conn); // ещё жива данная pcb ?
	if (ts_conn->pcb != NULL) tcpsrv_server_close(ts_conn);
}
/******************************************************************************
 * FunctionName : internal fun: tcpsrv_int_sent_data
 * Description  : передача данных (не буферизированная! только передача в tcp Lwip-у)
 * 				  вызывать только из call back с текущим pcb!
 * Parameters   : TCP_SERV_CONN * ts_conn
 *                uint8* psent - буфер с данными
 *                uint16 length - кол-во передаваемых байт
 * Returns      : tcp error
 ******************************************************************************/
err_t TCP_SRV_CODE_ATTR tcpsrv_int_sent_data(TCP_SERV_CONN * ts_conn, uint8 *psent, uint16 length) {
	err_t err = ERR_ARG;
	if(ts_conn == NULL) return err;
	ts_conn->pcb = find_tcp_pcb(ts_conn); // ещё жива данная pcb ?
	struct tcp_pcb *pcb = ts_conn->pcb;
	if(pcb == NULL || ts_conn->state == SRVCONN_CLOSEWAIT) return ERR_CONN;
	ts_conn->flag.busy_bufo = 1; // буфер bufo занят
	if(tcp_sndbuf(pcb) < length) {
#if DEBUGSOO > 1
		ts_printf("sent_data length (%u) > sndbuf (%u)!\n", length, tcp_sndbuf(pcb));
#endif
		return err;
	}
	if (length) {
		if(ts_conn->flag.nagle_disabled) tcp_nagle_disable(pcb);
		err = tcp_write(pcb, psent, length, 1);
		if (err == ERR_OK) {
			ts_conn->ptrtx = psent + length;
			ts_conn->cntro -= length;
			ts_conn->flag.wait_sent = 1; // ожидать завершения передачи (sent_cb)
			err = tcp_output(pcb); // передать что влезло
		} else { // ts_conn->state = SRVCONN_CLOSE;
#if DEBUGSOO > 1
			ts_printf("tcp_write(%p, %p, %u) = %d! pbuf = %u\n", pcb, psent, length, err, tcp_sndbuf(pcb));
#endif
			ts_conn->flag.wait_sent = 0;
			tcpsrv_server_close(ts_conn);
		};
	} else { // создать вызов tcpsrv_server_sent()
		tcp_nagle_enable(pcb);
		err = tcp_output(pcb); // передать пустое
	}
	ts_conn->flag.busy_bufo = 0; // буфер bufo свободен
	return err;
}
/******************************************************************************
 * FunctionName : tcpsrv_server_sent
 * Description  : Data has been sent and acknowledged by the remote host.
 * This means that more data can be sent.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 *                len -- The amount of bytes acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
 ******************************************************************************/
static err_t TCP_SRV_CODE_ATTR tcpsrv_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
	sint8 ret_err = ERR_OK;
	TCP_SERV_CONN * ts_conn = arg;
	if (ts_conn == NULL || pcb == NULL)	return ERR_ARG;
	ts_conn->state = SRVCONN_CONNECT;
	ts_conn->recv_check = 0;
	ts_conn->flag.wait_sent = 0; // блок передан
	if ((ts_conn->flag.tx_null == 0)
			&& (ts_conn->pcfg->func_sent_cb != NULL)) {
		ret_err = ts_conn->pcfg->func_sent_cb(ts_conn);
	}
	return ret_err;
}
/******************************************************************************
 * FunctionName : recv_trim_bufi
 * Description  : увеличение размера буфера приема на newadd
 * Parameters   : ts_conn, newadd - требуемое дополнение размера буфера
 * Returns      : err_t
 ******************************************************************************/
static err_t TCP_SRV_CODE_ATTR recv_trim_bufi(TCP_SERV_CONN * ts_conn, uint32 newadd)
{
	uint32 len = 0;
//	while(ts_conn->flag.busy_bufi) vTaskDelay(1);
	ts_conn->flag.busy_bufi = 1; // идет обработка bufi
	if(newadd) {
		// требуется дополнение размера буфера
		if(ts_conn->pbufi != NULL) { // уже есть какой-то буфер
			if ((ts_conn->flag.rx_buf) && ts_conn->cntri < ts_conn->sizei) { // буфер не пуст
				len = ts_conn->sizei - ts_conn->cntri; // размер необработанных данных
				if(ts_conn->cntri >= newadd) { // кол-во обработанных байт (пустого места в буфере) больше чем дополнение
					os_memcpy(ts_conn->pbufi, &ts_conn->pbufi[ts_conn->cntri], len ); // скопировать необработанную часть в начало буфера
					ts_conn->sizei = len;
					if(ts_conn->cntri != newadd) { // кол-во обработанных байт (пустого места в буфере) равно требуемому дополнению
						// уменьшение занимаемого буфера в памяти
						len += newadd; //
						ts_conn->pbufi = (uint8 *)os_realloc(ts_conn->pbufi, len + 1);	//mem_trim(ts_conn->pbufi, len);
						if(ts_conn->pbufi == NULL) {
#if DEBUGSOO > 2
							ts_printf("memtrim err %p[%d]  ", ts_conn->pbufi, len + 1);
#endif
							return ERR_MEM;
						}
#if DEBUGSOO > 2
						ts_printf("memi%p[%d] ", ts_conn->pbufi,  len);
#endif
					}
					ts_conn->pbufi[len] = '\0'; // вместо os_zalloc;
					ts_conn->cntri = 0;
					ts_conn->flag.busy_bufi = 0; // обработка bufi окончена
					return ERR_OK;
				}
			}
			else { // буфер пуст или не тот режим
				// удалить буфер
				os_free(ts_conn->pbufi);
				ts_conn->pbufi = NULL;
			}
		}
		// подготовка нового буфера, если его нет или не лезет дополнение
		uint8 * newbufi = (uint8 *) os_malloc(len + newadd + 1);
		if (newbufi == NULL) {
#if DEBUGSOO > 2
			ts_printf("memerr %p[%d]  ", newbufi, len + newadd);
#endif
			ts_conn->flag.busy_bufi = 0; // обработка bufi окончена
			return ERR_MEM;
		}
#if DEBUGSOO > 2
		ts_printf("memi%p[%d] ", newbufi,  len + newadd);
#endif
		newbufi[len + newadd] = '\0'; // вместо os_zalloc
		if(len)	{
			os_memcpy(newbufi, &ts_conn->pbufi[ts_conn->cntri], len);
			os_free(ts_conn->pbufi);
		};
		ts_conn->pbufi = newbufi;
		ts_conn->sizei = len;
	}
	else {
		// оптимизация буфера (уменьшение занимаемого размера)
		if((!ts_conn->flag.rx_buf) || ts_conn->cntri >= ts_conn->sizei)  { // буфер обработан
			ts_conn->sizei = 0;
			if (ts_conn->pbufi != NULL) {
				os_free(ts_conn->pbufi);  // освободить буфер.
				ts_conn->pbufi = NULL;
			};
		}
		else if(ts_conn->cntri) { // есть обработанные данные
			// уменьшение занимаемого размера на обработанные данные
			len = ts_conn->sizei - ts_conn->cntri;
			os_memcpy(ts_conn->pbufi, &ts_conn->pbufi[ts_conn->cntri], len );
			ts_conn->sizei = len;
			ts_conn->pbufi = (uint8 *)os_realloc(ts_conn->pbufi, len + 1);	//mem_trim(ts_conn->pbufi, len);
			if(ts_conn->pbufi == NULL) {
#if DEBUGSOO > 2
				ts_printf("memtrim err %p[%d]  ", ts_conn->pbufi, len + 1);
#endif
				ts_conn->flag.busy_bufi = 0; // обработка bufi окончена
				return ERR_MEM;
			}
			ts_conn->pbufi[len] = '\0'; // вместо os_zalloc;
		}
	}
	ts_conn->cntri = 0;
	ts_conn->flag.busy_bufi = 0; // обработка bufi окончена
	return ERR_OK;
}
/******************************************************************************
 * FunctionName : tcpsrv_server_recv
 * Description  : Data has been received on this pcb.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb which received data
 *                p -- The received data (or NULL when the connection has been closed!)
 *                err -- An error code if there has been an error receiving
 * Returns      : ERR_ABRT: if you have called tcp_abort from within the function!
 ******************************************************************************/
static err_t TCP_SRV_CODE_ATTR tcpsrv_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
	// Sets the callback function that will be called when new data arrives on the connection associated with pcb.
	// The callback function will be passed a NULL pbuf to indicate that the remote host has closed the connection.
	TCP_SERV_CONN * ts_conn = arg;
	if (ts_conn == NULL || pcb == NULL) return ERR_ARG;

//	if(syscfg.cfg.b.hi_speed_enable) set_cpu_clk();

	if (p == NULL || err != ERR_OK) { // the remote host has closed the connection.
		tcpsrv_server_close(ts_conn);
		return err;
	};
	// если нет функции обработки или ожидаем закрытия соединения, то принимаем данные в трубу
	if ((ts_conn->flag.rx_null != 0) || (ts_conn->pcfg->func_recv == NULL)
			|| (ts_conn->state == SRVCONN_CLOSEWAIT)) { // соединение закрыто? нет.
		tcp_recved(pcb, p->tot_len + ts_conn->unrecved_bytes); // сообщает стеку, что съели len и можно посылать ACK и принимать новые данные.
		ts_conn->unrecved_bytes = 0;
#if DEBUGSOO > 3
		ts_printf("rec_null %d of %d\n", ts_conn->cntri, p->tot_len);
#endif
		pbuf_free(p); // данные выели
		return ERR_OK;
	};
	ts_conn->state = SRVCONN_CONNECT; // был прием
	ts_conn->recv_check = 0; // счет времени до авто-закрытия с нуля
	if(p->tot_len) {
		err = recv_trim_bufi(ts_conn, p->tot_len); // увеличить буфер
		if(err != ERR_OK) return err;
		// добавление всего что отдал Lwip в буфер
		uint32 len = pbuf_copy_partial(p, &ts_conn->pbufi[ts_conn->sizei], p->tot_len, 0);
		ts_conn->sizei += len;
		pbuf_free(p); // все данные выели
		if(!ts_conn->flag.rx_buf) {
			 tcp_recved(pcb, len); // сообщает стеку, что съели len и можно посылать ACK и принимать новые данные.
		}
		else ts_conn->unrecved_bytes += len;
#if DEBUGSOO > 3
		ts_printf("rec %d of %d :\n", p->tot_len, ts_conn->sizei);
#endif
		err = ts_conn->pcfg->func_recv(ts_conn);
		err_t err2 = recv_trim_bufi(ts_conn, 0); // оптимизировать размер занимаемый буфером
		if(err2 != ERR_OK) return err2;
	}
	return err;
}
/******************************************************************************
 * FunctionName : tcpsrv_unrecved_win
 * Description  : Update the TCP window.
 * This can be used to throttle data reception (e.g. when received data is
 * programmed to flash and data is received faster than programmed).
 * Parameters   : TCP_SERV_CONN * ts_conn
 * Returns      : none
 * После включения throttle, будет принято до 5840 (MAX WIN) + 1460 (MSS) байт?
 ******************************************************************************/
void TCP_SRV_CODE_ATTR tcpsrv_unrecved_win(TCP_SERV_CONN *ts_conn) {
	if (ts_conn->unrecved_bytes) {
		// update the TCP window
#if DEBUGSOO > 3
		ts_printf("recved_bytes=%d\n", ts_conn->unrecved_bytes);
#endif
#if 1
		if(ts_conn->pcb != NULL) tcp_recved(ts_conn->pcb, ts_conn->unrecved_bytes);
#else
		struct tcp_pcb *pcb = find_tcp_pcb(ts_conn); // уже закрыто?
		if(pcb != NULL)	tcp_recved(ts_conn->pcb, ts_conn->unrecved_bytes);
#endif
	}
	ts_conn->unrecved_bytes = 0;
}
/******************************************************************************
 * FunctionName : tcpsrv_disconnect
 * Description  : disconnect with host
 * Parameters   : ts_conn
 * Returns      : none
 ******************************************************************************/
static void TCP_SRV_CODE_ATTR tcpsrv_disconnect_successful(TCP_SERV_CONN * ts_conn) {
	ts_conn->pcb = NULL;
	tcpsrv_list_delete(ts_conn); // remove the node from the server's connection list
}
/******************************************************************************
 * FunctionName : tcpsrv_server_close
 * Description  : The connection shall be actively closed.
 * Parameters   : ts_conn
 * Returns      : none
 ******************************************************************************/
static void TCP_SRV_CODE_ATTR tcpsrv_server_close(TCP_SERV_CONN * ts_conn) {

	struct tcp_pcb *pcb = ts_conn->pcb;
	if(pcb == NULL) {
#if DEBUGSOO > 3
		ts_printf("tcpsrv_server_close, state: %s, pcb = NULL!\n", tspsrv_srvconn_state_msg(ts_conn->state));
#endif
		return;
	}
#if DEBUGSOO > 3
	ts_printf("tcpsrv_server_close[%d], state: %s\n", pcb->local_port, tspsrv_srvconn_state_msg(ts_conn->state));
#endif
	if(ts_conn->state != SRVCONN_CLOSEWAIT && ts_conn->state != SRVCONN_CLOSED) {
#if DEBUGSOO > 2
		tcpsrv_print_remote_info(ts_conn);
		ts_printf("start close...\n");
#endif
		ts_conn->state = SRVCONN_CLOSEWAIT;
		ts_conn->recv_check = 0;
		ts_conn->flag.wait_sent = 0; // блок передан
		ts_conn->flag.rx_null = 1; // отключение вызова func_received_data() и прием в null
		ts_conn->flag.tx_null = 1; // отключение вызова func_sent_callback() и передача в null
		// отключение функций приема, передачи и poll
		tcp_recv(pcb, NULL); // отключение приема
		tcp_sent(pcb, NULL); // отключение передачи
		tcp_poll(pcb, NULL, 0); // отключение poll
		tcp_err(pcb, NULL);
		//
		if(ts_conn->unrecved_bytes) {
			tcp_recved(pcb, TCP_WND);
			ts_conn->unrecved_bytes = 0;
		}
		// освободить буфера
		if (ts_conn->pbufo != NULL) {
			os_free(ts_conn->pbufo);
			ts_conn->pbufo = NULL;
		}
		ts_conn->sizeo = 0;
		ts_conn->cntro = 0;
		if (ts_conn->pbufi != NULL)	{
			os_free(ts_conn->pbufi);
			ts_conn->pbufi = NULL;
		}
		ts_conn->sizei = 0;
		ts_conn->cntri = 0;
	}
	if(ts_conn->state == SRVCONN_CLOSEWAIT || ts_conn->state == SRVCONN_CLOSED) {
		if (pcb->state == CLOSED || pcb->state == TIME_WAIT) {
			/*remove the node from the server's active connection list*/
#if DEBUGSOO > 3
			ts_printf("close[%d]\n", pcb->local_port);
#endif
			tcpsrv_disconnect_successful(ts_conn);
		} else {
			if (ts_conn->recv_check > 3) { // счет до принудительного закрытия 3 раза по TCP_SRV_CLOSE_WAIT
#if DEBUGSOO > 1
				tcpsrv_print_remote_info(ts_conn);
				ts_printf("tcp_abandon!\n");
#endif
				tcp_poll(pcb, NULL, 0);
//?/			tcp_err(pcb, NULL);
				tcp_abandon(pcb, 0);
//				ts_conn->pcb = NULL;
				// remove the node from the server's active connection list
				tcpsrv_disconnect_successful(ts_conn);
			}
			else if (tcp_close(pcb) != ERR_OK) { // послать закрытие соединения, closing failed
#if DEBUGSOO > 1
				tcpsrv_print_remote_info(ts_conn);
				ts_printf("+ncls+[%d]\n", pcb->local_port);
#endif
				// closing failed, try again later
				tcp_poll(pcb, tcpsrv_poll, 2*(TCP_SRV_CLOSE_WAIT));
			}
			else {
#if DEBUGSOO > 3
				ts_printf("tcp_close[%d] ok.\n", pcb->local_port);
#endif
				tcpsrv_disconnect_successful(ts_conn);
			}
		}
	}
#if DEBUGSOO > 2
	else {
		tcpsrv_print_remote_info(ts_conn);
		ts_printf("already close!\n");
	}
#endif
}
/******************************************************************************
 * FunctionName : tcpsrv_poll (server and client)
 * Description  : The poll function is called every 1 second.
 * This could be increased, but we don't want to waste resources for bad connections.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
 *******************************************************************************/
static err_t TCP_SRV_CODE_ATTR tcpsrv_poll(void *arg, struct tcp_pcb *pcb) {
	TCP_SERV_CONN * ts_conn = arg;
	if (ts_conn == NULL) {
#if DEBUGSOO > 3
		ts_printf("poll, ts_conn = NULL! - abandon\n");
#endif
		tcp_poll(pcb, NULL, 0);
		tcp_err(pcb, NULL);
		tcp_abandon(pcb, 0);
		return ERR_ABRT;
	}
#if DEBUGSOO > 3
	tcpsrv_print_remote_info(ts_conn);
	ts_printf("poll: %d %s#%s, %d,%d, pcb:%p\n", ts_conn->recv_check, tspsrv_srvconn_state_msg(ts_conn->state), tspsrv_tcp_state_msg(pcb->state), ts_conn->pcfg->time_wait_rec, ts_conn->pcfg->time_wait_cls), pcb;
#endif
	if (ts_conn->pcb != NULL && ts_conn->state != SRVCONN_CLOSEWAIT) {
		ts_conn->recv_check++;
		if (pcb->state == ESTABLISHED) {
			if ((ts_conn->state == SRVCONN_LISTEN
			&& (ts_conn->pcfg->time_wait_rec)
			&& ts_conn->recv_check > ts_conn->pcfg->time_wait_rec)
			|| (ts_conn->state == SRVCONN_CONNECT
			&& (ts_conn->pcfg->time_wait_cls)
			&& ts_conn->recv_check > ts_conn->pcfg->time_wait_cls)) {
				tcpsrv_server_close(ts_conn);
			}
		}
		else tcpsrv_server_close(ts_conn);
	}
	else tcpsrv_server_close(ts_conn);
#ifdef SRV_WDGREFESH_IN_POOL
	WDGRefresh();
#endif
	return ERR_OK;
}
/******************************************************************************
 * FunctionName : tcpsrv_list_delete
 * Description  : remove the node from the connection list
 * Parameters   : ts_conn
 * Returns      : none
 *******************************************************************************/
static void TCP_SRV_CODE_ATTR tcpsrv_list_delete(TCP_SERV_CONN * ts_conn) {
//	if (ts_conn == NULL) return;
	if(ts_conn->state != SRVCONN_CLOSED) {
		ts_conn->state = SRVCONN_CLOSED; // исключить повторное вхождение из запросов в func_discon_cb()
		if (ts_conn->pcfg->func_discon_cb != NULL)
			ts_conn->pcfg->func_discon_cb(ts_conn);
		if(phcfg == NULL || ts_conn->pcfg == NULL) return;  // если в func_discon_cb() было вызвано tcpsrv_close_all() и т.д.
	}
#if DEBUGSOO > 3
	ts_printf("tcpsrv_list_delete (%p)\n", ts_conn->pcb);
#endif
	TCP_SERV_CONN ** p = &ts_conn->pcfg->conn_links;
	TCP_SERV_CONN *tcpsrv_cmp = ts_conn->pcfg->conn_links;
	while (tcpsrv_cmp != NULL) {
		if (tcpsrv_cmp == ts_conn) {
			*p = ts_conn->next;
			ts_conn->next = NULL;
			ts_conn->pcfg->conn_count--;
			if (ts_conn->linkd != NULL) {
				os_free(ts_conn->linkd);
				ts_conn->linkd = NULL;
			}
			if (ts_conn->pbufo != NULL) {
				os_free(ts_conn->pbufo);
				ts_conn->pbufo = NULL;
			}
			if (ts_conn->pbufi != NULL) {
				os_free(ts_conn->pbufi);
				ts_conn->pbufi = NULL;
			}
			os_free(ts_conn);
			return; // break;
		}
		p = &tcpsrv_cmp->next;
		tcpsrv_cmp = tcpsrv_cmp->next;
	};
}
//-----------------------------------------------------------------------------
static void tspsrv_delete_pcb(TCP_SERV_CONN * ts_conn)
{
	struct tcp_pcb *pcb = find_tcp_pcb(ts_conn);
	if(pcb) {
		tcp_arg(pcb, NULL);
		tcp_recv(pcb, NULL);
		tcp_err(pcb, NULL);
		tcp_poll(pcb, NULL, 0);
		tcp_sent(pcb, NULL);
		tcp_recved(pcb, TCP_WND);
		tcp_close(pcb);
		ts_conn->pcb = NULL;
	}
	tcpsrv_list_delete(ts_conn);
}
/******************************************************************************
 * FunctionName : tcpsrv_error (server and client)
 * Description  : The pcb had an error and is already deallocated.
 *		The argument might still be valid (if != NULL).
 * Parameters   : arg -- Additional argument to pass to the callback function
 *		err -- Error code to indicate why the pcb has been closed
 * Returns      : none
 *******************************************************************************/
static void TCP_SRV_CODE_ATTR tcpsrv_error(void *arg, err_t err) {
	TCP_SERV_CONN * ts_conn = arg;
//	struct tcp_pcb *pcb = NULL;
	if (ts_conn != NULL) {
#if DEBUGSOO > 2
		tcpsrv_print_remote_info(ts_conn);
		ts_printf("error %d (%s)\n", err, tspsrv_error_msg(err));
#elif DEBUGSOO > 1
		tcpsrv_print_remote_info(ts_conn);
#ifdef LWIP_DEBUG
		ts_printf("error %d (%s)\n", err, tspsrv_error_msg(err));
#else
		ts_printf("error %d\n", err);
#endif
#endif
		if (ts_conn->state != SRVCONN_CLOSEWAIT) {
			if(ts_conn->pcb != NULL) {
//					&& ts_conn->state != SRVCONN_CLOSED) {
//					&& (ts_conn->state != SRVCONN_CONNECT || ts_conn->state == SRVCONN_LISTEN)) {
#if DEBUGSOO > 1
				ts_printf("eclose[%d]\n", ts_conn->pcfg->port);
#endif
				tcpsrv_list_delete(ts_conn); // remove the node from the server's connection list
			};
		};
	}
}
/******************************************************************************
 * FunctionName : tcpsrv_tcp_accept
 * Description  : A new incoming connection has been accepted.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *		pcb -- The connection pcb which is accepted
 *		err -- An unused error code, always ERR_OK currently
 * Returns      : acception result
 *******************************************************************************/
static err_t TCP_SRV_CODE_ATTR tcpsrv_server_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
	struct tcp_pcb_listen *lpcb = (struct tcp_pcb_listen*) arg;
	TCP_SERV_CFG *p = tcpsrv_server_port2pcfg(pcb->local_port);
	TCP_SERV_CONN * ts_conn;
	if (p == NULL)	return ERR_ARG;
	if (system_get_free_heap_size() < p->min_heap) {
#if DEBUGSOO > 1
		ts_printf("srv[%u] new listen - low heap size!\n", p->port);
#endif
		return ERR_MEM;
	}
	if (p->conn_count >= p->max_conn) {
		if(p->flag.srv_reopen) {
#if DEBUGSOO > 1
			ts_printf("srv[%u] reconnect!\n", p->port);
#endif
			if (p->conn_links != NULL) {
				tspsrv_delete_pcb(p->conn_links);
			};
		}
		else {
#if DEBUGSOO > 1
			ts_printf("srv[%u] new listen - max connection!\n", p->port);
#endif
			return ERR_CONN;
		}
	}
	ts_conn = (TCP_SERV_CONN *) os_zalloc(sizeof(TCP_SERV_CONN));
	if (ts_conn == NULL) {
#if DEBUGSOO > 1
		ts_printf("srv[%u] new listen - out of mem!\n", ts_conn->pcfg->port);
#endif
		return ERR_MEM;
	}
	ts_conn->pcfg = p;
	// перенести флаги по умолчанию на данное соединение
	ts_conn->flag = p->flag;
	tcp_accepted(lpcb); // Decrease the listen backlog counter
	//  tcp_setprio(pcb, TCP_PRIO_MIN); // Set priority ?
	// init/copy data ts_conn
	ts_conn->pcb = pcb;
	ts_conn->remote_port = pcb->remote_port;
	ts_conn->remote_ip.dw = pcb->remote_ip.addr;
//	*(uint16 *) &ts_conn->flag = 0; //zalloc
//	ts_conn->recv_check = 0; //zalloc
//  ts_conn->linkd = NULL; //zalloc
	// Insert new ts_conn
	ts_conn->next = ts_conn->pcfg->conn_links;
	ts_conn->pcfg->conn_links = ts_conn;
	ts_conn->pcfg->conn_count++;
	ts_conn->state = SRVCONN_LISTEN;
	// Tell TCP that this is the structure we wish to be passed for our callbacks.
	tcp_arg(pcb, ts_conn);
	// Set up the various callback functions
	tcp_err(pcb, tcpsrv_error);
	tcp_sent(pcb, tcpsrv_server_sent);
	tcp_recv(pcb, tcpsrv_server_recv);
	tcp_poll(pcb, tcpsrv_poll, 2); /* every 1/2 seconds */
#if MEMP_MEM_MALLOC // tcp_alloc() уже управляет убийством TIME_WAIT если MEMP_MEM_MALLOC == 0
	if(ts_conn->flag.pcb_time_wait_free) tcpsrv_check_max_tm_tcp_pcb();
	// http://www.serverframework.com/asynchronousevents/2011/01/time-wait-and-its-design-implications-for-protocols-and-scalable-servers.html
#endif
	if (p->func_listen != NULL)
		return p->func_listen(ts_conn);
	return ERR_OK;
}
/******************************************************************************
 * FunctionName : tcpsrv_server_port2pcfg
 * Description  : поиск конфига servera по порту
 * Parameters   : номер порта
 * Returns      : указатель на TCP_SERV_CFG или NULL
 *******************************************************************************/
TCP_SERV_CFG * TCP_SRV_CODE_ATTR tcpsrv_server_port2pcfg(uint16 portn) {
	TCP_SERV_CFG * p;
	for (p = phcfg; p != NULL; p = p->next)
		if (p->port == portn) return p;
	return NULL;
}
/******************************************************************************
 tcpsrv_init server.
 *******************************************************************************/
TCP_SERV_CFG * TCP_SRV_CODE_ATTR tcpsrv_init(uint16 portn) {
	//	if (portn == 0)	portn = 80;
	if (portn == 0)	return NULL;
	TCP_SERV_CFG * p;
	for (p = phcfg; p != NULL; p = p->next) {
		if (p->port == portn) {
#if DEBUGSOO > 0
		ts_printf(txt_tcpsrv_already_initialized);
#endif
			return NULL;
		}
	}
	p = (TCP_SERV_CFG *) os_zalloc(sizeof(TCP_SERV_CFG));
	if (p == NULL) {
#if DEBUGSOO > 0
		ts_printf(txt_tcpsrv_out_of_mem);
#endif
		return NULL;
	}
	p->port = portn;
	p->conn_count = 0;
	p->min_heap = TCP_SRV_MIN_HEAP_SIZE;
	p->time_wait_rec = TCP_SRV_RECV_WAIT;
	p->time_wait_cls = TCP_SRV_END_WAIT;
	// p->phcfg->conn_links = NULL; // zalloc
	// p->pcb = NULL; // zalloc
	// p->lnk = NULL; // zalloc
	p->max_conn = TCP_SRV_MAX_CONNECTIONS;
	p->func_listen = tcpsrv_listen_default;
	p->func_discon_cb = tcpsrv_disconnect_calback_default;
	p->func_sent_cb = tcpsrv_sent_callback_default;
	p->func_recv = tcpsrv_received_data_default;
	return p;
}
/******************************************************************************
 tcpsrv_start
 *******************************************************************************/
err_t TCP_SRV_CODE_ATTR tcpsrv_start(TCP_SERV_CFG *p) {
	err_t err = ERR_OK;
	if (p == NULL) {
#if DEBUGSOO > 0
		ts_printf(txt_tcpsrv_NULL_pointer);
#endif
		return ERR_ARG;
	}
	if (p->pcb != NULL) {
#if DEBUGSOO > 0
		ts_printf(txt_tcpsrv_already_initialized);
#endif
		return ERR_USE;
	}
	p->pcb = tcp_new();

	if (p->pcb != NULL) {
		tcp_setprio(p->pcb, TCP_SRV_PRIO);
		err = tcp_bind(p->pcb, IP_ADDR_ANY, p->port); // Binds pcb to a local IP address and port number.
		if (err == ERR_OK) { // If another connection is bound to the same port, the function will return ERR_USE, otherwise ERR_OK is returned.
			p->pcb = tcp_listen(p->pcb); // Commands pcb to start listening for incoming connections.
			// When an incoming connection is accepted, the function specified with the tcp_accept() function
			// will be called. pcb must have been bound to a local port with the tcp_bind() function.
			// The tcp_listen() function returns a new connection identifier, and the one passed as an
			// argument to the function will be deallocated. The reason for this behavior is that less
			// memory is needed for a connection that is listening, so tcp_listen() will reclaim the memory
			// needed for the original connection and allocate a new smaller memory block for the listening connection.
			// tcp_listen() may return NULL if no memory was available for the listening connection.
			// If so, the memory associated with pcb will not be deallocated.
			if (p->pcb != NULL) {
				tcp_arg(p->pcb, p->pcb);
				// insert new tcpsrv_config
				p->next = phcfg;
				phcfg = p;
				// initialize callback arg and accept callback
				tcp_accept(p->pcb, tcpsrv_server_accept);
				return err;
			}
		}
		tcp_abandon(p->pcb, 0);
		p->pcb = NULL;
	} else
		err = ERR_MEM;
#if DEBUGSOO > 0
		ts_printf("tcpsrv: not new tcp!\n");
#endif
	return err;
}
/******************************************************************************
 tcpsrv_close
 *******************************************************************************/
err_t TCP_SRV_CODE_ATTR tcpsrv_close(TCP_SERV_CFG *p) {
	if (p == NULL) {
#if DEBUGSOO > 0
		ts_printf(txt_tcpsrv_NULL_pointer);
#endif
		return ERR_ARG;
	};
	TCP_SERV_CFG **pwr = &phcfg;
	TCP_SERV_CFG *pcmp = phcfg;
	while (pcmp != NULL) {
		if (pcmp == p) {
			*pwr = p->next;
			while (p->conn_links != NULL) {
				tspsrv_delete_pcb(p->conn_links);
			};
			if(p->pcb != NULL) tcp_close(p->pcb);
			os_free(p);
			p = NULL;
			return ERR_OK; // break;
		}
		pwr = &pcmp->next;
		pcmp = pcmp->next;
	};
#if DEBUGSOO > 2
	ts_printf("tcpsrv: srv_cfg not find!\n");
#endif
	return ERR_CONN;
}
/******************************************************************************
 tcpsrv_close_port
 *******************************************************************************/
err_t TCP_SRV_CODE_ATTR tcpsrv_close_port(uint16 portn)
{
	if(portn) return tcpsrv_close(tcpsrv_server_port2pcfg(portn));
	else return ERR_ARG;
}
/******************************************************************************
 tcpsrv_close_all_tcp_pcb
 *******************************************************************************/
int TCP_SRV_CODE_ATTR tcpsrv_close_all_tcp_pcb(void)
{
	struct tcp_pcb *pcb;
	int ret = 0;
	for (pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
#if DEBUGSOO > 3
		ts_printf("tcpsrv: abort %p pcb\n", pcb);
#endif
		tcp_abort(pcb);
		ret = 1;
	}
	return ret;
}
/******************************************************************************
 tcpsrv_delete_all_tm_tcp_pcb
 *******************************************************************************/
void TCP_SRV_CODE_ATTR tcpsrv_delete_all_tm_tcp_pcb(void)
{
	struct tcp_pcb *pcb;
	for (pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
#if DEBUGSOO > 3
		ts_printf("tcpsrv: del tm %p pcb\n", pcb);
#endif
		tcp_pcb_remove(&tcp_tw_pcbs, pcb);
		memp_free(MEMP_TCP_PCB, pcb);
	}
}
/******************************************************************************
 tcpsrv_close_all
 *******************************************************************************/
err_t TCP_SRV_CODE_ATTR tcpsrv_close_all(void)
{
	err_t err = ERR_OK;
	while(phcfg != NULL && err == ERR_OK) err = tcpsrv_close(phcfg);
	if(tcpsrv_close_all_tcp_pcb()) vTaskDelay(50);
	tcpsrv_delete_all_tm_tcp_pcb();
	return err;
}

