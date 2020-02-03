/*
 * FILE:	sha256.h
 * AUTHOR:	Aaron D. Gifford - http://www.aarongifford.com/
 *
 * Copyright (c) 2000-2001, Aaron D. Gifford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __SHA2_H__
#define __SHA2_H__

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Import u_intXX_t size_t type definitions from system headers.  You
 * may need to change this, or define these things yourself in this
 * file.
 */
#include <sys/types.h>

#ifdef SHA2_USE_INTTYPES_H

#include <inttypes.h>

#endif /* SHA2_USE_INTTYPES_H */

/* fix types for Sun Solaris */
#if defined(__SVR4) || defined(__sun)
    typedef uint8_t u_int8_t;
    typedef uint32_t u_int32_t;
    typedef uint64_t u_int64_t;
#endif

/*** SHA-256/384/512 Various Length Definitions ***********************/
#define SHA256_BLOCK_LENGTH		64
#define SHA256_DIGEST_LENGTH		32
#define SHA256_DIGEST_STRING_LENGTH	(SHA256_DIGEST_LENGTH * 2 + 1)
#define SHA384_BLOCK_LENGTH		128
#define SHA384_DIGEST_LENGTH		48
#define SHA384_DIGEST_STRING_LENGTH	(SHA384_DIGEST_LENGTH * 2 + 1)
#define SHA512_BLOCK_LENGTH		128
#define SHA512_DIGEST_LENGTH		64
#define SHA512_DIGEST_STRING_LENGTH	(SHA512_DIGEST_LENGTH * 2 + 1)


/*** SHA-256/384/512 Context Structures *******************************/
/* NOTE: If your architecture does not define either u_intXX_t types or
 * uintXX_t (from inttypes.h), you may need to define things by hand
 * for your system:
 */
#if 0
typedef unsigned char u_int8_t;		/* 1-byte  (8-bits)  */
typedef unsigned int u_int32_t;		/* 4-bytes (32-bits) */
typedef unsigned long long u_int64_t;	/* 8-bytes (64-bits) */
#endif
/*
 * Most BSD systems already define u_intXX_t types, as does Linux.
 * Some systems, however, like Compaq's Tru64 Unix instead can use
 * uintXX_t types defined by very recent ANSI C standards and included
 * in the file:
 *
 *   #include <inttypes.h>
 *
 * If you choose to use <inttypes.h> then please define:
 *
 *   #define SHA2_USE_INTTYPES_H
 *
 * Or on the command line during compile:
 *
 *   cc -DSHA2_USE_INTTYPES_H ...
 */
#ifdef SHA2_USE_INTTYPES_H

typedef struct _SHA256_CTX {
	uint32_t	state[8];
	uint64_t	bitcount;
	uint8_t	buffer[SHA256_BLOCK_LENGTH];
} SHA256_CTX;
typedef struct _SHA512_CTX {
	uint64_t	state[8];
	uint64_t	bitcount[2];
	uint8_t	buffer[SHA512_BLOCK_LENGTH];
} SHA512_CTX;

#else /* SHA2_USE_INTTYPES_H */

typedef struct _SHA256_CTX {
	u_int32_t	state[8];
	u_int64_t	bitcount;
	u_int8_t	buffer[SHA256_BLOCK_LENGTH];
} SHA256_CTX;
typedef struct _SHA512_CTX {
	u_int64_t	state[8];
	u_int64_t	bitcount[2];
	u_int8_t	buffer[SHA512_BLOCK_LENGTH];
} SHA512_CTX;

#endif /* SHA2_USE_INTTYPES_H */

typedef SHA512_CTX SHA384_CTX;


/*** SHA-256/384/512 Function Prototypes ******************************/
#ifndef NOPROTO
#ifdef SHA2_USE_INTTYPES_H

void sr_SHA256_Init(SHA256_CTX *);
void sr_SHA256_Update(SHA256_CTX*, const uint8_t*, size_t);
void sr_SHA256_Final(uint8_t[SHA256_DIGEST_LENGTH], SHA256_CTX*);
char* sr_SHA256_End(SHA256_CTX*, char[SHA256_DIGEST_STRING_LENGTH]);
char* sr_SHA256_Data(const uint8_t*, size_t, char[SHA256_DIGEST_STRING_LENGTH]);

void sr_SHA384_Init(SHA384_CTX*);
void sr_SHA384_Update(SHA384_CTX*, const uint8_t*, size_t);
void sr_SHA384_Final(uint8_t[SHA384_DIGEST_LENGTH], SHA384_CTX*);
char* sr_SHA384_End(SHA384_CTX*, char[SHA384_DIGEST_STRING_LENGTH]);
char* sr_SHA384_Data(const uint8_t*, size_t, char[SHA384_DIGEST_STRING_LENGTH]);

void sr_SHA512_Init(SHA512_CTX*);
void sr_SHA512_Update(SHA512_CTX*, const uint8_t*, size_t);
void sr_SHA512_Final(uint8_t[SHA512_DIGEST_LENGTH], SHA512_CTX*);
char* sr_SHA512_End(SHA512_CTX*, char[SHA512_DIGEST_STRING_LENGTH]);
char* sr_SHA512_Data(const uint8_t*, size_t, char[SHA512_DIGEST_STRING_LENGTH]);

#else /* SHA2_USE_INTTYPES_H */

void sr_SHA256_Init(SHA256_CTX *);
void sr_SHA256_Update(SHA256_CTX*, const u_int8_t*, size_t);
void sr_SHA256_Final(u_int8_t[SHA256_DIGEST_LENGTH], SHA256_CTX*);
char* sr_SHA256_End(SHA256_CTX*, char[SHA256_DIGEST_STRING_LENGTH]);
char* sr_SHA256_Data(const u_int8_t*, size_t, char[SHA256_DIGEST_STRING_LENGTH]);

void sr_SHA384_Init(SHA384_CTX*);
void sr_SHA384_Update(SHA384_CTX*, const u_int8_t*, size_t);
void sr_SHA384_Final(u_int8_t[SHA384_DIGEST_LENGTH], SHA384_CTX*);
char* sr_SHA384_End(SHA384_CTX*, char[SHA384_DIGEST_STRING_LENGTH]);
char* sr_SHA384_Data(const u_int8_t*, size_t, char[SHA384_DIGEST_STRING_LENGTH]);

void sr_SHA512_Init(SHA512_CTX*);
void sr_SHA512_Update(SHA512_CTX*, const u_int8_t*, size_t);
void sr_SHA512_Final(u_int8_t[SHA512_DIGEST_LENGTH], SHA512_CTX*);
char* sr_SHA512_End(SHA512_CTX*, char[SHA512_DIGEST_STRING_LENGTH]);
char* sr_SHA512_Data(const u_int8_t*, size_t, char[SHA512_DIGEST_STRING_LENGTH]);

#endif /* SHA2_USE_INTTYPES_H */

#else /* NOPROTO */

void sr_SHA256_Init();
void sr_SHA256_Update();
void sr_SHA256_Final();
char* sr_SHA256_End();
char* sr_SHA256_Data();

void sr_SHA384_Init();
void sr_SHA384_Update();
void sr_SHA384_Final();
char* sr_SHA384_End();
char* sr_SHA384_Data();

void sr_SHA512_Init();
void sr_SHA512_Update();
void sr_SHA512_Final();
char* sr_SHA512_End();
char* sr_SHA512_Data();

#endif /* NOPROTO */

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* __SHA2_H__ */
