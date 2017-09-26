/*
 * i2c_drv.h
 *
 *  Created on: 02/05/2017.
 *      Author: pvvx
 */

#ifndef PROJECT_INC_DRIVER_I2C_DRV_H_
#define PROJECT_INC_DRIVER_I2C_DRV_H_

#include "device.h"

typedef struct _I2C_HND_ {
	signed char status;		// _i2c_status_e
	unsigned char idx;		// Номер контроллера I2C
	unsigned char io_sel; 	// Location Index(Pin Mux Selection): S0 -> PC_4, PC_5
	unsigned char mode;		// _i2c_mode_e,  if(I2C_FIXED_SPEED_MODE != 0) user set -> i2c_mode_e
	void * base_regs;
} i2c_drv_t, *i2c_drv_p;

typedef enum
{
	DRV_I2C_OFF = 0,		// IC I2C  DeInit
	DRV_I2C_OK = 0,			// DRV ret Ok
	DRV_I2C_IC_OFF = 1, 	// IC I2C Off
	DRV_I2C_IC_ENABLE = 2, 	// IC I2C On
	DRV_I2C_ERR = -1,		// DRV ret err
	DRV_I2C_ABORT = -1, 	// IC I2C Abort
	DRV_I2C_TIMEOUT = -3 	// IC I2C / DRV ret Timeout
} _i2c_status_e;


typedef enum
{
	DRV_I2C_SS_MODE     =   1, // Standard Mode (100 Kbps)
	DRV_I2C_FS_MODE     =   2, // Fast Mode (400 Kbps)
	DRV_I2C_HS_MODE     =   3  // Fast Mode Plus (1 Mbps)
} _i2c_mode_e;

#define DRV_I2C_POOL_TIMEOUT 16384
#define _i2c_deinit(p) _i2c_ic_off(p)

// Setup
int _i2c_setup(i2c_drv_t *pi2c, PinName sda, PinName scl, unsigned char mode); // _i2c_mode_e
int _i2c_set_speed(i2c_drv_t *pi2c, uint32 clk_hz);
// Work
int _i2c_init(i2c_drv_t *pi2c);
int _i2c_write(i2c_drv_t *pi2c, uint32 address, const char *data, int length, int stop);
int _i2c_read(i2c_drv_t *pi2c, uint32 address, const char *data, int length, int stop);
// Utils
int _i2c_break(i2c_drv_t *pi2c);
void _i2c_ic_off(i2c_drv_t *pi2c);


#endif /* PROJECT_INC_DRIVER_I2C_DRV_H_ */
