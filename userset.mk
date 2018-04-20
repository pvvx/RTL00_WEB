#=============================================
# User defined
#=============================================
SDK_PATH = USDK/
#GCC_PATH = d:/MCU/GNU_Tools_ARM_Embedded/6.2017-q1-update/bin/# + or set in PATH
#----------------------------------
# Set JTAG/SWD 
#----------------------------------
#FLASHER_TYPE = Jlink
#FLASHER_TYPE = stlink-v2
FLASHER_TYPE = cmsis-dap
FLASHER_SPEED = 3500
#----------------------------------
# Tools Path
#----------------------------------
ifneq ($(shell uname), Linux)
PYTHON ?= C:/Python27/python
OPENOCD ?= d:/MCU/OpenOCD/bin/openocd.exe
JLINK_PATH ?= D:/MCU/SEGGER/JLink_V630g/
else
PYTHON ?= python
OPENOCD ?= /mnt/d/MCU/OpenOCD/bin/openocd.exe
JLINK_PATH ?= /mnt/d/MCU/SEGGER/JLink_V630g/
endif
