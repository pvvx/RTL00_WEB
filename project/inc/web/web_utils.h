 /******************************************************************************
 * FileName: web_utils.h
 * Alternate SDK 
 * Author: PV`
 * (c) PV` 2015
*******************************************************************************/

#ifndef _INCLUDE_WEB_UTILS_H_
#define _INCLUDE_WEB_UTILS_H_

typedef enum {
	SEG_ID_ROM = 0,
	SEG_ID_SRAM,
	SEG_ID_TCM,
	SEG_ID_FLASH,
	SEG_ID_SDRAM,
	SEG_ID_SOC,
	SEG_ID_CPU,
	SEG_ID_ERR,
	SEG_ID_MAX
} SEG_ID;

extern const uint32 tab_seg_def[];
SEG_ID get_seg_id(uint32 addr, int32 size);

int rom_atoi(const char *s);
void copy_align4(void *ptrd, void *ptrs, uint32 len);
uint32 hextoul(uint8 *s);
uint32 ahextoul(uint8 *s);
uint8 * cmpcpystr(uint8 *pbuf, uint8 *pstr, uint8 a, uint8 b, uint16 len);
uint8 * web_strnstr(const uint8* buffer, const uint8* token, int n);
bool base64decode(const uint8 *in, int len, uint8_t *out, int *outlen);
size_t base64encode(char* target, size_t target_len, const char* source, size_t source_len);
void strtomac(uint8 *s, uint8 *macaddr);
//uint32 strtoip(uint8 *s); // ipaddr_addr();
int urldecode(uint8 *d, uint8 *s, uint16 lend, uint16 lens);
//int urlencode(uint8 *d, uint8 *s, uint16 lend, uint16 lens);
int htmlcode(uint8 *d, uint8 *s, uint16 lend, uint16 lens);
void print_hex_dump(uint8 *buf, uint32 len, uint8 k);
// char* str_to_upper_case(char* text);
uint32 str_array(uint8 *s, uint32 *buf, uint32 max_buf);
uint32 str_array_w(uint8 *s, uint16 *buf, uint32 max_buf);
uint32 str_array_b(uint8 *s, uint8 *buf, uint32 max_buf);
char* word_to_lower_case(char* text);
int rom_xstrcmp(char * pd, const char * ps);
int rom_xstrcpy(char * pd, const char * ps);

#endif /* _INCLUDE_WEB_UTILS_H_ */
