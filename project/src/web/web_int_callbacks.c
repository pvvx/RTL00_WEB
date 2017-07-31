/******************************************************************************
 * FileName: web_int_callbacks.c
 * Description: The web server inernal callbacks.
*******************************************************************************/

#include "user_config.h"
#ifdef USE_WEB
#include "autoconf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "lwip/tcp.h"
#include "flash_eep.h"
#include "device_lock.h"
#include "ethernetif.h"
#include "tcpsrv/tcp_srv_conn.h"
#include "web_srv_int.h"
#include "web_utils.h"
#include "webfs/webfs.h"
#include "rtl8195a/rtl_libc.h"
#include "user/sys_cfg.h"
#include "wifi_api.h"
#include "sys_api.h"
#include "esp_comp.h"

#ifdef USE_NETBIOS
#include "netbios/netbios.h"
#endif

#ifdef USE_SNTP
#include "sntp/sntp.h"
#endif

#ifdef USE_CAPTDNS
#include "captdns.h"
#endif

#ifdef USE_MODBUS
#include "modbustcp.h"
#include "mdbtab.h"
#endif

#ifdef USE_RS485DRV
#include "driver/rs485drv.h"
#endif

#ifdef USE_OVERLAY
#include "overlay.h"
#endif

#define atoi rom_atoi

#define mMIN(a, b)  ((a<b)?a:b)
#define ifcmp(a)  if(rom_xstrcmp(cstr, a))

#define OpenFlash() { device_mutex_lock(RT_DEV_LOCK_FLASH); flash_turnon(); }
#define CloseFlash() { SpicDisableRtl8195A(); device_mutex_unlock(RT_DEV_LOCK_FLASH); }


extern struct netif xnetif[NET_IF_NUM]; /* network interface structure */


#if WEB_DEBUG_FUNCTIONS
//#define TEST_SEND_WAVE
#endif // #if WEB_DEBUG_FUNCTIONS

#ifdef TEST_SEND_WAVE
//-------------------------------------------------------------------------------
// Test adc
// Читает adc в одиночный буфер (~2килобайта) на ~20ksps и сохраняет в виде WAV
// Правильное чтение организуется по прерыванию таймера(!).
// Тут только демо!
//-------------------------------------------------------------------------------
typedef struct
{ // https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
  unsigned long int RIFF ;/* +00 'RIFF'           */
  unsigned long int size8;/* +04 file size - 8    */
  unsigned long int WAVE ;/* +08 'WAVE'           */
  unsigned long int fmt  ;/* +12 'fmt '           */
  unsigned long int fsize;/* +16 указатель до 'fact' или 'data' */
  unsigned short int ccod;/* +20 01 00  Compression code: 1 - PCM/uncompressed */
  unsigned short int mono;/* +22 00 01 или 00 02  */
  unsigned long int freq ;/* +24 частота          */
  unsigned long int bps  ;/* +28                  */
  unsigned short int blka;/* +32 1/2/4  BlockAlign*/
  unsigned short int bits;/* +34 разрядность 8/16 */
  unsigned long int data ;/* +36 'data'           */
  unsigned long int dsize;/* +40 размер данных    */
} WAV_HEADER;
const WAV_HEADER ICACHE_RODATA_ATTR wav_header =
{0x46464952L,
 0x00000008L,
 0x45564157L,
 0x20746d66L,
 0x00000010L,
 0x0001     ,
 0x0001     ,
 0x000055f0L,
 0x000055f0L,
 0x0002     ,
 0x0010     ,
 0x61746164L,
 0x00000000L};
#define WAV_HEADER_SIZE sizeof(wav_header)
//===============================================================================
// web_test_adc()
//-------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR web_test_adc(TCP_SERV_CONN *ts_conn)
{
	WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *) ts_conn->linkd;
    unsigned int len = web_conn->msgbufsize - web_conn->msgbuflen;
    if(len > WAV_HEADER_SIZE + 10) {
    	len -= WAV_HEADER_SIZE;
    	WAV_HEADER * ptr = (WAV_HEADER *) &web_conn->msgbuf[web_conn->msgbuflen];
    	os_memcpy(ptr, &wav_header, WAV_HEADER_SIZE);
    	ptr->dsize = len;
    	web_conn->msgbuflen += WAV_HEADER_SIZE;
    	len >>= 1;
    	read_adcs((uint16 *)(web_conn->msgbuf + web_conn->msgbuflen), len, 0x0808);
    	web_conn->msgbuflen += len << 1;
    }
    if(!CheckSCB(SCB_WEBSOC)) SetSCB(SCB_FCLOSE | SCB_DISCONNECT); // connection close
}
#endif // TEST_SEND_WAVE

//===============================================================================
// WiFi Scan XML
//-------------------------------------------------------------------------------
LOCAL void ICACHE_FLASH_ATTR web_wscan_xml(TCP_SERV_CONN *ts_conn)
{
	WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *) ts_conn->linkd;
	web_scan_handler_t * pwscn_rec = &web_scan_handler_ptr;
    // Check if this is a first round call
    if(CheckSCB(SCB_RETRYCB)==0) {
    	int i = 0;
		web_conn->udata_start = 0;
    	if(pwscn_rec->flg == 2) {
    		i = pwscn_rec->ap_count;
    		wifi_set_timer_scan(7000);
    	} else if(pwscn_rec->flg == 0) api_wifi_scan(NULL);
		tcp_puts_fd("<total>%d</total>", i);
		if(i == 0) return;
    }
	while(web_conn->msgbuflen + 96 + 10 + 32 <= web_conn->msgbufsize) {
	    if(pwscn_rec->flg && web_conn->udata_start < pwscn_rec->ap_count) {
	    	rtw_scan_result_t *si = &pwscn_rec->ap_details[web_conn->udata_start];
	    	uint8 ssid[32*6 + 1];
	    	int len = si->SSID.len;
	    	if(len > 32) len = 32;
	    	si->SSID.val[len] = '\0';
	    	if(web_conn->msgbuflen + 96 + 10 + htmlcode(ssid, si->SSID.val, 32*6, 32) > web_conn->msgbufsize) break;
			tcp_puts_fd("<ap id=\"%d\"><ch>%d</ch><au>%d</au><bs>" MACSTR "</bs><ss>%s</ss><rs>%d</rs><hd>%d</hd><wp>%d</wp></ap>",
					web_conn->udata_start,
					si->channel,
					rtw_security_to_idx(si->security),
					MAC2STR(si->BSSID.octet),
					ssid,
					si->signal_strength,
//					si->band,	// rtw_802_11_band_t
					si->bss_type & 3, // rtw_bss_type_t
					si->wps_type); // rtw_wps_type_t
	   		web_conn->udata_start++;
	    	if(web_conn->udata_start >= pwscn_rec->ap_count) {
	    		wifi_close_scan();
			    ClrSCB(SCB_RETRYCB);
			    return;
	    	}
	    }
	    else {
    		wifi_close_scan();
		    ClrSCB(SCB_RETRYCB);
		    return;
	    }
	}
	// repeat in the next call ...
    SetSCB(SCB_RETRYCB);
    SetNextFunSCB(web_wscan_xml);
    return;
}
#ifdef USE_MODBUS
//===============================================================================
// Mdb XML
//-------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR web_modbus_xml(TCP_SERV_CONN *ts_conn)
{
	WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *) ts_conn->linkd;
	while(web_conn->msgbuflen + 24 <= web_conn->msgbufsize) {
	    if(web_conn->udata_start < web_conn->udata_stop) {
	    	uint16 val16;
	    	if(RdMdbData((uint8 *)&val16, web_conn->udata_start, 1) != 0) tcp_puts_fd("<m%u>?</m%u>", web_conn->udata_start, web_conn->udata_start);
	    	else {
		    	if(ts_conn->flag.user_option2) {
		    		if(ts_conn->flag.user_option1) {
			    		tcp_puts_fd("<m%u>0x%04x</m%u>", web_conn->udata_start, val16, web_conn->udata_start);
		    		}
				    else {
			    		tcp_puts_fd("<m%u>%04x</m%u>", web_conn->udata_start, val16, web_conn->udata_start);
				    };
		    	}
		    	else {
			    	if(ts_conn->flag.user_option1) {
			    		tcp_puts_fd("<m%u>%d</m%u>", web_conn->udata_start, (sint32)((sint16)val16), web_conn->udata_start);
			    	}
			    	else {
			    		tcp_puts_fd("<m%u>%u</m%u>", web_conn->udata_start, val16, web_conn->udata_start);
			    	};
		    	};
	    	};
	    	web_conn->udata_start++;
	    }
	    else {
		    ClrSCB(SCB_RETRYCB);
		    return;
	    }
	}
	// repeat in the next call ...
    SetSCB(SCB_RETRYCB);
    SetNextFunSCB(web_modbus_xml);
    return;
}
#endif
//===============================================================================
// RAM hexdump
//-------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR web_hexdump(TCP_SERV_CONN *ts_conn)
{
	WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *) ts_conn->linkd;
	union {
		uint32 dw[4];
		uint8 b[16];
	}data;
	web_conn->udata_start &= 0xfffffff0;
	uint32 *addr = (uint32 *)web_conn->udata_start;
	int i;
	web_conn->udata_stop &= 0xfffffff0;
	while(web_conn->msgbuflen + (9+3*16+17+2) <= web_conn->msgbufsize) {
//		if((uint32)addr < 0x9A000000) {
			if((uint32)addr >= 0x98000000 && (uint32)addr < 0x9A000000) {
				OpenFlash();
			}
			tcp_puts("%08x", addr);
			for(i=0 ; i < 4 ; i++) data.dw[i] = *addr++;
			web_conn->udata_start = (uint32)addr;
			if(ts_conn->flag.user_option1) { // dword or byte ?
				for(i=0 ; i < 4 ; i++) tcp_puts(" %08x", data.dw[i]);
			}
			else {
				for(i=0 ; i < 16 ; i++) tcp_puts(" %02x", data.b[i]);
			}
			if((uint32)addr >= 0x98000000 && (uint32)addr < 0x9A000000) {
				CloseFlash();
			}
			tcp_put(' '); tcp_put(' ');
			for(i=0 ; i < 16 ; i++) tcp_put((data.b[i] >=' ' && data.b[i] != 0x7F)? data.b[i] : '.');
			tcp_puts("\r\n");
			if((uint32)addr >= web_conn->udata_stop) {
			    ClrSCB(SCB_RETRYCB);
			    if(!CheckSCB(SCB_WEBSOC)) SetSCB(SCB_FCLOSE | SCB_DISCONNECT); // connection close
			    return;
			}
//		} else {
//			tcp_puts("%p = Bad address!\r\n", addr);
//		    ClrSCB(SCB_RETRYCB);
//		    if(!CheckSCB(SCB_WEBSOC)) SetSCB(SCB_FCLOSE | SCB_DISCONNECT); // connection close
//		    return;
//		};
	}
	// repeat in the next call ...
    SetSCB(SCB_RETRYCB);
    SetNextFunSCB(web_hexdump);
    return;
}
/******************************************************************************
 * FunctionName : web saved flash
 * Description  : Processing the flash data send
 * Parameters   : none (Calback)
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR web_get_flash(TCP_SERV_CONN *ts_conn)
{
    WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *)ts_conn->linkd;
    // Check if this is a first round call
    if(CheckSCB(SCB_RETRYCB)==0) {
    	if(web_conn->udata_start == web_conn->udata_stop) return;
#if DEBUGSOO > 2
		os_printf("file_size:%08x ", web_conn->udata_stop - web_conn->udata_start );
#endif
    }
    // Get/put as many bytes as possible
    unsigned int len = mMIN(web_conn->msgbufsize - web_conn->msgbuflen, web_conn->udata_stop - web_conn->udata_start);
    // Read Flash addr = web_conn->webfinc_offsets, len = x, buf = sendbuf
#if DEBUGSOO > 2
	os_printf("%08x..%08x ",web_conn->udata_start, web_conn->udata_start + len );
#endif
	device_mutex_lock(RT_DEV_LOCK_FLASH);
    if(spi_flash_read(web_conn->udata_start, web_conn->msgbuf, len)) {
      web_conn->udata_start += len;
      web_conn->msgbuflen += len;
      if(web_conn->udata_start < web_conn->udata_stop) {
        SetNextFunSCB(web_get_flash);
   	    device_mutex_unlock(RT_DEV_LOCK_FLASH);
        SetSCB(SCB_RETRYCB);
        return;
      };
    };
    device_mutex_unlock(RT_DEV_LOCK_FLASH);
    ClrSCB(SCB_RETRYCB);
//    SetSCB(SCB_FCLOSE | SCB_DISCONNECT);
    return;
}
/******************************************************************************
 * FunctionName : web saved flash
 * Description  : Processing the flash data send
 * Parameters   : none (Calback)
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR web_get_ram(TCP_SERV_CONN *ts_conn)
{
    WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *)ts_conn->linkd;
    // Check if this is a first round call
    if(CheckSCB(SCB_RETRYCB)==0) { // On initial call, проверка параметров
		if(web_conn->udata_start == web_conn->udata_stop) {
//    	    SetSCB(SCB_FCLOSE | SCB_DISCONNECT);
    	    return;
		}
#if DEBUGSOO > 2
		os_printf("file_size:%08x ", web_conn->udata_stop - web_conn->udata_start );
#endif
	}
    // Get/put as many bytes as possible
    uint32 len = mMIN(web_conn->msgbufsize - web_conn->msgbuflen, web_conn->udata_stop - web_conn->udata_start);
	if((uint32)web_conn->udata_start >= 0x98000000 && (uint32)web_conn->udata_start < 0x9A000000) {
		OpenFlash();
	}
	copy_align4(web_conn->msgbuf, (void *)(web_conn->udata_start), len);
	if((uint32)web_conn->udata_start >= 0x98000000 && (uint32)web_conn->udata_start < 0x9A000000) {
		CloseFlash();
	}
	web_conn->msgbuflen += len;
	web_conn->udata_start += len;
#if DEBUGSOO > 2
	os_printf("%08x-%08x ",web_conn->udata_start, web_conn->udata_start + len );
#endif
    if(web_conn->udata_start != web_conn->udata_stop) {
        SetSCB(SCB_RETRYCB);
        SetNextFunSCB(web_get_ram);
        return;
    };
    ClrSCB(SCB_RETRYCB);
//    SetSCB(SCB_FCLOSE | SCB_DISCONNECT);
    return;
}
/******************************************************************************
 * FunctionName : web_callback
 * Description  : callback
 * Parameters   : struct TCP_SERV_CONN
 * Returns      : none
 ******************************************************************************/
void ICACHE_FLASH_ATTR web_int_callback(TCP_SERV_CONN *ts_conn, uint8 *cstr)
{
    WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *)ts_conn->linkd;
//        uint8 *cstr = &web_conn->msgbuf[web_conn->msgbuflen];
        {
           uint8 *vstr = os_strchr(cstr, '=');
           if(vstr != NULL) {
        	   *vstr++ = '\0';
        	   web_int_vars(ts_conn, cstr, vstr);
        	   return;
           }
        }
#if DEBUGSOO > 3
        os_printf("[%s]\n", cstr);
#endif
        ifcmp("start") tcp_puts("0x%08x", web_conn->udata_start);
        else ifcmp("stop") tcp_puts("0x%08x", web_conn->udata_stop);
#if USE_WEB_AUTH_LEVEL
        else ifcmp("realm") tcp_puts("%u", web_conn->auth_realm);
        else ifcmp("auth") tcp_puts("%u", web_conn->auth_level);
#endif
        else ifcmp("xml_") {
            cstr+=4;
            ifcmp("scan") web_wscan_xml(ts_conn);
#if WEB_DEBUG_FUNCTIONS
            else {
#if USE_WEB_AUTH_LEVEL
            	if(web_conn->auth_level < WEB_AUTH_LEVEL_USER) return;
#endif
            	else {
            		web_conn->udata_start&=~3;
            		ifcmp("ram") tcp_puts("0x%08x", *((uint32*)web_conn->udata_start));
            		else tcp_put('?');
            		web_conn->udata_start += 4;
            	}
            }
#endif
        }
        else ifcmp("sys_") {
          cstr+=4;
          ifcmp("url") tcp_strcpy(get_new_hostname());
          else ifcmp("cid") tcp_puts("%08x", HalGetChipId());
          else ifcmp("fid") tcp_puts("%08x", spi_flash_get_id());
          else ifcmp("fsize") tcp_puts("%u", spi_flash_real_size()); // flashchip->chip_size
          else ifcmp("sdkver") tcp_strcpy_fd(SDK_VERSION);
          else ifcmp("sysver") tcp_strcpy_fd(SYS_VERSION);
          else ifcmp("webver") tcp_strcpy_fd(WEB_SVERSION);
          else ifcmp("heap")  tcp_puts("%u", xPortGetFreeHeapSize());
          else ifcmp("heapm") tcp_puts("%u", xPortGetMinimumEverFreeHeapSize());
          else ifcmp("tcmh") tcp_puts("%u", tcm_heap_freeSpace());
          else ifcmp("time") tcp_puts("%u", xTaskGetTickCount());
          else ifcmp("mactime") {
        	  if(wifi_mode) {
        		  union {
        			  uint32 dw[2];
        			  uint64 dd;
        		  }ux;
        		  ux.dd = *((uint64_t *)(WIFI_REG_BASE + 0x0560)); // REG_TSFTR -> #include "hal_com_reg.h"
        		  tcp_puts("0x%08x%08x", ux.dw[1], ux.dw[0]);
        	  }
          }
          else ifcmp("clkcpu") tcp_puts("%u", HalGetCpuClk());
          else ifcmp("debug") tcp_put('1' - (print_off & 1)); // rtl_print on/off
#if WEB_DEBUG_FUNCTIONS
          else ifcmp("restart") {
#if USE_WEB_AUTH_LEVEL
        	  if(web_conn->auth_level < WEB_AUTH_LEVEL_USER) return;
#endif
        	  webserver_qfn((web_ex_func_cb)sys_reset, NULL, 200);
          }
          else ifcmp("ram") tcp_puts("0x%08x", *((uint32 *)(ahextoul(cstr+3)&(~3))));
          else ifcmp("rdec") tcp_puts("%d", *((uint32 *)(ahextoul(cstr+4)&(~3))));
#endif // #if WEB_DEBUG_FUNCTIONS
          else ifcmp("ip") {
        	  uint32 cur_ip;
        	  if(netif_default != NULL) cur_ip = netif_default->ip_addr.addr;
			  tcp_puts(IPSTR, IP2STR(&cur_ip));
          }
#ifdef USE_NETBIOS
          else ifcmp("netbios") {
       		  if(syscfg.cfg.b.netbios_ena) tcp_strcpy(netbios_name);
          }
#endif
          else tcp_put('?');
        }
#ifdef WEB_INA219_DRV
        else ifcmp("ina219") {
        	  if(CheckSCB(SCB_WEBSOC)) {
extern int ina219_ws(TCP_SERV_CONN *ts_conn, char cmd);
				  int x = ina219_ws(ts_conn, cstr[6]);
        		  if(x < 0) SetSCB(SCB_FCLOSE|SCB_DISCONNECT);
        		  else tcp_puts("%d", x);
        	  }
        }
#endif
#ifdef WEB_ADC_DRV
        else ifcmp("adc") {
        	  if(CheckSCB(SCB_WEBSOC)) {
extern int adc_ws(TCP_SERV_CONN *ts_conn, char cmd);
				  int x = adc_ws(ts_conn, cstr[6]);
        		  if(x < 0) SetSCB(SCB_FCLOSE|SCB_DISCONNECT);
        		  else tcp_puts("%d", x);
        	  }
        }
#endif
        else ifcmp("cfg_") {
			cstr += 4;
			ifcmp("web_") {
				cstr += 4;
				ifcmp("port") tcp_puts("%u", syscfg.web_port);
	        	else ifcmp("twrec") tcp_puts("%u", syscfg.web_twrec);
	        	else ifcmp("twcls") tcp_puts("%u", syscfg.web_twcls);
				else ifcmp("twd") tcp_put((syscfg.cfg.b.web_time_wait_delete)? '1' : '0');
				else tcp_put('?');
			}
			else ifcmp("sleep") tcp_put((syscfg.cfg.b.powersave_enable)? '1' : '0');
			else ifcmp("pinclr") tcp_put((syscfg.cfg.b.pin_clear_cfg_enable)? '1' : '0');
			else ifcmp("debug") tcp_put((syscfg.cfg.b.debug_print_enable)? '1' : '0');
#ifdef USE_NETBIOS
			else ifcmp("netbios") tcp_put((syscfg.cfg.b.netbios_ena)? '1' : '0');
#endif
#ifdef USE_SNTP
			else ifcmp("sntp") tcp_put((syscfg.cfg.b.sntp_ena)? '1' : '0');
#endif
#ifdef USE_CAPTDNS
			else ifcmp("cdns") tcp_put((syscfg.cfg.b.cdns_ena)? '1' : '0');
#endif
		    else tcp_put('?');
		}
        else ifcmp("wifi_") {
          cstr+=5;
          ifcmp("rdcfg") read_wifi_cfg(-1);
          else ifcmp("newcfg") webserver_qfn((web_ex_func_cb)wifi_run, (void *)wifi_cfg.mode, 200);
          else ifcmp("cmode") tcp_puts("%d", wifi_mode);
          else ifcmp("mode") tcp_puts("%d", wifi_cfg.mode);
          else ifcmp("bgn") tcp_puts("%d", wifi_cfg.bgn);
          else ifcmp("txpow") tcp_puts("%u", wifi_cfg.tx_pwr);
          else ifcmp("lflg") tcp_puts("%u", wifi_cfg.load_flg);
          else ifcmp("sflg") tcp_puts("%u", wifi_cfg.save_flg);
          else ifcmp("adpt") tcp_puts("%u", wifi_cfg.adaptivity);
          else ifcmp("country") tcp_puts("%u", wifi_cfg.country_code);
          else ifcmp("ap_") {
        	  cstr+=3;
              ifcmp("ssid") {
            	  wifi_ap_cfg.ssid[NDIS_802_11_LENGTH_SSID] = '\0';
            	  tcp_strcpy(wifi_ap_cfg.ssid);
           	  }
              else ifcmp("psw") {
            	  wifi_ap_cfg.password[IW_PASSPHRASE_MAX_SIZE] = '\0';
            	  tcp_strcpy(wifi_ap_cfg.password);
              }
              else ifcmp("chl") 	tcp_puts("%u", wifi_ap_cfg.channel);
              else ifcmp("mcns") 	tcp_puts("%u", wifi_ap_cfg.max_sta);
              else ifcmp("auth") 	tcp_put((wifi_ap_cfg.security) ? '1' : '0');
              else ifcmp("hssid") 	tcp_put((wifi_ap_cfg.ssid_hidden & 1) + '0');
              else ifcmp("bint") 	tcp_puts("%u", wifi_ap_cfg.beacon_interval);
              else ifcmp("mac") 	tcp_puts(MACSTR, MAC2STR(xnetif[WLAN_AP_NETIF_NUM].hwaddr));
              else ifcmp("hostname") tcp_strcpy(lwip_host_name[1]);
              else ifcmp("dhcp")	tcp_puts("%u", wifi_ap_dhcp.mode);
              else ifcmp("ip") 		tcp_puts(IPSTR, IP2STR(&wifi_ap_dhcp.ip));
              else ifcmp("gw") 		tcp_puts(IPSTR, IP2STR(&wifi_ap_dhcp.gw));
              else ifcmp("msk") 	tcp_puts(IPSTR, IP2STR(&wifi_ap_dhcp.mask));
              else ifcmp("cip") 	tcp_puts(IPSTR, IP2STR(&xnetif[WLAN_ST_NETIF_NUM].ip_addr.addr));
    //        else ifcmp("mac") 	strtomac(pvar, wifi_ap_cfg.macaddr);
    //    	  else ifcmp("sip") 	tcp_puts(IPSTR, IP2STR(&wifi_ap_dhcp.start_ip));
    //    	  else ifcmp("eip") 	tcp_puts(IPSTR, IP2STR(&wifi_ap_dhcp.end_ip));
    #if DEBUGSOO > 2
              else os_printf(" - none! ");
    #endif
          }
          else ifcmp("st_") {
        	  cstr+=3;
              ifcmp("rssi") 	{
            	  int rssi;
            	  wifi_get_rssi(&rssi);
            	  tcp_puts("%d", rssi);
              }
              else ifcmp("status") 	tcp_puts("%u", wifi_st_status);
              else ifcmp("arec") 	tcp_puts("%u", wifi_st_cfg.autoreconnect);
              else ifcmp("rect") 	tcp_puts("%u", wifi_st_cfg.reconnect_pause);
              ifcmp("ssid") {
            	  wifi_st_cfg.ssid[NDIS_802_11_LENGTH_SSID] = '\0';
            	  tcp_strcpy(wifi_st_cfg.ssid);
           	  }
              else ifcmp("psw") {
            	  wifi_st_cfg.password[IW_PASSPHRASE_MAX_SIZE] = '\0';
            	  tcp_strcpy(wifi_st_cfg.password);
              }
              else ifcmp("mac") 	tcp_puts(MACSTR, MAC2STR(xnetif[WLAN_ST_NETIF_NUM].hwaddr));
              else ifcmp("bssid") 	tcp_puts(MACSTR, MAC2STR(wifi_st_cfg.bssid));
              else ifcmp("sbss") 	tcp_puts("%u", wifi_st_cfg.flg);
              else ifcmp("sleep") 	tcp_puts("%d", wifi_st_cfg.sleep);
              else ifcmp("dtim") 	tcp_puts("%u", wifi_st_cfg.dtim);
#if LWIP_NETIF_HOSTNAME
              else ifcmp("hostname") tcp_strcpy(lwip_host_name[0]);
#endif
              else ifcmp("auth") 	tcp_puts("%u", wifi_st_cfg.security);
              else ifcmp("dhcp") 	tcp_puts("%u", wifi_st_dhcp.mode);
        	  else ifcmp("ip") 		tcp_puts(IPSTR, IP2STR(&wifi_st_dhcp.ip));
              else ifcmp("gw") 		tcp_puts(IPSTR, IP2STR(&wifi_st_dhcp.gw));
              else ifcmp("msk") 	tcp_puts(IPSTR, IP2STR(&wifi_st_dhcp.mask));
    #if DEBUGSOO > 5
              else os_printf(" - none!\n");
    #endif
          }
#if DEBUGSOO > 5
          else os_printf(" - none!\n");
#endif
        }
        else ifcmp("bin_") {
#if USE_WEB_AUTH_LEVEL
   			if(web_conn->auth_level < WEB_AUTH_LEVEL_USER) return;
#endif
        	cstr+=4;
        	ifcmp("flash") {
        		cstr+=5;
        		if(*cstr == '_') {
        			cstr++;
#if WEB_DEBUG_FUNCTIONS
            		ifcmp("all") {
            	    	web_conn->udata_start = 0;
            	    	web_conn->udata_stop = spi_flash_real_size();
            	    	web_get_flash(ts_conn);
            		}
            		else ifcmp("sec_") {
            			web_conn->udata_start = ahextoul(cstr+4) << 12;
            			web_conn->udata_stop = web_conn->udata_start + FLASH_SECTOR_SIZE;
            			web_get_flash(ts_conn);
            		}
            		else
#endif // #if WEB_DEBUG_FUNCTIONS
            		ifcmp("disk") {
            			web_conn->udata_start = WEBFS_base_addr();
            			web_conn->udata_stop = web_conn->udata_start + WEBFS_curent_size();
            			web_get_flash(ts_conn);
            		}
            		else tcp_put('?');
        		}
        		else web_get_flash(ts_conn);
        	}
#if WEB_DEBUG_FUNCTIONS
        	else ifcmp("ram") web_get_ram(ts_conn);
#endif // #if WEB_DEBUG_FUNCTIONS
        	else tcp_put('?');
        }
#if WEB_DEBUG_FUNCTIONS
        else ifcmp("hexdmp") {
#if USE_WEB_AUTH_LEVEL
   			if(web_conn->auth_level < WEB_AUTH_LEVEL_USER) return;
#endif
        	if(cstr[6]=='d') ts_conn->flag.user_option1 = 1;
        	else ts_conn->flag.user_option1 = 0;
        	web_hexdump(ts_conn);
        }
#endif // #if WEB_DEBUG_FUNCTIONS
        else ifcmp("web_") {
        	cstr+=4;
        	ifcmp("port") tcp_puts("%u", ts_conn->pcfg->port);
        	else ifcmp("host") tcp_puts(IPSTR ":%d", IP2STR(&(ts_conn->pcb->local_ip.addr)), ts_conn->pcb->local_port);
        	else ifcmp("remote") tcp_puts(IPSTR ":%d", IP2STR(&(ts_conn->remote_ip.dw)), ts_conn->remote_port);
        	else ifcmp("twrec") tcp_puts("%u", ts_conn->pcfg->time_wait_rec);
        	else ifcmp("twcls") tcp_puts("%u", ts_conn->pcfg->time_wait_cls);
        	else tcp_put('?');
        }
        else ifcmp("wfs_") {
        	cstr+=4;
        	ifcmp("files") tcp_puts("%u", numFiles);
        	else ifcmp("addr") tcp_puts("0x%08x", WEBFS_base_addr());
        	else ifcmp("size") tcp_puts("%u", WEBFS_curent_size());
        	else ifcmp("max_size") tcp_puts("%u", WEBFS_max_size());
        	else tcp_put('?');
        }
#ifdef USE_OVERLAY
		else ifcmp("ovl") {
			cstr += 3;
			if(*cstr == ':') {
				int i = ovl_loader(cstr + 1);
				if (i == 0) {
					if(CheckSCB(SCB_WEBSOC)) {
						tcp_puts("%d", ovl_call(1));
					}
					else {
						web_conn->web_disc_cb = (web_func_disc_cb)ovl_call; // адрес старта оверлея
						web_conn->web_disc_par = 1; // параметр функции - инициализация
					}
				}
				tcp_puts("%d", i);
			}
			else if(*cstr == '$') {
    			if(ovl_call != NULL) tcp_puts("%d", ovl_call(ahextoul(cstr + 1)));
    			else tcp_put('?');
    		}
			else if(*cstr == '@') {
    			if(ovl_call != NULL) tcp_puts("%d", ovl_call((int) cstr + 1));
    			else tcp_put('?');
    		}
			else tcp_put('?');
		}
#endif
#ifdef USE_SNTP
		else ifcmp("sntp_") {
			cstr += 5;
			ifcmp("time") tcp_puts("%u", sntp_gen_system_time(0)); // get_sntp_time
			else tcp_put('?');
		}
#endif
#ifdef TEST_SEND_WAVE
        else ifcmp("test_adc") web_test_adc(ts_conn);
#endif
        else ifcmp("hellomsg") tcp_puts_fd("Web on RTL871x!");
		else tcp_put('?');
}

#endif // USE_WEB
