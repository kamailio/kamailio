/*
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 * 
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 * 
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 * 
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 * 
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/*!
 * \file
 * \brief Kamailio core :: md5 hash support
 * \ingroup core
 * Module: \ref core
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "md5.h"
#include "md5utils.h"
#include "dprint.h"
#include "ut.h"


/*!
  * \brief Calculate a MD5 digests over a string array
  * 
  * Calculate a MD5 digests over a string array and stores the result in the
  * destination char array. This function assumes 32 bytes in the destination
  * buffer.
  * \param dst destination
  * \param src string input array
  * \param size elements in the input array
  */
void MD5StringArray (char *dst, str src[], int size)
{
	MD5_CTX context;
	unsigned char digest[16];
 	int i;
	int len;
	char *s;

	MD5Init (&context);
	for (i=0; i<size; i++) {
		trim_len( len, s, src[i] );
		if (len > 0)
  			MD5Update (&context, s, len);
  }
  U_MD5Final (digest, &context);

  string2hex(digest, 16, dst );
  LM_DBG("MD5 calculated: %.*s\n", MD5_LEN, dst );

}
