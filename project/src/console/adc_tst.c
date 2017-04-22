/*
 * adc_tst.c
 *
 *  Created on: 04/04/2017.
 *      Author: pvvx
 */

#include <platform_opts.h>
#include "rtl8195a.h"
#include "FreeRTOS.h"
#include "rtl8195a/rtl_libc.h"

//------------------------------------------------------------------------------
#include "objects.h"
#include "PinNames.h"
#include "hal_adc.h"
#include "analogin_api.h"
#include "strproc.h"
//------------------------------------------------------------------------------
analogin_t adc;

LOCAL void fATADI(int argc, char *argv[]) {

		int count = 8;
		int channel = 2;
		union {
			unsigned int 	ui[2];
			unsigned short 	us[4];
		}x;
		uint16_t adcdat;
		memset(&adc, 0, sizeof(adc));


	//	ConfigDebugErr	|= (_DBG_ADC_|_DBG_GDMA_);
	//	ConfigDebugInfo	|= (_DBG_ADC_|_DBG_GDMA_);

		if (argc > 1) {
			channel = atoi(argv[1]);
			channel &= 0x03;
			if(!channel) channel = 2;
		}

		if (argc > 2) {
			count = atoi(argv[2]);
		}

		analogin_init(&adc, (channel+1) | (PORT_V << 4));

	    PSAL_ADC_HND pSalADCHND = &((&(adc.SalADCMngtAdpt))->pSalHndPriv->SalADCHndPriv);

		uint32_t sum = 0;
		for (uint32_t i = 1; i <= count; i++) {
			RtkADCReceiveBuf(pSalADCHND, &x.ui);
			adcdat = x.us[channel];
			if((i % 8) == 0 || (i == count)) {
				printf("0x%04x\n", adcdat);
			} else  {
				printf("0x%04x, ", adcdat);
			}
			sum += adcdat;
		}
		analogin_deinit(&adc);
		printf("ADC%d = 0x%04x\n", channel, sum / count);
	//	sys_adc_calibration(0, &channel, &count);
}

LOCAL void fATADD(int argc, char *argv[]) {

	int count = 64;
	int channel = 2;
	uint16_t adcdat;
	memset(&adc, 0, sizeof(adc));


//	ConfigDebugErr	|= (_DBG_ADC_|_DBG_GDMA_);
//	ConfigDebugInfo	|= (_DBG_ADC_|_DBG_GDMA_);

	if (argc > 1) {
		channel = atoi(argv[1]);
		channel &= 0x03;
		if(!channel) channel = 1;
	}

	if (argc > 2) {
		count = atoi(argv[2]);
		if (count <= 2) {
			count = 64;
		}
	};

	analogin_init(&adc, (channel+1) | (PORT_V << 4));
	SAL_ADC_TRANSFER_BUF trbuf;
	trbuf.pDataBuf = zalloc(count*4);
	if(trbuf.pDataBuf) {
		trbuf.DataLen = count/2; // x32 bit ?
		trbuf.RSVD = 0;
		adc.SalADCHndPriv.SalADCHndPriv.pRXBuf = &trbuf;
		adc.SalADCHndPriv.SalADCHndPriv.OpType = ADC_DMA_TYPE;

		adc.HalADCInitData.ADCEndian = ADC_DATA_ENDIAN_LITTLE;	//ADC endian selection,
        //but actually it's for 32-bit ADC data swap control
        //1'b0: no swap,
        //1'b1: swap the upper 16-bit and the lower 16-bit
//		adc.HalADCInitData.ADCCompOnly = ADC_FEATURE_DISABLED;    //ADC compare mode only enable (without FIFO enable)
//		adc.HalADCInitData.ADCEnManul = ADC_FEATURE_ENABLED; // ADC_FEATURE_DISABLED;     //ADC enable manually
//		adc.HalADCInitData.ADCIdx = channel+1;	//ADC index used (1..3 ?)
//		adc.HalADCInitData.ADCBurstSz = 8;		//ADC DMA operation threshold
//		adc.HalADCInitData.ADCOneShotTD = 8;	//ADC one shot mode threshold
//		adc.HalADCInitData.ADCDataRate = 0; // 0xff;	// ADC down sample data rate ??
		adc.HalADCInitData.ADCAudioEn = ADC_FEATURE_ENABLED; 		//ADC audio mode enable // ADC_FEATURE_DISABLED
//		adc.HalADCInitData.ADCOneShotEn = ADC_FEATURE_DISABLED; 	//ADC one shot mode threshold
		adc.HalADCInitData.ADCInInput = ADC_FEATURE_ENABLED;		//ADC Input is internal?
//		adc.HalADCInitData.ADCEn = ADC_DISABLE; //ADC_ENABLE;

		HalADCInit8195a(&adc.HalADCInitData);
	    /* Read Content */
		HAL_ADC_READ32(REG_ADC_FIFO_READ);
		HAL_ADC_READ32(REG_ADC_INTR_STS);
		RtkADCReceive(&adc.SalADCHndPriv.SalADCHndPriv);
		while(adc.SalADCHndPriv.SalADCHndPriv.DevSts  != ADC_STS_IDLE);
		uint16 * ptr = (uint16 *) trbuf.pDataBuf;
		// RtkADCDMAInit(&adc.SalADCHndPriv.SalADCHndPriv);
		for (uint32_t i = 1; i <= count; i++) {
			if((i % 16) == 0 || (i == count)) {
				printf("%04x\n", *ptr);
			} else  {
				printf("%04x ", *ptr);
			}
			ptr++;
		}
		uint32_t sum = 0;
		ptr = (uint16 *) trbuf.pDataBuf;
		for (uint32_t i = 1; i <= count; i++) {
			printf("%d\n", *ptr);
			sum += *ptr;
			ptr++;
			if((i%512)==0) vTaskDelay(10);
		}
/*
		printf("OpType:\t\t%p\n", adc.SalADCHndPriv.SalADCHndPriv.OpType);
		printf("pRXBuf:\t\t%p\n", adc.SalADCHndPriv.SalADCHndPriv.pRXBuf);
		printf("pDataBuf:\t%p\n", adc.SalADCHndPriv.SalADCHndPriv.pRXBuf->pDataBuf);
		printf("DataLen:\t%p\n", adc.SalADCHndPriv.SalADCHndPriv.pRXBuf->DataLen);
		printf("ADCDataRate:\t%p\n", adc.HalADCInitData.ADCDataRate);
		printf("ADCData:\t%p\n", adc.HalADCInitData.ADCData);
		printf("ADCIdx:\t\t%p\n", adc.HalADCInitData.ADCIdx);
		printf("ADCPWCtrl:\t%p\n", adc.HalADCInitData.ADCPWCtrl);
		printf("ADCAnaParAd3:\t%p\n", adc.HalADCInitData.ADCAnaParAd3);
		printf("ADC%d = 0x%04x\n", channel, analogin_read_u16(&adc));
		printf("ADC%d = 0x%04x\n", channel, analogin_read_u16(&adc));
*/
		analogin_deinit(&adc);
		free(trbuf.pDataBuf);
		printf("ADC%d = 0x%04x\n", channel, sum / count);

	}
	else {
		error_printf("%s: malloc failed!\n", __func__);
	};

//	sys_adc_calibration(0, &channel, &count);
}

LOCAL void fATADC(int argc, char *argv[]) {

	int count = 8;
	int channel = 2;
	uint16_t adcdat;
	memset(&adc, 0, sizeof(adc));


//	ConfigDebugErr	|= (_DBG_ADC_|_DBG_GDMA_);
//	ConfigDebugInfo	|= (_DBG_ADC_|_DBG_GDMA_);

	if (argc > 1) {
		channel = atoi(argv[1]);
		channel &= 0x03;
		if(!channel) channel = 1;
	}

	if (argc > 2) {
		count = atoi(argv[2]);
	}

	analogin_init(&adc, (channel+1) | (PORT_V << 4));

	uint32_t sum = 0;
	for (uint32_t i = 1; i <= count; i++) {
		adcdat = analogin_read_u16(&adc);
		if((i % 8) == 0 || (i == count)) {
			printf("0x%04x\n", adcdat);
		} else  {
			printf("0x%04x, ", adcdat);
		}
		sum += adcdat;
	}
	analogin_deinit(&adc);
	printf("ADC%d = 0x%04x\n", channel, sum / count);
//	sys_adc_calibration(0, &channel, &count);
}

LOCAL void fATSA(int argc, char *argv[]) {
//	u32 tConfigDebugInfo = ConfigDebugInfo;
	int channel;
	char *ptmp;
	u16 offset, gain, adcdat;
	memset(&adc, 0, sizeof(adc));

	if (argc < 2) {
		printf("Usage: ATSA=CHANNEL(0~2)\n");
		printf("Usage: ATSA=k_get\n");
		printf("Usage: ATSA=k_set[offet(hex),gain(hex)]\n");
		return;
	}

	if (strcmp(argv[1], "k_get") == 0) {
		sys_adc_calibration(0, &offset, &gain);
//		printf("[ATSA] offset = 0x%04X, gain = 0x%04X", offset, gain);
	} else if (strcmp(argv[1], "k_set") == 0) {
		if (argc != 4) {
			printf("Usage: ATSA=k_set[offet(hex),gain(hex)]\n");
			return;
		}
		offset = strtoul(argv[2], &ptmp, 16);
		gain = strtoul(argv[3], &ptmp, 16);
		sys_adc_calibration(1, &offset, &gain);
//		printf("[ATSA] offset = 0x%04X, gain = 0x%04X", offset, gain);
	} else {
		channel = atoi(argv[1]);
		if (channel < 0 || channel > 2) {
			printf("Usage: ATSA=CHANNEL(0~2)\n");
			return;
		}
		// Remove debug info massage
//		ConfigDebugInfo = 0;
		if (channel == 0)
			analogin_init(&adc, AD_1);
		else if (channel == 1)
			analogin_init(&adc, AD_2);
		else
			analogin_init(&adc, AD_3);
//		analogin_read_u16(&adc);
		adcdat = analogin_read_u16(&adc) >> 4;
		analogin_deinit(&adc);
		// Recover debug info massage
//		ConfigDebugInfo = tConfigDebugInfo;

		printf("A%d = 0x%04X\n", channel, adcdat);
	}
}

//------------------------------------------------------------------------------
MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_adc[] = {
		{ "ATADC", 0, fATADC, ": ADC Test" },
		{ "ATADD", 0, fATADD, ": ADC DMA Test" },
		{ "ATADI", 0, fATADI, ": ADC Irq Test" },
		{ "ATSA" , 0, fATSA , ": ADC at" }
};
