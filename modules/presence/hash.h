/*
 * presence module - presence server implementation
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 */

/*! \file
 * \brief Kamailio presence module :: Hash tables
 * \ingroup presence 
 */



#ifndef PS_HASH_H
#define PS_HASH_H

#include "../../lock_ops.h"
//#include "presentity.h"

struct presentity;
#define REMOTE_TYPE   1<<1
#define LOCAL_TYPE    1<<2

#define PKG_MEM_STR       "pkg"
#define SHARE_MEM         "share"

#define ERR_MEM(mem_type)  LM_ERR("No more %s memory\n",mem_type);\
						goto error

#define CONT_COPY(buf, dest, source)\
	do{ \
	dest.s= (char*)buf+ size;\
	memcpy(dest.s, source.s, source.len);\
	dest.len= source.len;\
	size+= source.len; \
	} while(0);

#define PKG_MEM_TYPE     1<< 1
#define SHM_MEM_TYPE     1<< 2

/* subscribe hash entry */
struct subscription;

typedef struct subs_entry
{
	struct subscription* entries;
	gen_lock_t lock;
}subs_entry_t;	

typedef subs_entry_t* shtable_t;

shtable_t new_shtable(int hash_size);

struct subscription* search_shtable(shtable_t htable, str callid,str to_tag,str from_tag,
		unsigned int hash_code);

int insert_shtable(shtable_t htable, unsigned int hash_code, struct subscription* subs);

int delete_shtable(shtable_t htable, unsigned int hash_code, struct subscription* subs);

int update_shtable(shtable_t htable, unsigned int hash_code, struct subscription* subs,
		int type);

struct subscription* mem_copy_subs(struct subscription* s, int mem_type);

void free_subs_list(struct subscription* s_array, int mem_type, int ic);

void destroy_shtable(shtable_t htable, int hash_size);

/* subs htable functions type definitions */
typedef shtable_t (*new_shtable_t)(int hash_size);

typedef struct subscription* (*search_shtable_t)(shtable_t htable, str callid,str to_tag,
		str from_tag, unsigned int hash_code);

typedef int (*insert_shtable_t)(shtable_t htable, unsigned int hash_code,
		struct subscription* subs);

typedef int (*delete_shtable_t)(shtable_t htable, unsigned int hash_code,
		struct subscription* subs);

typedef int (*update_shtable_t)(shtable_t htable, unsigned int hash_code,
		struct subscription* subs, int type);

typedef void (*destroy_shtable_t)(shtable_t htable, int hash_size);

typedef struct subscription* (*mem_copy_subs_t)(struct subscription* s, int mem_type);


/* presentity hash table */
typedef struct pres_entry
{
	str pres_uri;
	int event;
	int publ_count;
	char* sphere;
	struct pres_entry* next;
}pres_entry_t;

typedef struct pres_htable
{
	pres_entry_t* entries;
	gen_lock_t lock;
}phtable_t;

phtable_t* new_phtable(void);

pres_entry_t* search_phtable(str* pres_uri, int event, unsigned int hash_code);

int insert_phtable(str* pres_uri, int event, char* sphere);

int update_phtable(struct presentity* presentity, str pres_uri, str body);

int delete_phtable(str* pres_uri, int event);

void destroy_phtable(void);

#endif

