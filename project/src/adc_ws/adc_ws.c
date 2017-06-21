/*
 *  ADC + Websocket
 *
 *  Created on: 18 июн. 2017 г.
 *      Author: pvvx
 */

#include <platform_opts.h>

#include "rtl8195a.h"
#include "device.h"
#include "PinNames.h"

#include "basic_types.h"
#include "diag.h"
#include "osdep_api.h"

#include "FreeRTOS.h"
#include "diag.h"

//------------------------------------------------------------------------------
#include "objects.h"
#include "PinNames.h"
#include "hal_adc.h"
#include "analogin_api.h"
#include "timer_api.h"
//#include "strproc.h"

#include "web_srv.h"
#include "websock.h"
#include "driver/adc_drv.h"
#include "rtl8195a/rtl_libc.h"


//------------------------------------------------------------------------------
//#define ADC_USE_TIMER TIMER3	// если назначено, то для чтения ADC используется таймер
								// иначе ADC int
//------------------------------------------------------------------------------


#ifndef CONFIG_MBED_ENABLED
#define malloc                  pvPortMalloc
#define zalloc                  pvPortZalloc
#define free                    vPortFree
#endif

//------------------------------------------------------------------------------
typedef union _adc_data {
	unsigned short us;
} ADC_DATA, *PADC_DATA;

typedef struct _adc_drv {
		signed char init;			// флаг
		unsigned char tmp;

		unsigned short count;		// счетчик считанных значений
		unsigned short overrun;		// счет переполнений буфера

		unsigned short buf_idx;		// объем буфера pbuf[buf_idx+1], максимальный индекс-номер замера
		unsigned short buf_rx;		// индекс-номер ещё не считанного замера
		unsigned short buf_tx;		// индекс-номер для записи следующего замера
		PADC_DATA pbuf;
#ifdef ADC_USE_TIMER
		gtimer_t timer;
#endif
} ADC_DRV, *PADC_DRV;

#define mMIN(a, b)  ((a<b)?a:b)
#define mMAX(a, b)  ((a>b)?a:b)

ADC_DRV adc_drv = {
		.buf_idx = (1460*2 - 80)/(sizeof(ADC_DATA)/2)	// циклический буфер на ~1420 замеров (см. sizeof(ADC_DATA))
								// Если шаг заполнения 1 ms -> буфер на 1.4 сек
								// Оптимизация под TCP: (TCP_MSS*2 - 80)/2 = (1460*2 - 80)/2 = 1420
};

void adc_int_handler(void *par) {
	PADC_DRV p = par; // &adc_drv
	if(p->pbuf) {
#ifndef ADC_USE_TIMER
		int i = 4;
		while(i--)
#endif
		{
			PADC_DATA pd = p->pbuf + p->buf_tx;
			pd->us = HAL_ADC_READ32(REG_ADC_FIFO_READ); // 2 -> sample -> 24.4 kHz (if ADC irq!)
//			(void)HAL_ADC_READ32(REG_ADC_FIFO_READ); // 4 sample -> 12.2 kHz (if ADC irq!)
//			(void)HAL_ADC_READ32(REG_ADC_FIFO_READ); // 6 sample -> 8.133 kHz (if ADC irq!)
//			(void)HAL_ADC_READ32(REG_ADC_FIFO_READ); // 8 sample -> 6.1 kHz (if ADC irq!)

			if(p->buf_tx >= p->buf_idx) p->buf_tx = 0;
			else p->buf_tx++;
			if(p->buf_rx == p->buf_tx) {
				p->overrun++; // todo: if(p->overrun++ > 100000) deinit() ?
				if(p->buf_rx >= p->buf_idx) p->buf_rx = 0;
				else p->buf_rx++;
			};
		}
		/* Clear ADC Status */
		(void)HAL_ADC_READ32(REG_ADC_INTR_STS);
	};
}

size_t adc_getdata(void *pd, uint16 cnt)
{
	PADC_DRV p = &adc_drv;
	if(p->init <= 0) return 0;
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
		size_t len = (cnt - lend) *sizeof(ADC_DATA);
		if(len)	memcpy(puc, (void *)(p->pbuf + buf_rx), len);
		if(lend) memcpy(puc + len, (void *)p->pbuf, lend *sizeof(ADC_DATA));
	}
	taskENABLE_INTERRUPTS();
	return cnt * sizeof(ADC_DATA) + 4;
}

uint16 adc_chkdata(uint16 cnt)
{
	PADC_DRV p = &adc_drv;
	if(p->init <= 0) return 0;
	int len = p->buf_tx - p->buf_rx;
	if(len < 0) len += p->buf_idx + 1;
	if(cnt > (uint16)len) cnt = (uint16)len;
	return cnt;
}

int adc_ws(TCP_SERV_CONN *ts_conn, char cmd)
{
	PADC_DRV p = &adc_drv;
	switch(cmd) {
	case 'd': // deinit
		if(p->init > 0) {
#ifdef ADC_USE_TIMER
		    gtimer_stop(&p->timer);
		    gtimer_deinit(&p->timer);
			ADCDisable();
#else
			ADCDisable();
			ADCIrqDeInit();
#endif
			ADCDeInit();
			if(p->pbuf) {
				free(p->pbuf);
				p->pbuf = NULL;
			}
			p->init = -1;
			return 0;
		}
		return 1;
	case 'c': // get count
		return adc_chkdata(p->buf_idx + 1);
	case 'i': // init
		return p->init;
	default: // get_data
		if(p->init <= 0) {
			p->count = 0;
			p->overrun = 0;
//			p->errs = 0;
			if(!p->pbuf) {
				p->pbuf = zalloc((p->buf_idx + 1) * sizeof(ADC_DATA));
				if(!p->pbuf) {
					error_printf("Error create buffer!\n");
					return -1;
				};
				p->buf_tx = 0;
				p->buf_rx = 0;
			};
			ADCInit(ADC2_SEL);
#ifdef ADC_USE_TIMER
			// Initial a periodical timer
		    gtimer_init(&p->timer, ADC_USE_TIMER);
		    gtimer_start_periodical(&p->timer, 1000, (void*)adc_int_handler, (uint32_t)p);
		    rtl_printf("ADC Timer Period = %u us\n", &p->timer.hal_gtimer_adp.TimerLoadValueUs);
#else
			ADCIrqInit(adc_int_handler,(uint32)p, BIT_ADC_FIFO_FULL_EN); // BIT_ADC_FIFO_RD_REQ_EN ?
#endif
			ADCEnable();
		    p->init = 1;
//			return 0;
		}
	case 'g': // get
		{
			uint32 i = adc_chkdata(p->buf_idx + 1);
			if(i) {
				WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *)ts_conn->linkd;
				i = mMIN((web_conn->msgbufsize / sizeof(ADC_DATA)), i);
				if(websock_tx_frame(ts_conn, WS_OPCODE_BINARY | WS_FRAGMENT_FIN, web_conn->msgbuf, adc_getdata(web_conn->msgbuf, i)) != ERR_OK)
					return -1;
			}
			return i;
		}
	}
	return -1;
}

