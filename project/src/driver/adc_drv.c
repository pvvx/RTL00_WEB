/*
 *  Simple ADC DRV (adc_drv.c)
 *  Created on: 18 июн. 2017 г.
 *  Author: pvvx
 */

#include <platform_opts.h>

#include "platform_autoconf.h"
#include "diag.h"
#include "rtl8195a_adc.h"
#include "hal_adc.h"
#include "driver/adc_drv.h"

//------------------------------------------------------------------------------

void ADCIrqInit(IRQ_FUN IrqFunc, u32 IrqData, u32 intr_enable) {
    IRQ_HANDLE IrqHandle = {
    		.IrqNum = ADC_IRQ,
			.Priority = 5
    };
    IrqHandle.Data        =   IrqData;
    IrqHandle.IrqFun      =   IrqFunc;
    InterruptRegister(&IrqHandle);
    InterruptEn(&IrqHandle);
    HAL_ADC_WRITE32(REG_ADC_INTR_EN, intr_enable);
}

void ADCIrqDeInit(void) {
    IRQ_HANDLE IrqHandle = {
    		.IrqNum = ADC_IRQ,
			.Priority = 5,
			.Data = (u32)NULL,
    		.IrqFun = (IRQ_FUN) NULL
    };
    HAL_ADC_WRITE32(REG_ADC_INTR_EN, 0);
    InterruptUnRegister(&IrqHandle);
}

void ADCEnable(void) {
	/* Clear ADC Status */
	(void)HAL_ADC_READ32(REG_ADC_INTR_STS);
    u32 AdcTempDat	=   HAL_ADC_READ32(REG_ADC_POWER);
    AdcTempDat      &=  (~BIT_ADC_PWR_AUTO);
    AdcTempDat      |= 0x02;
    HAL_ADC_WRITE32(REG_ADC_POWER, AdcTempDat);
	AdcTempDat      |= 0x04;
    HAL_ADC_WRITE32(REG_ADC_POWER, AdcTempDat);
	AdcTempDat      &= (~0x08);
    HAL_ADC_WRITE32(REG_ADC_POWER, AdcTempDat);
    /* */
   	HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD0,
		HAL_ADC_READ32(REG_ADC_ANAPAR_AD0) | BIT_ADC_EN_MANUAL);
   	HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD1,
		HAL_ADC_READ32(REG_ADC_ANAPAR_AD1) | BIT_ADC_EN_MANUAL);
}

void ADCDisable(void) {
#if ADC_USE_IRQ
    HAL_ADC_WRITE32(REG_ADC_INTR_EN, 0);
#endif

	HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD0,
			HAL_ADC_READ32(REG_ADC_ANAPAR_AD0) & (~BIT_ADC_EN_MANUAL));

	HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD1,
			HAL_ADC_READ32(REG_ADC_ANAPAR_AD1) & (~BIT_ADC_EN_MANUAL));

	HAL_ADC_WRITE32(REG_ADC_POWER,
		HAL_ADC_READ32(REG_ADC_POWER)
		& (~(BIT_ADC_PWR_AUTO)));
}

#ifdef CONFIG_SOC_PS_MODULE
static void ADCEnablePS(void)
{
    REG_POWER_STATE adcPwrState;
    // To register a new peripheral device power state
    adcPwrState.FuncIdx = ADC0;
    adcPwrState.PwrState = ACT;
    RegPowerState(adcPwrState);
}
#endif

void ADCInit(ADC_MODULE_SEL adc_idx) {
    /* ADC Function and Clock Enable */
    /* To release DAC delta sigma clock gating */
    HAL_WRITE32(SYSTEM_CTRL_BASE, REG_SYS_SYSPLL_CTRL2,
    		HAL_READ32(SYSTEM_CTRL_BASE, REG_SYS_SYSPLL_CTRL2) | BIT25);
    /* Turn on DAC active clock */
    ACTCK_ADC_CCTRL(ON);
    /* Enable DAC0 module */
    ADC0_FCTRL(ON);
    /* Enable ADC power cut ? */
//    ADCEnablePW();
    /* ADC Control register set-up*/
    HAL_ADC_WRITE32(REG_ADC_CONTROL,
    	BIT_CTRL_ADC_COMP_ONLY(ADC_FEATURE_DISABLED) 	// compare mode only enable (without FIFO enable)
		| BIT_CTRL_ADC_ONESHOT(ADC_FEATURE_DISABLED)	// one-shot mode enable
		| BIT_CTRL_ADC_OVERWRITE(ADC_FEATURE_DISABLED)	// overwrite mode enable
		| BIT_CTRL_ADC_ENDIAN(ADC_DATA_ENDIAN_LITTLE)		// endian selection
		| BIT_CTRL_ADC_BURST_SIZE(8)	// DMA operation threshold
		| BIT_CTRL_ADC_THRESHOLD(8)	// one shot mode threshold
		| BIT_CTRL_ADC_DBG_SEL(ADC_DBG_SEL_DISABLE));
#if 0
    /* ADC compare value and compare method setting*/
    switch (adc_idx) {
        case ADC0_SEL:
            HAL_ADC_WRITE32(REG_ADC_COMP_VALUE_L,
            		(HAL_ADC_READ32(REG_ADC_COMP_VALUE_L)
					& (~(BIT_ADC_COMP_TH_0(0xFFFF))))
					| BIT_CTRL_ADC_COMP_TH_0(0) // ADC compare mode threshold
					);
            break;
        case ADC1_SEL:
            HAL_ADC_WRITE32(REG_ADC_COMP_VALUE_L,
            		(HAL_ADC_READ32(REG_ADC_COMP_VALUE_L)
					& (~(BIT_ADC_COMP_TH_1(0xFFFF))))
					| BIT_CTRL_ADC_COMP_TH_1(0) // ADC compare mode threshold
					);
            break;
        case ADC2_SEL:
            HAL_ADC_WRITE32(REG_ADC_COMP_VALUE_H,
            		(HAL_ADC_READ32(REG_ADC_COMP_VALUE_H)
					& (~(BIT_ADC_COMP_TH_2(0xFFFF))))
					| BIT_CTRL_ADC_COMP_TH_2(0) // ADC compare mode threshold
					);
            break;

        case ADC3_SEL:
            HAL_ADC_WRITE32(REG_ADC_COMP_VALUE_H,
            		(HAL_ADC_READ32(REG_ADC_COMP_VALUE_H)
					& (~(BIT_ADC_COMP_TH_3(0xFFFF))))
					| BIT_CTRL_ADC_COMP_TH_3(0) // ADC compare mode threshold
					);
            break;
    }
    /* ADC compare mode setting */
    HAL_ADC_WRITE32(REG_ADC_COMP_SET,
    		HAL_ADC_READ32(REG_ADC_COMP_SET)
			& (~(1 << adc_idx))); // compare mode control : less than the compare threshold
#endif
    /* ADC audio mode set-up */
    /* ADC enable manually setting */
    HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD0,
    		HAL_ADC_READ32(REG_ADC_ANAPAR_AD0)
//			& (~(BIT_ADC_AUDIO_EN)))
//			& (~(BIT_ADC_EN_MANUAL))
			| BIT_ADC_AUDIO_EN 	// ADC audio mode enable
			| BIT_ADC_EN_MANUAL // ADC enable manually
			);
    /* ADC analog parameter 0 */
    HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD0,
    		(HAL_ADC_READ32(REG_ADC_ANAPAR_AD0)
//			& (~BIT14) //ADC Input is internal?
			| (BIT14))
			& (~(BIT3|BIT2)));
    /* ADC analog parameter 1 */
    HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD1,
    		(HAL_ADC_READ32(REG_ADC_ANAPAR_AD1) & (~BIT1)) | (BIT2|BIT0));
    /* ADC analog parameter 2 */
    HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD2, 0x67884400);
    /* ADC analog parameter 3 */
    HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD3, 0x77780039);
    /* ADC analog parameter 4 */
    HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD4, 0x0004d501);
    /* ADC analog parameter 5 */
    HAL_ADC_WRITE32(REG_ADC_ANAPAR_AD5, 0x1E010800);
#ifdef CONFIG_SOC_PS_MODULE
    ADCEnablePS();
#endif
}

void ADCDeInit(void) {
#ifdef CONFIG_SOC_PS_MODULE
	REG_POWER_STATE adcPwrState;
	u8 HwState;
	adcPwrState.FuncIdx = ADC0;
	QueryRegPwrState(adcPwrState.FuncIdx, &(adcPwrState.PwrState), &HwState);
	// if the power state isn't ACT, then switch the power state back to ACT first
	if ((adcPwrState.PwrState != ACT) && (adcPwrState.PwrState != INACT)) {
		ADCEnablePS();
		QueryRegPwrState(adcPwrState.FuncIdx, &(adcPwrState.PwrState), &HwState);
	}
	if (adcPwrState.PwrState == ACT) {
		adcPwrState.PwrState = INACT;
		RegPowerState(adcPwrState);
	}
#endif
    /* Turn on DAC active clock */
    ACTCK_ADC_CCTRL(OFF);
    /* Enable DAC1 module */
    ADC0_FCTRL(OFF);
    /* To release DAC delta sigma clock gating */
    HAL_WRITE32(SYSTEM_CTRL_BASE, REG_SYS_SYSPLL_CTRL2,
    		HAL_READ32(SYSTEM_CTRL_BASE, REG_SYS_SYSPLL_CTRL2) & (~(BIT25)));
}

