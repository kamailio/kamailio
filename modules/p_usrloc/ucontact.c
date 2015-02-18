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

/*! \file
 *  \brief USRLOC - Usrloc contact handling functions
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */

#include "ucontact.h"
#include <string.h>             /* memcpy */
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db.h"
#include "p_usrloc_mod.h"
#include "../usrloc/ul_callback.h"
#include "urecord.h"
#include "ucontact.h"
#include "ul_db_layer.h"
#include "dlist.h"

/*!
 * \brief Create a new contact structure
 * \param _dom domain
 * \param _aor address of record
 * \param _contact contact string
 * \param _ci contact informations
 * \return new created contact on success, 0 on failure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact, ucontact_info_t* _ci)
{
	ucontact_t *c;

	c = (ucontact_t*)shm_malloc(sizeof(ucontact_t));
	if (!c) {
		LM_ERR("no more shm memory\n");
		return 0;
	}
	memset(c, 0, sizeof(ucontact_t));

	if (shm_str_dup( &c->c, _contact) < 0) goto error;
	if (shm_str_dup( &c->callid, _ci->callid) < 0) goto error;
	if (shm_str_dup( &c->user_agent, _ci->user_agent) < 0) goto error;

	if (_ci->received.s && _ci->received.len) {
		if (shm_str_dup( &c->received, &_ci->received) < 0) goto error;
	}
	if (_ci->path && _ci->path->len) {
		if (shm_str_dup( &c->path, _ci->path) < 0) goto error;
	}
	if (_ci->ruid.s && _ci->ruid.len) {
		if (shm_str_dup( &c->ruid, &_ci->ruid) < 0) goto error;
	}
	if (_ci->instance.s && _ci->instance.len) {
		if (shm_str_dup( &c->instance, &_ci->instance) < 0) goto error;
	}

	c->domain = _dom;
	c->aor = _aor;
	c->expires = _ci->expires;
	c->q = _ci->q;
	c->sock = _ci->sock;
	c->cseq = _ci->cseq;
	c->state = CS_NEW;
	c->flags = _ci->flags;
	c->cflags = _ci->cflags;
	c->methods = _ci->methods;
	c->reg_id = _ci->reg_id;
	c->last_modified = _ci->last_modified;

	return c;
error:
	LM_ERR("no more shm memory\n");
	if (c->path.s) shm_free(c->path.s);
	if (c->received.s) shm_free(c->received.s);
	if (c->user_agent.s) shm_free(c->user_agent.s);
	if (c->callid.s) shm_free(c->callid.s);
	if (c->c.s) shm_free(c->c.s);
	if (c->ruid.s) shm_free(c->ruid.s);
	if (c->instance.s) shm_free(c->instance.s);
	shm_free(c);
	return 0;
}



/*!
 * \brief Free all memory associated with given contact structure
 * \param _c freed contact
 */
void free_ucontact(ucontact_t* _c)
{
	if (!_c) return;
	if (_c->path.s) shm_free(_c->path.s);
	if (_c->received.s) shm_free(_c->received.s);
	if (_c->user_agent.s) shm_free(_c->user_agent.s);
	if (_c->callid.s) shm_free(_c->callid.s);
	if (_c->c.s) shm_free(_c->c.s);
	if (_c->ruid.s) shm_free(_c->ruid.s);
	if (_c->instance.s) shm_free(_c->instance.s);
	shm_free( _c );
}


/*!
 * \brief Print contact, for debugging purposes only
 * \param _f output file
 * \param _c printed contact
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
	if (_c->expires == 0) {
		fprintf(_f, "Permanent\n");
	} else if (_c->expires == UL_EXPIRED_TIME) {
		fprintf(_f, "Deleted\n");
	} else if (t > _c->expires) {
		fprintf(_f, "Expired\n");
	} else {
		fprintf(_f, "%u\n", (unsigned int)(_c->expires - t));
	}
	fprintf(_f, "q         : %s\n", q2str(_c->q, 0));
	fprintf(_f, "Call-ID   : '%.*s'\n", _c->callid.len, ZSW(_c->callid.s));
	fprintf(_f, "CSeq      : %d\n", _c->cseq);
	fprintf(_f, "User-Agent: '%.*s'\n",
		_c->user_agent.len, ZSW(_c->user_agent.s));
	fprintf(_f, "received  : '%.*s'\n",
		_c->received.len, ZSW(_c->received.s));
	fprintf(_f, "Path      : '%.*s'\n",
		_c->path.len, ZSW(_c->path.s));
	fprintf(_f, "State     : %s\n", st);
	fprintf(_f, "Flags     : %u\n", _c->flags);
	if (_c->sock) {
		fprintf(_f, "Sock      : %.*s (%p)\n",
				_c->sock->sock_str.len,_c->sock->sock_str.s,_c->sock);
	} else {
		fprintf(_f, "Sock      : none (null)\n");
	}
	fprintf(_f, "Methods   : %u\n", _c->methods);
	fprintf(_f, "ruid      : '%.*s'\n",
		_c->ruid.len, ZSW(_c->ruid.s));
	fprintf(_f, "instance  : '%.*s'\n",
		_c->instance.len, ZSW(_c->instance.s));
	fprintf(_f, "reg-id    : %u\n", _c->reg_id);
	fprintf(_f, "next      : %p\n", _c->next);
	fprintf(_f, "prev      : %p\n", _c->prev);
	fprintf(_f, "~~~/Contact~~~~\n");
}


/*!
 * \brief Update existing contact in memory with new values
 * \param _c contact
 * \param _ci contact informations
 * \return 0 on success, -1 on failure
 */
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t* _ci)
{
#define update_str(_old,_new) \
	do{\
		if ((_old)->len < (_new)->len) { \
			ptr = (char*)shm_malloc((_new)->len); \
			if (ptr == 0) { \
				LM_ERR("no more shm memory\n"); \
				return -1; \
			}\
			memcpy(ptr, (_new)->s, (_new)->len);\
			if ((_old)->s) shm_free((_old)->s);\
			(_old)->s = ptr;\
		} else {\
			memcpy((_old)->s, (_new)->s, (_new)->len);\
		}\
		(_old)->len = (_new)->len;\
	} while(0)

	char* ptr;

	/* No need to update Callid as it is constant 
	 * per ucontact (set at insert time)  -bogdan */

	update_str( &_c->user_agent, _ci->user_agent);

	if (_ci->received.s && _ci->received.len) {
		update_str( &_c->received, &_ci->received);
	} else {
		if (_c->received.s) shm_free(_c->received.s);
		_c->received.s = 0;
		_c->received.len = 0;
	}
	
	if (_ci->path) {
		update_str( &_c->path, _ci->path);
	} else {
		if (_c->path.s) shm_free(_c->path.s);
		_c->path.s = 0;
		_c->path.len = 0;
	}

	_c->sock = _ci->sock;
	_c->expires = _ci->expires;
	_c->q = _ci->q;
	_c->cseq = _ci->cseq;
	_c->methods = _ci->methods;
	_c->last_modified = _ci->last_modified;
	_c->flags = _ci->flags;
	_c->cflags = _ci->cflags;

	return 0;
}


/* ================ State related functions =============== */

/*!
 * \brief Update state of the contact if we are using write-back scheme
 * \param _c updated contact
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
			 /* For db mode 1 & 2 a modified contact needs to be 
			  * updated also in the database, so transit into 
			  * CS_DIRTY and let the timer to do the update 
			  * again. For db mode 1 we try to update right
			  * now and if fails, let the timer to do the job
			  */
		if (db_mode == WRITE_BACK || db_mode == WRITE_THROUGH) {
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


/*!
 * \brief Update state of the contact
 * \param _c updated contact
 * \return 1 if the contact should be deleted from memory immediately, 0 otherwise
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
			_c->expires = UL_EXPIRED_TIME;
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


/*!
 * \brief Called when the timer is about to delete an expired contact
 * \param _c expired contact
 * \return 1 if the contact should be removed from the database and 0 otherwise
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


/*!
 * \brief Called when the timer is about flushing the contact, updates contact state
 * \param _c flushed contact
 * \return 1 if the contact should be inserted, 2 if update and 0 otherwise
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

/*!
 * \brief Insert contact into the database
 * \param _c inserted contact
 * \return 0 on success, -1 on failure
 */

int db_insert_ucontact(ucontact_t* _c)
{
	char* dom;
	db_key_t keys[18];
	db_val_t vals[18];
	int nr_cols = 0;

	int nr_cols_key = 0;
	struct udomain * _d;
	str user={0, 0};
	str domain={0, 0};

	if (_c->flags & FL_MEM) {
		return 0;
	}

	if(register_udomain(_c->domain->s, &_d) < 0){
		return -1;
	}
	LM_INFO("Domain set for contact %.*s\n", _c->domain->len, _c->domain->s);

	keys[nr_cols] = &user_col;
	vals[nr_cols].type = DB1_STR;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.str_val = *_c->aor;
	nr_cols++;

	keys[nr_cols] = &contact_col;
	vals[nr_cols].type = DB1_STR;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.str_val = _c->c;
	nr_cols++;

	if(use_domain) {
		keys[nr_cols] = &domain_col;
		vals[nr_cols].type = DB1_STR;
		vals[nr_cols].nul = 0;

		dom = memchr(_c->aor->s, '@', _c->aor->len);
		if (dom==0) {
			LM_INFO("*** use domain and AOR does not contain @\n");
			vals[nr_cols].val.str_val.len = 0;
			vals[nr_cols].val.str_val.s = 0;
		} else {
			vals[0].val.str_val.len = dom - _c->aor->s;
			vals[nr_cols].val.str_val.s = dom + 1;
			vals[nr_cols].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
		}
		domain = vals[nr_cols].val.str_val;
		LM_INFO("** Username=%.*s  Domain=%.*s\n", vals[0].val.str_val.len, vals[0].val.str_val.s,
				vals[nr_cols].val.str_val.len, vals[nr_cols].val.str_val.s);
		nr_cols++;
	}
	user = vals[0].val.str_val;

	keys[nr_cols] = &expires_col;
	vals[nr_cols].type = DB1_DATETIME;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.time_val = _c->expires;
	nr_cols++;

	keys[nr_cols] = &q_col;
	vals[nr_cols].type = DB1_DOUBLE;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.double_val = q2double(_c->q);
	nr_cols++;

	keys[nr_cols] = &callid_col;
	vals[nr_cols].type = DB1_STR;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.str_val = _c->callid;
	nr_cols++;

	keys[nr_cols] = &cseq_col;
	vals[nr_cols].type = DB1_INT;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.int_val = _c->cseq;
	nr_cols++;

	keys[nr_cols] = &flags_col;
	vals[nr_cols].type = DB1_INT;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.bitmap_val = _c->flags;
	nr_cols++;

	keys[nr_cols] = &cflags_col;
	vals[nr_cols].type = DB1_INT;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.bitmap_val = _c->cflags;
	nr_cols++;

	keys[nr_cols] = &user_agent_col;
	vals[nr_cols].type = DB1_STR;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.str_val = _c->user_agent;
	nr_cols++;

	keys[nr_cols] = &received_col;
	vals[nr_cols].type = DB1_STR;
	if (_c->received.s == 0) {
		vals[nr_cols].nul = 1;
	} else {
		vals[nr_cols].nul = 0;
		vals[nr_cols].val.str_val = _c->received;
	}
	nr_cols++;

	keys[nr_cols] = &path_col;
	vals[nr_cols].type = DB1_STR;
	if (_c->path.s == 0) {
		vals[nr_cols].nul = 1;
	} else {
		vals[nr_cols].nul = 0;
		vals[nr_cols].val.str_val = _c->path;
	}
	nr_cols++;

	keys[nr_cols] = &sock_col;
	vals[nr_cols].type = DB1_STR;
	if (_c->sock) {
		vals[nr_cols].val.str_val = _c->sock->sock_str;
		vals[nr_cols].nul = 0;
	} else {
		vals[nr_cols].nul = 1;
	}
	nr_cols++;

	keys[nr_cols] = &methods_col;
	vals[nr_cols].type = DB1_BITMAP;
	if (_c->methods == 0xFFFFFFFF) {
		vals[nr_cols].nul = 1;
	} else {
		vals[nr_cols].val.bitmap_val = _c->methods;
		vals[nr_cols].nul = 0;
	}
	nr_cols++;

	keys[nr_cols] = &last_mod_col;
	vals[nr_cols].type = DB1_DATETIME;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.time_val = _c->last_modified;
	nr_cols++;

	keys[nr_cols] = &ruid_col;
	if(_c->ruid.len>0)
	{
		vals[nr_cols].type = DB1_STR;
		vals[nr_cols].nul = 0;
		vals[nr_cols].val.str_val = _c->ruid;
	} else {
		vals[nr_cols].nul = 1;
	}
	nr_cols++;

	keys[nr_cols] = &instance_col;
	if(_c->instance.len>0)
	{
		vals[nr_cols].type = DB1_STR;
		vals[nr_cols].nul = 0;
		vals[nr_cols].val.str_val = _c->instance;
	} else {
		vals[nr_cols].nul = 1;
	}
	nr_cols++;

	keys[nr_cols] = &reg_id_col;
	vals[nr_cols].type = DB1_INT;
	vals[nr_cols].nul = 0;
	vals[nr_cols].val.int_val = (int)_c->reg_id;
	nr_cols++;

	nr_cols_key  = nr_cols;
	/* to prevent errors from the DB because of duplicated entries */
	
	if (ul_db_layer_replace(_d, &user, &domain, keys, vals, nr_cols, nr_cols_key) <0) {
		LM_ERR("inserting contact in db failed\n");
		return -1;
	}

	return 0;
}


/*!
 * \brief Update contact in the database
 * \param _c updated contact
 * \return 0 on success, -1 on failure
 */
int db_update_ucontact(ucontact_t* _c)
{
	char* dom;
	db_key_t keys1[4];
	db_val_t vals1[4];

	db_key_t keys2[14];
	db_val_t vals2[14];

	if (_c->flags & FL_MEM) {
		return 0;
	}
	struct udomain * _d;
	if(register_udomain(_c->domain->s, &_d) < 0){
		return -1;
	}

	keys1[0] = &user_col;
	keys1[1] = &contact_col;
	keys1[2] = &callid_col;
	keys1[3] = &domain_col;
	keys2[0] = &expires_col;
	keys2[1] = &q_col;
	keys2[2] = &cseq_col;
	keys2[3] = &flags_col;
	keys2[4] = &cflags_col;
	keys2[5] = &user_agent_col;
	keys2[6] = &received_col;
	keys2[7] = &path_col;
	keys2[8] = &sock_col;
	keys2[9] = &methods_col;
	keys2[10] = &last_mod_col;
	keys2[11] = &ruid_col;
	keys2[12] = &instance_col;
	keys2[13] = &reg_id_col;

	vals1[0].type = DB1_STR;
	vals1[0].nul = 0;
	vals1[0].val.str_val = *_c->aor;

	vals1[1].type = DB1_STR;
	vals1[1].nul = 0;
	vals1[1].val.str_val = _c->c;

	vals1[2].type = DB1_STR;
	vals1[2].nul = 0;
	vals1[2].val.str_val = _c->callid;

	vals2[0].type = DB1_DATETIME;
	vals2[0].nul = 0;
	vals2[0].val.time_val = _c->expires;

	vals2[1].type = DB1_DOUBLE;
	vals2[1].nul = 0;
	vals2[1].val.double_val = q2double(_c->q);

	vals2[2].type = DB1_INT;
	vals2[2].nul = 0;
	vals2[2].val.int_val = _c->cseq;

	vals2[3].type = DB1_INT;
	vals2[3].nul = 0;
	vals2[3].val.bitmap_val = _c->flags;

	vals2[4].type = DB1_INT;
	vals2[4].nul = 0;
	vals2[4].val.bitmap_val = _c->cflags;

	vals2[5].type = DB1_STR;
	vals2[5].nul = 0;
	vals2[5].val.str_val = _c->user_agent;

	vals2[6].type = DB1_STR;
	if (_c->received.s == 0) {
		vals2[6].nul = 1;
	} else {
		vals2[6].nul = 0;
		vals2[6].val.str_val = _c->received;
	}
	
	vals2[7].type = DB1_STR;
	if (_c->path.s == 0) {
		vals2[7].nul = 1;
	} else {
		vals2[7].nul = 0;
		vals2[7].val.str_val = _c->path;
	}

	vals2[8].type = DB1_STR;
	if (_c->sock) {
		vals2[8].val.str_val = _c->sock->sock_str;
		vals2[8].nul = 0;
	} else {
		vals2[8].nul = 1;
	}

	vals2[9].type = DB1_BITMAP;
	if (_c->methods == 0xFFFFFFFF) {
		vals2[9].nul = 1;
	} else {
		vals2[9].val.bitmap_val = _c->methods;
		vals2[9].nul = 0;
	}

	vals2[10].type = DB1_DATETIME;
	vals2[10].nul = 0;
	vals2[10].val.time_val = _c->last_modified;

	if(_c->ruid.len>0)
	{
		vals2[11].type = DB1_STR;
		vals2[11].nul = 0;
		vals2[11].val.str_val = _c->ruid;
	} else {
		vals2[11].nul = 1;
	}

	if(_c->instance.len>0)
	{
		vals2[12].type = DB1_STR;
		vals2[12].nul = 0;
		vals2[12].val.str_val = _c->instance;
	} else {
		vals2[12].nul = 1;
	}

	vals2[13].type = DB1_INT;
	vals2[13].nul = 0;
	vals2[13].val.int_val = (int)_c->reg_id;

	if (use_domain) {
		vals1[3].type = DB1_STR;
		vals1[3].nul = 0;
		dom = memchr(_c->aor->s, '@', _c->aor->len);
		if (dom==0) {
			vals1[0].val.str_val.len = 0;
			vals1[3].val.str_val = *_c->aor;
		} else {
			vals1[0].val.str_val.len = dom - _c->aor->s;
			vals1[3].val.str_val.s = dom + 1;
			vals1[3].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
		}
	}

	if (ul_db_layer_update(_d, &vals1[0].val.str_val, &vals1[3].val.str_val, keys1, 0, vals1, keys2, vals2, 
	(use_domain) ? (4) : (3), 14) < 0) {
		LM_ERR("updating database failed\n");
		return -1;
	}

	return 0;
}


/*!
 * \brief Delete contact from the database
 * \param _c deleted contact
 * \return 0 on success, -1 on failure
 */
int db_delete_ucontact(ucontact_t* _c)
{
	char* dom;
	db_key_t keys[4];
	db_val_t vals[4];

	if (_c->flags & FL_MEM) {
		return 0;
	}
	struct udomain * _d;
	if(register_udomain(_c->domain->s, &_d) < 0){
		return -1;
	}

	keys[0] = &user_col;
	keys[1] = &contact_col;
	keys[2] = &callid_col;
	keys[3] = &domain_col;

	vals[0].type = DB1_STR;
	vals[0].nul = 0;
	vals[0].val.str_val = *_c->aor;

	vals[1].type = DB1_STR;
	vals[1].nul = 0;
	vals[1].val.str_val = _c->c;

	vals[2].type = DB1_STR;
	vals[2].nul = 0;
	vals[2].val.str_val = _c->callid;

	if (use_domain) {
		vals[3].type = DB1_STR;
		vals[3].nul = 0;
		dom = memchr(_c->aor->s, '@', _c->aor->len);
		if (dom==0) {
			vals[0].val.str_val.len = 0;
			vals[3].val.str_val = *_c->aor;
		} else {
			vals[0].val.str_val.len = dom - _c->aor->s;
			vals[3].val.str_val.s = dom + 1;
			vals[3].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
		}
	}

	if (ul_db_layer_delete(_d, &vals[0].val.str_val, &vals[3].val.str_val, keys, 0, vals, (use_domain) ? (4) : (3)) < 0) {
		LM_ERR("deleting from database failed\n");
		return -1;
	}

	return 0;
}


/*!
 * \brief Remove a contact from list belonging to a certain record
 * \param _r record the contact belongs
 * \param _c removed contact
 */
static inline void unlink_contact(struct urecord* _r, ucontact_t* _c)
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


/*!
 * \brief Insert a new contact into the list at the correct position
 * \param _r record that holds the sorted contacts
 * \param _c new contact
 */
static inline void update_contact_pos(struct urecord* _r, ucontact_t* _c)
{
	ucontact_t *pos, *ppos;

	if (desc_time_order) {
		/* order by time - first the newest */
		if (_c->prev==0)
			return;
		unlink_contact(_r, _c);
		/* insert it at the beginning */
		_c->next = _r->contacts;
		_c->prev = 0;
		_r->contacts->prev = _c;
		_r->contacts = _c;
	} else {
		/* order by q - first the smaller q */
		if ( (_c->prev==0 || _c->q<=_c->prev->q)
		&& (_c->next==0 || _c->q>=_c->next->q)  )
			return;
		/* need to move , but where? */
		unlink_contact(_r, _c);
		_c->next = _c->prev = 0;
		for(pos=_r->contacts,ppos=0;pos&&pos->q<_c->q;ppos=pos,pos=pos->next);
		if (pos) {
			if (!pos->prev) {
				pos->prev = _c;
				_c->next = pos;
				_r->contacts = _c;
			} else {
				_c->next = pos;
				_c->prev = pos->prev;
				pos->prev->next = _c;
				pos->prev = _c;
			}
		} else if (ppos) {
			ppos->next = _c;
			_c->prev = ppos;
		} else {
			_r->contacts = _c;
		}
	}
}


/*!
 * \brief Update ucontact with new values
 * \param _r record the contact belongs to
 * \param _c updated contact
 * \param _ci new contact informations
 * \return 0 on success, -1 on failure
 */
int update_ucontact(struct urecord* _r, ucontact_t* _c, ucontact_info_t* _ci)
{
	/* we have to update memory in any case, but database directly
	 * only in db_mode 1 */
	if (mem_update_ucontact( _c, _ci) < 0) {
		LM_ERR("failed to update memory\n");
		return -1;
	}

	/* run callbacks for UPDATE event */
	if (exists_ulcb_type(UL_CONTACT_UPDATE))
	{
		LM_DBG("exists callback for type= UL_CONTACT_UPDATE\n");
		run_ul_callbacks( UL_CONTACT_UPDATE, _c);
	}

	if (_r && db_mode!=DB_ONLY)
		update_contact_pos( _r, _c);

	st_update_ucontact(_c);

	if (db_mode == WRITE_THROUGH || db_mode==DB_ONLY) {
		/*
		 * prevent problems when we're in a failover situation: the first DB contains
		 * the complete location entries, the other misses some of them. Before the
		 * update it checks for a entry in the first DB, this is ok. But the update
		 * in the second DB will not work. Thus the expire mechanism don't work, it
		 * takes too long until both DBs have the same number of entries again.
		 */
		if (db_insert_ucontact(_c) < 0) {
			LM_ERR("failed to insert_update database\n");
			return -1;
		} else {
			_c->state = CS_SYNC;
		}
	}
	return 0;
}
