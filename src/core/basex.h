/*
 * convert/decode to/from ascii using various bases
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*!
 * \file
 * \brief Kamailio core :: convert/decode to/from ascii using various bases
 *
 * Copyright (C) 2008 iptelorg GmbH
 * \ingroup core
 *
 * Module: \ref core
 *
 *
 * Functions:
 *  - base16_enc(src, src_len, dst, dst_len)    : encode to standard hex
 *  - base16_dec(src, src_len, dst, dst_len)    : decode from standard hex
 *  - base16_enc_len(len)                       : length needed to encode len bytes (macro)
 *  - base16_max_dec_len(len)                   : length needed to decode a string of size len
 *
 *  - base64_enc(src, src_len, dst, dst_len)    : encode to base64, standard alphabet
 *  - base64_dec(src, src_len, dst, dst_len)    : decode from base64, standard  alphabet
 *  - base64_enc_len(len)                       : length needed to encode len bytes (macro)
 *  - base64_max_dec_len(len)                   : maximum length needed to decode len bytes (macro)
 *  - base64_dec_len(str, len)                  : size of the decoded str 
 *  - q_base64_enc(src, src_len, dst, dst_len)  : encode to special base64 alphabet (non standard)
 *  - q_base64_dec(src, src_len, dst, dst_len)  - decode from special non-standard base64 alphabet
 *
 *  All the above functions return the size used (in dst) on success and
 *   0 or a negative number (which is -1*size_needed) on error.
 *
 * There are close to no checks for validity, an unexpected char will lead
 * to a corrupted result, but the functions won't return error.
 *
 * Notes:
 *  on a core2 duo the versions with lookup tables are way faster (see
 *  http://www.experts-exchange.com/Programming/Languages/CPP/Q_21988706.html
 *  for some interesting tests and ideeas).
 *
 *  Test results for 40 bytes  (typical ser nounce) in average cpu cycles:
\verbatim
 *                    lookup   lookup_large lookup8k no-lookup
 *  base16_enc           211/231  218/199      -       1331
 *  base16_dec           252/251  236          -       1226
 *  base64_enc           209      186         156      1005
 *  base64_dec           208      207         207      1242
 *  q_base64_enc         -                              288
 *  q_base64_dec         -                              281
 *  (see test/basex.txt for more results)
\endverbatim
 *
 * Defines:
 *  - BASE64_LOOKUP_TABLE/NO_BASE64_LOOKUP_TABLE : use (default)/don't use
 *     small lookup tables for conversions (faster in general).
 *  - BASE64_LOOKUP_LARGE    : use large lookup tables (2560 bytes for 
 *    encoding and 256 bytes for decoding; without it 64 bytes are used for
 *    encoding and 85 bytes for decoding.
 *  - BASE64_LOOKUP_8K : use even larger lookup tables (8K for encoding and
 *    256 for decoding); also try to write 2 bytes at a time (short) if
 *    the destination is 2 byte aligned
 *
 *  - BASE16_LOOKUP_TABLE/NO_BASE16_LOOKUP_TABLE : use (default)/don't use
 *     small lookup tables for conversions (faster in general).
 *  - BASE16_LOOKUP_LARGE  : use large lookup tables (512 bytes for 
 *    encoding and 256 bytes for decoding
 *  - BASE16_READ_WHOLE_INTS : read an int at a time
 *
 * History:
 * --------
 *  2008-06-11  created by andrei
 */
 


#ifndef _basex_h
#define _basex_h

#include "compiler_opt.h"

/* defaults */
#ifndef NO_BASE16_LOOKUP_TABLE
#define BASE16_LOOKUP_TABLE
#endif

#ifndef NO_BASE64_LOOKUP_TABLE
#define BASE64_LOOKUP_TABLE
#endif

#ifndef NO_BASE64_LOOKUP_8K
#define BASE64_LOOKUP_8K
#endif

#ifndef NO_BASE16_LOOKUP_LARGE
#define BASE16_LOOKUP_LARGE
#endif

#if !defined NO_BASE64_LOOKUP_LARGE && !defined BASE64_LOOKUP_8K
#define BASE64_LOOKUP_LARGE
#endif



#if defined BASE16_READ_WHOLE_INTS || defined BASE64_READ_WHOLE_INTS || \
	defined BASE64_LOOKUP_8K
#include "endianness.h"

/*! \brief aligns p to a type* pointer, type must have a 2^k size */
#define ALIGN_POINTER(p, type) \
	((type*) ((long)((char*)(p)+sizeof(type)-1)&~(long)(sizeof(type)-1)))

#define ALIGN_UINT_POINTER(p) ALIGN_POINTER(p, unsigned int)

#endif


#ifdef BASE16_LOOKUP_TABLE

#ifdef BASE16_LOOKUP_LARGE
/*! \brief use large tables: 512 for lookup and 256 for decode */

extern unsigned char _bx_hexdig_hi[256];
extern unsigned char _bx_hexdig_low[256];

/*! \brief returns the first 4 bits of c converted to a hex digit */
#define HEX_HI(h)	_bx_hexdig_hi[(unsigned char)(h)]
/*! \brief returns the low 4 bits of converted to a hex digit */
#define HEX_LOW(h)	_bx_hexdig_low[(unsigned char)(h)]

extern unsigned char _bx_unhexdig256[256];

/*! \brief  converts hex_digit to a number (0..15); it might
 *      \return 0xff for invalid digit (but with some compile
 *      option it won't check)
 */
#define UNHEX(h)	_bx_unhexdig256[(h)]

#else /* BASE16_LOOKUP_LARGE */
/*! \brief use small tabes: 16 bytes for lookup and 32 for decode */

extern unsigned char _bx_hexdig[16+1];

#define HEX_4BITS(h) _bx_hexdig[(h)]
#define HEX_HI(h)	HEX_4BITS(((unsigned char)(h))>>4)
#define HEX_LOW(h)	HEX_4BITS((h)&0xf)

extern unsigned char _bx_unhexdig32[32];
#define UNHEX(h) _bx_unhexdig32[(((h))-'0')&0x1f]

#endif /* BASE16_LOOKUP_LARGE */

#else /* BASE16_LOOKUP_TABLE */
/* no lookup tables */
#if 0
#define HEX_4BITS(h) (unsigned char)((unlikely((h)>=10))?((h)-10+'A'):(h)+'0')
#define UNHEX(c) (unsigned char)((unlikely((c)>='A'))?(c)-'A'+10:(c)-'0')
#else
#define HEX_4BITS(hc) (unsigned char)( ((((hc)>=10)-1)&((hc)+'0')) | \
									((((hc)<10)-1)&((hc)+'A')) )
#define UNHEX(c) (unsigned char) ( ((((c)>'9')-1)& ((c)-'0')) | \
								((((c)<='9')-1)&((c)-'A')) )
#endif 

#define HEX_HI(h)	HEX_4BITS(((unsigned char)(h))>>4)
#define HEX_LOW(h)	HEX_4BITS((h)&0xf)

#endif /* BASE16_LOOKUP_TABLE */


#ifdef BASE64_LOOKUP_TABLE
#ifdef BASE64_LOOKUP_LARGE
/* large lookup tables, 2.5 k */

extern unsigned char _bx_b64_first[256];
extern unsigned char _bx_b64_second[4][256];
extern unsigned char _bx_b64_third[4][256];
extern unsigned char _bx_b64_fourth[256];

#define BASE64_1(a) _bx_b64_first[(a)]
#define BASE64_2(a,b) _bx_b64_second[(a)&0x3][(b)]
#define BASE64_3(b,c) _bx_b64_third[(c)>>6][(b)]
#define BASE64_4(c) _bx_b64_fourth[(c)]

extern unsigned char _bx_ub64[256];
#define UNBASE64(v) _bx_ub64[(v)]

#elif defined BASE64_LOOKUP_8K
/* even larger encode tables: 8k */
extern unsigned short _bx_b64_12[4096];

/* return a word (16 bits) */
#define BASE64_12(a,b)	_bx_b64_12[((a)<<4)|((b)>>4)]
#define BASE64_34(b,c)	_bx_b64_12[(((b)&0xf)<<8)|(c)]
#ifdef __IS_LITTLE_ENDIAN
#define FIRST_8B(s)	((unsigned char)(s))
#define LAST_8B(s)	((s)>>8)
#elif defined __IS_BIG_ENDIAN
#define FIRST_8B(s)	((s)>>8)
#define LAST_8B(s)	((unsigned char)(s))
#else
#error neither __IS_LITTLE_ENDIAN nor __IS_BIG_ENDIAN are defined
#endif


extern unsigned char _bx_ub64[256];
#define UNBASE64(v) _bx_ub64[(v)]

#else /* BASE64_LOOKUP_LARGE */
/* small lookup tables */
extern unsigned char _bx_b64[64+1];

#define BASE64_DIG(v)	_bx_b64[(v)]

#define BASE64_1(a)		BASE64_DIG((a)>>2)
#define BASE64_2(a, b)	BASE64_DIG( (((a)<<4)&0x3f) | ((b)>>4))
#define BASE64_3(b, c)	BASE64_DIG( (((b)<<2)&0x3f) | ((c)>>6))
#define BASE64_4(c)		BASE64_DIG((c)&0x3f)

extern unsigned char _bx_ub64[0x54+1];
#define UNBASE64(v) _bx_ub64[(((v)&0x7f)-0x2b)]

#endif /* BASE64_LOOKUP_LARGE */


#else /* BASE64_LOOKUP_TABLE */

#define BASE64_DIG(v) base64_enc_char(v)
#define BASE64_1(a)		BASE64_DIG((a)>>2)
#define BASE64_2(a, b)	BASE64_DIG( (((a)<<4)&0x3f) | ((b)>>4))
#define BASE64_3(b, c)	BASE64_DIG( (((b)<<2)&0x3f) | ((c)>>6))
#define BASE64_4(c)		BASE64_DIG((c)&0x3f)

#define UNBASE64(v) base64_dec_char(v)

#endif /* BASE64_LOOKUP_TABLE */



/*! \brief lenght needed for encoding l bytes */
#define base16_enc_len(l) (l*2)
/*! \brief maximum lenght needed for decoding l bytes */
#define base16_max_dec_len(l) (l/2)
/*! \brief actual space needed for decoding a string b of size l */
#define base16_dec_len(b, l) base16_max_dec_len(l)
/*! \brief minimum valid source len for decoding */
#define base16_dec_min_len() 2
/*! \brief minimum valid source len for encoding */
#define base16_enc_min_len() 0

/*! \brief space needed for encoding l bytes */
#define base64_enc_len(l) (((l)+2)/3*4)
/*! \brief maximum space needed for encoding l bytes */
#define base64_max_dec_len(l) ((l)/4*3)
/*! \brief actual space needed for decoding a string b of size l, l>=4 */
#define base64_dec_len(b, l) \
	(base64_max_dec_len(l)-((b)[(l)-2]=='=') -((b)[(l)-1]=='='))
/*! \brief minimum valid source len for decoding */
#define base64_dec_min_len() 4
/*! \brief minimum valid source len for encoding */
#define base64_enc_min_len() 0


#ifdef BASE16_READ_WHOLE_INTS

/*! 
 * \params: 
 * \return: size used from the output buffer (dst) on success,
 *          -size_needed on error
 *
 * WARNING: the output string is not 0-term
 */
inline static int base16_enc(unsigned char* src, int slen, unsigned char*  dst, int dlen)
{
	unsigned int* p;
	unsigned char* end;
	int osize;
	unsigned short us;
	
	osize=2*slen;
	if (unlikely(dlen<osize))
		return -osize;
	end=src+slen;
	p=ALIGN_UINT_POINTER(src);
	if (likely((unsigned char*)p<end)){
		switch((unsigned char)((unsigned char*)p-src)){
			case 3:
				*dst=HEX_HI(*src);
				*(dst+1)=HEX_LOW(*src);
				dst+=2;
				src++;
				/* no break */
			case 2:
				us=*(unsigned short*)(src);
#if   defined __IS_LITTLE_ENDIAN
				*(dst+0)=HEX_HI(us);
				*(dst+1)=HEX_LOW(us);
				*(dst+2)=HEX_HI(us>>8);
				*(dst+3)=HEX_LOW(us>>8);
#elif defined __IS_BIG_ENDIAN
				*(dst+2)=HEX_HI(us);
				*(dst+3)=HEX_LOW(us);
				*(dst+0)=HEX_HI(us>>8);
				*(dst+1)=HEX_LOW(us>>8);
#endif
				dst+=4;
				/* no need to inc src */
				break;
			case 1:
				*dst=HEX_HI(*src);
				*(dst+1)=HEX_LOW(*src);
				dst+=2;
				/* no need to inc src */
			case 0:
				break;
		}
		for(;(unsigned char*)p<=(end-4);p++,dst+=8){
#if   defined __IS_LITTLE_ENDIAN
			*(dst+0)=HEX_HI(*p);
			*(dst+1)=HEX_LOW(*p);
			*(dst+2)=HEX_HI(((*p)>>8));
			*(dst+3)=HEX_LOW(((*p)>>8));
			*(dst+4)=HEX_HI(((*p)>>16));
			*(dst+5)=HEX_LOW(((*p)>>16));
			*(dst+6)=HEX_HI(((*p)>>24));
			*(dst+7)=HEX_LOW(((*p)>>24));
#elif defined __IS_BIG_ENDIAN
			*(dst+6)=HEX_HI(*p);
			*(dst+7)=HEX_LOW(*p);
			*(dst+4)=HEX_HI(((*p)>>8));
			*(dst+5)=HEX_LOW(((*p)>>8));
			*(dst+2)=HEX_HI(((*p)>>16));
			*(dst+3)=HEX_LOW(((*p)>>16));
			*(dst+0)=HEX_HI(((*p)>>24));
			*(dst+1)=HEX_LOW(((*p)>>24));
#else
#error neither BIG ro LITTLE endian defined
#endif /* __IS_*_ENDIAN */
		}
		src=(unsigned char*)p;
		/* src is 2-bytes aligned (short) */
		switch((unsigned char)((unsigned char*)end-src)){
			case 3:
			case 2:
				us=*(unsigned short*)(src);
#if   defined __IS_LITTLE_ENDIAN
				*(dst+0)=HEX_HI(us);
				*(dst+1)=HEX_LOW(us);
				*(dst+2)=HEX_HI(us>>8);
				*(dst+3)=HEX_LOW(us>>8);
#elif defined __IS_BIG_ENDIAN
				*(dst+2)=HEX_HI(us);
				*(dst+3)=HEX_LOW(us);
				*(dst+0)=HEX_HI(us>>8);
				*(dst+1)=HEX_LOW(us>>8);
#endif
				if ((end-src)==3){
					*(dst+4)=HEX_HI(*(src+2));
					*(dst+5)=HEX_LOW(*(src+2));
				}
				/* no need to inc anything */
				break;
			case 1:
				*dst=HEX_HI(*src);
				*(dst+1)=HEX_LOW(*src);
				/* no need to inc anything */
			case 0:
				break;
		}
	}else if (unlikely((long)src&1)){
		/* src is not 2-bytes (short) aligned */
		switch((unsigned char)((unsigned char*)end-src)){
			case 3:
				*dst=HEX_HI(*src);
				*(dst+1)=HEX_LOW(*src);
				dst+=2;
				src++;
				/* no break */
			case 2:
				us=*(unsigned short*)(src);
#if   defined __IS_LITTLE_ENDIAN
				*(dst+0)=HEX_HI(us);
				*(dst+1)=HEX_LOW(us);
				*(dst+2)=HEX_HI(us>>8);
				*(dst+3)=HEX_LOW(us>>8);
#elif defined __IS_BIG_ENDIAN
				*(dst+2)=HEX_HI(us);
				*(dst+3)=HEX_LOW(us);
				*(dst+0)=HEX_HI(us>>8);
				*(dst+1)=HEX_LOW(us>>8);
#endif
				/* no need to inc anything */
				break;
			case 1:
				*dst=HEX_HI(*src);
				*(dst+1)=HEX_LOW(*src);
				/* no need to inc anything */
			case 0:
				break;
		}
	}else{
		/* src is 2-bytes aligned (short) */
		switch((unsigned char)((unsigned char*)end-src)){
			case 3:
			case 2:
				us=*(unsigned short*)(src);
#if   defined __IS_LITTLE_ENDIAN
				*(dst+0)=HEX_HI(us);
				*(dst+1)=HEX_LOW(us);
				*(dst+2)=HEX_HI(us>>8);
				*(dst+3)=HEX_LOW(us>>8);
#elif defined __IS_BIG_ENDIAN
				*(dst+2)=HEX_HI(us);
				*(dst+3)=HEX_LOW(us);
				*(dst+0)=HEX_HI(us>>8);
				*(dst+1)=HEX_LOW(us>>8);
#endif
				if ((end-src)==3){
					*(dst+4)=HEX_HI(*(src+2));
					*(dst+5)=HEX_LOW(*(src+2));
				}
				/* no need to inc anything */
				break;
			case 1:
				*dst=HEX_HI(*src);
				*(dst+1)=HEX_LOW(*src);
				/* no need to inc anything */
			case 0:
				break;
		}
	}
	
	return osize;
}



#else /* BASE16_READ_WHOLE_INTS */


/*!
 * \return : size used from the output buffer (dst) on success,
 *          -size_needed on error
 *
 * \note WARNING: the output string is not 0-term
 */
inline static int base16_enc(unsigned char* src, int slen,
							 unsigned char*  dst, int dlen)
{
	unsigned char* end;
	int osize;
	
	osize=2*slen;
	if (unlikely(dlen<osize))
		return -osize;
	end=src+slen;
	for (;src<end; src++,dst+=2){
		*dst=HEX_HI(*src);
		*(dst+1)=HEX_LOW(*src);
	}
	return osize;
}


#endif /* BASE16_READ_WHOLE_INTS */

inline static int base16_dec(unsigned char* src, int slen, unsigned char* dst, int dlen)
{
	unsigned char* end;
	int osize;
	
	osize=slen/2;
	if (unlikely(dlen<osize))
		return -osize;
	end=src+2*osize;
	for (; src<end; src+=2, dst++)
		*dst=(UNHEX(*src)<<4) | UNHEX(*(src+1));
	return osize;
}





/*! \brief helper internal function: encodes v (6 bits value)
 * \return char ascii encoding on success and 0xff on error
 * (value out of range) */
inline static unsigned char base64_enc_char(unsigned char v)
{
	switch(v){
		case 0x3f:
			return '/';
		case 0x3e:
			return '+';
		default:
			if (v<=25)
				return v+'A';
			else if (v<=51)
				return v-26+'a';
			else if (v<=61)
				return v-52+'0';
	}
	return 0xff;
}

/*! \brief helper internal function: decodes a base64 "digit",
 * \return value on success (0-63) and 0xff on error (invalid)*/
inline static unsigned base64_dec_char(unsigned char v)
{
	switch(v){
		case '/':
			return 0x3f;
		case '+':
			return 0x3e;
		case ':':
		case ';':
		case '<':
		case '=':
		case '>':
		case '?':
		case '@':
		case '[':
		case '\\':
		case ']':
		case '^':
		case '_':
		case '`':
			return 0xff;
		default:
			if ((v)<'0')
				return 0xff;
			if ((v)<='9')
				return (v)-'0'+0x34;
			else if ((v)<='Z')
				return (v)-'A';
			else if ((v) <='z')
				return (v)-'a'+0x1a;
	}
	return 0xff;
}


#ifdef BASE64_LOOKUP_8K
/*!
 * \return : size used from the output buffer (dst) on success ((slen+2)/3*4)
 *          -size_needed on error
 *
 * \note WARNING: the output string is not 0-term
 */
inline static int base64_enc(unsigned char* src, int slen,
							unsigned char* dst,  int dlen)
{
	unsigned char* end;
	int osize;
	
	osize=(slen+2)/3*4;
	if (unlikely(dlen<osize))
		return -osize;
	end=src+slen/3*3;
	if (unlikely((long)dst%2)){
		for (;src<end; src+=3,dst+=4){
			dst[0]=FIRST_8B(BASE64_12(src[0], src[1]));
			dst[1]=LAST_8B(BASE64_12(src[0], src[1]));
			dst[2]=FIRST_8B(BASE64_34(src[1], src[2]));
			dst[3]=LAST_8B(BASE64_34(src[1], src[2]));
		}
		switch(slen%3){
			case 2:
				dst[0]=FIRST_8B(BASE64_12(src[0], src[1]));
				dst[1]=LAST_8B(BASE64_12(src[0], src[1]));
				dst[2]=FIRST_8B(BASE64_34(src[1], 0));
				dst[3]='=';
				break;
			case 1:
				dst[0]=FIRST_8B(BASE64_12(src[0], 0));
				dst[1]=LAST_8B(BASE64_12(src[0], 0));
				dst[2]='=';
				dst[3]='=';
				break;
		}
	}else{
		for (;src<end; src+=3,dst+=4){
			*(unsigned short*)(dst+0)=_bx_b64_12[(src[0]<<4)|(src[1]>>4)];
			*(unsigned short*)(dst+2)=_bx_b64_12[((src[1]&0xf)<<8)|src[2]];
		}
		switch(slen%3){
			case 2:
				*(unsigned short*)(dst+0)=_bx_b64_12[(src[0]<<4)|(src[1]>>4)];
				*(unsigned short*)(dst+2)=_bx_b64_12[((src[1]&0xf)<<8)|0];
				dst[3]='=';
				break;
			case 1:
				*(unsigned short*)(dst+0)=_bx_b64_12[(src[0]<<4)|0];
				dst[2]='=';
				dst[3]='=';
				break;
		}
	}
	return osize;
}
#else /*BASE64_LOOKUP_8K*/
/*! \brief Convert to base64
 * \return size used from the output buffer (dst) on success ((slen+2)/3*4)
 *          -size_needed on error
 * \note WARNING: the output string is not 0-term
 */
inline static int base64_enc(unsigned char* src, int slen,
							unsigned char* dst,  int dlen)
{
	unsigned char* end;
	int osize;
	
	osize=(slen+2)/3*4;
	if (unlikely(dlen<osize))
		return -osize;
	end=src+slen/3*3;
	for (;src<end; src+=3,dst+=4){
		dst[0]=BASE64_1(src[0]);
		dst[1]=BASE64_2(src[0], src[1]);
		dst[2]=BASE64_3(src[1], src[2]);
		dst[3]=BASE64_4(src[2]);
	}
	switch(slen%3){
		case 2:
			dst[0]=BASE64_1(src[0]);
			dst[1]=BASE64_2(src[0], src[1]);
			dst[2]=BASE64_3(src[1], 0);
			dst[3]='=';
			break;
		case 1:
			dst[0]=BASE64_1(src[0]);
			dst[1]=BASE64_2(src[0], 0);
			dst[2]='=';
			dst[3]='=';
			break;
	}
	return osize;
}
#endif /*BASE64_LOOKUP_8K*/



/*! \brief
 * \return size used from the output buffer (dst) on success (max: slen/4*3)
 *          -size_needed on error or 0 on bad base64 encoded string
 * \note WARNING: the output string is not 0-term
 */
inline static int base64_dec(unsigned char* src, int slen,
							unsigned char* dst,  int dlen)
{
	
	unsigned char* end;
	int osize;
	register unsigned a, b, c, d; /* more registers used, but allows for
									 paralles execution */
	
	if (unlikely((slen<4) || (slen%4) || 
				(src[slen-2]=='=' && src[slen-1]!='=')))
		return 0; /* invalid base64 enc. */
	osize=(slen/4*3)-(src[slen-2]=='=')-(src[slen-1]=='=');
	if (unlikely(dlen<osize))
		return -osize;
	end=src+slen-4;
	for (;src<end; src+=4,dst+=3){
#if 0
		u=	(UNBASE64(src[0])<<18) | (UNBASE64(src[1])<<12) | 
			(UNBASE64(src[2])<<6)  |  UNBASE64(src[3]);
		dst[0]=u>>16;
		dst[1]=u>>8;
		dst[3]=u;
#endif
		a=UNBASE64(src[0]);
		b=UNBASE64(src[1]);
		c=UNBASE64(src[2]);
		d=UNBASE64(src[3]);
		dst[0]=(a<<2) | (b>>4);
		dst[1]=(b<<4) | (c>>2);
		dst[2]=(c<<6) | d;
	}
	switch(osize%3){
		case 0: /* no '=' => 3 output bytes at the end */
			a=UNBASE64(src[0]);
			b=UNBASE64(src[1]);
			c=UNBASE64(src[2]);
			d=UNBASE64(src[3]);
			dst[0]=(a<<2) | (b>>4);
			dst[1]=(b<<4) | (c>>2);
			dst[2]=(c<<6) | d;
			break;
		case 2: /* 1  '=' => 2 output bytes at the end */
			a=UNBASE64(src[0]);
			b=UNBASE64(src[1]);
			c=UNBASE64(src[2]);
			dst[0]=(a<<2) | (b>>4);
			dst[1]=(b<<4) | (c>>2);
			break;
		case 1: /* 2  '=' => 1 output byte at the end */
			a=UNBASE64(src[0]);
			b=UNBASE64(src[1]);
			dst[0]=(a<<2) | (b>>4);
			break;
	}
	return osize;
}




/*! \brief
 * same as \ref base64_enc() but with a different alphabet, that allows simpler and
 *  faster enc/dec
 * \return size used from the output buffer (dst) on success ((slen+2)/3*4)
 *          -size_needed on error
 * \note WARNING: the alphabet includes ":;<>?@[]\`", so it might not be suited
 *  in all cases (e.g. encoding something in a sip uri).
 */
inline static int q_base64_enc(unsigned char* src, int slen,
							unsigned char* dst,  int dlen)
{
#define q_b64_base	'0'
#define q_b64_pad	'z'
#define Q_BASE64(v)	(unsigned char)((v)+q_b64_base)
	unsigned char* end;
	int osize;
	
	osize=(slen+2)/3*4;
	if (unlikely(dlen<osize))
		return -osize;
	end=src+slen/3*3;
	for (;src<end; src+=3,dst+=4){
		dst[0]=Q_BASE64(src[0]>>2);
		dst[1]=(Q_BASE64((src[0]<<4)&0x3f) | (src[1]>>4));
		dst[2]=(Q_BASE64((src[1]<<2)&0x3f) | (src[2]>>6) );
		dst[3]=Q_BASE64(src[2]&0x3f);
	}
	switch(slen%3){
		case 2:
			dst[0]=Q_BASE64(src[0]>>2);
			dst[1]=(Q_BASE64((src[0]<<4)&0x3f) | (src[1]>>4));
			dst[2]=Q_BASE64((src[1]<<2)&0x3f);
			dst[3]=q_b64_pad;
			break;
		case 1:
			dst[0]=Q_BASE64(src[0]>>2);
			dst[1]=Q_BASE64((src[0]<<4)&0x3f);
			dst[2]=q_b64_pad;
			dst[3]=q_b64_pad;
			break;
	}
	return osize;
#undef Q_BASE64
}



/*! \brief
 * same as \ref base64_enc() but with a different alphabet, that allows simpler and
 *  faster enc/dec
 *
 * \return size used from the output buffer (dst) on success (max: slen/4*3)
 *          -size_needed on error or 0 on bad base64 encoded string
 * \note WARNING: the output string is not 0-term
 */
inline static int q_base64_dec(unsigned char* src, int slen,
							unsigned char* dst,  int dlen)
{
#define Q_UNBASE64(v) (unsigned char)((v)-q_b64_base)
	
	unsigned char* end;
	int osize;
#ifdef SINGLE_REG
	register unsigned u;
#else
	register unsigned a, b, c, d; /* more registers used, but allows for
									 paralles execution */
#endif
	
	if (unlikely((slen<4) || (slen%4) || 
				(src[slen-2]==q_b64_pad && src[slen-1]!=q_b64_pad)))
		return 0; /* invalid base64 enc. */
	osize=(slen/4*3)-(src[slen-2]==q_b64_pad)-(src[slen-1]==q_b64_pad);
	if (unlikely(dlen<osize))
		return -osize;
	end=src+slen-4;
	for (;src<end; src+=4,dst+=3){
#ifdef SINGLE_REG
		u=	(Q_UNBASE64(src[0])<<18) | (Q_UNBASE64(src[1])<<12) | 
			(Q_UNBASE64(src[2])<<6)  |  Q_UNBASE64(src[3]);
		dst[0]=u>>16;
		dst[1]=u>>8;
		dst[2]=u;
#else
		a=Q_UNBASE64(src[0]);
		b=Q_UNBASE64(src[1]);
		c=Q_UNBASE64(src[2]);
		d=Q_UNBASE64(src[3]);
		dst[0]=(a<<2) | (b>>4);
		dst[1]=(b<<4) | (c>>2);
		dst[2]=(c<<6) | d;
#endif
	}
	switch(osize%3){
		case 0: /* no '=' => 3 output bytes at the end */
#ifdef SINGLE_REG
			u=	(Q_UNBASE64(src[0])<<18) | (Q_UNBASE64(src[1])<<12) | 
				(Q_UNBASE64(src[2])<<6)  |  Q_UNBASE64(src[3]);
			dst[0]=u>>16;
			dst[1]=u>>8;
			dst[2]=u;
#else
			a=Q_UNBASE64(src[0]);
			b=Q_UNBASE64(src[1]);
			c=Q_UNBASE64(src[2]);
			d=Q_UNBASE64(src[3]);
			dst[0]=(a<<2) | (b>>4);
			dst[1]=(b<<4) | (c>>2);
			dst[2]=(c<<6) | d;
#endif
			break;
		case 2: /* 1  '=' => 2 output bytes at the end */
#ifdef SINGLE_REG
			u=	(Q_UNBASE64(src[0])<<12) | (Q_UNBASE64(src[1])<<6) | 
				(Q_UNBASE64(src[2]));
			dst[0]=u>>10;
			dst[1]=u>>2;
#else
			a=Q_UNBASE64(src[0]);
			b=Q_UNBASE64(src[1]);
			c=Q_UNBASE64(src[2]);
			dst[0]=(a<<2) | (b>>4);
			dst[1]=(b<<4) | (c>>2);
#endif
			break;
		case 1: /* 2  '=' => 1 output byte at the end */
#ifdef SINGLE_REG
			dst[0]=(Q_UNBASE64(src[0])<<2) | (Q_UNBASE64(src[1])>>4); 
#else
			a=Q_UNBASE64(src[0]);
			b=Q_UNBASE64(src[1]);
			dst[0]=(a<<2) | (b>>4);
#endif
			break;
	}
	return osize;
#undef q_b64_base
#undef q_b64_pad
}

/*! \brief inits internal lookup tables */
int init_basex(void);


#endif /* _basex_h */
