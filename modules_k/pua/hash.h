/*
 * $Id$
 *
 * pua module - presence user agent module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _PU_HASH_H_
#define _PU_HASH_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../str.h"
#include "../../lock_ops.h"

#define UL_PUBLISH			1<<0
#define MI_PUBLISH			1<<1
#define MI_SUBSCRIBE		1<<2
#define END2END_PUBLISH		1<<3
#define END2END_SUBSCRIBE   1<<4
#define XMPP_PUBLISH		1<<5
#define XMPP_SUBSCRIBE      1<<6

#define PRESENCE_EVENT      1<<0
#define PWINFO_EVENT        1<<1


#define NO_UPDATEDB_FLAG	1<<0
#define UPDATEDB_FLAG		1<<1
#define INSERTDB_FLAG		1<<2

typedef struct hentity
{
	str* pres_uri;
	str* watcher_uri;
	str id;
	str tuple_id;
	int event;
	int flag;
	int desired_expires;
}hentity_t;

typedef struct ua_pres{
 
    /* common*/
    str* pres_uri;
	str id;
	int event;
	time_t expires;
	time_t desired_expires;
	int flag;
	int db_flag;
	struct ua_pres* next;

	/* publish */
	str etag;
	str tuple_id;
	
	/* subscribe */
	str* watcher_uri;
	str call_id;
	str to_tag;
    str from_tag;
	int cseq;
	
}ua_pres_t;

typedef struct hash_entry
{
	ua_pres_t* entity;
	gen_lock_t lock;
}hash_entry_t;	

typedef struct htable{
    hash_entry_t* p_records;        	              
}htable_t;

htable_t* new_htable();

ua_pres_t* search_htable(str* pres_uri, str* watcher_uri, str id, 
		int FLAG, int event, int hash_code);

void insert_htable(ua_pres_t* presentity );

void update_htable(ua_pres_t* presentity, int expires, int hash_code);

void delete_htable(ua_pres_t* presentity );

void destroy_htable();


#endif
