/*
 * web_auth.c
 *
 *  Created on: 23/04/2017.
 *      Author: pvvx
 */
#include "autoconf.h"
#include "FreeRTOS.h"
#include "diag.h"
#include "web_utils.h"
#include "wifi_api.h"
#include "web_srv.h"
#include "rtl8195a/rtl_libc.h"
#include "esp_comp.h"


/* ----------------------------------------------------------------------------------
 * pbuf[77] = Username and password are combined into a string "username:password"
 * Return: Authorization Level
 * 0 - Not Authorized  */

uint8 UserAuthorization(uint8 *pbuf, size_t declen)
{
		uint8 * psw = rtl_strchr(pbuf, ':');
		if(psw != NULL) {
#if USE_WEB_AUTH_LEVEL
			if(rtl_strcmp(pbuf, "rtl871x:webfs_write") == 0) {
				return WEB_AUTH_LEVEL_WEBFS;
			}
			if(rtl_strcmp(pbuf, "rtl871x:ota_write") == 0) {
				return WEB_AUTH_LEVEL_OTA;
			}
			if(rtl_strcmp(pbuf, "rtl871x:supervisor") == 0) {
				return WEB_AUTH_LEVEL_SUPERVISOR;
			}
#endif
			*psw++ = 0;
			if(rom_xstrcmp(wifi_ap_cfg.ssid, pbuf)
			&& rom_xstrcmp( wifi_ap_cfg.password, psw)) {
				return WEB_AUTH_LEVEL_USER;
			}
			if(rom_xstrcmp(wifi_st_cfg.ssid, pbuf)
			&& rom_xstrcmp( wifi_st_cfg.password, psw)) {
				return WEB_AUTH_LEVEL_USER1;
			}
		}
		return 0;
}
