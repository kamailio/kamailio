/*
 * $Id$
 *
 * eXtended JABber module
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

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include "../../dprint.h"
#include "../../mem/mem.h"

#include "xjab_jconf.h"
#include "xjab_base.h"

/**
 *
 */
xj_jconf xj_jconf_new(str *u)
{
	xj_jconf jcf = NULL;
	
	if(!u || !u->s || u->len<=0)
		return NULL;
	jcf = (xj_jconf)pkg_malloc(sizeof(t_xj_jconf));
	if(jcf == NULL)
	{
		DBG("XJAB:xj_jconf_new: error - no pkg memory.\n");
		return NULL;
	}

	jcf->uri.s = (char*)pkg_malloc((u->len+1)*sizeof(char));
	if(jcf->uri.s == NULL)
	{
		DBG("XJAB:xj_jconf_new: error - no pkg memory!\n");
		pkg_free(jcf);
		return NULL;
	}

	strncpy(jcf->uri.s, u->s, u->len);
	jcf->uri.len = u->len;
	jcf->uri.s[jcf->uri.len] = 0;

	jcf->jcid = 0;
	jcf->status = XJ_JCONF_NULL;
	
	jcf->room.s = NULL;
	jcf->room.len = 0;
	jcf->server.s = NULL;
	jcf->server.len = 0;
	jcf->nick.s = NULL;
	jcf->nick.len = 0;
	
	return jcf;
}

/**
 *
 */
int xj_jconf_init_sip(xj_jconf jcf, str *sid, char dl)
{
	char *p, *p0;
	int n = 0;
	if(!jcf || !jcf->uri.s || jcf->uri.len <= 0 
			|| !sid || !sid->s || sid->len <= 0)
		return -1;
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jconf_init_sip: parsing uri\n");
#endif	
	p = jcf->uri.s;
	while(p<(jcf->uri.s + jcf->uri.len)	&& *p != '@') 
		p++;
	if(*p != '@')
		goto bad_format;
	p0 = p;
	
	while(p0 > jcf->uri.s)
	{
		p0--;
		if(*p0 == dl)
		{
			switch(n)
			{
				case 0:
						jcf->server.s = p0+1;
						jcf->server.len = p - jcf->server.s;
					break;
				case 1:
						jcf->room.s = p0+1;
						jcf->room.len = p - jcf->room.s;
					break;
				case 2:
						jcf->nick.s = p0+1;
						jcf->nick.len = p - jcf->nick.s;
					break;
			}
			n++;
			p = p0;
		}
	}
	if(n != 2 || p0 != jcf->uri.s)
		goto bad_format;

	if(p0 == jcf->uri.s && *p0 != dl)
	{
		jcf->nick.s = p0;
		jcf->nick.len = p - jcf->nick.s;
	}
	else
	{
		jcf->nick.s = p = sid->s;
		while(p < sid->s + sid->len && *p!='@')
		{
			if(*p == ':')
				jcf->nick.s = p+1;
			p++;
		}
		jcf->nick.len = p - jcf->nick.s;
	}

	jcf->jcid = xj_get_hash(&jcf->room, &jcf->server);
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jconf_init_sip: conference id=%d\n", jcf->jcid);
#endif	
	return 0;
	
bad_format:
	DBG("XJAB:xj_jconf_init_sip: error parsing uri - bad format\n");
	return -2;
}

/**
 *
 */
int xj_jconf_init_jab(xj_jconf jcf)
{
	char *p, *p0;
	if(!jcf || !jcf->uri.s || jcf->uri.len <= 0)
		return -1;
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jconf_init_jab: parsing uri\n");
#endif	
	p = jcf->uri.s;
	while(p<(jcf->uri.s + jcf->uri.len)	&& *p != '@') 
		p++;
	if(*p != '@' || p==jcf->uri.s)
		goto bad_format;
	
	p0 = p+1;
	
	while(p0 < ((jcf->uri.s + jcf->uri.len)) && *p0 != '/')
		p0++;
	
	jcf->server.s = p+1;
	jcf->server.len = p0 - jcf->server.s;
	jcf->room.s = jcf->uri.s;
	jcf->room.len = p - jcf->room.s;
	if(p0 < jcf->uri.s + jcf->uri.len)
	{
		jcf->nick.s = p0+1;
		jcf->nick.len = jcf->uri.s + jcf->uri.len - jcf->nick.s;
	}
	jcf->jcid = xj_get_hash(&jcf->room, &jcf->server);
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jconf_init_jab: conference id=%d\n", jcf->jcid);
#endif
	return 0;
	
bad_format:
	DBG("XJAB:xj_jconf_init_jab: error parsing uri - bad format\n");
	return -2;
}


/**
 *
 */
int xj_jconf_set_status(xj_jconf jcf, int s)
{
	if(!jcf || !jcf->uri.s || jcf->uri.len <= 0)
		return -1;
	jcf->status = s;
	return 0;
}

/**
 *
 */
int xj_jconf_cmp(void *a, void *b)
{
	int n;
	if(a == NULL)
	    return -1;
	if(b == NULL)
	    return 1;
	
	// DBG("XJAB: xj_jconf_cmp: comparing <%.*s> / <%.*s>\n",((str *)a)->len,
	// 		((str *)a)->s, ((str *)b)->len, ((str *)b)->s);
	if(((xj_jconf)a)->jcid < ((xj_jconf)b)->jcid)
			return -1;
	if(((xj_jconf)a)->jcid > ((xj_jconf)b)->jcid)
			return 1;
	
	if(((xj_jconf)a)->room.len < ((xj_jconf)b)->room.len)
		return -1;
	if(((xj_jconf)a)->room.len > ((xj_jconf)b)->room.len)
		return 1;
	
	if(((xj_jconf)a)->server.len < ((xj_jconf)b)->server.len)
		return -1;
	if(((xj_jconf)a)->server.len > ((xj_jconf)b)->server.len)
		return 1;

	n = strncmp(((xj_jconf)a)->room.s, ((xj_jconf)b)->room.s, 
					((xj_jconf)a)->room.len);
	if(n<0)
		return -1;
	if(n>0)
		return 1;
	
	n = strncmp(((xj_jconf)a)->server.s, ((xj_jconf)b)->server.s, 
					((xj_jconf)a)->server.len);
	if(n<0)
		return -1;
	if(n>0)
		return 1;
	
	return 0;
}

/**
 *
 */
int xj_jconf_free(xj_jconf jcf)
{
	if(!jcf)
		return 0;
	
	if(jcf->uri.s != NULL)
		pkg_free(jcf->uri.s);
	pkg_free(jcf);
	jcf = NULL;
	
	return 0;
}

/**
 *
 */
int xj_jconf_check_addr(str *addr, char dl)
{
	char *p;
	int i;

	if(!addr || !addr->s || addr->len <= 0)
		return -1;

	p = addr->s;
	i= 0;
	while((p < addr->s+addr->len) && *p != '@')
	{
		if(*p==dl)
			i++;
		p++;
	}
	if(i==2 && *p=='@')
		return 0;

	return -1;
}

