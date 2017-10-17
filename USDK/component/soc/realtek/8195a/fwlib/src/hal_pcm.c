/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 */


#include "platform_autoconf.h"
#include "hal_pcm.h"

#ifdef CONFIG_PCM_EN

VOID HalPcmOpInit(
    IN  VOID *Data
)
{
    PHAL_PCM_OP pHalPcmOp = (PHAL_PCM_OP) Data;

    pHalPcmOp->HalPcmOnOff = HalPcmOnOffRtl8195a;
    pHalPcmOp->HalPcmInit = HalPcmInitRtl8195a;
    pHalPcmOp->HalPcmSetting = HalPcmSettingRtl8195a;
    pHalPcmOp->HalPcmEn = HalPcmEnRtl8195a;
    pHalPcmOp->HalPcmIsrEnAndDis= HalPcmIsrEnAndDisRtl8195a;
    pHalPcmOp->HalPcmDumpReg= HalPcmDumpRegRtl8195a;
    pHalPcmOp->HalPcm= HalPcmRtl8195a;
}

#endif //CONFIG_PCM_EN
