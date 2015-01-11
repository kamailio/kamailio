/*
 * pipelimit module
 *
 * Copyright (C) 2006 Hendrik Scholz <hscholz@raisdorf.net>
 * Copyright (C) 2008 Ovidiu Sas <osas@voipembedded.com>
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../str.h"
#include "../../hashes.h"
#include "../../mem/shm_mem.h"
#include "../../lib/kmi/mi.h"
#include "../../rpc_lookup.h"

#include "pl_ht.h"

static rlp_htable_t *_pl_pipes_ht = NULL;

str_map_t algo_names[] = {
	{str_init("NOP"),	PIPE_ALGO_NOP},
	{str_init("RED"),	PIPE_ALGO_RED},
	{str_init("TAILDROP"),	PIPE_ALGO_TAILDROP},
	{str_init("FEEDBACK"),	PIPE_ALGO_FEEDBACK},
	{str_init("NETWORK"),	PIPE_ALGO_NETWORK},
	{{0, 0},		0},
};


int pl_init_htable(unsigned int hsize)
{
	int i;

	if(_pl_pipes_ht!=NULL)
		return -1;

	_pl_pipes_ht = (rlp_htable_t*)shm_malloc(sizeof(rlp_htable_t));
	if(_pl_pipes_ht==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(_pl_pipes_ht, 0, sizeof(rlp_htable_t));
	_pl_pipes_ht->htsize = hsize;

	_pl_pipes_ht->slots =
			(rlp_slot_t*)shm_malloc(_pl_pipes_ht->htsize*sizeof(rlp_slot_t));
	if(_pl_pipes_ht->slots==NULL)
	{
		LM_ERR("no more shm.\n");
		shm_free(_pl_pipes_ht);
		return -1;
	}
	memset(_pl_pipes_ht->slots, 0, _pl_pipes_ht->htsize*sizeof(rlp_slot_t));

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		if(lock_init(&_pl_pipes_ht->slots[i].lock)==0)
		{
			LM_ERR("cannot initalize lock[%d]\n", i);
			i--;
			while(i>=0)
			{
				lock_destroy(&_pl_pipes_ht->slots[i].lock);
				i--;
			}
			shm_free(_pl_pipes_ht->slots);
			shm_free(_pl_pipes_ht);
			return -1;

		}
	}

	return 0;
}


void pl_pipe_free(pl_pipe_t *it)
{
	return;
}

int pl_destroy_htable(void)
{
	int i;
	pl_pipe_t *it;
	pl_pipe_t *it0;

	if(_pl_pipes_ht==NULL)
		return -1;

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		/* free entries */
		it = _pl_pipes_ht->slots[i].first;
		while(it)
		{
			it0 = it;
			it = it->next;
			pl_pipe_free(it0);
		}
		/* free locks */
		lock_destroy(&_pl_pipes_ht->slots[i].lock);
	}
	shm_free(_pl_pipes_ht->slots);
	shm_free(_pl_pipes_ht);
	_pl_pipes_ht = NULL;
	return 0;
}

#define pl_compute_hash(_s)        get_hash1_raw((_s)->s,(_s)->len)
#define pl_get_entry(_h,_size)    (_h)&((_size)-1)

int pl_pipe_add(str *pipeid, str *algorithm, int limit)
{
	unsigned int cellid;
	unsigned int idx;
	pl_pipe_t *it, *prev, *cell;
	
	if(_pl_pipes_ht==NULL)
		return -1;

	cellid = pl_compute_hash(pipeid);
	idx = pl_get_entry(cellid, _pl_pipes_ht->htsize);

	lock_get(&_pl_pipes_ht->slots[idx].lock);
	it = _pl_pipes_ht->slots[idx].first;
	prev = NULL;
	while(it!=NULL && it->cellid < cellid)
	{
		prev = it;
		it = it->next;
	}
	while(it!=NULL && it->cellid == cellid)
	{
		if(pipeid->len==it->name.len 
				&& strncmp(pipeid->s, it->name.s, pipeid->len)==0)
		{
			 lock_release(&_pl_pipes_ht->slots[idx].lock);
			 return 1;
		}
		prev = it;
		it = it->next;
	}

	cell =
		(pl_pipe_t*)shm_malloc(sizeof(pl_pipe_t)+(1+pipeid->len)*sizeof(char));
	if(cell == NULL)
	{
		lock_release(&_pl_pipes_ht->slots[idx].lock);
		LM_ERR("cannot create new cell.\n");
		return -1;
	}
	memset(cell, 0, sizeof(pl_pipe_t)+(1+pipeid->len)*sizeof(char));
	
	cell->name.s = (char*)cell + sizeof(pl_pipe_t);
	strncpy(cell->name.s, pipeid->s, pipeid->len);
	cell->name.len = pipeid->len;
	cell->name.s[cell->name.len] = '\0';
	cell->cellid = cellid;
	cell->limit = limit;
	if (str_map_str(algo_names, algorithm, &cell->algo))
	{
		lock_release(&_pl_pipes_ht->slots[idx].lock);
		shm_free(cell);
		LM_ERR("cannot find algorithm [%.*s].\n", algorithm->len,
				algorithm->s);
		return -1;
	}

	if(prev==NULL)
	{
		if(_pl_pipes_ht->slots[idx].first!=NULL)
		{
			cell->next = _pl_pipes_ht->slots[idx].first;
			_pl_pipes_ht->slots[idx].first->prev = cell;
		}
		_pl_pipes_ht->slots[idx].first = cell;
	} else {
		cell->next = prev->next;
		cell->prev = prev;
		if(prev->next)
			prev->next->prev = cell;
		prev->next = cell;
	}
	_pl_pipes_ht->slots[idx].ssize++;
	lock_release(&_pl_pipes_ht->slots[idx].lock);

	return 0;
}

pl_pipe_t* pl_pipe_get(str *pipeid, int mode)
{
	unsigned int cellid;
	unsigned int idx;
	pl_pipe_t *it;
	
	if(_pl_pipes_ht==NULL)
		return NULL;

	cellid = pl_compute_hash(pipeid);
	idx = pl_get_entry(cellid, _pl_pipes_ht->htsize);

	lock_get(&_pl_pipes_ht->slots[idx].lock);
	it = _pl_pipes_ht->slots[idx].first;
	while(it!=NULL && it->cellid < cellid)
	{
		it = it->next;
	}
	while(it!=NULL && it->cellid == cellid)
	{
		if(pipeid->len==it->name.len 
				&& strncmp(pipeid->s, it->name.s, pipeid->len)==0)
		{
			 if(mode==0) lock_release(&_pl_pipes_ht->slots[idx].lock);
			 return it;
		}
		it = it->next;
	}
	lock_release(&_pl_pipes_ht->slots[idx].lock);
	return NULL;
}

void pl_pipe_release(str *pipeid)
{
	unsigned int cellid;
	unsigned int idx;

	if(_pl_pipes_ht==NULL)
		return;

	cellid = pl_compute_hash(pipeid);
	idx = pl_get_entry(cellid, _pl_pipes_ht->htsize);

	lock_release(&_pl_pipes_ht->slots[idx].lock);
}


int pl_print_pipes(void)
{
	int i;
	pl_pipe_t *it;

	if(_pl_pipes_ht==NULL)
		return -1;

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		lock_get(&_pl_pipes_ht->slots[i].lock);
		it = _pl_pipes_ht->slots[i].first;
		while(it)
		{
			LM_DBG("+++ pipe: %.*s [%u/%d]\n", it->name.len, it->name.s,
					it->cellid, i);
			LM_DBG("+++ ++++ algo: %d\n", it->algo);
			LM_DBG("+++ ++++ limit: %d\n", it->limit);
			LM_DBG("+++ ++++ counter: %d\n", it->counter);
			LM_DBG("+++ ++++ last_counter: %d\n", it->last_counter);
			LM_DBG("+++ ++++ load: %d\n", it->load);
			it = it->next;
		}
		lock_release(&_pl_pipes_ht->slots[i].lock);
	}
	return 0;
}

int pl_pipe_check_feedback_setpoints(int *cfgsp)
{
	int i, sp;
	pl_pipe_t *it;

	if(_pl_pipes_ht==NULL)
		return -1;

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		lock_get(&_pl_pipes_ht->slots[i].lock);
		it = _pl_pipes_ht->slots[i].first;
		while(it)
		{
			if (it->algo == PIPE_ALGO_FEEDBACK) {
				sp = it->limit;

				if (sp < 0 || sp > 100) {
					LM_ERR("FEEDBACK cpu load must be >=0 and <= 100 [%.*s]\n",
							 it->name.len, it->name.s);
					lock_release(&_pl_pipes_ht->slots[i].lock);
					return -1;
				} else if (*cfgsp == -1) {
					*cfgsp = sp;
				} else if (sp != *cfgsp) {
					LM_ERR("pipe %.*s: FEEDBACK cpu load values must "
						"be equal for all pipes\n",  it->name.len, it->name.s);
					lock_release(&_pl_pipes_ht->slots[i].lock);
					return -1;
				}
			}
			it = it->next;
		}
		lock_release(&_pl_pipes_ht->slots[i].lock);
	}
	return 0;
}

void pl_pipe_timer_update(int interval, int netload)
{
	int i;
	pl_pipe_t *it;

	if(_pl_pipes_ht==NULL)
		return;

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		lock_get(&_pl_pipes_ht->slots[i].lock);
		it = _pl_pipes_ht->slots[i].first;
		while(it)
		{
			if (it->algo != PIPE_ALGO_NOP) {
				if( it->algo == PIPE_ALGO_NETWORK ) {
					it->load = ( netload > it->limit ) ? 1 : -1;
				} else if (it->limit && interval) {
					it->load = it->counter / (it->limit * interval);
				}
				it->last_counter = it->counter;
				it->counter = 0;
			}

			it = it->next;
		}
		lock_release(&_pl_pipes_ht->slots[i].lock);
	}
}

extern int _pl_cfg_setpoint;
extern double *_pl_pid_setpoint;

/**
 * checks that all FEEDBACK pipes use the same setpoint 
 * cpu load. also sets (common) cfg_setpoint value
 * \param	modparam 1 to check modparam (static) fields, 0 to use shm ones
 *
 * \return	0 if ok, -1 on error
 */
static int check_feedback_setpoints(int modparam)
{
	_pl_cfg_setpoint = -1;
	return pl_pipe_check_feedback_setpoints(&_pl_cfg_setpoint);
}


/*
 * MI functions
 *
 * mi_stats() dumps the current config/statistics
 * mi_{invite|register|subscribe}() set the limits
 */

/* mi function implementations */
struct mi_root* mi_stats(struct mi_root* cmd_tree, void* param)
{
	struct mi_root *rpl_tree;
	struct mi_node *node=NULL, *rpl=NULL;
	struct mi_attr* attr;
	char* p;
	int i, len;
	pl_pipe_t *it;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		lock_get(&_pl_pipes_ht->slots[i].lock);
		it = _pl_pipes_ht->slots[i].first;
		while(it)
		{
			if (it->algo != PIPE_ALGO_NOP) {
				node = add_mi_node_child(rpl, 0, "PIPE", 4, 0, 0);
				if(node == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}

				attr = add_mi_attr(node, MI_DUP_VALUE, "id", 2, it->name.s,
						it->name.len);
				if(attr == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}

				p = int2str((unsigned long)(it->load), &len);
				attr = add_mi_attr(node, MI_DUP_VALUE, "load", 4, p, len);
				if(attr == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}

				p = int2str((unsigned long)(it->last_counter), &len);
				attr = add_mi_attr(node, MI_DUP_VALUE, "counter", 7, p, len);
				if(attr == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}
			}
			it = it->next;
		}
		lock_release(&_pl_pipes_ht->slots[i].lock);
	}

#if 0
	p = int2str((unsigned long)(*drop_rate), &len);
	node = add_mi_node_child(rpl, MI_DUP_VALUE, "DROP_RATE", 9, p, len);
#endif

	return rpl_tree;
error:
	LM_ERR("Unable to create reply\n");
	free_mi_tree(rpl_tree); 
	return 0;
}

struct mi_root* mi_get_pipes(struct mi_root* cmd_tree, void* param)
{
	struct mi_root *rpl_tree;
	struct mi_node *node=NULL, *rpl=NULL;
	struct mi_attr* attr;
	str algo;
	char* p;
	int i, len;
	pl_pipe_t *it;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		lock_get(&_pl_pipes_ht->slots[i].lock);
		it = _pl_pipes_ht->slots[i].first;
		while(it)
		{
			if (it->algo != PIPE_ALGO_NOP) {
				node = add_mi_node_child(rpl, 0, "PIPE", 4, 0, 0);
				if(node == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}

				attr = add_mi_attr(node, MI_DUP_VALUE, "id" , 2, it->name.s,
						it->name.len);
				if(attr == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}

				if (str_map_int(algo_names, it->algo, &algo))
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}
				attr = add_mi_attr(node, 0, "algorithm", 9, algo.s, algo.len);
				if(attr == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}

				p = int2str((unsigned long)(it->limit), &len);
				attr = add_mi_attr(node, MI_DUP_VALUE, "limit", 5, p, len);
				if(attr == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}

				p = int2str((unsigned long)(it->counter), &len);
				attr = add_mi_attr(node, MI_DUP_VALUE, "counter", 7, p, len);
				if(attr == NULL)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					goto error;
				}
			}
			it = it->next;
		}
		lock_release(&_pl_pipes_ht->slots[i].lock);
	}
	return rpl_tree;
error:
	LM_ERR("Unable to create reply\n");
	free_mi_tree(rpl_tree); 
	return 0;
}

struct mi_root* mi_set_pipe(struct mi_root* cmd_tree, void* param)
{
	struct mi_node *node;
	unsigned int algo_id, limit = 0;
	pl_pipe_t *it;
	str pipeid;

	node = cmd_tree->node.kids;
	if (node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	if ( !node->value.s || !node->value.len)
		goto error;
	pipeid = node->value;
	
	node = node->next;
	if ( !node || !node->value.s || !node->value.len)
		goto error;
	if (str_map_str(algo_names, &(node->value), (int*)&algo_id)) {
		LM_ERR("unknown algorithm: '%.*s'\n", node->value.len, node->value.s);
		goto error;
	}
	
	node = node->next;
	if ( !node || !node->value.s || !node->value.len || strno2int(&node->value,&limit)<0)
		goto error;

	LM_DBG("set_pipe: %.*s:%d:%d\n", pipeid.len, pipeid.s, algo_id, limit);

	it = pl_pipe_get(&pipeid, 1);
	if (it==NULL) {
		LM_ERR("no pipe: %.*s\n", pipeid.len, pipeid.s);
		goto error;
	}

	it->algo = algo_id;
	it->limit = limit;

	pl_pipe_release(&pipeid);

	if (check_feedback_setpoints(0)) {
		LM_ERR("feedback limits don't match\n");
		goto error;
	} else {
		*_pl_pid_setpoint = 0.01 * (double)_pl_cfg_setpoint;
	}

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
error:
	return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
}

void rpl_pipe_lock(int slot)
{
	lock_get(&_pl_pipes_ht->slots[slot].lock);
}

void rpl_pipe_release(int slot)
{
	lock_release(&_pl_pipes_ht->slots[slot].lock);
}


/* rpc function implementations */
void rpc_pl_stats(rpc_t *rpc, void *c)
{
	int i;
	pl_pipe_t *it;

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		lock_get(&_pl_pipes_ht->slots[i].lock);
		it = _pl_pipes_ht->slots[i].first;
		while(it)
		{
			if (it->algo != PIPE_ALGO_NOP) {
				if (rpc->rpl_printf(c, "PIPE: id=%.*s load=%d counter=%d",
					it->name.len, it->name.s,
					it->load, it->last_counter) < 0)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					return;
				}
			}
			it = it->next;
		}
		lock_release(&_pl_pipes_ht->slots[i].lock);
	}
}

void rpc_pl_get_pipes(rpc_t *rpc, void *c)
{
	int i;
	str algo;
	pl_pipe_t *it;

	for(i=0; i<_pl_pipes_ht->htsize; i++)
	{
		lock_get(&_pl_pipes_ht->slots[i].lock);
		it = _pl_pipes_ht->slots[i].first;
		while(it)
		{
			if (it->algo != PIPE_ALGO_NOP) {
				if (str_map_int(algo_names, it->algo, &algo))
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					return;
				}
				if (rpc->rpl_printf(c, "PIPE: id=%.*s algorithm=%.*s limit=%d counter=%d",
					it->name.len, it->name.s, algo.len, algo.s,
					it->limit, it->counter) < 0)
				{
					lock_release(&_pl_pipes_ht->slots[i].lock);
					return;
				}
			}
			it = it->next;
		}
		lock_release(&_pl_pipes_ht->slots[i].lock);
	}
}

void rpc_pl_set_pipe(rpc_t *rpc, void *c)
{
	unsigned int algo_id, limit = 0;
	pl_pipe_t *it;
	str pipeid, algo_str;

	if (rpc->scan(c, "SSd", &pipeid, &algo_str, &limit) < 3) return;

	if (str_map_str(algo_names, &algo_str, (int*)&algo_id)) {
		LM_ERR("unknown algorithm: '%.*s'\n", algo_str.len, algo_str.s);
		rpc->fault(c, 400, "Unknown algorithm");
		return;
	}

	LM_DBG("set_pipe: %.*s:%d:%d\n", pipeid.len, pipeid.s, algo_id, limit);

	it = pl_pipe_get(&pipeid, 1);
	if (it==NULL) {
		LM_ERR("no pipe: %.*s\n", pipeid.len, pipeid.s);
		rpc->fault(c, 400, "Unknown pipe id %.*s", pipeid.len, pipeid.s);
		return;
	}

	it->algo = algo_id;
	it->limit = limit;
	pl_pipe_release(&pipeid);

	if (check_feedback_setpoints(0)) {
		LM_ERR("feedback limits don't match\n");
		rpc->fault(c, 400, "Feedback limits don't match");
		return;
	} else {
		*_pl_pid_setpoint = 0.01 * (double)_pl_cfg_setpoint;
	}
}

