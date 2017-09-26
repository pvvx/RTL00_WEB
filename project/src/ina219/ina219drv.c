/*
 * ina219drv.c
 *
 *  Created on: 02/05/2017
 *      Author: pvvx
 */
#include <platform_opts.h>
#include "rtl8195a.h"
#include "device.h"
#include "PinNames.h"

#include "basic_types.h"
#include "diag.h"
#include "osdep_api.h"

//#include "i2c_api.h"
//#include "i2c_ex_api.h"
#include "pinmap.h"
#include "rtl_lib.h"
#include "ina219/ina219drv.h"
#include "rtl8195a/rtl_libc.h"
#include "driver/i2c_drv.h"
#include "platform_stdlib.h"
#include "web_websocket.h"
#include "tcpsrv/tcp_srv_conn.h"

#include "hal_com_reg.h"

//#define ReadTSF_Lo32() (*((volatile unsigned int *)(WIFI_REG_BASE + REG_TSFTR)))
//#define ReadTSF_Hi32() (*((volatile unsigned int *)(WIFI_REG_BASE + REG_TSFTR1)))

INA219DRV ina219drv = {
		.addr = INA219_ADDRESS>>1,
		.config =
		INA219_CONFIG_BVOLTAGERANGE_16V | // INA219_CONFIG_BVOLTAGERANGE_32V
		INA219_CONFIG_GAIN_8_320MV | // INA219_CONFIG_GAIN_1_40MV |
		INA219_CONFIG_BADCRES_12BIT |
		INA219_CONFIG_SADCRES_12BIT_1S_532US | //INA219_CONFIG_SADCRES_12BIT_2S_1060US | // INA219_CONFIG_SADCRES_12BIT_1S_532US | //
		INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS,
		.calibration = 8192,	// при шунте 0.1 Ом
		.buf_idx = 709,			// циклический буфер на 710 замеров (по 4 байт -> sizeof(INA219DATA))
								// Если шаг заполнения 1 ms -> буфер на 0.71 сек
								// Оптимизация под TCP: (TCP_MSS*2 - 80)/4 = (1460*2 - 80)/4 = 710
		.i2c.status = DRV_I2C_OFF,
		.i2c.idx = 1,				// I2C1
		.i2c.io_sel = S0,			// PC_4, PC_5
		.i2c.mode = DRV_I2C_FS_MODE // DRV_I2C_HS_MODE
};
/*
void  ina219_write(unsigned char reg, unsigned short data)
{
	PINA219DRV p = &ina219drv;
	p->buf_i2c.uc[0] = reg;
	p->buf_i2c.uc[1] = (unsigned char)((unsigned short)(data >> 8));
	p->buf_i2c.uc[2] = (unsigned char)data;
  	_i2c_write(&p->i2c, p->addr, (const char *)p->buf_i2c.uc, 3, 1);
  	UBaseType_t f;
  	portBASE_TYPE d;
}

unsigned int ina219_read(unsigned char reg)
{
	PINA219DRV p = &ina219drv;
	p->buf_i2c.uc[0] = reg;
    _i2c_write(&p->i2c, p->addr, (const char *)p->buf_i2c.uc, 1, 1);
	p->buf_i2c.ui = 0;
	_i2c_read(&p->i2c, p->addr, (const char *)p->buf_i2c.uc, 2, 1);
	return (p->buf_i2c.uc[0] << 8) | p->buf_i2c.uc[1];
}
*/

#define i2c_reg(r) *((volatile uint32 *)(pi2c->base_regs + r))

/* Пример непрерывного чтения регистров
 * тока и напряжения IN219 по прерыванию таймера */
void ina_tick_handler(void *par) {
	PINA219DRV p = &ina219drv;
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
	case 2:
		// Заполним FIFO ic I2C командами инициализации INA219
		//  Write addr reg.
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = INA219_REG_CONFIG;
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = p->config >> 8;
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = (p->config & 0x0FF) | BIT_IC_DATA_CMD_STOP;
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = INA219_REG_CALIBRATION;
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = p->calibration >> 8;
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = (p->calibration & 0x0FF) | BIT_IC_DATA_CMD_STOP;
		//
		p->status = 3;
		break;
	case 4:
		if (i2c_reg(REG_DW_I2C_IC_RAW_INTR_STAT) &	BIT_IC_RAW_INTR_STAT_TX_ABRT) {
			(volatile uint32)i2c_reg(REG_DW_I2C_IC_CLR_INTR);
			p->errs++;
			p->status = 0;
			break;
		} else {
			if(p->pbuf) {
				PINA219DATA pd = p->pbuf + p->buf_tx;
				// Считаем готовые значения из FIFO ic I2C
				pd->v.vuc[1] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				pd->v.vuc[0] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				pd->i.iuc[1] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				pd->i.iuc[0] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				if(p->buf_tx >= p->buf_idx) p->buf_tx = 0;
				else p->buf_tx++;
				if(p->buf_rx == p->buf_tx) {
					p->overrun++; // todo: if(p->overrun++ > 100000) deinit() ?
					if(p->buf_rx >= p->buf_idx) p->buf_rx = 0;
					else p->buf_rx++;
				};
			} else {
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				(volatile uint32)i2c_reg(REG_DW_I2C_IC_DATA_CMD);
			};
		}
	case 3:
		// Заполним FIFO ic I2C командами чтения
		//  Write addr reg.
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = INA219_REG_BUSVOLTAGE | BIT_IC_DATA_CMD_STOP;
		//  Read command.
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD;
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD | BIT_IC_DATA_CMD_STOP;
		//  Write addr reg.
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = INA219_REG_SHUNTVOLTAGE | BIT_IC_DATA_CMD_STOP;
		//  Read command.
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD;
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD | BIT_IC_DATA_CMD_STOP;
		p->status = 4;
		break;
	}
}

uint16 ina219_chkdata(uint16 cnt)
{
	PINA219DRV p = &ina219drv;
	if(p->init <= 0) return 0;
	int len = p->buf_tx - p->buf_rx;
	if(len < 0) len += p->buf_idx + 1;
	if(cnt > (uint16)len) cnt = (uint16)len;
	return cnt;
}

size_t ina219_getdata(void *pd, uint16 cnt)
{
	PINA219DRV p = &ina219drv;
	if(p->init <= 0) return 0;
//	cnt = ina219_chkdata(cnt);
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
		size_t len = (cnt - lend) *sizeof(INA219DATA);
		if(len)	memcpy(puc, (void *)(p->pbuf + buf_rx), len);
		if(lend) memcpy(puc + len, (void *)p->pbuf, lend *sizeof(INA219DATA));
	}
	taskENABLE_INTERRUPTS();
	return cnt * sizeof(INA219DATA) + 4;
}

//#define mMIN(a, b)  ((a<b)?a:b)
#define mMAX(a, b)  ((a>b)?a:b)

#include "web_srv.h"
#include "websock.h"

int ina219_ws(TCP_SERV_CONN *ts_conn, char cmd)
{
	PINA219DRV p = &ina219drv;
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
		return ina219_chkdata(p->buf_idx + 1);
	case 'i': // init
		return p->init;
	default: // get_data
		if(p->init <= 0) {
			p->status = 0;
			p->count = 0;
			p->overrun = 0;
			p->errs = 0;
			if(!p->pbuf) {
				p->pbuf = zalloc((p->buf_idx + 1) * sizeof(INA219DATA));
				if(!p->pbuf) {
					error_printf("Error create buffer!\n");
					return -1;
				};
				p->buf_tx = 0;
				p->buf_rx = 0;
			};
			_i2c_init(&p->i2c);
		    gtimer_init(&p->timer, INA219_TIMER);
		    gtimer_start_periodical(&p->timer, 532*2, (void*)ina_tick_handler, (uint32_t)&ina219drv);
		    p->init = 1;
//			return 0;
		}
	case 'g': // get
		{
			uint32 i = ina219_chkdata(p->buf_idx + 1);
			if(i) {
				WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *)ts_conn->linkd;
				i = mMIN((web_conn->msgbufsize / sizeof(INA219DATA)), i);
				if(websock_tx_frame(ts_conn, WS_OPCODE_BINARY | WS_FRAGMENT_FIN, web_conn->msgbuf, ina219_getdata(web_conn->msgbuf, i)) != ERR_OK)
					return -1;
			}
			return i;
		}
	}
	return -1;
}

void ina219_init(void)
{
	PINA219DRV p = &ina219drv;
	if(p->init <= 0) {
		rtl_printf("Init INA219\n");
		p->status = 0;
		p->count = 0;
		p->errs = 0;
		if(!p->pbuf) {
			p->pbuf = zalloc((p->buf_idx + 1) * sizeof(INA219DATA));
			if(!p->pbuf) {
				error_printf("Error create buffer!\n");
				return;
			};
			p->buf_tx = 0;
			p->buf_rx = 0;
		};
// (!) Установки драйвера I2C заданы в структуре ina219drv
//		_i2c_setup(&p->i2c, INA219_I2C_PIN_SDA , INA219_I2C_PIN_SCL, DRV_I2C_FS_MODE); // == DRV_I2C_OK?
		_i2c_init(&p->i2c);
//		_i2c_set_speed(&p->i2c, INA219_I2C_BUS_CLK);
		rtl_printf("I2C%d mode = %d, drvStatus = %d\n", p->i2c.idx, p->i2c.mode, p->i2c.status);
// (!) Инициализация INA219 перенесена в прерывание таймера
//		ina219_write(INA219_REG_CONFIG, p->config);
//		ina219_write(INA219_REG_CALIBRATION, p->calibration);
		// Initial a periodical timer
	    gtimer_init(&p->timer, INA219_TIMER);
	    // Tick every 0.000532 sec (N*532 μs)
//	    uint32 tus = (1 << ((p->config >> 3) & 7));
//	    tus *= 532;
	    uint32 tus = 532*2;
	    gtimer_start_periodical(&p->timer, tus, (void*)ina_tick_handler, (uint32_t)&ina219drv);
	    rtl_printf("INA219 Timer Period = %u us\n", tus);
	    p->init = 1;
	}
}

void ina219_deinit(void)
{
	PINA219DRV p = &ina219drv;
	i2c_drv_t *pi2c = &p->i2c;
	if(p->init > 0) {
		rtl_printf("Deinit INA219\n");
	    gtimer_stop(&p->timer);
	    gtimer_deinit(&p->timer);
// (!) Не используется ina219_write / ina219_read для сокращения кода
//		ina219_write(INA219_REG_CONFIG, INA219_CONFIG_MODE_POWERDOWN);
		// Break controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) |= 2;
        vTaskDelay(2); // +WDT
		// Disable controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) = 0;
		vTaskDelay(2); // +WDT
		// Master Target Address
		i2c_reg(REG_DW_I2C_IC_TAR) = p->addr;
		// Enable controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) = BIT_IC_ENABLE;
		// Заполним FIFO ic I2C командой отключения INA219
        vTaskDelay(2); // +WDT
		//  Write addr reg. INA219 POWER_DOWN
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = INA219_REG_CONFIG;
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = (INA219_CONFIG_MODE_POWERDOWN >> 8);
		i2c_reg(REG_DW_I2C_IC_DATA_CMD) = (INA219_CONFIG_MODE_POWERDOWN & 0x0F) | BIT_IC_DATA_CMD_STOP;
        vTaskDelay(2); // +WDT
		_i2c_deinit(&p->i2c);
		if(p->pbuf) {
			free(p->pbuf);
			p->pbuf = NULL;
		}
	}
	p->init = 0;
}

LOCAL void ShowIna(void)
{
	PINA219DRV p = &ina219drv;
	rtl_printf("INA219: %d, %d\n", p->buf_i2c.us[0], p->buf_i2c.ss[1]);
	rtl_printf("INA219: Cnt = %u, Err = %u\n", p->count, p->errs);
}

LOCAL void fATINA(int argc, char *argv[])
{
	if(argc > 1) {
		if(atoi(argv[1])) {
			ina219_init();
	        vTaskDelay(10); // +WDT
		}
		else ina219_deinit();
	}
	ShowIna();
}

MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_ina219[] = {
		{"ATINA", 0, fATINA, "=[0/1]: INA219 =1 start, =0 stop"}
};

