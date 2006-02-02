#ifndef __CDS_HASH_FUNCTIONS_H
#define __CDS_HASH_FUNCTIONS_H

/* hash functions taken from everywhere:
 * http://www.partow.net/programming/hashfunctions/
 * SER sources (from jabber module, from pa module, from core)
 * RSA (MD5 - more in separate MD5 files)
 *
 * copyrights will be corrected soon
 *
 * */

unsigned int hash_md5(const char *s, unsigned int len);
unsigned int hash_sha(const char *s, unsigned int len);
unsigned int hash_domain(const char* s, unsigned int len); /* :-) */

#include <cds/GeneralHashFunctions.h>

#endif
