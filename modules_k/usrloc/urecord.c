/* 
 * $Id$ 
 *
 * Usrloc record structure
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ---------
 * 2003-03-12 added replication mark and zombie state support (nils)
 * 2004-03-17 generic callbacks added (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 */

/*! \file
 *  \brief USRLOC - Usrloc record structure
 *  \ingroup usrloc
 *
 * - Module \ref usrloc
 */


#include "urecord.h"
#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "ul_mod.h"
#include "utime.h"
#include "ul_callback.h"


int matching_mode = CONTACT_ONLY;

int cseq_delay = 20;

/*! \brief
 * Create and initialize new record structure
 */
int new_urecord(str* _dom, str* _aor, urecord_t** _r)
{
	*_r = (urecord_t*)shm_malloc(sizeof(urecord_t));
	if (*_r == 0) {
		LM_ERR("no more share memory\n");
		return -1;
	}
	memset(*_r, 0, sizeof(urecord_t));

	(*_r)->aor.s = (char*)shm_malloc(_aor->len);
	if ((*_r)->aor.s == 0) {
		LM_ERR("no more share memory\n");
		shm_free(*_r);
		*_r = 0;
		return -2;
	}
	memcpy((*_r)->aor.s, _aor->s, _aor->len);
	(*_r)->aor.len = _aor->len;
	(*_r)->domain = _dom;
	(*_r)->aorhash = core_hash(_aor, 0, 0);
	return 0;
}


/*! \brief
 * Free all memory used by the given structure
 * The structure must be removed from all linked
 * lists first
 */
void free_urecord(urecord_t* _r)
{
	ucontact_t* ptr;

	while(_r->contacts) {
		ptr = _r->contacts;
		_r->contacts = _r->contacts->next;
		free_ucontact(ptr);
	}
	
	/* if mem cache is not used, the urecord struct is static*/
	if (db_mode!=DB_ONLY) {
		if (_r->aor.s) shm_free(_r->aor.s);
		shm_free(_r);
	}
}


/*! \brief
 * Print a record
 */
void print_urecord(FILE* _f, urecord_t* _r)
{
	ucontact_t* ptr;

	fprintf(_f, "...Record(%p)...\n", _r);
	fprintf(_f, "domain : '%.*s'\n", _r->domain->len, ZSW(_r->domain->s));
	fprintf(_f, "aor    : '%.*s'\n", _r->aor.len, ZSW(_r->aor.s));
	fprintf(_f, "aorhash: '%u'\n", (unsigned)_r->aorhash);
	fprintf(_f, "slot:    '%d'\n", _r->aorhash&(_r->slot->d->size-1));
	
	if (_r->contacts) {
		ptr = _r->contacts;
		while(ptr) {
			print_ucontact(_f, ptr);
			ptr = ptr->next;
		}
	}

	fprintf(_f, ".../Record...\n");
}


/*! \brief
 * Add a new contact
 * Contacts are ordered by: 1) q 
 *                          2) descending modification time
 */
ucontact_t* mem_insert_ucontact(urecord_t* _r, str* _c, ucontact_info_t* _ci)
{
	ucontact_t* ptr, *prev = 0;
	ucontact_t* c;

	if ( (c=new_ucontact(_r->domain, &_r->aor, _c, _ci)) == 0) {
		LM_ERR("failed to create new contact\n");
		return 0;
	}
	if_update_stat( _r->slot, _r->slot->d->contacts, 1);

	ptr = _r->contacts;

	if (!desc_time_order) {
		while(ptr) {
			if (ptr->q < c->q) break;
			prev = ptr;
			ptr = ptr->next;
		}
	}

	if (ptr) {
		if (!ptr->prev) {
			ptr->prev = c;
			c->next = ptr;
			_r->contacts = c;
		} else {
			c->next = ptr;
			c->prev = ptr->prev;
			ptr->prev->next = c;
			ptr->prev = c;
		}
	} else if (prev) {
		prev->next = c;
		c->prev = prev;
	} else {
		_r->contacts = c;
	}

	return c;
}


/*! \brief
 * Remove the contact from lists
 */
void mem_remove_ucontact(urecord_t* _r, ucontact_t* _c)
{
	if (_c->prev) {
		_c->prev->next = _c->next;
		if (_c->next) {
			_c->next->prev = _c->prev;
		}
	} else {
		_r->contacts = _c->next;
		if (_c->next) {
			_c->next->prev = 0;
		}
	}
}	



/*! \brief
 * Remove contact from the list and delete
 */
void mem_delete_ucontact(urecord_t* _r, ucontact_t* _c)
{
	mem_remove_ucontact(_r, _c);
	if_update_stat( _r->slot, _r->slot->d->contacts, -1);
	free_ucontact(_c);
}


/*! \brief
 * This timer routine is used when
 * db_mode is set to NO_DB
 */
static inline int nodb_timer(urecord_t* _r)
{
	ucontact_t* ptr, *t;

	ptr = _r->contacts;

	while(ptr) {
		if (!VALID_CONTACT(ptr, act_time)) {
			/* run callbacks for EXPIRE event */
			if (exists_ulcb_type(UL_CONTACT_EXPIRE))
				run_ul_callbacks( UL_CONTACT_EXPIRE, ptr);

			LM_DBG("Binding '%.*s','%.*s' has expired\n",
				ptr->aor->len, ZSW(ptr->aor->s),
				ptr->c.len, ZSW(ptr->c.s));

			t = ptr;
			ptr = ptr->next;

			mem_delete_ucontact(_r, t);
			update_stat( _r->slot->d->expires, 1);
		} else {
			ptr = ptr->next;
		}
	}

	return 0;
}



/*! \brief
 * This routine is used when db_mode is
 * set to WRITE_THROUGH
 */
static inline int wt_timer(urecord_t* _r)
{
	ucontact_t* ptr, *t;

	ptr = _r->contacts;

	while(ptr) {
		if (!VALID_CONTACT(ptr, act_time)) {
			/* run callbacks for EXPIRE event */
			if (exists_ulcb_type(UL_CONTACT_EXPIRE)) {
				run_ul_callbacks( UL_CONTACT_EXPIRE, ptr);
			}

			LM_DBG("Binding '%.*s','%.*s' has expired\n",
				ptr->aor->len, ZSW(ptr->aor->s),
				ptr->c.len, ZSW(ptr->c.s));

			t = ptr;
			ptr = ptr->next;

			if (db_delete_ucontact(t) < 0) {
				LM_ERR("deleting contact from database failed\n");
			}
			mem_delete_ucontact(_r, t);
			update_stat( _r->slot->d->expires, 1);
		} else {
			ptr = ptr->next;
		}
	}
	
	return 0;
}



/*! \brief
 * Write-back timer
 */
static inline int wb_timer(urecord_t* _r)
{
	ucontact_t* ptr, *t;
	cstate_t old_state;
	int op;

	ptr = _r->contacts;

	while(ptr) {
		if (!VALID_CONTACT(ptr, act_time)) {
			/* run callbacks for EXPIRE event */
			if (exists_ulcb_type(UL_CONTACT_EXPIRE)) {
				run_ul_callbacks( UL_CONTACT_EXPIRE, ptr);
			}

			LM_DBG("Binding '%.*s','%.*s' has expired\n",
				ptr->aor->len, ZSW(ptr->aor->s),
				ptr->c.len, ZSW(ptr->c.s));
			update_stat( _r->slot->d->expires, 1);

			t = ptr;
			ptr = ptr->next;

			/* Should we remove the contact from the database ? */
			if (st_expired_ucontact(t) == 1) {
				if (db_delete_ucontact(t) < 0) {
					LM_ERR("failed to delete contact from the database\n");
				}
			}

			mem_delete_ucontact(_r, t);
		} else {
			/* Determine the operation we have to do */
			old_state = ptr->state;
			op = st_flush_ucontact(ptr);

			switch(op) {
			case 0: /* do nothing, contact is synchronized */
				break;

			case 1: /* insert */
				if (db_insert_ucontact(ptr) < 0) {
					LM_ERR("inserting contact into database failed\n");
					ptr->state = old_state;
				}
				break;

			case 2: /* update */
				if (db_update_ucontact(ptr) < 0) {
					LM_ERR("updating contact in db failed\n");
					ptr->state = old_state;
				}
				break;
			}

			ptr = ptr->next;
		}
	}

	return 0;
}



int timer_urecord(urecord_t* _r)
{
	switch(db_mode) {
	case NO_DB:         return nodb_timer(_r);
	/* use also the write_back timer routine to handle the failed
	 * realtime inserts/updates */
	case WRITE_THROUGH: return wb_timer(_r); /*wt_timer(_r);*/
	case WRITE_BACK:    return wb_timer(_r);
	}

	return 0; /* Makes gcc happy */
}



int db_delete_urecord(urecord_t* _r)
{
	db_key_t keys[2];
	db_val_t vals[2];
	char* dom;

	keys[0] = &user_col;
	keys[1] = &domain_col;
	vals[0].type = DB_STR;
	vals[0].nul = 0;
	vals[0].val.str_val.s = _r->aor.s;
	vals[0].val.str_val.len = _r->aor.len;

	if (use_domain) {
		dom = q_memchr(_r->aor.s, '@', _r->aor.len);
		vals[0].val.str_val.len = dom - _r->aor.s;

		vals[1].type = DB_STR;
		vals[1].nul = 0;
		vals[1].val.str_val.s = dom + 1;
		vals[1].val.str_val.len = _r->aor.s + _r->aor.len - dom - 1;
	}

	if (ul_dbf.use_table(ul_dbh, _r->domain) < 0) {
		LM_ERR("use_table failed\n");
		return -1;
	}

	if (ul_dbf.delete(ul_dbh, keys, 0, vals, (use_domain) ? (2) : (1)) < 0) {
		LM_ERR("failed to delete from database\n");
		return -1;
	}

	return 0;
}


/*! \brief
 * Release urecord previously obtained
 * through get_urecord
 */
void release_urecord(urecord_t* _r)
{
	if (db_mode==DB_ONLY) {
		free_urecord(_r);
	} else if (_r->contacts == 0) {
		mem_delete_urecord(_r->slot->d, _r);
	}
}


/*! \brief
 * Create and insert new contact
 * into urecord
 */
int insert_ucontact(urecord_t* _r, str* _contact, ucontact_info_t* _ci,
															ucontact_t** _c)
{
	if ( ((*_c)=mem_insert_ucontact(_r, _contact, _ci)) == 0) {
		LM_ERR("failed to insert contact\n");
		return -1;
	}

	if (exists_ulcb_type(UL_CONTACT_INSERT)) {
		run_ul_callbacks( UL_CONTACT_INSERT, *_c);
	}

	if (db_mode == WRITE_THROUGH || db_mode==DB_ONLY) {
		if (db_insert_ucontact(*_c) < 0) {
			LM_ERR("failed to insert in database\n");
		} else {
			(*_c)->state = CS_SYNC;
		}
	}

	return 0;
}


/*! \brief
 * Delete ucontact from urecord
 */
int delete_ucontact(urecord_t* _r, struct ucontact* _c)
{
	if (exists_ulcb_type(UL_CONTACT_DELETE)) {
		run_ul_callbacks( UL_CONTACT_DELETE, _c);
	}

	if (st_delete_ucontact(_c) > 0) {
		if (db_mode == WRITE_THROUGH || db_mode==DB_ONLY) {
			if (db_delete_ucontact(_c) < 0) {
				LM_ERR("failed to remove contact from database\n");
			}
		}

		mem_delete_ucontact(_r, _c);
	}

	return 0;
}


static inline struct ucontact* contact_match( ucontact_t* ptr, str* _c)
{
	while(ptr) {
		if ((_c->len == ptr->c.len) && !memcmp(_c->s, ptr->c.s, _c->len)) {
			return ptr;
		}
		
		ptr = ptr->next;
	}
	return 0;
}


static inline struct ucontact* contact_callid_match( ucontact_t* ptr,
														str* _c, str *_callid)
{
	while(ptr) {
		if ( (_c->len==ptr->c.len) && (_callid->len==ptr->callid.len)
		&& !memcmp(_c->s, ptr->c.s, _c->len)
		&& !memcmp(_callid->s, ptr->callid.s, _callid->len)
		) {
			return ptr;
		}
		
		ptr = ptr->next;
	}
	return 0;
}


/*! \brief
 * Get pointer to ucontact with given contact
 * Returns:
 *      0 - found
 *      1 - not found
 *     -1 - invalid found
 *     -2 - found, but to be skipped (same cseq)
 */
int get_ucontact(urecord_t* _r, str* _c, str* _callid, int _cseq,
														struct ucontact** _co)
{
	ucontact_t* ptr;
	int no_callid;

	ptr = 0;
	no_callid = 0;
	*_co = 0;

	switch (matching_mode) {
		case CONTACT_ONLY:
			ptr = contact_match( _r->contacts, _c);
			break;
		case CONTACT_CALLID:
			ptr = contact_callid_match( _r->contacts, _c, _callid);
			no_callid = 1;
			break;
		default:
			LM_CRIT("unknown matching_mode %d\n", matching_mode);
			return -1;
	}

	if (ptr) {
		/* found -> check callid and cseq */
		if ( no_callid || (ptr->callid.len==_callid->len
		&& memcmp(_callid->s, ptr->callid.s, _callid->len)==0 ) ) {
			if (_cseq<ptr->cseq)
				return -1;
			if (_cseq==ptr->cseq) {
				get_act_time();
				return (ptr->last_modified+cseq_delay>act_time)?-2:-1;
			}
		}
		*_co = ptr;
		return 0;
	}

	return 1;
}
