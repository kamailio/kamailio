
#ifndef _CRC_H_
#define _CRC_H_

#include "str.h"

#define CRC16_LEN	4

extern unsigned long int crc_32_tab[];
extern unsigned short int ccitt_tab[];
extern unsigned short int crc_16_tab[];


unsigned short crcitt_string_ex( char *s, int len, register unsigned short ccitt);
unsigned short crcitt_string( char *s, int len );
void crcitt_string_array( char *dst, str src[], int size );

void crc32_uint (str *source_string, unsigned int *hash_ret);

#endif /* _CRC_H_ */

