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

#include "dlgs_records.h"

#define dlgs_compute_hash(_s) core_case_hash(_s, 0, 0)
#define dlgs_get_index(_h, _size) (_h) & ((_size)-1)

extern sruid_t _dlgs_sruid;

typedef struct _dlgs_sipfields {
	str callid;
	str ftag;
	str ttag;
} dlgs_sipfields_t;

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


dlgs_ht_t *dlgs_ht_init(unsigned int htsize, int expire, int initexpire)
{
	int i;
	dlgs_ht_t *dsht = NULL;

	dsht = (dlgs_ht_t *)shm_malloc(sizeof(dlgs_ht_t));
	if(dsht == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(dsht, 0, sizeof(dlgs_ht_t));
	dsht->htsize = htsize;
	dsht->htexpire = expire;
	dsht->htinitexpire = initexpire;

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

int dlgs_ht_destroy(dlgs_ht_t *dsht)
{
	int i;
	dlgs_item_t *it, *it0;

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


int dlgs_add_item(dlgs_ht_t *dsht, sip_msg_t *msg, str *src, str *dst, str *data)
{
	unsigned int idx;
	unsigned int hid;
	dlgs_item_t *it, *prev, *nitem;
	dlgs_sipfields_t sf;

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
			LM_WARN("call-id already in hash table [%.*s].\n", sf.callid.len,
					sf.callid.s);
			return -2;
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

int dlgs_unlock_item(dlgs_ht_t *dsht, sip_msg_t *msg)
{
	unsigned int idx;
	unsigned int hid;
	str *cid;
	dlgs_sipfields_t sf;

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

dlgs_item_t *dlgs_get_item(dlgs_ht_t *dsht, sip_msg_t *msg)
{
	unsigned int idx;
	unsigned int hid;
	dlgs_item_t *it;
	str *cid;
	dlgs_sipfields_t sf;

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


int dlgs_del_item(dlgs_ht_t *dsht, sip_msg_t *msg)
{
	unsigned int idx;
	unsigned int hid;
	dlgs_item_t *it;
	str *cid;
	dlgs_sipfields_t sf;

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
int dlgs_ht_dbg(dlgs_ht_t *dsht)
{
	int i;
	dlgs_item_t *it;

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