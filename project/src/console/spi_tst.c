/*
 * spi_test.c
 */
#include <platform_opts.h>
#include "rtl8195a.h"
#include "spi_api.h"
#include "spi_ex_api.h"
#include "rtl8195a/rtl_libc.h"

#define SPI0_MOSI  PC_2
#define SPI0_MISO  PC_3
#define SPI0_SCLK  PC_1
#define SPI0_CS    PC_0

spi_t spi_master;

LOCAL void show_reg_spi(int i) {
	rtl_printf("Regs SPI:\n");
	for(int x = 0; x < 64 ; x += 4) {
		rtl_printf("0x%08x ", HAL_SSI_READ32(i, x));
		if((x & 0x0F) == 0x0C) rtl_printf("\n");
	}
}


LOCAL void fATSSI(int argc, char *argv[])
{
    int len = 128;
    int count = 32;
    int clk = 1000000;
    int ssn = 0;
    if(argc > 1) {
        len = atoi(argv[1]);
        if(len > 32768 || len <= 0) {
        	len = 128;
            error_printf("%s: len = %u!\n", __func__, len);
        };
    };
    if(argc > 2) {
        count = atoi(argv[2]);
        if(count > 10000 || count <= 0) {
        	count = 32;
            error_printf("%s: count = %u!\n", __func__, count);
        };
    };
    if(argc > 3) {
        clk = atoi(argv[3]);
        if(clk <= 0) {
        	clk = 1000000;
            error_printf("%s: clk = %u!\n", __func__, clk);
        };
    };
    if(argc > 4) {
        ssn = atoi(argv[4]);
        if(ssn > 7 || ssn < 0) {
        	ssn = 0;
            error_printf("%s: ssn = %u!\n", __func__, ssn);
        };
    };
    char* buff = pvPortMalloc(len);
    if(buff) {
    	spi_init(&spi_master, SPI0_MOSI, SPI0_MISO, SPI0_SCLK, SPI0_CS); // CS заданный тут нигде не используется
        spi_format(&spi_master, 16, 3, 0);
        spi_frequency(&spi_master, clk);
       	spi_slave_select(&spi_master, ssn); // выбор CS
       	for(int i = 0; i < len; i++) buff[i] = (char)i;
        while(count--) {
            spi_master_write_stream(&spi_master, buff, len);
            while(spi_busy(&spi_master));
            rtl_printf("Master write: %d\n", count);
        };
//		show_reg_spi(spi_master.spi_adp.Index);
        spi_free(&spi_master);
        free(buff);
    }
    else {
        error_printf("%s: error malloc!\n", __func__);
    };
}

MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_spitst[] = {
		{"ATSSI", 0, fATSSI, "[len[,count[,clk[,ssn]]]]: Spi test"}
};
