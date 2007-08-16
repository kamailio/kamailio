/*
 * $Id: hash.h 2583 2007-08-08 11:33:25Z anca_vamanu $
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *
 * History:
 * --------
 *  2007-08-20  initial version (anca)
 */


#ifndef PS_HASH_H
#define PS_HASH_H

#include "subscribe.h"

#define REMOTE_TYPE   1<<1
#define LOCAL_TYPE    1<<2

#define ERR_MEM(func)  LOG(L_ERR, "PRESENCE: %s: ERROR No more memory\n", func);\
						goto error

#define CONT_COPY(buf, dest, source)\
	dest.s= (char*)buf+ size;\
	memcpy(dest.s, source.s, source.len);\
	dest.len= source.len;\
	size+= source.len;

#define PKG_MEM_TYPE     1<< 1
#define SHM_MEM_TYPE     1<< 2

/* subscribe hash entry */

typedef struct subs_entry
{
	subs_t* entries;
	gen_lock_t lock;
}subs_entry_t;	

typedef subs_entry_t* shtable_t;

shtable_t new_shtable();

subs_t* search_shtable();

int insert_shtable(subs_t* subs);

int delete_shtable(str pres_uri, str ev_stored_name, str to_tag);

int update_shtable(subs_t* subs, int type);

subs_t* mem_copy_subs(subs_t* s, int mem_type);

void free_subs_list(subs_t* s_array, int mem_type);

void destroy_shtable();

/* presentity hash table */
typedef struct pres_entry
{
	str pres_uri;
	int event;
	int publ_count;
	struct pres_entry* next;
}pres_entry_t;

typedef struct pres_htable
{
	pres_entry_t* entries;
	gen_lock_t lock;
}phtable_t;

phtable_t* new_phtable();

pres_entry_t* search_phtable(str* pres_uri, int event, unsigned int hash_code);

int insert_phtable(str* pres_uri, int event);

int delete_phtable(str* pres_uri, int event);

void destroy_phtable();

#endif

