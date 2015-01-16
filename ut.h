/*
 * - various general purpose functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 */
/** Kamailio core :: various general purpose/helper functions.
 * @file
 * @ingroup core
 */


#ifndef ut_h
#define ut_h

#include "comp_defs.h"

#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>

#include "compiler_opt.h"
#include "config.h"
#include "dprint.h"
#include "str.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"



/* zero-string wrapper */
#define ZSW(_c) ((_c)?(_c):"")

/* returns string beginning and length without insignificant chars */
#define trim_len( _len, _begin, _mystr ) \
	do{ 	static char _c; \
		(_len)=(_mystr).len; \
		while ((_len) && ((_c=(_mystr).s[(_len)-1])==0 || _c=='\r' || \
					_c=='\n' || _c==' ' || _c=='\t' )) \
			(_len)--; \
		(_begin)=(_mystr).s; \
		while ((_len) && ((_c=*(_begin))==' ' || _c=='\t')) { \
			(_len)--;\
			(_begin)++; \
		} \
	}while(0)

#define trim_r( _mystr ) \
	do{	static char _c; \
		while( ((_mystr).len) && ( ((_c=(_mystr).s[(_mystr).len-1]))==0 ||\
									_c=='\r' || _c=='\n' ) \
				) \
			(_mystr).len--; \
	}while(0)


#define  translate_pointer( _new_buf , _org_buf , _p) \
	( (_p)?(_new_buf + (_p-_org_buf)):(0) )

#define via_len(_via) \
	((_via)->bsize-((_via)->name.s-\
		((_via)->hdr.s+(_via)->hdr.len)))



/* rounds to sizeof(type), but type must have a 2^k size (e.g. short, int,
 * long, void*) */
#define ROUND2TYPE(s, type) \
	(((s)+(sizeof(type)-1))&(~(sizeof(type)-1)))


/* rounds to sizeof(char*) - the first 4 byte multiple on 32 bit archs
 * and the first 8 byte multiple on 64 bit archs */
#define ROUND_POINTER(s) ROUND2TYPE(s, char*)

/* rounds to sizeof(long) - the first 4 byte multiple on 32 bit archs
 * and the first 8 byte multiple on 64 bit archs  (equiv. to ROUND_POINTER)*/
#define ROUND_LONG(s)  ROUND2TYPE(s, long)

/* rounds to sizeof(int) - the first t byte multiple on 32 and 64  bit archs */
#define ROUND_INT(s) ROUND2TYPE(s, int)

/* rounds to sizeof(short) - the first 2 byte multiple */
#define ROUND_SHORT(s) ROUND2TYPE(s, short)


/* params: v - either a variable name, structure member or a type
 * returns an unsigned long containing the maximum possible value that will
 * fit in v, if v is unsigned or converted to an unsigned version
 * example: MAX_UVAR_VALUE(unsigned short); MAX_UVAR_VALUE(i);
 *  MAX_UVAR_VALUE(((struct foo*)0)->bar) */
#define MAX_UVAR_VALUE(v) \
	(((unsigned long)(-1))>>((sizeof(unsigned long)-sizeof(v))*8UL))


#define MIN_int(a, b) (((a)<(b))?(a):(b))
#define MAX_int(a, b) (((a)>(b))?(a):(b))

#define MIN_unsigned(a, b) (unsigned)(((unsigned)(a)<(unsigned)(b))?(a):(b))
#define MAX_unsigned(a, b) (unsigned)(((unsigned)(a)>(unsigned)(b))?(a):(b))

#if 0
#define MIN_int(a, b) ((b)+(((a)-(b))& -((a)<(b))))
#define MAX_int(a, b) ((a)-(((a)-(b))& -((b)>(a))))

/* depend on signed right shift result which depends on the compiler */
#define MIN_int(a, b) ((b)+(((a)-(b))&(((a)-(b))>>(sizeof(int)*8-1))))
#define MAX_int(a, b) ((a)-(((a)-(b))&(((a)-(b))>>(sizeof(int)*8-1))))
#endif


#define append_str(_dest,_src,_len)				\
	do{											\
		memcpy( (_dest) , (_src) , (_len) );	\
		(_dest) += (_len) ;						\
	}while(0);									\

	
/*! append _c char to _dest string */
#define append_chr(_dest,_c) \
	*((_dest)++) = _c;


#define is_in_str(p, in) (p < in->s + in->len && *p)


/* links a value to a msgid */
struct msgid_var{
	union{
		char char_val;
		int int_val;
		long long_val;
	}u;
	unsigned int msgid;
};

/* return the value or 0 if the msg_id doesn't match */
#define get_msgid_val(var, id, type)\
	((type)((type)((var).msgid!=(id))-1)&((var).u.type##_val))

#define set_msgid_val(var, id, type, value)\
	do{\
		(var).msgid=(id); \
		(var).u.type##_val=(value); \
	}while(0)

/* char to hex conversion table */
static char fourbits2char[16] = { '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };


/* converts a str to an u. short, returns the u. short and sets *err on
 * error and if err!=null
  */
static inline unsigned short str2s(const char* s, unsigned int len,
									int *err)
{
	unsigned short ret;
	int i;
	unsigned char *limit;
	unsigned char* str;

	/*init*/
	str=(unsigned char*)s;
	ret=i=0;
	limit=str+len;

	for(;str<limit ;str++){
		if ( (*str <= '9' ) && (*str >= '0') ){
				ret=ret*10+*str-'0';
				i++;
				if (i>5) goto error_digits;
		}else{
				/* error unknown char */
				goto error_char;
		}
	}
	if (err) *err=0;
	return ret;

error_digits:
	if (err) *err=1;
	return 0;
error_char:
	if (err) *err=1;
	return 0;
}



static inline int btostr( char *p,  unsigned char val)
{
	unsigned int a,b,i =0;

	if ( (a=val/100)!=0 )
		*(p+(i++)) = a+'0';         /*first digit*/
	if ( (b=val%100/10)!=0 || a)
		*(p+(i++)) = b+'0';        /*second digit*/
	*(p+(i++)) = '0'+val%10;              /*third digit*/

	return i;
}


#define INT2STR_MAX_LEN  (19+1+1+1) /* 2^64~= 16*10^18 =>
									   19+1 digits + sign + \0 */

/* 
 * returns a pointer to a static buffer containing l in asciiz (with base "base") & sets len 
 * left padded with 0 to "size"
 */
static inline char* int2str_base_0pad(unsigned int l, int* len, int base, 
											int size)
{
	static char r[INT2STR_MAX_LEN];
	int i, j;

	if (base < 2) {
		BUG("base underflow\n");
		return NULL;
	}
	if (base > 36) {
		BUG("base overflow\n");
		return NULL;
	}
	i=INT2STR_MAX_LEN-2;
	j=i-size;
	r[INT2STR_MAX_LEN-1]=0; /* null terminate */
	do{
		r[i]=l%base;
		if (r[i]<10)
		        r[i]+='0';
		else
		        r[i]+='a'-10;
		i--;
		l/=base;
	}while((l || i>j) && (i>=0));
	if (l && (i<0)){
		BUG("result buffer overflow\n");
	}
	if (len) *len=(INT2STR_MAX_LEN-2)-i;
	return &r[i+1];
}

/* returns a pointer to a static buffer containing l in asciiz (with base "base") & sets len */
static inline char* int2str_base(unsigned int l, int* len, int base)
{
        return int2str_base_0pad(l, len, base, 0);
}



/** unsigned long to str conversion using a provided buffer.
 * Converts/prints an unsigned long to a string. The result buffer must be
 * provided  and its length must be at least INT2STR_MAX_LEN.
 * @param l - unsigned long to be converted
 * @param r - pointer to result buffer
 * @param r_size - result buffer size, must be at least INT2STR_MAX_LEN.
 * @param *len - length of the written string, _without_ the terminating 0.
 * @return  pointer _inside_ r, to the converted string (note: the string
 *  is written from the end of the buffer and not from the start and hence
 *  the returned pointer will most likely not be equal to r). In case of error
 *  it returns 0 (the only error being insufficient provided buffer size).
 */
static inline char* int2strbuf(unsigned long l, char *r, int r_size, int* len)
{
	int i;

	if(unlikely(r_size<INT2STR_MAX_LEN)) {
		if (len)
			*len = 0;
		return 0; /* => if someone misuses it => crash (feature no. 1) */
	}
	i=INT2STR_MAX_LEN-2;
	r[INT2STR_MAX_LEN-1]=0; /* null terminate */
	do{
		r[i]=l%10+'0';
		i--;
		l/=10;
	}while(l && (i>=0));
	if (l && (i<0)){
		LM_CRIT("overflow\n");
	}
	if (len) *len=(INT2STR_MAX_LEN-2)-i;
	return &r[i+1];
}

extern char ut_buf_int2str[INT2STR_MAX_LEN];
/** interger(long) to string conversion.
 * This version uses a static buffer (shared with sint2str()).
 * WARNING: other function calls might overwrite the static buffer, so
 * either always save the result immediately or use int2strbuf(...).
 * @param l - unsigned long to be converted/printed.
 * @param *len - will be filled with the final length (without the terminating
 *   0).
 * @return a pointer to a static buffer containing l in asciiz & sets len.
 */
static inline char* int2str(unsigned long l, int* len)
{
	return int2strbuf(l, ut_buf_int2str, INT2STR_MAX_LEN, len);
}



/** signed long to str conversion using a provided buffer.
 * Converts a long to a signed string. The result buffer must be provided
 * and its length must be at least INT2STR_MAX_LEN.
 * @param l - long to be converted
 * @param r - pointer to result buffer
 * @param r_size - result buffer size, must be at least INT2STR_MAX_LEN.
 * @param *len - length of the written string, _without_ the terminating 0.
 * @return  pointer _inside_ r, to the converted string (note: the string
 *  is written from the end of the buffer and not from the start and hence
 *  the returned pointer will most likely not be equal to r). In case of error
 *  it returns 0 (the only error being insufficient provided buffer size).
 */
static inline char* sint2strbuf(long l, char* r, int r_size, int* len)
{
	int sign;
	char *p;
	int p_len;

	sign = 0;
	if(l<0) {
		sign = 1;
		l = -l;
	}
	p = int2strbuf((unsigned long)l, r, r_size, &p_len);
	if(sign && p_len<(r_size-1)) {
		*(--p) = '-';
		p_len++;;
	}
	if (likely(len))
		*len = p_len;
	return p;
}


/** Signed INTeger-TO-STRing: converts a long to a string.
 * This version uses a static buffer, shared with int2str().
 * WARNING: other function calls might overwrite the static buffer, so
 * either always save the result immediately or use sint2strbuf(...).
 * @param l - long to be converted/printed.
 * @param *len - will be filled with the final length (without the terminating
 *   0).
 * @return a pointer to a static buffer containing l in asciiz & sets len.
 */
static inline char* sint2str(long l, int* len)
{
	return sint2strbuf(l, ut_buf_int2str, INT2STR_MAX_LEN, len);
}



#define USHORT2SBUF_MAX_LEN  5 /* 65535*/
/* converts an unsigned short (16 bits) to asciiz
 * returns bytes written or 0 on error
 * the passed len must be at least USHORT2SBUF_MAX chars or error
 * would be returned.
 * (optimized for port conversion (4 or 5 digits most of the time)*/
static inline int ushort2sbuf(unsigned short u, char* buf, int len)
{
	int offs;
	unsigned char a, b, c, d;
	
	if (unlikely(len<USHORT2SBUF_MAX_LEN))
		return 0;
	offs=0;
	a=u/10000; u%=10000;
	buf[offs]=a+'0'; offs+=(a!=0);
	b=u/1000;  u%=1000;
	buf[offs]=b+'0'; offs+=((offs|b)!=0);
	c=u/100;   u%=100;
	buf[offs]=c+'0'; offs+=((offs|c)!=0);
	d=u/10;    u%=10;
	buf[offs]=d+'0'; offs+=((offs|d)!=0);
	buf[offs]=(unsigned char)u+'0';
	return offs+1;
}



#define USHORT2STR_MAX_LEN  (USHORT2SBUF_MAX_LEN+1) /* 65535\0*/
/* converts an unsigned short (16 bits) to asciiz
 * (optimized for port conversiob (4 or 5 digits most of the time)*/
static inline char* ushort2str(unsigned short u)
{
	static char buf[USHORT2STR_MAX_LEN];
	int len;

	len=ushort2sbuf(u, buf, sizeof(buf)-1);
	buf[len]=0;
	return buf;
}



/* fast memchr version */
static inline char* q_memchr(char* p, int c, unsigned int size)
{
	char* end;

	end=p+size;
	for(;p<end;p++){
		if (*p==(unsigned char)c) return p;
	}
	return 0;
}


/* fast reverse char search */

static inline char* q_memrchr(char* p, int c, unsigned int size)
{
	char* end;

	end=p+size-1;
	for(;end>=p;end--) {
		if (*end==(unsigned char)c) return end;
	}
	return 0;
}

/* returns -1 on error, 1! on success (consistent with int2reverse_hex) */
inline static int reverse_hex2int( char *c, int len, unsigned int* res)
{
	char *pc;
	char mychar;

	*res=0;
	for (pc=c+len-1; len>0; pc--, len--) {
		*res <<= 4 ;
		mychar=*pc;
		if ( mychar >='0' && mychar <='9') *res+=mychar -'0';
		else if (mychar >='a' && mychar <='f') *res+=mychar -'a'+10;
		else if (mychar  >='A' && mychar <='F') *res+=mychar -'A'+10;
		else return -1;
	}
	return 1;
}

inline static int int2reverse_hex( char **c, int *size, unsigned int nr )
{
	unsigned short digit;

	if (*size && nr==0) {
		**c = '0';
		(*c)++;
		(*size)--;
		return 1;
	}

	while (*size && nr ) {
		digit = nr & 0xf ;
		**c= digit >= 10 ? digit + 'a' - 10 : digit + '0';
		nr >>= 4;
		(*c)++;
		(*size)--;
	}
	return nr ? -1 /* number not processed; too little space */ : 1;
}

/* double output length assumed ; does NOT zero-terminate */
inline static int string2hex( 
	/* input */ unsigned char *str, int len,
	/* output */ char *hex )
{
	int orig_len;

	if (len==0) {
		*hex='0';
		return 1;
	}

	orig_len=len;
	while ( len ) {

		*hex=fourbits2char[(*str) >> 4];
		hex++;
		*hex=fourbits2char[(*str) & 0xf];
		hex++;
		len--;
		str++;

	}
	return orig_len-len;
}

/* portable sleep in microseconds (no interrupt handling now) */

inline static void sleep_us( unsigned int nusecs )
{
	struct timeval tval;
	tval.tv_sec =nusecs/1000000;
	tval.tv_usec=nusecs%1000000;
	select(0, NULL, NULL, NULL, &tval );
}


/* portable determination of max_path */
inline static int pathmax(void)
{
#ifdef PATH_MAX
	static int pathmax=PATH_MAX;
#else
	static int pathmax=0;
#endif
	if (pathmax==0) { /* init */
		pathmax=pathconf("/", _PC_PATH_MAX);
		pathmax=(pathmax<=0)?PATH_MAX_GUESS:pathmax+1;
	}
	return pathmax;
}

inline static int hex2int(char hex_digit)
{
	if (hex_digit>='0' && hex_digit<='9')
		return hex_digit-'0';
	if (hex_digit>='a' && hex_digit<='f')
		return hex_digit-'a'+10;
	if (hex_digit>='A' && hex_digit<='F')
		return hex_digit-'A'+10;
	/* no valid hex digit ... */
	LM_ERR("'%c' is no hex char\n", hex_digit );
	return -1;
}

/* Un-escape URI user  -- it takes a pointer to original user
   str, as well as the new, unescaped one, which MUST have
   an allocated buffer linked to the 'str' structure ;
   (the buffer can be allocated with the same length as
   the original string -- the output string is always
   shorter (if escaped characters occur) or same-long
   as the original one).

   only printable characters are permitted

	<0 is returned on an unescaping error, length of the
	unescaped string otherwise
*/
inline static int un_escape(str *user, str *new_user ) 
{
 	int i, j, value;
	int hi, lo;

	if( new_user==0 || new_user->s==0) {
		LM_CRIT("invalid param\n");
		return -1;
	}

	new_user->len = 0;
	j = 0;

	for (i = 0; i < user->len; i++) {
		if (user->s[i] == '%') {
			if (i + 2 >= user->len) {
				LM_ERR("escape sequence too short in '%.*s' @ %d\n",
					user->len, user->s, i );
				goto error;
			}
			hi=hex2int(user->s[i + 1]);
			if (hi<0) {
				LM_ERR("non-hex high digit in an escape sequence in"
					" '%.*s' @ %d\n",
					user->len, user->s, i+1 );
				goto error;
			}
			lo=hex2int(user->s[i + 2]);
			if (lo<0) {
				LM_ERR("non-hex low digit in an escape sequence in "
					"'%.*s' @ %d\n",
					user->len, user->s, i+2 );
				goto error;
			}
			value=(hi<<4)+lo;
			if (value < 32 || value > 126) {
				LM_ERR("non-ASCII escaped character in '%.*s' @ %d\n",
					user->len, user->s, i );
				goto error;
			}
			new_user->s[j] = value;
			i+=2; /* consume the two hex digits, for cycle will move to the next char */
		} else {
			new_user->s[j] = user->s[i];
		}
        j++; /* good -- we translated another character */
	}
	new_user->len = j;
	return j;

error:
	new_user->len = j;
	return -1;
} 


/*
 * Convert a string to lower case
 */
static inline void strlower(str* _s)
{
	int i;

	for(i = 0; i < _s->len; i++) {
		_s->s[i] = tolower(_s->s[i]);
	}
}


/*
 * Convert a str into integer
 */
static inline int str2int(str* _s, unsigned int* _r)
{
	int i;
	
	*_r = 0;
	for(i = 0; i < _s->len; i++) {
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			*_r *= 10;
			*_r += _s->s[i] - '0';
		} else {
			return -1;
		}
	}
	
	return 0;
}

/*
 * Convert an str to signed integer
 */
static inline int str2sint(str* _s, int* _r)
{
	int i;
	int sign;

	if (_s->len == 0) return -1;

	*_r = 0;
	sign = 1;
	i = 0;
	if (_s->s[0] == '+') {
		i++;
	} else if (_s->s[0] == '-') {
		sign = -1;
		i++;
	}
	for(; i < _s->len; i++) {
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			*_r *= 10;
			*_r += _s->s[i] - '0';
		} else {
			return -1;
		}
	}
	*_r *= sign;

	return 0;
}



#ifdef SHM_MEM
/**
 * \brief Make a copy of a str structure using shm_malloc
 * \param dst destination
 * \param src source
 * \return 0 on success, -1 on failure
 */
static inline int shm_str_dup(str* dst, const str* src)
{
	dst->s = (char*)shm_malloc(src->len);
	if (!dst->s) {
		SHM_MEM_ERROR;
		return -1;
	}

	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	return 0;
}
#endif /* SHM_MEM */



/**
 * \brief Make a copy of a str structure using pkg_malloc
 * \param dst destination
 * \param src source
 * \return 0 on success, -1 on failure
 */
static inline int pkg_str_dup(str* dst, const str* src)
{
	dst->s = (char*)pkg_malloc(src->len);
	if (dst->s==NULL)
	{
		PKG_MEM_ERROR;
		return -1;
	}

	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	return 0;
}

/**
 * \brief Compare two str's case sensitive
 * \param str1 first str
 * \param str2 second str
 * \return 0 if both are equal, positive if str1 is greater, negative if str2 is greater, -2 on errors
 */
static inline int str_strcmp(const str *str1, const str *str2)
{
	if(str1==NULL || str2==NULL || str1->s ==NULL || str2->s==NULL || str1->len<0 || str2->len<0)
	{
		LM_ERR("bad parameters\n");
		return -2;
	}

	if (str1->len < str2->len)
		return -1;
	else if (str1->len > str2->len)
		return 1;
	else
		return strncmp(str1->s, str2->s, str1->len);
}

/**
 * \brief Compare two str's case insensitive
 * \param str1 first str
 * \param str2 second str
 * \return 0 if both are equal, positive if str1 is greater, negative if str2 is greater, -2 on errors
 */
static inline int str_strcasecmp(const str *str1, const str *str2)
{
	if(str1==NULL || str2==NULL || str1->s ==NULL || str2->s==NULL || str1->len<0 || str2->len<0)
	{
		LM_ERR("bad parameters\n");
		return -2;
	}
	if (str1->len < str2->len)
		return -1;
	else if (str1->len > str2->len)
		return 1;
	else
		return strncasecmp(str1->s, str2->s, str1->len);
}

#ifndef MIN
#define	MIN(x, y)	((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define	MAX(x, y)	((x) > (y) ? (x) : (y))
#endif


/* INTeger-TO-Buffer-STRing : convers an unsigned long to a string 
 * IMPORTANT: the provided buffer must be at least INT2STR_MAX_LEN size !! */
static inline char* int2bstr(unsigned long l, char *s, int* len)
{
	int i;
	i=INT2STR_MAX_LEN-2;
	s[INT2STR_MAX_LEN-1]=0;
	/* null terminate */
	do{
		s[i]=l%10+'0';
		i--;
		l/=10;
	}while(l && (i>=0));
	if (l && (i<0)){
		LM_CRIT("overflow error\n");
	}
	if (len) *len=(INT2STR_MAX_LEN-2)-i;
	return &s[i+1];
}


inline static int hexstr2int(char *c, int len, unsigned int *val)
{
	char *pc;
	int r;
	char mychar;

	r=0;
	for (pc=c; pc<c+len; pc++) {
		r <<= 4 ;
		mychar=*pc;
		if ( mychar >='0' && mychar <='9') r+=mychar -'0';
		else if (mychar >='a' && mychar <='f') r+=mychar -'a'+10;
		else if (mychar  >='A' && mychar <='F') r+=mychar -'A'+10;
		else return -1;
	}
	*val = r;
	return 0;
}



/*
 * Convert a str (base 10 or 16) into integer
 */
static inline int strno2int( str *val, unsigned int *mask )
{
	/* hexa or decimal*/
	if (val->len>2 && val->s[0]=='0' && val->s[1]=='x') {
		return hexstr2int( val->s+2, val->len-2, mask);
	} else {
		return str2int( val, mask);
	}
}

/* converts a username into uid:gid,
 * returns -1 on error & 0 on success */
int user2uid(int* uid, int* gid, char* user);

/* converts a group name into a gid
 * returns -1 on error, 0 on success */
int group2gid(int* gid, char* group);

/*
 * Replacement of timegm (does not exists on all platforms
 * Taken from 
 * http://lists.samba.org/archive/samba-technical/2002-November/025737.html
 */
time_t _timegm(struct tm* t);

/* Convert time_t value that is relative to local timezone to UTC */
time_t local2utc(time_t in);

/* Convert time_t value in UTC to to value relative to local time zone */
time_t utc2local(time_t in);

/*
 * Return str as zero terminated string allocated
 * using pkg_malloc
 */
char* as_asciiz(str* s);


/* return system version (major.minor.minor2) as
 *  (major<<16)|(minor)<<8|(minor2)
 * (if some of them are missing, they are set to 0)
 * if the parameters are not null they are set to the coresp. part */
unsigned int get_sys_version(int* major, int* minor, int* minor2);

/** Converts relative pathnames to absolute pathnames. This function returns
 * the full pathname of a file in parameter. If the file pathname does not
 * start with / then it will be converted into an absolute pathname. The
 * function gets the absolute directory pathname from \c base and appends \c
 * file to it. The first parameter can be NULL, in this case the function will
 * use the location of the main SER configuration file as reference.
 * @param base filename to be used as reference when \c file is relative. It
 *             must be absolute. The location of the SER configuration file
 *             will be used as reference if you set the value of this
 *             parameter to NULL.
 * @param file A pathname to be converted to absolute.
 * @return A string containing absolute pathname, the string must be freed
 * with free. NULL on error.
 */
char* get_abs_pathname(str* base, str* file);

/**
 * search for needle in text
 */
char *str_search(str *text, str *needle);

/*
 * ser_memmem() returns the location of the first occurrence of data
 * pattern b2 of size len2 in memory block b1 of size len1 or
 * NULL if none is found. Obtained from NetBSD.
 */
void * ser_memmem(const void *b1, const void *b2, size_t len1, size_t len2);

/*
 * ser_memrmem() returns the location of the last occurrence of data
 * pattern b2 of size len2 in memory block b1 of size len1 or
 * NULL if none is found.
 */
void * ser_memrmem(const void *b1, const void *b2, size_t len1, size_t len2);

#endif
