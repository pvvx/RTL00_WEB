@echo off
call paths.bat
cd flasher
openocd -f interface/cmsis-dap.cfg -c "adapter_khz 1000" -f rtl8710.ocd -f cortex.ocd -c "init" -c "reset halt" -c "rtl8710_flash_read_id" -c "adapter_khz 1000" -c "rtl8710_flash_read ../fullflash.bin 0 2097152" -c "shutdown"
echo flash read fullflash.bin
pause

