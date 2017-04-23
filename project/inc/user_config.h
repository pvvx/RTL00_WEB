#ifndef _user_config_h_
#define _user_config_h_

//#include "sdk/sdk_config.h"

#define SYS_VERSION "1.0.0"
#define SDK_VERSION "3.5.3"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Конфигурация для проекта MODBUS-RS-485
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define USE_WEB		80 	// включить в трансялцию порт Web, если =0 - по умолчанию выключен
#define WEBSOCKET_ENA 1 // включить WEBSOCKET
#define USE_SNTP	1 	// включить в трансялцию драйвер SNTP, если =0 - по умолчанию выключен, = 1 - по умолчанию включен.
#define USE_NETBIOS	1 	// включить в трансялцию драйвер NETBIOS, если =0 - по умолчанию выключен.

//#define USE_CPU_SPEED  166 // установить частоту CPU (по умолчанию 83)
/*


#define USE_RS485DRV	// использовать RS-485 драйвер
#define MDB_RS485_MASTER // Modbus RTU RS-485 master & slave
#define USE_MODBUS	502 // включить в трансялцию Modbus TCP, если =0 - по умолчанию выключен
#define MDB_ID_ESP 50 	// номер устройства RTL на шине modbus

//#define USE_CAPTDNS	0	// включить в трансялцию DNS отвечающий на всё запросы клиента при соединении к AP модуля
						// указанием на данный WebHttp (http://aesp8266/), если =0 - по умолчанию выключен

*/

#endif // _user_config_h_


