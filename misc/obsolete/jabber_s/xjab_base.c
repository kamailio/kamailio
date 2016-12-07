/*
 * $Id$
 *
 * eXtended JABber module - Jabber connections pool
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"

#include "xjab_base.h"
#include "mdefines.h"

/**
 * get the hash code - based on Andrei's function
 * 
 */
int xj_get_hash(str *x, str *y)
{
	char* p;
	register unsigned v;
	register unsigned h;

	if(!x && !y)
		return 0;
	h=0;
	if(x)
	{
		for (p=x->s; p<=(x->s+x->len-4); p+=4)
		{
			v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
			h+=v^(v>>3);
		}
		v=0;
		for (;p<(x->s+x->len); p++)
		{ 
			v<<=8; 
			v+=*p;
		}
		h+=v^(v>>3);
	}
	if(y)
	{
		for (p=y->s; p<=(y->s+y->len-4); p+=4)
		{
			v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
			h+=v^(v>>3);
		}
	
		v=0;
		for (;p<(y->s+y->len); p++)
		{ 
			v<<=8; 
			v+=*p;
		}
		h+=v^(v>>3);
	}
	h=((h)+(h>>11))+((h>>13)+(h>>23));
	
	return (h)?h:1;
}

/**
 * function used to compare two xj_jkey elements
 */
int xj_jkey_cmp(void *x, void *y)
{
	int n;
	xj_jkey a, b;
	a = (xj_jkey)x;
	b = (xj_jkey)y;
	if(a == NULL || a->id == NULL || a->id->s == NULL)
	    return -1;
	if(b == NULL || b->id == NULL || b->id->s == NULL)
	    return 1;
	// DBG("JABBER: k_kmp: comparing <%.*s> / <%.*s>\n", ((str *)a)->len,
	// 		((str *)a)->s, ((str *)b)->len, ((str *)b)->s);
	if(a->hash != b->hash)
		return (a->hash < b->hash)?-1:1;
	
	if(a->id->len != b->id->len)
		return (a->id->len < b->id->len)?-1:1;
	
	n=strncmp(a->id->s,b->id->s,a->id->len);
	
	if(n!=0)
		return (n<0)?-1:1;
	
	return 0;
}
/**
 * free the information from a jkey
 */
void xj_jkey_free_p(void *p)
{
	if(p == NULL)
		return;
	if(((xj_jkey)p)->id != NULL)
	{
		if(((xj_jkey)p)->id->s != NULL)
			_M_SHM_FREE(((xj_jkey)p)->id->s);
		_M_SHM_FREE(((xj_jkey)p)->id);
	}
	_M_SHM_FREE(p);
}

/**
 * free a pointer to a t_jab_sipmsg structure
 * > element where points 'from' MUST be deliberated separated
 */
void xj_sipmsg_free(xj_sipmsg jsmsg)
{
	if(jsmsg == NULL)
		return;
	if(jsmsg->to.s != NULL)
		_M_SHM_FREE(jsmsg->to.s);
	// the key is deallocated when the connection is closed
//	if(jsmsg->jkey->id->s != NULL)
//		_M_SHM_FREE(jsmsg->from->id->s);
	if(jsmsg->msg.s != NULL)
		_M_SHM_FREE(jsmsg->msg.s);
	_M_SHM_FREE(jsmsg);
}

int xj_extract_aor(str* u, int t)
{
	struct sip_uri puri;

	if(!u)
		return -1;
	if (parse_uri(u->s, u->len, &puri) < 0)
	{
		LOG(L_ERR, "XJAB:extract_aor: Error while parsing URI\n");
		return -1;
	}
	
	if(t == 1)
		u->s = puri.user.s;
	u->len = puri.host.s + puri.host.len - u->s;
	return 0;
}


