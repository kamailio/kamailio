/**
 * Copyright (C) 2022 Daniel-Constantin Mierla (asipto.com)
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
#include <string.h>
#include <stdlib.h>

#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"
#include "../../core/hashes.h"
#include "../../core/config.h"
#include "../../core/parser/parse_via.h"
#include "../../core/parser/parse_from.h"
#include "../../core/route.h"
#include "../../core/trim.h"
#include "../../core/ut.h"
#include "../../core/receive.h"
#include "../../core/route.h"
#include "../../core/kemi.h"

#include "siprepo_data.h"


static siprepo_slot_t *_siprepo_table = NULL;
extern int _siprepo_table_size;
extern int _siprepo_expire;

/**
 *
 */
int siprepo_table_init(void)
{
	int n;

	_siprepo_table = (siprepo_slot_t*)shm_malloc(_siprepo_table_size
			* sizeof(siprepo_slot_t));
	if(_siprepo_table == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(_siprepo_table, 0, _siprepo_table_size * sizeof(siprepo_slot_t));
	for(n=0; n<_siprepo_table_size; n++) {
		if(lock_init(&_siprepo_table[n].lock)==NULL) {
			LM_ERR("cannot init the lock %d\n", n);
			n--;
			while(n>=0) {
				lock_destroy(&_siprepo_table[n].lock);
				n--;
			}
			shm_free(_siprepo_table);
			_siprepo_table = 0;
			return -1;
		}
	}
	return 0;
}

/**
 *
 */
siprepo_msg_t *siprepo_msg_find(sip_msg_t *msg, str *callid, str *msgid, int lmode)
{
	unsigned int hid;
	unsigned int slotid;
	siprepo_msg_t *it;

	hid = get_hash1_raw(callid->s, callid->len);
	slotid = hid % _siprepo_table_size;

	lock_get(&_siprepo_table[slotid].lock);
	for(it=_siprepo_table[slotid].plist; it!=NULL; it=it->next) {
		if(hid==it->hid && callid->len==it->callid.len
				&& msgid->len==it->msgid.len
				&& memcmp(callid->s, it->callid.s, callid->len)==0
				&& memcmp(msgid->s, it->msgid.s, msgid->len)==0) {
			if(lmode==0) {
				lock_release(&_siprepo_table[slotid].lock);
			}
			return it;
		}
	}
	if(lmode==0) {
		lock_release(&_siprepo_table[slotid].lock);
	}

	return 0;
}

/**
 *
 */
int siprepo_msg_set(sip_msg_t *msg, str *msgid)
{
	unsigned int hid;
	unsigned int slotid;
	size_t dsize;
	struct via_param *vbr;
	str scallid;
	siprepo_msg_t *it = NULL;

	if(_siprepo_table == NULL) {
		LM_ERR("hash table not initialized\n");
		return -1;
	}

	if(parse_headers(msg, HDR_FROM_F|HDR_VIA1_F|HDR_CALLID_F|HDR_CSEQ_F, 0)<0) {
		LM_ERR("failed to parse required headers\n");
		return -1;
	}
	if (msg->callid==NULL || msg->callid->body.s==NULL) {
		LM_ERR("failed to parse callid headers\n");
		return -1;
	}
	if(msg->cseq==NULL || msg->cseq->parsed==NULL) {
		LM_ERR("failed to parse cseq headers\n");
		return -1;
	}
	if(get_cseq(msg)->method_id==METHOD_ACK
			|| get_cseq(msg)->method_id==METHOD_CANCEL) {
		LM_DBG("no pre-transaction management for ACK or CANCEL\n");
		return -1;
	}
	if (msg->via1==0) {
		LM_ERR("failed to get Via header\n");
		return -1;
	}
	if (parse_from_header(msg)<0 || get_from(msg)->tag_value.len==0) {
		LM_ERR("failed to get From header\n");
		return -1;
	}

	scallid = msg->callid->body;
	trim(&scallid);
	hid = get_hash1_raw(scallid.s, scallid.len);
	slotid = hid % _siprepo_table_size;

	if(siprepo_msg_find(msg, &scallid, msgid, 1)!=NULL) {
		LM_DBG("msg [%.*s] found in repo\n", msgid->len, msgid->s);
		lock_release(&_siprepo_table[slotid].lock);
		return 1;
	}

	dsize = ROUND_POINTER(sizeof(siprepo_msg_t))
				+ ROUND_POINTER(msgid->len + 1) + ROUND_POINTER(msg->len + 1);

	it = (siprepo_msg_t*)shm_malloc(dsize);
	if(it == NULL) {
		SHM_MEM_ERROR_FMT("new repo structure\n");
		lock_release(&_siprepo_table[slotid].lock);
		return -1;
	}
	memset(it, 0, dsize);

	it->dbuf.s = (char*)it + ROUND_POINTER(sizeof(siprepo_msg_t));
	it->callid.len = scallid.len;
	it->callid.s = translate_pointer(it->dbuf.s, msg->buf, it->callid.s);

	it->cseqmet = get_cseq(msg)->method;
	trim(&it->cseqmet);
	it->cseqnum = get_cseq(msg)->number;
	trim(&it->cseqnum);
	it->ftag = get_from(msg)->tag_value;
	trim(&it->ftag);

	vbr = msg->via1->branch;
	if(likely(vbr!=NULL)) {
		it->vbranch = vbr->value;
		trim(&it->vbranch);
	}

	it->hid = get_hash1_raw(it->callid.s, it->callid.len);
	slotid = it->hid % _siprepo_table_size;

	it->mtype = msg->first_line.type;
	it->itime = time(NULL);

	_siprepo_table[slotid].plist->prev = it;
	it->next = _siprepo_table[slotid].plist;
	_siprepo_table[slotid].plist = it;

	lock_release(&_siprepo_table[slotid].lock);

	return 0;
}

/**
 *
 */
int siprepo_msg_rm(sip_msg_t *msg, str *callid, str *msgid)
{
	unsigned int slotid;
	siprepo_msg_t *it = NULL;

	it = siprepo_msg_find(msg, callid, msgid, 1);
	if(it==NULL) {
		LM_DBG("msg [%.*s] not found in repo\n", msgid->len, msgid->s);
		slotid = get_hash1_raw(callid->s, callid->len) % _siprepo_table_size;
		lock_release(&_siprepo_table[slotid].lock);
		return 1;
	}
	slotid = it->hid % _siprepo_table_size;
	if(it->prev==NULL) {
		_siprepo_table[slotid].plist = it->next;
		if(_siprepo_table[slotid].plist) {
			_siprepo_table[slotid].plist->prev = NULL;
		}
	} else {
		it->prev->next = it->next;
	}
	if(it->next!=NULL) {
		it->next->prev = it->prev;
	}
	lock_release(&_siprepo_table[slotid].lock);
	shm_free(it);

	return 0;
}

/**
 *
 */
int siprepo_msg_check(sip_msg_t *msg)
{
	unsigned int hid;
	unsigned int slotid;
	str scallid;
	siprepo_msg_t *it = NULL;

	if(_siprepo_table == NULL) {
		LM_ERR("hash table not initialized\n");
		return -1;
	}

	if(parse_headers(msg, HDR_FROM_F|HDR_VIA1_F|HDR_CALLID_F|HDR_CSEQ_F, 0)<0) {
		LM_ERR("failed to parse required headers\n");
		return -1;
	}
	if (msg->callid==NULL || msg->callid->body.s==NULL) {
		LM_ERR("failed to parse callid headers\n");
		return -1;
	}
	if(msg->cseq==NULL || msg->cseq->parsed==NULL) {
		LM_ERR("failed to parse cseq headers\n");
		return -1;
	}
	if(get_cseq(msg)->method_id==METHOD_ACK
			|| get_cseq(msg)->method_id==METHOD_CANCEL) {
		LM_DBG("no pre-transaction management for ACK or CANCEL\n");
		return -1;
	}
	if (msg->via1==0) {
		LM_ERR("failed to get Via header\n");
		return -1;
	}
	if (parse_from_header(msg)<0 || get_from(msg)->tag_value.len==0) {
		LM_ERR("failed to get From header\n");
		return -1;
	}

	scallid = msg->callid->body;
	trim(&scallid);
	hid = get_hash1_raw(scallid.s, scallid.len);
	slotid = hid % _siprepo_table_size;

	lock_get(&_siprepo_table[slotid].lock);
	for(it=_siprepo_table[slotid].plist; it!=NULL; it=it->next) {
		if(hid==it->hid && scallid.len==it->callid.len
				&& memcmp(scallid.s, it->callid.s, scallid.len)==0) {
			lock_release(&_siprepo_table[slotid].lock);
			return 1;
		}
	}
	lock_release(&_siprepo_table[slotid].lock);

	return 0;
}

/**
 *
 */
int siprepo_msg_pull(sip_msg_t *msg, str *callid, str *msgid, str *rname)
{
	unsigned int slotid;
	sip_msg_t lmsg;
	siprepo_msg_t *it = NULL;
	int rtype;
	int rtbk;
	int rtno;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("siprepo:msg");

	it = siprepo_msg_find(msg, callid, msgid, 1);
	if(it==NULL) {
		LM_DBG("msg [%.*s] not found in repo\n", msgid->len, msgid->s);
		slotid = get_hash1_raw(callid->s, callid->len) % _siprepo_table_size;
		lock_release(&_siprepo_table[slotid].lock);
		return 1;
	}
	memset(&lmsg, 0, sizeof(sip_msg_t));
	slotid = it->hid % _siprepo_table_size;

	if(it->mtype==SIP_REQUEST) {
		rtype = REQUEST_ROUTE;
	} else {
		rtype = CORE_ONREPLY_ROUTE;
	}
	rtbk = get_route_type();
	set_route_type(REQUEST_ROUTE);
	keng = sr_kemi_eng_get();
	if(keng==NULL) {
		rtno = route_lookup(&main_rt, rname->s);
		if(rtno>=0 && main_rt.rlist[rtno]!=NULL) {
			run_top_route(main_rt.rlist[rtno], &lmsg, 0);
		}
	} else {
		if(sr_kemi_route(keng, &lmsg, rtype, rname, &evname)<0) {
			LM_ERR("error running route kemi callback [%.*s]\n",
					rname->len, rname->s);
		}
	}
	set_route_type(rtbk);
	ksr_msg_env_reset();

	return 0;
}

/**
 *
 */
void siprepo_timer_exec(unsigned int ticks, int worker, void *param)
{
	time_t tnow;
	int i;
	siprepo_msg_t *it = NULL;
	siprepo_msg_t *elist = NULL;

	tnow = time(NULL);
	for(i=0; i<_siprepo_table_size; i++) {
		lock_get(&_siprepo_table[i].lock);
		for(it=_siprepo_table[i].plist; it!=NULL; it=it->next) {
			if(it->itime+_siprepo_expire < tnow) {
				if(it->prev==NULL) {
					_siprepo_table[i].plist = it->next;
					if(_siprepo_table[i].plist) {
						_siprepo_table[i].plist->prev = NULL;
					}
				} else {
					it->prev->next = it->next;
				}
				if(it->next!=NULL) {
					it->next->prev = it->prev;
				}
				if(elist) {
					it->next = elist;
					elist = it;
				} else {
					it->next = NULL;
					elist = it;
				}
			}
		}
		lock_release(&_siprepo_table[i].lock);
	}
	while(elist) {
		it = elist;
		elist = elist->next;
		shm_free(it);
	}
}
