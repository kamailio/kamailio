/*
 * $Id$
 *
 * eXtended JABber module
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *  2003-03-11  major locking changes - uses locking.h (andrei)
 *  2004-07-28  s/lock_set_t/gen_lock_set_t/ because of a type conflict
 *              on darwin (andrei)
 */


#ifndef _XJAB_WORKER_H_
#define _XJAB_WORKER_H_

#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "../../locking.h"
#include "../../modules/tm/tm_load.h"

#include "xjab_util.h"
#include "tree234.h"

/**********             ***/
typedef struct _xj_jalias
{
	int size;	// number of aliases
	str *jdm;	// Jabber domain
	char dlm;	// user part delimiter
	str *proxy; // outbound proxy
	str *a;		// aliases
	char *d;	// user part delimiter for aliases
} t_xj_jalias, *xj_jalias;

typedef struct _xj_worker
{
	int pid;			// process id
	int wpipe;			// communication pipe - write
	int rpipe;			// communication pipe - read
	int nr;				// number of jobs
	tree234 *sip_ids;   // sip ids allocated for the worker
} t_xj_worker, *xj_worker;

typedef struct _xj_wlist
{
	int len;   			// length of the list
	int maxj;			// maximum jobs / worker
	int cachet;
	int delayt;
	int sleept;
	gen_lock_set_t	 *sems;	 // semaphores
	xj_jalias	aliases; // added aliases
	xj_worker	workers; // the list of workers
} t_xj_wlist, *xj_wlist;

/**********   LOOK AT IMPLEMENTATION OF FUNCTIONS FOR DESCRIPTION    ***/

xj_wlist xj_wlist_init(int **, int, int, int, int, int);
int  xj_wlist_set_pid(xj_wlist, int, int);
int  xj_wlist_get(xj_wlist, xj_jkey, xj_jkey*);
int  xj_wlist_check(xj_wlist, xj_jkey, xj_jkey*);
int  xj_wlist_set_flag(xj_wlist, xj_jkey, int);
void xj_wlist_del(xj_wlist, xj_jkey, int);
void xj_wlist_free(xj_wlist);
int  xj_wlist_set_aliases(xj_wlist, char *, char *, char *);
int  xj_wlist_check_aliases(xj_wlist, str*);
int  xj_wlist_clean_jobs(xj_wlist, int, int); 

int xj_worker_process(xj_wlist, char*, int, char*, int, db1_con_t*, db_func_t*);

int xj_address_translation(str *src, str *dst, xj_jalias als, int flag);
int xj_manage_jab(char *buf, int len, int *pos, xj_jalias als, xj_jcon jbc);

void xj_sig_handler(int s);

/**********             ***/

int xj_send_sip_msg(str *, str *, str *, str *, int *);
int xj_send_sip_msgz(str *,str *, str *, char *, int *);
void xj_tuac_callback( struct cell *t, int type, struct tmcb_params *ps);
void xj_worker_check_jcons(xj_wlist, xj_jcon_pool, int, fd_set*);
void xj_worker_check_qmsg(xj_wlist, xj_jcon_pool);
void xj_worker_check_watcher(xj_wlist, xj_jcon_pool, xj_jcon, xj_sipmsg);

#endif

