#ifndef MAIN_H
#define MAIN_H

#include <autoconf.h>

#ifndef CONFIG_WLAN
#define CONFIG_WLAN	1
#endif

/* Header file declaration*/
void wlan_network();

/* Interactive Mode */
#define SERIAL_DEBUG_RX 1


#define ATVER_1 1 // For First AT command
#define ATVER_2 2 // For UART Module AT command

#if CONFIG_EXAMPLE_UART_ATCMD
#define ATCMD_VER ATVER_2
#else
#define ATCMD_VER ATVER_1
#endif



/*Static IP ADDRESS*/
#define IP_ADDR0   192
#define IP_ADDR1   168
#define IP_ADDR2   3
#define IP_ADDR3   80

/*NETMASK*/
#define NETMASK_ADDR0   255
#define NETMASK_ADDR1   255
#define NETMASK_ADDR2   255
#define NETMASK_ADDR3   0

/*Gateway Address*/
#define GW_ADDR0   192
#define GW_ADDR1   168
#define GW_ADDR2   3
#define GW_ADDR3   1

/*******************************************/

/*Static IP ADDRESS*/
#define AP_IP_ADDR0   192
#define AP_IP_ADDR1   168
#define AP_IP_ADDR2   43
#define AP_IP_ADDR3   1
   
/*NETMASK*/
#define AP_NETMASK_ADDR0   255
#define AP_NETMASK_ADDR1   255
#define AP_NETMASK_ADDR2   255
#define AP_NETMASK_ADDR3   0

/*Gateway Address*/
#define AP_GW_ADDR0   192
#define AP_GW_ADDR1   168
#define AP_GW_ADDR2   43
#define AP_GW_ADDR3   1  



#endif
