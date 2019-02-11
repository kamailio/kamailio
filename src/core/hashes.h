/*
 * Copyright (C) 2006 iptelorg GmbH 
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
* \brief Kamailio core :: hash support
* \author Andrei
* \ingroup core
* Module: \ref core
*/


#ifndef _hashes_h
#define _hashes_h

#include "str.h"



/** internal use: hash update
 * params: char* s   - string start,
 *         char* end - end
 *         char* p,  and unsigned v temporary vars (used)
 *         unsigned h - result
 * h should be initialized (e.g. set it to 0), the result in h */
#define hash_update_str(s, end, p, v, h) \
	do{ \
		for ((p)=(s); (p)<=((end)-4); (p)+=4){ \
			(v)=(*(p)<<24)+((p)[1]<<16)+((p)[2]<<8)+(p)[3]; \
			(h)+=(v)^((v)>>3); \
		} \
		switch((end)-(p)){\
			case 3: \
				(v)=(*(p)<<16)+((p)[1]<<8)+(p)[2]; break; \
			case 2: \
				(v)=(*(p)<<8)+p[1]; break; \
			case 1: \
				(v)=*p; break; \
			default: \
				(v)=0; break; \
		} \
		(h)+=(v)^((v)>>3); \
	}while(0)

/** like hash_update_str, but case insensitive 
 * params: char* s   - string start,
 *         char* end - end
 *         char* p,  and unsigned v temporary vars (used)
 *         unsigned h - result
 * h should be initialized (e.g. set it to 0), the result in h */
#define hash_update_case_str(s, end, p, v, h) \
	do{ \
		for ((p)=(s); (p)<=((end)-4); (p)+=4){ \
			(v)=((*(p)<<24)+((p)[1]<<16)+((p)[2]<<8)+(p)[3])|0x20202020; \
			(h)+=(v)^((v)>>3); \
		} \
		(v)=0; \
		for (;(p)<(end); (p)++){ (v)<<=8; (v)+=*(p)|0x20;} \
		(h)+=(v)^((v)>>3); \
	}while(0)


/** internal use: call it to adjust the h from hash_update_str */
#define hash_finish(h) (((h)+((h)>>11))+(((h)>>13)+((h)>>23)))



/** "raw" 2 strings hash
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash2_raw(const str* key1, const str* key2)
{
	char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_str(key1->s, key1->s+key1->len, p, v, h);
	hash_update_str(key2->s, key2->s+key2->len, p, v, h);
	return hash_finish(h);
}



/** "raw" 1 string hash
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash1_raw(const char* s, int len)
{
	const char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_str(s, s+len, p, v, h);
	return hash_finish(h);
}



/** a little slower than hash_* , but better distribution for 
 * numbers and about the same for strings */
#define hash_update_str2(s, end, p, v, h) \
	do{ \
		for ((p)=(s); (p)<=((end)-4); (p)+=4){ \
			(v)=(*(p)*16777213)+((p)[1]*65537)+((p)[2]*257)+(p)[3]; \
			(h)=16777259*(h)+((v)^((v)<<17)); \
		} \
		(v)=0; \
		for (;(p)<(end); (p)++){ (v)*=251; (v)+=*(p);} \
		(h)=16777259*(h)+((v)^((v)<<17)); \
	}while(0)

/**  like hash_update_str2 but case insensitive */
#define hash_update_case_str2(s, end, p, v, h) \
	do{ \
		for ((p)=(s); (p)<=((end)-4); (p)+=4){ \
			(v)=((*(p)|0x20)*16777213)+(((p)[1]|0x20)*65537)+\
				(((p)[2]|0x20)*257)+((p)[3]|0x20); \
			(h)=16777259*(h)+((v)^((v)<<17)); \
		} \
		(v)=0; \
		for (;(p)<(end); (p)++){ (v)*=251; (v)+=*(p)|0x20;} \
		(h)=16777259*(h)+((v)^((v)<<17)); \
	}while(0)

/** internal use: call it to adjust the h from hash_update_str */
#define hash_finish2(h) (((h)+((h)>>7))+(((h)>>13)+((h)>>23)))



/** a little slower than get_hash1_raw() , but better distribution for 
 * numbers and about the same for strings */
inline static unsigned int get_hash1_raw2(const char* s, int len)
{
	const char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_str2(s, s+len, p, v, h);
	return hash_finish2(h);
}



/* "raw" 2 strings hash optimized for numeric strings (see above)
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash2_raw2(const str* key1, const str* key2)
{
	char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_str2(key1->s, key1->s+key1->len, p, v, h);
	hash_update_str2(key2->s, key2->s+key2->len, p, v, h);
	return hash_finish2(h);
}



/* "raw" 2 strings case insensitive hash (like get_hash2_raw but case 
 * insensitive)
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash2_case_raw(const str* key1, const str* key2)
{
	char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_case_str(key1->s, key1->s+key1->len, p, v, h);
	hash_update_case_str(key2->s, key2->s+key2->len, p, v, h);
	return hash_finish(h);
}



/* "raw" 1 string case insensitive hash
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash1_case_raw(const char* s, int len)
{
	const char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_case_str(s, s+len, p, v, h);
	return hash_finish(h);
}


/* same as get_hash1_raw2, but case insensitive and slower
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash1_case_raw2(const char* s, int len)
{
	const char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_case_str2(s, s+len, p, v, h);
	return hash_finish2(h);
}



/* "raw" 2 strings hash optimized for numeric strings (see above)
 * same as get_hash2_raw2 but case insensitive and slower
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash2_case_raw2(const str* key1,
											   const str* key2)
{
	char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_case_str2(key1->s, key1->s+key1->len, p, v, h);
	hash_update_case_str2(key2->s, key2->s+key2->len, p, v, h);
	return hash_finish2(h);
}


/*
 * generic hashing - from the intial origins of ser
 */
#define ch_h_inc h+=v^(v>>3)
#define ch_icase(_c) (((_c)>='A'&&(_c)<='Z')?((_c)|0x20):(_c))

/*
 * case sensitive hashing
 * - s1 - str to hash
 * - s2 - optional - continue hashing over s2
 * - size - optional - size of hash table (must be power of 1); if set (!=0),
 *   instead of hash id, returned value is slot index
 * return computed hash id or hash table slot index
 */
static inline unsigned int core_hash(const str *s1, const str *s2,
		const unsigned int size)
{
	char *p, *end;
	register unsigned v;
	register unsigned h;

	h=0;

	end=s1->s+s1->len;
	for ( p=s1->s ; p<=(end-4) ; p+=4 ){
		v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
		ch_h_inc;
	}
	v=0;
	for (; p<end ; p++){ v<<=8; v+=*p;}
	ch_h_inc;

	if (s2) {
		end=s2->s+s2->len;
		for (p=s2->s; p<=(end-4); p+=4){
			v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
			ch_h_inc;
		}
		v=0;
		for (; p<end ; p++){ v<<=8; v+=*p;}
		ch_h_inc;
	}
	h=((h)+(h>>11))+((h>>13)+(h>>23));
	return size?((h)&(size-1)):h;
}


/*
 * case insensitive hashing
 * - s1 - str to hash
 * - s2 - optional - continue hashing over s2
 * - size - optional - size of hash table (must be power of 1); if set (!=0),
 *   instead of hash id, returned value is slot index
 * return computed hash id or hash table slot index
 */
static inline unsigned int core_case_hash( str *s1, str *s2,
		unsigned int size)
{
	char *p, *end;
	register unsigned v;
	register unsigned h;

	h=0;

	end=s1->s+s1->len;
	for ( p=s1->s ; p<=(end-4) ; p+=4 ){
		v=(ch_icase(*p)<<24)+(ch_icase(p[1])<<16)+(ch_icase(p[2])<<8)
			+ ch_icase(p[3]);
		ch_h_inc;
	}
	v=0;
	for (; p<end ; p++){ v<<=8; v+=ch_icase(*p);}
	ch_h_inc;

	if (s2) {
		end=s2->s+s2->len;
		for (p=s2->s; p<=(end-4); p+=4){
			v=(ch_icase(*p)<<24)+(ch_icase(p[1])<<16)+(ch_icase(p[2])<<8)
				+ ch_icase(p[3]);
			ch_h_inc;
		}
		v=0;
		for (; p<end ; p++){ v<<=8; v+=ch_icase(*p);}
		ch_h_inc;
	}
	h=((h)+(h>>11))+((h>>13)+(h>>23));
	return size?((h)&(size-1)):h;
}


#endif
