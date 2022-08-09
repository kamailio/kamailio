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
 *
 */

/*!
 * \file
 * \brief TM ::
 * \ingroup tm
 */

#include <stdlib.h>


#include "../../core/mem/shm_mem.h"
#include "../../core/hash_func.h"
#include "../../core/dprint.h"
#include "../../core/crypto/md5utils.h"
#include "../../core/ut.h"
#include "../../core/globals.h"
#include "../../core/error.h"
#include "../../core/char_msg_val.h"
#include "../../core/rand/kam_rand.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_stats.h"
#include "t_funcs.h"
#include "h_table.h"
#include "../../core/fix_lumps.h" /* free_via_clen_lump */
#include "timer.h"
#include "uac.h" /* free_local_ack */


#define T_UAC_PTR(T) ((tm_ua_client_t*)((char*)T + sizeof(tm_cell_t) \
								+ MD5_LEN - sizeof(((tm_cell_t*)0)->md5)))


static enum kill_reason kr;

/* pointer to the big table where all the transaction data lives */
struct s_table *_tm_table;

struct s_table *tm_get_table(void)
{
	return _tm_table;
}

void reset_kr(void)
{
	kr = 0;
}

void set_kr(enum kill_reason _kr)
{
	kr |= _kr;
}


enum kill_reason get_kr()
{
	return kr;
}


void lock_hash(int i)
{

	int mypid;

	mypid = my_pid();
	if(likely(atomic_get(&_tm_table->entries[i].locker_pid) != mypid)) {
		lock(&_tm_table->entries[i].mutex);
		atomic_set(&_tm_table->entries[i].locker_pid, mypid);
	} else {
		/* locked within the same process that called us*/
		_tm_table->entries[i].rec_lock_level++;
	}
}


void unlock_hash(int i)
{
	if(likely(_tm_table->entries[i].rec_lock_level == 0)) {
		atomic_set(&_tm_table->entries[i].locker_pid, 0);
		unlock(&_tm_table->entries[i].mutex);
	} else {
		/* recursive locked => decrease rec. lock count */
		_tm_table->entries[i].rec_lock_level--;
	}
}


#ifdef TM_HASH_STATS
unsigned int transaction_count(void)
{
	unsigned int i;
	unsigned int count;

	count = 0;
	for(i = 0; i < TABLE_ENTRIES; i++)
		count += _tm_table->entries[i].cur_entries;
	return count;
}
#endif


void free_cell_helper(
		tm_cell_t *dead_cell, int silent, const char *fname, unsigned int fline)
{
	char *b;
	int i;
	struct sip_msg *rpl;
	struct totag_elem *tt, *foo;
	struct tm_callback *cbs, *cbs_tmp;

	LM_DBG("freeing transaction %p from %s:%u\n", dead_cell, fname, fline);

	if(dead_cell==NULL) {
		return;
	}

	if(dead_cell->prev_c != NULL && dead_cell->next_c != NULL) {
		if(likely(silent == 0)) {
			LM_WARN("removed cell %p is still linked in hash table (%s:%u)\n",
					dead_cell, fname, fline);
			if(t_on_wait(dead_cell)) {
				INIT_REF(dead_cell, 1);
				LM_WARN("cell %p is still linked in wait timer (%s:%u)"
						" - skip freeing now\n",
						dead_cell, fname, fline);
				return;
			}
		}
		unlink_timers(dead_cell);
		remove_from_hash_table_unsafe(dead_cell);
	}
	release_cell_lock(dead_cell); /* does nothing */

	dead_cell->fcount++;
	if(dead_cell->fcount!=1) {
		LM_WARN("unexpected fcount value: %d\n", dead_cell->fcount);
	}
	if(dead_cell->uac==NULL || dead_cell->uac!=T_UAC_PTR(dead_cell)) {
		LM_WARN("unexpected tm cell content: %p\n", dead_cell);
		return;
	}

	if(unlikely(has_tran_tmcbs(dead_cell, TMCB_DESTROY)))
		run_trans_callbacks(TMCB_DESTROY, dead_cell, 0, 0, 0);

	shm_global_lock();
	/* UA Server */
	if(dead_cell->uas.request)
		sip_msg_free_unsafe(dead_cell->uas.request);
	if(dead_cell->uas.response.buffer)
		shm_free_unsafe(dead_cell->uas.response.buffer);
	if(unlikely(dead_cell->uas.cancel_reas))
		shm_free_unsafe(dead_cell->uas.cancel_reas);

	/* callbacks */
	for(cbs = (struct tm_callback *)dead_cell->tmcb_hl.first; cbs;) {
		cbs_tmp = cbs;
		cbs = cbs->next;
		if(cbs_tmp->release) {
			/* It is safer to release the shm memory lock
			 * otherwise the release function must to be aware of
			 * the lock state (Miklos)
			 */
			shm_global_unlock();
			cbs_tmp->release(cbs_tmp->param);
			shm_global_lock();
		}
		shm_free_unsafe(cbs_tmp);
	}

	/* UA Clients */
	for(i = 0; i < dead_cell->nr_of_outgoings; i++) {
		/* retransmission buffer */
		if((b = dead_cell->uac[i].request.buffer))
			shm_free_unsafe(b);
		b = dead_cell->uac[i].local_cancel.buffer;
		if(b != 0 && b != BUSY_BUFFER)
			shm_free_unsafe(b);
		rpl = dead_cell->uac[i].reply;
		if(rpl && rpl != FAKED_REPLY && rpl->msg_flags & FL_SHM_CLONE) {
			sip_msg_free_unsafe(rpl);
		}
#ifdef USE_DNS_FAILOVER
		if(dead_cell->uac[i].dns_h.a) {
			LM_DBG("branch %d -> dns_h.srv (%.*s) ref=%d,"
				" dns_h.a (%.*s) ref=%d\n",
					i, dead_cell->uac[i].dns_h.srv
							? dead_cell->uac[i].dns_h.srv->name_len
							: 0,
					dead_cell->uac[i].dns_h.srv
							? dead_cell->uac[i].dns_h.srv->name
							: "",
					dead_cell->uac[i].dns_h.srv
							? dead_cell->uac[i].dns_h.srv->refcnt.val
							: 0,
					dead_cell->uac[i].dns_h.a->name_len,
					dead_cell->uac[i].dns_h.a->name,
					dead_cell->uac[i].dns_h.a->refcnt.val);
		}
		dns_srv_handle_put_shm_unsafe(&dead_cell->uac[i].dns_h);
#endif
		if(unlikely(dead_cell->uac[i].path.s)) {
			shm_free_unsafe(dead_cell->uac[i].path.s);
		}
		if(unlikely(dead_cell->uac[i].instance.s)) {
			shm_free_unsafe(dead_cell->uac[i].instance.s);
		}
		if(unlikely(dead_cell->uac[i].ruid.s)) {
			shm_free_unsafe(dead_cell->uac[i].ruid.s);
		}
		if(unlikely(dead_cell->uac[i].location_ua.s)) {
			shm_free_unsafe(dead_cell->uac[i].location_ua.s);
		}
	}

	if(dead_cell->uac[0].local_ack)
		free_local_ack_unsafe(dead_cell->uac[0].local_ack);

	/* collected to tags */
	tt = dead_cell->fwded_totags;
	while(tt) {
		foo = tt->next;
		shm_free_unsafe(tt->tag.s);
		shm_free_unsafe(tt);
		tt = foo;
	}

	/* free the avp list */
	if(dead_cell->user_avps_from)
		destroy_avp_list_unsafe(&dead_cell->user_avps_from);
	if(dead_cell->user_avps_to)
		destroy_avp_list_unsafe(&dead_cell->user_avps_to);
	if(dead_cell->uri_avps_from)
		destroy_avp_list_unsafe(&dead_cell->uri_avps_from);
	if(dead_cell->uri_avps_to)
		destroy_avp_list_unsafe(&dead_cell->uri_avps_to);
	if(dead_cell->xavps_list)
		xavp_destroy_list_unsafe(&dead_cell->xavps_list);
	if(dead_cell->xavus_list)
		xavu_destroy_list_unsafe(&dead_cell->xavus_list);
	if(dead_cell->xavis_list)
		xavi_destroy_list_unsafe(&dead_cell->xavis_list);

	memset(dead_cell, 0, sizeof(tm_cell_t));
	/* the cell's body */
	shm_free_unsafe(dead_cell);

	shm_global_unlock();
	t_stats_freed();
}


static inline void init_synonym_id(struct sip_msg *p_msg, char *hash)
{
	int size;
	char *c;
	unsigned int myrand;

	if(p_msg) {
		/* char value of a proxied transaction is
		 * calculated out of header-fields forming
		 * transaction key
		*/
		char_msg_val(p_msg, hash);
	} else {
		/* char value for a UAC transaction is created
		 * randomly -- UAC is an originating stateful element
		 * which cannot be refreshed, so the value can be
		 * anything
		*/
		/* HACK : not long enough */
		myrand = kam_rand();
		c = hash;
		size = MD5_LEN;
		memset(c, '0', size);
		int2reverse_hex(&c, &size, myrand);
	}
}

static void inline init_branches(struct cell *t)
{
	unsigned int i;
	struct ua_client *uac;

	for(i = 0; i < sr_dst_max_branches; i++) {
		uac = &t->uac[i];
		uac->request.my_T = t;
		uac->request.branch = i;
		init_rb_timers(&uac->request);
		uac->local_cancel = uac->request;
#ifdef USE_DNS_FAILOVER
		dns_srv_handle_init(&uac->dns_h);
#endif
	}
}


struct cell *build_cell(struct sip_msg *p_msg)
{
	struct cell *new_cell;
	int sip_msg_len;
	avp_list_t *old;
	struct tm_callback *cbs, *cbs_tmp;
	sr_xavp_t **xold;
	unsigned int cell_size;

	/* allocs a new cell, add space for:
	 * md5 (MD5_LEN - sizeof(struct cell.md5))
	 * uac (sr_dst_max_banches * sizeof(struct ua_client) ) */
	cell_size = sizeof(struct cell) + MD5_LEN - sizeof(((struct cell *)0)->md5)
				+ (sr_dst_max_branches * sizeof(struct ua_client));

	new_cell = (struct cell *)shm_malloc(cell_size);
	if(!new_cell) {
		SHM_MEM_ERROR;
		ser_error = E_OUT_OF_MEM;
		return NULL;
	}

	/* filling with 0 */
	memset(new_cell, 0, cell_size);

	/* UAS */
	new_cell->uas.response.my_T = new_cell;
	init_rb_timers(&new_cell->uas.response);
	/* UAC */
	new_cell->uac = T_UAC_PTR(new_cell);
	/* timers */
	init_cell_timers(new_cell);

	old = set_avp_list(
			AVP_TRACK_FROM | AVP_CLASS_URI, &new_cell->uri_avps_from);
	new_cell->uri_avps_from = *old;
	*old = 0;

	old = set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &new_cell->uri_avps_to);
	new_cell->uri_avps_to = *old;
	*old = 0;

	old = set_avp_list(
			AVP_TRACK_FROM | AVP_CLASS_USER, &new_cell->user_avps_from);
	new_cell->user_avps_from = *old;
	*old = 0;

	old = set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &new_cell->user_avps_to);
	new_cell->user_avps_to = *old;
	*old = 0;

	xold = xavp_set_list(&new_cell->xavps_list);
	new_cell->xavps_list = *xold;
	*xold = 0;

	xold = xavu_set_list(&new_cell->xavus_list);
	new_cell->xavus_list = *xold;
	*xold = 0;

	xold = xavi_set_list(&new_cell->xavis_list);
	new_cell->xavis_list = *xold;
	*xold = 0;

	/* We can just store pointer to domain avps in the transaction context,
	 * because they are read-only */
	new_cell->domain_avps_from =
			get_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN);
	new_cell->domain_avps_to = get_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN);

	/* enter callback, which may potentially want to parse some stuff,
	 * before the request is shmem-ized */
	if(p_msg) {
		set_early_tmcb_list(p_msg, new_cell);
		if(has_reqin_tmcbs())
			run_reqin_callbacks(new_cell, p_msg, p_msg->REQ_METHOD);
	}

	if(p_msg) {
		new_cell->uas.request = sip_msg_cloner(p_msg, &sip_msg_len);
		if(!new_cell->uas.request)
			goto error;
		new_cell->uas.end_request =
				((char *)new_cell->uas.request) + sip_msg_len;
	}

	/* UAC */
	init_branches(new_cell);

	new_cell->relayed_reply_branch = -1;
	/* new_cell->T_canceled = T_UNDEFINED; */

	init_synonym_id(p_msg, new_cell->md5);
	init_cell_lock(new_cell);
	t_stats_created();
	LM_DBG("created new cell %p\n", new_cell);
	return new_cell;

error:
	/* Other modules may have already registered some
	 * transaction callbacks and may also allocated
	 * additional memory for their parameters,
	 * hence TMCB_DESTROY needs to be called. (Miklos)
	 */
	if(unlikely(has_tran_tmcbs(new_cell, TMCB_DESTROY)))
		run_trans_callbacks(TMCB_DESTROY, new_cell, 0, 0, 0);

	/* free the callback list */
	for(cbs = (struct tm_callback *)new_cell->tmcb_hl.first; cbs;) {
		cbs_tmp = cbs;
		cbs = cbs->next;
		if(cbs_tmp->release) {
			cbs_tmp->release(cbs_tmp->param);
		}
		shm_free(cbs_tmp);
	}

	destroy_avp_list(&new_cell->user_avps_from);
	destroy_avp_list(&new_cell->user_avps_to);
	destroy_avp_list(&new_cell->uri_avps_from);
	destroy_avp_list(&new_cell->uri_avps_to);
	xavp_destroy_list(&new_cell->xavps_list);
	xavu_destroy_list(&new_cell->xavus_list);
	xavi_destroy_list(&new_cell->xavis_list);
	shm_free(new_cell);
	/* unlink transaction AVP list and link back the global AVP list (bogdan)*/
	reset_avps();
	xavp_reset_list();
	xavu_reset_list();
	xavi_reset_list();
	LM_DBG("could not create cell\n");
	return NULL;
}


/* Release all the data contained by the hash table. All the aux. structures
 *  as sems, lists, etc, are also released */
void free_hash_table()
{
	struct cell *p_cell;
	struct cell *tmp_cell;
	int i;

	if(_tm_table) {
		/* remove the data contained by each entry */
		for(i = 0; i < TABLE_ENTRIES; i++) {
			release_entry_lock((_tm_table->entries) + i);
			/* delete all synonyms at hash-collision-slot i */
			clist_foreach_safe(&_tm_table->entries[i], p_cell, tmp_cell, next_c)
			{
				free_cell_silent(p_cell);
			}
		}
		shm_free(_tm_table);
		_tm_table = 0;
	}
}


/*
 */
struct s_table *init_hash_table()
{
	int i;

	/*allocs the table*/
	_tm_table = (struct s_table *)shm_malloc(sizeof(struct s_table));
	if(!_tm_table) {
		SHM_MEM_ERROR;
		goto error0;
	}

	memset(_tm_table, 0, sizeof(struct s_table));

	/* try first allocating all the structures needed for syncing */
	if(lock_initialize() == -1)
		goto error1;

	/* inits the entriess */
	for(i = 0; i < TABLE_ENTRIES; i++) {
		init_entry_lock(_tm_table, (_tm_table->entries) + i);
		_tm_table->entries[i].next_label = kam_rand();
		/* init cell list */
		clist_init(&_tm_table->entries[i], next_c, prev_c);
	}

	return _tm_table;

error1:
	free_hash_table();
error0:
	return 0;
}


/**
 * backup xdata from/to msg context to local var and use T lists
 * - mode = 0 - from msg context to _txdata and use T lists
 * - mode = 1 - restore to msg context from _txdata
 */
void tm_xdata_swap(tm_cell_t *t, tm_xlinks_t *xd, int mode)
{
	static tm_xlinks_t _txdata;
	tm_xlinks_t *x;

	if(xd == NULL)
		x = &_txdata;
	else
		x = xd;

	if(mode == 0) {
		LM_DBG("copy X/AVPs from msg context to txdata\n");
		if(t == NULL)
			return;
		x->uri_avps_from =
				set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
		x->uri_avps_to =
				set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
		x->user_avps_from = set_avp_list(
				AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
		x->user_avps_to =
				set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
		x->domain_avps_from = set_avp_list(
				AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
		x->domain_avps_to = set_avp_list(
				AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);
		x->xavps_list = xavp_set_list(&t->xavps_list);
		x->xavus_list = xavu_set_list(&t->xavus_list);
		x->xavis_list = xavi_set_list(&t->xavis_list);
	} else if(mode == 1) {
		LM_DBG("restore X/AVPs msg context from txdata\n");
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, x->uri_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, x->uri_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, x->user_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, x->user_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, x->domain_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, x->domain_avps_to);
		xavp_set_list(x->xavps_list);
		xavu_set_list(x->xavus_list);
		xavi_set_list(x->xavis_list);
	}
}

/**
 * replace existing lists with newxd and backup in bakxd or restore from bakxd
 */
void tm_xdata_replace(tm_xdata_t *newxd, tm_xlinks_t *bakxd)
{
	if(newxd == NULL && bakxd != NULL) {
		LM_DBG("restore X/AVP msg context from backup data\n");
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, bakxd->uri_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, bakxd->uri_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, bakxd->user_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, bakxd->user_avps_to);
		set_avp_list(
				AVP_TRACK_FROM | AVP_CLASS_DOMAIN, bakxd->domain_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, bakxd->domain_avps_to);
		xavp_set_list(bakxd->xavps_list);
		xavu_set_list(bakxd->xavus_list);
		xavi_set_list(bakxd->xavis_list);
		return;
	}

	if(newxd != NULL && bakxd != NULL) {
		LM_DBG("replace existing list in backup xd from new xd\n");
		bakxd->uri_avps_from = set_avp_list(
				AVP_TRACK_FROM | AVP_CLASS_URI, &newxd->uri_avps_from);
		bakxd->uri_avps_to =
				set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &newxd->uri_avps_to);
		bakxd->user_avps_from = set_avp_list(
				AVP_TRACK_FROM | AVP_CLASS_USER, &newxd->user_avps_from);
		bakxd->user_avps_to = set_avp_list(
				AVP_TRACK_TO | AVP_CLASS_USER, &newxd->user_avps_to);
		bakxd->domain_avps_from = set_avp_list(
				AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &newxd->domain_avps_from);
		bakxd->domain_avps_to = set_avp_list(
				AVP_TRACK_TO | AVP_CLASS_DOMAIN, &newxd->domain_avps_to);
		bakxd->xavps_list = xavp_set_list(&newxd->xavps_list);
		bakxd->xavus_list = xavu_set_list(&newxd->xavus_list);
		bakxd->xavis_list = xavi_set_list(&newxd->xavis_list);
		return;
	}
}

void tm_log_transaction(tm_cell_t *tcell, int llev, char *ltext)
{
	LOG(llev, "%s [start] transaction %p\n", ltext, tcell);
	LOG(llev, "%s - tindex=%u tlabel=%u method='%.*s' from='%.*s'"
			" to='%.*s' callid='%.*s' cseq='%.*s' uas_request=%s"
			" tflags=%u outgoings=%u ref_count=%u lifetime=%u\n",
			ltext, (unsigned)tcell->hash_index, (unsigned)tcell->label,
			tcell->method.len, tcell->method.s,
			tcell->from.len, tcell->from.s,
			tcell->to.len, tcell->to.s,
			tcell->callid.len, tcell->callid.s,
			tcell->cseq_n.len, tcell->cseq_n.s,
			(tcell->uas.request)?"yes":"no",
			(unsigned)tcell->flags,
			(unsigned)tcell->nr_of_outgoings,
			(unsigned)atomic_get(&tcell->ref_count),
			(unsigned)TICKS_TO_S(tcell->end_of_life)
		);

	LOG(llev, "%s [end] transaction %p\n", ltext, tcell);
}

/* clean active but very old transactions */
void tm_clean_lifetime(void)
{
	int r;
	tm_cell_t *tcell;
	tm_cell_t *bcell;
	ticks_t texp;

	texp = get_ticks_raw() - S_TO_TICKS(TM_LIFETIME_LIMIT);

	for (r=0; r<TABLE_ENTRIES; r++) {
		/* faster first try without lock */
		if(clist_empty(&_tm_table->entries[r], next_c)) {
			continue;
		}
		lock_hash(r);
		/* one more time with lock to be avoid any cpu races */
		if(clist_empty(&_tm_table->entries[r], next_c)) {
			unlock_hash(r);
			continue;
		}

		clist_foreach_safe(&_tm_table->entries[r], tcell, bcell, next_c)
		{
			if(TICKS_GT(texp, tcell->end_of_life)) {
				tm_log_transaction(tcell, L_WARN, "[hard cleanup]");
				free_cell(tcell);
			}
		}
		unlock_hash(r);
	}
}
