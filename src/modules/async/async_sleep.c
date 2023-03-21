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
#include "../../core/script_cb.h"
#include "../../core/fmsg.h"
#include "../../core/route.h"
#include "../../core/kemi.h"

#include "async_sleep.h"

#define ASYNC_CBNAME_SIZE 64
/* tm */
extern struct tm_binds tmb;

/* clang-format off */
typedef struct async_task_param {
	unsigned int tindex;
	unsigned int tlabel;
	cfg_action_t *ract;
	char cbname[ASYNC_CBNAME_SIZE];
	int cbname_len;
} async_task_param_t;

typedef struct async_item {
	unsigned int tindex;
	unsigned int tlabel;
	unsigned int ticks;
	cfg_action_t *ract;
	char cbname[ASYNC_CBNAME_SIZE];
	int cbname_len;
	struct async_item *next;
} async_item_t;

typedef struct async_ms_item {
	async_task_t *at;
	struct timeval due;
	struct async_ms_item *next;
} async_ms_item_t;

typedef struct async_slot {
	async_item_t *lstart;
	async_item_t *lend;
	gen_lock_t lock;
} async_slot_t;

#define ASYNC_RING_SIZE	100
#define MAX_MS_SLEEP 30*1000
#define MAX_MS_SLEEP_QUEUE 10000

static struct async_ms_list {
	async_ms_item_t *lstart;
	async_ms_item_t *lend;
	int	len;
	gen_lock_t lock;
} *_async_ms_list = NULL;

static struct async_list_head {
	async_slot_t ring[ASYNC_RING_SIZE];
	async_slot_t *later;
} *_async_list_head = NULL;

typedef struct async_data_param {
	int dtype;
	str sval;
	cfg_action_t *ract;
	char cbname[ASYNC_CBNAME_SIZE];
	int cbname_len;
} async_data_param_t;

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
		SHM_MEM_ERROR;
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

int async_init_ms_timer_list(void)
{
	_async_ms_list = (struct async_ms_list *)shm_malloc(
			sizeof(struct async_ms_list));
	if(_async_ms_list == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(_async_ms_list, 0, sizeof(struct async_ms_list));
	if(lock_init(&_async_ms_list->lock) == 0) {
		LM_ERR("cannot init lock \n");
		shm_free(_async_ms_list);
		_async_ms_list = NULL;
		return -1;
	}
	return 0;
}

int async_destroy_ms_timer_list(void)
{
	if (_async_ms_list) {
		lock_destroy(&_async_ms_list->lock);
		shm_free(_async_ms_list);
		_async_ms_list = NULL;
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

int async_insert_item(async_ms_item_t *ai)
{
	struct timeval *due = &ai->due;

	if (unlikely(_async_ms_list == NULL))
		return -1;
	lock_get(&_async_ms_list->lock);
	// Check if we want to insert in front
	if (_async_ms_list->lstart == NULL || timercmp(due, &_async_ms_list->lstart->due, <=)) {
		ai->next = _async_ms_list->lstart;
		_async_ms_list->lstart = ai;
		if (_async_ms_list->lend == NULL)
			_async_ms_list->lend = ai;
	} else {
		// Check if we want to add to the tail
		if (_async_ms_list->lend && timercmp(due, &_async_ms_list->lend->due, >)) {
			_async_ms_list->lend->next = ai;
			_async_ms_list->lend = ai;
		} else {
			async_ms_item_t *aip;
			// Find the place to insert into a sorted timer list
			// Most likely head && tail scanarios are covered above
			int i = 1;
			for (aip = _async_ms_list->lstart; aip->next; aip = aip->next, i++) {
				if (timercmp(due, &aip->next->due, <=)) {
					ai->next = aip->next;
					aip->next = ai;
					break;
				}
			}
		}
	}
	_async_ms_list->len++;
	lock_release(&_async_ms_list->lock);
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
		SHM_MEM_ERROR;
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

static unsigned int _async_timer_exec_last_slot = -1;

void async_timer_exec(unsigned int ticks, void *param)
{
	unsigned int idx;
	unsigned int slot;
	async_item_t *ai;
	sr_kemi_eng_t *keng = NULL;
	str cbname = STR_NULL;
	str evname = str_init("async:timer-exec");

	if(_async_list_head == NULL)
		return;

	idx = ticks % ASYNC_RING_SIZE;

	if(idx == _async_timer_exec_last_slot) {
		/* timer faster than 1sec */
		return;
	}

	if(_async_timer_exec_last_slot < 0) {
		_async_timer_exec_last_slot = idx;
	}
	slot = (_async_timer_exec_last_slot + 1) % ASYNC_RING_SIZE;
	if(slot != idx) {
		LM_DBG("need to catch up from slot %u to %u (slots: %u)\n", slot, idx,
				ASYNC_RING_SIZE);
	}

	do {
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
		if(slot == idx) {
			break;
		}
		slot = (slot + 1) % ASYNC_RING_SIZE;
	} while(1);

	_async_timer_exec_last_slot = idx;
}

void async_mstimer_exec(unsigned int ticks, void *param)
{
	struct timeval now;
	gettimeofday(&now, NULL);

	if (_async_ms_list == NULL)
		return;
	lock_get(&_async_ms_list->lock);

	async_ms_item_t *aip, *next;
	int i = 0;
	for (aip = _async_ms_list->lstart; aip; aip = next, i++) {
		next = aip->next;
		if (timercmp(&now, &aip->due, >=)) {
			if ((_async_ms_list->lstart = next) == NULL)
				_async_ms_list->lend = NULL;
			if (async_task_push(aip->at)<0) {
				shm_free(aip->at);
			}
			_async_ms_list->len--;
			continue;
		}
		break;
	}

	lock_release(&_async_ms_list->lock);

	return;

}


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

int async_ms_sleep(sip_msg_t *msg, int milliseconds, cfg_action_t *act, str *cbname)
{
	async_ms_item_t *ai;
	int dsize;
	tm_cell_t *t = 0;
	unsigned int tindex;
	unsigned int tlabel;
	async_task_param_t *atp;
	async_task_t *at;

	if (_async_ms_list==NULL) {
		LM_ERR("async timer list not initialized - check modparams\n");
		return -1;
	}
	if(milliseconds <= 0) {
		LM_ERR("negative or zero sleep time (%d)\n", milliseconds);
		return -1;
	}
	if(milliseconds >= MAX_MS_SLEEP) {
		LM_ERR("max sleep time is %d msec\n", MAX_MS_SLEEP);
		return -1;
	}
	if(_async_ms_list->len >= MAX_MS_SLEEP_QUEUE) {
		LM_ERR("max sleep queue length exceeded (%d) \n", MAX_MS_SLEEP_QUEUE);
		return -1;
	}
	if(cbname && cbname->len>=ASYNC_CBNAME_SIZE-1) {
		LM_ERR("callback name is too long: %.*s\n", cbname->len, cbname->s);
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

	if(tmb.t_suspend(msg, &tindex, &tlabel) < 0) {
		LM_ERR("failed to suspend the processing\n");
		return -1;
	}

	dsize = sizeof(async_task_t) + sizeof(async_task_param_t) + sizeof(async_ms_item_t);

	at = (async_task_t *)shm_malloc(dsize);
	if(at == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(at, 0, dsize);
	at->param = (char *)at + sizeof(async_task_t);
	atp = (async_task_param_t *)at->param;
	ai = (async_ms_item_t *) ((char *)at +  sizeof(async_task_t) + sizeof(async_task_param_t));
	ai->at = at;

	at->exec = async_exec_task;
	at->param = atp;
	atp->ract = act;
	atp->tindex = tindex;
	atp->tlabel = tlabel;
	if(cbname && cbname->len>0) {
		memcpy(atp->cbname, cbname->s, cbname->len);
		atp->cbname[cbname->len] = '\0';
		atp->cbname_len = cbname->len;
	}

	struct timeval now, upause;
	gettimeofday(&now, NULL);
	upause.tv_sec = milliseconds / 1000;
	upause.tv_usec = (milliseconds * 1000) % 1000000;

	timeradd(&now, &upause, &ai->due);
	async_insert_item(ai);

	return 0;
}

/**
 *
 */
int async_send_task(sip_msg_t *msg, cfg_action_t *act, str *cbname, str *gname)
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
		SHM_MEM_ERROR;
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

	if (gname!=NULL && gname->len>0) {
		if (async_task_group_push(gname, at)<0) {
			shm_free(at);
			return -1;
		}
	} else {
		if (async_task_push(at)<0) {
			shm_free(at);
			return -1;
		}
	}

	return 0;
}

async_data_param_t *_ksr_async_data_param = NULL;

/**
 *
 */
void async_exec_data(void *param)
{
	async_data_param_t *adp;
	sr_kemi_eng_t *keng = NULL;
	sip_msg_t *fmsg;
	str cbname = STR_NULL;
	str evname = str_init("async:task-data");
	int rtype = 0;

	adp = (async_data_param_t *)param;
	fmsg = faked_msg_next();
	if (exec_pre_script_cb(fmsg, REQUEST_CB_TYPE)==0) {
		return;
	}
	rtype = get_route_type();
	_ksr_async_data_param = adp;
	set_route_type(REQUEST_ROUTE);
	keng = sr_kemi_eng_get();
	if(adp->ract != NULL) {
		run_top_route(adp->ract, fmsg, 0);
	} else {
		keng = sr_kemi_eng_get();
		if(keng != NULL && adp->cbname_len > 0) {
			cbname.s = adp->cbname;
			cbname.len = adp->cbname_len;
			if(sr_kemi_route(keng, fmsg, EVENT_ROUTE, &cbname, &evname)<0) {
				LM_ERR("error running event route kemi callback [%.*s]\n",
						cbname.len, cbname.s);
			}
		}
	}
	exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
	ksr_msg_env_reset();
	set_route_type(rtype);
	_ksr_async_data_param = NULL;
	/* param is freed along with the async task strucutre in core */
}

/**
 *
 */
int async_send_data(sip_msg_t *msg, cfg_action_t *act, str *cbname, str *gname,
		str *sdata)
{
	async_task_t *at;
	int dsize;
	async_data_param_t *adp;

	if(cbname && cbname->len>=ASYNC_CBNAME_SIZE-1) {
		LM_ERR("callback name is too long: %.*s\n", cbname->len, cbname->s);
		return -1;
	}

	dsize = sizeof(async_task_t) + sizeof(async_data_param_t) + sdata->len + 1;
	at = (async_task_t *)shm_malloc(dsize);
	if(at == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(at, 0, dsize);
	at->exec = async_exec_data;
	at->param = (char *)at + sizeof(async_task_t);
	adp = (async_data_param_t *)at->param;
	adp->sval.s = (char*)adp + sizeof(async_data_param_t);
	adp->sval.len = sdata->len;
	memcpy(adp->sval.s, sdata->s, sdata->len);
	adp->ract = act;
	if(cbname && cbname->len>0) {
		memcpy(adp->cbname, cbname->s, cbname->len);
		adp->cbname_len = cbname->len;
	}

	if (gname!=NULL && gname->len>0) {
		if (async_task_group_push(gname, at)<0) {
			shm_free(at);
			return -1;
		}
	} else {
		if (async_task_push(at)<0) {
			shm_free(at);
			return -1;
		}
	}

	return 0;
}


/**
 *
 */
int pv_get_async(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	async_wgroup_t *awg = NULL;

	switch(param->pvn.u.isname.name.n) {
		case 0:
			if(_ksr_async_data_param==NULL || _ksr_async_data_param->sval.s==NULL
					|| _ksr_async_data_param->sval.len<0) {
				return pv_get_null(msg, param, res);
			}
			return pv_get_strval(msg, param, res, &_ksr_async_data_param->sval);
		case 1:
			awg = async_task_workers_get_crt();
			if(awg==NULL || awg->name.s==NULL || awg->name.len<0) {
				return pv_get_null(msg, param, res);
			}
			return pv_get_strval(msg, param, res, &awg->name);
		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
int pv_parse_async_name(pv_spec_t *sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len) {
		case 4:
			if(strncmp(in->s, "data", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
		break;
		case 5:
			if(strncmp(in->s, "gname", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV time name %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
static sr_kemi_xval_t _ksr_kemi_async_xval = {0};

/**
 *
 */
sr_kemi_xval_t* ki_async_get_gname(sip_msg_t *msg)
{
	async_wgroup_t *awg = NULL;

	memset(&_ksr_kemi_async_xval, 0, sizeof(sr_kemi_xval_t));

	awg = async_task_workers_get_crt();
	if(awg==NULL || awg->name.s==NULL || awg->name.len<0) {
		sr_kemi_xval_null(&_ksr_kemi_async_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_async_xval;
	}
	_ksr_kemi_async_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_async_xval.v.s = awg->name;
	return &_ksr_kemi_async_xval;
}

/**
 *
 */
sr_kemi_xval_t* ki_async_get_data(sip_msg_t *msg)
{
	memset(&_ksr_kemi_async_xval, 0, sizeof(sr_kemi_xval_t));

	if(_ksr_async_data_param==NULL || _ksr_async_data_param->sval.s==NULL
			|| _ksr_async_data_param->sval.len<0) {
		sr_kemi_xval_null(&_ksr_kemi_async_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_async_xval;
	}
	_ksr_kemi_async_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_async_xval.v.s = _ksr_async_data_param->sval;
	return &_ksr_kemi_async_xval;
}
