/*
 *  WebSclLib: Single Compilation Unit "web_user"
 */

#define COMPILE_SCI 1

#include "user_config.h"
#ifdef USE_WEB
#include "autoconf.h"
#include "FreeRTOS.h"
#include "freertos_pmu.h"
#include "task.h"
#include "diag.h"
#include "tcm_heap.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"
#include "tcpsrv/tcp_srv_conn.h"
#include "ethernetif.h"
#include "web_srv_int.h"
#include "web_utils.h"
#include "webfs/webfs.h"
#include "flash_eep.h"
#include "device_lock.h"
#include "rtl8195a/rtl_libc.h"
#include "user/sys_cfg.h"
#include "wifi_api.h"
#include "sleep_ex_api.h"
#include "sys_api.h"
#include "esp_comp.h"

#ifdef USE_NETBIOS
#include "netbios/netbios.h"
#endif

#ifdef USE_SNTP
#include "sntp/sntp.h"
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
#undef atoi
#define atoi rom_atoi

#undef mMIN
#define mMIN(a, b)  ((a < b)? a : b)
#undef mMAX
#define mMAX(a, b)  ((a > b)? a : b)

#define ifcmp(a)  if(rom_xstrcmp(cstr, a))

extern int rom_atoi(const char *);
#undef atoi
#define atoi rom_atoi

extern struct netif xnetif[NET_IF_NUM]; /* network interface structure */

#include "web_int_vars.c"
#include "web_int_callbacks.c"

#endif // USE_WEB