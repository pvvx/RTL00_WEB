/**************************************************************************
 * Драйвер для INA219
 **************************************************************************/
#ifndef _INA219DRV_H_
#define _INA219DRV_H_
#include "ina219/ina219.h"
#include "device.h"
#include "timer_api.h"
#include "driver/i2c_drv.h"

typedef struct _ina219_data {
	union {
		signed short voltage; 	// Voltage, 1mV
		unsigned char 	vuc[2];
	} v;
	union {
		signed short current;	// Current, 50uA?
		unsigned char 	iuc[2];
	} i;
} INA219DATA, *PINA219DATA;

typedef struct _ina219drv {
	unsigned char status;
	unsigned char addr;			// адрес INA219 на шине I2C (формат 7 bit)
	signed char init;
	unsigned char tmp;

	unsigned short config;		// регистр конфигурации INA219
	unsigned short calibration;	// коэф. шунта для INA219

	unsigned short count;		// счетчик считанных значений
	unsigned short overrun;		// счет переполнений буфера

	unsigned short errs;		// счет ошибок на I2C
	unsigned short buf_idx;		// объем буфера pbuf[buf_idx+1], максимальный индекс-номер замера
	unsigned short buf_rx;		// индекс-номер ещё не считанного замера
	unsigned short buf_tx;		// индекс-номер для записи следующего замера
	PINA219DATA pbuf;
	gtimer_t timer;
	union {						// буфер
		unsigned char 	uc[4];
		unsigned short	us[2];
		signed short	ss[2];
		unsigned int	ui;
	} buf_i2c;
	i2c_drv_t   i2c;
} INA219DRV, *PINA219DRV;


#define INA219_I2C_PIN_SDA	PC_4
#define INA219_I2C_PIN_SCL	PC_5
#define INA219_I2C_BUS_CLK	300000  //hz
#define INA219_TIMER TIMER3	// используемый таймер

#endif // _INA219DRV_H_
