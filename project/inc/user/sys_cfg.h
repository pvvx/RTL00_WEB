/*
 * sys_cfg.h
 *
 *  Created on: 17/04/17
 *      Author: pvvx
 */
#ifndef __SYS_CFG_H__
#define __SYS_CFG_H__
#include "user_config.h"
#define FEEP_ID_SYS_CFG 0x5359

struct sys_bits_config {
	uint16 hi_speed_enable		: 1;	//0  0x0000001 =1 Set CPU 160 MHz ...
	uint16 pin_clear_cfg_enable : 1;	//1  0x0000002 =1 Проверять ножку RX на сброс конфигурации WiFi
	uint16 debug_print_enable	: 1;	//2  0x0000004 =1 Вывод отладочной информации на GPIO2
	uint16 web_time_wait_delete	: 1;	//3  0x0000008 =1 Закрывать соединение и убивать pcb c TIME_WAIT
	uint16 netbios_ena			: 1;	//4  0x0000010 =1 включить NetBios
	uint16 sntp_ena				: 1;	//5  0x0000020 =1 включить SNTP
	uint16 cdns_ena				: 1;	//6  0x0000040 =1 включить CAPDNS
	uint16 tcp2uart_reopen		: 1;	//7  0x0000080 =1 открытие нового соединения tcp2uart ведет к закрытию старого соединения (в режиме tcp2uart = сервер)
	uint16 mdb_reopen			: 1;	//8  0x0000100 =1 открытие нового соединения modbus ведет к закрытию старого соединения (в режиме modbus = сервер)
};

#define SYS_CFG_HI_SPEED	0x0000001	// Set CPU 160 MHz ...
#define SYS_CFG_PIN_CLR_ENA	0x0000002	// Проверять ножку RX на сброс конфигурации WiFi
#define SYS_CFG_DEBUG_ENA 	0x0000004	// Вывод отладочной информации на GPIO2
#define SYS_CFG_TWPCB_DEL 	0x0000008	// Закрывать соединение и убивать pcb c TIME_WAIT
#define SYS_CFG_NETBIOS_ENA	0x0000010	// включить NetBios
#define SYS_CFG_SNTP_ENA	0x0000020	// включить SNTP
#define SYS_CFG_CDNS_ENA	0x0000040	// включить CAPDNS
#define SYS_CFG_T2U_REOPEN	0x0000080	// открытие нового соединения tcp2uart ведет к закрытию старого соединения (сервер)
#define SYS_CFG_MDB_REOPEN	0x0000100	// открытие нового соединения modbus ведет к закрытию старого соединения (сервер)


struct SystemCfg { // структура сохранения системных настроек в Flash
	union {
		struct sys_bits_config b;
		uint16 w;
	}cfg;
	uint16 tcp_client_twait;	// время (миллисек) до повтора соединения клиента
#ifdef USE_TCP2UART
	uint16 tcp2uart_port;	// номер порта TCP-UART (=0 - отключен)
	uint16 tcp2uart_twrec;	// время (сек) стартового ожидания приема/передачи первого пакета, до авто-закрытия соединения
	uint16 tcp2uart_twcls;	// время (сек) до авто-закрытия соединения после приема или передачи
#endif
#ifdef USE_WEB
	uint16 web_port;	// номер порта WEB (=0 - отключен)
	uint16 web_twrec;	// время (сек) стартового ожидания приема/передачи первого пакета, до авто-закрытия соединения
	uint16 web_twcls;	// время (сек) до авто-закрытия соединения после приема или передачи
#endif
#ifdef USE_MODBUS
	uint16 mdb_port;	// =0 - отключен
	uint16 mdb_twrec;	// время (сек) стартового ожидания приема/передачи первого пакета, до авто-закрытия соединения
	uint16 mdb_twcls;	// время (сек) до авто-закрытия соединения после приема или передачи
	uint8  mdb_id;	// номер устройства ESP8266 по шине modbus
#endif
} __attribute__((packed));


extern void sys_write_cfg(void);

extern struct SystemCfg syscfg;

#endif // #define __SYS_CFG_H__
