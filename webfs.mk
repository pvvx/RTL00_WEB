include userset.mk
include $(SDK_PATH)paths.mk

all:
	@mkdir -p $(BIN_DIR)
	./tools/webfs/WEBFS22.exe -h "*.htm, *.html, *.cgi, *.xml, *.bin, *.txt, *.wav" -z "mdbini.bin, *.inc, *.ini, snmp.bib, *.ovl" ./WEBFiles $(BIN_DIR) WEBFiles.bin
	#@$(PHYTON) ./tools/webfs/py/webfs_tool.py build -s ./WEBFiles -d "*.htm, *.html, *.cgi, *.xml, *.bin, *.txt, *.wav, *.js" -n "*.inc, *.ini, snmp.bib, *.ovl" $(BIN_DIR)/WEBFiles1.bin

clean:
