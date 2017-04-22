include paths.mk

all:
	@mkdir -p $(BIN_DIR)
	./tools/webfs/WEBFS22.exe -h "*.htm, *.html, *.cgi, *.xml, *.bin, *.txt, *.wav" -z "mdbini.bin, *.inc, *.ini, snmp.bib, *.ovl" ./WEBFiles $(BIN_DIR) WEBFiles.bin

clean:
