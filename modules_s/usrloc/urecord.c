/* 
 * $Id$ 
 *
 * Usrloc record structure
 */


#include "urecord.h"
#include <stdio.h>
#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../fastlock.h"
#include "ul_mod.h"
#include "utime.h"


/*
 * Create and initialize new record structure
 */
int new_urecord(str* _aor, urecord_t** _r)
{
	*_r = (urecord_t*)shm_malloc(sizeof(urecord_t));
	if (*_r == 0) {
		LOG(L_ERR, "new_urecord(): No memory left\n");
		return -1;
	}
	memset(*_r, 0, sizeof(urecord_t));

	(*_r)->aor.s = (char*)shm_malloc(_aor->len);
	if ((*_r)->aor.s == 0) {
		LOG(L_ERR, "new_urecord(): No memory left 2\n");
		shm_free(*_r);
		return -2;
	}
	memcpy((*_r)->aor.s, _aor->s, _aor->len);
	(*_r)->aor.len = _aor->len;
	(*_r)->domain = 0;
	return 0;
}


/*
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
	
	if (_r->aor.s) shm_free(_r->aor.s);
	shm_free(_r);
}


/*
 * Print a record
 */
void print_urecord(urecord_t* _r)
{
	ucontact_t* ptr;

	printf("...Record(%p)...\n", _r);
	printf("domain: \'%.*s\'\n", _r->domain->len, _r->domain->s);
	printf("aor   : \'%.*s\'\n", _r->aor.len, _r->aor.s);
	
	if (_r->contacts) {
		ptr = _r->contacts;
		while(ptr) {
			print_ucontact(ptr);
			ptr = ptr->next;
		}
	}

	printf(".../Record...\n");
}


/*
 * Add a new contact
 * Contacts are ordered by: 1) q 
 *                          2) descending modification time
 */
int insert_ucontact(urecord_t* _r, str* _c, time_t _e, float _q, str* _cid, int _cs)
{
	ucontact_t* c, *ptr, *prev = 0;

	if (new_ucontact(&_r->aor, _c, _e, _q, _cid, _cs, &c) < 0) {
		LOG(L_ERR, "insert_ucontact(): Can't create new contact\n");
		return -1;
	}
	

	ptr = _r->contacts;
	while(ptr) {
		if (ptr->q < _q) break;
		prev = ptr;
		ptr = ptr->next;
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

	c->domain = _r->domain;

	if (use_db) {
		if (write_through) {
			if (db_ins_ucontact(c) < 0) {
				LOG(L_ERR, "insert_ucontact(): Error while inserting in database\n");
			}
		} else { /* write back */

		}
	}

	return 0;
}


/*
 * Remove contact from the list
 */
int delete_ucontact(urecord_t* _r, ucontact_t* _c)
{
	if (use_db) {
		if (write_through) {
			if (db_del_ucontact(_c) < 0) {
				LOG(L_ERR, "delete_ucontact(): Error while deleting from database\n");
			}
		} else { /* write back */
		}
	}
	
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

	_c->domain = 0;
	free_ucontact(_c);
	return 0;
}


/*
 * Find a contact
 */
int get_ucontact(urecord_t* _r, str* _c, ucontact_t** _co)
{
	ucontact_t* ptr;

	ptr = _r->contacts;
	while(ptr) {
		if ((_c->len == ptr->c.len) &&
		    !memcmp(_c->s, ptr->c.s, _c->len)) {
			*_co = ptr;
			return 0;
		}
		
		ptr = ptr->next;
	}
	return 1;
}


int timer_urecord(urecord_t* _r)
{
	ucontact_t* ptr, *t;

	ptr = _r->contacts;

	while(ptr) {
		if (ptr->expires < act_time) {
			LOG(L_NOTICE, "Binding '\%.*s\',\'%.*s\' has expired\n",
			    ptr->aor->len, ptr->aor->s,
			    ptr->c.len, ptr->c.s);
			
			t = ptr;
			ptr = ptr->next;

			delete_ucontact(_r, t);
		} else {
			LOG(L_NOTICE, "Binding \'%.*s\',\'%.*s\' is fresh: %d\n", ptr->aor->len, ptr->aor->s,
				ptr->c.len, ptr->c.s, ptr->expires - act_time);
			ptr = ptr->next;
		}
	}

	return 0;
}


int db_del_urecord(urecord_t* _r)
{
	char b[256];
	db_key_t keys[1] = {user_col};
	db_val_t vals[1] = {{DB_STR, 0, {.str_val = {_r->aor.s, _r->aor.len}}}};

	     /* FIXME */
	memcpy(b, _r->domain->s, _r->domain->len);
	b[_r->domain->len] = '\0';
	db_use_table(db, b);

	if (db_delete(db, keys, vals, 1) < 0) {
		LOG(L_ERR, "db_del_urecord(): Error while deleting from database\n");
		return -1;
	}

	return 0;
}
