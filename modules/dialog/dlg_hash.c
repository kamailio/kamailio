/*
 * Copyright (C) 2006 Voice System SRL
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
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
 * \brief Functions related to dialog creation and searching
 * \ingroup dialog
 * Module: \ref dialog
 */

#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../hashes.h"
#include "../../lib/kmi/mi.h"
#include "dlg_timer.h"
#include "dlg_var.h"
#include "dlg_hash.h"
#include "dlg_profile.h"
#include "dlg_req_within.h"
#include "dlg_db_handler.h"

extern int dlg_ka_interval;

/*! global dialog table */
struct dlg_table *d_table = 0;

dlg_ka_t **dlg_ka_list_head = NULL;
dlg_ka_t **dlg_ka_list_tail = NULL;
gen_lock_t *dlg_ka_list_lock = NULL;

/*!
 * \brief Reference a dialog without locking
 * \param _dlg dialog
 * \param _cnt increment for the reference counter
 */
#define ref_dlg_unsafe(_dlg,_cnt)     \
	do { \
		(_dlg)->ref += (_cnt); \
		LM_DBG("ref dlg %p with %d -> %d\n", \
			(_dlg),(_cnt),(_dlg)->ref); \
	}while(0)


/*!
 * \brief Unreference a dialog without locking
 * \param _dlg dialog
 * \param _cnt decrement for the reference counter
 * \param _d_entry dialog entry
 */
#define unref_dlg_unsafe(_dlg,_cnt,_d_entry)   \
	do { \
		if((_dlg)->ref <= 0 ) { \
			LM_WARN("invalid unref'ing dlg %p with ref %d by %d\n",\
					(_dlg),(_dlg)->ref,(_cnt));\
			break; \
		} \
		(_dlg)->ref -= (_cnt); \
		LM_DBG("unref dlg %p with %d -> %d\n",\
			(_dlg),(_cnt),(_dlg)->ref);\
		if ((_dlg)->ref<0) {\
			LM_CRIT("bogus ref %d with cnt %d for dlg %p [%u:%u] "\
				"with clid '%.*s' and tags '%.*s' '%.*s'\n",\
				(_dlg)->ref, _cnt, _dlg,\
				(_dlg)->h_entry, (_dlg)->h_id,\
				(_dlg)->callid.len, (_dlg)->callid.s,\
				(_dlg)->tag[DLG_CALLER_LEG].len,\
				(_dlg)->tag[DLG_CALLER_LEG].s,\
				(_dlg)->tag[DLG_CALLEE_LEG].len,\
				(_dlg)->tag[DLG_CALLEE_LEG].s); \
		}\
		if ((_dlg)->ref<=0) { \
			unlink_unsafe_dlg( _d_entry, _dlg);\
			LM_DBG("ref <=0 for dialog %p\n",_dlg);\
			destroy_dlg(_dlg);\
		}\
	}while(0)

/**
 * add item to keep-alive list
 *
 */
int dlg_ka_add(dlg_cell_t *dlg)
{
	dlg_ka_t *dka;

	if(dlg_ka_interval<=0)
		return 0;
	if(!(dlg->iflags & (DLG_IFLAG_KA_SRC | DLG_IFLAG_KA_DST)))
		return 0;

	dka = (dlg_ka_t*)shm_malloc(sizeof(dlg_ka_t));
	if(dka==NULL) {
		LM_ERR("no more shm mem\n");
		return -1;
	}
	memset(dka, 0, sizeof(dlg_ka_t));
	dka->katime = get_ticks() + dlg_ka_interval;
	dka->iuid.h_entry = dlg->h_entry;
	dka->iuid.h_id = dlg->h_id;
	dka->iflags = dlg->iflags;

	lock_get(dlg_ka_list_lock);
	if(*dlg_ka_list_tail!=NULL)
		(*dlg_ka_list_tail)->next = dka;
	if(*dlg_ka_list_head==NULL)
		*dlg_ka_list_head = dka;
	*dlg_ka_list_tail = dka;
	lock_release(dlg_ka_list_lock);
	LM_DBG("added dlg[%d,%d] to KA list\n", dlg->h_entry, dlg->h_id);
	return 0;
}

/**
 * run keep-alive list
 *
 */
int dlg_ka_run(ticks_t ti)
{
	dlg_ka_t *dka;
	dlg_cell_t *dlg;

	if(dlg_ka_interval<=0)
		return 0;

	while(1) {
		/* get head item */
		lock_get(dlg_ka_list_lock);
		if(*dlg_ka_list_head==NULL) {
			lock_release(dlg_ka_list_lock);
			return 0;
		}
		dka = *dlg_ka_list_head;
#if 0
		LM_DBG("dlg ka timer at %lu for"
				" dlg[%u,%u] on %lu\n", (unsigned long)ti,
				dka->iuid.h_entry, dka->iuid.h_id,
				(unsigned long)dka->katime);
#endif
		if(dka->katime>ti) {
			lock_release(dlg_ka_list_lock);
			return 0;
		}
		if(*dlg_ka_list_head == *dlg_ka_list_tail) {
			*dlg_ka_list_head = NULL;
			*dlg_ka_list_tail = NULL;
		} else {
			*dlg_ka_list_head = dka->next;
		}
		lock_release(dlg_ka_list_lock);

		/* send keep-alive for dka */
		dlg = dlg_get_by_iuid(&dka->iuid);
		if(dlg==NULL) {
			shm_free(dka);
			dka = NULL;
		} else {
			if((dka->iflags & DLG_IFLAG_KA_SRC)
					&& (dlg->state==DLG_STATE_CONFIRMED))
				dlg_send_ka(dlg, DLG_CALLER_LEG);
			if((dka->iflags & DLG_IFLAG_KA_DST)
					&& (dlg->state==DLG_STATE_CONFIRMED))
				dlg_send_ka(dlg, DLG_CALLEE_LEG);
			if(dlg->state==DLG_STATE_DELETED) {
				shm_free(dka);
				dka = NULL;
			}
			dlg_release(dlg);
		}
		/* append to tail */
		if(dka!=NULL)
		{
			dka->katime = ti + dlg_ka_interval;
			lock_get(dlg_ka_list_lock);
			if(*dlg_ka_list_tail!=NULL)
				(*dlg_ka_list_tail)->next = dka;
			if(*dlg_ka_list_head==NULL)
				*dlg_ka_list_head = dka;
			*dlg_ka_list_tail = dka;
			lock_release(dlg_ka_list_lock);
		}
	}

	return 0;
}

/**
 * clean old unconfirmed dialogs
 *
 */
int dlg_clean_run(ticks_t ti)
{
	unsigned int i;
	unsigned int tm;
	dlg_cell_t *dlg;
	dlg_cell_t *tdlg;

	tm = (unsigned int)time(NULL);
	for(i=0; i<d_table->size; i++)
	{
		dlg_lock(d_table, &d_table->entries[i]);
		dlg = d_table->entries[i].first;
		while (dlg) {
			tdlg = dlg;
			dlg = dlg->next;
			if(tdlg->state==DLG_STATE_UNCONFIRMED && tdlg->init_ts<tm-300) {
				/* dialog in early state older than 5min */
				LM_NOTICE("dialog in early state is too old (%p ref %d)\n",
						tdlg, tdlg->ref);
				unlink_unsafe_dlg(&d_table->entries[i], tdlg);
				destroy_dlg(tdlg);
			}
			if(tdlg->state==DLG_STATE_CONFIRMED_NA && tdlg->start_ts<tm-60) {
				if(update_dlg_timer(&tdlg->tl, 10)<0) {
					LM_ERR("failed to update dialog lifetime in long non-ack state\n");
				}
				tdlg->lifetime = 10;
				tdlg->dflags |= DLG_FLAG_CHANGED;
			}
		}
		dlg_unlock(d_table, &d_table->entries[i]);
	}
	return 0;
}

/*!
 * \brief Initialize the global dialog table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_dlg_table(unsigned int size)
{
	unsigned int i;

	dlg_ka_list_head = (dlg_ka_t **)shm_malloc(sizeof(dlg_ka_t *));
	if(dlg_ka_list_head==NULL) {
		LM_ERR("no more shm mem (h)\n");
		goto error0;
	}
	dlg_ka_list_tail = (dlg_ka_t **)shm_malloc(sizeof(dlg_ka_t *));
	if(dlg_ka_list_tail==NULL) {
		LM_ERR("no more shm mem (t)\n");
		goto error0;
	}
	*dlg_ka_list_head = NULL;
	*dlg_ka_list_tail = NULL;
	dlg_ka_list_lock = (gen_lock_t*)shm_malloc(sizeof(gen_lock_t));
	if(dlg_ka_list_lock==NULL) {
		LM_ERR("no more shm mem (l)\n");
		goto error0;
	}
	lock_init(dlg_ka_list_lock);

	d_table = (struct dlg_table*)shm_malloc
		( sizeof(struct dlg_table) + size*sizeof(struct dlg_entry));
	if (d_table==0) {
		LM_ERR("no more shm mem (1)\n");
		goto error0;
	}

	memset( d_table, 0, sizeof(struct dlg_table) );
	d_table->size = size;
	d_table->entries = (struct dlg_entry*)(d_table+1);

	for( i=0 ; i<size; i++ ) {
		memset( &(d_table->entries[i]), 0, sizeof(struct dlg_entry) );
		if(lock_init(&d_table->entries[i].lock)<0) {
			LM_ERR("failed to init lock for slot: %d\n", i);
			goto error1;
		}
		d_table->entries[i].next_id = rand() % (3*size);
	}

	return 0;
error1:
	shm_free( d_table );
	d_table = NULL;
error0:
	if(dlg_ka_list_head!=NULL)
		shm_free(dlg_ka_list_head);
	if(dlg_ka_list_tail!=NULL)
		shm_free(dlg_ka_list_tail);
	dlg_ka_list_head = NULL;
	dlg_ka_list_tail = NULL;
	return -1;
}


/*!
 * \brief Destroy a dialog, run callbacks and free memory
 * \param dlg destroyed dialog
 */
void destroy_dlg(struct dlg_cell *dlg)
{
	int ret = 0;
	struct dlg_var *var;

	LM_DBG("destroying dialog %p (ref %d)\n", dlg, dlg->ref);

	ret = remove_dialog_timer(&dlg->tl);
	if (ret < 0) {
		LM_CRIT("unable to unlink the timer on dlg %p [%u:%u] "
			"with clid '%.*s' and tags '%.*s' '%.*s'\n",
			dlg, dlg->h_entry, dlg->h_id,
			dlg->callid.len, dlg->callid.s,
			dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
			dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
	} else if (ret > 0) {
		LM_DBG("removed timer for dlg %p [%u:%u] "
			"with clid '%.*s' and tags '%.*s' '%.*s'\n",
			dlg, dlg->h_entry, dlg->h_id,
			dlg->callid.len, dlg->callid.s,
			dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
			dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
	}

	run_dlg_callbacks( DLGCB_DESTROY , dlg, NULL, NULL, DLG_DIR_NONE, 0);


	/* delete the dialog from DB*/
	if (dlg_db_mode)
		remove_dialog_from_db(dlg);

	if (dlg->cbs.first)
		destroy_dlg_callbacks_list(dlg->cbs.first);

	if (dlg->profile_links)
		destroy_linkers(dlg->profile_links);

	if (dlg->tag[DLG_CALLER_LEG].s)
		shm_free(dlg->tag[DLG_CALLER_LEG].s);

	if (dlg->tag[DLG_CALLEE_LEG].s)
		shm_free(dlg->tag[DLG_CALLEE_LEG].s);

	if (dlg->cseq[DLG_CALLER_LEG].s)
		shm_free(dlg->cseq[DLG_CALLER_LEG].s);

	if (dlg->cseq[DLG_CALLEE_LEG].s)
		shm_free(dlg->cseq[DLG_CALLEE_LEG].s);

	if (dlg->toroute_name.s)
		shm_free(dlg->toroute_name.s);

	
	while (dlg->vars) {
		var = dlg->vars;
		dlg->vars = dlg->vars->next;
		shm_free(var->key.s);
		shm_free(var->value.s);
		shm_free(var);
	}


	shm_free(dlg);
	dlg = 0;
}


/*!
 * \brief Destroy the global dialog table
 */
void destroy_dlg_table(void)
{
	struct dlg_cell *dlg, *l_dlg;
	unsigned int i;

	if (d_table==0)
		return;

	for( i=0 ; i<d_table->size; i++ ) {
		dlg = d_table->entries[i].first;
		while (dlg) {
			l_dlg = dlg;
			dlg = dlg->next;
			destroy_dlg(l_dlg);
		}
		lock_destroy(&d_table->entries[i].lock);
	}

	shm_free(d_table);
	d_table = 0;

	return;
}


/*!
 * \brief Create a new dialog structure for a SIP dialog
 * \param callid dialog callid
 * \param from_uri dialog from uri
 * \param to_uri dialog to uri
 * \param from_tag dialog from tag
 * \param req_uri dialog r-uri
 * \return created dialog structure on success, NULL otherwise
 */
struct dlg_cell* build_new_dlg( str *callid, str *from_uri, str *to_uri,
		str *from_tag, str *req_uri)
{
	struct dlg_cell *dlg;
	int len;
	char *p;

	len = sizeof(struct dlg_cell) + callid->len + from_uri->len +
		to_uri->len + req_uri->len;
	dlg = (struct dlg_cell*)shm_malloc( len );
	if (dlg==0) {
		LM_ERR("no more shm mem (%d)\n",len);
		return 0;
	}

	memset( dlg, 0, len);
	dlg->state = DLG_STATE_UNCONFIRMED;
	dlg->init_ts = (unsigned int)time(NULL);

	dlg->h_entry = core_hash( callid, 0, d_table->size);
	LM_DBG("new dialog on hash %u\n",dlg->h_entry);

	p = (char*)(dlg+1);

	dlg->callid.s = p;
	dlg->callid.len = callid->len;
	memcpy( p, callid->s, callid->len);
	p += callid->len;

	dlg->from_uri.s = p;
	dlg->from_uri.len = from_uri->len;
	memcpy( p, from_uri->s, from_uri->len);
	p += from_uri->len;

	dlg->to_uri.s = p;
	dlg->to_uri.len = to_uri->len;
	memcpy( p, to_uri->s, to_uri->len);
	p += to_uri->len; 

	dlg->req_uri.s = p;
	dlg->req_uri.len = req_uri->len;
	memcpy( p, req_uri->s, req_uri->len);
	p += req_uri->len;

	if ( p!=(((char*)dlg)+len) ) {
		LM_CRIT("buffer overflow\n");
		shm_free(dlg);
		return 0;
	}

	return dlg;
}


/*!
 * \brief Set the leg information for an existing dialog
 * \param dlg dialog
 * \param tag from tag or to tag
 * \param rr record-routing information
 * \param contact caller or callee contact
 * \param cseq CSEQ of caller or callee
 * \param leg must be either DLG_CALLER_LEG, or DLG_CALLEE_LEG
 * \return 0 on success, -1 on failure
 */
int dlg_set_leg_info(struct dlg_cell *dlg, str* tag, str *rr, str *contact,
					str *cseq, unsigned int leg)
{
	char *p;
	str cs = {"0", 1};

	/* if we don't have cseq, set it to 0 */
	if(cseq->len>0) {
		cs = *cseq;
	}

	if(dlg->tag[leg].s)
		shm_free(dlg->tag[leg].s);
	dlg->tag[leg].s = (char*)shm_malloc( tag->len + rr->len + contact->len );

	if(dlg->cseq[leg].s) {
		if (dlg->cseq[leg].len < cs.len) {
			shm_free(dlg->cseq[leg].s);
			dlg->cseq[leg].s = (char*)shm_malloc(cs.len);
		}
	} else {
		dlg->cseq[leg].s = (char*)shm_malloc( cs.len );
	}

	if ( dlg->tag[leg].s==NULL || dlg->cseq[leg].s==NULL) {
		LM_ERR("no more shm mem\n");
		if (dlg->tag[leg].s)
		{
			shm_free(dlg->tag[leg].s);
			dlg->tag[leg].s = NULL;
		}
		if (dlg->cseq[leg].s)
		{
			shm_free(dlg->cseq[leg].s);
			dlg->cseq[leg].s = NULL;
		}
		return -1;
	}
	p = dlg->tag[leg].s;

	/* tag */
	dlg->tag[leg].len = tag->len;
	memcpy( p, tag->s, tag->len);
	p += tag->len;
	/* contact */
	dlg->contact[leg].s = p;
	dlg->contact[leg].len = contact->len;
	memcpy( p, contact->s, contact->len);
	p += contact->len;
	/* rr */
	if (rr->len) {
		dlg->route_set[leg].s = p;
		dlg->route_set[leg].len = rr->len;
		memcpy( p, rr->s, rr->len);
	}

	/* cseq */
	dlg->cseq[leg].len = cs.len;
	memcpy( dlg->cseq[leg].s, cs.s, cs.len);

	return 0;
}


/*!
 * \brief Update or set the CSEQ for an existing dialog
 * \param dlg dialog
 * \param leg must be either DLG_CALLER_LEG, or DLG_CALLEE_LEG
 * \param cseq CSEQ of caller or callee
 * \return 0 on success, -1 on failure
 */
int dlg_update_cseq(struct dlg_cell * dlg, unsigned int leg, str *cseq)
{	dlg_entry_t *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);

	dlg_lock(d_table, d_entry);

	if ( dlg->cseq[leg].s ) {
		if (dlg->cseq[leg].len < cseq->len) {
			shm_free(dlg->cseq[leg].s);
			dlg->cseq[leg].s = (char*)shm_malloc(cseq->len);
			if (dlg->cseq[leg].s==NULL)
				goto error;
		}
	} else {
		dlg->cseq[leg].s = (char*)shm_malloc(cseq->len);
		if (dlg->cseq[leg].s==NULL)
			goto error;
	}

	memcpy( dlg->cseq[leg].s, cseq->s, cseq->len );
	dlg->cseq[leg].len = cseq->len;

	LM_DBG("cseq of leg[%d] is %.*s\n", leg,
			dlg->cseq[leg].len, dlg->cseq[leg].s);
	dlg_unlock(d_table, d_entry);
	return 0;
error:
	dlg_unlock(d_table, d_entry);
	LM_ERR("not more shm mem\n");
	return -1;
}


/*!
 * \brief Lookup a dialog in the global list
 *
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again iff a dialog has been found.
 * \param h_entry number of the hash table entry
 * \param h_id id of the hash table entry
 * \return dialog structure on success, NULL on failure
 */
dlg_cell_t *dlg_lookup( unsigned int h_entry, unsigned int h_id)
{
	dlg_cell_t *dlg;
	dlg_entry_t *d_entry;

	if(d_table==NULL)
		return 0;

	if (h_entry>=d_table->size)
		goto not_found;

	d_entry = &(d_table->entries[h_entry]);

	dlg_lock( d_table, d_entry);

	for( dlg=d_entry->first ; dlg ; dlg=dlg->next ) {
		if (dlg->h_id == h_id) {
			ref_dlg_unsafe(dlg, 1);
			dlg_unlock( d_table, d_entry);
			LM_DBG("dialog id=%u found on entry %u\n", h_id, h_entry);
			return dlg;
		}
	}

	dlg_unlock( d_table, d_entry);
not_found:
	LM_DBG("no dialog id=%u found on entry %u\n", h_id, h_entry);
	return 0;
}


/*!
 * \brief Search a dialog in the global list by iuid
 *
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again if a dialog has been found.
 * \param diuid internal unique id per dialog
 * \return dialog structure on success, NULL on failure
 */
dlg_cell_t* dlg_get_by_iuid(dlg_iuid_t *diuid)
{
	if(diuid==NULL)
		return NULL;
	if(diuid->h_id==0)
		return NULL;
	/* dlg ref counter is increased by next line */
	return dlg_lookup(diuid->h_entry, diuid->h_id);
}

/*!
 * \brief Helper function to get a dialog corresponding to a SIP message
 * \see get_dlg
 * \param h_entry hash index in the directory list
 * \param callid callid
 * \param ftag from tag
 * \param ttag to tag
 * \param dir direction
 * \param mode let hash table slot locked if dialog is not found
 * \return dialog structure on success, NULL on failure
 */
static inline struct dlg_cell* internal_get_dlg(unsigned int h_entry,
						str *callid, str *ftag, str *ttag,
						unsigned int *dir, int mode)
{
	struct dlg_cell *dlg;
	struct dlg_entry *d_entry;

	d_entry = &(d_table->entries[h_entry]);

	dlg_lock( d_table, d_entry);

	for( dlg = d_entry->first ; dlg ; dlg = dlg->next ) {
		/* Check callid / fromtag / totag */
		if (match_dialog( dlg, callid, ftag, ttag, dir)==1) {
			ref_dlg_unsafe(dlg, 1);
			dlg_unlock( d_table, d_entry);
			LM_DBG("dialog callid='%.*s' found on entry %u, dir=%d\n",
				callid->len, callid->s,h_entry,*dir);
			return dlg;
		}
	}

	if(likely(mode==0)) dlg_unlock( d_table, d_entry);
	LM_DBG("no dialog callid='%.*s' found\n", callid->len, callid->s);
	return 0;
}



/*!
 * \brief Get dialog that correspond to CallId, From Tag and To Tag
 *
 * Get dialog that correspond to CallId, From Tag and To Tag.
 * See RFC 3261, paragraph 4. Overview of Operation:                 
 * "The combination of the To tag, From tag, and Call-ID completely  
 * defines a peer-to-peer SIP relationship between [two UAs] and is 
 * referred to as a dialog."
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again iff a dialog has been found.
 * \param callid callid
 * \param ftag from tag
 * \param ttag to tag
 * \param dir direction
 * \return dialog structure on success, NULL on failure
 */
struct dlg_cell* get_dlg( str *callid, str *ftag, str *ttag, unsigned int *dir)
{
	struct dlg_cell *dlg;
	unsigned int he;

	he = core_hash(callid, 0, d_table->size);
	dlg = internal_get_dlg(he, callid, ftag, ttag, dir, 0);

	if (dlg == 0) {
		LM_DBG("no dialog callid='%.*s' found\n", callid->len, callid->s);
		return 0;
	}
	return dlg;
}


/*!
 * \brief Search dialog that corresponds to CallId, From Tag and To Tag
 *
 * Get dialog that correspond to CallId, From Tag and To Tag.
 * See RFC 3261, paragraph 4. Overview of Operation:
 * "The combination of the To tag, From tag, and Call-ID completely
 * defines a peer-to-peer SIP relationship between [two UAs] and is
 * referred to as a dialog."
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again if a dialog has been found.
 * If the dialog is not found, the hash slot is left locked, to allow
 * linking the structure of a new dialog.
 * \param callid callid
 * \param ftag from tag
 * \param ttag to tag
 * \param dir direction
 * \return dialog structure on success, NULL on failure (and slot locked)
 */
dlg_cell_t* search_dlg( str *callid, str *ftag, str *ttag, unsigned int *dir)
{
	struct dlg_cell *dlg;
	unsigned int he;

	he = core_hash(callid, 0, d_table->size);
	dlg = internal_get_dlg(he, callid, ftag, ttag, dir, 1);

	if (dlg == 0) {
		LM_DBG("dialog with callid='%.*s' not found\n", callid->len, callid->s);
		return 0;
	}
	return dlg;
}


/*!
 * \brief Lock hash table slot by call-id
 * \param callid call-id value
 */
void dlg_hash_lock(str *callid)
{
	unsigned int he;
	struct dlg_entry *d_entry;

	he = core_hash(callid, 0, d_table->size);
	d_entry = &(d_table->entries[he]);
	dlg_lock(d_table, d_entry);
}


/*!
 * \brief Release hash table slot by call-id
 * \param callid call-id value
 */
void dlg_hash_release(str *callid)
{
	unsigned int he;
	struct dlg_entry *d_entry;

	he = core_hash(callid, 0, d_table->size);
	d_entry = &(d_table->entries[he]);
	dlg_unlock(d_table, d_entry);
}


/*!
 * \brief Link a dialog structure
 * \param dlg dialog
 * \param n extra increments for the reference counter
 * \param mode link in safe mode (0 - lock slot; 1 - don't)
 */
void link_dlg(struct dlg_cell *dlg, int n, int mode)
{
	struct dlg_entry *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);

	if(unlikely(mode==0)) dlg_lock( d_table, d_entry);

	/* keep id 0 for special cases */
	dlg->h_id = 1 + d_entry->next_id++;
	if(dlg->h_id == 0) dlg->h_id = 1;
	LM_DBG("linking dialog [%u:%u]\n", dlg->h_entry, dlg->h_id);
	if (d_entry->first==0) {
		d_entry->first = d_entry->last = dlg;
	} else {
		d_entry->last->next = dlg;
		dlg->prev = d_entry->last;
		d_entry->last = dlg;
	}

	ref_dlg_unsafe(dlg, 1+n);

	if(unlikely(mode==0)) dlg_unlock( d_table, d_entry);
	return;
}


/*!
 * \brief Refefence a dialog with locking
 * \see ref_dlg_unsafe
 * \param dlg dialog
 * \param cnt increment for the reference counter
 */
void dlg_ref(dlg_cell_t *dlg, unsigned int cnt)
{
	dlg_entry_t *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);

	dlg_lock( d_table, d_entry);
	ref_dlg_unsafe( dlg, cnt);
	dlg_unlock( d_table, d_entry);
}


/*!
 * \brief Unreference a dialog with locking
 * \see unref_dlg_unsafe
 * \param dlg dialog
 * \param cnt decrement for the reference counter
 */
void dlg_unref(dlg_cell_t *dlg, unsigned int cnt)
{
	dlg_entry_t *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);

	dlg_lock( d_table, d_entry);
	unref_dlg_unsafe( dlg, cnt, d_entry);
	dlg_unlock( d_table, d_entry);
}


/*!
 * \brief Release a dialog from ref counter by 1
 * \see dlg_unref
 * \param dlg dialog
 */
void dlg_release(dlg_cell_t *dlg)
{
	if(dlg==NULL)
		return;
	dlg_unref(dlg, 1);
}


/*!
 * \brief Small logging helper functions for next_state_dlg.
 * \param event logged event
 * \param dlg dialog data
 * \see next_state_dlg
 */
static inline void log_next_state_dlg(const int event, const struct dlg_cell *dlg) {
	LM_CRIT("bogus event %d in state %d for dlg %p [%u:%u] with clid '%.*s' and tags "
		"'%.*s' '%.*s'\n", event, dlg->state, dlg, dlg->h_entry, dlg->h_id,
		dlg->callid.len, dlg->callid.s,
		dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
		dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
}


/*!
 * \brief Update a dialog state according a event and the old state
 *
 * This functions implement the main state machine that update a dialog
 * state according a processed event and the current state. If necessary
 * it will delete the processed dialog. The old and new state are also
 * saved for reference.
 * \param dlg updated dialog
 * \param event current event
 * \param old_state old dialog state
 * \param new_state new dialog state
 * \param unref set to 1 when the dialog was deleted, 0 otherwise
 */
void next_state_dlg(dlg_cell_t *dlg, int event,
		int *old_state, int *new_state, int *unref)
{
	dlg_entry_t *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);

	*unref = 0;

	dlg_lock( d_table, d_entry);

	*old_state = dlg->state;

	switch (event) {
		case DLG_EVENT_TDEL:
			switch (dlg->state) {
				case DLG_STATE_UNCONFIRMED:
				case DLG_STATE_EARLY:
					dlg->state = DLG_STATE_DELETED;
					unref_dlg_unsafe(dlg,1,d_entry);
					*unref = 1;
					break;
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_CONFIRMED:
					unref_dlg_unsafe(dlg,1,d_entry);
					break;
				case DLG_STATE_DELETED:
					*unref = 1;
					break;
				default:
					log_next_state_dlg(event, dlg);
			}
			break;
		case DLG_EVENT_RPL1xx:
			switch (dlg->state) {
				case DLG_STATE_UNCONFIRMED:
				case DLG_STATE_EARLY:
					dlg->state = DLG_STATE_EARLY;
					break;
				default:
					log_next_state_dlg(event, dlg);
			}
			break;
		case DLG_EVENT_RPL3xx:
			switch (dlg->state) {
				case DLG_STATE_UNCONFIRMED:
				case DLG_STATE_EARLY:
					dlg->state = DLG_STATE_DELETED;
					*unref = 1;
					break;
				default:
					log_next_state_dlg(event, dlg);
			}
			break;
		case DLG_EVENT_RPL2xx:
			switch (dlg->state) {
				case DLG_STATE_DELETED:
					if (dlg->dflags&DLG_FLAG_HASBYE) {
						LM_CRIT("bogus event %d in state %d (with BYE) "
							"for dlg %p [%u:%u] with clid '%.*s' and tags '%.*s' '%.*s'\n",
							event, dlg->state, dlg, dlg->h_entry, dlg->h_id,
							dlg->callid.len, dlg->callid.s,
							dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
							dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
						break;
					}
					ref_dlg_unsafe(dlg,1);
				case DLG_STATE_UNCONFIRMED:
				case DLG_STATE_EARLY:
					dlg->state = DLG_STATE_CONFIRMED_NA;
					break;
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_CONFIRMED:
					break;
				default:
					log_next_state_dlg(event, dlg);
			}
			break;
		case DLG_EVENT_REQACK:
			switch (dlg->state) {
				case DLG_STATE_CONFIRMED_NA:
					dlg->state = DLG_STATE_CONFIRMED;
					break;
				case DLG_STATE_CONFIRMED:
					break;
				case DLG_STATE_DELETED:
					break;
				default:
					log_next_state_dlg(event, dlg);
			}
			break;
		case DLG_EVENT_REQBYE:
			switch (dlg->state) {
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_CONFIRMED:
					dlg->dflags |= DLG_FLAG_HASBYE;
					dlg->state = DLG_STATE_DELETED;
					*unref = 1;
					break;
				case DLG_STATE_EARLY:
				case DLG_STATE_DELETED:
					break;
				default:
					log_next_state_dlg(event, dlg);
			}
			break;
		case DLG_EVENT_REQPRACK:
			switch (dlg->state) {
				case DLG_STATE_EARLY:
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_DELETED:
					break;
				default:
					log_next_state_dlg(event, dlg);
			}
			break;
		case DLG_EVENT_REQ:
			switch (dlg->state) {
				case DLG_STATE_EARLY:
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_CONFIRMED:
				case DLG_STATE_DELETED:
					break;
				default:
					log_next_state_dlg(event, dlg);
			}
			break;
		default:
			LM_CRIT("unknown event %d in state %d "
				"for dlg %p [%u:%u] with clid '%.*s' and tags '%.*s' '%.*s'\n",
				event, dlg->state, dlg, dlg->h_entry, dlg->h_id,
				dlg->callid.len, dlg->callid.s,
				dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
				dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
	}
	*new_state = dlg->state;

	/* remove the dialog from profiles when is not no longer active */
	if(*new_state==DLG_STATE_DELETED && dlg->profile_links!=NULL
				&& *old_state!=*new_state) {
		destroy_linkers(dlg->profile_links);
		dlg->profile_links = NULL;
	}

	dlg_unlock( d_table, d_entry);

	LM_DBG("dialog %p changed from state %d to "
		"state %d, due event %d (ref %d)\n", dlg, *old_state, *new_state, event,
		dlg->ref);
}

/**
 *
 */
int dlg_set_toroute(struct dlg_cell *dlg, str *route)
{
	if(dlg==NULL || route==NULL || route->len<=0)
		return 0;
	if(dlg->toroute_name.s!=NULL) {
		shm_free(dlg->toroute_name.s);
		dlg->toroute_name.s = NULL;
		dlg->toroute_name.len = 0;
	}
	dlg->toroute_name.s = (char*)shm_malloc((route->len+1)*sizeof(char));
	if(dlg->toroute_name.s==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memcpy(dlg->toroute_name.s, route->s, route->len);
	dlg->toroute_name.len = route->len;
	dlg->toroute_name.s[dlg->toroute_name.len] = '\0';
	dlg->toroute = route_lookup(&main_rt, dlg->toroute_name.s);
	return 0;
}

/*
 * Internal function to adjust the lifetime of a dialog, used by
 * various userland functions that touch the dialog timeout.
 */

int	update_dlg_timeout(dlg_cell_t *dlg, int timeout)
{
	if(dlg->state!=DLG_STATE_UNCONFIRMED
			&& dlg->state!=DLG_STATE_EARLY) {
		if(update_dlg_timer(&dlg->tl, timeout) < 0) {
			LM_ERR("failed to update dialog lifetime\n");
			dlg_release(dlg);
			return -1;
		}
	}
	dlg->lifetime = timeout;
	dlg->dflags |= DLG_FLAG_CHANGED;

	dlg_release(dlg);

	return 0;
}

/**************************** MI functions ******************************/
/*!
 * \brief Helper method that output a dialog via the MI interface
 * \see mi_print_dlg
 * \param rpl MI node that should be filled
 * \param dlg printed dialog
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
static inline int internal_mi_print_dlg(struct mi_node *rpl,
									struct dlg_cell *dlg, int with_context)
{
	struct mi_node* node= NULL;
	struct mi_node* node1 = NULL;
	struct mi_attr* attr= NULL;
	int len;
	char* p;

	node = add_mi_node_child(rpl, 0, "dialog",6 , 0, 0 );
	if (node==0)
		goto error;

	attr = addf_mi_attr( node, 0, "hash", 4, "%u:%u",
			dlg->h_entry, dlg->h_id );
	if (attr==0)
		goto error;

	p= int2str((unsigned long)dlg->state, &len);
	node1 = add_mi_node_child( node, MI_DUP_VALUE, "state", 5, p, len);
	if (node1==0)
		goto error;

	p= int2str((unsigned long)dlg->ref, &len);
	node1 = add_mi_node_child( node, MI_DUP_VALUE, "ref_count", 9, p, len);
	if (node1==0)
		goto error;

	p= int2str((unsigned long)dlg->start_ts, &len);
	node1 = add_mi_node_child(node,MI_DUP_VALUE,"timestart",9, p, len);
	if (node1==0)
		goto error;

	p= int2str((unsigned long)dlg->tl.timeout, &len);
	node1 = add_mi_node_child(node,MI_DUP_VALUE, "timeout", 7, p, len);
	if (node1==0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "callid", 6,
			dlg->callid.s, dlg->callid.len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "from_uri", 8,
			dlg->from_uri.s, dlg->from_uri.len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "from_tag", 8,
			dlg->tag[DLG_CALLER_LEG].s, dlg->tag[DLG_CALLER_LEG].len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "caller_contact", 14,
			dlg->contact[DLG_CALLER_LEG].s,
			dlg->contact[DLG_CALLER_LEG].len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "caller_cseq", 11,
			dlg->cseq[DLG_CALLER_LEG].s,
			dlg->cseq[DLG_CALLER_LEG].len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE,"caller_route_set",16,
			dlg->route_set[DLG_CALLER_LEG].s,
			dlg->route_set[DLG_CALLER_LEG].len);
	if(node1 == 0)
		goto error;

	if (dlg->bind_addr[DLG_CALLER_LEG]) {
		node1 = add_mi_node_child(node, 0,
			"caller_bind_addr",16,
			dlg->bind_addr[DLG_CALLER_LEG]->sock_str.s,
			dlg->bind_addr[DLG_CALLER_LEG]->sock_str.len);
	} else {
		node1 = add_mi_node_child(node, 0,
			"caller_bind_addr",16,0,0);
	}

	if (dlg->bind_addr[DLG_CALLEE_LEG]) {
		node1 = add_mi_node_child(node, 0,
			"callee_bind_addr",16,
			dlg->bind_addr[DLG_CALLEE_LEG]->sock_str.s,
			dlg->bind_addr[DLG_CALLEE_LEG]->sock_str.len);
	} else {
		node1 = add_mi_node_child(node, 0,
			"callee_bind_addr",16,0,0);
	}

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "to_uri", 6,
			dlg->to_uri.s, dlg->to_uri.len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "to_tag", 6,
			dlg->tag[DLG_CALLEE_LEG].s, dlg->tag[DLG_CALLEE_LEG].len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "callee_contact", 14,
			dlg->contact[DLG_CALLEE_LEG].s,
			dlg->contact[DLG_CALLEE_LEG].len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE, "callee_cseq", 11,
			dlg->cseq[DLG_CALLEE_LEG].s,
			dlg->cseq[DLG_CALLEE_LEG].len);
	if(node1 == 0)
		goto error;

	node1 = add_mi_node_child(node, MI_DUP_VALUE,"callee_route_set",16,
			dlg->route_set[DLG_CALLEE_LEG].s,
			dlg->route_set[DLG_CALLEE_LEG].len);
	if(node1 == 0)
		goto error;

	if (with_context) {
		node1 = add_mi_node_child(node, 0, "context", 7, 0, 0);
		if(node1 == 0)
			goto error;
		run_dlg_callbacks( DLGCB_MI_CONTEXT,
		                   dlg,
		                   NULL,
		                   NULL,
		                   DLG_DIR_NONE,
		                   (void *)node1);
	}
	return 0;

error:
	LM_ERR("failed to add node\n");
	return -1;
}


/*!
 * \brief Output a dialog via the MI interface
 * \param rpl MI node that should be filled
 * \param dlg printed dialog
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
int mi_print_dlg(struct mi_node *rpl, struct dlg_cell *dlg, int with_context)
{
	return internal_mi_print_dlg( rpl, dlg, with_context);
}

/*!
 * \brief Helper function that output all dialogs via the MI interface
 * \see mi_print_dlgs
 * \param rpl MI node that should be filled
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
static int internal_mi_print_dlgs(struct mi_node *rpl, int with_context)
{
	struct dlg_cell *dlg;
	unsigned int i;

	LM_DBG("printing %i dialogs\n", d_table->size);

	for( i=0 ; i<d_table->size ; i++ ) {
		dlg_lock( d_table, &(d_table->entries[i]) );

		for( dlg=d_table->entries[i].first ; dlg ; dlg=dlg->next ) {
			if (internal_mi_print_dlg(rpl, dlg, with_context)!=0)
				goto error;
		}
		dlg_unlock( d_table, &(d_table->entries[i]) );
	}
	return 0;

error:
	dlg_unlock( d_table, &(d_table->entries[i]) );
	LM_ERR("failed to print dialog\n");
	return -1;
}


static inline struct mi_root* process_mi_params(struct mi_root *cmd_tree,
													struct dlg_cell **dlg_p)
{
	struct mi_node* node;
	struct dlg_entry *d_entry;
	struct dlg_cell *dlg;
	str *callid;
	str *from_tag;
	unsigned int h_entry;

	node = cmd_tree->node.kids;
	if (node == NULL) {
		/* no parameters at all */
		*dlg_p = NULL;
		return NULL;
	}

	/* we have params -> get callid and fromtag */
	callid = &node->value;
	if(callid->s==NULL || callid->len<=0)
		return init_mi_tree(400, MI_SSTR(MI_MISSING_PARM));
	LM_DBG("callid='%.*s'\n", callid->len, callid->s);

	node = node->next;
	if ( !node || !node->value.s || !node->value.len) {
		from_tag = NULL;
	} else {
		from_tag = &node->value;
		LM_DBG("from_tag='%.*s'\n", from_tag->len, from_tag->s);
		if ( node->next!=NULL )
			return init_mi_tree( 400, MI_SSTR(MI_MISSING_PARM));
	}

	h_entry = core_hash( callid, 0, d_table->size);

	d_entry = &(d_table->entries[h_entry]);
	dlg_lock( d_table, d_entry);

	for( dlg = d_entry->first ; dlg ; dlg = dlg->next ) {
		if (match_downstream_dialog( dlg, callid, from_tag)==1) {
			if (dlg->state==DLG_STATE_DELETED) {
				*dlg_p = NULL;
				break;
			} else {
				*dlg_p = dlg;
				dlg_unlock( d_table, d_entry);
				return 0;
			}
		}
	}
	dlg_unlock( d_table, d_entry);

	return init_mi_tree( 404, MI_SSTR("Nu such dialog"));
}


/*!
 * \brief Output all dialogs via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_print_dlgs(struct mi_root *cmd_tree, void *param )
{
	struct mi_root* rpl_tree= NULL;
	struct mi_node* rpl = NULL;
	struct dlg_cell* dlg = NULL;

	rpl_tree = process_mi_params( cmd_tree, &dlg);
	if (rpl_tree)
		/* param error */
		return rpl_tree;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	if (dlg==NULL) {
		if ( internal_mi_print_dlgs(rpl,0)!=0 )
			goto error;
	} else {
		if ( internal_mi_print_dlg(rpl,dlg,0)!=0 )
			goto error;
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return NULL;
}


/*!
 * \brief Print a dialog context via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_print_dlgs_ctx(struct mi_root *cmd_tree, void *param )
{
	struct mi_root* rpl_tree= NULL;
	struct mi_node* rpl = NULL;
	struct dlg_cell* dlg = NULL;

	rpl_tree = process_mi_params( cmd_tree, &dlg);
	if (rpl_tree)
		/* param error */
		return rpl_tree;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	if (dlg==NULL) {
		if ( internal_mi_print_dlgs(rpl,1)!=0 )
			goto error;
	} else {
		if ( internal_mi_print_dlg(rpl,dlg,1)!=0 )
			goto error;
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return NULL;
}

/*!
 * \brief Terminate all or selected dialogs via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_terminate_dlgs(struct mi_root *cmd_tree, void *param )
{
	struct mi_root* rpl_tree= NULL;
	struct dlg_cell* dlg = NULL;
	str headers = {0, 0};

	rpl_tree = process_mi_params( cmd_tree, &dlg);
	if (rpl_tree)
		/* param error */
		return rpl_tree;
	if (dlg==NULL)
		return init_mi_tree( 400, MI_SSTR(MI_MISSING_PARM));

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	if (dlg_bye_all(dlg, &headers)!=0)
		goto error;
	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return NULL;
}


