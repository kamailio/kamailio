/* 
 * $Id$ 
 *
 * Usrloc contact structure
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * ---------
 * 2003-03-12 added replication mark and three zombie states (nils)
 * 2004-03-17 generic callbacks added (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 * 2005-02-25 incoming socket is saved in ucontact record (bogdan)
 */



#include "ucontact.h"
#include <string.h>             /* memcpy */
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../lib/srdb2/db.h"
#include "../../ip_addr.h"
#include "../../globals.h"
#include "../../ser_time.h"
#include "ul_mod.h"
#include "ul_callback.h"
#include "reg_avps.h"
#include "reg_avps_db.h"


/*
 * Create a new contact structure
 */
int new_ucontact(str* _dom, str* _uid, str* aor, str* _contact, time_t _e, qvalue_t _q,
				 str* _callid, int _cseq, unsigned int _flags, 
				 ucontact_t** _c, str* _ua, str* _recv, struct socket_info* sock,
				 str* _inst, int sid)
{
	*_c = (ucontact_t*)shm_malloc(sizeof(ucontact_t));
	if (!(*_c)) {
	        LOG(L_ERR, "new_ucontact(): No memory left\n");
		return -1;
	}
	memset(*_c, 0, sizeof(ucontact_t));

	(*_c)->domain = _dom;
	(*_c)->uid = _uid;

	(*_c)->aor.s = (char*)shm_malloc(aor->len);
	if ((*_c)->aor.s == 0) {
		LOG(L_ERR, "new_ucontact(): No memory left\n");
		goto error;
	}
	memcpy((*_c)->aor.s, aor->s, aor->len);
	(*_c)->aor.len = aor->len;

	(*_c)->c.s = (char*)shm_malloc(_contact->len);
	if ((*_c)->c.s == 0) {
		LOG(L_ERR, "new_ucontact(): No memory left 2\n");
		goto error;
	}
	memcpy((*_c)->c.s, _contact->s, _contact->len);
	(*_c)->c.len = _contact->len;

	(*_c)->expires = _e;
	(*_c)->q = _q;

	(*_c)->callid.s = (char*)shm_malloc(_callid->len);
	if ((*_c)->callid.s == 0) {
		LOG(L_ERR, "new_ucontact(): No memory left 4\n");
		goto error;
	}
	memcpy((*_c)->callid.s, _callid->s, _callid->len);
	(*_c)->callid.len = _callid->len;

	(*_c)->user_agent.s = (char*)shm_malloc(_ua->len);
	if ((*_c)->user_agent.s == 0) {
		LOG(L_ERR, "new_ucontact(): No memory left 8\n");
		goto error;
	}
	memcpy((*_c)->user_agent.s, _ua->s, _ua->len);
	(*_c)->user_agent.len = _ua->len;

	if (_recv) {
		(*_c)->received.s = (char*)shm_malloc(_recv->len);
		if ((*_c)->received.s == 0) {
			LOG(L_ERR, "new_ucontact(): No memory left\n");
			goto error;
		}
		memcpy((*_c)->received.s, _recv->s, _recv->len);
		(*_c)->received.len = _recv->len;
	} else {
		(*_c)->received.s = 0;
		(*_c)->received.len = 0;
	}

	if(_inst && _inst->len > 0) {
		(*_c)->instance.s = (char*)shm_malloc(_inst->len);
		if ((*_c)->instance.s == 0) {
			LOG(L_ERR, "new_ucontact(): No memory left\n");
			goto error;
		}
		memcpy((*_c)->instance.s, _inst->s, _inst->len);
		(*_c)->instance.len = _inst->len;
	} else {
		(*_c)->instance.s = 0;
		(*_c)->instance.len = 0;
	}

	(*_c)->server_id = sid;
	(*_c)->cseq = _cseq;
	(*_c)->state = CS_NEW;
	(*_c)->flags = _flags;
	(*_c)->sock = sock;
	return 0;

error:
	if (*_c) {
		if ((*_c)->instance.s) shm_free((*_c)->instance.s);
		if ((*_c)->received.s) shm_free((*_c)->received.s);
 		if ((*_c)->user_agent.s) shm_free((*_c)->user_agent.s);
 		if ((*_c)->callid.s) shm_free((*_c)->callid.s);
 		if ((*_c)->c.s) shm_free((*_c)->c.s);
		if ((*_c)->aor.s) shm_free((*_c)->aor.s);
 		shm_free(*_c);
	}
	return -1;	
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
	if (_c->aor.s) shm_free(_c->aor.s);
	if (_c->instance.s) shm_free(_c->instance.s);
	shm_free(_c);
}


/*
 * Print contact, for debugging purposes only
 */
void print_ucontact(FILE* _f, ucontact_t* _c)
{
	time_t t = ser_time(0);
	char* st;

	switch(_c->state) {
	case CS_NEW:   st = "CS_NEW";     break;
	case CS_SYNC:  st = "CS_SYNC";    break;
	case CS_DIRTY: st = "CS_DIRTY";   break;
	default:       st = "CS_UNKNOWN"; break;
	}

	fprintf(_f, "~~~Contact(%p)~~~\n", _c);
	fprintf(_f, "domain    : '%.*s'\n", _c->domain->len, ZSW(_c->domain->s));
	fprintf(_f, "uid       : '%.*s'\n", _c->uid->len, ZSW(_c->uid->s));
	fprintf(_f, "aor       : '%.*s'\n", _c->aor.len, ZSW(_c->aor.s));
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
	fprintf(_f, "User-Agent: '%.*s'\n", _c->user_agent.len, ZSW(_c->user_agent.s));
	fprintf(_f, "received  : '%.*s'\n", _c->received.len, ZSW(_c->received.s));
	fprintf(_f, "instance  : '%.*s'\n", _c->instance.len, ZSW(_c->instance.s));
	fprintf(_f, "State     : %s\n", st);
	fprintf(_f, "Flags     : %u\n", _c->flags);
	fprintf(_f, "server_id : %d\n", _c->server_id);
	fprintf(_f, "Sock      : %p\n", _c->sock);
	fprintf(_f, "next      : %p\n", _c->next);
	fprintf(_f, "prev      : %p\n", _c->prev);
	fprintf(_f, "~~~/Contact~~~~\n");
}


/*
 * Update ucontact structure in memory
 */
int mem_update_ucontact(ucontact_t* _c, str* _u, str* aor, time_t _e,
						qvalue_t _q, str* _cid, int _cs,
						unsigned int _set, unsigned int _reset,
						str* _ua, str* _recv,
						struct socket_info* sock, str* _inst)
{
	char* ptr;
	
	if (_c->aor.len < aor->len) {
		ptr = (char*)shm_malloc(aor->len);
		if (ptr == 0) {
			LOG(L_ERR, "update_ucontact(): No memory left\n");
			return -1;
		}
		memcpy(ptr, aor->s, aor->len);
		shm_free(_c->aor.s);
		_c->aor.s = ptr;
	} else {
		memcpy(_c->aor.s, aor->s, aor->len);
	}
	_c->aor.len = aor->len;

	if (_c->c.len < _u->len) {
		ptr = (char*)shm_malloc(_u->len);
		if (ptr == 0) {
			LOG(L_ERR, "update_ucontact(): No memory left\n");
			return -1;
		}
		memcpy(ptr, _u->s, _u->len);
		shm_free(_c->c.s);
		_c->c.s = ptr;
	} else {
		memcpy(_c->c.s, _u->s, _u->len);
	}
	_c->c.len = _u->len;

	if (_c->callid.len < _cid->len) {
		ptr = (char*)shm_malloc(_cid->len);
		if (ptr == 0) {
			LOG(L_ERR, "update_ucontact(): No memory left\n");
			return -1;
		}
		
		memcpy(ptr, _cid->s, _cid->len);
		shm_free(_c->callid.s);
		_c->callid.s = ptr;
	} else {
		memcpy(_c->callid.s, _cid->s, _cid->len);
	}
	_c->callid.len = _cid->len;

	if (_c->user_agent.len < _ua->len) {
		ptr = (char*)shm_malloc(_ua->len);
		if (ptr == 0) {
			LOG(L_ERR, "update_ucontact(): No memory left\n");
			return -1;
		}

		memcpy(ptr, _ua->s, _ua->len);
		shm_free(_c->user_agent.s);
		_c->user_agent.s = ptr;
	} else {
		memcpy(_c->user_agent.s, _ua->s, _ua->len);
	}
	_c->user_agent.len = _ua->len;

	if (_recv) {
		if (_c->received.len < _recv->len) {
			ptr = (char*)shm_malloc(_recv->len);
			if (ptr == 0) {
				LOG(L_ERR, "update_ucontact(): No memory left\n");
				return -1;
			}
			
			memcpy(ptr, _recv->s, _recv->len);
			if (_c->received.s) shm_free(_c->received.s);
			_c->received.s = ptr;
			/* fixme: The buffer could be in fact bigger than the value
			 * of len and we would then free it and allocate a new one.
			 */
			_c->received.len = _recv->len;
		} else {
			memcpy(_c->received.s, _recv->s, _recv->len);
		}
		_c->received.len = _recv->len;
	} else {
		if (_c->received.s) shm_free(_c->received.s);
		_c->received.s = 0;
		_c->received.len = 0;
	}

	if (_inst) {
		if (_c->instance.len < _inst->len) {
			ptr = (char *)shm_malloc(_inst->len);
			if (ptr == 0) {
				LOG(L_ERR, "update_ucontact(): No memory left\n");
				return -1;
			}
			memcpy(ptr, _inst->s, _inst->len);
			if (_c->instance.s)
				shm_free(_c->instance.s);
			_c->instance.s = ptr;
		} else {
			memcpy(_c->instance.s, _inst->s, _inst->len);
		}
		_c->instance.len = _inst->len;
	} else {
		if (_c->instance.s)
			shm_free(_c->instance.s);
		_c->instance.s = 0;
		_c->instance.len = 0;
	}

	_c->expires = _e;
	_c->q = _q;
	_c->cseq = _cs;
	_c->flags |= _set;
	_c->flags &= ~_reset;
	_c->sock = sock;

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
			     /* WRITE_THROUGH, READONLY or NO_DB -- we can
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
		if (db_skip_delete) return 0;
		else return 1;
	}

	return 0; /* Makes gcc happy */
}


/*
 * Called when the timer is about flushing the contact,
 * updates contact state and returns 1 if the contact
 * should be inserted, 2 if update , 3 if delete 
 * from memory, 4 if delete from database and 0 otherwise
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
int db_store_ucontact(ucontact_t* _c)
{
	str avps;
	
	if (_c->flags & FL_MEM) {
		return 0;
	}

	avps.s = NULL;

	ins_contact[cur_cmd]->vals[0].v.lstr = *_c->uid;
	ins_contact[cur_cmd]->vals[1].v.lstr.s = _c->c.s;
	ins_contact[cur_cmd]->vals[1].v.lstr.len = MIN(_c->c.len, 255);
	ins_contact[cur_cmd]->vals[2].v.time = _c->expires;
	ins_contact[cur_cmd]->vals[3].v.flt = (float)q2double(_c->q);
	ins_contact[cur_cmd]->vals[4].v.lstr.s = _c->callid.s;
	ins_contact[cur_cmd]->vals[4].v.lstr.len = MIN(_c->callid.len, 255);
	ins_contact[cur_cmd]->vals[5].v.int4 = _c->cseq;
	ins_contact[cur_cmd]->vals[6].v.bitmap = _c->flags;
	ins_contact[cur_cmd]->vals[7].v.lstr.s = _c->user_agent.s;
	ins_contact[cur_cmd]->vals[7].v.lstr.len = MIN(_c->user_agent.len, 64);

	if (_c->received.s == 0) {
		ins_contact[cur_cmd]->vals[8].flags |= DB_NULL;
	} else {
		ins_contact[cur_cmd]->vals[8].flags &= ~DB_NULL;
		ins_contact[cur_cmd]->vals[8].v.lstr = _c->received;
	}

	if (_c->instance.s == 0) {
		ins_contact[cur_cmd]->vals[9].flags |= DB_NULL;
	} else {
		ins_contact[cur_cmd]->vals[9].flags &= ~DB_NULL;
		ins_contact[cur_cmd]->vals[9].v.lstr = _c->instance;
	}

	ins_contact[cur_cmd]->vals[10].v.lstr.s = _c->aor.s;
	ins_contact[cur_cmd]->vals[10].v.lstr.len = MIN(_c->aor.len, 255);

	ins_contact[cur_cmd]->vals[11].v.int4 = _c->server_id;

	if (use_reg_avps()) {
		if (serialize_avps(_c->avps, &avps) < 0) {
			ERR("Error while serializing AVPs\n");
			return -1;
		}
		ins_contact[cur_cmd]->vals[12].v.lstr = avps;
	}

	     /* FIXME */
	if (db_exec(NULL, ins_contact[cur_cmd]) < 0) {
		ERR("Error while storing contact in database\n");
		if (avps.s) pkg_free(avps.s);
		return -1;
	}

	if (avps.s) pkg_free(avps.s);	
	return 0;
}


/*
 * Delete contact from the database
 */
int db_delete_ucontact(ucontact_t* _c)
{
	if (_c->flags & FL_MEM) {
		return 0;
	}

	del_contact[cur_cmd]->match[0].v.lstr = *_c->uid;
	del_contact[cur_cmd]->match[1].v.lstr = _c->c;

	if (db_exec(NULL, del_contact[cur_cmd]) < 0) {
		ERR("Error while deleting contact from database\n");
		return -1;
	}

	return 0;
}


/*
 * Update ucontact with new values
 */
int update_ucontact(ucontact_t* _c, str* _u, str* aor, time_t _e,
					qvalue_t _q, str* _cid, int _cs,
					unsigned int _set, unsigned int _reset,
					str* _ua, str* _recv,
					struct socket_info* sock, str* _inst, int sid)
{
	/* run callbacks for UPDATE event */
	if (exists_ulcb_type(UL_CONTACT_UPDATE)) {
		run_ul_callbacks( UL_CONTACT_UPDATE, _c);
	}

	/* we have to update memory in any case, but database directly
	 * only in db_mode 1 */
	if (mem_update_ucontact(_c, _u, aor, _e, _q, _cid, _cs, _set, _reset,
							_ua, _recv, sock, _inst) < 0) {
		LOG(L_ERR, "update_ucontact(): Error while updating\n");
		return -1;
	}
	st_update_ucontact(_c);
	save_reg_avps(_c);
	if (db_mode == WRITE_THROUGH) {
		if (db_store_ucontact(_c) < 0) {
			LOG(L_ERR, "update_ucontact(): Error while updating database\n");
		}
	}
	return 0;
}
