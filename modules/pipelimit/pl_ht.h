/*
 * pipelimit module
 *
 * Copyright (C) 2006 Hendrik Scholz <hscholz@raisdorf.net>
 * Copyright (C) 2008 Ovidiu Sas <osas@voipembedded.com>
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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
 
/*! \file
 * \ingroup pipelimit
 * \brief pipelimit :: pl_ht
 */

#ifndef _RL_HT_H_
#define _RL_HT_H_

#include "../../str.h"

typedef struct _pl_pipe
{
    unsigned int cellid;
	str name;

	/* stuff that gets read as a modparam or set via fifo */
	int algo;
	int limit;

	/* updated values */
	int counter;
	int last_counter;
	int load;

    struct _pl_pipe *prev;
    struct _pl_pipe *next;
} pl_pipe_t;

typedef struct _rlp_slot
{
	unsigned int ssize;
	pl_pipe_t *first;
	gen_lock_t lock;	
} rlp_slot_t;

typedef struct _rlp_htable
{
	unsigned int htsize;
	rlp_slot_t *slots;
} rlp_htable_t;

int pl_init_htable(unsigned int hsize);
int pl_destroy_htable(void);
void pl_pipe_release(str *pipeid);
pl_pipe_t* pl_pipe_get(str *pipeid, int mode);
int pl_pipe_add(str *pipeid, str *algorithm, int limit);
int pl_print_pipes(void);
int pl_pipe_check_feedback_setpoints(int *cfgsp);
void pl_pipe_timer_update(int interval, int netload);

void rpl_pipe_lock(int slot);
void rpl_pipe_release(int slot);

/* PIPE_ALGO_FEEDBACK holds cpu usage to a fixed value using 
 * negative feedback according to the PID controller model
 *
 * <http://en.wikipedia.org/wiki/PID_controller>
 */
enum {
	PIPE_ALGO_NOP = 0,
	PIPE_ALGO_RED,
	PIPE_ALGO_TAILDROP,
	PIPE_ALGO_FEEDBACK,
	PIPE_ALGO_NETWORK
};

typedef struct str_map {
	str     str;
	int     id;
} str_map_t;

extern str_map_t algo_names[];

static inline int str_cmp(const str * a , const str * b)
{
	return ! (a->len == b->len && ! strncmp(a->s, b->s, a->len));
}

static inline int str_i_cmp(const str * a, const str * b)
{
	return ! (a->len == b->len && ! strncasecmp(a->s, b->s, a->len));
}

/**
 * converts a mapped str to an int
 * \return	0 if found, -1 otherwise
 */
static inline int str_map_str(const str_map_t * map, const str * key, int * ret)
{
	for (; map->str.s; map++) 
		if (! str_cmp(&map->str, key)) {
			*ret = map->id;
			return 0;
		}
	LM_DBG("str_map_str() failed map=%p key=%.*s\n", map, key->len, key->s);
	return -1;
}

/**
 * converts a mapped int to a str
 * \return	0 if found, -1 otherwise
 */
static inline int str_map_int(const str_map_t * map, int key, str * ret)
{
	for (; map->str.s; map++) 
		if (map->id == key) {
			*ret = map->str;
			return 0;
		}
	LM_DBG("str_map_str() failed map=%p key=%d\n", map, key);
	return -1;
}

/**
 * strcpy for str's (does not allocate the str structure but only the .s member)
 * \return	0 if succeeded, -1 otherwise
 */
static inline int str_cpy(str * dest, str * src)
{
	dest->len = src->len;
	dest->s = shm_malloc(src->len);
	if (! dest->s) {
		LM_ERR("oom: '%.*s'\n", src->len, src->s);
		return -1;
	}
	memcpy(dest->s, src->s, src->len);
	return 0;
}


#endif
