# RTL8710 Flasher v0.0.alfa
# pvvx 21.09.2016
include userset.mk
include $(SDK_PATH)paths.mk
include project.mk
#---------------------------
# Default
#---------------------------
FLASHER_TYPE ?= Jlink
FLASHER_PATH ?= $(SDK_PATH)flasher/
FLASHER_SPEED ?= 1000 # Start speed 1000 kHz
#---------------------------
# Make bunary tools
IMAGETOOL ?= $(PYTHON) $(SDK_PATH)rtlaimage.py
#---------------------------
# openocd tools
OPENOCD ?= d:/MCU/OpenOCD/bin/openocd.exe
#---------------------------
# jlink tools
JLINK_PATH ?= D:/MCU/SEGGER/JLink_V612i/
JLINK_EXE ?= JLink.exe
JLINK_GDB ?= JLinkGDBServer.exe

NMAPFILE = $(OBJ_DIR)/$(TARGET).nmap

RAM1_IMAGE ?= $(BIN_DIR)/ram_1.bin
RAM1P_IMAGE ?= $(BIN_DIR)/ram_1.p.bin

RAM2_IMAGE ?= $(BIN_DIR)/ram_2.bin
RAM2P_IMAGE ?= $(BIN_DIR)/ram_2.p.bin

RAM3_IMAGE ?= $(BIN_DIR)/sdram.bin
RAM3P_IMAGE ?= $(BIN_DIR)/sdram.p.bin

FLASH_IMAGE = $(BIN_DIR)/ram_all.bin
OTA_IMAGE = $(BIN_DIR)/ota.bin

#all: FLASH_IMAGE = $(BIN_DIR)/ram_all.bin
#all: OTA_IMAGE = $(BIN_DIR)/ota.bin
mp: FLASH_IMAGE = $(BIN_DIR)/ram_all_mp.bin
mp: OTA_IMAGE = $(BIN_DIR)/ota_mp.bin


.PHONY: genbin flashburn reset test readfullflash flash_boot flash_webfs flash_OTA flash_images runram runsdram
.NOTPARALLEL: all mp genbin flashburn reset test readfullflash _endgenbin flash_webfs flash_OTA flash_images

all: $(ELFFILE) $(FLASH_IMAGE) _endgenbin
mp: $(ELFFILE) $(FLASH_IMAGE) _endgenbin

genbin: $(ELFFILE) $(FLASH_IMAGE) _endgenbin

$(ELFFILE):
	$(error Falsher: file $@ not found)

$(NMAPFILE): $(ELFFILE)
	@echo "==========================================================="
	@echo "Build names map file"
	@echo $@
	@$(NM) $< | sort > $@
#	@echo "==========================================================="

$(FLASH_IMAGE):$(ELFFILE)
	@echo "==========================================================="
	$(IMAGETOOL) -a -r -o $(BIN_DIR)/ $(ELFFILE) 
 
_endgenbin:
	@echo "-----------------------------------------------------------"
	@echo "Image ($(OTA_IMAGE)) size $(shell printf '%d\n' $$(( $$(stat --printf="%s" $(OTA_IMAGE)) )) ) bytes"
	@echo "Image ($(FLASH_IMAGE)) size $(shell printf '%d\n' $$(( $$(stat --printf="%s" $(FLASH_IMAGE)) )) ) bytes"
	@echo "==========================================================="  

ifeq ($(FLASHER_TYPE), Jlink)

reset:
	@$(JLINK_PATH)$(JLINK_EXE) -Device CORTEX-M3 -If SWD -Speed $(FLASHER_SPEED) $(FLASHER_PATH)RTL_Reset.JLinkScript

runram:
	@$(JLINK_PATH)$(JLINK_EXE) -Device CORTEX-M3 -If SWD -Speed $(FLASHER_SPEED) $(FLASHER_PATH)RTL_RunRAM.JLinkScript

runsdram:
	@$(JLINK_PATH)$(JLINK_EXE) -Device CORTEX-M3 -If SWD -Speed $(FLASHER_SPEED) $(FLASHER_PATH)RTL_RunRAM_SDR.JLinkScript

readfullflash:
	@$(JLINK_PATH)$(JLINK_EXE) -Device CORTEX-M3 -If SWD -Speed $(FLASHER_SPEED) $(FLASHER_PATH)RTL_FFlash.JLinkScript


flash_boot:
	@echo define call1>$(FLASHER_PATH)file_info.jlink
	@echo set '$$'ImageSize = $(shell printf '0x%X\n' $$(stat --printf="%s" $(RAM1P_IMAGE))>>$(FLASHER_PATH)file_info.jlink
	@echo set '$$'ImageAddr = 0x000000>>$(FLASHER_PATH)file_info.jlink
	@echo end>>$(FLASHER_PATH)file_info.jlink
	@echo define call2>>$(FLASHER_PATH)file_info.jlink
	@echo FlasherWrite $(RAM2P_IMAGE) '$$'ImageAddr '$$'ImageSize>>$(FLASHER_PATH)file_info.jlink
	@echo end>>$(FLASHER_PATH)file_info.jlink
	@cmd /K start $(JLINK_PATH)$(JLINK_GDBSRV) -device Cortex-M3 -if SWD -ir -endian little -speed $(FLASHER_SPEED) 
	@$(GDB) -x $(FLASHER_PATH)gdb_wrfile.jlink

flash_images:
	@cmd /K start $(JLINK_PATH)$(JLINK_GDBSRV) -device Cortex-M3 -if SWD -ir -endian little -speed $(FLASHER_SPEED)
	@$(GDB) -x $(FLASHER_PATH)gdb_images.jlink

flash_OTA:
	@cmd /K start $(JLINK_PATH)$(JLINK_GDBSRV) -device Cortex-M3 -if SWD -ir -endian little -speed $(FLASHER_SPEED)
	@$(GDB) -x $(FLASHER_PATH)gdb_ota.jlink

flash_webfs:
	@echo define call1>$(FLASHER_PATH)file_info.jlink
	@echo set '$$'ImageSize = $(shell printf '0x%X\n' $$(stat --printf="%s" $(BIN_DIR)/WEBFiles.bin))>>$(FLASHER_PATH)file_info.jlink
	@echo set '$$'ImageAddr = 0x0D0000>>$(FLASHER_PATH)file_info.jlink
	@echo end>>$(FLASHER_PATH)file_info.jlink
	@echo define call2>>$(FLASHER_PATH)file_info.jlink
	@echo FlasherWrite $(BIN_DIR)/WEBFiles.bin '$$'ImageAddr '$$'ImageSize>>$(FLASHER_PATH)file_info.jlink
	@echo end>>$(FLASHER_PATH)file_info.jlink
	@cmd /K start $(JLINK_PATH)$(JLINK_GDBSRV) -device Cortex-M3 -if SWD -ir -endian little -speed $(FLASHER_SPEED)
	@$(GDB) -x $(FLASHER_PATH)gdb_wrfile.jlink

flash_espfs:
	@echo define call1>$(FLASHER_PATH)file_info.jlink
	@echo set '$$'ImageSize = $(shell printf '0x%X\n' $$(stat --printf="%s" $(BIN_DIR)/webpages.espfs))>>$(FLASHER_PATH)file_info.jlink
	@echo set '$$'ImageAddr = 0x0D0000>>$(FLASHER_PATH)file_info.jlink
	@echo end>>$(FLASHER_PATH)file_info.jlink
	@echo define call2>>$(FLASHER_PATH)file_info.jlink
	@echo FlasherWrite $(BIN_DIR)/webpages.espfs '$$'ImageAddr '$$'ImageSize>>$(FLASHER_PATH)file_info.jlink
	@echo end>>$(FLASHER_PATH)file_info.jlink
	@cmd /K start $(JLINK_PATH)$(JLINK_GDBSRV) -device Cortex-M3 -if SWD -ir -endian little -speed $(FLASHER_SPEED) 
	@$(GDB) -x $(FLASHER_PATH)gdb_wrfile.jlink


flashburn:
	@echo define call1>$(FLASHER_PATH)flash_file.jlink
	@echo SetFirwareSize build/bin/ram_all.bin>>$(FLASHER_PATH)flash_file.jlink
	@echo end>>$(FLASHER_PATH)flash_file.jlink
	@echo define call2>>$(FLASHER_PATH)flash_file.jlink
	@echo FlasherWrite build/bin/ram_all.bin 0 '$$'Image1Size>>$(FLASHER_PATH)flash_file.jlink
	@echo end>>$(FLASHER_PATH)flash_file.jlink
	@echo define call3>>$(FLASHER_PATH)flash_file.jlink
	@echo FlasherWrite build/bin/ram_all.bin '$$'Image2Addr '$$'Image2Size>>$(FLASHER_PATH)flash_file.jlink
	@echo end>>$(FLASHER_PATH)flash_file.jlink
	@cmd /K start $(JLINK_PATH)$(JLINK_GDBSRV) -device Cortex-M3 -if SWD -ir -endian little -speed $(FLASHER_SPEED)
	@$(GDB) -x $(FLASHER_PATH)gdb_wrflash.jlink
	#@taskkill /F /IM $(JLINK_GDBSRV)

else

flash_boot:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'rtl8710_flash_auto_erase 1' -c 'rtl8710_flash_auto_verify 1' \
	-c 'rtl8710_flash_write $(RAM1P_IMAGE) 0' \
	-c 'rtl8710_reboot' -c 'reset run' -c shutdown

flash_images:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'rtl8710_flash_auto_erase 1' -c 'rtl8710_flash_auto_verify 1' \
	-c 'rtl8710_flash_write $(OTA_IMAGE) 0x0b000' \
	-c 'rtl8710_reboot' -c 'reset run' -c shutdown

flash_OTA:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'rtl8710_flash_auto_erase 1' -c 'rtl8710_flash_auto_verify 1' \
	-c 'rtl8710_flash_write $(OTA_IMAGE) 0x80000' \
	-c 'rtl8710_reboot' -c 'reset run' -c shutdown
	
flash_webfs:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'rtl8710_flash_auto_erase 1' -c 'rtl8710_flash_auto_verify 1' \
	-c 'rtl8710_flash_write $(BIN_DIR)/WEBFiles.bin 0xd0000' \
	-c 'rtl8710_reboot' -c 'reset run' -c shutdown

flashespfs:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'rtl8710_flash_auto_erase 1' -c 'rtl8710_flash_auto_verify 1' \
	-c 'rtl8710_flash_write $(BIN_DIR)/webpages.espfs 0xd0000' \
	-c 'rtl8710_reboot' -c 'reset run' -c shutdown
	
reset:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'mww 0x40000210 0x111157' -c 'rtl8710_reboot' -c shutdown
	
runram:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'load_image $(RAM1_IMAGE) 0x10000bc8 bin' \
	-c 'load_image $(RAM2_IMAGE) 0x10006000 bin' \
	-c 'mww 0x40000210 0x20111157' -c 'rtl8710_reboot' -c shutdown

runsdram:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'load_image $(RAM1_IMAGE) 0x10000bc8 bin' \
	-c 'load_image $(RAM2_IMAGE) 0x10006000 bin' \
	-c 'boot_load_srdam $(RAM3_IMAGE) 0x30000000' \
	-c shutdown

flashburn:
	@$(OPENOCD) -f interface/$(FLASHER_TYPE).cfg -c 'transport select swd' -c 'adapter_khz 1000' \
	-f $(FLASHER_PATH)rtl8710.ocd -c 'init' -c 'reset halt' -c 'adapter_khz $(FLASHER_SPEED)' \
	-c 'rtl8710_flash_auto_erase 1' -c 'rtl8710_flash_auto_verify 1' \
	-c 'rtl8710_flash_write $(RAM1P_IMAGE) 0' \
	-c 'rtl8710_flash_write $(OTA_IMAGE) 0xb000' \
	-c 'rtl8710_reboot' -c 'reset run' -c shutdown

endif

clean:
	@rm -f $(BIN_DIR)/*.bin
	