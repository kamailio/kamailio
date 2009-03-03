/*
 * $Id$
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
 * History:
 *---------
 *
 * 2006-01-20 - new_hash1() added; support for configurable hash size
 *              added (bogdan)
 * 2006-03-13 - new_hash1() and new_hash2() merged into core_hash();
 *              added core_case_hash() for case insensitive hashes;
 *              all TM dependet stuff moved to TM config file (bogdan)
 */

/*!
 * \file
 * \brief Hash functions
 */


#ifndef _HASH_FUNC_H_
#define _HASH_FUNC_H_

#include "../../str.h"


#define ch_h_inc h+=v^(v>>3)
#define ch_icase(_c) (((_c)>='A'&&(_c)<='Z')?((_c)|0x20):(_c))
static inline unsigned int core_hash(const str *s1, const str *s2, const unsigned int size)
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


static inline unsigned int core_case_hash( str *s1, str *s2, unsigned int size)
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
