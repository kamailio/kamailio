/*
 *
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


#ifndef _XJAB_WORKER_H_
#define _XJAB_WORKER_H_

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
typedef struct _xj_jalias
{
	int size;
	str *a;
	str *b;
} t_xj_jalias, *xj_jalias;

typedef struct _xj_worker
{
	int pid;			// process id
	int pipe;			// communication pipe
	int nr;				// number of jobs
	tree234 *sip_ids;   // sip ids allocated for the worker
} t_xj_worker, *xj_worker;

typedef struct _xj_wlist
{
	int len;   			// length of the list
	int maxj;			// maximum jobs / worker
	struct sockaddr_in sserver;  // sip server address
	str *contact_h;	// contact header
    smart_lock	*sems;	 // semaphores
	xj_jalias	aliases; // addess aliases
	xj_worker	workers; // the list of workers
} t_xj_wlist, *xj_wlist;

/**********   LOOK AT IMPLEMENTATION OF FUNCTIONS FOR DESCRIPTION    ***/

xj_wlist xj_wlist_init(int **, int, int);
int  xj_wlist_init_contact(xj_wlist, char *);
int  xj_wlist_set_pids(xj_wlist, int *, int);
int  xj_wlist_get(xj_wlist, str *, str **);
void xj_wlist_del(xj_wlist, str *, int);
void xj_wlist_free(xj_wlist);
int  xj_wlist_set_aliases(xj_wlist, char *);
 
int xj_worker_process(xj_wlist, char*, int, int, int, int, int, int, db_con_t*);

int xj_address_translation(str *src, str *dst, xj_jalias als, int flag);
int xj_manage_jab(char *buf, int len, int *pos, str *sid,
		str *sct, xj_jalias als, xj_jcon jbc);
int xj_send_sip_msg(str *, str *, str *, str *);

/**********             ***/

#endif

