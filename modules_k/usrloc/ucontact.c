/* 
 * $Id$ 
 *
 * Usrloc contact structure
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
 * 2003-03-12 added replication mark and three zombie states (nils)
 * 2004-03-17 generic callbacks added (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 */



#include "ucontact.h"
#include <string.h>             /* memcpy */
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "ul_mod.h"
#include "ul_callback.h"


/*
 * Create a new contact structure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact,
														ucontact_info_t* _ci)
{
#define str_dup(_new,_org) \
	do{ \
		(_new)->s = (char*)shm_malloc((_org)->len); \
		if ( (_new)->s == 0) { \
			LOG(L_ERR, "ERROR:usrloc:new_contact: no more shm memory\n"); \
			goto error;\
		}\
		memcpy((_new)->s, (_org)->s, (_org)->len);\
		(_new)->len = (_org)->len; \
	}while(0)

	ucontact_t *c;

	c = (ucontact_t*)shm_malloc(sizeof(ucontact_t));
	if (!c) {
		LOG(L_ERR, "ERROR:usrloc:new_ucontact: no more shm memory\n");
		return 0;
	}
	memset(c, 0, sizeof(ucontact_t));

	str_dup( &c->c, _contact);
	str_dup( &c->callid, _ci->callid);
	str_dup( &c->user_agent, _ci->user_agent);
	if (_ci->received && _ci->received->len) {
		str_dup( &c->received, _ci->received);
	}

	c->domain = _dom;
	c->aor = _aor;
	c->expires = _ci->expires;
	c->q = _ci->q;
	c->sock = _ci->sock;
	c->cseq = _ci->cseq;
	c->state = CS_NEW;
	c->flags = _ci->flags1;
	c->methods = _ci->methods;

	return c;
error:
	if (c->received.s) shm_free(c->received.s);
	if (c->user_agent.s) shm_free(c->user_agent.s);
	if (c->callid.s) shm_free(c->callid.s);
	if (c->c.s) shm_free(c->c.s);
	shm_free(c);
	return 0;
}



/*
 * Free all memory associated with given contact structure
 */
void free_ucontact(ucontact_t* _c)
{
	if (!_c) return;
	if (_c->received.s) shm_free(_c->received.s);
	if (_c->user_agent.s) shm_free(_c->user_agent.s);
	if (_c->callid.s) shm_free(_c->callid.s);
	if (_c->c.s) shm_free(_c->c.s);
	shm_free( _c );
}


/*
 * Print contact, for debugging purposes only
 */
void print_ucontact(FILE* _f, ucontact_t* _c)
{
	time_t t = time(0);
	char* st;

	switch(_c->state) {
	case CS_NEW:   st = "CS_NEW";     break;
	case CS_SYNC:  st = "CS_SYNC";    break;
	case CS_DIRTY: st = "CS_DIRTY";   break;
	default:       st = "CS_UNKNOWN"; break;
	}

	fprintf(_f, "~~~Contact(%p)~~~\n", _c);
	fprintf(_f, "domain    : '%.*s'\n", _c->domain->len, ZSW(_c->domain->s));
	fprintf(_f, "aor       : '%.*s'\n", _c->aor->len, ZSW(_c->aor->s));
	fprintf(_f, "Contact   : '%.*s'\n", _c->c.len, ZSW(_c->c.s));
	fprintf(_f, "Expires   : ");
	if (_c->flags & FL_PERMANENT) {
		fprintf(_f, "Permanent\n");
	} else {
		if (_c->expires == 0) {
			fprintf(_f, "Deleted\n");
		} else if (t > _c->expires) {
			fprintf(_f, "Expired\n");
		} else {
			fprintf(_f, "%u\n", (unsigned int)(_c->expires - t));
		}
	}
	fprintf(_f, "q         : %s\n", q2str(_c->q, 0));
	fprintf(_f, "Call-ID   : '%.*s'\n", _c->callid.len, ZSW(_c->callid.s));
	fprintf(_f, "CSeq      : %d\n", _c->cseq);
	fprintf(_f, "User-Agent: '%.*s'\n",
		_c->user_agent.len, ZSW(_c->user_agent.s));
	fprintf(_f, "received  : '%.*s'\n",
		_c->received.len, ZSW(_c->received.s));
	fprintf(_f, "State     : %s\n", st);
	fprintf(_f, "Flags     : %u\n", _c->flags);
	if (_c->sock) {
		fprintf(_f, "Sock      : %.*s:%d (%p)\n",
			_c->sock->address_str.len,_c->sock->address_str.s,
			_c->sock->port_no,_c->sock);
	} else {
		fprintf(_f, "Sock      : none (null)\n");
	}
	fprintf(_f, "Methods   : %u\n", _c->methods);
	fprintf(_f, "next      : %p\n", _c->next);
	fprintf(_f, "prev      : %p\n", _c->prev);
	fprintf(_f, "~~~/Contact~~~~\n");
}


/*
 * Update ucontact structure in memory
 */
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t* _ci)
{
#define update_str(_old,_new) \
	do{\
		if ((_old)->len < (_new)->len) { \
			ptr = (char*)shm_malloc((_new)->len); \
			if (ptr == 0) { \
				LOG(L_ERR, "ERROR:usrloc:update_ucontact: no "\
					"more shm memory\n"); \
				return -1; \
			}\
			memcpy(ptr, (_new)->s, (_new)->len);\
			shm_free((_old)->s);\
			(_old)->s = ptr;\
		} else {\
			memcpy((_old)->s, (_new)->s, (_new)->len);\
		}\
		(_old)->len = (_new)->len;\
	} while(0)

	char* ptr;

	update_str( &_c->callid, _ci->callid);

	update_str( &_c->user_agent, _ci->user_agent);

	if (_ci->received) {
		update_str( &_c->received, _ci->received);
	} else {
		if (_c->received.s) shm_free(_c->received.s);
		_c->received.s = 0;
		_c->received.len = 0;
	}

	_c->sock = _ci->sock;
	_c->expires = _ci->expires;
	_c->q = _ci->q;
	_c->cseq = _ci->cseq;
	_c->methods = _ci->methods;
	_c->flags |= _ci->flags1;
	_c->flags &= ~_ci->flags2;

	return 0;
}


/* ================ State related functions =============== */


/*
 * Update state of the contact
 */
void st_update_ucontact(ucontact_t* _c)
{
	switch(_c->state) {
	case CS_NEW:
		     /* Contact is new and is not in the database yet,
		      * we remain in the same state here because the
		      * contact must be inserted later in the timer
		      */
		break;

	case CS_SYNC:
		     /* For db mode 2 a modified contact needs to be 
			  * updated also in the database, so transit into 
			  * CS_DIRTY and let the timer to do the update 
			  * again. For db mode 1 the db update is already
			  * done and we don't have to change the state.
		      */
		if (db_mode == WRITE_BACK) {
			_c->state = CS_DIRTY;
		}
		break;

	case CS_DIRTY:
		     /* Modification of dirty contact results in
		      * dirty contact again, don't change anything
		      */
		break;
	}
}


/*
 * Update state of the contact
 * Returns 1 if the contact should be
 * delete from memory immediately,
 * 0 otherwise
 */
int st_delete_ucontact(ucontact_t* _c)
{
	switch(_c->state) {
	case CS_NEW:
		     /* Contact is new and isn't in the database
		      * yet, we can delete it from the memory
		      * safely.
		      */
		return 1;

	case CS_SYNC:
	case CS_DIRTY:
		     /* Contact is in the database,
		      * we cannot remove it from the memory 
		      * directly, but we can set expires to zero
		      * and the timer will take care of deleting 
		      * the contact from the memory as well as 
		      * from the database
		      */
		if (db_mode == WRITE_BACK) {
			/* Reset permanent flag */
			_c->flags &= ~FL_PERMANENT;
			_c->expires = 0;
			return 0;
		} else {
			     /* WRITE_THROUGH or NO_DB -- we can
			      * remove it from memory immediately and
			      * the calling function would also remove
			      * it from the database if needed
			      */
			return 1;
		}
	}

	return 0; /* Makes gcc happy */
}


/*
 * Called when the timer is about to delete
 * an expired contact, this routine returns
 * 1 if the contact should be removed from
 * the database and 0 otherwise
 */
int st_expired_ucontact(ucontact_t* _c)
{
	     /* There is no need to change contact
	      * state, because the contact will
	      * be deleted anyway
	      */

	switch(_c->state) {
	case CS_NEW:
		     /* Contact is not in the database
		      * yet, remove it from memory only
		      */
		return 0;

	case CS_SYNC:
	case CS_DIRTY:
		     /* Remove from database here */
		return 1;
	}

	return 0; /* Makes gcc happy */
}


/*
 * Called when the timer is about flushing the contact,
 * updates contact state and returns 1 if the contact
 * should be inserted, 2 if update and 0 otherwise
 */
int st_flush_ucontact(ucontact_t* _c)
{
	switch(_c->state) {
	case CS_NEW:
		     /* Contact is new and is not in
		      * the database yet so we have
		      * to insert it
		      */
		_c->state = CS_SYNC;
		return 1;

	case CS_SYNC:
		     /* Contact is synchronized, do
		      * nothing
		      */
		return 0;

	case CS_DIRTY:
		     /* Contact has been modified and
		      * is in the db already so we
		      * have to update it
		      */
		_c->state = CS_SYNC;
		return 2;
	}

	return 0; /* Makes gcc happy */
}


/* ============== Database related functions ================ */

/*
 * Insert contact into the database
 */
int db_insert_ucontact(ucontact_t* _c)
{
	char* dom;
	db_key_t keys[12];
	db_val_t vals[12];
	
	if (_c->flags & FL_MEM) {
		return 0;
	}

	keys[0] = user_col.s;
	keys[1] = contact_col.s;
	keys[2] = expires_col.s;
	keys[3] = q_col.s;
	keys[4] = callid_col.s;
	keys[5] = cseq_col.s;
	keys[6] = flags_col.s;
	keys[7] = user_agent_col.s;
	keys[8] = received_col.s;
	keys[9] = sock_col.s;
	keys[10] = methods_col.s;
	keys[11] = domain_col.s;

	vals[0].type = DB_STR;
	vals[0].nul = 0;
	vals[0].val.str_val.s = _c->aor->s;
	vals[0].val.str_val.len = _c->aor->len;

	vals[1].type = DB_STR;
	vals[1].nul = 0;
	vals[1].val.str_val.s = _c->c.s; 
	vals[1].val.str_val.len = _c->c.len;

	vals[2].type = DB_DATETIME;
	vals[2].nul = 0;
	vals[2].val.time_val = _c->expires;

	vals[3].type = DB_DOUBLE;
	vals[3].nul = 0;
	vals[3].val.double_val = q2double(_c->q);

	vals[4].type = DB_STR;
	vals[4].nul = 0;
	vals[4].val.str_val.s = _c->callid.s;
	vals[4].val.str_val.len = _c->callid.len;

	vals[5].type = DB_INT;
	vals[5].nul = 0;
	vals[5].val.int_val = _c->cseq;

	vals[6].type = DB_INT;
	vals[6].nul = 0;
	vals[6].val.bitmap_val = _c->flags;

	vals[7].type = DB_STR;
	vals[7].nul = 0;
	vals[7].val.str_val.s = _c->user_agent.s;
	vals[7].val.str_val.len = _c->user_agent.len;

	vals[8].type = DB_STR;

	if (_c->received.s == 0) {
		vals[8].nul = 1;
	} else {
		vals[8].nul = 0;
		vals[8].val.str_val.s = _c->received.s;
		vals[8].val.str_val.len = _c->received.len;
	}

	vals[9].type = DB_STR;
	if (_c->sock) {
		vals[9].val.str_val = _c->sock->sock_str;
		vals[9].nul = 0;
	} else {
		vals[9].nul = 1;
	}

	vals[10].type = DB_BITMAP;
	if (_c->methods == 0xFFFFFFFF) {
		vals[10].nul = 1;
	} else {
		vals[10].val.bitmap_val = _c->methods;
		vals[10].nul = 0;
	}

	if (use_domain) {
		dom = q_memchr(_c->aor->s, '@', _c->aor->len);
		vals[0].val.str_val.len = dom - _c->aor->s;

		vals[11].type = DB_STR;
		vals[11].nul = 0;
		vals[11].val.str_val.s = dom + 1;
		vals[11].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
	}

	if (ul_dbf.use_table(ul_dbh, _c->domain->s) < 0) {
		LOG(L_ERR, "db_insert_ucontact(): Error in use_table\n");
		return -1;
	}

	if (ul_dbf.insert(ul_dbh, keys, vals, (use_domain) ? (12) : (11)) < 0) {
		LOG(L_ERR, "db_insert_ucontact(): Error while inserting contact\n");
		return -1;
	}

	return 0;
}


/*
 * Update contact in the database
 */
int db_update_ucontact(ucontact_t* _c)
{
	char* dom;
	db_key_t keys1[3];
	db_val_t vals1[3];

	db_key_t keys2[9];
	db_val_t vals2[9];

	if (_c->flags & FL_MEM) {
		return 0;
	}

	keys1[0] = user_col.s;
	keys1[1] = contact_col.s;
	keys1[2] = domain_col.s;
	keys2[0] = expires_col.s;
	keys2[1] = q_col.s;
	keys2[2] = callid_col.s;
	keys2[3] = cseq_col.s;
	keys2[4] = flags_col.s;
	keys2[5] = user_agent_col.s;
	keys2[6] = received_col.s;
	keys2[7] = sock_col.s;
	keys2[8] = methods_col.s;
	
	vals1[0].type = DB_STR;
	vals1[0].nul = 0;
	vals1[0].val.str_val = *_c->aor;

	vals1[1].type = DB_STR;
	vals1[1].nul = 0;
	vals1[1].val.str_val = _c->c;

	vals2[0].type = DB_DATETIME;
	vals2[0].nul = 0;
	vals2[0].val.time_val = _c->expires;

	vals2[1].type = DB_DOUBLE;
	vals2[1].nul = 0;
	vals2[1].val.double_val = q2double(_c->q);

	vals2[2].type = DB_STR;
	vals2[2].nul = 0;
	vals2[2].val.str_val = _c->callid;

	vals2[3].type = DB_INT;
	vals2[3].nul = 0;
	vals2[3].val.int_val = _c->cseq;

	vals2[4].type = DB_INT;
	vals2[4].nul = 0;
	vals2[4].val.bitmap_val = _c->flags;

	vals2[5].type = DB_STR;
	vals2[5].nul = 0;
	vals2[5].val.str_val = _c->user_agent;

	vals2[6].type = DB_STR;
	if (_c->received.s == 0) {
		vals2[6].nul = 1;
	} else {
		vals2[6].nul = 0;
		vals2[6].val.str_val = _c->received;
	}

	vals2[7].type = DB_STR;
	if (_c->sock) {
		vals2[7].val.str_val = _c->sock->sock_str;
		vals2[7].nul = 0;
	} else {
		vals2[7].nul = 1;
	}

	vals2[8].type = DB_BITMAP;
	if (_c->methods == 0xFFFFFFFF) {
		vals2[8].nul = 1;
	} else {
		vals2[8].val.bitmap_val = _c->methods;
		vals2[8].nul = 0;
	}

	if (use_domain) {
		dom = q_memchr(_c->aor->s, '@', _c->aor->len);
		vals1[0].val.str_val.len = dom - _c->aor->s;

		vals1[2].type = DB_STR;
		vals1[2].nul = 0;
		vals1[2].val.str_val.s = dom + 1;
		vals1[2].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
	}

	if (ul_dbf.use_table(ul_dbh, _c->domain->s) < 0) {
		LOG(L_ERR, "db_upd_ucontact(): Error in use_table\n");
		return -1;
	}

	if (ul_dbf.update(ul_dbh, keys1, 0, vals1, keys2, vals2, 
	(use_domain) ? (3) : (2), 9) < 0) {
		LOG(L_ERR, "db_upd_ucontact(): Error while updating database\n");
		return -1;
	}

	return 0;
}


/*
 * Delete contact from the database
 */
int db_delete_ucontact(ucontact_t* _c)
{
	char* dom;
	db_key_t keys[3];
	db_val_t vals[3];

	if (_c->flags & FL_MEM) {
		return 0;
	}

	keys[0] = user_col.s;
	keys[1] = contact_col.s;
	keys[2] = domain_col.s;

	vals[0].type = DB_STR;
	vals[0].nul = 0;
	vals[0].val.str_val = *_c->aor;

	vals[1].type = DB_STR;
	vals[1].nul = 0;
	vals[1].val.str_val = _c->c;

	if (use_domain) {
		dom = q_memchr(_c->aor->s, '@', _c->aor->len);
		vals[0].val.str_val.len = dom - _c->aor->s;

		vals[2].type = DB_STR;
		vals[2].nul = 0;
		vals[2].val.str_val.s = dom + 1;
		vals[2].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
	}

	if (ul_dbf.use_table(ul_dbh, _c->domain->s) < 0) {
		LOG(L_ERR, "db_del_ucontact: Error in use_table\n");
		return -1;
	}

	if (ul_dbf.delete(ul_dbh, keys, 0, vals, (use_domain) ? (3) : (2)) < 0) {
		LOG(L_ERR, "db_del_ucontact(): Error while deleting from database\n");
		return -1;
	}

	return 0;
}


/*
 * Update ucontact with new values
 */
int update_ucontact(ucontact_t* _c, ucontact_info_t* _ci)
{
	/* run callbacks for UPDATE event */
	if (exists_ulcb_type(UL_CONTACT_UPDATE)) {
		run_ul_callbacks( UL_CONTACT_UPDATE, _c);
	}

	/* we have to update memory in any case, but database directly
	 * only in db_mode 1 */
	if (mem_update_ucontact( _c, _ci) < 0) {
		LOG(L_ERR, "ERROR:usrloc:update_ucontact: failed to update memory\n");
		return -1;
	}
	st_update_ucontact(_c);
	if (db_mode == WRITE_THROUGH) {
		if (db_update_ucontact(_c) < 0) {
			LOG(L_ERR, "ERROR:usrloc:update_ucontact: failed to update "
				"database\n");
		}
	}
	return 0;
}
