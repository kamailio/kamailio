/* 
 * $Id$ 
 */

#include "cache.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "defs.h"
#include <time.h>


static void      add_slot_elem (c_slot_t* _slot, c_elem_t* _el);
static c_elem_t* rem_slot_elem (c_slot_t* _slot, c_elem_t* _el);
static void      add_cache_elem(cache_t* _c, c_elem_t* _el);
static c_elem_t* rem_cache_elem(cache_t* _c, c_elem_t* _el);
static int       hash_func     (cache_t* _c, char* _s, int _len);


/*
 * Create a new cache, _size is size
 * of the cache, -1 means default size
 */
cache_t* create_cache(int _size, const char* _table)
{
	cache_t* c;
	int s, i;
	
	if (!_table) {
		LOG(L_ERR, "create_cache(): Invalid parameter value\n");
		return NULL;
	}

	if (_size == -1) {
		s = DEFAULT_CACHE_SIZE;
	} else {
		s = _size;
	}

	c = (cache_t*)pkg_malloc(sizeof(cache_t));
	if (!c) {
		LOG(L_ERR, "create_cache(): No memory left\n");
		return NULL;
	}

	c->table = (c_slot_t*)pkg_malloc(sizeof(c_slot_t) * s);
	if (!c->table) {
		LOG(L_ERR, "create_cache(): No memory left\n");
		pkg_free(c);
		return NULL;
	}

	c->db_table = (char*)pkg_malloc(strlen(_table) + 1);
	if (!c->db_table) {
		LOG(L_ERR, "create_cache(): No memory left\n");
		pkg_free(c);
		pkg_free(c->table);
		return NULL;
	}

	memcpy(c->db_table, _table, strlen(_table) + 1);

	for(i = 0; i < s; i++) {
		init_slot(c, &c->table[i]);
	}

	c->size = s;

	c->c_ll.first = NULL;
	c->c_ll.last = NULL;
	c->c_ll.count = 0;

	init_lock(c->lock);

	return c;
}


/*
 * Release all memory associated with cache
 */
void free_cache(cache_t* _c)
{
	int i;

	get_lock(&(_c->lock));
	if (_c->table) {
		for(i = 0; i < _c->size; i++) {
			deinit_slot(&(_c->table[i]));
		}
		pkg_free(_c->table);
		pkg_free(_c->db_table);
	}
	release_lock(&(_c->lock));
	pkg_free(_c);
	
}


/*
 * Hash function
 * FIXME: Do some measurements and implement
 *        better hash function generating flat
 *        distribution
 */
static int hash_func(cache_t* _c, char* _s, int _len)
{
	int res = 0;
	int i;

#ifdef PARANOID
	if ((!_s) || (!_c)) {
		LOG(L_ERR, "hash_func(): Invalid parameter value\n");
		return -1;
	}

#endif

	for(i = 0; i < _len; i++) {
		res += _s[i];
	}

	return res % _c->size;
}


/*
 * Put an element into cache
 */
int cache_put(cache_t* _c, db_con_t* _con, location_t* _l)
{
	c_elem_t* ptr;
	c_slot_t* slot;
	int slot_num;

	ptr = create_element(_l);
	if (!ptr) {
		LOG(L_ERR, "cache_put(): No memory left\n");
		return FALSE;
	}

	slot_num = hash_func(_c, _l->user.s, _l->user.len);

	if (slot_num == -1) {
		LOG(L_ERR, "cache_put(): Error while hashing slot\n");
		free_element(ptr);
		return FALSE;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);
	get_lock(&(slot->lock));

	     /* Synchronize with database if we are using database */
	if (_con) {
		if (db_insert_location(_con, _l) == FALSE) {
			LOG(L_ERR, "cache_put(): Error while inserting bindings into database\n");
			free_element(ptr);
			release_lock(&(slot->lock));
			return FALSE;
		}
	}

	add_slot_elem(slot, ptr);
	add_cache_elem(_c, ptr);

	release_lock(&(slot->lock));
	return TRUE;
}



/* 
 * It is neccessary to call cache_release_el when you don't need element
 * returned by this function anymore
 */
c_elem_t* cache_get(cache_t* _c, str* _aor)
{
	int slot_num, count, i;
	c_slot_t* slot;
	c_elem_t* el;
	char *p;

	p = (char*)pkg_malloc(_aor->len + 1);
	if (!p) {
		LOG(L_ERR, "cache_get(): No memory left\n");
		return NULL;
	}

	memcpy(p, _aor->s, _aor->len);
	p[_aor->len] = '\0';
	strlower(p, _aor->len);

	printf("p=%s\n", p);

	slot_num = hash_func(_c, p, _aor->len);
	if (slot_num == -1) {
		LOG(L_ERR, "cache_get(): Error while generating hash\n");
		pkg_free(p);
		return NULL;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);
	     /* FIXME: Tady by se mel zamykat jenom element */
	DBG("Before lock\n");
	get_lock(&(slot->lock));
	DBG("After lock\n");

	count = SLOT_ELEM_COUNT(slot);
	el = SLOT_FIRST_ELEM(slot);

	for(i = 0; i < count; i++) {
		if (!cmp_location(el->loc, p)) {
			pkg_free(p);
			return el;
		}
		el = SLOT_ELEM_NEXT(el);
	}

	DBG("Before release\n");
	release_lock(&(slot->lock));
	DBG("After release\n");
	pkg_free(p);
	return NULL;
}



void cache_release_elem(c_elem_t* _el)
{
	     /* FIXME: Tady by se mel uvolnovat lock elementu */
	release_lock(&(_el->ht_slot->lock));
}



void print_cache(cache_t* _c)
{
	c_elem_t* el;

	el = CACHE_FIRST_ELEM(_c);

	DBG("=== Cache content:\n");

	while(el) {
		print_element(el);
		el = CACHE_NEXT_ELEM(el);
	}

	DBG("=== End of cache content\n");
}



int clean_cache(cache_t* _c, db_con_t* _con)
{
	c_elem_t* el, *ptr;
	time_t t;
	fl_lock_t* l;

	el = CACHE_FIRST_ELEM(_c);
	t = time(NULL);

	while(el) {
		get_lock(&(el->ht_slot->lock));
		if (clean_location(el->loc, _con,  t) == FALSE) {
			LOG(L_ERR, "Error while cleaning cache\n");
			release_lock(&(el->ht_slot->lock));
			return FALSE;
		}
		ptr = CACHE_NEXT_ELEM(el);
		if (!(el->loc->contacts)) {
			l = &(el->ht_slot->lock);
			rem_slot_elem(el->ht_slot, el);
			rem_cache_elem(_c, el);
			release_lock(l);
			free_element(el);
		} else {
			release_lock(&(el->ht_slot->lock));
		}
		el = ptr;
	}
	return TRUE;
}


/*
 * Remove location from cache
 * If you want to remove all bindings for given
 */
int cache_remove(cache_t* _c, db_con_t* _con, str* _aor)
{
	int slot_num, count, i;
	c_slot_t* slot;
	c_elem_t* el;
	char *p;

	p = (char*)pkg_malloc(_aor->len + 1);
	if (!p) {
		LOG(L_ERR, "cache_remove(): No memory left\n");
		return FALSE;
	}

	memcpy(p, _aor->s, _aor->len);
	p[_aor->len] = '0';
	strlower(p, _aor->len);	

	slot_num = hash_func(_c, p, _aor->len);
	if (slot_num == -1) {
		LOG(L_ERR, "cache_remove(): Error while generating hash\n");
		pkg_free(p);
		return FALSE;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);

	get_lock(&(slot->lock));
	count = SLOT_ELEM_COUNT(slot);
	el = SLOT_FIRST_ELEM(slot);

	for(i = 0; i < count; i++) {
		if (!cmp_location(el->loc, p)) {
			if (_con) {
				if (db_remove_location(_con, el->loc) == FALSE) {
					LOG(L_ERR, "cache_remove(): Error while removing bindings from database\n");
					pkg_free(p);
					release_lock(&(slot->lock));
					return FALSE;
				}
			}

			rem_slot_elem(slot, el);
			rem_cache_elem(_c, el);
			release_lock(&(slot->lock));
			free_element(el);
			pkg_free(p);
			return TRUE;
		}
		el = SLOT_ELEM_NEXT(el);
	}
	release_lock(&(slot->lock));
	pkg_free(p);
	return TRUE;
}



int cache_update(cache_t* _c, db_con_t* _con, c_elem_t* _el, location_t* _loc)
{
        fl_lock_t* lock;
	if (update_location(_con, _el->loc, _loc) == FALSE) {
		LOG(L_ERR, "cache_update(): Error while updating location\n");
		return FALSE;
	}

	if (!(_el->loc->contacts)) {
	        lock = &(_el->ht_slot->lock);
		rem_slot_elem(_el->ht_slot, _el);
		rem_cache_elem(_c, _el);
		release_lock(lock);
		free_element(_el);
	}

	free_location(_loc);
	return TRUE;
}


int preload_cache(cache_t* _c, db_con_t* _con)
{
	db_key_t columns[6] = { "user", "contact", "expires", "q", "callid", "cseq" };
	db_res_t* res;
	int i, cseq, slot_num;
	location_t* loc = NULL;
	const char* cur_user, *user = NULL, *contact, *callid;
	db_row_t* row;
        double q;
	time_t expires;
	c_elem_t* ptr;
	c_slot_t* slot;
	str s;


#ifdef PARANOID
	if (!_c) {
		LOG(L_ERR, "preload_cache(): Invalid parameter value\n");
		return FALSE;
	}
#endif

	if (db_query(_con, NULL, NULL, columns, 0, 6, "user", &res) == FALSE) {
		LOG(L_ERR, "preload_cache(): Error while doing db_query\n");
		return FALSE;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("preload_cache(): Table is empty\n");
		db_free_query(_con, res);
		return TRUE;
	}

    
	for(i = 0; i < RES_ROW_N(res); i++) {
		row = RES_ROWS(res) + i;
		cur_user = ROW_VALUES(row)[0].val.string_val;
		if (!i) user = cur_user;

		if ((strcmp(cur_user, user)) || (!i)) {
			DBG("Preloading contacts for username %s\n", cur_user);
			user = cur_user;
			if (loc) {
				     //cache_put(_c, loc);
				ptr = create_element(loc);
				slot_num = hash_func(_c, loc->user.s, loc->user.len);
				slot = CACHE_GET_SLOT(_c, slot_num);
				add_slot_elem(slot, ptr);
				add_cache_elem(_c, ptr);
			}

			s.s = (char*)cur_user;
			s.len = strlen(cur_user);
			create_location(&loc, &s);
		}
		
		contact = ROW_VALUES(row)[1].val.string_val;
		expires = ROW_VALUES(row)[2].val.time_val;
		q = ROW_VALUES(row)[3].val.double_val;
		callid = ROW_VALUES(row)[4].val.string_val;
		cseq = ROW_VALUES(row)[5].val.int_val;
		
		DBG("    contact=%s expires=%d q=%3.2f callid=%s cseq=%d\n", contact, (unsigned int)expires, q, callid, cseq);
		add_contact(loc, contact, expires, q, callid, cseq);
	}

	if (loc) {
		//		cache_put(_c, loc);
		ptr = create_element(loc);
		slot_num = hash_func(_c, loc->user.s, loc->user.len);
		slot = CACHE_GET_SLOT(_c, slot_num);
		add_slot_elem(slot, ptr);
		add_cache_elem(_c, ptr);
	}

	db_free_query(_con, res);
	return TRUE;
}





/*
 * Add an element to an slot's linked list
 */
static void add_slot_elem(c_slot_t* _slot, c_elem_t* _el)
{
	if (!_slot->ll.count++) {
		_slot->ll.first = _slot->ll.last = _el;
	} else {
		_el->ll.prev = _slot->ll.last;
		_slot->ll.last->ll.next = _el;
		_slot->ll.last = _el;
	}
	
	_el->ht_slot = _slot;
}



static c_elem_t* rem_slot_elem(c_slot_t* _slot, c_elem_t* _el)
{
	c_elem_t* ptr;

	if (!_slot->ll.count) return NULL;

	ptr = _slot->ll.first;

	while(ptr) {
		if (ptr == _el) {
			if (ptr->ll.prev) {
				ptr->ll.prev->ll.next = ptr->ll.next;
			} else {
				_slot->ll.first = ptr->ll.next;
			}
			if (ptr->ll.next) {
				ptr->ll.next->ll.prev = ptr->ll.prev;
			} else {
				_slot->ll.last = ptr->ll.prev;
			}
			ptr->ll.prev = ptr->ll.next = NULL;
			ptr->ht_slot = NULL;
			_slot->ll.count--;
			break;
		}
		ptr = ptr->ll.next;
	}
	return ptr;
}


/*
 * Add an element to linked list of all elements in hash table
 */
static void add_cache_elem(cache_t* _c, c_elem_t* _el)
{
	get_lock(&(_c->lock));
	if (!_c->c_ll.count++) {
		_c->c_ll.first = _c->c_ll.last = _el;
	} else {
		_el->c_ll.prev = _c->c_ll.last;
		_c->c_ll.last->c_ll.next = _el;
		_c->c_ll.last = _el;
	}
	release_lock(&(_c->lock));
}



static c_elem_t* rem_cache_elem(cache_t* _c, c_elem_t* _el)
{
	c_elem_t* ptr;

	get_lock(&(_c->lock));

	if (!_c->c_ll.count) {
		release_lock(&(_c->lock));
		return NULL;
	}
		

	ptr = _c->c_ll.first;

	while(ptr) {
		if (ptr == _el) {
			if (ptr->c_ll.prev) {
				ptr->c_ll.prev->c_ll.next = ptr->c_ll.next;
			} else {
				_c->c_ll.first = ptr->c_ll.next;
			}
			if (ptr->c_ll.next) {
				ptr->c_ll.next->c_ll.prev = ptr->c_ll.prev;
			} else {
				_c->c_ll.last = ptr->c_ll.prev;
			}
			ptr->c_ll.prev = ptr->c_ll.next = NULL;
			_c->c_ll.count--;
			break;
		}
		ptr = ptr->c_ll.next;
	}

	release_lock(&(_c->lock));

	return ptr;
}
		

