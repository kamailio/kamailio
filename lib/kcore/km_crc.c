#include "km_crc.h"

/*!
 * \brief CRC32 value from source string
 * \param source_string source string
 * \param hash_ret calulated CRC32
 */
void crc32_uint (str *source_string, unsigned int *hash_ret) 
{	
	unsigned int hash;	
	unsigned int len;
	const char *data;
	
	hash = 0xffffffff;
	data = source_string->s;
	
	for (len = source_string->len / 4; len--; data += 4) {
		hash = crc_32_tab[((unsigned char)hash) ^ data[0]] ^ (hash >> 8);
		hash = crc_32_tab[((unsigned char)hash) ^ data[1]] ^ (hash >> 8);
		hash = crc_32_tab[((unsigned char)hash) ^ data[2]] ^ (hash >> 8);
		hash = crc_32_tab[((unsigned char)hash) ^ data[3]] ^ (hash >> 8);
	}
	
	for (len = source_string->len % 4; len--; data++) {
		hash = crc_32_tab[((unsigned char)hash) ^ *data] ^ (hash >> 8);
	}
	
	*hash_ret = ~hash;
}

