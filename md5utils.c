/* MDDRIVER.C - test driver for MD2, MD4 and MD5
 */


/* Copyright (C) 1990-2, RSA Data Security, Inc. Created 1990. All
rights reserved.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

/*

jku: added support to deal with vectors

*/

#define MD 5

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "md5global.h"
#include "md5.h"
#include "md5utils.h"

#include "dprint.h"


static void MDString PROTO_LIST ((char *));

#define MD_CTX MD5_CTX
#define MDInit MD5Init
#define MDUpdate MD5Update
#define MDFinal MD5Final


/* Digests a string array and store the result in dst; assumes
  32 bytes in dst
 */
void MDStringArray (char *dst, str src[], int size)
{
  MD_CTX context;
  unsigned char digest[16];
  int i;

  MDInit (&context);
  for (i=0; i<size; i++) {
  	MDUpdate (&context, src[i].s, src[i].len);
  }
  MDFinal (digest, &context);

  for (i=0; i<16; i++)
    sprintf(dst+i*2, "%02x", digest[i] );

  DBG("DEBUG: MD5 calculated: %32s\n", dst );
}
