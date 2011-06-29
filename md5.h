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
 * \brief SIP-router core :: md5 hash support
 * \ingroup core
 * Module: \ref core
 */

#ifndef MD5_H
#define MD5_H

/**
 * \brief MD5 context
 */
typedef struct {
  unsigned int state[4];                                   /* state (ABCD) */
  unsigned int count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

/**
 * \brief MD5 context initialization
 * 
 * MD5 context initialization. Begins an MD5 operation, writing a new context.
 * \param context initialized context
 */
void MD5Init (MD5_CTX *context);

/**
 * \brief MD5 block update operation
 * 
 * MD5 block update operation. Continues an MD5 message-digest
 * operation, processing another message block, and updating the
 * context.
 * \param context context
 * \param input input block
 * \param inputLen length of input block
 */
void U_MD5Update (MD5_CTX *context, unsigned char *input, unsigned int inputLen);

  /**
 * \brief MD5 finalization
 * 
 * MD5 finalization. Ends an MD5 message-digest operation, writing the
 * the message digest and zeroizing the context.
 * \param digest message digest
 * \param context context
 */
void U_MD5Final (unsigned char digest[16], MD5_CTX *context);

/*!
 * \brief Small wrapper around MD5Update
 *
 * Small wrapper around MD5Update, because everybody uses this on 'str' types
 * \param context MD5 context
 * \param input input block
 * \param inputLen length of input block
 * \note please not use this in new code
 * \todo review and fix all wrong usage
 */
static inline void MD5Update (MD5_CTX *context, char *input, unsigned int inputLen)
{
	return U_MD5Update(context, (unsigned char *)input, inputLen);
}

/*!
 * \brief Small wrapper around MD5Final
 *
 * Small wrapper around MD5Final, because everybody uses this on 'str' types
 * \param digest message digest
 * \param context MD5 context
 * \note please not use this in new code
 * \todo review and fix all wrong usage
 */
static inline void MD5Final (char digest[16], MD5_CTX *context)
{
	U_MD5Final((unsigned char *)digest, context);
}

#endif /* MD5_H */
