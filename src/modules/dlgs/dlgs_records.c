/**
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
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
#include "../../core/mem/shm_mem.h"
#include "../../core/mem/mem.h"
#include "../../core/hashes.h"
#include "../../core/trim.h"
#include "../../core/utils/sruid.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_from.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "dlgs_records.h"

#define dlgs_compute_hash(_s) core_case_hash(_s, 0, 0)
#define dlgs_get_index(_h, _size) (_h) & ((_size)-1)

extern int _dlgs_active_lifetime;
extern int _dlgs_init_lifetime;
extern int _dlgs_finish_lifetime;
extern int _dlgs_htsize;
extern sruid_t _dlgs_sruid;

static dlgs_ht_t *_dlgs_htb = NULL;

/**
 *
 */
int dlgs_init(void)
{
	if (_dlgs_htb!=NULL) {
		return 0;
	}
	_dlgs_htb = dlgs_ht_init();
	if(_dlgs_htb==NULL) {
		return -1;
	}
	return 0;
}

/**
 *
 */
int dlgs_destroy(void)
{
	if (_dlgs_htb!=NULL) {
		return 0;
	}
	dlgs_ht_destroy();
	_dlgs_htb = NULL;

	return 0;
}

/**
 *
 */
int dlgs_sipfields_get(sip_msg_t *msg, dlgs_sipfields_t *sf)
{
	memset(sf, 0, sizeof(dlgs_sipfields_t));

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("failed to parse the request headers\n");
		return -1;
	}
	if (parse_headers(msg, HDR_CALLID_F|HDR_TO_F, 0)<0 || !msg->callid
			|| !msg->to ) {
		LM_ERR("bad request or missing Call-Id or To headers\n");
		return -1;
	}
	if (get_to(msg)->tag_value.len>0) {
		sf->ttag = get_to(msg)->tag_value;
	}
	if (parse_from_header(msg)<0 || get_from(msg)->tag_value.len==0) {
		LM_ERR("failed to get From header\n");
		return -1;
	}

	/* callid */
	sf->callid = msg->callid->body;
	trim(&sf->callid);
	/* from tag */
	sf->ftag = get_from(msg)->tag_value;

	return 0;
}

dlgs_item_t *dlgs_item_new(sip_msg_t *msg, dlgs_sipfields_t *sf, str *src,
			str *dst, str *data, unsigned int hashid)
{
	dlgs_item_t *item;
	unsigned int msize;
	str ruid = STR_NULL;
	char ruidbuf[SRUID_SIZE + 16];

	if(msg->first_line.u.request.method_value != METHOD_INVITE) {
		LM_ERR("executed for non-INVITE request\n");
		return NULL;
	}

	if(sruid_next_safe(&_dlgs_sruid)<0) {
		return NULL;
	}
	ruid.len = snprintf(ruidbuf, SRUID_SIZE + 16, "%.*s-%x", _dlgs_sruid.uid.len,
			_dlgs_sruid.uid.s, hashid);
	if(ruid.len<=0 || ruid.len>=SRUID_SIZE + 16) {
		LM_ERR("failed to generate dlg ruid\n");
		return NULL;
	}
	ruid.s = ruidbuf;

	msize = sizeof(dlgs_item_t) + (sf->callid.len + 1 + sf->ftag.len + 1
			+ ((sf->ttag.len>0)?(sf->ttag.len+1):DLGS_TOTAG_SIZE) + ruid.len + 1
			+ dst->len + 1 + src->len + 1 + data->len + 1) * sizeof(char);

	item = (dlgs_item_t *)shm_malloc(msize);
	if(item == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}

	memset(item, 0, msize);
	item->ts_init = time(NULL);
	item->hashid = hashid;

	item->callid.len = sf->callid.len;
	item->callid.s = (char *)item + sizeof(dlgs_item_t);
	memcpy(item->callid.s, sf->callid.s, sf->callid.len);

	item->ftag.len = sf->ftag.len;
	item->ftag.s = item->callid.s + item->callid.len + 1;
	memcpy(item->ftag.s, sf->ftag.s, sf->ftag.len);

	item->ttag.len = sf->ttag.len;
	item->ttag.s = item->ftag.s + item->ftag.len + 1;
	if(sf->ttag.len>0) {
		memcpy(item->ttag.s, sf->ttag.s, sf->ttag.len);
	}

	item->ruid.len = ruid.len;
	item->ruid.s = item->ttag.s
					+ ((item->ttag.len>0)?(item->ttag.len + 1):DLGS_TOTAG_SIZE);
	memcpy(item->ruid.s, ruid.s, ruid.len);

	item->src.len = src->len;
	item->src.s = item->ruid.s + item->ruid.len + 1;
	memcpy(item->src.s, src->s, src->len);

	item->dst.len = dst->len;
	item->dst.s = item->src.s + item->src.len + 1;
	memcpy(item->dst.s, dst->s, dst->len);

	item->data.len = data->len;
	item->data.s = item->dst.s + item->dst.len + 1;
	memcpy(item->data.s, data->s, data->len);

	return item;
}

int dlgs_item_free(dlgs_item_t *item)
{
	if(item == NULL) {
		return -1;
	}
	shm_free(item);
	return 0;
}


dlgs_ht_t *dlgs_ht_init(void)
{
	int i;
	dlgs_ht_t *dsht = NULL;

	dsht = (dlgs_ht_t *)shm_malloc(sizeof(dlgs_ht_t));
	if(dsht == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(dsht, 0, sizeof(dlgs_ht_t));
	dsht->htsize = _dlgs_htsize;
	dsht->alifetime = _dlgs_active_lifetime;
	dsht->ilifetime = _dlgs_init_lifetime;
	dsht->flifetime = _dlgs_finish_lifetime;

	dsht->slots = (dlgs_slot_t*)shm_malloc(dsht->htsize * sizeof(dlgs_slot_t));
	if(dsht->slots == NULL) {
		SHM_MEM_ERROR;
		shm_free(dsht);
		dsht = NULL;
		return NULL;
	}
	memset(dsht->slots, 0, dsht->htsize * sizeof(dlgs_slot_t));

	for(i = 0; i < dsht->htsize; i++) {
		if(lock_init(&dsht->slots[i].lock) == 0) {
			LM_ERR("cannot initialize lock[%d]\n", i);
			i--;
			while(i >= 0) {
				lock_destroy(&dsht->slots[i].lock);
				i--;
			}
			shm_free(dsht->slots);
			shm_free(dsht);
			dsht = NULL;
			return NULL;
		}
	}

	return dsht;
}

int dlgs_ht_destroy(void)
{
	int i;
	dlgs_item_t *it, *it0;
	dlgs_ht_t *dsht;

	dsht = _dlgs_htb;
	if(dsht == NULL) {
		return -1;
	}

	for(i = 0; i < dsht->htsize; i++) {
		/* free entries */
		it = dsht->slots[i].first;
		while(it) {
			it0 = it;
			it = it->next;
			dlgs_item_free(it0);
		}
		/* free locks */
		lock_destroy(&dsht->slots[i].lock);
	}
	shm_free(dsht->slots);
	shm_free(dsht);
	dsht = NULL;
	return 0;
}


int dlgs_add_item(sip_msg_t *msg, str *src, str *dst, str *data)
{
	unsigned int idx;
	unsigned int hid;
	dlgs_item_t *it, *prev, *nitem;
	dlgs_sipfields_t sf;
	dlgs_ht_t *dsht;

	dsht = _dlgs_htb;
	if(dsht == NULL || dsht->slots == NULL) {
		LM_ERR("invalid parameters.\n");
		return -1;
	}

	if(dlgs_sipfields_get(msg, &sf) < 0) {
		LM_ERR("failed to fill sip message attributes\n");
		return -1;
	}

	hid = dlgs_compute_hash(&sf.callid);

	idx = dlgs_get_index(hid, dsht->htsize);

	prev = NULL;
	lock_get(&dsht->slots[idx].lock);
	it = dsht->slots[idx].first;
	while(it != NULL && it->hashid < hid) {
		prev = it;
		it = it->next;
	}
	while(it != NULL && it->hashid == hid) {
		if(sf.callid.len == it->callid.len
				&& strncmp(sf.callid.s, it->callid.s, sf.callid.len) == 0) {
			lock_release(&dsht->slots[idx].lock);
			LM_DBG("call-id already in hash table [%.*s].\n", sf.callid.len,
					sf.callid.s);
			return 1;
		}
		prev = it;
		it = it->next;
	}
	/* add */
	nitem = dlgs_item_new(msg, &sf, src, dst, data, hid);
	if(nitem == NULL) {
		LM_ERR("cannot create new cell.\n");
		lock_release(&dsht->slots[idx].lock);
		return -1;
	}

	if(prev == NULL) {
		if(dsht->slots[idx].first != NULL) {
			nitem->next = dsht->slots[idx].first;
			dsht->slots[idx].first->prev = nitem;
		}
		dsht->slots[idx].first = nitem;
	} else {
		nitem->next = prev->next;
		nitem->prev = prev;
		if(prev->next)
			prev->next->prev = nitem;
		prev->next = nitem;
	}
	dsht->slots[idx].esize++;
	lock_release(&dsht->slots[idx].lock);
	return 0;
}

int dlgs_unlock_item(sip_msg_t *msg)
{
	unsigned int idx;
	unsigned int hid;
	str *cid;
	dlgs_sipfields_t sf;
	dlgs_ht_t *dsht;

	dsht = _dlgs_htb;
	if(dsht == NULL || dsht->slots == NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(dlgs_sipfields_get(msg, &sf) < 0) {
		LM_ERR("failed to fill sip message attributes\n");
		return -1;
	}
	cid = &sf.callid;

	hid = dlgs_compute_hash(cid);

	idx = dlgs_get_index(hid, dsht->htsize);

	/* head test and return */
	if(dsht->slots[idx].first == NULL)
		return 0;

	lock_release(&dsht->slots[idx].lock);
	return 0;
}

dlgs_item_t *dlgs_get_item(sip_msg_t *msg)
{
	unsigned int idx;
	unsigned int hid;
	dlgs_item_t *it;
	str *cid;
	dlgs_sipfields_t sf;
	dlgs_ht_t *dsht;

	dsht = _dlgs_htb;
	if(dsht == NULL || dsht->slots == NULL) {
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	if(dlgs_sipfields_get(msg, &sf) < 0) {
		LM_ERR("failed to fill sip message attributes\n");
		return NULL;
	}
	cid = &sf.callid;

	hid = dlgs_compute_hash(cid);

	idx = dlgs_get_index(hid, dsht->htsize);

	/* head test and return */
	if(dsht->slots[idx].first == NULL)
		return 0;

	lock_get(&dsht->slots[idx].lock);
	it = dsht->slots[idx].first;
	while(it != NULL && it->hashid < hid)
		it = it->next;
	while(it != NULL && it->hashid == hid) {
		if(cid->len == it->callid.len
				&& strncmp(cid->s, it->callid.s, cid->len) == 0) {
			/* found */
			return it;
		}
		it = it->next;
	}
	lock_release(&dsht->slots[idx].lock);
	return 0;
}


int dlgs_del_item(sip_msg_t *msg)
{
	unsigned int idx;
	unsigned int hid;
	dlgs_item_t *it;
	str *cid;
	dlgs_sipfields_t sf;
	dlgs_ht_t *dsht;

	dsht = _dlgs_htb;
	if(dsht == NULL || dsht->slots == NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(dlgs_sipfields_get(msg, &sf) < 0) {
		LM_ERR("failed to fill sip message attributes\n");
		return -1;
	}
	cid = &sf.callid;

	hid = dlgs_compute_hash(cid);

	idx = dlgs_get_index(hid, dsht->htsize);

	/* head test and return */
	if(dsht->slots[idx].first == NULL)
		return 0;

	lock_get(&dsht->slots[idx].lock);
	it = dsht->slots[idx].first;
	while(it != NULL && it->hashid < hid)
		it = it->next;
	while(it != NULL && it->hashid == hid) {
		if(cid->len == it->callid.len
				&& strncmp(cid->s, it->callid.s, cid->len) == 0) {
			/* found */
			if(it->prev == NULL)
				dsht->slots[idx].first = it->next;
			else
				it->prev->next = it->next;
			if(it->next)
				it->next->prev = it->prev;
			dsht->slots[idx].esize--;
			lock_release(&dsht->slots[idx].lock);
			dlgs_item_free(it);
			return 0;
		}
		it = it->next;
	}
	lock_release(&dsht->slots[idx].lock);
	return 0;
}

/**
 *
 */
int dlgs_ht_dbg(void)
{
	int i;
	dlgs_item_t *it;
	dlgs_ht_t *dsht;

	dsht = _dlgs_htb;
	for(i = 0; i < dsht->htsize; i++) {
		lock_get(&dsht->slots[i].lock);
		LM_ERR("htable[%d] -- <%d>\n", i, dsht->slots[i].esize);
		it = dsht->slots[i].first;
		while(it) {
			LM_ERR("\tcallid: %.*s\n", it->callid.len, it->callid.s);
			LM_ERR("\tftag: %.*s\n", it->ftag.len, it->ftag.s);
			LM_ERR("\tttag: %.*s\n", it->ttag.len, it->ttag.s);
			LM_ERR("\tsrc: %.*s\n", it->src.len, it->src.s);
			LM_ERR("\tdst: %.*s\n", it->dst.len, it->dst.s);
			LM_ERR("\tdata: %.*s\n", it->data.len, it->data.s);
			LM_ERR("\truid: %.*s\n", it->ruid.len, it->ruid.s);
			LM_ERR("\thashid: %u ts_init: %u ts_answer: %u\n", it->hashid,
					(unsigned int)it->ts_init, (unsigned int)it->ts_answer);
			it = it->next;
		}
		lock_release(&dsht->slots[i].lock);
	}
	return 0;
}

/**
 *
 */
void dlgs_update_stats(dlgs_stats_t *stats, int state, int val)
{
	switch(state) {
		case DLGS_STATE_INIT:
			stats->c_init += val;
			return;
		case DLGS_STATE_PROGRESS:
			stats->c_progress += val;
			return;
		case DLGS_STATE_ANSWERED:
			stats->c_answered += val;
			return;
		case DLGS_STATE_CONFIRMED:
			stats->c_confirmed += val;
			return;
		case DLGS_STATE_TERMINATED:
			stats->c_terminted += val;
			return;
		case DLGS_STATE_NOTANSWERED:
			stats->c_notanswered += val;
			return;
	}
}

/**
 *
 */
int dlgs_update_item(sip_msg_t *msg)
{
	unsigned int idx;
	int rtype = 0;
	int rmethod = 0;
	int rcode = 0;
	int ostate = 0;
	int nstate = 0;
	dlgs_item_t *it;
	time_t tnow;

	if(msg->first_line.type == SIP_REQUEST) {
		rtype = SIP_REQUEST;
		if(msg->first_line.u.request.method_value == METHOD_INVITE) {
			rmethod = METHOD_INVITE;
		} else {
			rmethod = msg->first_line.u.request.method_value;
		}
	} else {
		rtype = SIP_REPLY;
		if(msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ_F, 0)==-1) ||
				(msg->cseq==NULL))) {
			LM_ERR("no CSEQ header\n");
			return -1;
		}
		rmethod = get_cseq(msg)->method_id;
		rcode = (int)msg->first_line.u.reply.statuscode;
	}

	tnow = time(NULL);

	it = dlgs_get_item(msg);
	if(it==NULL) {
		LM_DBG("no matching item found\n");
		return 0;
	}
	idx = dlgs_get_index(it->hashid, _dlgs_htb->htsize);
	ostate = it->state;
	if(rtype == SIP_REQUEST) {
		switch(rmethod) {
			case METHOD_ACK:
				if(it->state==DLGS_STATE_ANSWERED) {
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, -1);
					it->state = DLGS_STATE_CONFIRMED;
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, 1);
				}
			break;
			case METHOD_CANCEL:
				if(it->state<DLGS_STATE_ANSWERED) {
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, -1);
					it->state = DLGS_STATE_NOTANSWERED;
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, 1);
					it->ts_finish = tnow;
				}
			break;
			case METHOD_BYE:
				if(it->state==DLGS_STATE_ANSWERED
						|| it->state==DLGS_STATE_CONFIRMED) {
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, -1);
					it->state = DLGS_STATE_TERMINATED;
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, 1);
					it->ts_finish = tnow;
				}
			break;
		}
		goto done;
	}

	switch(rmethod) {
		case METHOD_INVITE:
			if(rcode>=100 && rcode<200) {
				if(it->state==DLGS_STATE_INIT) {
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, -1);
					it->state = DLGS_STATE_PROGRESS;
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, 1);
				}
			} else if(rcode>=200 && rcode<300) {
				if(it->state==DLGS_STATE_INIT
						|| it->state==DLGS_STATE_PROGRESS) {
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, -1);
					it->state = DLGS_STATE_ANSWERED;
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, 1);
					it->ts_answer = tnow;
					if(it->ttag.len<=0) {
						to_body_t *tb;
						tb = get_to(msg);
						if(tb!=NULL && tb->tag_value.len>0
								&& (tb->tag_value.len<DLGS_TOTAG_SIZE-1)) {
							it->ttag.len = tb->tag_value.len;
							memcpy(it->ttag.s, tb->tag_value.s, tb->tag_value.len);
						}
					}
				}
			} else if(rcode>=300) {
				if(it->state==DLGS_STATE_INIT
						|| it->state==DLGS_STATE_PROGRESS) {
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, -1);
					it->state = DLGS_STATE_NOTANSWERED;
					dlgs_update_stats(&_dlgs_htb->slots[idx].astats, it->state, 1);
					it->ts_finish = tnow;
				}
			}
		break;
	}

done:
	nstate = it->state;
	dlgs_unlock_item(msg);
	LM_DBG("old state %d - new state %d\n", ostate, nstate);
	return 0;
}

/**
 *
 */
void dlgs_ht_timer(unsigned int ticks, void *param)
{
	time_t tnow;
	int i;
	dlgs_item_t *it;
	dlgs_item_t *ite;

	if(_dlgs_htb == NULL) {
		return;
	}

	tnow = time(NULL);

	for(i = 0; i < _dlgs_htb->htsize; i++) {
		lock_get(&_dlgs_htb->slots[i].lock);
		it = _dlgs_htb->slots[i].first;
		while(it) {
			ite = NULL;
			if(it->state == DLGS_STATE_INIT || it->state == DLGS_STATE_PROGRESS
					|| it->state == DLGS_STATE_ANSWERED) {
				if(it->ts_init + _dlgs_htb->ilifetime < tnow) {
					ite = it;
				}
			} else if(it->state == DLGS_STATE_CONFIRMED) {
				if(it->ts_answer + _dlgs_htb->alifetime < tnow) {
					ite = it;
				}
			} else if(it->state == DLGS_STATE_NOTANSWERED
					|| it->state == DLGS_STATE_TERMINATED) {
				if(it->ts_finish + _dlgs_htb->flifetime < tnow) {
					ite = it;
				}
			}
			it = it->next;
			if(ite != NULL) {
				if(ite==_dlgs_htb->slots[i].first) {
					_dlgs_htb->slots[i].first = it;
					if(it!=NULL) {
						it->prev = NULL;
					}
				} else {
					ite->prev->next = it;
					if(it!=NULL) {
						it->prev = ite->prev;
					}
				}
				dlgs_item_free(ite);
			}
		}
		lock_release(&_dlgs_htb->slots[i].lock);
	}

	return;
}

static const char *dlgs_rpc_stats_doc[2] = {
	"Stats of the dlgs records",
	0
};


/*
 * RPC command to list the stats of the records
 */
static void dlgs_rpc_stats(rpc_t *rpc, void *ctx)
{
}

static const char *dlgs_rpc_list_doc[2] = {
	"List the dlgs records",
	0
};


/*
 * RPC command to list the records
 */
static void dlgs_rpc_list(rpc_t *rpc, void *ctx)
{
	dlgs_item_t *it;
	int n = 0;
	int i;
	void *th;

	if(_dlgs_htb == NULL) {
		return;
	}

	for(i = 0; i < _dlgs_htb->htsize; i++) {
		lock_get(&_dlgs_htb->slots[i].lock);
		it = _dlgs_htb->slots[i].first;
		while(it) {
			if (rpc->add(ctx, "{", &th) < 0) {
				lock_release(&_dlgs_htb->slots[i].lock);
				rpc->fault(ctx, 500, "Internal error creating rpc");
				return;
			}
			if(rpc->struct_add(th, "dSSSSSSSuuu",
							"count", ++n,
							"src", &it->src,
							"dst", &it->dst,
							"data", &it->data,
							"ruid", &it->ruid,
							"callid", &it->callid,
							"ftag", &it->ftag,
							"ttag", &it->ttag,
							"ts_init", (unsigned int)it->ts_init,
							"ts_answer", (unsigned int)it->ts_answer,
							"state", it->state)<0) {
				lock_release(&_dlgs_htb->slots[i].lock);
				rpc->fault(ctx, 500, "Internal error creating item");
				return;
			}
			it = it->next;
		}
		lock_release(&_dlgs_htb->slots[i].lock);
	}
}

/* clang-format off */
rpc_export_t dlgs_rpc_cmds[] = {
	{"dlgs.stats", dlgs_rpc_stats,
		dlgs_rpc_stats_doc, 0},
	{"dlgs.list",  dlgs_rpc_list,
		dlgs_rpc_list_doc, RET_ARRAY},

	{0, 0, 0, 0}
};
/* clang-format on */

/**
 *
 */
int dlgs_rpc_init(void)
{
	if(rpc_register_array(dlgs_rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
