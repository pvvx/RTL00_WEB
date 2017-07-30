/*
 * sd_fat.c
 *
 *  Created on: 17 дек. 2016 г.
 *      Author: PVV
 */
#include "rtl8195a.h"

#ifdef CONFIG_SDIO_HOST_EN

#include "rtl8195a_sdio_host.h"
#include "hal_sdio_host.h"
#include "sd.h"
#include "sdio_host.h"
#include "pinmap.h"
#include "sdcard.h"
//#include "ff.h"
//#include "fatfs_ext/inc/ff_driver.h"
#include "rtl8195a/os.h"
#include "at_cmd/log_service.h"


//extern ll_diskio_drv SD_disk_Driver;

char *logical_drv = "0:/";

#define START_CHAR_DISK '0'

typedef struct _msftm_td {
	unsigned short day :5;
	unsigned short month :4;
	unsigned short year :7;
} msftm_d;

typedef struct _msftm_tt {
	unsigned short sec :5;
	unsigned short min :6;
	unsigned short hour :5;
} msftm_t;

typedef union {
	unsigned short w;
	msftm_d d;
} msftm_td;

typedef union {
	unsigned short w;
	msftm_t t;
} msftm_tt;
uint8_t * month[12] = { "Jan" , "Feb" , "Mar" , "Apr" , "May" , "Jun" , "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
/*
 * MS files times
 */
int ms_ftime(unsigned short td, unsigned short tt, struct os_tm *tm) {
	msftm_td d;
	msftm_tt t;
	d.w = td;
	t.w = tt;
	tm->year = d.d.year + 1980;
	tm->month = d.d.month;
	tm->day = d.d.day;
	tm->hour = t.t.hour;
	tm->min = t.t.min;
	tm->sec = t.t.sec << 1;
	if (tm->month > 11 || tm->hour > 23 || tm->min > 59 || tm->sec > 59) {
		return 0;
	}
	return 1;
}
/*
 * MS files attr
 */
uint8_t * ms_fattr(uint8_t *s, uint8_t attr) {
	memset(s, '-', 7);
	if (attr & AM_ARC)
		s[0] = 'a';
	if (attr & AM_DIR)
		s[1] = 'd';
	if (attr & AM_LFN)
		s[2] = 'l';
	if (attr & AM_VOL)
		s[3] = 'v';
	if (attr & AM_SYS)
		s[4] = 's';
	if (attr & AM_HID)
		s[5] = 'h';
	if (attr & AM_RDO)
		s[6] = 'r';
	s[7] = 0;
	return s;
}

/*
 * Linux files attr
 */
uint8_t * ux_fattr(uint8_t *s, uint8_t attr) {
	memset(s, '-', 10);
	if (attr & AM_DIR) {
		s[0] = 'd';
//   		s[3] = 'x';
//   		s[6] = 'x';
//   		s[9] = 'x';
	}
	if (!(attr & AM_VOL)) {
//   	 {
		s[1] = 'r';
		s[4] = 'r';
		s[7] = 'r';
		if (!(attr & AM_RDO)) {
			s[2] = 'w';
			s[5] = 'w';
			s[8] = 'w';
		}
	}
//   	if(attr & AM_VOL) s[3] = 'x';
	if (!(attr & AM_SYS))
		s[3] = 'x';
	if (!(attr & AM_HID))
		s[6] = 'x';
	if (!(attr & AM_ARC))
		s[9] = 'x';
	s[10] = 0;
	return s;
}

FATFS * sd_mount(void) {
	FATFS * m_fs = NULL;
	// Инициализация I/O SDIOH
	if (HalGetChipId() != CHIP_ID_8195AM) {
		GPIOState[0] &= ~((1 << 8) - 1);
		HAL_GPIO_PullCtrl(PA_0, PullUp); 	// D2
		HAL_GPIO_PullCtrl(PA_1, PullUp); 	// D3
		HAL_GPIO_PullCtrl(PA_2, PullUp); 	// CMD
		HAL_GPIO_PullCtrl(PA_3, PullNone); 	// CLK
		HAL_GPIO_PullCtrl(PA_4, PullUp); 	// D0
		HAL_GPIO_PullCtrl(PA_5, PullUp); 	// D1
		HAL_GPIO_PullCtrl(PA_6, PullDown); 	// SD card removed
		HAL_GPIO_PullCtrl(PA_7, PullDown); 	// WP enable
		vTaskDelay(1);
	};
	s8 ret = SD_Init();
	if (ret < SD_NODISK) { // sdio_sd_init();
		if (sdio_sd_status() >= 0 && sdio_status == SDIO_SD_OK) {
			sdio_sd_getProtection();
#if CONFIG_DEBUG_LOG > 2
			// чтение информации о SD карте
			printf("\nSD CSD: ");
			for (int i = 0; i < 16; i++)
				printf("%02x", SdioHostAdapter.Csd[i]);
			u32 i = sdio_sd_getCapacity();
			printf("\nSD Capacity: %d sectors (%d GB | %d MB | %d KB)\n", i,
					i >> 21, i >> 11, i >> 1);
#endif
			m_fs = (FATFS *) malloc(sizeof(FATFS));
			if (m_fs != NULL) {
				memset(m_fs, 0, sizeof(FATFS));
				int drv_num = FATFS_RegisterDiskDriver(&SD_disk_Driver);
				if (drv_num >= 0) {
					*logical_drv = (char) drv_num + START_CHAR_DISK;
					if (f_mount(m_fs, logical_drv, 1) == FR_OK) {
						m_fs->drv = (BYTE) drv_num;
#if CONFIG_DEBUG_LOG > 3
						printf("SD disk:%c mounted\n", logical_drv[0], m_fs);
#endif
					} else {
#if CONFIG_DEBUG_LOG > 2
						printf("SD disk mount failed!\n");
#endif
						FATFS_UnRegisterDiskDriver(drv_num);
						free(m_fs);
						m_fs = NULL;
					}
				} else {
#if CONFIG_DEBUG_LOG > 2
					printf("SD Register Disk Driver failed!\n");
#endif
					free(m_fs);
					m_fs = NULL;
				}
			} else {
#if CONFIG_DEBUG_LOG > 2
				printf("Malloc failed!\n");
#endif
			}
		} else {
#if CONFIG_DEBUG_LOG > 2
			printf("SD Error (%d) get status!\n", sdio_status);
#endif
		}
	} else {
#if CONFIG_DEBUG_LOG > 2
		printf("SD Init Error (%d)!\n", ret);
#endif
	}
	return m_fs;
}

void sd_unmount(FATFS *m_fs) {
	if (m_fs != NULL) {
#if CONFIG_DEBUG_LOG > 2
		printf("unmount (%d)\n", f_mount(NULL, logical_drv, 1));
#else
		f_mount(NULL, logical_drv, 1);
#endif
		FATFS_UnRegisterDiskDriver(m_fs->drv);
		free(m_fs);
	}
	pin_mode(PA_6, PullUp); // SD card removed
	pin_mode(PA_7, PullUp); // WP enable
	vTaskDelay(1);
	SD_DeInit(); // sdio_sd_deinit();
}

void read_file_test(char* file_name) {
    FIL *f = malloc(sizeof(FIL));
    if (f_open(f, file_name, FA_READ) == FR_OK) {
        char * buf = malloc(2048);
        unsigned int bytesread =0;
        unsigned int totalread=0;
        do {
            if (f_read(f, buf, 2048, &bytesread) != FR_OK) {
                totalread += bytesread;
                printf("Read error!");
                break;
            }
            totalread += bytesread;
        }
        while (bytesread !=0);
        free(buf);
        f_close(f);
    } else {
    	printf("No open file!");
    }
    free(f);
}


LOCAL void fATHF(int argc, char *argv[]) {
	uint8 buf[512];
	FATFS * fs = sd_mount();
	if (fs != NULL) {
		u8 * pbuf = (u8 *) malloc(512); // char *lfn = malloc (_MAX_LFN + 1);
		if (pbuf != NULL) {
			DIR dir;
			FILINFO fno;
			struct os_tm tm;
			fno.lfname = (TCHAR*) pbuf;
			fno.lfsize = 512;
			u8 * sdir;
			if(argc > 1
				&& argv[1] != NULL
				&& (sdir = (u8 *) malloc(strlen(argv[1]) + 4)) != NULL )
					strcpy(strcpy(sdir, logical_drv) + 3, argv[1]);
			else sdir = logical_drv;
			if (f_opendir(&dir, sdir) == FR_OK) {
				while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
					if ((fno.fattrib & AM_VOL)==0 && fno.fsize > 0) {
						if (*fno.lfname) {
							strcpy(strcpy(buf, logical_drv) + 3, fno.lfname);
						} else {
							strcpy(strcpy(buf, logical_drv) + 3, fno.fname);
						}
					    TickType_t t1 = xTaskGetTickCount();
						read_file_test(buf);
						t1 = xTaskGetTickCount() - t1;
						if(t1 == 0) t1 = 1;
						printf("%u kbytes/sec\t%s\n", t1, buf);
					}
				}
			} else
				printf("FATFS: Open dir fail!\n");
			free(pbuf);
			if(sdir != logical_drv) free(sdir);
		}
	}
	sd_unmount(fs);

}
/*	Test SD  */
LOCAL void fATHS(int argc, char *argv[]) {
	//	HalPinCtrlRtl8195A(UART0,0,0);
	//	HalPinCtrlRtl8195A(UART1,0,0);
#if CONFIG_DEBUG_LOG > 4
	ConfigDebugErr = -1;
	ConfigDebugInfo = -1;
	ConfigDebugWarn = -1;
	CfgSysDebugErr = -1;
	CfgSysDebugInfo = -1;
	CfgSysDebugWarn = -1;
#endif

#if	DEBUG_AT_USER_LEVEL > 3
	printf("ATHS: dir\n");
#endif
	FATFS * fs = sd_mount();
	if (fs != NULL) {
		u8 * pbuf = (u8 *) malloc(512); // char *lfn = malloc (_MAX_LFN + 1);
		if (pbuf != NULL) {
			DIR dir;
			FILINFO fno;
			struct os_tm tm;
			fno.lfname = (TCHAR*) pbuf;
			fno.lfsize = 512;
			u8 * sdir;
			if(argc > 1
				&& argv[1] != NULL
				&& (sdir = (u8 *) malloc(strlen(argv[1]) + 4)) != NULL )
					strcpy(strcpy(sdir, logical_drv) + 3, argv[1]);
			else sdir = logical_drv;
			printf("\ndir %s\n", sdir);
			if (f_opendir(&dir, sdir) == FR_OK) {
				while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
					int srtlen = 0;
					{
						u8 buf[12];
						srtlen += printf(ux_fattr(buf, fno.fattrib));
					}
					srtlen += printf(" 0");
					if (fno.fattrib & AM_VOL)
						printf(" volume");
					else if (fno.fattrib & AM_SYS)
						printf(" system");
					else
						printf(" none  ");
					srtlen += 7;
					if (fno.fattrib & AM_HID)
						printf(" hidden");
					else
						printf(" none  ");
					srtlen += 7;
					srtlen += printf(" %d", fno.fsize);
					while (srtlen < 37)
						srtlen += printf(" ");
					ms_ftime(fno.fdate, fno.ftime, &tm);
					srtlen += printf(" %04d-%02d-%02d %02d:%02d:%02d ", tm.year,
							tm.month, tm.day, tm.hour, tm.min, tm.sec);
					if (*fno.lfname) {
						srtlen += printf(fno.lfname);
						//fnlen = fno.lfsize;
					} else {
						srtlen += printf(fno.fname);
						//fnlen = fno.fsize;
					}
					printf("\n");
				};
				printf("\n");
				f_closedir(&dir);
			} else
				printf("FATFS: Open dir fail!\n");
			free(pbuf);
			if(sdir != logical_drv) free(sdir);
		}
	}
	sd_unmount(fs);

	/*
	 } else
	 printf("FATFS: Malloc fail!\n");
	 f_mount(NULL, logical_drv, 1);
	 } else
	 printf("FATFS mount logical drive fail!\n");
	 FATFS_UnRegisterDiskDriver(drv_num);
	 } else
	 printf("Register disk driver to FATFS fail.\n");
	 free(m_fs);
	 } else
	 printf("FATFS: Malloc fail!\n");
	 } else
	 printf("SD Init fail!\n");
	pin_mode(PA_6, PullUp); // SD card removed
	pin_mode(PA_7, PullUp); // WP enable
	vTaskDelay(5);

	SD_DeInit(); // sdio_sd_deinit();
	 */
}


MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_dscard[] = {
		{"ATHS", 0, fATHS, "=[dir]: SD test"},
		{"ATHF", 0, fATHF, "=[dir]: SD file read"}
};

#endif // CONFIG_SDIO_HOST_EN
