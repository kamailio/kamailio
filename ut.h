/*
 *$Id$
 *
 * - various general purpose functions
 */

#ifndef ut_h
#define ut_h

#include "dprint.h"

/* returns string beginning and length without insignificant chars */
#define trim_len( _len, _begin, _mystr ) \
	do{ 	static char _c; \
		(_len)=(_mystr).len; \
		while ((_len) && ((_c=(_mystr).s[(_len)-1])==0 || _c=='\r' || _c=='\n' || _c==' ' || _c=='\t' )) \
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

#define via_s(_via,_p_msg) \
	translate_pointer((_p_msg)->orig,(_p_msg)->buf,(_via)->name.s)




/* converts a str to an u. short, returns the u. short and sets *err on
 * error and if err!=null
  */
static inline unsigned short str2s(unsigned char* str, unsigned int len,
									int *err)
{
	unsigned short ret;
	int i;
	unsigned char *limit;
	unsigned char *init;

	/*init*/
	ret=i=0;
	limit=str+len;
	init=str;

	for(;str<limit ;str++){
		if ( (*str <= '9' ) && (*str >= '0') ){
				ret=ret*10+*str-'0';
				i++;
				if (i>5) goto error_digits;
		}else{
				//error unknown char
				goto error_char;
		}
	}
	if (err) *err=0;
	return ret;

error_digits:
	DBG("str2s: ERROR: too many letters in [%s]\n", init);
	if (err) *err=1;
	return 0;
error_char:
	DBG("str2s: ERROR: unexpected char %c in %s\n", *str, init);
	if (err) *err=1;
	return 0;
}



/* converts a str to an ipv4 address, returns the address and sets *err
 * if error and err!=null
 */
static inline unsigned int str2ip(unsigned char* str, unsigned int len,
		int * err)
{
	unsigned int ret;
	int i;
	unsigned char *limit;
	unsigned char *init;

	/*init*/
	ret=i=0;
	limit=str+len;
	init=str;

	for(;str<limit ;str++){
		if (*str=='.'){
				i++;
				if (i>3) goto error_dots;
		}else if ( (*str <= '9' ) && (*str >= '0') ){
				((unsigned char*)&ret)[i]=((unsigned char*)&ret)[i]*10+
											*str-'0';
		}else{
				//error unknown char
				goto error_char;
		}
	}
	if (err) *err=0;
	return ret;

error_dots:
	DBG("str2ip: ERROR: too many dots in [%s]\n", init);
	if (err) *err=1;
	return 0;
error_char:
	DBG("str2ip: WARNING: unexpected char %c in %s\n", *str, init);
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
	


#endif
