/* $Id$*/

#ifndef _CRC_H_
#define _CRC_H_

#define CRC16_LEN	4

extern unsigned long int crc_32_tab[];
extern unsigned short int ccitt_tab[];
extern unsigned short int crc_16_tab[];

unsigned short crcitt_string( char *s, int len );
void crcitt_string_array( char *dst, str src[], int size );

#endif

