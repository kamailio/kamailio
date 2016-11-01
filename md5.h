/*	$OpenBSD: md5.h,v 1.15 2004/05/03 17:30:14 millert Exp $	*/

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 */

#ifndef _MD5_H_
#define _MD5_H_

#ifndef __OS_solaris

#define	MD5_BLOCK_LENGTH		64
#define	MD5_DIGEST_LENGTH		16
#define	MD5_DIGEST_STRING_LENGTH	(MD5_DIGEST_LENGTH * 2 + 1)

/* Probably not the proper place, but will do for Debian: */
#include <sys/types.h>

typedef struct MD5Context {
	u_int32_t state[4];			/* state */
	u_int64_t count;			/* number of bits, mod 2^64 */
	unsigned char buffer[MD5_BLOCK_LENGTH];	/* input buffer */
} MD5_CTX;

void	 MD5Init(MD5_CTX *);
void	 U_MD5Update(MD5_CTX *, const unsigned char *, size_t);
void	 MD5Pad(MD5_CTX *);
void	 U_MD5Final(unsigned char [MD5_DIGEST_LENGTH], MD5_CTX *);
void	 MD5Transform(u_int32_t [4], const unsigned char [MD5_BLOCK_LENGTH]);

static inline void MD5Update(MD5_CTX *ctx, const char *str, size_t len) {
	U_MD5Update(ctx, (const unsigned char *)str, len);
}

static inline void MD5Final(char buf[MD5_DIGEST_LENGTH], MD5_CTX *ctx) {
	U_MD5Final((unsigned char *)buf, ctx);
}

#else /* __OS_solaris */
#include <md5.h>

#define U_MD5Update(ctx, input, len) \
	MD5Update(ctx, input, len)
#define U_MD5Final(digest, ctx) \
	MD5Final(digest, ctx)

#endif /* __OS_solaris */

#endif /* _MD5_H_ */
