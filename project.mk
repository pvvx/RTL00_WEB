#=============================================
# SDK CONFIG
#=============================================
#WEB_INA219_DRV = 1
WEB_ADC_DRV = 1
#USE_SDCARD = 1
#USE_AT = 1
#USE_FATFS = 1
#USE_SDIOH = 1
#USE_POLARSSL = 1
#USE_P2P_WPS = 1
#USE_GCC_LIB = 1
USE_MBED = 1

ifndef USE_AT
USE_NEWCONSOLE = 1
USE_WIFI_API = 1
endif

ifdef USE_SDCARD
USE_FATFS = 1
USE_SDIOH = 1
endif

#RTOSDIR=freertos_v8.1.2
RTOSDIR=freertos_v9.0.0
LWIPDIR=lwip_v1.4.1

include $(SDK_PATH)sdkset.mk
#CFLAGS += -DDEFAULT_BAUDRATE=1562500
CFLAGS += -DLOGUART_STACK_SIZE=1024
#=============================================
# User Files
#=============================================
#user main
ADD_SRC_C += project/src/user/main.c
ADD_SRC_C += project/src/user/user_start.c
# components
ADD_SRC_C += project/src/console/atcmd_user.c
ADD_SRC_C += project/src/console/wifi_console.c
ADD_SRC_C += project/src/console/wlan_tst.c
#ADD_SRC_C += project/src/console/pwm_tst.c

ifdef USE_SDCARD
ADD_SRC_C += project/src/console/sd_fat.c
endif

ifdef WEB_INA219_DRV
ADD_SRC_C += project/src/driver/i2c_drv.c
ADD_SRC_C += project/src/ina219/ina219drv.c
CFLAGS += -DWEB_INA219_DRV=1
endif

ifdef WEB_ADC_DRV
ADD_SRC_C += project/src/driver/adc_drv.c
ADD_SRC_C += project/src/adc_ws/adc_ws.c
CFLAGS += -DWEB_ADC_DRV=1
endif

#Web-������
INCLUDES += project/inc/web
ADD_SRC_C += project/src/tcpsrv/tcp_srv_conn.c
ADD_SRC_C += project/src/webfs/webfs.c
ADD_SRC_C += project/src/web/web_srv.c
ADD_SRC_C += project/src/web/web_utils.c
ADD_SRC_C += project/src/web/web_websocket.c
ADD_SRC_C += project/src/web/websock.c
ADD_SRC_C += project/src/web/web_int_callbacks.c
ADD_SRC_C += project/src/web/web_int_vars.c
ADD_SRC_C += project/src/web/web_auth.c



