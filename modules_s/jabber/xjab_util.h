/*
 * eXtended JABber module
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */


#ifndef _XJAB_UTIL_H_
#define _XHAB_UTIL_H_

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<sys/un.h>

#include "xjab_jcon.h"
#include "tree234.h"
#include "../../str.h"
#include "../../db/db.h"
#include "lock.h"
#include "../tm/tm_load.h"

/**********             ***/

typedef struct _xj_sipmsg
{
	str *from; // pointer to FROM
	str to;    // destination
	str msg;   // message body
} t_xj_sipmsg, *xj_sipmsg;

/**********             ***/

typedef struct _xj_jmsg_queue
{
	int len;		// maximum size of the queue
	int size;		// numebr of elements in the queue
	int cache;		// cache time (seconds)
	int *expire;	// expire time of the queued message
	xj_sipmsg *jsm;	// pointer to the message
	xj_jcon *ojc;	// pointer to the connection which will be used on sending
} t_xj_jmsg_queue, *xj_jmsg_queue;

/**********             ***/

typedef struct _xj_jcon_pool
{
	int len;					// maximum len of the pool
	xj_jcon *ojc;				// the connections to the Jabber
	t_xj_jmsg_queue jmqueue;	// messages queue
} t_xj_jcon_pool, *xj_jcon_pool;

/**********   LOOK AT IMPLEMENTATION OF FUNCTIONS FOR DESCRIPTION    ***/

xj_jcon_pool xj_jcon_pool_init(int, int, int);
int xj_jcon_pool_add(xj_jcon_pool, xj_jcon);
xj_jcon xj_jcon_pool_get(xj_jcon_pool, str*);
int xj_jcon_pool_del(xj_jcon_pool, str*);
void xj_jccon_pool_free(xj_jcon_pool);
void xj_jcon_pool_print(xj_jcon_pool);
int xj_jcon_pool_add_jmsg(xj_jcon_pool, xj_sipmsg, xj_jcon);
int xj_jcon_pool_del_jmsg(xj_jcon_pool, int);

/**********             ***/

char *shahash(const char *);

#endif

