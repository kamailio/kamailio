/*
 *$Id$
 *
 * - various general purpose functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History
 * ------
 * 2003-01-18 un_escape function introduced for convenience of code needing
 *            the complex&slow feature of unescaping
 * 2003-01-28 scratchpad removed (jiri)
 * 2003-01-29 pathmax added (jiri)
 * 2003-02-13 strlower added (janakj)
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-03-30 str2int and str2float added (janakj)
 * 2003-04-26 ZSW (jiri)
 * 2004-03-08 updated int2str (64 bits, INT2STR_MAX_LEN used) (andrei)
 * 2005-11-29 reverse_hex2int/int2reverse_hex switched to unsigned int (andrei)
 * 2005-12-09 added msgid_var (andrei)
 */


#ifndef ut_h
#define ut_h

#include "comp_defs.h"

#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#include "config.h"
#include "dprint.h"
#include "str.h"



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
	(type)((type)((var).msgid!=(id))-1)&((var).u.type##_val)

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
	unsigned char *init;
	unsigned char* str;

	/*init*/
	str=(unsigned char*)s;
	ret=i=0;
	limit=str+len;
	init=str;

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
	/*DBG("str2s: ERROR: too many letters in [%.*s]\n", (int)len, init); */
	if (err) *err=1;
	return 0;
error_char:
	/*DBG("str2s: ERROR: unexpected char %c in %.*s\n", *str, (int)len, init);
	 * */
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


#define INT2STR_MAX_LEN  (19+1+1) /* 2^64~= 16*10^18 => 19+1 digits + \0 */

/* 
 * returns a pointer to a static buffer containing l in asciiz (with base "base") & sets len 
 * left padded with 0 to "size"
 */
static inline char* int2str_base_0pad(unsigned int l, int* len, int base, int size)
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



/* returns a pointer to a static buffer containing l in asciiz & sets len */
static inline char* int2str(unsigned int l, int* len)
{
	static char r[INT2STR_MAX_LEN];
	int i;
	
	i=INT2STR_MAX_LEN-2;
	r[INT2STR_MAX_LEN-1]=0; /* null terminate */
	do{
		r[i]=l%10+'0';
		i--;
		l/=10;
	}while(l && (i>=0));
	if (l && (i<0)){
		LOG(L_CRIT, "BUG: int2str: overflow\n");
	}
	if (len) *len=(INT2STR_MAX_LEN-2)-i;
	return &r[i+1];
}



/* faster memchr version */
static inline char* q_memchr(char* p, int c, unsigned int size)
{
	char* end;

	end=p+size;
	for(;p<end;p++){
		if (*p==(unsigned char)c) return p;
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
inline static int pathmax()
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
	LOG(L_ERR, "ERROR: hex2int: '%c' is no hex char\n", hex_digit );
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
		LOG(L_CRIT, "BUG: un_escape: called with invalid param\n");
		return -1;
	}

	new_user->len = 0;
	j = 0;

	for (i = 0; i < user->len; i++) {
		if (user->s[i] == '%') {
			if (i + 2 >= user->len) {
				LOG(L_ERR, "ERROR: un_escape: escape sequence too short in"
					" '%.*s' @ %d\n",
					user->len, user->s, i );
				goto error;
			}
			hi=hex2int(user->s[i + 1]);
			if (hi<0) {
				LOG(L_ERR, "ERROR: un_escape: non-hex high digit in an escape sequence in"
					" '%.*s' @ %d\n",
					user->len, user->s, i+1 );
				goto error;
			}
			lo=hex2int(user->s[i + 2]);
			if (lo<0) {
				LOG(L_ERR, "ERROR: non-hex low digit in an escape sequence in "
					"'%.*s' @ %d\n",
					user->len, user->s, i+2 );
				goto error;
			}
			value=(hi<<4)+lo;
			if (value < 32 || value > 126) {
				LOG(L_ERR, "ERROR: non-ASCII escaped character in '%.*s' @ %d\n",
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


/*
 * Return str as zero terminated string allocated
 * using pkg_malloc
 */
char* as_asciiz(str* s);

#endif
