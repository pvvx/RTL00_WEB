/*
 * power_tst.c
 *
 *  Created on: 04 апр. 2017 г.
 *      Author: PVV
 */

#include "rtl8195a.h"
#include "freertos_pmu.h"
#include "rtl8195a/rtl_libc.h"
/*------------------------------------------------------------------------------
 * power saving mode
 *----------------------------------------------------------------------------*/
void fATSP(int argc, char *argv[])
{
	if(argc > 2) {
		switch (argv[1][0]) {
		case 'a': // acquire
		{
			acquire_wakelock(atoi(argv[2]));
			break;
		}
		case 'r': // release
		{
			release_wakelock(atoi(argv[2]));
			break;
		}
		}
	}
	printf("WakeLock Status %d\n", pmu_get_wakelock_status());
}


MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_pwrs[] = {
		{"ATSP", 0, fATSP, "=<a,r>,<wakelock_status:1|2|4|8>: Power"}
};


