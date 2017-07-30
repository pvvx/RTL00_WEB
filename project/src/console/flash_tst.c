/*
 * flash_tst.c
 *
 *  Created on: 10/04/2017
 *      Author: pvvx
 */

#include <platform_opts.h>
#include "rtl8195a.h"
#include "flash_api.h"
#include "rtl8195a/rtl_libc.h"

extern void dump_bytes(uint32 addr, int size);

LOCAL void FlashDump(int argc, char *argv[]) {
		if (argc > 1) {
			int addr;
			sscanf(argv[1], "%x", &addr);
			int size = 16;
			if (argc > 2) {
				size = atoi(argv[2]);
				if (size <= 0 || size > 16384) {
					size = 16;
				};
			};
			flash_turnon();
			dump_bytes(addr + SPI_FLASH_BASE, size);
			SpicDisableRtl8195A();
		}
}

MON_RAM_TAB_SECTION COMMAND_TABLE console_flash_tst[] = {
		{"FLASHDB", 1, FlashDump, "=<faddr(HEX)>[,size]: Flash Dump"}
};




