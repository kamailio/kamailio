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

#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/receive.h"
#include "../../core/locking.h"
#include "../../core/timer.h"
#include "../../core/async_task.h"
#include "../../modules/tm/tm_load.h"
#include "../../core/kemi.h"

#include "async_sleep.h"

#define ASYNC_CBNAME_SIZE 64
/* tm */
extern struct tm_binds tmb;

/* clang-format off */
typedef struct async_item {
	unsigned int tindex;
	unsigned int tlabel;
	unsigned int ticks;
	cfg_action_t *ract;
	char cbname[ASYNC_CBNAME_SIZE];
	int cbname_len;
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
/* clang-format on */

/**
 *
 */
int async_init_timer_list(void)
{
	int i;
	_async_list_head = (struct async_list_head *)shm_malloc(
			sizeof(struct async_list_head));
	if(_async_list_head == NULL) {
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(_async_list_head, 0, sizeof(struct async_list_head));
	for(i = 0; i < ASYNC_RING_SIZE; i++) {
		if(lock_init(&_async_list_head->ring[i].lock) == 0) {
			LM_ERR("cannot init lock at %d\n", i);
			i--;
			while(i >= 0) {
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
	if(_async_list_head == NULL)
		return 0;
	for(i = 0; i < ASYNC_RING_SIZE; i++) {
		/* TODO: clean the list */
		lock_destroy(&_async_list_head->ring[i].lock);
	}
	shm_free(_async_list_head);
	_async_list_head = 0;
	return 0;
}

int async_sleep(sip_msg_t *msg, int seconds, cfg_action_t *act, str *cbname)
{
	int slot;
	unsigned int ticks;
	async_item_t *ai;
	tm_cell_t *t = 0;

	if(seconds <= 0) {
		LM_ERR("negative or zero sleep time (%d)\n", seconds);
		return -1;
	}
	if(seconds >= ASYNC_RING_SIZE) {
		LM_ERR("max sleep time is %d sec (%d)\n", ASYNC_RING_SIZE, seconds);
		return -1;
	}
	if(cbname && cbname->len>=ASYNC_CBNAME_SIZE-1) {
		LM_ERR("callback name is too long: %.*s\n", cbname->len, cbname->s);
		return -1;
	}
	t = tmb.t_gett();
	if(t == NULL || t == T_UNDEFINED) {
		if(tmb.t_newtran(msg) < 0) {
			LM_ERR("cannot create the transaction\n");
			return -1;
		}
		t = tmb.t_gett();
		if(t == NULL || t == T_UNDEFINED) {
			LM_ERR("cannot lookup the transaction\n");
			return -1;
		}
	}

	ticks = seconds + get_ticks();
	slot = ticks % ASYNC_RING_SIZE;
	ai = (async_item_t *)shm_malloc(sizeof(async_item_t));
	if(ai == NULL) {
		LM_ERR("no more shm memory\n");
		return -1;
	}
	memset(ai, 0, sizeof(async_item_t));
	ai->ticks = ticks;
	ai->ract = act;
	if(cbname && cbname->len>0) {
		memcpy(ai->cbname, cbname->s, cbname->len);
		ai->cbname[cbname->len] = '\0';
		ai->cbname_len = cbname->len;
	}
	if(tmb.t_suspend(msg, &ai->tindex, &ai->tlabel) < 0) {
		LM_ERR("failed to suspend the processing\n");
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
	sr_kemi_eng_t *keng = NULL;
	str cbname = STR_NULL;
	str evname = str_init("async:timer-exec");

	if(_async_list_head == NULL)
		return;

	slot = ticks % ASYNC_RING_SIZE;

	while(1) {
		lock_get(&_async_list_head->ring[slot].lock);
		ai = _async_list_head->ring[slot].lstart;
		if(ai != NULL)
			_async_list_head->ring[slot].lstart = ai->next;
		lock_release(&_async_list_head->ring[slot].lock);

		if(ai == NULL)
			break;
		if(ai->ract != NULL) {
			tmb.t_continue(ai->tindex, ai->tlabel, ai->ract);
			ksr_msg_env_reset();
		} else {
			keng = sr_kemi_eng_get();
			if(keng != NULL && ai->cbname_len>0) {
				cbname.s = ai->cbname;
				cbname.len = ai->cbname_len;
				tmb.t_continue_cb(ai->tindex, ai->tlabel, &cbname, &evname);
				ksr_msg_env_reset();
			} else {
				LM_WARN("no callback to be executed\n");
			}
		}
		shm_free(ai);
	}
}

typedef struct async_task_param {
	unsigned int tindex;
	unsigned int tlabel;
	cfg_action_t *ract;
	char cbname[ASYNC_CBNAME_SIZE];
	int cbname_len;
} async_task_param_t;

/**
 *
 */
void async_exec_task(void *param)
{
	async_task_param_t *atp;
	sr_kemi_eng_t *keng = NULL;
	str cbname = STR_NULL;
	str evname = str_init("async:task-exec");

	atp = (async_task_param_t *)param;

	if(atp->ract != NULL) {
		tmb.t_continue(atp->tindex, atp->tlabel, atp->ract);
		ksr_msg_env_reset();
	} else {
		keng = sr_kemi_eng_get();
		if(keng != NULL && atp->cbname_len > 0) {
			cbname.s = atp->cbname;
			cbname.len = atp->cbname_len;
			tmb.t_continue_cb(atp->tindex, atp->tlabel, &cbname, &evname);
			ksr_msg_env_reset();
		} else {
			LM_WARN("no callback to be executed\n");
		}
	}
	/* param is freed along with the async task strucutre in core */
}

/**
 *
 */
int async_send_task(sip_msg_t *msg, cfg_action_t *act, str *cbname)
{
	async_task_t *at;
	tm_cell_t *t = 0;
	unsigned int tindex;
	unsigned int tlabel;
	int dsize;
	async_task_param_t *atp;

	if(cbname && cbname->len>=ASYNC_CBNAME_SIZE-1) {
		LM_ERR("callback name is too long: %.*s\n", cbname->len, cbname->s);
		return -1;
	}

	t = tmb.t_gett();
	if(t == NULL || t == T_UNDEFINED) {
		if(tmb.t_newtran(msg) < 0) {
			LM_ERR("cannot create the transaction\n");
			return -1;
		}
		t = tmb.t_gett();
		if(t == NULL || t == T_UNDEFINED) {
			LM_ERR("cannot lookup the transaction\n");
			return -1;
		}
	}
	dsize = sizeof(async_task_t) + sizeof(async_task_param_t);
	at = (async_task_t *)shm_malloc(dsize);
	if(at == NULL) {
		LM_ERR("no more shm memory\n");
		return -1;
	}
	memset(at, 0, dsize);
	if(tmb.t_suspend(msg, &tindex, &tlabel) < 0) {
		LM_ERR("failed to suspend the processing\n");
		shm_free(at);
		return -1;
	}
	at->exec = async_exec_task;
	at->param = (char *)at + sizeof(async_task_t);
	atp = (async_task_param_t *)at->param;
	atp->ract = act;
	atp->tindex = tindex;
	atp->tlabel = tlabel;
	if(cbname && cbname->len>0) {
		memcpy(atp->cbname, cbname->s, cbname->len);
		atp->cbname[cbname->len] = '\0';
		atp->cbname_len = cbname->len;
	}
	async_task_push(at);
	return 0;
}
