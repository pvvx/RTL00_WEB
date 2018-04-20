#---------------------------
# Set Paths & Tools
#---------------------------
SDK_PATH ?= ../RTL00_WEB/USDK/
#GCC_PATH ?= d:/MCU/GNU_Tools_ARM_Embedded/7.2017-q4-major/bin/# + or set in PATH
TOOLS_PATH ?= $(SDK_PATH)

FLASHER_TYPE ?= Jlink

ifneq ($(shell uname), Linux)
PYTHON ?= C:/Python27/python
OPENOCD ?= d:/MCU/OpenOCD/bin/openocd.exe
JLINK_PATH ?= D:/MCU/SEGGER/JLink_V630g/
else
PYTHON ?= python
OPENOCD ?= /mnt/d/MCU/OpenOCD/bin/openocd.exe
JLINK_PATH ?= /mnt/d/MCU/SEGGER/JLink_V630g/
endif

# Compilation tools
CROSS_COMPILE = $(GCC_PATH)arm-none-eabi-
AR = $(CROSS_COMPILE)ar
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
NM = $(CROSS_COMPILE)nm
LD = $(CROSS_COMPILE)gcc
GDB = $(CROSS_COMPILE)gdb
SIZE = $(CROSS_COMPILE)size
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# TARGET dirs
TARGET ?= build
OBJ_DIR ?= $(TARGET)/obj
BIN_DIR ?= $(TARGET)/bin
ELFFILE ?= $(OBJ_DIR)/$(TARGET).axf


