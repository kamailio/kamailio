/*
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
 */

/*!
 * \file
 * \brief Kamailio core :: Flags
 * \ingroup core
 * Module: \ref core
 */


#include <limits.h>
#include <stdint.h>
#include "sr_module.h"
#include "dprint.h"
#include "parser/msg_parser.h"
#include "flags.h"
#include "error.h"
#include "stdlib.h"
#include "hashes.h"
#include "clist.h"
#include "mem/mem.h"

/* Script flags */
static flag_t sflags = 0;


int setflag( struct sip_msg* msg, flag_t flag ) {
	msg->flags |= 1 << flag;
	return 1;
}

int resetflag( struct sip_msg* msg, flag_t flag ) {
	msg->flags &= ~ (1 << flag);
	return 1;
}

int resetflags( struct sip_msg* msg, flag_t flags ) {
	msg->flags &= ~ flags;
	return 1;
}

int isflagset( struct sip_msg* msg, flag_t flag ) {
	return (msg->flags & (1<<flag)) ? 1 : -1;
}

int flag_in_range( flag_t flag ) {
	if (flag > MAX_FLAG ) {
		LM_ERR("message flag %d too high; MAX=%d\n", flag, MAX_FLAG );
		return 0;
	}
	return 1;
}


int setsflagsval(flag_t val)
{
	sflags = val;
	return 1;
}


int setsflag(flag_t flag)
{
	sflags |= 1 << flag;
	return 1;
}


int resetsflag(flag_t flag)
{
	sflags &= ~ (1 << flag);
	return 1;
}


int issflagset(flag_t flag)
{
	return (sflags & (1<<flag)) ? 1 : -1;
}


flag_t getsflags(void)
{
	return sflags;
}


/* use 2^k */
#define FLAGS_NAME_HASH_ENTRIES		32

struct flag_entry{
	struct flag_entry* next;
	struct flag_entry* prev;
	str name;
	int no;
};


struct flag_hash_head{
	struct flag_entry* next;
	struct flag_entry* prev;
};

static struct flag_hash_head  name2flags[FLAGS_NAME_HASH_ENTRIES];
static unsigned char registered_flags[MAX_FLAG+1];


void init_named_flags()
{
	int r;
	
	for (r=0; r<FLAGS_NAME_HASH_ENTRIES; r++)
		clist_init(&name2flags[r], next, prev);
}



/* returns 0 on success, -1 on error */
int check_flag(int n)
{
	if (!flag_in_range(n))
		return -1;
	if (registered_flags[n]){
		LM_WARN("flag %d is already used by a named flag\n", n);
	}
	return 0;
}


inline static struct flag_entry* flag_search(struct flag_hash_head* lst,
												char* name, int len)
{
	struct flag_entry* fe;
	
	clist_foreach(lst, fe, next){
		if ((fe->name.len==len) && (memcmp(fe->name.s, name, len)==0)){
			/* found */
			return fe;
		}
	}
	return 0;
}



/* returns flag entry or 0 on not found */
inline static struct flag_entry* get_flag_entry(char* name, int len)
{
	int h;
	/* get hash */
	h=get_hash1_raw(name, len) & (FLAGS_NAME_HASH_ENTRIES-1);
	return flag_search(&name2flags[h], name, len);
}



/* returns flag number, or -1 on error */
int get_flag_no(char* name, int len)
{
	struct flag_entry* fe;
	
	fe=get_flag_entry(name, len);
	return (fe)?fe->no:-1;
}



/* resgiter a new flag name and associates it with pos
 * pos== -1 => any position will do 
 * returns flag pos on success (>=0)
 *         -1  flag is an alias for an already existing flag
 *         -2  flag already registered
 *         -3  mem. alloc. failure
 *         -4  invalid pos
 *         -5 no free flags */
int register_flag(char* name, int pos)
{
	struct flag_entry* e;
	int len;
	unsigned int r;
	static unsigned int crt_flag=0;
	unsigned int last_flag;
	unsigned int h;
	
	len=strlen(name);
	h=get_hash1_raw(name, len) & (FLAGS_NAME_HASH_ENTRIES-1);
	/* check if the name already exists */
	e=flag_search(&name2flags[h], name, len);
	if (e){
		LM_ERR("flag %.*s already registered\n", len, name);
		return -2;
	}
	/* check if there is already another flag registered at pos */
	if (pos!=-1){
		if ((pos<0) || (pos>MAX_FLAG)){
			LM_ERR("invalid flag %.*s position(%d)\n", len, name, pos);
			return -4;
		}
		if (registered_flags[pos]!=0){
			LM_WARN("%.*s:  flag %d already in use under another name\n",
					len, name, pos);
			/* continue */
		}
	}else{
		/* alloc an empty flag */
		last_flag=crt_flag+(MAX_FLAG+1);
		for (; crt_flag!=last_flag; crt_flag++){
			r=crt_flag%(MAX_FLAG+1);
			if (registered_flags[r]==0){
				pos=r;
				break;
			}
		}
		if (pos==-1){
			LM_ERR("could not register %.*s - too many flags\n", len, name);
			return -5;
		}
	}
	registered_flags[pos]++;
	
	e=pkg_malloc(sizeof(struct flag_entry));
	if (e==0){
		PKG_MEM_ERROR;
		return -3;
	}
	e->name.s=name;
	e->name.len=len;
	e->no=pos;
	clist_insert(&name2flags[h], e, next, prev);
	return pos;
}



/**
 *
 */
int setxflag(struct sip_msg* msg, flag_t flag)
{
	uint32_t fi;
	uint32_t fb;
	fi = flag / (sizeof(flag_t)*CHAR_BIT);
	fb = flag % (sizeof(flag_t)*CHAR_BIT);
	msg->xflags[fi] |= 1 << fb;
	return 1;
}

/**
 *
 */
int resetxflag(struct sip_msg* msg, flag_t flag)
{
	uint32_t fi;
	uint32_t fb;
	fi = flag / (sizeof(flag_t)*CHAR_BIT);
	fb = flag % (sizeof(flag_t)*CHAR_BIT);
	msg->xflags[fi] &= ~ (1 << fb);
	return 1;
}

/**
 *
 */
int isxflagset(struct sip_msg* msg, flag_t flag)
{
	uint32_t fi;
	uint32_t fb;
	fi = flag / (sizeof(flag_t)*CHAR_BIT);
	fb = flag % (sizeof(flag_t)*CHAR_BIT);
	return (msg->xflags[fi] & (1<<fb)) ? 1 : -1;
}
