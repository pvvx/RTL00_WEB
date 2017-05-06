/*
 * ina219buf.c
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
#include "ina219/ina219buf.h"
#include "rtl8195a/rtl_libc.h"
#include "driver/i2c_drv.h"

INA219DRV ina219drv = {
		.addr = INA219_ADDRESS>>1,
		.config =
		INA219_CONFIG_BVOLTAGERANGE_16V | // INA219_CONFIG_BVOLTAGERANGE_32V
		INA219_CONFIG_GAIN_8_320MV | // INA219_CONFIG_GAIN_1_40MV |
		INA219_CONFIG_BADCRES_12BIT |
		INA219_CONFIG_SADCRES_12BIT_2S_1060US | // INA219_CONFIG_SADCRES_12BIT_128S_69MS |
		INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS,
		.calibration = 8192,
		.i2c.status = DRV_I2C_OFF,
		.i2c.idx = 1,				// I2C1
		.i2c.io_sel = S0,			// PC_4, PC_5
		.i2c.mode = DRV_I2C_FS_MODE // DRV_I2C_HS_MODE
};

void  ina219_write(unsigned char reg, unsigned short data)
{
	PINA219DRV p = &ina219drv;
	p->buf_i2c.uc[0] = reg;
	p->buf_i2c.uc[1] = (unsigned char)((unsigned short)(data >> 8));
	p->buf_i2c.uc[2] = (unsigned char)data;
  	_i2c_write(&p->i2c, p->addr, (const char *)p->buf_i2c.uc, 3, 1);
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
			uint32 tmp = i2c_reg(REG_DW_I2C_IC_CLR_INTR);
			p->errs++;
			p->status = 0;
			break;
		} else {
			// Считаем готовые значения из FIFO ic I2C
			p->buf_i2c.uc[1] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
			p->buf_i2c.uc[0] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
			p->buf_i2c.uc[3] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
			p->buf_i2c.uc[2] = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
			p->count++;
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

void ina219_init(void)
{
	PINA219DRV p = &ina219drv;
	if(p->init <= 0) {
		rtl_printf("Init INA219\n");
		p->status = 0;
		p->count = 0;
		p->errs = 0;
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
	    uint32 tus = (1 << ((p->config >> 3) & 7));
	    tus *= 532;
	    gtimer_start_periodical(&p->timer, tus, (void*)ina_tick_handler, (uint32_t)&ina219drv);
	    rtl_printf("INA219 Read Period = %u us\n", tus);
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
	}
	p->init = -1;
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


extern void dump_bytes(uint32 addr, int size);
extern uint32 hextoul(uint8 *s);

i2c_drv_t ti2c;
/* Sample:
 * ati2c i
 * ati2c w 40 5
 * ati2c r 40 2
 */
LOCAL void fATI2C(int argc, char *argv[])
{
	i2c_drv_t *pi2c = &ti2c;
	uint8 buf[32];
	if(argc > 1) {
		if(argv[1][0] == 'i') {
			if(!pi2c->status) {
				uint8 sda = 0;
				uint8 scl = 0;
				uint8 mode = 0;
				uint32 speed = 0;
				if(argc > 2) sda = hextoul(argv[2]);
				else if(argc > 3) scl = hextoul(argv[3]);
				else if(argc > 4) mode = hextoul(argv[4]);
				else if(argc > 5) speed = hextoul(argv[5]);
				if(!sda) sda = PC_4;
				if(!scl) scl = PC_5;
				if(!mode) mode = DRV_I2C_FS_MODE;
				if(!speed) speed = 400000;
				if(_i2c_setup(pi2c, sda, scl, mode) == DRV_I2C_OK
						&& _i2c_init(pi2c) == DRV_I2C_OK
						&& _i2c_set_speed(pi2c, speed) == DRV_I2C_OK) {
					rtl_printf("I2C%d Init\n", pi2c->idx);
				};
			} else {
				rtl_printf("Already init!\n");
				return;
			};
		} else {
			if(pi2c->status) {
				if(argv[1][0] == 'd') {
					_i2c_ic_off(pi2c);
					rtl_printf("I2C%d DeInit\n", pi2c->idx);
					return;
				};
				int i;
				for(i = 0; i + 2 < argc; i++) {
					buf[i] = hextoul(argv[i+2]);
				};
				if(i) {
					if(argv[1][0] == 'w') {
						_i2c_write(pi2c, buf[0], &buf[1], i-1, 1);
						rtl_printf("I2C%d write[%d]:\n", pi2c->idx, i-1);
						dump_bytes(&buf[0], i);
					}
					else if(argv[1][0] == 'r') {
						i = buf[1];
						if(i > sizeof(buf) - 1) i = sizeof(buf) - 1;
						_i2c_read(pi2c, buf[0], &buf[1], i, 1);
						rtl_printf("I2C%d read[%d]:\n", pi2c->idx, i);
						dump_bytes(&buf[0], i+1);
					};

				};
			};
		};
	};
	rtl_printf("I2C%d drvStatus = %d\n", pi2c->idx, pi2c->status);
	return;
}

MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_ina219[] = {
		{"ATI2C", 0, fATI2C, ": Test I2C, <i>nit, <d>einit, <w>rite, <r>ead"},
		{"ATINA", 0, fATINA, "=[0/1]: INA219 =1 start, =0 stop"}
};

