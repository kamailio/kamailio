/* 
 * $Id$ 
 */

#include "udomain.h"
#include <stdio.h>
#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "ul_mod.h"            /* usrloc module parameters */


/*
 * Hash function
 */
static inline int hash_func(udomain_t* _d, char* _s, int _l)
{
	int res = 0, i;
	
	for(i = 0; i < _l; i++) {
		res += _s[i];
	}
	
	return res % _d->size;
}


/*
 * Add a record to list of all records in a domain
 */
static inline void udomain_add(udomain_t* _d, urecord_t* _r)
{
	if (_d->d_ll.n == 0) {
		_d->d_ll.first = _r;
		_d->d_ll.last = _r;
	} else {
		_r->d_ll.prev = _d->d_ll.last;
		_d->d_ll.last->d_ll.next = _r;
		_d->d_ll.last = _r;
	}
	_d->d_ll.n++;
}


/*
 * Remove a record from list of all records in a domain
 */
static inline void udomain_remove(udomain_t* _d, urecord_t* _r)
{
	if (_d->d_ll.n == 0) return;

	if (_r->d_ll.prev) {
		_r->d_ll.prev->d_ll.next = _r->d_ll.next;
	} else {
		_d->d_ll.first = _r->d_ll.next;
	}

	if (_r->d_ll.next) {
		_r->d_ll.next->d_ll.prev = _r->d_ll.prev;
	} else {
		_d->d_ll.last = _r->d_ll.prev;
	}

	_r->d_ll.prev = _r->d_ll.next = 0;
	_d->d_ll.n--;
}


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_udomain(str* _n, int _s, udomain_t** _d)
{
	int i;
	
	     /* Must be always in shared memory, since
	      * the cache is accessed from timer which
	      * lives in a separate process
	      */
	*_d = (udomain_t*)shm_malloc(sizeof(udomain_t));
	if (!(*_d)) {
		LOG(L_ERR, "new_udomain(): No memory left\n");
		return -1;
	}
	memset(*_d, 0, sizeof(udomain_t));
	
	(*_d)->table = (hslot_t*)shm_malloc(sizeof(hslot_t) * _s);
	if (!(*_d)->table) {
		LOG(L_ERR, "new_udomain(): No memory left 2\n");
		shm_free(*_d);
		return -2;
	}

	(*_d)->name = _n;
	
	for(i = 0; i < _s; i++) {
		if (init_slot(*_d, &((*_d)->table[i])) < 0) {
			LOG(L_ERR, "new_udomain(): Error while initializing hash table\n");
			shm_free((*_d)->table);
			shm_free(*_d);
			return -3;
		}
	}

	(*_d)->size = _s;
	init_lock((*_d)->lock);

	return 0;
}


/*
 * Free all memory allocated for
 * the domain
 */
void free_udomain(udomain_t* _d)
{
	int i;
	
	if (_d->table) {
		for(i = 0; i < _d->size; i++) {
			deinit_slot(_d->table + i);
		}
		shm_free(_d->table);
	}

        shm_free(_d);
}


/*
 * Just for debugging
 */
void print_udomain(udomain_t* _d)
{
	struct urecord* r;
	printf("---Domain---\n");
	printf("name : \'%.*s\'\n", _d->name->len, _d->name->s);
	printf("size : %d\n", _d->size);
	printf("table: %p\n", _d->table);
	printf("d_ll {\n");
	printf("    n    : %d\n", _d->d_ll.n);
	printf("    first: %p\n", _d->d_ll.first);
	printf("    last : %p\n", _d->d_ll.last);
	printf("}\n");
	printf("lock : %d\n", _d->lock);
	if (_d->d_ll.n > 0) {
		printf("\n");
		r = _d->d_ll.first;
		while(r) {
			print_urecord(r);
			r = r->d_ll.next;
		}


		printf("\n");
	}
	printf("---/Domain---\n");
}


static inline int ins(struct urecord* _r, str* _c, int _e, float _q, str* _cid, int _cs)
{
	ucontact_t* c, *ptr, *prev = 0;
	
	if (new_ucontact(&_r->aor, _c, _e, _q, _cid, _cs, &c) < 0) {
		LOG(L_ERR, "ins(): Can't create new contact\n");
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
	return 0;
}


/*
 * Load data from a database
 */
int preload_udomain(udomain_t* _d)
{
	char b[256];
	db_key_t columns[6] = {user_col, contact_col, expires_col, q_col, callid_col, cseq_col};
	db_res_t* res;
	db_row_t* row;
	int i, cseq;
	urecord_t* rec = 0;
	const char* user, *aor = 0;
        double q;
	time_t expires;
	str s, contact, callid;

	     /* FIXME */
	memcpy(b, _d->name->s, _d->name->len);
	b[_d->name->len] = '\0';
	db_use_table(db, b);
	if (db_query(db, 0, 0, columns, 0, 6, user_col, &res) < 0) {
		LOG(L_ERR, "preload_udomain(): Error while doing db_query\n");
		return -1;
	}
	
	if (!RES_ROW_N(res)) {
		DBG("preload_udomain(): Table is empty\n");
		db_free_query(db, res);
		return 0;
	}
	
	for(i = 0; i < RES_ROW_N(res); i++) {
		row = RES_ROWS(res) + i;
		user = VAL_STRING(ROW_VALUES(row));
		if (i == 0) aor = user;

		if (strcmp(user, aor) || !i) {
			DBG("preload_udomain(): Preloading contacts for username \'%s\'\n", user);
			aor = user;

			if (rec && (insert_urecord(_d, rec) < 0)) {
				LOG(L_ERR, "preload_udomain(): Error while inserting record\n");
				free_urecord(rec);
				db_free_query(db, res);
				free_urecord(rec);
				return -2;
			}

			s.s = (char*)user;
			s.len = strlen(user);

			if (new_urecord(&s, &rec) < 0) {
				LOG(L_ERR, "preload_udomain(): Can't create new record\n");
				db_free_query(db, res);
				return -3;
			}
			rec->domain = _d->name;
		}
		
		contact.s   = (char*)VAL_STRING(ROW_VALUES(row) + 1);
		contact.len = strlen(contact.s);
		expires     = VAL_TIME  (ROW_VALUES(row) + 2);
		q           = VAL_DOUBLE(ROW_VALUES(row) + 3);
		cseq        = VAL_INT   (ROW_VALUES(row) + 5);
		callid.s    = (char*)VAL_STRING(ROW_VALUES(row) + 4);
		callid.len  = strlen(callid.s);

		if (ins(rec, &contact, expires, q, &callid, cseq) < 0) {
			LOG(L_ERR, "preload_udomain(): Error while adding contact\n");
			db_free_query(db, res);
			free_urecord(rec);
			return -3;
		}
	}

	if (rec && (insert_urecord(_d, rec) < 0)) {
		LOG(L_ERR, "preload_udomain(): Error while inserting record 2\n");
		free_urecord(rec);
		db_free_query(db, res);
		return -4;
	}

	db_free_query(db, res);
	return 0;
}


/*
 * Insert a new record into domain
 */
int insert_urecord(udomain_t* _d, urecord_t* _r)
{
	int sl;

	sl = hash_func(_d, _r->aor.s, _r->aor.len);
	
	if (sl == -1) {
		LOG(L_ERR, "insert_urecord(): Error while hashing slot\n");
		return -1;
	}

	get_lock(&_d->lock);

	if (db) {  /* Update database */
		/*
	        if (db_insert_location(db, _l) < 0) {
			LOG(L_ERR, "insert_record(): Error while inserting bindings into database\n");
			free_element(ptr);
			release_lock(&_d->lock);
			return -3;
		}
		*/
	}

	slot_add(&_d->table[sl], _r);
	udomain_add(_d, _r);
	_r->domain = _d->name;
	release_lock(&_d->lock);
	return 0;
}


/* 
 * It is neccessary to call cache_release_el when you don't need element
 * returned by this function anymore
 *
 * WARNING: You must call no other domain function between get_record
 *          and release_record, if you do so, deadlock will occur !!!
 */
int get_urecord(udomain_t* _d, str* _a, urecord_t** _r)
{
	int sl, i;
	urecord_t* r;

	sl = hash_func(_d, _a->s, _a->len);

	if (sl == -1) {
		LOG(L_ERR, "get_urecord(): Error while generating hash\n");
		return -1;
	}

	get_lock(&_d->lock);
	r = _d->table[sl].first;

	for(i = 0; i < _d->table[sl].n; i++) {
		if ((r->aor.len == _a->len) && !memcmp(r->aor.s, _a->s, _a->len)) {
			*_r = r;
			return 0;
		}

		r = r->s_ll.next;
	}

	release_lock(&_d->lock);
	return 1;   /* Nothing found */
}


/*
 * Remove a record from domain
 */
int delete_urecord(udomain_t* _d, str* _a)
{
	int sl, i;
	urecord_t* r;
	
	sl = hash_func(_d, _a->s, _a->len);
	if (sl == -1) {
		LOG(L_ERR, "delete_urecord(): Error while generating hash\n");
		return -1;
	}

	get_lock(&_d->lock);
	r = _d->table[sl].first;

	for(i = 0; i < _d->table[sl].n; i++) {
		if ((r->aor.len == _a->len) && !memcmp(r->aor.s, _a->s, _a->len)) {
			if (use_db) {
				if (write_through) {
					if (db_del_urecord(r) < 0) {
						LOG(L_ERR, "delete_urecord(): Error while deleting from database\n");
					}
				} else {
				}
			}
			
			udomain_remove(_d, r);
			slot_rem(&_d->table[sl], r);

			r->domain = 0;
			free_urecord(r);
			release_lock(&_d->lock);
			return 0;
		}

		r = r->s_ll.next;
	}

	release_lock(&_d->lock);
	return 1; /* Record not found */
}


/*
 * Release a record previosly obtained through get_record
 */
void release_urecord(urecord_t* _r)
{
	fl_lock_t* l;

	if (_r->contacts == 0) {
		l = &_r->slot->d->lock;
		udomain_remove(_r->slot->d, _r);
		slot_rem(_r->slot, _r);
		_r->domain = 0;
		release_lock(l);
		free_urecord(_r);
		return;
	}

	release_lock(&_r->slot->d->lock);
}


int timer_udomain(udomain_t* _d)
{
	struct urecord* ptr, *t;

	get_lock(&_d->lock);

	ptr = _d->d_ll.first;

	while(ptr) {
		if (timer_urecord(ptr) < 0) {
			LOG(L_ERR, "timer_udomain(): Error in timer_urecord\n");
			release_lock(&_d->lock);
			return -1;
		}
		
		if (ptr->contacts == 0) {
			udomain_remove(ptr->slot->d, ptr);
			slot_rem(ptr->slot, ptr);
			ptr->domain = 0;
			t = ptr;
			ptr = ptr->d_ll.next;
			free_urecord(t);
		} else {
			ptr = ptr->d_ll.next;
		}
	}
	
	release_lock(&_d->lock);
	return 0;
}
