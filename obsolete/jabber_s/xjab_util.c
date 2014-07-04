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


#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../timer.h"

#include "xjab_util.h"
#include "xjab_jcon.h"

#include "mdefines.h"

/**
 * init a jc_pool structure
 * - size : maximum number of the open connection to Jabber
 * - jlen : maximum size of messages queue
 * #return : pointer to the structure or NULL on error
 */
xj_jcon_pool xj_jcon_pool_init(int size, int jlen, int ch)
{
	xj_jcon_pool jcp = (xj_jcon_pool)_M_MALLOC(sizeof(t_xj_jcon_pool));
	if(jcp == NULL)
		return NULL;
	jcp->len = size;
	jcp->ojc = (xj_jcon*)_M_MALLOC(size*sizeof(xj_jcon));
	if(jcp->ojc == NULL)
	{
		_M_FREE(jcp);
		return NULL;
	}
	memset( jcp->ojc , 0, size*sizeof(xj_jcon) );
	jcp->jmqueue.len = jlen;
	jcp->jmqueue.size = 0;
	jcp->jmqueue.cache = (ch>0)?ch:90;
	jcp->jmqueue.expire = (int*)_M_MALLOC(jlen*sizeof(int));
	if(jcp->jmqueue.expire == NULL)
	{
		_M_FREE(jcp->ojc);
		_M_FREE(jcp);
		return NULL;
	}
	jcp->jmqueue.jsm=(xj_sipmsg*)_M_MALLOC(jlen*sizeof(xj_sipmsg));
	if(jcp->jmqueue.jsm == NULL)
	{
		_M_FREE(jcp->jmqueue.expire);
		_M_FREE(jcp->ojc);
		_M_FREE(jcp);
		return NULL;
	}
	jcp->jmqueue.ojc = (xj_jcon*)_M_MALLOC(jlen*sizeof(xj_jcon));
	if(jcp->jmqueue.ojc == NULL)
	{
		_M_FREE(jcp->jmqueue.expire);
		_M_FREE(jcp->jmqueue.jsm);
		_M_FREE(jcp->ojc);
		_M_FREE(jcp);
		return NULL;
	}
	memset( jcp->jmqueue.expire , 0, jlen*sizeof(int) );
	memset( jcp->jmqueue.jsm , 0, jlen*sizeof(xj_sipmsg) );
	memset( jcp->jmqueue.ojc , 0, jlen*sizeof(xj_jcon) );
	return jcp;
}

/**
 * add a new element in messages queue
 * - jcp : pointer to the Jabber connections pool structure
 * - _jsm : pointer to the message
 * - _ojc : pointer to the Jabber connection that will be used for this message
 * #return : 0 on success or <0 on error
 */
int xj_jcon_pool_add_jmsg(xj_jcon_pool jcp, xj_sipmsg _jsm, xj_jcon _ojc)
{
	int i;
	
	if(jcp == NULL)
		return -1;
	if(jcp->jmqueue.size == jcp->jmqueue.len)
		return -2;
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jcon_pool_add_jmsg: add msg into the pool\n");
#endif
	for(i = 0; i<jcp->jmqueue.len; i++)
	{
		if(jcp->jmqueue.jsm[i] == NULL || jcp->jmqueue.ojc[i] == NULL)
		{
			jcp->jmqueue.size++;
			jcp->jmqueue.expire[i] = get_ticks() + jcp->jmqueue.cache;
			jcp->jmqueue.jsm[i] = _jsm;
			jcp->jmqueue.ojc[i] = _ojc;
			return 0;
		}
	}
	return -2;
}

/**
 * delete first element from messages queue
 * - jcp : pointer to the Jabber connections pool structure
 * #return : 0 on success or <0 on error
 */
int xj_jcon_pool_del_jmsg(xj_jcon_pool jcp, int idx)
{
	if(jcp == NULL)
		return -1;
	if(jcp->jmqueue.size <= 0)
		return -2;
	jcp->jmqueue.size--;
	jcp->jmqueue.jsm[idx] = NULL;
	jcp->jmqueue.ojc[idx] = NULL;
	
	return 0;
}

/**
 * add a new connection in pool
 * - jcp : pointer to the Jabber connections pool structure
 * #return : 0 on success or <0 on error
 */
int xj_jcon_pool_add(xj_jcon_pool jcp, xj_jcon jc)
{
	int i = 0;
	
	if(jcp == NULL)
		return -1;
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jcon_pool_add: add connection into the pool\n");
#endif	
	while(i < jcp->len && jcp->ojc[i] != NULL)
		i++;
	if(i >= jcp->len)
		return -1;
	jcp->ojc[i] = jc;
	
	return 0;
}

/**
 * get the jabber connection associated with 'id'
 * - jcp : pointer to the Jabber connections pool structure
 * - id : id of the Jabber connection
 * #return : pointer to the open connection to Jabber structure or NULL on error
 */
xj_jcon xj_jcon_pool_get(xj_jcon_pool jcp, xj_jkey jkey)
{
	int i = 0;
	xj_jcon _ojc;
	
	if(jcp==NULL || jkey==NULL || jkey->id==NULL || jkey->id->s==NULL)
		return NULL;
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jcon_pool_get: looking for the connection of <%.*s>"
		" into the pool\n", jkey->id->len, jkey->id->s);
#endif
	while(i < jcp->len)
	{
	 	if((jcp->ojc[i]!=NULL) && jcp->ojc[i]->jkey->hash==jkey->hash && 
			(!strncmp(jcp->ojc[i]->jkey->id->s, jkey->id->s, jkey->id->len)))
	 	{
	 		_ojc = jcp->ojc[i];
	 		//jcp->ojc[i] = NULL;
	 		return _ojc;
	 	}
		i++;
	}

	return NULL;
}

/**
 * remove the connection associated with 'id' from pool
 * - jcp : pointer to the Jabber connections pool structure
 * - id : id of the Jabber connection
 * #return : 0 on success or <0 on error
 */
int xj_jcon_pool_del(xj_jcon_pool jcp, xj_jkey jkey)
{
	int i = 0;
	
	if(jcp==NULL || jkey==NULL || jkey->id==NULL || jkey->id->s==NULL)
		return -1;
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jcon_pool_del: removing a connection from the pool\n");
#endif
	while(i < jcp->len)
	{
	 	if((jcp->ojc[i]!=NULL) && jcp->ojc[i]->jkey->hash==jkey->hash && 
			(!strncmp(jcp->ojc[i]->jkey->id->s,jkey->id->s,jkey->id->len)))
	 	{
	 		xj_jcon_free(jcp->ojc[i]);
	 		jcp->ojc[i] = NULL;
	 		break;
	 	}
		i++;
	}

	return 0;
}

/**
 * free a Jabber connections pool structure
 * - jcp : pointer to the Jabber connections pool structure
 */
void xj_jcon_pool_free(xj_jcon_pool jcp)
{
	int i;
	if(jcp == NULL)
		return;
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_jcon_pool_free: -----START-----\n");
#endif
	if(jcp->ojc != NULL)
	{
		for(i=0; i<jcp->len; i++)
		{
			if(jcp->ojc[i] != NULL)
				xj_jcon_free(jcp->ojc[i]);
		}
		_M_FREE(jcp->ojc);
	}
	if(jcp->jmqueue.jsm != NULL)
		_M_FREE(jcp->jmqueue.jsm);
	if(jcp->jmqueue.ojc != NULL)
		_M_FREE(jcp->jmqueue.ojc);
	if(jcp->jmqueue.expire != NULL)
		_M_FREE(jcp->jmqueue.expire);
		
	_M_FREE(jcp);
}

