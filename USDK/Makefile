include userset.mk

all: ram_all
mp: ram_all_mp

.PHONY: ram_all
ram_all:
	@$(MAKE) -f $(SDK_PATH)sdkbuild.mk
	@$(MAKE) -f $(SDK_PATH)flasher.mk genbin

.PHONY: ram_all_mp
ram_all_mp:
	@$(MAKE) -f $(SDK_PATH)sdkbuild.mk mp
	@$(MAKE) -f $(SDK_PATH)flasher.mk mp
	
.PHONY: clean  clean_all
clean:
	@$(MAKE) -f $(SDK_PATH)sdkbuild.mk clean

clean_all:
	@$(MAKE) -f $(SDK_PATH)sdkbuild.mk clean_all
	
.PHONY: flash_images runram reset test readfullflash flash_webfs flash_boot
flash_boot:
	@$(MAKE) -f $(SDK_PATH)flasher.mk flash_boot
	
flash_images:	
	@$(MAKE) -f $(SDK_PATH)flasher.mk flash_images

flash_OTA:
	@$(MAKE) -f $(SDK_PATH)flasher.mk flash_OTA

	
runram: 
	@$(MAKE) --f $(SDK_PATH)flasher.mk runram

runsdram: 
	@$(MAKE) --f $(SDK_PATH)flasher.mk runsdram

reset: 
	@$(MAKE) -f $(SDK_PATH)flasher.mk reset 

test: 
	JLink-RTL00ConsoleROM.bat

readfullflash:
	@$(MAKE) -f $(SDK_PATH)flasher.mk readfullflash 

