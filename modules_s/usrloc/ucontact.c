/* 
 * $Id$ 
 *
 * Usrloc contact structure
 */


#include "ucontact.h"
#include <string.h>             /* memcpy */
#include <stdio.h>              /* printf */
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "ul_mod.h"


/*
 * Create a new contact structure
 */
int new_ucontact(str* _aor, str* _contact, time_t _e, float _q,
		 str* _callid, int _cseq, ucontact_t** _c)
{
	*_c = (ucontact_t*)shm_malloc(sizeof(ucontact_t));
	if (!(*_c)) {
	        LOG(L_ERR, "new_ucontact(): No memory left\n");
		return -1;
	}

	(*_c)->aor = _aor;

	(*_c)->c.s = (char*)shm_malloc(_contact->len);
	if ((*_c)->c.s == 0) {
		LOG(L_ERR, "new_ucontact(): No memory left 2\n");
		shm_free(*_c);
		return -2;
	}
	memcpy((*_c)->c.s, _contact->s, _contact->len);
	(*_c)->c.len = _contact->len;

	(*_c)->expires = _e;
	(*_c)->q = _q;

	(*_c)->callid.s = (char*)shm_malloc(_callid->len);
	if ((*_c)->callid.s == 0) {
		LOG(L_ERR, "new_ucontact(): No memory left 4\n");
		shm_free((*_c)->c.s);
		shm_free(*_c);
		return -4;
	}
	memcpy((*_c)->callid.s, _callid->s, _callid->len);
	(*_c)->callid.len = _callid->len;

	(*_c)->cseq = _cseq;
	(*_c)->next = 0;
	(*_c)->prev = 0;
	
	return 0;
}	


/*
 * Free all memory associated with given contact structure
 */
void free_ucontact(ucontact_t* _c)
{
		shm_free(_c->callid.s);
		shm_free(_c->c.s);
		shm_free(_c);
}


/*
 * Print contact, for debugging purposes only
 */
void print_ucontact(ucontact_t* _c)
{
	time_t t = time(0);

	printf("~~~Contact(%p)~~~\n", _c);
	printf("domain : \'%.*s\'\n", _c->domain->len, _c->domain->s);
	printf("aor    : \'%.*s\'\n", _c->aor->len, _c->aor->s);
	printf("Contact: \'%.*s\'\n", _c->c.len, _c->c.s);
	printf("Expires: %d\n", (unsigned int)(_c->expires) - t);
	printf("q      : %10.2f\n", _c->q);
	printf("Call-ID: \'%.*s\'\n", _c->callid.len, _c->callid.s);
	printf("CSeq   : %d\n", _c->cseq);
	printf("next   : %p\n", _c->next);
	printf("prev   : %p\n", _c->prev);
	printf("~~~/Contact~~~~\n");
}


/*
 * Update existing contact with new values
 */
int update_ucontact(ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs)
{
	char* ptr;

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

	_c->expires = _e;
	_c->q = _q;
	_c->cseq = _cs;

	if (use_db) {
		if (write_through) {
			if (db_upd_ucontact(_c) < 0) {
				LOG(L_ERR, "update_ucontact(): Error while updating database\n");
			}
		} else { /* write back */

		}
	}

	return 0;
}


/*
 * Delete contact from the database
 */
int db_del_ucontact(ucontact_t* _c)
{
	char b[256];
	db_key_t keys[2] = {user_col, contact_col};
	db_val_t vals[2] = {{DB_STR, 0, {.str_val = {_c->aor->s, _c->aor->len}}},
			    {DB_STR, 0, {.str_val = {_c->c.s, _c->c.len}}}
	};

	     /* FIXME */
	memcpy(b, _c->domain->s, _c->domain->len);
	b[_c->domain->len] = '\0';
	db_use_table(db, b);

	if (db_delete(db, keys, vals, 2) < 0) {
		LOG(L_ERR, "db_del_ucontact(): Error while deleting from database\n");
		return -1;
	}

	return 0;
}


/*
 * Update contact in the database
 */
int db_upd_ucontact(ucontact_t* _c)
{
	char b[256];
	db_key_t keys1[2] = {user_col, contact_col};
	db_val_t vals1[2] = {{DB_STR, 0, {.str_val = {_c->aor->s, _c->aor->len}}},
			     {DB_STR, 0, {.str_val = {_c->c.s, _c->c.len}}}
	};

	db_key_t keys2[4] = {expires_col, q_col, callid_col, cseq_col};
	db_val_t vals2[4] = {{DB_DATETIME, 0, {.time_val = _c->expires}},
			     {DB_DOUBLE,   0, {.double_val = _c->q}},
			     {DB_STR,      0, {.str_val = {_c->callid.s, _c->callid.len}}},
			     {DB_INT,      0, {.int_val = _c->cseq}}
	};

	     /* FIXME */
	memcpy(b, _c->domain->s, _c->domain->len);
	b[_c->domain->len] = '\0';
	db_use_table(db, b);

	if (db_update(db, keys1, vals1, keys2, vals2, 2, 4) < 0) {
		LOG(L_ERR, "db_upd_ucontact(): Error while updating database\n");
		return -1;
	}

	return 0;
}


/*
 * Insert contact into the database
 */
int db_ins_ucontact(ucontact_t* _c)
{
	char b[256];
	db_key_t keys[] = {user_col, contact_col, expires_col, q_col, callid_col, cseq_col};
	db_val_t vals[] = {{DB_STR,     0, {.str_val = {_c->aor->s, _c->aor->len}}},
			  {DB_STR,      0, {.str_val = {_c->c.s, _c->c.len}}},
			  {DB_DATETIME, 0, {.time_val = _c->expires}},
			  {DB_DOUBLE,   0, {.double_val = _c->q}},
			  {DB_STR,      0, {.str_val = {_c->callid.s, _c->callid.len}}},
			  {DB_INT,      0, {.int_val = _c->cseq}}
	};

	     /* FIXME */
	memcpy(b, _c->domain->s, _c->domain->len);
	b[_c->domain->len] = '\0';
	db_use_table(db, b);

	if (db_insert(db, keys, vals, 6) < 0) {
		LOG(L_ERR, "db_ins_ucontact(): Error while inserting contact\n");
		return -1;
	}

	return 0;
}
