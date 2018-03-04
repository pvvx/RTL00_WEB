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

void ADCInit(unsigned char mode, unsigned char xclk, unsigned char dcmf);
void ADCDeInit(void);
void ADCEnable(void);	// ADC Start
void ADCDisable(void);	// ADC Stop

#define ADC_DECIMATION_1		1
#define ADC_DECIMATION_2		2
#define ADC_DECIMATION_4		3
#define ADC_DECIMATION_8		4

#define ADC_SAMPLE_CLK_x1		0
#define ADC_SAMPLE_CLK_x2		1
#define ADC_SAMPLE_CLK_x4		2
#define ADC_SAMPLE_CLK_x8		3

#define ADC_DECIMATION_FILTER	ADC_DECIMATION_1
#define ADC_SAMPLE_XCLK			ADC_SAMPLE_CLK_x8
#define ADC_SMPS = 975*(1<<ADC_SAMPLE_XCLK)/(1<<ADC_DECIMATION_FILTER)
// 975*(1<<2)/(1<<1) = 1950 sps

// Add ADC_ANAPAR_AD0
#define BIT_SHIFT_ADC_SAMPLE_CLK            	14
#define BIT_MASK_ADC_SAMPLE_CLK             	0x3
#define BIT_CTRL_ADC_SAMPLE_CLKL(x)          	(((x) & BIT_MASK_ADC_SAMPLE_CLK) << BIT_SHIFT_ADC_SAMPLE_CLK)
// Add ADC_ANAPAR_AD1
#define BIT_SHIFT_DOWN_SAMPLE_RATE            	18
#define BIT_MASK_DOWN_SAMPLE_RATE             	0x7
#define BIT_CTRL_DOWN_SAMPLE_RATE(x)          	(((x) & BIT_MASK_DOWN_SAMPLE_RATE) << BIT_SHIFT_DOWN_SAMPLE_RATE)
#define BIT_ADC_DECIMATION_FILTER_ORDER			((u32)0x00000001 << 12) /*!< decimation filter order, 0: order3, 1: order4 */
#define BIT_ADC_EXT_VREF_EN						((u32)0x00000001 << 2)
#define BIT_ADC_FEEDBK_CAPACITY_VAL				((u32)0x00000001 << 1)
#define BIT_ADC_DIGITAL_RST_BAR					((u32)0x00000001 << 0)


#endif /* _DRIVER_ADC_DRV_H_ */
