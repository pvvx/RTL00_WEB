/**************************************************************************
 * Драйвер для INA219
 **************************************************************************/
#ifndef _INA219DRV_H_
#define _INA219DRV_H_
#include "ina219/ina219.h"
#include "device.h"
#include "timer_api.h"
#include "driver/i2c_drv.h"

typedef struct _ina219data {
	signed short voltage; 	// Voltage, 1mV
	signed short current;	// Current, 50uA?
	signed short power;		// Power, 1mW
	unsigned short shunt;		// 10uV
} INA219DATA, *PINA219DATA;

typedef struct _ina219drv {
	unsigned char status;
	unsigned char addr;
	signed char init;
	unsigned char tmp;
	unsigned short config;		// регистр конфигурации INA219
	unsigned short calibration;	// коэф. шунта для INA219
	unsigned int count;
	unsigned int errs;
	gtimer_t timer;
	union {						// буфер
		unsigned char 	uc[4];
		unsigned short	us[2];
		signed short	ss[2];
		unsigned int	ui;
	} buf_i2c;
	i2c_drv_t i2c;
} INA219DRV, *PINA219DRV;


#define INA219_I2C_PIN_SDA	PC_4
#define INA219_I2C_PIN_SCL	PC_5
#define INA219_I2C_BUS_CLK	300000  //hz
#define INA219_TIMER TIMER3	// используемый таймер

#endif // _INA219DRV_H_
