/*
 * rtl8195a_sdio_host_open.c
 *
 *  Created on: Jul 12, 2017
 *      Author: sharikov
 */

#include <stdint.h>
#include "rtl8195a.h"
#include "hal_timer.h"
#include "hal_sdio_host.h"
#include "rtl8195a_sdio_host.h"

#if 1

#define HAL_SDIOH_REG32(a) (*(volatile unsigned int *)(SDIO_HOST_REG_BASE+a))
#define HAL_SDIOH_REG16(a) (*(volatile unsigned short *)(SDIO_HOST_REG_BASE+a))
#define HAL_SDIOH_REG8(a) (*(volatile unsigned char *)(SDIO_HOST_REG_BASE+a))

#define SD_CHK_TIMEOUT 3225

//-----
HAL_Status SdioHostSdClkCtrl(void *Data, int En, u8 Divisor) { // SD_CLK_DIVISOR
	PHAL_SDIO_HOST_ADAPTER psha = Data; // u8 *v3; // r3@1 	v3 = Data;
	if (HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE)
			& (PRES_STATE_CMD_INHIBIT_CMD | PRES_STATE_CMD_INHIBIT_DAT)) {
		return HAL_BUSY;
	} else {
		HAL_SDIOH_REG16(REG_SDIO_HOST_CLK_CTRL) &= ~CLK_CTRL_SD_CLK_EN;
		if (!En)
			return HAL_OK;
		else {
			switch(Divisor) {
			case BASE_CLK:
				psha->CurrSdClk = SD_CLK_41_6MHZ;
				break;
			case BASE_CLK_DIVIDED_BY_2:
				psha->CurrSdClk = SD_CLK_20_8MHZ;
				break;
			case BASE_CLK_DIVIDED_BY_4:
				psha->CurrSdClk = SD_CLK_10_4MHZ;
				break;
			case BASE_CLK_DIVIDED_BY_8:
				psha->CurrSdClk = SD_CLK_5_2MHZ;
				break;
			case BASE_CLK_DIVIDED_BY_16:
				psha->CurrSdClk = SD_CLK_2_6MHZ;
				break;
			case BASE_CLK_DIVIDED_BY_32:
				psha->CurrSdClk = SD_CLK_1_3MHZ;
				break;
			case BASE_CLK_DIVIDED_BY_64:
				psha->CurrSdClk = SD_CLK_650KHZ;
				break;
			case BASE_CLK_DIVIDED_BY_128:
				psha->CurrSdClk = SD_CLK_325KHZ;
				break;
			case BASE_CLK_DIVIDED_BY_256:
				psha->CurrSdClk = SD_CLK_162KHZ;
				break;
			default:
				DBG_SDIO_ERR("Unsupported SDCLK divisor!\n");
				Divisor = 0;
				psha->CurrSdClk = SD_CLK_41_6MHZ;
			}
		}
		HAL_SDIOH_REG16(REG_SDIO_HOST_CLK_CTRL) = (Divisor << 8) |                      CLK_CTRL_INTERAL_CLK_EN;
		HAL_SDIOH_REG16(REG_SDIO_HOST_CLK_CTRL) = (Divisor << 8) | CLK_CTRL_SD_CLK_EN | CLK_CTRL_INTERAL_CLK_EN;
	}
	return HAL_OK;
}

static HAL_Status CheckSramAddress(void* address)
{
	// SRAM: 0x10000000... 0x1006ffff
	if ((uint32_t)address >= 0x10070000 || (uint32_t)address < 0x10000000 || ((uint32_t)address & 3) !=0)
		return HAL_ERR_PARA;
	return HAL_OK;
}

//-----SdioHostIsTimeout(StartCount, TimeoutCnt)
HAL_Status SdioHostIsTimeout(u32 StartCount, u32 TimeoutCnt) {
	uint32_t t1, t2;
	HAL_Status result;

	t1 = HalTimerOp.HalTimerReadCount(1);
	t2 = StartCount - t1;
	if (StartCount < t1)
		t2--;
	if (TimeoutCnt >= t2)
		result = HAL_OK;
	else
		result = HAL_TIMEOUT;
	return result;
}

//----- SdioHostSendCmd(PSDIO_HOST_CMD)
void SdioHostSendCmd(PSDIO_HOST_CMD Cmd) {
	//u16 reg_cmd = (*(u8 *) &Cmd->CmdFmt & 0x3B) | (*(u8 *) &Cmd->CmdFmt & 0xC0)
	//		| ((*((u8 *)&Cmd->CmdFmt + 1) & 0x3F) << 8);
	Cmd->CmdFmt.Rsvd0=0;
	Cmd->CmdFmt.Rsvd1=0;
	HAL_SDIOH_REG32(REG_SDIO_HOST_ARG) = Cmd->Arg; // 40058008 = Cmd->Arg
	HAL_SDIOH_REG16(REG_SDIO_HOST_CMD) = *(u16 *) &Cmd->CmdFmt; // & 0x3FFB;
}

//-----
HAL_Status SdioHostGetResponse(void *Data, int RspType) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;

	if (psha) {
		psha->Response[0] = HAL_SDIOH_REG32(REG_SDIO_HOST_RSP0); // 40058010;
		psha->Response[1] = HAL_SDIOH_REG32(REG_SDIO_HOST_RSP2);
		if (RspType == 1) {
			psha->Response[2] = HAL_SDIOH_REG32(REG_SDIO_HOST_RSP4);
			psha->Response[3] = HAL_SDIOH_REG32(REG_SDIO_HOST_RSP6);
		}
		result = HAL_OK;
	} else
		result = HAL_ERR_PARA;
	return result;
}



#if 1
//-----
//void SdioHostSdBusPwrCtrl(uint8_t En) {
void SdioHostSdBusPwrCtrl(void) {
	HAL_SDIOH_REG8(REG_SDIO_HOST_PWR_CTRL) &= ~ PWR_CTRL_SD_BUS_PWR;
	if (HAL_SDIOH_REG32(REG_SDIO_HOST_CAPABILITIES) & CAPA_VOLT_SUPPORT_18V) {
		DBG_SDIO_WARN("Supply SD bus voltage: 1.8V\n");
		HAL_SDIOH_REG8(REG_SDIO_HOST_PWR_CTRL) = VOLT_18V << 1;
	}
	else if (HAL_SDIOH_REG32(REG_SDIO_HOST_CAPABILITIES) & CAPA_VOLT_SUPPORT_30V) {
		DBG_SDIO_WARN("Supply SD bus voltage: 3.0V\n");
		HAL_SDIOH_REG8(REG_SDIO_HOST_PWR_CTRL) = VOLT_30V << 1;
	}
	else if (HAL_SDIOH_REG32(REG_SDIO_HOST_CAPABILITIES) & CAPA_VOLT_SUPPORT_33V) {
		DBG_SDIO_WARN("Supply SD bus voltage: 3.3V\n");
		HAL_SDIOH_REG8(REG_SDIO_HOST_PWR_CTRL) = VOLT_33V << 1;
	}
	else DBG_SDIO_ERR("No supported voltage\n");
	HAL_SDIOH_REG8(REG_SDIO_HOST_PWR_CTRL) |= PWR_CTRL_SD_BUS_PWR;
}

//----- (000005D8) --------------------------------------------------------
HAL_Status SdioHostErrIntRecovery(void *Data) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	int t;
	int result= HAL_OK;

	if (!psha)
		return HAL_ERR_PARA;

	DBG_SDIO_ERR("Recovering error interrupt...\n");
	u16 ierr = HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS); // v40058032;

	if ( HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) &
			(ERR_INT_STAT_CMD_TIMEOUT |
					ERR_INT_STAT_CMD_CRC |
					ERR_INT_STAT_CMD_END_BIT |
					ERR_INT_STAT_CMD_IDX) )
	{
		HAL_SDIOH_REG8(REG_SDIO_HOST_SW_RESET) |= SW_RESET_FOR_CMD;
		t = 0;
		while((HAL_SDIOH_REG8(REG_SDIO_HOST_SW_RESET) & SW_RESET_FOR_CMD)) {
			if(++t > 1000) {
				DBG_SDIO_ERR("CMD line reset timeout!\n");
				result= HAL_TIMEOUT;
				goto RECOVERY_END;
			}
		}
	}
	if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS)
			& (ERR_INT_STAT_DATA_TIMEOUT
					| ERR_INT_STAT_DATA_CRC
					| ERR_INT_STAT_DATA_END_BIT)) {
		HAL_SDIOH_REG8(REG_SDIO_HOST_SW_RESET) |= SW_RESET_FOR_DAT;
		t = 0;
		while ((HAL_SDIOH_REG8(REG_SDIO_HOST_SW_RESET) & SW_RESET_FOR_DAT)) {
			if (++t > 1000) {
				DBG_SDIO_ERR("DAT line reset timeout!\n");
				result= HAL_TIMEOUT;
				goto RECOVERY_END;
			}
		}
	}

	DBG_SDIO_ERR("Error interrupt status: 0x%04X\n", HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS));

	HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = ierr;
	psha->ErrIntFlg = 0;
	result = HalSdioHostStopTransferRtl8195a(psha);
	if (result == HAL_OK) {
		t = 0;
		while (HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE)
				& (PRES_STATE_CMD_INHIBIT_CMD | PRES_STATE_CMD_INHIBIT_DAT)) {
			if(++t > 1000)
				break;
		}
		if(HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS)
				& (   ERR_INT_STAT_CMD_TIMEOUT
						| ERR_INT_STAT_CMD_CRC
						| ERR_INT_STAT_CMD_END_BIT
						| ERR_INT_STAT_CMD_IDX)) {
			DBG_SDIO_ERR("Non-recoverable error(1)!\n");
			result= HAL_ERR_UNKNOWN;
			goto RECOVERY_END;
		}
		if(HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS)
				&  ERR_INT_STAT_DATA_TIMEOUT) {
			DBG_SDIO_ERR("Non-recoverable error(2)!\n");
			result= HAL_ERR_UNKNOWN;
			goto RECOVERY_END;
		}
		HalDelayUs(50);
		//if((HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE) & 0xF00000) != 0xF00000) {
		if((HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE) & 0xF00000) != 0x900000) {
			DBG_SDIO_ERR("Non-recoverable error(3)!\n");
			result= HAL_ERR_UNKNOWN;
			goto RECOVERY_END;
		}
		DBG_SDIO_ERR("Recoverable error...\n");
		HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_SIG_EN) = 0x17F;
		result = 16;
		goto RECOVERY_END;
	}
	DBG_SDIO_ERR("Stop transmission error!\n");
	result = HAL_ERR_UNKNOWN;

	RECOVERY_END:
	HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_STATUS) = HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_STATUS);
	HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS);

	HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_SIG_EN) =
			NOR_INT_SIG_EN_CMD_COMP
			| NOR_INT_SIG_EN_XFER_COMP
			| NOR_INT_SIG_EN_CARD_REMOVAL
			| NOR_INT_SIG_EN_CARD_INSERT;	// 0xc3;

	HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_SIG_EN)=
			ERR_INT_STAT_EN_CMD_TIMEOUT
			| ERR_INT_STAT_EN_CMD_CRC
			| ERR_INT_STAT_EN_CMD_END_BIT
			| ERR_INT_STAT_EN_CMD_IDX
			| ERR_INT_STAT_EN_DATA_TIMEOUT
			| ERR_INT_STAT_EN_DATA_CRC
			| ERR_INT_STAT_EN_DATA_END_BIT
			| ERR_INT_STAT_EN_AUTO_CMD; 	// 0x17F;
	return result;
}
#endif
// 23D4: using guessed type int DiagPrintf(const char *, ...);
// 23F0: using guessed type int  HalDelayUs(u32);
#if 1
void SdioHostIsrHandle(void *Data) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;

	u32 status = HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_STATUS);// v40058030;
	HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_STATUS) = status & 255;

	HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_SIG_EN) = 0;

	if (status) {
		if (status & NOR_INT_STAT_CMD_COMP)
			psha->CmdCompleteFlg = 1;

		psha->errType = 0;
		if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS))
			psha->errType = 0xff;
		if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) & ERR_INT_STAT_EN_CMD_TIMEOUT)
			psha->errType = SDIO_ERR_CMD_TIMEOUT;
		if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) & ERR_INT_STAT_DATA_CRC)
			psha->errType = SDIO_ERR_DAT_CRC;

		if (status & NOR_INT_STAT_XFER_COMP) {
			psha->XferCompleteFlg = 1;
			if ((status & NOR_INT_STAT_ERR_INT) == 0) {
				if (psha->XferCompCallback)
					psha->XferCompCallback(psha->XferCompCbPara);
			}
			else
				if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS)) {

					DBG_SDIO_ERR("XFER CP with ErrIntVal: 0x%04X /0x%04X -- TYPE 0x%02X\n",
							status,
							HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS),
							psha->CmdCompleteFlg);

					if (psha->ErrorCallback)
						psha->ErrorCallback(psha->ErrorCbPara);
				}
		}

		if (status & NOR_INT_STAT_CARD_INSERT) // 0x40
		{
			SdioHostSdClkCtrl(psha, 1, BASE_CLK_DIVIDED_BY_128); // BASE_CLK_DIVIDED_BY_128
			SdioHostSdBusPwrCtrl();
			if (psha->CardInsertCallBack)
				psha->CardInsertCallBack(psha->CardInsertCbPara);
		}
		if (status & NOR_INT_STAT_CARD_REMOVAL) // 0x80
		{
			HAL_SDIOH_REG8(REG_SDIO_HOST_PWR_CTRL) &= ~PWR_CTRL_SD_BUS_PWR;
			SdioHostSdClkCtrl(psha, 0, BASE_CLK); // BASE_CLK
			if (psha->CardRemoveCallBack)
				psha->CardRemoveCallBack(psha->CardRemoveCbPara);
		}
		if (status & NOR_INT_STAT_CARD_INT) // 0x100 )
		{
			u16 val = HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_STATUS_EN);
			HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_STATUS_EN) = val & (~NOR_INT_STAT_EN_CARD_INT);
			DBG_SDIO_ERR("CARD INT: 0x%04X\n", status);
			HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_STATUS_EN) = val;
		}
		if (status & NOR_INT_STAT_ERR_INT) // 0x8000 )
		{
			HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_SIG_EN) = 0;
			u16 err = HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS);

			DBG_SDIO_ERR("XFER CP with ErrIntVal: 0x%04X /0x%04X -- TYPE 0x%02X\n",
					status,
					err,
					psha->CmdCompleteFlg);
			psha->ErrIntFlg = 1;
			return;
			/*
			if (psha->errType != SDIO_ERR_CMD_TIMEOUT)
				return;
			else {
				HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) =0xffff;
				HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_SIG_EN)=
						ERR_INT_STAT_EN_CMD_TIMEOUT
						| ERR_INT_STAT_EN_CMD_CRC
						| ERR_INT_STAT_EN_CMD_END_BIT
						| ERR_INT_STAT_EN_CMD_IDX
						| ERR_INT_STAT_EN_DATA_TIMEOUT
						| ERR_INT_STAT_EN_DATA_CRC
						| ERR_INT_STAT_EN_DATA_END_BIT
						| ERR_INT_STAT_EN_AUTO_CMD; 	// 0x17F;
			}
			 */

			/*if (psha->CmdCompleteFlg) {
				SdioHostErrIntRecovery(psha);
				goto ir_end;
			}
			DiagPrintf("\r[SDIO Err]Read/Write command Error\n");
			 */
		}
	}
//	ir_end:
	HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_SIG_EN)
	= NOR_INT_SIG_EN_CMD_COMP
	| NOR_INT_SIG_EN_XFER_COMP
	| NOR_INT_SIG_EN_CARD_REMOVAL
	| NOR_INT_SIG_EN_CARD_INSERT;	// 0xc3;  */
}
#else
extern void SdioHostIsrHandle(void *Data);
#endif




#if 1
//----- SdioHostChkDataLineActive
HAL_Status SdioHostChkDataLineActive(void) {
	HAL_Status result = HAL_OK;
	uint32_t t1 = HalTimerOp.HalTimerReadCount(1);
	do {
		if ((HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE)
				& PRES_STATE_DAT_LINE_ACTIVE) == 0)
			break;
		result = SdioHostIsTimeout(t1, SD_CHK_TIMEOUT);
	} while (result != HAL_TIMEOUT);
	return result;
}

//----- SdioHostChkCmdInhibitCMD
HAL_Status SdioHostChkCmdInhibitCMD(void) {
	HAL_Status result = HAL_OK;
	uint32_t t1 = HalTimerOp.HalTimerReadCount(1);
	do {
		if ((HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE)
				& PRES_STATE_CMD_INHIBIT_CMD) == 0)
			break;
		result = SdioHostIsTimeout(t1, SD_CHK_TIMEOUT);
	} while (result != HAL_TIMEOUT);
	return result;
}
//----- SdioHostChkCmdInhibitDAT
int SdioHostChkCmdInhibitDAT(void) {
	HAL_Status result = HAL_OK;
	uint32_t t1 = HalTimerOp.HalTimerReadCount(1);
	do {
		if ((HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE)
				& PRES_STATE_CMD_INHIBIT_DAT) == 0)
			break;
		result = SdioHostIsTimeout(t1, SD_CHK_TIMEOUT);
	} while (result != HAL_TIMEOUT);
	return result;
}

//----- SdioHostChkXferComplete
HAL_Status SdioHostChkXferComplete(void *Data)
{
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	uint32_t StartCount;
	HAL_Status result;
	if (psha) {
		StartCount = HalTimerOp.HalTimerReadCount(1);
		while((psha->XferCompleteFlg == 0)
				/*&& ((HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE)
						& PRES_STATE_DAT0_SIGNAL_LEVEL) == 0) */) {
			if(psha->ErrIntFlg)
				return SdioHostErrIntRecovery(psha);
			result = SdioHostIsTimeout(StartCount, SD_CHK_TIMEOUT*50);
			if (result == HAL_TIMEOUT)
				return result;
		}
		result = HAL_OK;
	} else {
		result = HAL_ERR_PARA;
	}
	return result;
}
//----- SdioHostChkCmdComplete
HAL_Status SdioHostChkCmdComplete(void *Data, uint32_t Timeout) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	uint32_t TimeoutCnt;
	uint32_t StartCount;
	HAL_Status result;


	if (psha) {
		if (Timeout == SDIO_HOST_WAIT_FOREVER) {
			StartCount = 0;
			TimeoutCnt = 0;
		} else {
			TimeoutCnt = 1000 * Timeout / 31;
			StartCount = HalTimerOp.HalTimerReadCount(1); //	v6 = (*((int (**)(u32)) &HalTimerOp + 2))(1);
		}
		while(psha->CmdCompleteFlg == 0) {
			if (psha->ErrIntFlg )
				return SdioHostErrIntRecovery(psha);
			if(TimeoutCnt) {
				result = SdioHostIsTimeout(StartCount, TimeoutCnt);
				if (result == HAL_TIMEOUT)
					return result;
			} else if(Timeout == 0) {
				result = HAL_BUSY;
				return result;
			}
		}
		result = HAL_OK;
	} else
		result = HAL_ERR_PARA;
	return result;
}
#endif


#if 1
//----- (00000D34) --------------------------------------------------------
// Mode: 0=test / 1=set
// Fn1Sel: адрес буфера Switch Function Status
// Fn2Sel: включаемая функция. доступна только группа 1, остальные маскируются
// StatusBuf: не используется (убрать!)
// проверено в режиме test (Mode=0)
HAL_Status SdioHostSwitchFunction(void *Data, int Mode, int Fn2Sel, int Fn1Sel,
		uint8_t *StatusBuf)
{
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result; // r0@3
	SDIO_HOST_CMD Cmd; // [sp+0h] [bp-20h]@1
	ADMA2_DESC_FMT *pAdmaDescTbl = psha->AdmaDescTbl;

	result = CheckSramAddress(pAdmaDescTbl);
	if (result)
		return result;
	result = CheckSramAddress((void*)Fn1Sel);
	if (result)
		return result;

	HAL_SDIOH_REG32(REG_SDIO_HOST_ADMA_SYS_ADDR) = (uint32_t)pAdmaDescTbl;
	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_SIZE) = 64;
	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_CNT) =1;
	HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) =
			XFER_MODE_DATA_XFER_DIR
			| XFER_MODE_DMA_EN ;

	//*(u16 *) (v6 + 2) = 64;
	pAdmaDescTbl->Len1 = 64;

	//v9 = *(u8 *) v6;
	//*(u8 *) v6 = ((v9 | 3) & 0xFB | 4 * ((Fn1Sel | v6) & 1)) & 0xEF
	//				| 16 * ((Fn1Sel | v6) & 1) | 0x20;
	pAdmaDescTbl->Attrib1.End=1;
	pAdmaDescTbl->Attrib1.Valid =1;
	pAdmaDescTbl->Attrib1.Int = 0;
	pAdmaDescTbl->Attrib1.Act1= 0;
	pAdmaDescTbl->Attrib1.Act2=1;
	// *(u32 *) (v6 + 4) = Fn1Sel;
	pAdmaDescTbl->Addr1 = Fn1Sel;

	result = SdioHostChkCmdInhibitCMD();//v6);
	if (result)
		return result;
	result = SdioHostChkDataLineActive();
	if (result)
			return result;

	//Cmd.CmdFmt = (SDIO_HOST_CMD_FMT) ((*(u8 *) &Cmd.CmdFmt & 0xF4
	//					| 0x3A) & 0x3F);
	//v11 = *((u8 *) &Cmd.CmdFmt + 1);
	Cmd.CmdFmt.RespType = RSP_LEN_48; //2
	Cmd.CmdFmt.Rsvd0 =0;
	Cmd.CmdFmt.CmdCrcChkEn =1;
	Cmd.CmdFmt.CmdIdxChkEn =1;
	Cmd.CmdFmt.DataPresent =1;
	Cmd.CmdFmt.CmdType = NORMAL;

	//v5[128] = 0;
	//v5[129] = 0;
	psha->CmdCompleteFlg =0;
	psha->XferCompleteFlg =0;

	//	*((u8 *) &Cmd.CmdFmt + 1) = v11 & 0xC0 | 6;
	Cmd.CmdFmt.CmdIdx = CMD_SWITCH_FUNC;  // CMD6
	Cmd.CmdFmt.Rsvd1 = 0;
	//Cmd.Arg = v8 | 0xFFFFF0 | (v7 << 31);
	// от Fn2Sel оставили только 0
	Cmd.Arg = Fn2Sel | 0xFFFFF0 | (Mode << 31);

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result)
		return result;
	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	result = SdioHostChkXferComplete(psha);
	if (result) {
		if (result != 16) {
			if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) & ERR_INT_STAT_ADMA) {
				HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = ERR_INT_STAT_ADMA;
				if (HalSdioHostStopTransferRtl8195a(psha)) {
					DBG_SDIO_ERR("Stop transmission error!\n");
				}
			}
		}
		result = HAL_ERR_UNKNOWN;
	}
	return result;
}
#endif

#if 1
HAL_Status HalSdioHostChangeSdClockRtl8195a(IN VOID *Data, IN uint8_t Frequency) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;
	uint8_t Divisor;
	volatile uint8_t *StatusData=NULL;
	PADMA2_DESC_FMT	pAdmaDescTbl;
	SDIO_HOST_CMD Cmd;

	if (!psha || Frequency > SD_CLK_41_6MHZ)
		return HAL_ERR_PARA;

	//StatusData = malloc(64);
	//if (CheckSramAddress(StatusData) != HAL_OK)
	//	return HAL_ERR_UNKNOWN;  // буфер не годится для ADMA2

	if (psha->CurrSdClk == Frequency) {
		DBG_SDIO_WARN("Current SDCLK frequency is already the specified value...\n");
		result = HAL_OK;
		goto LABEL_EXIT;
	}
	if (Frequency != SD_CLK_41_6MHZ) {
		if (Frequency == SD_CLK_10_4MHZ)
			Divisor = BASE_CLK_DIVIDED_BY_4;	// 10.4 MHz
		else if (Frequency == SD_CLK_20_8MHZ)
			Divisor = BASE_CLK_DIVIDED_BY_2;	// 20.8 MHz
		else if (Frequency != SD_CLK_5_2MHZ) {
			DBG_SDIO_ERR("Unsupported SDCLK frequency!\n");
			result = HAL_ERR_PARA;
			goto LABEL_EXIT;
		}
		Divisor = BASE_CLK_DIVIDED_BY_8; // 5.2 MHZ

		result = SdioHostSdClkCtrl(psha, 1, Divisor);
		if (result != HAL_OK) {
			DBG_SDIO_ERR("Host changes clock fail!\n");
			goto LABEL_EXIT;
		}
		HalDelayUs(20);
	}

	pAdmaDescTbl = psha->AdmaDescTbl;
	if (CheckSramAddress(pAdmaDescTbl) != HAL_OK) {
		result = HAL_ERR_PARA;
		goto LABEL_EXIT;
	}

	// подразумевается что psha->AdmaDescTbl = gAdmaTbls
	// здесь используется только первый дескриптор,
	// 64 байта начиная со второго дескриптора используем как буфер данных
	// это грязный хак но из-за ограничений ADMA2 буфер нельза размещать на стеке
	// а malloc не гарантирует размещение в SRAM в любых ситуациях
	StatusData = (uint8_t*)pAdmaDescTbl + sizeof(ADMA2_DESC_FMT);

	HAL_SDIOH_REG32(REG_SDIO_HOST_ADMA_SYS_ADDR) = (uint32_t)pAdmaDescTbl;
	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_SIZE) = 8; //	v40058004 = 8;
	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_CNT) =1;
	HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) =
				XFER_MODE_DATA_XFER_DIR
				//| XFER_MODE_MULT_SINGLE_BLK
				//| XFER_MODE_BLK_CNT_EN
				//| XFER_MODE_AUTO_CMD12_EN
				| XFER_MODE_DMA_EN ;

	pAdmaDescTbl->Attrib1.End=1;
	pAdmaDescTbl->Attrib1.Valid =1;
	pAdmaDescTbl->Attrib1.Int = 0;
	pAdmaDescTbl->Attrib1.Act1= 0;
	pAdmaDescTbl->Attrib1.Act2=1;
	pAdmaDescTbl->Addr1 = (uint32_t)StatusData;
	pAdmaDescTbl->Len1 = 8;

	result = SdioHostChkCmdInhibitCMD();
	if (result != HAL_OK)
		goto LABEL_EXIT;

	Cmd.CmdFmt.RespType = RSP_LEN_48; //2
	Cmd.CmdFmt.Rsvd0 =0;
	Cmd.CmdFmt.CmdCrcChkEn =1;
	Cmd.CmdFmt.CmdIdxChkEn =1;
	Cmd.CmdFmt.DataPresent =0;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = CMD_APP_CMD;  // 0x37 = CMD55
	Cmd.CmdFmt.Rsvd1 = 0;
	psha->CmdCompleteFlg =0;
	Cmd.Arg = psha->RCA <<16;

	SdioHostSendCmd(&Cmd);

	result = SdioHostChkCmdComplete(psha, 50);
	if (result != HAL_OK)
		goto LABEL_EXIT;

	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	// проверим команду
	if ((psha->Response[1] & 255) != CMD_APP_CMD) {
		DBG_SDIO_ERR("Command index error!\n");
		goto LABEL_EXIT;
	}
	if (!(psha->Response[0] & 0x20)) {
		DBG_SDIO_ERR("ACMD isn't expected!\n");
		goto LABEL_EXIT;
	}

	result = SdioHostChkCmdInhibitCMD();
	if (result)
		goto LABEL_EXIT;
	result = SdioHostChkDataLineActive();
	if (result)
		goto LABEL_EXIT;

	// прочитаем SCR

	//memset(StatusData, 0, 64);

	Cmd.CmdFmt.RespType = RSP_LEN_48; //2
	Cmd.CmdFmt.Rsvd0 =0;
	Cmd.CmdFmt.CmdCrcChkEn =1;
	Cmd.CmdFmt.CmdIdxChkEn =1;
	Cmd.CmdFmt.DataPresent =1;
	Cmd.CmdFmt.CmdType = NORMAL;

	psha->CmdCompleteFlg =0;
	psha->XferCompleteFlg =0;

	Cmd.CmdFmt.CmdIdx = CMD_SEND_SCR;  // ACMD51
	Cmd.CmdFmt.Rsvd1 =0;
	Cmd.Arg = 0;

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result != HAL_OK)
		goto LABEL_EXIT;

	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	result = SdioHostChkXferComplete(psha);
	if (result != HAL_OK) {
		if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) & ERR_INT_STAT_ADMA) {
			HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = ERR_INT_STAT_ADMA;
		}
		if (result ==16)
			return HAL_ERR_UNKNOWN;

		if (!HalSdioHostStopTransferRtl8195a(psha))
			DBG_SDIO_ERR("Stop transmission error!\n");

		result = HAL_ERR_UNKNOWN;
		goto LABEL_EXIT;
	}
	psha->SdSpecVer = StatusData[0];
	psha->SdSpecVer &= 0x0f;  // SCR.59:56
	if (psha->SdSpecVer != 2) {
		DBG_SDIO_ERR("Invalid Spec version %d\n", psha->SdSpecVer);
		psha->SdSpecVer =2;
	}
	// 0: v1.0-1.01
	// 1: v1.1
	// 2: v2.00
	if (psha->SdSpecVer) {
		result = SdioHostSwitchFunction(psha, 0, 15, (int)StatusData,
						NULL);
		if (result !=HAL_OK)
			goto LABEL_EXIT;
		// поддерживается ли функция переключения скорости ?
		if (StatusData[13] & 2) {
			// тест переключения скорости
			result = SdioHostSwitchFunction(psha, 0, 1, (int)StatusData,
								NULL);
			if (result !=HAL_OK)
				goto LABEL_EXIT;
			// тест переключения успешен ?
			if ((StatusData[16] & 0xF) != 1) {
				result= HAL_ERR_UNKNOWN;
				DBG_SDIO_ERR("\"High-Speed\" can't be switched!\n");
					goto LABEL_EXIT;
			}
			// переключаем
			result = SdioHostSwitchFunction(psha, 1, 1, (int) StatusData,
								NULL);
			if (result !=HAL_OK)
				goto LABEL_EXIT;

			// переключилось ?
			if ((StatusData[16] & 0xF) != 1) {
				result= HAL_ERR_UNKNOWN;
				DBG_SDIO_ERR("Card changes to High-Speed fail!\n");
				goto LABEL_EXIT;
			}
			// задержка перед переключением не менее 8*SDclk
			HalDelayUs(20);
			// включаем максимальную частоту
			result = SdioHostSdClkCtrl(psha, 1, BASE_CLK);
			if (result != HAL_OK) {
				DBG_SDIO_ERR("Host changes to High-Speed fail!\n");
				goto LABEL_EXIT;
			}

#if 0
			// если в хосте включить бит High Speed mode - не работает совсем
			// без High Speed mode работает стабильно на 20МГц и нестабильно на 41
			HAL_SDIOH_REG16(REG_SDIO_HOST_CLK_CTRL) &= ~CLK_CTRL_SD_CLK_EN;
			HAL_SDIOH_REG8(REG_SDIO_HOST_HOST_CTRL)|= 1<<2;
			HalDelayUs(5);
			HAL_SDIOH_REG16(REG_SDIO_HOST_CLK_CTRL) |= CLK_CTRL_SD_CLK_EN;
#endif
			HalDelayUs(5);
		}
		else {
			DBG_SDIO_INFO("This card doesn't support \"High-Speed Function\" and can't change to high-speed...\n");
			goto LABEL_EXIT;
		}
	}
	else {
		DBG_SDIO_INFO("This card doesn't support CMD6 and can't change to high-speed...\n");
		goto LABEL_EXIT;
	}


	LABEL_EXIT:
	//if (StatusData)
	//	free(StatusData);

	return result;
}
#endif

#if 1
HAL_Status HalSdioHostReadBlocksDmaRtl8195a(IN VOID *Data, IN u64 ReadAddr,
		IN u32 BlockCnt) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	SDIO_HOST_CMD Cmd;
	HAL_Status result;
	ADMA2_DESC_FMT *admadesc_ptr;
	uint64_t sd_read_addr = ReadAddr;


	if (BlockCnt >= 0x10000 || (psha == NULL))
		return HAL_ERR_PARA;

	admadesc_ptr = psha->AdmaDescTbl;
	// проверка валидности адреса и дескриптора ADMA2
	result = CheckSramAddress((void*)admadesc_ptr) |
			CheckSramAddress((void*)admadesc_ptr->Addr1);
	if (result)
		return result;

	// адрес в байтах или блоках ?
	if (psha->IsSdhc)
		sd_read_addr =ReadAddr >> 9;  // sdhc: адрес в блоках


	HAL_SDIOH_REG32(REG_SDIO_HOST_ADMA_SYS_ADDR) = (uint32_t)admadesc_ptr;
	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_SIZE) = DATA_BLK_LEN;
	result = SdioHostChkCmdInhibitCMD();
	if (result) {
		DBG_SDIO_ERR("SDIO Err]SdioHostChkCmdInhibitCMD fail\n");
		return result;
	}
	result = SdioHostChkDataLineActive();
	if (result) {
		DBG_SDIO_ERR("SDIO Err]SdioHostChkDataLineActive fail\n");
		return result;
	}

	Cmd.CmdFmt.RespType =RSP_LEN_48;
	Cmd.CmdFmt.CmdCrcChkEn =1;
	Cmd.CmdFmt.CmdIdxChkEn =1;
	Cmd.CmdFmt.DataPresent =1;
	Cmd.CmdFmt.CmdType = NORMAL;

	psha->CmdCompleteFlg = 0;
	psha->XferCompleteFlg = 0;
	psha->ErrIntFlg =0;
	Cmd.CmdFmt.Rsvd1 =0;
	Cmd.CmdFmt.Rsvd0 =0;
	Cmd.Arg = sd_read_addr & 0xffffffff;

	if (BlockCnt <= 1) {
		// single block
		Cmd.CmdFmt.CmdIdx = CMD_READ_SINGLE_BLOCK;
		HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) = XFER_MODE_DATA_XFER_DIR
				| XFER_MODE_DMA_EN;   // 17;
	}
	else {
		// multiple block
		Cmd.CmdFmt.CmdIdx = CMD_READ_MULTIPLE_BLOCK;
		HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_CNT) = BlockCnt;
		HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) = XFER_MODE_MULT_SINGLE_BLK
				| XFER_MODE_DATA_XFER_DIR
				| XFER_MODE_AUTO_CMD12_EN
				| XFER_MODE_BLK_CNT_EN
				| XFER_MODE_DMA_EN; 			//0x37 =55
	}

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result) {
		DBG_SDIO_ERR("SDIO Err]SdioHostChkCmdComplete timeout\n");
		return result;
	}

	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	result = SdioHostChkXferComplete(psha);
	if (!result)
		return 0;
	// data error
	if (result != 16) {
		if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) & ERR_INT_STAT_ADMA) {
			HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = ERR_INT_STAT_ADMA;
			if (HalSdioHostStopTransferRtl8195a(psha)) {
				DBG_SDIO_ERR("SDIO Err]Stop transmission error!\n");
			}
		}
	}
	return HAL_ERR_UNKNOWN;
}
#endif

#if 0
// так в sdk
//----- (000009CC) --------------------------------------------------------
HAL_Status HalSdioHostReadBlocksDmaRtl8195a_sdk(IN VOID *Data, IN u64 ReadAddr,
		IN u32 BlockCnt) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	SDIO_HOST_CMD Cmd;
	HAL_Status result;
	ADMA2_DESC_FMT *admadesc_ptr;
	uint64_t sd_read_addr = ReadAddr;


	//Cmd.CmdFmt.RespType = RSP_LEN_48_CHK_BUSY;
	//Cmd.CmdFmt.Rsvd0 = 0;
	//Cmd.CmdFmt.CmdCrcChkEn = 1;
	//Cmd.CmdFmt.CmdIdxChkEn = 1;
	//Cmd.CmdFmt.DataPresent = NO_DATA;
	//Cmd.CmdFmt.CmdType = NORMAL;
	//Cmd.CmdFmt.CmdIdx = CMD_SELECT_DESELECT_CARD;
	//Cmd.CmdFmt.Rsvd1 = 0;
	//Cmd.Arg = psha->RCA << 16;  // card is addressed

	if (!psha)
		return HAL_ERR_PARA;
	if (BlockCnt >= 0x10000)
		return HAL_ERR_PARA;
	// проверка валидности адреса и дескриптора ADMA2
	admadesc_ptr = psha->AdmaDescTbl;
	// проверим выравнивание адресов дескриптора на 32 бита
	if ((uint32_t)admadesc_ptr <<30 || admadesc_ptr->Addr1 << 30)
		return HAL_ERR_PARA;
	// адрес в байтах или блоках ?
	if (psha->IsSdhc)
		sd_read_addr =ReadAddr >> 9;  // sdhc: адрес в блоках

	while (1) {  // loop 1
		while(1) {  // loop  2
			// CMD17 READ_SINGLE_BLOCK
			HAL_SDIOH_REG32(REG_SDIO_HOST_ADMA_SYS_ADDR) =admadesc_ptr;
			HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_SIZE) = DATA_BLK_LEN;
			if (BlockCnt != 1)
				break;
			HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) = XFER_MODE_DATA_XFER_DIR
					| XFER_MODE_DMA_EN;   // 17;

			LABEL_18:
			result = SdioHostChkCmdInhibitCMD();
			if (result) {
				DBG_SDIO_ERR("SDIO Err]SdioHostChkCmdInhibitCMD fail\n");
				return result;
			}
			result = SdioHostChkDataLineActive();
			if (result) {
				DBG_SDIO_ERR("SDIO Err]SdioHostChkDataLineActive fail\n");
				return result;
			}

			//Cmd.CmdFmt =
			//					(SDIO_HOST_CMD_FMT) ((*(u8 *) &Cmd.CmdFmt & 0xF4 | 0x3A)
			//							& 0x3F);
			Cmd.CmdFmt.RespType =RSP_LEN_48;
			Cmd.CmdFmt.CmdCrcChkEn =1;
			Cmd.CmdFmt.CmdIdxChkEn =1;
			Cmd.CmdFmt.DataPresent =1;
			Cmd.CmdFmt.CmdType = NORMAL;


			//			v12 = *((u8 *) &Cmd.CmdFmt + 1);
			//	 *(u8 *) (v5 + 128) = 0;
			//	 *(u8 *) (v5 + 129) = 0;
			psha->CmdCompleteFlg = 0;
			psha->XferCompleteFlg = 0;
			psha->ErrIntFlg =0;
			//	 *((u8 *) &Cmd.CmdFmt + 1) = v12 & 0xC0 | 0x11;
			Cmd.CmdFmt.CmdIdx = CMD_READ_SINGLE_BLOCK;
			Cmd.CmdFmt.Rsvd1 =0;
			Cmd.CmdFmt.Rsvd0 =0;
			//			Cmd.Arg = v4;
			Cmd.Arg = sd_read_addr & 0xffffffff;

			SdioHostSendCmd(&Cmd);
			result = SdioHostChkCmdComplete(psha, 500);//, v13);
			if (result) {
				DBG_SDIO_ERR("SDIO Err]SdioHostChkCmdComplete timeout\n");
				goto LABEL_21;
			}

			SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
			result = SdioHostChkXferComplete(psha);
			if (!result)
				return 0;
			if (result != 16) {
				if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) & ERR_INT_STAT_ADMA) {
					HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = ERR_INT_STAT_ADMA;
					if (HalSdioHostStopTransferRtl8195a(psha)) {
						DBG_SDIO_ERR("SDIO Err]Stop transmission error!\n");
					}
				}
				return HAL_ERR_UNKNOWN;
			}
		} // loop 2 end

		// CMD18 READ_MULTIPLE_ BLOCK

		HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_CNT) = BlockCnt;
		HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) = XFER_MODE_MULT_SINGLE_BLK
				| XFER_MODE_DATA_XFER_DIR
				| XFER_MODE_AUTO_CMD12_EN
				| XFER_MODE_BLK_CNT_EN
				| XFER_MODE_DMA_EN; 			//0x37 =55
		if (BlockCnt <= 1)
			goto LABEL_18;
		result = SdioHostChkCmdInhibitCMD();
		if (result)
			return result;
		result = SdioHostChkDataLineActive();
		if (result)
			return result;
		//Cmd.CmdFmt = (SDIO_HOST_CMD_FMT) ((*(u8 *) &Cmd.CmdFmt & 0xF4 | 0x3A)
		//				& 0x3F);
		Cmd.CmdFmt.RespType =RSP_LEN_48;
		Cmd.CmdFmt.CmdCrcChkEn =1;
		Cmd.CmdFmt.CmdIdxChkEn =1;
		Cmd.CmdFmt.DataPresent =1;
		Cmd.CmdFmt.CmdType = NORMAL;

		//	 *(u8 *) (v5 + 128) = 0;
		//	 *(u8 *) (v5 + 129) = 0;
		//	 *(u8 *) (v5 + 130) = 0;
		psha->CmdCompleteFlg = 0;
		psha->XferCompleteFlg = 0;
		psha->ErrIntFlg =0;

		//		v7 = *((u8 *) &Cmd.CmdFmt + 1);
		//	 *((u8 *) &Cmd.CmdFmt + 1) = v7 & 0xC0 | 0x12;
		Cmd.CmdFmt.CmdIdx = CMD_READ_MULTIPLE_BLOCK;
		Cmd.CmdFmt.Rsvd1 =0;
		Cmd.CmdFmt.Rsvd0 =0;
		//		Cmd.Arg = v4;
		Cmd.Arg = sd_read_addr & 0xffffffff;
		SdioHostSendCmd(&Cmd);

		result = SdioHostChkCmdComplete(psha, 50);
		if (!result) {
			SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
			result = SdioHostChkXferComplete(psha);
			if (!result)
				break;
			continue;
		}

		LABEL_21:
		if (result != 16)
			return result;
	}  // loop 1
	if (!(HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) & ERR_INT_STAT_DATA_CRC))
		return 0;
	HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = ERR_INT_STAT_DATA_CRC;
	DBG_SDIO_ERR("Data CRC error!\n");
	return HAL_ERR_UNKNOWN;
}
#endif



#if 1
//-------------------------------------------------------------------------
// проверно запись работает, обработку ошибок нет возможности проверить
HAL_Status HalSdioHostWriteBlocksDmaRtl8195a(IN VOID *Data, IN uint64 WriteAddr,
		IN uint32_t BlockCnt) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;
	SDIO_HOST_CMD Cmd;
	PADMA2_DESC_FMT admadesc_ptr;
	uint64_t sd_write_addr = WriteAddr;
	uint32_t t1;

	if (!psha)
		return HAL_ERR_PARA;
	if ((BlockCnt >= 0x10000) || (BlockCnt == 0))
		return HAL_ERR_PARA;
	// проверка валидности адреса и дескриптора ADMA2
	admadesc_ptr = psha->AdmaDescTbl;
	// проверим адрес дескриптора ADMA2
	if (CheckSramAddress((void *)admadesc_ptr) || CheckSramAddress((void *)admadesc_ptr->Addr1))
		return HAL_ERR_PARA;
	// адрес в байтах или блоках ?
	if (psha->IsSdhc)
		sd_write_addr >>= 9;  // sdhc: адрес в блоках

	// ждем готовность карты
	t1 = HalTimerOp.HalTimerReadCount(1);
	while (!(HAL_SDIOH_REG32(REG_SDIO_HOST_PRESENT_STATE) & PRES_STATE_DAT0_SIGNAL_LEVEL)) {
		result = SdioHostIsTimeout(t1, SD_CHK_TIMEOUT*5);
		if (result == HAL_TIMEOUT) {
			DBG_SDIO_ERR("card busy TIMEOUT\n");
			return result;
		}
	}

	HAL_SDIOH_REG32(REG_SDIO_HOST_ADMA_SYS_ADDR)= (uint32_t)admadesc_ptr;
	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_SIZE) = DATA_BLK_LEN; // 512
	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_CNT) = BlockCnt;


	result = SdioHostChkCmdInhibitCMD();
	if (result != HAL_OK)
		return result;
	result = SdioHostChkDataLineActive();
	if (result != HAL_OK)
		return result;

	psha->CmdCompleteFlg = 0;
	psha->XferCompleteFlg = 0;

	Cmd.CmdFmt.RespType =RSP_LEN_48;
	Cmd.CmdFmt.CmdCrcChkEn =1;
	Cmd.CmdFmt.CmdIdxChkEn =1;
	Cmd.CmdFmt.DataPresent =1;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.Rsvd1 =0;
	Cmd.CmdFmt.Rsvd0 =0;
	Cmd.Arg = sd_write_addr & 0xffffffff;

	if (BlockCnt == 1) {
		Cmd.CmdFmt.CmdIdx = CMD_WRITE_BLOCK;
		HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) =
			XFER_MODE_DMA_EN;
	}
	else {
		Cmd.CmdFmt.CmdIdx = CMD_WRITE_MULTIPLE_BLOCK;
		HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) =
			XFER_MODE_DMA_EN
			| XFER_MODE_BLK_CNT_EN
			| XFER_MODE_AUTO_CMD12_EN
			| XFER_MODE_MULT_SINGLE_BLK;
	}

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result == HAL_OK) {
		SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
		// WP violation ?
		if (psha->Response[0] & (1<<26)) {
			DBG_SDIO_ERR("Write protect violation!\n");
			return HAL_ERR_PARA;
		}
		result = SdioHostChkXferComplete(psha);
		if (result != HAL_OK) {
			if (result != 16) {
				t1= HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS);
				HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = t1;

				if (HalSdioHostStopTransferRtl8195a(psha))
					DBG_SDIO_ERR("Stop transmission error!\n");

				if (t1 & ERR_INT_STAT_DATA_TIMEOUT)
					return HAL_TIMEOUT;
			}
			return HAL_ERR_UNKNOWN;
		}
	}
	return result;
}
#endif








#if 1
//----- SdioHostGetCSD
HAL_Status SdioHostGetCSD(void *Data)
{
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;
	SDIO_HOST_CMD Cmd;

	if (psha) {
		result = SdioHostChkCmdInhibitCMD();
		if (result == HAL_OK) {

			Cmd.CmdFmt.RespType = RSP_LEN_136;
			//Cmd.CmdFmt.Rsvd0 = 0;
			Cmd.CmdFmt.CmdCrcChkEn = 1;
			Cmd.CmdFmt.CmdIdxChkEn = 0;
			Cmd.CmdFmt.DataPresent = NO_DATA;
			Cmd.CmdFmt.CmdType = NORMAL;
			Cmd.CmdFmt.CmdIdx = CMD_SEND_CSD;
			Cmd.CmdFmt.Rsvd1 = 0;
			Cmd.Arg = psha->RCA << 16;

			psha->CmdCompleteFlg = 0;
			psha->ErrIntFlg =0;
			psha->XferType = SDIO_XFER_NOR;

			SdioHostSendCmd(&Cmd);
			result = SdioHostChkCmdComplete(psha, 50);
			if(result == HAL_OK) {
				SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
				psha->Csd[15] = 1;
				uint32 x = psha->Response[3];
				psha->Csd[0] = x >> 16;
				psha->Csd[1] = x >> 8;
				psha->Csd[2] = x;
				x = psha->Response[2];
				psha->Csd[3] = x >> 24;
				psha->Csd[4] = x >> 16;
				psha->Csd[5] = x >> 8;
				psha->Csd[6] = x;
				x = psha->Response[1];
				psha->Csd[7] = x >> 24;
				psha->Csd[8] = x >> 16;
				psha->Csd[9] = x >> 8;
				psha->Csd[10] = x;
				x = psha->Response[0];
				psha->Csd[11] = x >> 24;
				psha->Csd[12] = x >> 16;
				psha->Csd[13] = x >> 8;
				psha->Csd[14] = x;
			}
		}
	} else
		result = HAL_ERR_PARA;
	return result;
}
#endif

#if 1
// проверно - работает
HAL_Status HalSdioHostGetCardStatusRtl8195a(IN VOID *Data) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	SDIO_HOST_CMD Cmd;
	HAL_Status result;
	uint8_t retrycount;

	if (psha) {
		retrycount =0;
		LABEL10:
		result = SdioHostChkCmdInhibitCMD();
		if (result)
			return result;
		// Send Status command
		Cmd.Arg = psha->RCA << 16;
		Cmd.CmdFmt.CmdIdx = CMD_SEND_STATUS;
		Cmd.CmdFmt.RespType = RSP_LEN_48;
		Cmd.CmdFmt.CmdCrcChkEn = 1;
		Cmd.CmdFmt.CmdIdxChkEn = 1;
		Cmd.CmdFmt.DataPresent = 0;
		Cmd.CmdFmt.CmdType = 0;
		psha->XferType = SDIO_XFER_NOR;

		psha->CmdCompleteFlg = 0;
		psha->ErrIntFlg =0;
		SdioHostSendCmd(&Cmd);
		result = SdioHostChkCmdComplete(psha, 50);  // timeout = 50*31us

		if ((result == 16) || (psha->errType == SDIO_ERR_CMD_TIMEOUT)) {
			//rtl_printf("R%d\n", retrycount);
			if (++retrycount < 3)
				goto LABEL10;
		}
		if (result)
			return result;

		SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
		// check index
		if ( (psha->Response[1] & 0x3f) == CMD_SEND_STATUS) {
			psha->CardStatus = psha->Response[0];  // card status
			psha->CardCurState = (psha->Response[0] >> 9) & 0xF;  // CURRENT_STATE only
			result = HAL_OK;
		}
		else
			result = HAL_ERR_UNKNOWN;
	}
	else
		result = HAL_ERR_PARA;
	return result;
}
#endif


#if 1
// проверно - работает
// перед вызовом должна быть выделена память для дескрипторов ADMA2
// psha->AdmaDescTbl
HAL_Status HalSdioHostGetSdStatusRtl8195a(IN VOID *Data) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	SDIO_HOST_CMD Cmd;
	HAL_Status result;
	PADMA2_DESC_FMT pAdmaDescTbl;

	if (!psha)
		return HAL_ERR_PARA;
	pAdmaDescTbl = psha->AdmaDescTbl;

	if ((pAdmaDescTbl == NULL) || (uint32_t)pAdmaDescTbl & 3)
		return HAL_ERR_PARA;

	pAdmaDescTbl->Len1 = 64;
	pAdmaDescTbl->Attrib1.End=1;
	pAdmaDescTbl->Attrib1.Act1=0;
	pAdmaDescTbl->Attrib1.Act2=1;
	pAdmaDescTbl->Addr1 = (uint32_t) &psha->SdStatus[0];
	pAdmaDescTbl->Attrib1.Valid =1;

	//memset(&psha->SdStatus[0], 0x55, SD_STATUS_LEN);

	result = SdioHostChkCmdInhibitCMD();
	if (result)
		return result;

	Cmd.CmdFmt.RespType = RSP_LEN_48; //2
	Cmd.CmdFmt.Rsvd0 =0;
	Cmd.CmdFmt.CmdCrcChkEn =1;
	Cmd.CmdFmt.CmdIdxChkEn =1;
	Cmd.CmdFmt.DataPresent =0;
	Cmd.CmdFmt.CmdType = NORMAL;

	Cmd.CmdFmt.CmdIdx = CMD_APP_CMD;  // 0x37 = CMD55
	Cmd.CmdFmt.Rsvd1 = 0;

	psha->CmdCompleteFlg =0;

	Cmd.Arg = psha->RCA <<16;

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result)
		return result;
	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	// проверим команду
	if ((psha->Response[1] & 255) != CMD_APP_CMD) {
		DBG_SDIO_ERR("Command index error!\n");
		return HAL_ERR_UNKNOWN;
	}
	if (!(psha->Response[0] & 0x20)) {
		DBG_SDIO_ERR("ACMD isn't expected!\n");
		return HAL_ERR_UNKNOWN;
	}

	result = SdioHostChkCmdInhibitCMD();
	if (result)
		return result;
	result = SdioHostChkDataLineActive();
	if (result)
		return result;

	HAL_SDIOH_REG32(REG_SDIO_HOST_ADMA_SYS_ADDR) = (uint32_t)pAdmaDescTbl;
	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_SIZE) = 64;

	HAL_SDIOH_REG16(REG_SDIO_HOST_XFER_MODE) =
			XFER_MODE_DATA_XFER_DIR
			| XFER_MODE_AUTO_CMD12_EN
			| XFER_MODE_BLK_CNT_EN
			| XFER_MODE_DMA_EN  ;			//17;

	HAL_SDIOH_REG16(REG_SDIO_HOST_BLK_CNT) =1;

	Cmd.CmdFmt.RespType = RSP_LEN_48; //2
	Cmd.CmdFmt.Rsvd0 =0;
	Cmd.CmdFmt.CmdCrcChkEn =1;
	Cmd.CmdFmt.CmdIdxChkEn =1;
	Cmd.CmdFmt.DataPresent =1;
	Cmd.CmdFmt.CmdType = NORMAL;

	psha->CmdCompleteFlg =0;
	psha->XferCompleteFlg =0;

	Cmd.CmdFmt.CmdIdx = CMD_SEND_STATUS;  // ACMD13
	Cmd.CmdFmt.Rsvd1 =0;

	Cmd.Arg = 0;
	//Cmd.Arg = psha->RCA <<16;  // без разницы

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result)
		return result;
	if (!(psha->Response[0] & (1<<5)) && ((psha->Response[1] & 255)!=CMD_SEND_STATUS)) {
			return HAL_ERR_UNKNOWN;
	}
	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	result = SdioHostChkXferComplete(psha);
	if (result) {
		if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) & ERR_INT_STAT_ADMA) {
			HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = ERR_INT_STAT_ADMA;
		}
		if (result ==16)
			return HAL_ERR_UNKNOWN;

		if (!HalSdioHostStopTransferRtl8195a(psha))
			DBG_SDIO_ERR("Stop transmission error!\n");

		return HAL_ERR_UNKNOWN;
	}
	return result;
}
#endif





#if 1
// проверно - работает
HAL_Status HalSdioHostIrqInitRtl8195a(IN VOID *Data) // PIRQ_HANDLE Data
{
	HAL_Status result;
	PIRQ_HANDLE pih = (PIRQ_HANDLE)Data;
	if (pih) {
		pih->Data = (uint32_t)Data;
		pih->IrqNum = SDIO_HOST_IRQ;
		pih->IrqFun = SdioHostIsrHandle;
		pih->Priority = 6;
		VectorIrqRegisterRtl8195A((PIRQ_HANDLE) pih);
		VectorIrqEnRtl8195A((PIRQ_HANDLE) pih);
		result = HAL_OK;
	} else
		result = HAL_ERR_PARA;
	return result;
}


//----- HalSdioHostInitHostRtl8195a
// проверно - работает
HAL_Status HalSdioHostInitHostRtl8195a(IN VOID *Data) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	uint32_t reg;
	int32_t x;
	// переменная для фиктивного чтения.
	// после части опeраций записи выпоняют чтение, результат чтения игнорируют.
	// зачем так сделано неизвестно, возможно попытка обойти буферизацию
	// в шинном коммутаторе

	HAL_WRITE32(PERI_ON_BASE, REG_PESOC_HCI_CLK_CTRL0,
			HAL_READ32(PERI_ON_BASE, REG_PESOC_HCI_CLK_CTRL0) & (~BIT_SOC_ACTCK_SDIO_DEV_EN));
	//reg = HAL_READ32(PERI_ON_BASE, 0x50000);  // dummy read

	HAL_WRITE32(PERI_ON_BASE, REG_SOC_HCI_COM_FUNC_EN,
			HAL_READ32(PERI_ON_BASE, REG_SOC_HCI_COM_FUNC_EN) & (~BIT_SOC_HCI_SDIOD_ON_EN));
	//reg = HAL_READ32(PERI_ON_BASE, 0x50000);  // dummy read

	HAL_WRITE32(PERI_ON_BASE, REG_SOC_HCI_COM_FUNC_EN,
			HAL_READ32(PERI_ON_BASE, REG_SOC_HCI_COM_FUNC_EN) & (~BIT_SOC_HCI_SDIOD_OFF_EN));

	HAL_WRITE32(PERI_ON_BASE, REG_HCI_PINMUX_CTRL,
			HAL_READ32(PERI_ON_BASE, REG_HCI_PINMUX_CTRL) & (~BIT_HCI_SDIOD_PIN_EN));

	HAL_WRITE32(PERI_ON_BASE, REG_PESOC_HCI_CLK_CTRL0,
			HAL_READ32(PERI_ON_BASE, REG_PESOC_HCI_CLK_CTRL0) | BIT_SOC_ACTCK_SDIO_HST_EN);
	HAL_WRITE32(PERI_ON_BASE, REG_PESOC_HCI_CLK_CTRL0,
			HAL_READ32(PERI_ON_BASE, REG_PESOC_HCI_CLK_CTRL0) | BIT_SOC_SLPCK_SDIO_HST_EN);

	HalPinCtrlRtl8195A(SDIOH, 0, 1);
	//reg = HAL_READ32(PERI_ON_BASE, 0x50000);  // dummy read

	HAL_WRITE32(PERI_ON_BASE, REG_SOC_HCI_COM_FUNC_EN,
			HAL_READ32(PERI_ON_BASE, REG_SOC_HCI_COM_FUNC_EN) | BIT_SOC_HCI_SDIOH_EN);
	HAL_SDIOH_REG8(REG_SDIO_HOST_SW_RESET) |= SW_RESET_FOR_ALL; //4005802F |= 1;

	x = 1000;
	while (HAL_SDIOH_REG8(REG_SDIO_HOST_SW_RESET) & SW_RESET_FOR_ALL) {
		if (x-- == 0) {
			DBG_SDIO_ERR("SD host initialization FAIL!\n");
			return HAL_TIMEOUT;
		}
	}
	HalSdioHostIrqInitRtl8195a(&psha->IrqHandle);

	HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_STATUS_EN)
	= NOR_INT_STAT_EN_CMD_COMP
	| NOR_INT_STAT_EN_XFER_COMP
	| NOR_INT_STAT_EN_CARD_REMOVAL
	| NOR_INT_SIG_EN_CARD_INSERT;  // 0xC3;
	HAL_SDIOH_REG16(REG_SDIO_HOST_NORMAL_INT_SIG_EN)
	= NOR_INT_SIG_EN_CMD_COMP
	| NOR_INT_SIG_EN_XFER_COMP
	| NOR_INT_SIG_EN_CARD_REMOVAL
	| NOR_INT_SIG_EN_CARD_INSERT;	// 0xc3;

	reg = ERR_INT_STAT_EN_CMD_TIMEOUT
			| ERR_INT_STAT_EN_CMD_CRC
			| ERR_INT_STAT_EN_CMD_END_BIT
			| ERR_INT_STAT_EN_CMD_IDX
			| ERR_INT_STAT_EN_DATA_TIMEOUT
			| ERR_INT_STAT_EN_DATA_CRC
			| ERR_INT_STAT_EN_DATA_END_BIT
			| ERR_INT_STAT_EN_AUTO_CMD; 	// 0x17F;
	HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS_EN) = reg;
	HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_SIG_EN) = reg;

	HAL_SDIOH_REG16(REG_SDIO_HOST_CLK_CTRL) |= CLK_CTRL_INTERAL_CLK_EN;

	x = 1000;
	while (!(HAL_SDIOH_REG16(REG_SDIO_HOST_CLK_CTRL)
			& CLK_CTRL_INTERAL_CLK_STABLE)) {
		if (x-- == 0) {
			DBG_SDIO_ERR("SD host initialization FAIL!\n");
			return HAL_TIMEOUT;
		}
	}
	HAL_SDIOH_REG32(0x1000) |= 0x400; // 40059000 |= 0x400;

	if (HAL_SDIOH_REG32(REG_SDIO_HOST_CAPABILITIES) & CAPA_ADMA2_SUPPORT)
		HAL_SDIOH_REG16(REG_SDIO_HOST_HOST_CTRL) = 0x10; // 32-bit Address ADMA2 is selected
	HAL_SDIOH_REG8(REG_SDIO_HOST_TIMEOUT_CTRL) = 0x0E; // TMCLK x 2^27
	return HAL_OK;
}


//----- HalSdioHostDeInitRtl8195a
HAL_Status HalSdioHostDeInitRtl8195a(IN VOID *Data) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;

	if (!psha)
		return HAL_ERR_PARA;

	HAL_SDIOH_REG8(REG_SDIO_HOST_PWR_CTRL) &= ~PWR_CTRL_SD_BUS_PWR;
	result = SdioHostSdClkCtrl(psha, 0, BASE_CLK);
	if (result == HAL_OK) {
		VectorIrqDisRtl8195A(&psha->IrqHandle);
		VectorIrqUnRegisterRtl8195A(&psha->IrqHandle);
		HAL_SDIOH_REG16(REG_SDIO_HOST_CLK_CTRL) &= ~CLK_CTRL_INTERAL_CLK_EN;
		HAL_SDIOH_REG32(0x1000)  &= 0xFFFFFBFF; // v40059000
		HAL_WRITE32(PERI_ON_BASE, REG_SOC_HCI_COM_FUNC_EN,
				HAL_READ32(PERI_ON_BASE, REG_SOC_HCI_COM_FUNC_EN) & (~BIT_SOC_HCI_SDIOH_EN));
		HalPinCtrlRtl8195A(SDIOH, 0, 0);
		ACTCK_SDIOH_CCTRL(OFF);
		SLPCK_SDIOH_CCTRL(OFF);
	}
	return result;
}



//----- HalSdioHostStopTransferRtl8195a
HAL_Status HalSdioHostStopTransferRtl8195a(IN VOID *Data) {
//	HAL_Status result;
	SDIO_HOST_CMD Cmd;
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	uint32_t x;
	uint8_t retrycount;

	if (psha) {
		// asynchronous abort sequence
		// CMD12 -> CMD13 -> check tran -> repeat...

		//result = SdioHostChkCmdInhibitCMD(); //(uint32_t) Data);
		//if (result == HAL_OK) {
		//result = SdioHostChkCmdInhibitDAT();
		//if (result == HAL_OK) {

		//psha->CmdCompleteFlg = 0;
		//psha->XferType = SDIO_XFER_NOR;
		//psha->XferCompleteFlg = 0;
		retrycount = 0;
		do {
			SdioHostChkCmdInhibitCMD();
			Cmd.CmdFmt.RespType = RSP_LEN_48_CHK_BUSY;
			Cmd.CmdFmt.Rsvd0 = 0;
			Cmd.CmdFmt.CmdCrcChkEn = 1;
			Cmd.CmdFmt.CmdIdxChkEn = 0;
			Cmd.CmdFmt.DataPresent = NO_DATA;
			Cmd.CmdFmt.CmdType = ABORT;
			Cmd.CmdFmt.CmdIdx = CMD_STOP_TRANSMISSION;
			Cmd.CmdFmt.Rsvd1 = 0;
			Cmd.Arg = 0;

			HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = 0xffff;
			SdioHostSendCmd(&Cmd);
			x=HalTimerOp.HalTimerReadCount(1);
			while (SdioHostIsTimeout(x,50) !=HAL_TIMEOUT) ;

			if (HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS)) {
				retrycount++;
			}
			else
				retrycount = 101;
		}
		while (retrycount < 100);
		DBG_SDIO_ERR("Stop transfer retry %d\n", retrycount);
		HAL_SDIOH_REG16(REG_SDIO_HOST_ERROR_INT_STATUS) = 0xffff;

		//x = 1000;
		//HAL_SDIOH_REG8(REG_SDIO_HOST_SW_RESET) = SW_RESET_FOR_ALL;
		//while (HAL_SDIOH_REG8(REG_SDIO_HOST_SW_RESET) & SW_RESET_FOR_ALL) {
		//	if (x-- == 0) {
		//		DBG_SDIO_ERR("SD host initialization FAIL!\n");
		//		return HAL_TIMEOUT;
		//	}

		return HAL_OK;
	}
	//}
	//}
	return HAL_ERR_PARA;
}
#endif


#if 1
//----- SdioHostCardSelection
// проверено - работает
HAL_Status SdioHostCardSelection(void *Data, int Select) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;
	SDIO_HOST_CMD Cmd; // [sp+0h] [bp-18h]@1

	if (psha) {
		result = SdioHostChkCmdInhibitCMD();
		if (result == HAL_OK) {
			psha->CmdCompleteFlg = 0;
			psha->XferType = SDIO_XFER_NOR;
			psha->XferCompleteFlg = 0;
			psha->ErrIntFlg =0;

			if (Select == 1) {
				result = SdioHostChkCmdInhibitDAT();
				if (result == HAL_OK) {

					Cmd.CmdFmt.RespType = RSP_LEN_48_CHK_BUSY;
					Cmd.CmdFmt.Rsvd0 = 0;
					Cmd.CmdFmt.CmdCrcChkEn = 1;
					Cmd.CmdFmt.CmdIdxChkEn = 1;
					Cmd.CmdFmt.DataPresent = NO_DATA;
					Cmd.CmdFmt.CmdType = NORMAL;
					Cmd.CmdFmt.CmdIdx = CMD_SELECT_DESELECT_CARD;
					Cmd.CmdFmt.Rsvd1 = 0;
					Cmd.Arg = psha->RCA << 16;  // card is addressed


					SdioHostSendCmd(&Cmd);
					result = SdioHostChkCmdComplete(psha, 50);
					if(result == HAL_OK) {
						result = SdioHostChkXferComplete(psha);
						if(result != HAL_OK) return result;
						result = SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
						if(result != HAL_OK) return result;
						if((psha->Response[1] & 255) == 7) {
							//
							return result;
						}
						else {
							DBG_SDIO_ERR("Command index error!\n");
							result = HAL_ERR_UNKNOWN;
						}
					}
				}
			} else {
				Cmd.CmdFmt.RespType = NO_RSP;
				Cmd.CmdFmt.Rsvd0 = 0;
				Cmd.CmdFmt.CmdCrcChkEn = 0;
				Cmd.CmdFmt.CmdIdxChkEn = 0;
				Cmd.CmdFmt.DataPresent = NO_DATA;
				Cmd.CmdFmt.CmdType = NORMAL;
				Cmd.CmdFmt.CmdIdx = CMD_SELECT_DESELECT_CARD;
				Cmd.CmdFmt.Rsvd1 = 0;
				Cmd.Arg = 0; // card is not addressed

				SdioHostSendCmd(&Cmd);
				result = SdioHostChkCmdComplete(psha, 50);
			}
		}
	} else result = HAL_ERR_PARA;
	return result;
}
#endif


//-------------------------------------------------------------------------
// проверено - работает
HAL_Status HalSdioHostEraseRtl8195a(IN VOID *Data, IN uint64_t StartAddr,
IN uint64_t EndAddr) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;
	SDIO_HOST_CMD Cmd;
	uint64_t start = StartAddr;
	uint64_t end = EndAddr;

	if (!psha)
		return HAL_ERR_PARA;

	if (psha->IsSdhc) {
		start >>=9;
		end >>=9;
	}
	result = SdioHostChkCmdInhibitCMD();//EndAddr);
	if (result)
		return result;

	// CMD32
	psha->CmdCompleteFlg = 0;
	psha->XferType = SDIO_XFER_NOR;

	Cmd.CmdFmt.RespType = RSP_LEN_48;
	Cmd.CmdFmt.Rsvd0 = 0;
	Cmd.CmdFmt.CmdCrcChkEn = 1;
	Cmd.CmdFmt.CmdIdxChkEn = 1;
	Cmd.CmdFmt.DataPresent = NO_DATA;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = CMD_ERASE_WR_BLK_START;
	Cmd.CmdFmt.Rsvd1 = 0;
	Cmd.Arg = start;

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result) {
		return result;
	}
	result = SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	if ((psha->Response[1] & 255) != CMD_ERASE_WR_BLK_START) {
		DBG_SDIO_ERR("Command index error!\n");
		return HAL_ERR_UNKNOWN;
	}

	result = SdioHostChkCmdInhibitCMD();
	if (result)
		return result;

	// CMD33
	psha->CmdCompleteFlg = 0;

	Cmd.CmdFmt.RespType = RSP_LEN_48;
	Cmd.CmdFmt.Rsvd0 = 0;
	Cmd.CmdFmt.CmdCrcChkEn = 1;
	Cmd.CmdFmt.CmdIdxChkEn = 1;
	Cmd.CmdFmt.DataPresent = NO_DATA;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = CMD_ERASE_WR_BLK_END;
	Cmd.CmdFmt.Rsvd1 = 0;
	Cmd.Arg = end;

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result) {
		return result;
	}
	result = SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	if ((psha->Response[1] & 255) != CMD_ERASE_WR_BLK_END) {
		DBG_SDIO_ERR("Command index error!\n");
		return HAL_ERR_UNKNOWN;
	}

	result = SdioHostChkCmdInhibitCMD();
	if (result)
		return result;
	result = SdioHostChkCmdInhibitDAT();
	if (result)
		return result;

	// CMD38
	psha->CmdCompleteFlg = 0;
	psha->XferCompleteFlg =0;
	Cmd.CmdFmt.RespType = RSP_LEN_48_CHK_BUSY;
	Cmd.CmdFmt.CmdCrcChkEn = 1;
	Cmd.CmdFmt.CmdIdxChkEn = 1;
	Cmd.CmdFmt.DataPresent = NO_DATA;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = CMD_ERASE;
	Cmd.CmdFmt.Rsvd1 = 0;
	Cmd.Arg = 0;
	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (!result) {
		result = SdioHostChkXferComplete(psha);
	}

	return result;
}



//-------------------------------------------------------------------------
// проверено не полностью - нет карты с защитой записи в CSD
#if 1
HAL_Status HalSdioHostGetWriteProtectRtl8195a(IN VOID *Data) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;
//	SDIO_HOST_CMD Cmd; // [sp+0h] [bp-18h]@1

	if (!psha)
		return HAL_ERR_PARA;

	result = HalSdioHostGetCardStatusRtl8195a(psha);
	if (result) {
		DBG_SDIO_ERR("Get card status fail!\n");
		return result;
	}

	if (psha->CardCurState ==3) {
		LABEL_10:
		result = SdioHostGetCSD(psha);
		if (!result) {
			//   Permanent write protect - CSD_bit[12:12]
			//psha->IsWriteProtect = ((unsigned int) *((uint8_t *)psha + 126) >> 4) & 1;
			psha->IsWriteProtect = (unsigned int) (psha->Csd[14] >> 4) & 1;

			result = SdioHostCardSelection(psha, 1);
			// now card state is 4 (tran)
			return result;
		}
		DBG_SDIO_ERR("Get CSD fail!\n");
		return result;
	}
	if (psha->CardCurState == 4 || psha->CardCurState == 5) {
		result = SdioHostCardSelection(psha, 0); // card is not addressed
		if (result) {
			DBG_SDIO_ERR("Card selection fail!\n");
			return result;
		}
		// now card state is 3 (stby)
		goto LABEL_10;
	}
	DBG_SDIO_ERR("Wrong card state!\n");
	return HAL_ERR_UNKNOWN;
}
#endif

#if 1
HAL_Status HalSdioHostSetWriteProtectRtl8195a(IN VOID *Data, IN u8 Setting) {
	DBG_SDIO_ERR("HalSdioHostSetWriteProtectRtl8195a is not implemented!\n");
	return HAL_ERR_UNKNOWN;
}
#endif



#if 1
static HAL_Status HalSdioHostACMD41(PHAL_SDIO_HOST_ADAPTER psha, uint32_t Arg) {
	HAL_Status result;
	SDIO_HOST_CMD Cmd;

	result = SdioHostChkCmdInhibitCMD();//v9);
	if (result)
		return result;
	// CMD55
	psha->CmdCompleteFlg = 0;

	Cmd.CmdFmt.RespType = RSP_LEN_48;
	Cmd.CmdFmt.Rsvd0 = 0;
	Cmd.CmdFmt.CmdCrcChkEn = 1;
	Cmd.CmdFmt.CmdIdxChkEn = 1;
	Cmd.CmdFmt.DataPresent = NO_DATA;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = CMD_APP_CMD;
	Cmd.CmdFmt.Rsvd1 = 0;
	Cmd.Arg = 0;
	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 500);
	if (result) {
		DBG_SDIO_ERR("Get OCR fail!\n");
		return result;
	}
	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);

	result = HAL_ERR_UNKNOWN;
	// проверим команду
	if ((psha->Response[1] & 255) != CMD_APP_CMD) {
		DBG_SDIO_ERR("Command index error!\n");
		return result;
	}
	if (!(psha->Response[0] & 0x20)) {
		DBG_SDIO_ERR("ACMD isn't expected!\n");
		return result;
	}

	result = SdioHostChkCmdInhibitCMD();
	if (result) {
		DBG_SDIO_ERR("Get OCR fail!\n");
		return result;
	}

	Cmd.CmdFmt.RespType = RSP_LEN_48; //2
	Cmd.CmdFmt.Rsvd0 =0;
	Cmd.CmdFmt.CmdCrcChkEn =0;
	Cmd.CmdFmt.CmdIdxChkEn =0;
	Cmd.CmdFmt.DataPresent =0;
	Cmd.CmdFmt.CmdType = NORMAL;

	psha->CmdCompleteFlg =0;

	Cmd.CmdFmt.CmdIdx = CMD_SD_SEND_OP_COND;  // ACMD41
	Cmd.CmdFmt.Rsvd1 =0;
	Cmd.Arg = Arg;

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 500);
	if (result) {
		DBG_SDIO_ERR("Get OCR fail!\n");
		return result;
	}
	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	return result;
}

//-------------------------------------------------------------------------
// проверено - работает
HAL_Status HalSdioHostInitCardRtl8195a(IN VOID *Data) {
	PHAL_SDIO_HOST_ADAPTER psha = (PHAL_SDIO_HOST_ADAPTER)Data;
	HAL_Status result;
	SDIO_HOST_CMD Cmd;
	uint32_t retrycount;

	if (!psha) {
		result = HAL_ERR_PARA;
		goto LABEL_115;
	};

	result = SdioHostChkCmdInhibitCMD();
	psha->XferType = SDIO_XFER_NOR;
	// карта может находится в непределенном состоянии поэтому
	// подаем CMD0 всегда
	// в sdk CMD0 подается по условию
	//if (result) {
		psha->CmdCompleteFlg = 0;
		psha->XferType = SDIO_XFER_NOR;
		psha->XferCompleteFlg = 0;

		// Reset card
		Cmd.CmdFmt.RespType = NO_RSP;
		Cmd.CmdFmt.Rsvd0 = 0;
		Cmd.CmdFmt.CmdCrcChkEn = 0;
		Cmd.CmdFmt.CmdIdxChkEn = 0;
		Cmd.CmdFmt.DataPresent = NO_DATA;
		Cmd.CmdFmt.CmdType = NORMAL;
		Cmd.CmdFmt.CmdIdx = CMD_GO_IDLE_STATE;
		Cmd.CmdFmt.Rsvd1 = 0;
		Cmd.Arg = 0;

		SdioHostSendCmd(&Cmd);
		result = SdioHostChkCmdComplete(psha, 500);
		if (result) {
			DBG_SDIO_ERR("Reset sd card fail!\n");
			goto LABEL_115;
		}
	//}

	result = SdioHostChkCmdInhibitCMD();
	if (!result) {
		psha->CmdCompleteFlg = 0;
		// CMD8
		Cmd.CmdFmt.RespType = RSP_LEN_48;
		Cmd.CmdFmt.Rsvd0 = 0;
		Cmd.CmdFmt.CmdCrcChkEn = 1;
		Cmd.CmdFmt.CmdIdxChkEn = 1;
		Cmd.CmdFmt.DataPresent = NO_DATA;
		Cmd.CmdFmt.CmdType = NORMAL;
		Cmd.CmdFmt.CmdIdx = CMD_SEND_IF_COND;
		Cmd.CmdFmt.Rsvd1 = 0;
		Cmd.Arg = 0x1aa;  // Voltage Supplied = 2.7...3.6  Check pattern = 0xaa

		SdioHostSendCmd(&Cmd);
		result = SdioHostChkCmdComplete(psha, 500);

		if (result) {
			DBG_SDIO_ERR("Voltage check fail!\n");
			goto LABEL_115;
		}
		SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);

		result = HAL_ERR_UNKNOWN;
		if ((psha->Response[1] & 255) != CMD_SEND_IF_COND) {
			DBG_SDIO_ERR("Command index error!\n");
			goto LABEL_115;
		}
		if ((psha->Response[0] & 255) !=0xaa) {
			DBG_SDIO_ERR("Echo-back of check pattern: %X\n");
			goto LABEL_115;
		}
		if (!(psha->Response[0] & 0x100)) {
			DBG_SDIO_ERR("Voltage accepted error!\n");
			goto LABEL_115;
		}
	}
	else {
		DBG_SDIO_ERR("Voltage check fail!\n");
		goto LABEL_115;
	}

	result = HalSdioHostACMD41(psha, 0);
	if (result) {
		DBG_SDIO_ERR("Get OCR fail!\n");
		goto LABEL_115;
	}
	//*((u32 *) v3 + 9) = *((u32 *) v3 + 5) & 0xFFFFFF;
	psha->CardOCR = psha->Response[0] & 0xFFFFFF;

	// ожем окончание инициализации карты
	retrycount = ACMD41_INIT_TIMEOUT / ACMD41_POLL_INTERVAL;  // 1s max
	while (--retrycount) {
		result = HalSdioHostACMD41(psha, 0x403C0000); //1077673984
		if (result) {
				DBG_SDIO_ERR("Get OCR fail!\n");
				goto LABEL_115;
			}
		if (psha->Response[0] & 0x80000000)
			break;
		HalDelayUs(ACMD41_POLL_INTERVAL); // 10ms
	} // while
	if (retrycount ==0) {
		result = HAL_TIMEOUT;
		goto LABEL_115;
	}
	// сейчас карта в состоянии ready

	// проверим Card Capacity Status
	if (psha->Response[0] & 0x40000000) {
		psha->IsSdhc= 1;
		DBG_SDIO_INFO("This is a SDHC card\n");
	}
	else {
		psha->IsSdhc= 0;
		DBG_SDIO_INFO("This is a SDSC card\n");
	}

	HalDelayUs(2000);
	result = SdioHostChkCmdInhibitCMD();
	if (result) {
		goto LABEL_115;
	}
	// CMD2
	psha->CmdCompleteFlg = 0;
	Cmd.CmdFmt.RespType = RSP_LEN_136;
	Cmd.CmdFmt.Rsvd0 = 0;
	Cmd.CmdFmt.CmdCrcChkEn = 0;
	Cmd.CmdFmt.CmdIdxChkEn = 0;
	Cmd.CmdFmt.DataPresent = NO_DATA;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = CMD_ALL_SEND_CID;
	Cmd.CmdFmt.Rsvd1 = 0;
	Cmd.Arg = 0;
	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 500);
	if (result) {
		DBG_SDIO_ERR("Get CID fail!\n");
		goto LABEL_115;
	}
	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);	// Card IDentification (CID) ингорируем
	// сейчас карта в состоянии ident

	result = SdioHostChkCmdInhibitCMD();
	if (result) {
		goto LABEL_115;
	}

	// CMD3
	psha->CmdCompleteFlg = 0;

	Cmd.CmdFmt.RespType = RSP_LEN_48;
	Cmd.CmdFmt.Rsvd0 = 0;
	Cmd.CmdFmt.CmdCrcChkEn = 1;
	Cmd.CmdFmt.CmdIdxChkEn = 1;
	Cmd.CmdFmt.DataPresent = NO_DATA;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = CMD_SEND_RELATIVE_ADDR;
	Cmd.CmdFmt.Rsvd1 = 0;
	Cmd.Arg = 0;
	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 500);
	if (result) {
		DBG_SDIO_ERR("Get RCA fail!\n");
		goto LABEL_115;
	}
	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	if ((psha->Response[1] & 255) != CMD_SEND_RELATIVE_ADDR) {
		DBG_SDIO_ERR("Command index error!\n");
		result = HAL_ERR_UNKNOWN;
		goto LABEL_115;
	}
	// сейчас карта в состоянии stby

	psha->RCA = psha->Response[0] >> 16; // relative card address

	SdioHostSdClkCtrl(psha, 1, BASE_CLK_DIVIDED_BY_2);
	HalDelayUs(1000);

	result = SdioHostGetCSD(psha);
	if (result) {
		DBG_SDIO_ERR("Get CSD fail!\n");
		goto LABEL_115;
	}
	result = SdioHostCardSelection(psha, 1);
	if (result) {
		DBG_SDIO_ERR("Select sd card fail!\n");
		goto LABEL_115;
	}
	result = SdioHostChkCmdInhibitCMD();//0);
	if (result) {
		goto LABEL_115;
	}

	// переключение интерфейса в 4-х битный режим
//#if 1
	// CMD55
	psha->CmdCompleteFlg = 0;

	Cmd.CmdFmt.RespType = RSP_LEN_48;
	Cmd.CmdFmt.Rsvd0 = 0;
	Cmd.CmdFmt.CmdCrcChkEn = 1;
	Cmd.CmdFmt.CmdIdxChkEn = 1;
	Cmd.CmdFmt.DataPresent = NO_DATA;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = CMD_APP_CMD;
	Cmd.CmdFmt.Rsvd1 = 0;
	Cmd.Arg = psha->RCA << 16;
	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result) {
		DBG_SDIO_ERR("Get OCR fail!\n");
		goto LABEL_115;
	}
	SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);

	result = HAL_ERR_UNKNOWN;
	// проверим команду
	if ((psha->Response[1] & 255) != CMD_APP_CMD) {
		DBG_SDIO_ERR("Command index error!\n");
		goto LABEL_115;
	}
	if (!(psha->Response[0] & 0x20)) {
		DBG_SDIO_ERR("ACMD isn't expected!\n");
		goto LABEL_115;
	}

	result = SdioHostChkCmdInhibitCMD();
	if (result) {
		goto LABEL_115;
	}

	//ACMD6
	psha->CmdCompleteFlg = 0;

	Cmd.CmdFmt.RespType = RSP_LEN_48;
	Cmd.CmdFmt.Rsvd0 = 0;
	Cmd.CmdFmt.CmdCrcChkEn = 1;
	Cmd.CmdFmt.CmdIdxChkEn = 1;
	Cmd.CmdFmt.DataPresent = NO_DATA;
	Cmd.CmdFmt.CmdType = NORMAL;
	Cmd.CmdFmt.CmdIdx = 6;
	Cmd.CmdFmt.Rsvd1 = 0;
	Cmd.Arg = 2;

	SdioHostSendCmd(&Cmd);
	result = SdioHostChkCmdComplete(psha, 50);
	if (result) {
		DBG_SDIO_ERR("Set bus width fail!\n");
		goto LABEL_115;
	}
	result = SdioHostGetResponse(psha, Cmd.CmdFmt.RespType);
	if ((psha->Response[1] & 255) != 6) {
		DBG_SDIO_ERR("Command index error!\n");
		result = HAL_ERR_UNKNOWN;
		goto LABEL_115;
	}
	HAL_SDIOH_REG8(REG_SDIO_HOST_HOST_CTRL)|=2;
	// теперь работает в 4-х битном режиме
//#endif

	// проверим состояние карты
	result = HalSdioHostGetCardStatusRtl8195a(psha);
	if (result) {
		DBG_SDIO_ERR("SDIO Err]Get sd card current state fail!\n");
		goto LABEL_115;
	}
	if (psha->CardCurState != 4) {
		DBG_SDIO_ERR("The card isn't in TRANSFER state! (Current state: %d)\n",
				psha->CardCurState);
		result = HAL_ERR_UNKNOWN;
		goto LABEL_115;
	}
	// сейчас карта в состоянии tran


	LABEL_115:
	if (result)
		DBG_SDIO_ERR("SD card initialization FAIL!\n");
	return result;
}
#endif


#endif
