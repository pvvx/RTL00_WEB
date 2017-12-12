/*
 *  WebSclLib: Single Compilation Unit "web"
 */

#define COMPILE_SCI 1

#include "user_config.h"
#ifdef USE_WEB
#include "autoconf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "hal_crypto.h"
#include "lwip/tcp.h"
#include "tcpsrv/tcp_srv_conn.h"
#include "web_srv_int.h"
#include "web_utils.h"
#include "flash_eep.h"
#include "device_lock.h"
#include "webfs/webfs.h"
#include "user/sys_cfg.h"
#include "wifi_api.h"
#include "sys_api.h"

#include "web_srv.h"

#include "rtl8195a/rtl_libc.h"
#include "esp_comp.h"

#ifdef WEBSOCKET_ENA
#include "web_websocket.h"
#endif

#ifdef USE_CAPTDNS
#include "captdns.h"
#endif

#ifdef USE_OVERLAY
#include "overlay.h"
#endif

#undef mMIN
#define mMIN(a, b)  ((a < b)? a : b)
#undef mMAX
#define mMAX(a, b)  ((a > b)? a : b)

#define ifcmp(a)  if(rom_xstrcmp(cstr, a))

extern int rom_atoi(const char *);
#undef atoi
#define atoi rom_atoi

extern struct netif xnetif[NET_IF_NUM]; /* network interface structure */

#include "web_auth.c"
#include "web_srv.c"
#include "web_utils.c"
#include "web_websocket.c"
#include "websock.c"

//#include "web_int_vars.c"
//#include "web_int_callbacks.c"

#endif // USE_WEB