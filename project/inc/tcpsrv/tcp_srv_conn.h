#ifndef __TCP_SERV_CONN_H__
/***********************************
 * FileName: tcp_srv_conn.h
 * Tcp сервачек для ESP8266
 * PV` ver1.0 20/12/2014
 * PV` ver1.0 29/10/2016 для RTL87xx
 ***********************************/
#define __TCP_SERV_CONN_H__

#include "user_config.h"
//#include "os_type.h"

#include "lwip/err.h"

#define mMIN(a, b)  ((a < b)? a : b)

enum srvconn_state {
    SRVCONN_NONE =0,
    SRVCONN_CLOSEWAIT,  // ожидает закрытия
    SRVCONN_LISTEN, // соединение открыто, ждет rx или tx
    SRVCONN_CONNECT, // соединение открыто, было rx или tx
    SRVCONN_CLOSED // соединение закрыто
};

// приоритет pcb 1..127 1 - min, 127 - max
#ifndef TCP_SRV_PRIO
#define TCP_SRV_PRIO 99 // TCP_PRIO_MIN
#endif

// максимальное кол-во TCP c TIME_WAIT
#ifndef MAX_TIME_WAIT_PCB
#define  MAX_TIME_WAIT_PCB 10
#endif

// кол-во одновременно открытых соединений по умолчанию
#ifndef TCP_SRV_MAX_CONNECTIONS
 #define TCP_SRV_MAX_CONNECTIONS  mMIN(MEMP_NUM_TCP_PCB, 10)
#endif

// порт сервера по умолчанию
#ifndef TCP_SRV_SERVER_PORT
 #define TCP_SRV_SERVER_PORT 80
#endif


#define SRV_WDGREFESH_IN_POOL // использовать WDGRefresh() в tcpsrv_poll()

// время (сек), по умолчанию, ожидания запроса (передачи пакета) от клиента, до авто-закрытия соединения,
// при = 0 заменяется на эти 5 сек.
#ifndef TCP_SRV_RECV_WAIT
 #define TCP_SRV_RECV_WAIT  5
#endif
// время (сек), по умолчанию, до авто-закрытия соединения после приема или передачи,
// при = 0 заменяется на эти 5 сек.
#ifndef TCP_SRV_END_WAIT
 #define TCP_SRV_END_WAIT  5
#endif

// время (в сек) до повтора закрытия соединения (исполняется до 3-х раз).
#define TCP_SRV_CLOSE_WAIT 5 // 5 сек

// минимальный размер heap по умолчанию, при открытии нового соединения, при = 0 заменяется на это:
#define TCP_SRV_MIN_HEAP_SIZE 32768  // самый минимум от 6Kb

// максимальный размер выделяемого буфера в heap для приема порции
#ifndef TCP_SRV_SERVER_MAX_RXBUF
 #define TCP_SRV_SERVER_MAX_RXBUF (TCP_MSS*3) // 1460*2=2920, 1460*3=4380, 1460*4=5840
#endif

// размер выделяемого буфера в heap для передачи при буферизированном выводе
#ifndef TCP_SRV_SERVER_DEF_TXBUF
 #define TCP_SRV_SERVER_DEF_TXBUF (TCP_MSS*3) // 1460*2=2920, 1460*3=4380, 1460*4=5840
#endif

#define ID_CLIENTS_PORT 3 // до 3-х clients
#define tcpsrv_init_client1() tcpsrv_init(1) // tcp2uart_client
#define tcpsrv_init_client2() tcpsrv_init(2) // mdb_tcp_client
#define tcpsrv_init_client3() tcpsrv_init(3)

#define TCP_CLIENT_NEXT_CONNECT_S		5 // syscfg.tcp_client_twait // 5000 // через 5 сек
#define TCP_CLIENT_MAX_CONNECT_RETRY	7 // до 7 раз с интервалом TCP_CLIENT_NEXT_CONNECT_MS

//--------------------------------------------------------------------------
// Структура соединения
//
typedef struct t_tcpsrv_conn_flags  {
	uint16 srv_reopen:			1; //0001 открытие нового соединения более max_conn ведет к закрытию наиболее старого соединения.
	uint16 pcb_time_wait_free:	1; //0002 проверка на макс кол-во и уничтожение pcb с TIME_WAIT при вызове disconnect() (иначе pcb TIME_WAIT 60 сек http://www.serverframework.com/asynchronousevents/2011/01/time-wait-and-its-design-implications-for-protocols-and-scalable-servers.html)
	uint16 nagle_disabled: 		1; //0004 выключение nagle
	uint16 rx_buf: 				1; //0008 прием в буфер, используется ручное управление размером окна TCP
	uint16 rx_null:				1; //0010 отключение вызова func_received_data() и прием в null (устанавливается автоматически при вызове tcpsrv_disconnect())
	uint16 tx_null:				1; //0020 отключение вызова func_sent_callback() и передача в null (устанавливается автоматически при вызове tcpsrv_disconnect())
	uint16 wait_sent:			1; //0040 ожидет завершения/подтверждения передачи от lwip
	uint16 busy_bufo:			1; //0080 идет обработка bufo
	uint16 busy_bufi:			1; //0100 идет обработка bufi
	uint16 tmp0:				1; //0200 резерв
	uint16 tmp1:				1; //0400 резерв
	// далее идут биты не относящиеся к работе tcp_srv_conn
	uint16 user_flg1:			1; //0800 для нужд процедур уровнем выше (пока свободен)
	uint16 user_flg2:			1; //1000 для нужд процедур уровнем выше (пока свободен)
	uint16 user_option1:		1; //2000 для нужд процедур обработки переменных (использован для hexdump, xml_mdb в web_int_callbacks.c)
	uint16 user_option2:		1; //4000 для нужд процедур обработки переменных (использован для xml_mdb в web_int_callbacks.c)
} __attribute__((packed)) tcpsrv_conn_flags;

typedef struct t_TCP_SERV_CONN {
	volatile tcpsrv_conn_flags flag;//+0 флаги соеднения
	enum srvconn_state state;		//+4 состояние
	struct t_TCP_SERV_CFG *pcfg;  	//+8 указатель на базовую структуру сервера
	uint16 recv_check;   			//+12 счет тиков соединения в tcpsrv_poll
	uint16 remote_port;  			//+16 номер порта клиента
	union {              			//+20 ip клиента
	  uint32 dw;
	  uint8  b[4];
	} remote_ip;
	struct t_TCP_SERV_CONN *next; 	//+24 указатель на следующую структуру
	struct tcp_pcb *pcb; 			//+28 указатель на pcb в Lwip
	uint8 *pbufo; 					//+32 указатель на сегмент с передаваемыми данными
	uint8 *ptrtx; 					//+36 указатель на ещё не переданные данные
	uint8 *pbufi;   				//+40 указатель на сегмент буфера с принимаемыми данными
	uint16 sizeo; 					//+44 размер буфера передачи
	uint16 sizei; 					//+48 размер приемного буфера (кол-во принятых и ещё не обработанных байт)
	uint16 cntro; 					//+52 кол-во обработанных байт в буфере передачи
	uint16 cntri; 					//+56 кол-во обработанных байт в буфере приема
	uint16 unrecved_bytes; 			//+60 используется при ручном управлении TCP WIN / This can be used to throttle data reception
	// далее идут переменные не относящиеся к работе tcp_srv_conn
	uint8 *linkd; 					//+64 указатель на прилепленные данные пользователя (при закрытии соединения вызывается os_close(linkd), если linkd != NULL;
} TCP_SERV_CONN;


//--------------------------------------------------------------------------
// Вызываемые функции пользователя (calback-и)
//
typedef void (*func_disconect_calback)(TCP_SERV_CONN *ts_conn); // соединение закрыто
typedef err_t (*func_listen)(TCP_SERV_CONN *ts_conn); // новый клиент
typedef err_t (*func_received_data)(TCP_SERV_CONN *ts_conn); // принято всего ts_conn->sizei байт, лежат в буфере по ts_conn->pbufi, по выходу принимается обработанных ts_conn->cntri;
typedef err_t (*func_sent_callback)(TCP_SERV_CONN *ts_conn); // блок данных передан

//--------------------------------------------------------------------------
// Структура конфигурации tcp сервера
//
typedef struct t_TCP_SERV_CFG {
	    struct t_tcpsrv_conn_flags flag;	// начальные флаги для соединения
        uint16 port;						// номер порта
        uint16 min_heap;					// минимальный размер heap при открытии нового соединения, при = 0 заменяется на 8192.
        uint8 max_conn;						// максимальное кол-во одновременных соединений
        uint8 conn_count;					// кол-во текущих соединений, при инициализации прописывает 0
        uint8 time_wait_rec;				// время (сек) ожидания запроса (передачи пакета) от клиента, до авто-закрытия соединения, по умолчанию TCP_SRV_RECV_WAIT сек.
        uint8 time_wait_cls;				// время (сек) до авто-закрытия соединения после приема или передачи, по умолчанию TCP_SRV_END_WAIT сек.
        TCP_SERV_CONN * conn_links;			// указатель на цепочку активных соединений, при инициализации или отсуствии активных соединений = NULL
        struct tcp_pcb *pcb;				// начальный pcb [LISTEN] если сервер, иначе NULL
        func_disconect_calback func_discon_cb;	// функция вызываемая после закрытия соединения, если = NULL - не вызывается
        func_listen func_listen;			// функция вызываемая при присоединении клиента или коннекта к серверу, если = NULL - не вызывается
        func_sent_callback func_sent_cb;	// функция вызываемая после передачи данных или наличию места в ip стеке для следушей передачи данных, если = NULL - не вызывается (+см. флаги)
        func_received_data func_recv;		// функция вызываемая при приеме данных, если = NULL - не вызывается (+см. флаги)
        struct t_TCP_SERV_CFG *next;		// следующий экземпляр структуры сервера/клиента
}TCP_SERV_CFG;
//--------------------------------------------------------------------------
// Данные
//
extern TCP_SERV_CFG *phcfg; // указатель на цепочку TCP_SERV_CFG (стартовавших серверов)
//--------------------------------------------------------------------------
// Функции
//
err_t tcpsrv_int_sent_data(TCP_SERV_CONN * ts_conn, uint8 *psent, uint16 length); // передать length байт (внутрення функция - никаких проверок)
void tcpsrv_disconnect(TCP_SERV_CONN * ts_conn); // закрыть соединение
void tcpsrv_print_remote_info(TCP_SERV_CONN *ts_conn); // выводит remote_ip:remote_port [conn_count] os_printf("srv x.x.x.x:x [n] ")
TCP_SERV_CFG * tcpsrv_server_port2pcfg(uint16 portn); // поиск структуры конфига по номеру порта
void tcpsrv_unrecved_win(TCP_SERV_CONN *ts_conn); // Восстановить размер TCP WIN, если используется ручное управление размером окна TCP

void tcpsrv_disconnect_calback_default(TCP_SERV_CONN *ts_conn);
err_t tcpsrv_listen_default(TCP_SERV_CONN *ts_conn);
err_t tcpsrv_sent_callback_default(TCP_SERV_CONN *ts_conn);
err_t tcpsrv_received_data_default(TCP_SERV_CONN *ts_conn);

TCP_SERV_CFG *tcpsrv_init(uint16 portn);
err_t tcpsrv_start(TCP_SERV_CFG *p);
err_t tcpsrv_close(TCP_SERV_CFG *p);
err_t tcpsrv_close_port(uint16 portn);
err_t tcpsrv_close_all(void);

char * tspsrv_error_msg(err_t err);
char * tspsrv_tcp_state_msg(enum tcp_state state);
char * tspsrv_srvconn_state_msg(enum srvconn_state state);

#endif // __TCP_SERV_CONN_H__
