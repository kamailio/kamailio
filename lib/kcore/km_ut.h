/*
 *$Id$
 *
 * - various general purpose functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!
 * \file
 * \brief Various utility functions, mostly related to string handling
 */

#ifndef _KM_UT_H
#define _KM_UT_H

#include "../../ut.h"

#ifndef MIN
#define MIN(a, b) (a<b?a:b)
#endif
#ifndef MAX
#define MAX(a, b) (a>b?a:b)
#endif


#define append_str(_dest,_src,_len)				\
	do{											\
		memcpy( (_dest) , (_src) , (_len) );	\
		(_dest) += (_len) ;						\
	}while(0);									\
	
/*! append _c char to _dest string */
#define append_chr(_dest,_c) \
	*((_dest)++) = _c;


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



/*! right and left space trimming */
#define trim_spaces_lr(_s_)												\
	do{																	\
		for(;(_s_).s[(_s_).len-1]==' ';(_s_).s[--(_s_).len]=0);			\
		for(;(_s_).s[0]==' ';(_s_).s=(_s_).s+1,(_s_).len--);			\
																		\
	}																	\
	while(0);															\
	


#endif /* _KM_UT_H */
