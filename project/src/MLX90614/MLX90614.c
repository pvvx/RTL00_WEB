/*
 * MLX90614drv.c
 *  https://esp8266.ru/forum/threads/i2c-na-rtl.2450/
 *  Created on: 02/09/2017
 *      Author: pvvx
 */
#include <platform_opts.h>
#include "rtl8195a.h"
#include "device.h"
#include "PinNames.h"

#include "basic_types.h"
#include "diag.h"
#include "osdep_api.h"
#include "timer_api.h"

//#include "i2c_api.h"
//#include "i2c_ex_api.h"
#include "pinmap.h"
#include "rtl_lib.h"
#include "rtl8195a/rtl_libc.h"
#include "driver/i2c_drv.h"
#include "platform_stdlib.h"
#include "web_websocket.h"
#include "tcpsrv/tcp_srv_conn.h"

#include "hal_com_reg.h"

#define MLX90614_I2CADDR 0x5A
// RAM
#define MLX90614_RAWIR1 0x04
#define MLX90614_RAWIR2 0x05
#define MLX90614_TA 0x06
#define MLX90614_TOBJ1 0x07
#define MLX90614_TOBJ2 0x08
// EEPROM
#define MLX90614_TOMAX 0x20
#define MLX90614_TOMIN 0x21
#define MLX90614_PWMCTRL 0x22
#define MLX90614_TARANGE 0x23
#define MLX90614_EMISS 0x24
#define MLX90614_CONFIG 0x25
#define MLX90614_ADDR 0x0E
#define MLX90614_ID1 0x3C
#define MLX90614_ID2 0x3D
#define MLX90614_ID3 0x3E
#define MLX90614_ID4 0x3F
#define MLX90614_SLEEP 0xFF

typedef struct _MLX90614_data {
	union {
		signed short to1; 		// Tobj1
		unsigned char 	uc[2];
	} o;
	union {
		signed short ta;		// Ta
		unsigned char 	uc[2];
	} a;
} MLX90614DATA, *PMLX90614DATA;

typedef struct _MLX90614drv {
	unsigned char status;
	unsigned char addr;			// адрес MLX90614 на шине I2C (формат 7 bit)
	signed char init;
	unsigned char tmp;

	unsigned short config;		// регистр конфигурации MLX90614
	unsigned short timetik;		// время опроса таймером (в us)

	unsigned short count;		// счетчик считанных значений
	unsigned short overrun;		// счет переполнений буфера

	unsigned short errs;		// счет ошибок на I2C
	unsigned short buf_idx;		// объем буфера pbuf[buf_idx+1], максимальный индекс-номер замера
	unsigned short buf_rx;		// индекс-номер ещё не считанного замера
	unsigned short buf_tx;		// индекс-номер для записи следующего замера
	PMLX90614DATA pbuf;
	gtimer_t timer;
	union {						// буфер
		unsigned char 	uc[4];
		unsigned short	us[2];
		signed short	ss[2];
		unsigned int	ui;
	} buf_i2c;
	i2c_drv_t   i2c;
} MLX90614DRV, *PMLX90614DRV;


#define MLX90614_I2C_PIN_SDA	PC_4
#define MLX90614_I2C_PIN_SCL	PC_5
#define MLX90614_I2C_BUS_CLK	50000  // The maximum frequency of the MLX90614 SMBus is 100 KHz and the minimum is 10 KHz.
#define MLX90614_TIMER TIMER3	// используемый таймер

MLX90614DRV mlx90614drv = {
		.addr = MLX90614_I2CADDR,
		.timetik = 10000,		// Время опроса таймером: 10000 us
		.buf_idx = 709,			// циклический буфер на 710 замеров (по 4 байт -> sizeof(MLX90614DATA))
								// Если шаг заполнения 1 ms -> буфер на 0.71 сек
								// Оптимизация под TCP: (TCP_MSS*2 - 80)/4 = (1460*2 - 80)/4 = 710
		.i2c.status = DRV_I2C_OFF,
		.i2c.idx = 1,				// I2C1
		.i2c.io_sel = S0,			// PC_4, PC_5
		.i2c.mode = DRV_I2C_SS_MODE // DRV_I2C_FS_MODE // DRV_I2C_HS_MODE
};

#define i2c_reg(r) *((volatile uint32 *)(pi2c->base_regs + r))

#define MLX90614_USE_CRC

#ifdef MLX90614_USE_CRC
unsigned char crc8(unsigned char x) {
	for (int i = 8; i; i--) {
	    x = ( x << 1 ) ^ ( x & 128 ? 7 : 0 );
	}
	return x;
}
#endif


/* Пример непрерывного чтения регистров Tobj1 и Tobj2
 * MLX90614 по прерыванию таймера */
void mlx_tick_handler(void *par) {
	PMLX90614DRV p = &mlx90614drv;
	i2c_drv_t *pi2c = &p->i2c;
	switch(p->status) {
	default:
		// Disable controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) = 0;
		p->status = 1;
		break;
	case 1:
		// Master Target Address
		i2c_reg(REG_DW_I2C_IC_TAR) = p->addr;
		// Enable controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) = BIT_IC_ENABLE;
		p->status = 2;
		break;
	case 3:
		if (i2c_reg(REG_DW_I2C_IC_RAW_INTR_STAT) &	BIT_IC_RAW_INTR_STAT_TX_ABRT) {
			(volatile uint32)i2c_reg(REG_DW_I2C_IC_CLR_INTR);
			p->errs++;
			p->status = 0;
			break;
		} else {
			if(p->pbuf) {
				PMLX90614DATA pd = p->pbuf + p->buf_tx;
				// Считаем готовые значения из FIFO ic I2C
				pd->o.uc[0] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				pd->o.uc[1] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
#ifdef MLX90614_USE_CRC
				// B407B5 -> 0x28 (http://crccalc.com/)
				if(crc8(pd->o.uc[1] ^ crc8(pd->o.uc[0] ^ 0x28)) != (uint8) i2c_reg(REG_DW_I2C_IC_DATA_CMD)) {
					i2c_reg(REG_DW_I2C_IC_ENABLE) = 2; // ABORT
					p->errs++;
					p->status = 0;
					break;
				}
#endif
				pd->a.uc[0] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				pd->a.uc[1] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
#ifdef MLX90614_USE_CRC
				// B406B5 -> 0x3D (http://crccalc.com/)
				if(crc8(pd->a.uc[1] ^ crc8(pd->a.uc[0] ^ 0x3D)) != (uint8) i2c_reg(REG_DW_I2C_IC_DATA_CMD)) {
					i2c_reg(REG_DW_I2C_IC_ENABLE) = 2; // ABORT
					p->errs++;
					p->status = 0;
					break;
				}
#endif
				if(p->buf_tx >= p->buf_idx) p->buf_tx = 0;
				else p->buf_tx++;
				if(p->buf_rx == p->buf_tx) {
					p->overrun++; // todo: if(p->overrun++ > 100000) deinit() ?
					if(p->buf_rx >= p->buf_idx) p->buf_rx = 0;
					else p->buf_rx++;
				};
			} else {
				// удаление из FIFO блока данных
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
#ifdef MLX90614_USE_CRC
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
#endif
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
#ifdef MLX90614_USE_CRC
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
#endif
			};
		}
	case 2:
		// Заполним FIFO ic I2C командами чтения
		//  Write Command.
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = MLX90614_TOBJ1;
		//  Read command & data (3 bytes).
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD | BIT_IC_DATA_CMD_RESTART;
#ifdef MLX90614_USE_CRC
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD;
#endif
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD | BIT_IC_DATA_CMD_STOP;
		//  Write Command.
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = MLX90614_TA;
		//  Read command & data (3 bytes).
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD | BIT_IC_DATA_CMD_RESTART;
#ifdef MLX90614_USE_CRC
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD;
#endif
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD | BIT_IC_DATA_CMD_STOP;
		p->status = 3;
		break;
	}
}

uint16 mlx90614_chkdata(uint16 cnt)
{
	PMLX90614DRV p = &mlx90614drv;
	if(p->init <= 0) return 0;
	int len = p->buf_tx - p->buf_rx;
	if(len < 0) len += p->buf_idx + 1;
	if(cnt > (uint16)len) cnt = (uint16)len;
	return cnt;
}

size_t mlx90614_getdata(void *pd, uint16 cnt)
{
	PMLX90614DRV p = &mlx90614drv;
	if(p->init <= 0) return 0;
//	cnt = mlx90614_chkdata(cnt);
	unsigned short *pus = (unsigned short *) pd;
	taskDISABLE_INTERRUPTS();
	uint16 buf_rx = p->buf_rx;
	*pus++ = cnt;		// кол-во замеров
	*pus++ = p->count + p->overrun; // индекс замера для анализа пропусков на стороне приемника
	// если не пропущено, то равен прошлому + кол-во считанных замеров в прошлом блоке
	p->count += cnt; //	p->overrun = 0;
	unsigned char *puc = (unsigned char *) pus;
	if(cnt) {
		uint16 lend = buf_rx + cnt;
		if(lend > p->buf_idx) {
			lend -= p->buf_idx + 1;
			p->buf_rx = lend;
		} else {
			p->buf_rx = lend;
			lend = 0;
		};
		size_t len = (cnt - lend) *sizeof(MLX90614DATA);
		if(len)	memcpy(puc, (void *)(p->pbuf + buf_rx), len);
		if(lend) memcpy(puc + len, (void *)p->pbuf, lend *sizeof(MLX90614DATA));
	}
	taskENABLE_INTERRUPTS();
	return cnt * sizeof(MLX90614DATA) + 4;
}

//#define mMIN(a, b)  ((a<b)?a:b)
#define mMAX(a, b)  ((a>b)?a:b)

#include "web_srv.h"
#include "websock.h"

int mlx90614_ws(TCP_SERV_CONN *ts_conn, char cmd)
{
	PMLX90614DRV p = &mlx90614drv;
	switch(cmd) {
	case 'd': // deinit
		if(p->init > 0) {
			gtimer_stop(&p->timer);
			gtimer_deinit(&p->timer);
			_i2c_deinit(&p->i2c);
			if(p->pbuf) {
				free(p->pbuf);
				p->pbuf = NULL;
			}
			p->init = -1;
			return 0;
		}
		return 1;
	case 'c': // get count
		return mlx90614_chkdata(p->buf_idx + 1);
	case 'i': // init
		return p->init;
	default: // get_data
		if(p->init <= 0) {
			p->status = 0;
			p->count = 0;
			p->overrun = 0;
			p->errs = 0;
			if(!p->pbuf) {
				p->pbuf = zalloc((p->buf_idx + 1) * sizeof(MLX90614DATA));
				if(!p->pbuf) {
					error_printf("Error create buffer!\n");
					return -1;
				};
				p->buf_tx = 0;
				p->buf_rx = 0;
			};
			_i2c_init(&p->i2c);
		    gtimer_init(&p->timer, MLX90614_TIMER);
		    gtimer_start_periodical(&p->timer, p->timetik, (void*)mlx_tick_handler, (uint32_t)&mlx90614drv);
		    p->init = 1;
//			return 0;
		}
	case 'g': // get
		{
			uint32 i = mlx90614_chkdata(p->buf_idx + 1);
			if(i) {
				WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *)ts_conn->linkd;
				i = mMIN((web_conn->msgbufsize / sizeof(MLX90614DATA)), i);
				if(websock_tx_frame(ts_conn, WS_OPCODE_BINARY | WS_FRAGMENT_FIN, web_conn->msgbuf, mlx90614_getdata(web_conn->msgbuf, i)) != ERR_OK)
					return -1;
			}
			return i;
		}
	}
	return -1;
}

void mlx90614_init(void)
{
	PMLX90614DRV p = &mlx90614drv;
	if(p->init <= 0) {
		rtl_printf("Init MLX90614\n");
		p->status = 0;
		p->count = 0;
		p->errs = 0;
		if(!p->pbuf) {
			p->pbuf = zalloc((p->buf_idx + 1) * sizeof(MLX90614DATA));
			if(!p->pbuf) {
				error_printf("Error create buffer!\n");
				return;
			};
			p->buf_tx = 0;
			p->buf_rx = 0;
		};
// (!) Установки драйвера I2C заданы в структуре mlx90614drv
//		_i2c_setup(&p->i2c, MLX90614_I2C_PIN_SDA , MLX90614_I2C_PIN_SCL, DRV_I2C_FS_MODE); // == DRV_I2C_OK?
		_i2c_init(&p->i2c);
//		_i2c_set_speed(&p->i2c, MLX90614_I2C_BUS_CLK);
		rtl_printf("I2C%d mode = %d, drvStatus = %d\n", p->i2c.idx, p->i2c.mode, p->i2c.status);
// (!) Инициализация MLX90614 перенесена в прерывание таймера
		// Initial a periodical timer
	    gtimer_init(&p->timer, MLX90614_TIMER);
	    // Tick every 0.001 sec (p->timetik)
	    gtimer_start_periodical(&p->timer, p->timetik, (void*)mlx_tick_handler, (uint32_t)&mlx90614drv);
	    rtl_printf("MLX90614 Timer Period = %u us\n", p->timetik);
	    p->init = 1;
	}
}

void mlx90614_deinit(void)
{
	PMLX90614DRV p = &mlx90614drv;
	i2c_drv_t *pi2c = &p->i2c;
	if(p->init > 0) {
		rtl_printf("Deinit MLX90614\n");
	    gtimer_stop(&p->timer);
	    gtimer_deinit(&p->timer);
// (!) Не используется mlx90614_write / mlx90614_read для сокращения кода
//		mlx90614_write(MLX90614_REG_CONFIG, MLX90614_CONFIG_MODE_POWERDOWN);
		// Break controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) |= 2;
        vTaskDelay(2); // +WDT
		// Disable controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) = 0;
/* Exit from Sleep Mode > 250 ms !
		vTaskDelay(2); // +WDT
		// Master Target Address
		i2c_reg(REG_DW_I2C_IC_TAR) = p->addr;
		// Enable controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) = BIT_IC_ENABLE;
		// Заполним FIFO ic I2C командой sleep MLX90614
        vTaskDelay(2); // +WDT
		//  MLX90614 Enter Sleep Mode
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = MLX90614_SLEEP | BIT_IC_DATA_CMD_STOP;
*/
        vTaskDelay(2); // +WDT
		_i2c_deinit(&p->i2c);
		if(p->pbuf) {
			free(p->pbuf);
			p->pbuf = NULL;
		}
	}
	p->init = 0;
}

LOCAL void ShowMlx(void)
{
	PMLX90614DRV p = &mlx90614drv;
	rtl_printf("MLX90614: %d, %d\n", p->buf_i2c.us[0], p->buf_i2c.ss[1]);
	rtl_printf("MLX90614: Cnt = %u, Err = %u\n", p->count, p->errs);
}

LOCAL void fATMLX(int argc, char *argv[])
{
	if(argc > 1) {
		if(atoi(argv[1])) {
			mlx90614_init();
	        vTaskDelay(10); // +WDT
		}
		else mlx90614_deinit();
	}
	ShowMlx();
}


MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_mlx90614[] = {
		{"ATMLX", 0, fATMLX, "=[0/1]: MLX90614 =1 start, =0 stop"}
};
/*
 * Test i2c:
 * ati2c i
 * ati2c g 5a 1 6 3
 * ati2c d
 */
