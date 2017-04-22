/*
 *
 */

#include "platform_autoconf.h"
#include "autoconf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "hal_crypto.h"
#include "hal_log_uart.h"
#include "hal_misc.h"
#include "diag.h"
//#include "wdt_api.h"
//#include <osdep_service.h>
#include "hal_platform.h"
#include "rtl8195a_sys_on.h"

#ifdef CONFIG_WDG_ON_IDLE
#include "hal_peri_on.h"
#include "rtl8195a_peri_on.h"
#endif

/* ---------------------------------------------------
  *  Customized Signature (Image Name)
  * ---------------------------------------------------*/
#include "section_config.h"
SECTION(".custom.validate.rodata")
const unsigned char cus_sig[32] = "WEB Sample";

#ifdef CONFIG_DEBUG_LOG
#define DEBUG_MAIN_LEVEL CONFIG_DEBUG_LOG
#else
#define DEBUG_MAIN_LEVEL 0
#endif

#ifndef CONFIG_INIT_NET
#define CONFIG_INIT_NET             1
#endif
#ifndef CONFIG_INTERACTIVE_MODE
#define CONFIG_INTERACTIVE_MODE     1
#endif

extern void user_init_thrd(void);

/* RAM/TCM/Heaps info */
void ShowMemInfo(void)
{
	DiagPrintf("\nCLK CPU\t\t%d Hz\nRAM heap\t%d bytes\nTCM heap\t%d bytes\n",
			HalGetCpuClk(), xPortGetFreeHeapSize(), tcm_heap_freeSpace());
}

/* main */
void main(void)
{
#if DEBUG_MAIN_LEVEL > 3
	 ConfigDebugErr  = -1;
	 ConfigDebugInfo = ~(_DBG_SPI_FLASH_);//|_DBG_TCM_HEAP_);
	 ConfigDebugWarn = -1;
	 CfgSysDebugErr = -1;
	 CfgSysDebugInfo = -1;
	 CfgSysDebugWarn = -1;
#endif

#ifdef CONFIG_WDG_ON_IDLE
	HAL_PERI_ON_WRITE32(REG_SOC_FUNC_EN, HAL_PERI_ON_READ32(REG_SOC_FUNC_EN) & 0x1FFFFF);
#if CONFIG_DEBUG_LOG > 3
	WDGInitial(CONFIG_WDG_ON_IDLE * 3000); // 30 s
#else
	WDGInitial(CONFIG_WDG_ON_IDLE * 1000); // 10 s
#endif
	WDGStart();
#endif

#if (defined(CONFIG_CRYPTO_STARTUP) && (CONFIG_CRYPTO_STARTUP))
	 if(rtl_cryptoEngine_init() != 0 ) {
		 DBG_8195A("Crypto engine init failed!\n");
	 }
#endif

#if DEBUG_MAIN_LEVEL > 1
	vPortFree(pvPortMalloc(4)); // Init RAM heap
	ShowMemInfo(); // RAM/TCM/Heaps info
#endif

	/* wlan & user_start intialization */
	xTaskCreate(user_init_thrd, "user_init", 1024, NULL, tskIDLE_PRIORITY + 1 + PRIORITIE_OFFSET, NULL);

	/*Enable Schedule, Start Kernel*/
#if defined(CONFIG_KERNEL) && !TASK_SCHEDULER_DISABLED
#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
#endif
#else
	RtlConsolTaskRom(NULL);
#endif
}
