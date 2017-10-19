/*
 * Automatically generated by make menuconfig: don't edit
 */
#define AUTOCONF_INCLUDED

#define RTL8710AF
#define RTL8711AM
/* Image1 on project */
#define PRESENT_IMAGE1
/* Image2 on project */
#define PRESENT_IMAGE2
/*
 * Target Platform Selection
 */
#define CONFIG_USE_TCM_HEAP 1
#define configUSE_STACK_TCM_HEAP 5 // min priority use tcm ?

#define CONFIG_WITHOUT_MONITOR 1

#undef  CONFIG_RTL8195A 
#define CONFIG_RTL8195A 1
#undef  CONFIG_FPGA
#undef  CONFIG_RTL_SIM
#undef  CONFIG_POST_SIM
/*
 * < Mass Production Option 
 */
#undef  CONFIG_MP
#undef  CONFIG_CP
#undef  CONFIG_FT
#define RTL8195A 1
/* 0 - 166666666 Hz, 1 - 83333333  Hz, 2 - 41666666 Hz, 3 - 20833333 Hz, 4 - 10416666 Hz, 5 - 4000000? Hz,
   6 - 200000000 Hz, 7 - 100000000 Hz, 8 - 50000000 Hz, 9 - 25000000 Hz, 10 - 12500000 Hz, 11 - 4000000? Hz */
#define CONFIG_CPU_CLK 1
//166.6MHZ - RUN/IDLE/SLP ~63/21/6.4 mA
//83.3MHZ  - RUN/IDLE/SLP ~55/15/6.4 mA
//41.6MHZ  - RUN/IDLE ~51/11 mA
//20.8MHZ  - RUN/IDLE ~49/9.5 mA
//4MHZ     - IDLE ~8 mA
#undef  CONFIG_FPGA_CLK
#define CONFIG_SDR_CLK 1
#define CONFIG_SDR_100MHZ 1
#undef  CONFIG_SDR_50MHZ 
#undef  CONFIG_SDR_25MHZ
#undef  CONFIG_SDR_12_5MHZ
#define SDR_CLOCK_SEL_VALUE (0)
#define CONFIG_BOOT_PROCEDURE 1
#define CONFIG_IMAGE_PAGE_LOAD 1
#undef  CONFIG_IMAGE_AUTO_LOAD
#undef  CONFIG_IMAGE_PAGE_LOAD
//#define CONFIG_IMAGE_AUTO_LOAD 1
//#define CONFIG_BOOT_TO_UPGRADE_IMG2 1
#undef  CONFIG_PERI_UPDATE_IMG
#define CONFIG_BOOT_FROM_JTAG 1
#undef  CONFIG_ALIGNMENT_EXCEPTION_ENABLE
#define CONFIG_KERNEL 1
#define PLATFORM_FREERTOS 1
#undef  PLATFORM_UCOSII
#undef  PLATFORM_ECOS
#undef  CONFIG_TASK_SCHEDUL_DIS
#define TASK_SCHEDULER_DISABLED (0)
#define CONFIG_NORMALL_MODE 1
#undef  CONFIG_MEMORY_VERIFY_MODE
#define CONFIG_TIMER_EN 1
#define CONFIG_TIMER_NORMAL 1
#undef  CONFIG_TIMER_TEST
#define CONFIG_TIMER_MODULE 1
#define CONFIG_WDG 1
#undef  CONFIG_WDG_NON
#define CONFIG_WDG_NORMAL 1
#define CONFIG_WDG_ON_IDLE 10 // add pvvx: wdt on 10 s -> main.c + tasks.c
#define CONFIG_GDMA_EN 1
#define CONFIG_GDMA_NORMAL 1
#undef  CONFIG_GDMA_TEST
#define CONFIG_GDMA_MODULE 1
#define CONFIG_WIFI_EN 1
#define CONFIG_WIFI_NORMAL 1
#undef  CONFIG_WIFI_TEST
#define CONFIG_WIFI_MODULE 1
#define CONFIG_GPIO_EN 1
#define CONFIG_GPIO_NORMAL 1
#undef  CONFIG_GPIO_TEST
#define CONFIG_GPIO_MODULE 1
#if defined(CONFIG_INIC) || (CONFIG_SDIOD)
#define CONFIG_SDIO_DEVICE_EN 1
#define CONFIG_SDIO_DEVICE_NORMAL 1
#undef  CONFIG_SDIO_DEVICE_TEST
#define CONFIG_SDIO_DEVICE_MODULE 1
#else
#undef  CONFIG_SDIO_DEVICE_EN
#endif
#define CONFIG_SDIO_HOST_EN   1
//#define CONFIG_USB_EN 1
#undef  CONFIG_USB_NORMAL
#define CONFIG_USB_TEST 1
#define CONFIG_USB_MODULE 1
#define CONFIG_USB_VERIFY 1
#undef CONFIG_USB_ROM_LIB
//#define CONFIG_USB_DBGINFO_EN 1
#if defined(CONFIG_INIC) || (CONFIG_USBD)
#define DWC_DEVICE_ONLY	1
#else
#define  DWC_HOST_ONLY 1
#define CONFIG_USB_HOST_ONLY 1
#endif
#define CONFIG_SPI_COM_EN 1
#define CONFIG_SPI_COM_NORMAL 1
#undef  CONFIG_SPI_COM_TEST
#define CONFIG_SPI_COM_MODULE 1
#define CONFIG_UART_EN 1
#define CONFIG_UART_NORMAL 1
#undef  CONFIG_UART_TEST
#define CONFIG_UART_MODULE 1
#define CONFIG_I2C_EN 1
#define CONFIG_I2C_NORMAL 1
#undef  CONFIG_I2C_TEST
#define CONFIG_I2C_MODULE 1
#undef  CONFIG_DEBUG_LOG_I2C_HAL
#undef  CONFIG_PCM_EN
#define CONFIG_I2S_EN 1
#define CONFIG_I2S_NORMAL 1
#undef  CONFIG_I2S_TEST
#define CONFIG_I2S_MODULE 1
#undef  CONFIG_DEBUG_LOG_I2S_HAL
#define CONFIG_NFC_EN 1
#define CONFIG_NFC_NORMAL 1
#undef  CONFIG_NFC_TEST 
#define CONFIG_NFC_MODULE 1
#define CONFIG_SOC_PS_EN 1
#define CONFIG_SOC_PS_NORMAL 1
#undef  CONFIG_SOC_PS_TEST
#define CONFIG_SOC_PS_MODULE 1 // hal_soc_ps_monitor.c
//#define CONFIG_SOC_PS_VERIFY 1 // hal_soc_ps_monitor.c
#define CONFIG_CRYPTO_EN 1
#define CONFIG_CRYPTO_NORMAL 1
#undef  CONFIG_CRYPTO_TEST
#define CONFIG_CRYPTO_MODULE 1
#define CONFIG_CRYPTO_STARTUP 1
#define CONFIG_MII_EN	0 //1
#define CONFIG_PWM_EN 1
#define CONFIG_PWM_NORMAL 1
#undef  CONFIG_PWM_TEST
#define CONFIG_PWM_MODULE 1
#define CONFIG_EFUSE_EN 1 // common/mbed/targets/hal/rtl8195a/efuse_api.c
#define CONFIG_EFUSE_NORMAL 1
#undef  CONFIG_EFUSE_TEST
#define CONFIG_EFUSE_MODULE 1
#if defined(RTL8711AM) || defined(USE_SDRAM)
#define CONFIG_SDR_EN 1
#endif
#define CONFIG_SDR_NORMAL 1
#undef  CONFIG_SDR_TEST
#define CONFIG_SDR_MODULE 1
#define CONFIG_SPIC_EN 1
#define CONFIG_SPIC_NORMAL 1
#undef  CONFIG_SPIC_TEST
#define CONFIG_SPIC_MODULE 1
#define CONFIG_ADC_EN 1
#define CONFIG_DAC_EN 1
#define CONFIG_NOR_FLASH 1
#undef  CONFIG_SPI_FLASH
#undef  CONFIG_NAND_FLASH
#undef  CONFIG_NONE_FLASH
#undef  CONFIG_BTBX_EN

// add pvvx
#define CONFIG_LOG_UART_EN 1

/*
 * < Engineer Mode Config 
 */
#undef  CONFIG_JTAG
#undef  CONFIG_COMPILE_FLASH_DOWNLOAD_CODE
#undef  CONIFG_COMPILE_EXTERNAL_SRAM_CALIBRATE
#undef  CONFIG_CMSIS_MATH_LIB_EN

/*
 * < Application Config 
 */
#define CONFIG_NETWORK 1
#define CONFIG_RTLIB_EN 1
#define CONFIG_RTLIB_NORMAL 1
#undef  CONFIG_RTLIB_TEST
#define CONFIG_RTLIB_MODULE 1

/*
 * < System Debug Message Config 
 */
#define CONFIG_UART_LOG_HISTORY 1
#undef  CONFIG_CONSOLE_NORMALL_MODE
#define CONFIG_CONSOLE_VERIFY_MODE 1

/* CONFIG_DEBUG_LOG:
=0 Off all diag/debug msg,
=1 Only errors,
=2 errors + warning, (default)
=3 errors + warning + info,
=4 errors + warning + info + debug,
=5 full */
#define CONFIG_DEBUG_LOG 2
#if CONFIG_DEBUG_LOG > 0
//#define CONFIG_DEBUG_ERR_MSG 1
#define CONFIG_DEBUG_LOG_ADC_HAL 1
#define CONFIG_DEBUG_LOG_I2S_HAL 1
//#undef  CONFIG_DEBUG_WARN_MSG
//#undef  CONFIG_DEBUG_INFO_MSG
#endif // CONFIG_DEBUG_LOG
/*
 * < SDK Option Config 
 */
#undef  CONFIG_MBED_ENABLED
#undef  CONFIG_APP_DEMO

/*
 * < Select Chip Version 
 */
#undef  CONFIG_CHIP_A_CUT
#define CONFIG_CHIP_B_CUT 1
#undef  CONFIG_CHIP_C_CUT
#undef  CONFIG_CHIP_E_CUT

/*
 * < Select toolchain 
 */
#undef  CONFIG_TOOLCHAIN_ASDK
#undef  CONFIG_TOOLCHAIN_ARM_GCC

/*
 * < Build Option 
 */
#define CONFIG_LINK_ROM_LIB 1
#undef  CONFIG_LINK_ROM_SYMB
#undef  CONFIG_NORMAL_BUILD
#undef  CONFIG_RELEASE_BUILD
#undef  CONFIG_RELEASE_BUILD_LIBRARIES
#undef  CONFIG_LIB_BUILD_RAM
#define CONFIG_RELEASE_BUILD_RAM_ALL 1
#undef  CONFIG_IMAGE_ALL
#define CONFIG_IMAGE_SEPARATE 1

#if CONFIG_CPU_CLK < 6
#define CPU_CLOCK_SEL_DIV5_3		0
#define CPU_CLOCK_SEL_VALUE CONFIG_CPU_CLK
#else
#define CPU_CLOCK_SEL_DIV5_3		1
#define CPU_CLOCK_SEL_VALUE (CONFIG_CPU_CLK-6)
#endif

#if CPU_CLOCK_SEL_DIV5_3
#define PLATFORM_CLOCK (200000000ul>>CPU_CLOCK_SEL_VALUE)
#else
#define PLATFORM_CLOCK (((200000000ul*5ul)/6ul)>>CPU_CLOCK_SEL_VALUE)
#endif

