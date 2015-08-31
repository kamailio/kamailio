/*
 * Copyright (C) 2014 Federico Cabiddu (federico.cabiddu@gmail.com)
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


/*!
 * \file
 * \brief Functions and definitions related to per user transaction indexing and searching
 * \ingroup tsilo
 * Module: \ref tsilo
 */

#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../hashes.h"
#include "../../lib/kmi/mi.h"
#include "ts_hash.h"
#include "ts_handlers.h"

/*! global transaction table */
struct ts_table *t_table = 0;

/*!
 * \brief Destroy a urecord and free memory
 * \param tma destroyed urecord
 */
void free_ts_urecord(struct ts_urecord *urecord)
{
	LM_DBG("destroying urecord %p\n", urecord);
	ts_transaction_t* ptr;

	while(urecord->transactions) {
		ptr = urecord->transactions;
		urecord->transactions = urecord->transactions->next;
		free_ts_transaction(ptr);
	}

	if (urecord->ruri.s) shm_free(urecord->ruri.s);

	shm_free(urecord);

	urecord = 0;
}

/*!
 * \brief Initialize the per user transactions table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_ts_table(unsigned int size)
{
	unsigned int n;
	unsigned int i;

	t_table = (struct ts_table*)shm_malloc( sizeof(struct ts_table));
	if (t_table==0) {
		LM_ERR("no more shm mem (1)\n");
		return -1;
	}

	memset( t_table, 0, sizeof(struct ts_table) );

	t_table->size = size;

	n = (size<MAX_TS_LOCKS)?size:MAX_TS_LOCKS;
	for(  ; n>=MIN_TS_LOCKS ; n-- ) {
		t_table->locks = lock_set_alloc(n);
		if (t_table->locks==0)
			continue;
		if (lock_set_init(t_table->locks)==0) {
			lock_set_dealloc(t_table->locks);
			t_table->locks = 0;
			continue;
		}
		t_table->locks_no = n;
		break;
	}

	if (t_table->locks==0) {
		LM_ERR("unable to allocted at least %d locks for the hash table\n",
			MIN_TS_LOCKS);
		goto error;
	}

	t_table->entries = (ts_entry_t*)shm_malloc(sizeof(ts_entry_t) * size);
	if (!t_table->entries) {
		LM_ERR("no more shm mem (2)\n");
		goto error;
	}

	for( i=0 ; i<size; i++ ) {
		memset( &(t_table->entries[i]), 0, sizeof(struct ts_entry) );
		t_table->entries[i].next_id = rand() % (3*size);
		t_table->entries[i].lock_idx = i % t_table->locks_no;
	}

	return 0;
error:
	shm_free( t_table );
	t_table = NULL;
	return -1;
}

/*!
 * \brief Destroy the per user transaction table
 */
void destroy_ts_table(void)
{
	struct ts_urecord *ts_u, *l_ts_u;
	unsigned int i;

	if (t_table==0)
		return;

	if (t_table->locks) {
		lock_set_destroy(t_table->locks);
		lock_set_dealloc(t_table->locks);
	}

	for( i=0 ; i<t_table->size; i++ ) {
		ts_u = t_table->entries[i].first;
		while (ts_u) {
			l_ts_u = ts_u;
			ts_u = ts_u->next;
			free_ts_urecord(l_ts_u);
		}
	}

	shm_free(t_table);
	t_table = 0;

	return;
}

void lock_entry(ts_entry_t *entry) {
	ts_lock(t_table, entry);
}

void unlock_entry(ts_entry_t *entry) {
	ts_unlock(t_table, entry);
}

void lock_entry_by_ruri(str* ruri)
{
	unsigned int sl;

	sl = core_hash(ruri, 0, 0) & (t_table->size-1);
	ts_lock(t_table, &t_table->entries[sl]);
}

void unlock_entry_by_ruri(str* ruri)
{
	unsigned int sl;

	sl = core_hash(ruri, 0, 0) & (t_table->size-1);
	ts_unlock(t_table, &t_table->entries[sl]);
}

/*
 * Obtain a urecord pointer if the urecord exists in the table
 */
int get_ts_urecord(str* ruri, struct ts_urecord** _r)
{
	int sl, i, rurihash;
	ts_urecord_t* r;

	rurihash = core_hash(ruri, 0, 0);
	sl = rurihash&(t_table->size-1);
	r = t_table->entries[sl].first;

	for(i = 0; r!=NULL && i < t_table->entries[sl].n; i++) {
		if((r->rurihash==rurihash) && (r->ruri.len==ruri->len)
				&& !memcmp(r->ruri.s,ruri->s,ruri->len)){
			*_r = r;
			return 0;
		}
		r = r->next;
	}

	return 1;   /* Nothing found */
}

/*!
 * \brief Create and initialize new record structure
 * \param ruri request uri
 * \param _r pointer to the new record
 * \return 0 on success, negative on failure
 */
int new_ts_urecord(str* ruri, ts_urecord_t** _r)
{
	*_r = (ts_urecord_t*)shm_malloc(sizeof(ts_urecord_t));
	if (*_r == 0) {
		LM_ERR("no more share memory\n");
		return -1;
	}
	memset(*_r, 0, sizeof(ts_urecord_t));

	(*_r)->ruri.s = (char*)shm_malloc(ruri->len);
	if ((*_r)->ruri.s == 0) {
		LM_ERR("no more share memory\n");
		shm_free(*_r);
		*_r = 0;
		return -2;
	}
	memcpy((*_r)->ruri.s, ruri->s, ruri->len);
	(*_r)->ruri.len = ruri->len;
	(*_r)->rurihash = core_hash(ruri, 0, 0);
	return 0;
}

/*!
 * \brief Insert a new record into transactions table
 * \param ruri request uri
 * \return 0 on success, -1 on failure
 */
int insert_ts_urecord(str* ruri, ts_urecord_t** _r)
{
	ts_entry_t* entry;

	int sl;

	if (new_ts_urecord(ruri, _r) < 0) {
		LM_ERR("creating urecord failed\n");
		return -1;
	}

	sl = ((*_r)->rurihash)&(t_table->size-1);
	entry = &t_table->entries[sl];

	if (entry->n == 0) {
		entry->first = entry->last = *_r;
	} else {
		(*_r)->prev = entry->last;
		entry->last->next = *_r;
		entry->last = *_r;
	}
	entry->n++;
	(*_r)->entry = entry;
	LM_DBG("urecord entry %p",entry);
	return 0;
}

/*!
 * \brief remove a urecord from table and free the memory
 * \param urecord t
 * \return 0 on success, -1 on failure
 */
void remove_ts_urecord(ts_urecord_t* _r)
{
	ts_entry_t* entry;

	entry = _r->entry;

	if (_r->prev)
		_r->prev->next = _r->next;
	if (_r->next)
		_r->next->prev = _r->prev;

	/* it was the last urecord */
	if (entry->n == 1) {
                entry->first = entry->last = NULL;
	}

	entry->n--;
	free_ts_urecord(_r);

        return;
}

/*!
 * \brief Insert a new transaction structure into urecord
 * \param _r urecord
 * \param tindex transaction index in tm table
 * \param tlabel transaction label in tm table
 * \return 0 on success, -1 otherwise
 */
int insert_ts_transaction(struct cell* t, struct sip_msg* msg, struct ts_urecord* _r)
{
	ts_transaction_t *ptr, *prev;
    ts_transaction_t* ts;

	unsigned int tindex;
	unsigned int tlabel;

	tindex = t->hash_index;
	tlabel = t->label;

	ptr = prev = 0;
	ptr = _r->transactions;

	while(ptr) {
		if ((ptr->tindex == tindex) && (ptr->tlabel == tlabel)) {
			LM_DBG("transaction already inserted\n");
			return -1;
		}
		prev = ptr;
		ptr = ptr->next;
	}

	if ( (ts=new_ts_transaction(tindex, tlabel) ) == 0) {
		LM_ERR("failed to create new contact\n");
		return -1;
	}

	ts->urecord = _r;
	/* add the new transaction at the end of the list */

	if (prev) {
		prev->next = ts;
		ts->prev = prev;
	} else {
		_r->transactions = ts;
	}

	if (ts_set_tm_callbacks(t, msg, ts) < 0) {
		LM_ERR("failed to set transaction %d:%d callbacks\n", tindex, tlabel);
	}
	return 0;
}
/*!
 * \brief Create a new transaction structure
 * \param tindex transaction index in tm table
 * \param tlabel transaction label in tm table
 * \return created transaction structure on success, NULL otherwise
 */
ts_transaction_t* new_ts_transaction(int tindex, int tlabel)
{
	ts_transaction_t *ts;
	int len;

	len = sizeof(ts_transaction_t);
	ts = (ts_transaction_t*)shm_malloc(len);
	if (ts==0) {
		LM_ERR("no more shm mem (%d)\n",len);
		return 0;
	}

	memset(ts, 0, len);
	ts->tindex = tindex;
	ts->tlabel = tlabel;
	return ts;
}

/*!
 * \brief Clone a transaction structure
 * \param ts transaction to be cloned
 * \return cloned transaction structure on success, NULL otherwise
 */
ts_transaction_t* clone_ts_transaction(ts_transaction_t* ts)
{
	ts_transaction_t *ts_clone;
	int len;

	if (ts == NULL)
		return NULL;

	len = sizeof(ts_transaction_t);
	ts_clone = (ts_transaction_t*)shm_malloc(len);
	if (ts_clone==NULL) {
		LM_ERR("no more shm mem (%d)\n",len);
		return NULL;
	}

	memcpy(ts_clone, ts, len);
	return ts_clone;
}

/*!
 * \brief remove a transaction from the urecord transactions list
 * \param tma unlinked transaction
 */
void remove_ts_transaction(ts_transaction_t* ts_t)
{
	if (ts_t->next)
		ts_t->next->prev = ts_t->prev;
	if (ts_t->prev)
		ts_t->prev->next = ts_t->next;

	if (ts_t->urecord->transactions == ts_t)
		ts_t->urecord->transactions = ts_t->next;

	free_ts_transaction((void*)ts_t);

	return;
}


/*!
 * \brief Destroy a transaction and free memory
 * \param tma destroyed transaction
 */
inline void free_ts_transaction(void *ts_t)
{
	shm_free((struct ts_transaction*)ts_t);
	ts_t = 0;
}
