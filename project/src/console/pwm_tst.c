/*
 * pwm_tst.c
 *
 *  Created on: 19/04/2017.
 *      Author: pvvx
 */

#include <platform_opts.h>
#include "rtl8195a.h"
#include "FreeRTOS.h"
#include "rtl8195a/rtl_libc.h"

//#include "device.h"
#include "pwmout_api.h"   // mbed
//#include "main.h"
#include "web_utils.h"
#include "objects.h"
#include "pinmap.h"

extern const PinMap PinMap_PWM[];
extern u32 gTimerRecord;

HAL_PWM_ADAPTER pwm_hal_adp;

LOCAL void fATPWM(int argc, char *argv[]) {

	uint8_t pin = ahextoul(argv[1]);
	uint32_t period = ahextoul(argv[2]);
	uint32_t pulse = ahextoul(argv[3]);

	uint32_t peripheral = pinmap_peripheral(pin, PinMap_PWM);

	if(pwm_hal_adp.enable) {
		HAL_Pwm_Disable(&pwm_hal_adp);
		gTimerRecord &= ~(1 << pwm_hal_adp.gtimer_id);
		rtl_memset((void *)&pwm_hal_adp, 0, sizeof(HAL_PWM_ADAPTER));
	};
	if((period) && (unlikely(peripheral != NC))
    	&& (HAL_Pwm_Init(&pwm_hal_adp, RTL_GET_PERI_IDX(peripheral), RTL_GET_PERI_SEL(peripheral)) == HAL_OK)) {
    	HAL_Pwm_SetDuty(&pwm_hal_adp, period, pulse);
    	HAL_Pwm_Enable(&pwm_hal_adp);
    } else {
    	printf("Error parameters!");
    };
}

//------------------------------------------------------------------------------
// atpwm=34,1048575,524287
// atpwm=34,122,61 (8.187kHz)
// atsw 40000368 85001002 (8.187kHz)
// atsd 40000360 6
MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_pwm[] = {
		{ "ATPWM", 3, fATPWM, "=<pin>,<period>,<pulse>: PWM Test" }
};
