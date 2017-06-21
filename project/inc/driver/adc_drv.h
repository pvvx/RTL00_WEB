/*
 * Simple ADC DRV (adc_drv.h)
 *
 *  Created on: 19 июн. 2017 г.
 *      Author: pvvx
 */

#ifndef _DRIVER_ADC_DRV_H_
#define _DRIVER_ADC_DRV_H_

#include "rtl8195a.h"
#include "rtl8195a_adc.h"

void ADCIrqInit(IRQ_FUN IrqFunc, u32 IrqData, u32 intr_enable); // intr_enable = bits: REG_ADC_INTR_EN - BIT_ADC_FIFO_RD_ERROR_EN | BIT_ADC_FIFO_RD_REQ_EN | BIT_ADC_FIFO_FULL_EN ...
void ADCIrqDeInit(void);

void ADCInit(ADC_MODULE_SEL adc_idx); // RTL8711AM: adc_idx = ADC2_SEL = 2
void ADCDeInit(void);
void ADCEnable(void);	// ADC Start
void ADCDisable(void);	// ADC Stop


#endif /* _DRIVER_ADC_DRV_H_ */
