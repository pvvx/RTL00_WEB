/******************************************************************************
 * FileName: webserver.c
 * Description: The web server mode configuration.
*******************************************************************************/

#include "user_config.h"
#ifdef USE_WEB
#include "autoconf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"
#include "tcpsrv/tcp_srv_conn.h"
#include "ethernetif.h"
#include "web_srv_int.h"
#include "web_utils.h"
#include "flash_eep.h"
#include "device_lock.h"
#include "rtl8195a/rtl_libc.h"
#include "user/sys_cfg.h"
#include "wifi_api.h"
#include "sys_api.h"
#include "esp_comp.h"

#ifdef USE_NETBIOS
#include "netbios.h"
#endif

#ifdef USE_SNTP
#include "sntp.h"
#endif

#ifdef USE_LWIP_PING
#include "lwip/app/ping.h"
struct ping_option pingopt; // for test
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
#include "mdbrs485.h"
#endif

#ifdef USE_OVERLAY
#include "overlay.h"
#endif

extern void web_get_ram(TCP_SERV_CONN *ts_conn);
extern void web_get_flash(TCP_SERV_CONN *ts_conn);
extern void web_hexdump(TCP_SERV_CONN *ts_conn);

#define ifcmp(a)  if(rom_xstrcmp(cstr, a))

extern int rom_atoi(const char *);
#define atoi rom_atoi

typedef uint32 (* call_func)(uint32 a, uint32 b, uint32 c);

/******************************************************************************
 * FunctionName : parse_url
 * Description  : parse the received data from the server
 * Parameters   : CurHTTP -- the result of parsing the url
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR web_int_vars(TCP_SERV_CONN *ts_conn, uint8 *pcmd, uint8 *pvar)
{
    WEB_SRV_CONN *web_conn = (WEB_SRV_CONN *)ts_conn->linkd;
	uint32 val = ahextoul(pvar);
	char *cstr = pcmd;
#if DEBUGSOO > 1
    os_printf("[%s=%s]\n", pcmd, pvar);
#endif
	ifcmp("start") 		web_conn->udata_start = val;
	else ifcmp("stop") 	web_conn->udata_stop = val;
	else ifcmp("sys_") {
		cstr+=4;
		ifcmp("restart") {
			if(val == 12345) web_conn->web_disc_cb = (web_func_disc_cb)sys_reset;
		}
		else ifcmp("ram") { uint32 *ptr = (uint32 *)(ahextoul(cstr+3)&(~3)); str_array(pvar, ptr, 32); }
		else ifcmp("debug") print_off = (!val) & 1; // rtl_print on/off
#ifdef USE_LWIP_PING
		else ifcmp("ping") {
//			struct ping_option *pingopt = (struct ping_option *)UartDev.rcv_buff.pRcvMsgBuff;
			pingopt.ip = ipaddr_addr(pvar);
			pingopt.count = 3;
			pingopt.recv_function=NULL;
			pingopt.sent_function=NULL;
			ping_start(&pingopt);
		}
#endif
    }
	else ifcmp("cfg_") {
		cstr += 4;
		ifcmp("web_") {
			cstr += 4;
			ifcmp("port") {
				if(syscfg.web_port != val) {
	        		web_conn->web_disc_par = syscfg.web_port; // ts_conn->pcfg->port
					syscfg.web_port = val;
	    			web_conn->web_disc_cb = (web_func_disc_cb)webserver_reinit;
				}
			}
			else ifcmp("twd") {
				if(val) {
					syscfg.cfg.b.web_time_wait_delete = 1;
					ts_conn->pcfg->flag.pcb_time_wait_free = 1;
				}
				else {
					syscfg.cfg.b.web_time_wait_delete = 0;
					ts_conn->pcfg->flag.pcb_time_wait_free = 0;
				}
			}
	   		else ifcmp("twrec") {
	   			syscfg.web_twrec = val;
   				ts_conn->pcfg->time_wait_rec = val;
	   		}
	   		else ifcmp("twcls") {
	   			syscfg.web_twcls = val;
   				ts_conn->pcfg->time_wait_cls = val;
	   		}
#if DEBUGSOO > 5
			else os_printf(" - none!\n");
#endif
		}
		else ifcmp("pinclr") 	syscfg.cfg.b.pin_clear_cfg_enable = (val)? 1 : 0;
		else ifcmp("debug") {
			syscfg.cfg.b.debug_print_enable = val;
			print_off = (!val) & 1; // rtl_print on/off
		}
		else ifcmp("save") {
			if(val == 2) SetSCB(SCB_SYSSAVE); // по закрытию соединения вызвать sys_write_cfg()
			else if(val == 1) sys_write_cfg();
		}
#ifdef USE_NETBIOS
		else ifcmp("netbios") {
			syscfg.cfg.b.netbios_ena = (val)? 1 : 0;
			if(syscfg.cfg.b.netbios_ena) netbios_init();
			else netbios_off();
		}
#endif
#ifdef USE_SNTP
		else ifcmp("sntp") {
			syscfg.cfg.b.sntp_ena = (val)? 1 : 0;
			if(syscfg.cfg.b.sntp_ena) sntp_inits();
			else sntp_close();
		}
#endif
#ifdef USE_CAPTDNS
		else ifcmp("cdns") {
			syscfg.cfg.b.cdns_ena = (val)? 1 : 0;
			if(syscfg.cfg.b.cdns_ena && wifi_softap_get_station_num()) captdns_init();
			else captdns_close();
		}
#endif
#if DEBUGSOO > 5
		else os_printf(" - none!\n");
#endif
		// sys_write_cfg();
	}
    else ifcmp("wifi_") {
      cstr+=5;
      ifcmp("rdcfg") web_conn->udata_stop = read_wifi_cfg(val);
      else ifcmp("newcfg") {
    	  web_conn->web_disc_cb = (web_func_disc_cb)wifi_run;
    	  web_conn->web_disc_par = wifi_cfg.mode;
      }
      else ifcmp("mode")	wifi_cfg.mode = val;
      else ifcmp("bgn")  	wifi_cfg.bgn = val;
      else ifcmp("lflg") 	wifi_cfg.load_flg = val;
      else ifcmp("sflg") 	wifi_cfg.save_flg = val;
      else ifcmp("sleep") 	wifi_cfg.sleep = val;
      else ifcmp("txpow") 	wifi_cfg.tx_pwr = val;
      else ifcmp("country") wifi_cfg.country_code = val;
//      else ifcmp("scan") {
//    	  web_conn->web_disc_par = val;
//    	  web_conn->web_disc_cb = (web_func_disc_cb)wifi_start_scan;
//      }
      else ifcmp("save") { write_wifi_cfg(val); }
      else ifcmp("ap_") {
    	  cstr+=3;
          ifcmp("ssid") {
        	  if(pvar[0]!='\0') {
        		  int len = os_strlen(pvar);
        		  if(len > sizeof(wifi_ap_cfg.ssid)) {
        			  len = sizeof(wifi_ap_cfg.ssid);
        		  }
        		  else os_memset(wifi_ap_cfg.ssid, 0, sizeof(wifi_ap_cfg.ssid));
        		  os_memcpy(wifi_ap_cfg.ssid, pvar, len);
#ifdef USE_NETBIOS
        		  netbios_set_name(wifi_ap_cfg.ssid);
#endif
        	  }
          }
          else ifcmp("psw") {
    		  int len = os_strlen(pvar);
    		  if(len > sizeof(wifi_ap_cfg.password)) {
    			  len = sizeof(wifi_ap_cfg.password);
    		  }
    		  else os_memset(wifi_ap_cfg.password, 0, sizeof(wifi_ap_cfg.password));
    		  os_memcpy(wifi_ap_cfg.password, pvar, len);
          }
          else ifcmp("chl") 	wifi_ap_cfg.channel = val;
          else ifcmp("mcns") 	wifi_ap_cfg.max_sta = val;
          else ifcmp("auth") 	wifi_ap_cfg.security_type = (val)? RTW_SECURITY_WEP_PSK : RTW_SECURITY_OPEN;
          else ifcmp("hssid") 	wifi_ap_cfg.ssid_hidden = val;
          else ifcmp("bint") 	wifi_ap_cfg.beacon_interval = val;
#if LWIP_NETIF_HOSTNAME
          else ifcmp("hostname") {
       		  int len = os_strlen(pvar);
       		  if(len >= LWIP_NETIF_HOSTNAME_SIZE) {
       			  len = LWIP_NETIF_HOSTNAME_SIZE-1;
       		  }
       		  os_memcpy(lwip_host_name[wlan_ap_netifn], pvar, len);
       		  lwip_host_name[wlan_ap_netifn][len] = 0;
       		  netbios_set_name(wlan_ap_netifn, pvar);
          }
#endif
          else ifcmp("dhcp")	wifi_ap_dhcp.mode = val;
          else ifcmp("ip") 		wifi_ap_dhcp.ip = ipaddr_addr(pvar);
          else ifcmp("gw") 		wifi_ap_dhcp.gw = ipaddr_addr(pvar);
          else ifcmp("msk") 	wifi_ap_dhcp.mask = ipaddr_addr(pvar);
//        else ifcmp("mac") 	strtomac(pvar, wifi_ap_cfg.macaddr);
//    	  else ifcmp("sip") 	wifi_ap_dhcp.start_ip = ipaddr_addr(pvar);
//    	  else ifcmp("eip") 	wifi_ap_dhcp.end_ip = ipaddr_addr(pvar);
#if DEBUGSOO > 2
          else os_printf(" - none! ");
#endif
      }
      else ifcmp("st_") {
    	  cstr+=3;
          ifcmp("arec") 	wifi_st_cfg.autoreconnect = val;
          else ifcmp("rect") 	wifi_st_cfg.reconnect_pause = val;
          else ifcmp("ssid") {
        	  if(pvar[0]!='\0') {
           		  int len = os_strlen(pvar);
           		  if(len > sizeof(wifi_st_cfg.ssid)) {
           			  len = sizeof(wifi_st_cfg.ssid);
           		  }
           		  else os_memset(wifi_st_cfg.ssid, 0, sizeof(wifi_st_cfg.ssid));
           		  os_memcpy(wifi_st_cfg.ssid, pvar, len);
        	  }
          }
          else ifcmp("psw") {
    		  int len = os_strlen(pvar);
    		  if(len > sizeof(wifi_st_cfg.password)) {
    			  len = sizeof(wifi_st_cfg.password);
    		  }
    		  else os_memset(wifi_st_cfg.password, 0, sizeof(wifi_st_cfg.password));
    		  os_memcpy(wifi_st_cfg.password, pvar, len);
          }
          else ifcmp("auth") 	wifi_st_cfg.security_type = val;
          else ifcmp("bssid") 	strtomac(pvar, wifi_st_cfg.bssid);
          else ifcmp("sbss") 	wifi_st_cfg.flg = val;
#if LWIP_NETIF_HOSTNAME
          else ifcmp("hostname") {
       		  int len = os_strlen(pvar);
       		  if(len >= LWIP_NETIF_HOSTNAME_SIZE) {
       			  len = LWIP_NETIF_HOSTNAME_SIZE-1;
       		  }
       		  os_memcpy(lwip_host_name[wlan_st_netifn], pvar, len);
       		  lwip_host_name[wlan_st_netifn][len] = 0;
       		  netbios_set_name(wlan_st_netifn, pvar);
          }
#endif
          else ifcmp("dhcp") 	wifi_st_dhcp.mode = val;
    	  else ifcmp("ip") 		wifi_st_dhcp.ip = ipaddr_addr(pvar);
          else ifcmp("gw") 		wifi_st_dhcp.gw = ipaddr_addr(pvar);
          else ifcmp("msk") 	wifi_st_dhcp.mask = ipaddr_addr(pvar);
//          else ifcmp("mac") 	strtomac(pvar, wifi_st_cfg.mac);
//          else ifcmp("sbss") 	wifi_st_cfg.bssidx = val;
#if DEBUGSOO > 5
          else os_printf(" - none!\n");
#endif
      }
#if DEBUGSOO > 5
      else os_printf(" - none!\n");
#endif
    }
    else if(web_conn->bffiles[0]==WEBFS_WEBCGI_HANDLE && CheckSCB(SCB_GET)) {
    	ifcmp("hexdmp") {
#if DEBUGSOO > 5
    	os_printf("hexdmp(%p)\n", val);
#endif
    		if(val > 0) {
    	    	if(cstr[6]=='d') ts_conn->flag.user_option1 = 1;
    	    	else ts_conn->flag.user_option1 = 0;
    			uint32 x = ahextoul(cstr+7);
    			web_conn->udata_start = x;
    			web_conn->udata_stop = val + web_conn->udata_start;
#if DEBUGSOO > 5
    			os_printf("start=%p, stop=%p\n", web_conn->udata_start, web_conn->udata_stop);
#endif
    			web_conn->fileType = HTTP_TXT;
    			SetSCB(SCB_RETRYCB | SCB_FCALBACK);
    			SetNextFunSCB(web_hexdump);
    		};
    	}
    	else ifcmp("flash") {
    		cstr+=5;
    		if(*cstr == '_') {
    			cstr++;
        		ifcmp("all") {
        	    	web_conn->udata_start = 0;
        	    	web_conn->udata_stop = spi_flash_real_size();
        			web_conn->fileType = HTTP_BIN;
        			SetSCB(SCB_RETRYCB | SCB_FCALBACK);
        			SetNextFunSCB(web_get_flash);
        		}
        		else ifcmp("sec_") {
        			web_conn->udata_start = ahextoul(cstr+4) << 12;
        			web_conn->udata_stop = web_conn->udata_start + FLASH_SECTOR_SIZE*val;
        			web_conn->fileType = HTTP_BIN;
        			SetSCB(SCB_RETRYCB | SCB_FCALBACK);
        			SetNextFunSCB(web_get_flash);
        		}
        		else ifcmp("disk") {
        			web_conn->udata_start = WEBFS_base_addr();
        			web_conn->udata_stop = web_conn->udata_start + WEBFS_curent_size();
        			web_conn->fileType = HTTP_BIN;
        			SetSCB(SCB_RETRYCB | SCB_FCALBACK);
        			SetNextFunSCB(web_get_flash);
        		}
        		else tcp_put('?');
    		}
    		else {
    			web_conn->fileType = HTTP_BIN;
    			SetSCB(SCB_RETRYCB | SCB_FCALBACK);
    			SetNextFunSCB(web_get_flash);
    		}
    	}
    	else ifcmp("bin_ram") {
			web_conn->fileType = HTTP_BIN;
			SetSCB(SCB_RETRYCB | SCB_FCALBACK);
			SetNextFunSCB(web_get_ram);
    	}
#if DEBUGSOO > 5
    	else os_printf(" - none! ");
#endif
    }
#if DEBUGSOO > 5
    else os_printf(" - none! ");
#endif
}

#endif // USE_WEB