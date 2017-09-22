/*
 * wlan_tst.c
 *
 *  Created on: 10 апр. 2017 г.
 *      Author: PVV
 */
#include <platform_opts.h>
#include "rtl8195a.h"
#include "FreeRTOS.h"
#include "task.h"
#if 1
#include "drv_types.h" // or #include "wlan_lib.h"
#else
#include "wifi_constants.h"
#include "wifi_structures.h"
#include "wlan_lib.h" // or #include "drv_types.h"
#endif

//#include "section_config.h"
//#include "hal_diag.h"
#include "rtl8195a/rtl_libc.h"

extern void dump_bytes(uint32 addr, int size);
extern Rltk_wlan_t rltk_wlan_info[2]; // in wrapper.h

LOCAL void tst_wlan_struct(int argc, char *argv[])
{
	(void)argc; (void)argv;
	printf("Test: sizeof(struct _ADAPTER) = %d\n", sizeof(struct _ADAPTER)); //6088
	printf("mlmeextpriv\t+%d\n", offsetof(struct _ADAPTER, mlmeextpriv)); //+1256
	printf("TSFValue\t+%d\n", offsetof(struct _ADAPTER, mlmeextpriv.TSFValue)); //+1992
	printf("stapriv\t\t+%d\n", offsetof(struct _ADAPTER, stapriv)); //+3024 [164]
	printf("pwrctrlpriv.bInternalAutoSuspend +%d\n", offsetof(struct _ADAPTER, pwrctrlpriv.bInternalAutoSuspend));  //+5061
	printf("eeprompriv\t+%d\n", offsetof(struct _ADAPTER, eeprompriv)); // +5128
	printf("HalData\t\t+%d\n", offsetof(struct _ADAPTER, HalData)); //+5656
	printf("HalFunc\t\t+%d\n", offsetof(struct _ADAPTER, HalFunc)); //+5664
	printf("bDriverStopped\t+%d\n", offsetof(struct _ADAPTER, bDriverStopped)); //+5880
	printf("hw_init_completed +%d\n", offsetof(struct _ADAPTER, hw_init_completed)); //+5905
	printf("stats\t\t+%d\n", offsetof(struct _ADAPTER, stats)); //+6024
	printf("hw_init_mutex\t+%d\n", offsetof(struct _ADAPTER, hw_init_mutex)); //+6060
	printf("fix_rate\t+%d\n", offsetof(struct _ADAPTER, fix_rate)); //+6084

	printf("rltk_wlan_info = %p\n", &rltk_wlan_info);
	dump_bytes((u32)&rltk_wlan_info, sizeof(rltk_wlan_info));
	_adapter * ad = *(_adapter **)((rltk_wlan_info[0].dev)->priv);
	printf("adapter0 = %p, %p\n", ad, ad->pbuddy_adapter);
	ad = *(_adapter **)((rltk_wlan_info[1].dev)->priv);
	printf("adapter1 = %p, %p\n", ad, ad->pbuddy_adapter);
	vTaskDelay(5);
	dump_bytes((u32)ad,sizeof(struct _ADAPTER));
	vTaskDelay(5);

	if (sizeof(struct _ADAPTER) != 6088) {
		printf("Error: Check WiFi adapter struct!\n");
	};
}

LOCAL void show_wlan_param(int argc, char *argv[]) {
	(void)argc; (void)argv;
	_adapter * ad = *(_adapter **)((rltk_wlan_info[0].dev)->priv);
#if 1
	printf("reconnect_deauth_filtered\t%u\n", ad->mlmeextpriv.reconnect_deauth_filtered);
	printf("reconnect_times\t%u\n", ad->mlmeextpriv.reconnect_times);
	printf("reconnect_cnt\t%u\n", ad->mlmeextpriv.reconnect_cnt);
	printf("reconnect_timeout\t%u\n", ad->mlmeextpriv.reconnect_timeout);
#endif

}



MON_RAM_TAB_SECTION COMMAND_TABLE console_wlan_tst[] = {
		{"CHKWL", 0, tst_wlan_struct, ": Chk wlan struct"},
		{"CHKAP", 0, show_wlan_param, ": Chow wlan parm"}
};
