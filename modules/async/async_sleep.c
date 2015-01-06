/**
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../locking.h"
#include "../../timer.h"
#include "../../async_task.h"
#include "../../modules/tm/tm_load.h"

#include "async_sleep.h"

/* tm */
extern struct tm_binds tmb;

typedef struct async_item {
	unsigned int tindex;
	unsigned int tlabel;
	unsigned int ticks;
	cfg_action_t *act;
	struct async_item *next;
} async_item_t;

typedef struct async_slot {
	async_item_t *lstart;
	async_item_t *lend;
	gen_lock_t lock;
} async_slot_t;

#define ASYNC_RING_SIZE	100

static struct async_list_head {
	async_slot_t ring[ASYNC_RING_SIZE];
	async_slot_t *later;
} *_async_list_head = NULL;

int async_init_timer_list(void)
{
	int i;
	_async_list_head = (struct async_list_head*)
						shm_malloc(sizeof(struct async_list_head));
	if(_async_list_head==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(_async_list_head, 0, sizeof(struct async_list_head));
	for(i=0; i<ASYNC_RING_SIZE; i++)
	{
		if(lock_init(&_async_list_head->ring[i].lock)==0)
		{
			LM_ERR("cannot init lock at %d\n", i);
			i--;
			while(i>=0)
			{
				lock_destroy(&_async_list_head->ring[i].lock);
				i--;
			}
			shm_free(_async_list_head);
			_async_list_head = 0;

			return -1;
		}
	}
	return 0;
}

int async_destroy_timer_list(void)
{
	int i;
	if(_async_list_head==NULL)
		return 0;
	for(i=0; i<ASYNC_RING_SIZE; i++)
	{
		/* TODO: clean the list */
		lock_destroy(&_async_list_head->ring[i].lock);
	}
	shm_free(_async_list_head);
	_async_list_head = 0;
	return 0;
}

int async_sleep(struct sip_msg* msg, int seconds, cfg_action_t *act)
{
	int slot;
	unsigned int ticks;
	async_item_t *ai;
	tm_cell_t *t = 0;

	if(seconds<=0) {
		LM_ERR("negative or zero sleep time (%d)\n", seconds);
		return -1;
	}
	if(seconds>=ASYNC_RING_SIZE)
	{
		LM_ERR("max sleep time is %d sec (%d)\n", ASYNC_RING_SIZE, seconds);
		return -1;
	}
	t = tmb.t_gett();
	if (t==NULL || t==T_UNDEFINED)
	{
		if(tmb.t_newtran(msg)<0)
		{
			LM_ERR("cannot create the transaction\n");
			return -1;
		}
		t = tmb.t_gett();
		if (t==NULL || t==T_UNDEFINED)
		{
			LM_ERR("cannot lookup the transaction\n");
			return -1;
		}
	}

	ticks = seconds + get_ticks();
	slot = ticks % ASYNC_RING_SIZE;
	ai = (async_item_t*)shm_malloc(sizeof(async_item_t));
	if(ai==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(ai, 0, sizeof(async_item_t));
	ai->ticks = ticks;
	ai->act = act;
	if(tmb.t_suspend(msg, &ai->tindex, &ai->tlabel)<0)
	{
		LM_ERR("failed to suppend the processing\n");
		shm_free(ai);
		return -1;
	}
	lock_get(&_async_list_head->ring[slot].lock);
	ai->next = _async_list_head->ring[slot].lstart;
	_async_list_head->ring[slot].lstart = ai;
	lock_release(&_async_list_head->ring[slot].lock);

	return 0;
}

void async_timer_exec(unsigned int ticks, void *param)
{
	int slot;
	async_item_t *ai;

	if(_async_list_head==NULL)
		return;

	slot = ticks % ASYNC_RING_SIZE;

	while(1) {
		lock_get(&_async_list_head->ring[slot].lock);
		ai = _async_list_head->ring[slot].lstart;
		if(ai!=NULL)
			_async_list_head->ring[slot].lstart = ai->next;
		lock_release(&_async_list_head->ring[slot].lock);

		if(ai==NULL)
			break;
		if(ai->act!=NULL)
			tmb.t_continue(ai->tindex, ai->tlabel, ai->act);
		shm_free(ai);
	}
}


/**
 *
 */
void async_exec_task(void *param)
{
	cfg_action_t *act;
	unsigned int *p;
	unsigned int tindex;
	unsigned int tlabel;

	act = *((cfg_action_t**)param);
	p = (unsigned int*)((char*)param + sizeof(cfg_action_t*));
	tindex = p[0];
	tlabel = p[1];

	if(act!=NULL)
		tmb.t_continue(tindex, tlabel, act);
	/* param is freed along with the async task strucutre in core */
}

/**
 *
 */
int async_send_task(sip_msg_t* msg, cfg_action_t *act)
{
	async_task_t *at;
	tm_cell_t *t = 0;
	unsigned int tindex;
	unsigned int tlabel;
	int dsize;
	unsigned int *p;

	t = tmb.t_gett();
	if (t==NULL || t==T_UNDEFINED)
	{
		if(tmb.t_newtran(msg)<0)
		{
			LM_ERR("cannot create the transaction\n");
			return -1;
		}
		t = tmb.t_gett();
		if (t==NULL || t==T_UNDEFINED)
		{
			LM_ERR("cannot lookup the transaction\n");
			return -1;
		}
	}
	dsize = sizeof(async_task_t) + sizeof(cfg_action_t*)
		+ 2*sizeof(unsigned int);
	at = (async_task_t*)shm_malloc(dsize);
	if(at==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(at, 0, dsize);
	if(tmb.t_suspend(msg, &tindex, &tlabel)<0)
	{
		LM_ERR("failed to suppend the processing\n");
		shm_free(at);
		return -1;
	}
	at->exec = async_exec_task;
	at->param = (char*)at + sizeof(async_task_t);
	*((cfg_action_t**)at->param) = act;
	p = (unsigned int*)((char*)at->param + sizeof(cfg_action_t*));
	p[0] = tindex;
	p[1] = tlabel;
	async_task_push(at);
	return 0;
}
