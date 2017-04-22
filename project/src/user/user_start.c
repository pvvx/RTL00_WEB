/*
 * user_start.c
 *
 *  Created on: 26/03/2017
 *      Author: pvvx
 */
#include "user_config.h"
#include "platform_autoconf.h"
#include "autoconf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "netbios/netbios.h"
#include "user/sys_cfg.h"
#include "web/web_srv.h"
#include "webfs/webfs.h"

struct SystemCfg syscfg = {
		.cfg.w = SYS_CFG_DEBUG_ENA | SYS_CFG_NETBIOS_ENA,
#if defined(USE_WEB)
		.web_port = USE_WEB,
#else
		.web_port = 0,
#endif
		.web_twrec = 5,
		.web_twcls = 5
};

void connect_start(void)
{
	info_printf("\%s: Time at start %d ms.\n", __func__, xTaskGetTickCount());
}

void connect_close(void)
{
	info_printf("\%s: Time at start %d ms.\n", __func__, xTaskGetTickCount());
}

void user_start(void)
{
	info_printf("\%s: Time at start %d ms.\n", __func__, xTaskGetTickCount());

}

void sys_write_cfg(void)
{
	flash_write_cfg(&syscfg, FEEP_ID_SYS_CFG, sizeof(syscfg));
}


void user_init_thrd(void) {

	flash_read_cfg(&syscfg, FEEP_ID_SYS_CFG, sizeof(syscfg));

	if(!syscfg.cfg.b.debug_print_enable) print_off = 1;

	/* Initilaize the console stack */

	console_init();

	/* Web Disk Init */
	WEBFSInit();

	/* Load cfg, init WiFi + LwIP init, WiFi start if wifi_cfg.mode !=  RTW_MODE_NONE */
	wifi_init();

	if(syscfg.cfg.b.netbios_ena) netbios_init();

	// webstuff_init(); // httpd_init();
	webserver_init(syscfg.web_port);

	//	xTaskCreate(x_init_thrd, "wifi_init", 1024, NULL, tskIDLE_PRIORITY + 1 + PRIORITIE_OFFSET, NULL);

	/* Kill init thread after all init tasks done */
	vTaskDelete(NULL);
}

