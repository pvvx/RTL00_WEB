/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 */

#include "rtl8195a.h"
#include "hal_common.h"

extern HAL_TIMER_OP        HalTimerOp;

HAL_Status 
HalCommonInit(void){

#ifdef CONFIG_TIMER_MODULE
    HalTimerOpInit_Patch((VOID*)(&HalTimerOp));
#endif

    return HAL_OK;
}
