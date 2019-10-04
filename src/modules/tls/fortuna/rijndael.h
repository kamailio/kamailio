/*
 * contrib/pgcrypto/rijndael.h
 *
 *	$OpenBSD: rijndael.h,v 1.3 2001/05/09 23:01:32 markus Exp $ */

/* This is an independent implementation of the encryption algorithm:	*/
/*																		*/
/*		   RIJNDAEL by Joan Daemen and Vincent Rijmen					*/
/*																		*/
/* which is a candidate algorithm in the Advanced Encryption Standard	*/
/* programme of the US National Institute of Standards and Technology.	*/
/*																		*/
/* Copyright in this implementation is held by Dr B R Gladman but I		*/
/* hereby give permission for its free direct or derivative use subject */
/* to acknowledgment of its origin and compliance with any conditions	*/
/* that the originators of the algorithm place on its exploitation.		*/
/*																		*/
/* Dr Brian Gladman (gladman@seven77.demon.co.uk) 14th January 1999		*/

#ifndef _RIJNDAEL_H_
#define _RIJNDAEL_H_

#include <sys/types.h>

/* 1. Standard types for AES cryptography source code				*/

typedef u_int8_t u1byte;			/* an 8 bit u_int8_tacter type */
typedef u_int16_t u2byte;			/* a 16 bit unsigned integer type	*/
typedef u_int32_t u4byte;			/* a 32 bit unsigned integer type	*/

typedef int8_t s1byte;			/* an 8 bit signed character type	*/
typedef int16_t s2byte;			/* a 16 bit signed integer type		*/
typedef int32_t s4byte;			/* a 32 bit signed integer type		*/

typedef struct _rijndael_ctx
{
	u4byte		k_len;
	int			decrypt;
	u4byte		e_key[64];
	u4byte		d_key[64];
} rijndael_ctx;


/* 2. Standard interface for AES cryptographic routines				*/

/* These are all based on 32 bit unsigned values and will therefore */
/* require endian conversions for big-endian architectures			*/

rijndael_ctx *rijndael_set_key(rijndael_ctx *, const u4byte *, const u4byte, int);
void		rijndael_encrypt(rijndael_ctx *, const u4byte *, u4byte *);
void		rijndael_decrypt(rijndael_ctx *, const u4byte *, u4byte *);

/* conventional interface */

void		aes_set_key(rijndael_ctx *ctx, const u_int8_t *key, unsigned keybits, int enc);
void		aes_ecb_encrypt(rijndael_ctx *ctx, u_int8_t *data, unsigned len);
void		aes_ecb_decrypt(rijndael_ctx *ctx, u_int8_t *data, unsigned len);
void		aes_cbc_encrypt(rijndael_ctx *ctx, u_int8_t *iva, u_int8_t *data, unsigned len);
void		aes_cbc_decrypt(rijndael_ctx *ctx, u_int8_t *iva, u_int8_t *data, unsigned len);

#endif   /* _RIJNDAEL_H_ */
