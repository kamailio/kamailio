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
#include "sh_malloc.h"


static void      cache_add_elem(cache_t* _c, c_elem_t* _el);
static c_elem_t* cache_rem_elem(cache_t* _c, c_elem_t* _el);
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

	     /* Must be always in shared memory, since
	      * the cache is accessed from timer which
	      * lives in a separate process
	      */
	c = (cache_t*)sh_malloc(sizeof(cache_t));
	if (!c) {
		LOG(L_ERR, "create_cache(): No memory left\n");
		return NULL;
	}

	CACHE_HTABLE(c) = (c_slot_t*)sh_malloc(sizeof(c_slot_t) * s);
	if (!CACHE_HTABLE(c)) {
		LOG(L_ERR, "create_cache(): No memory left\n");
		sh_free(c);
		return NULL;
	}

	CACHE_DB_TABLE(c) = (char*)sh_malloc(strlen(_table) + 1);
	if (!CACHE_DB_TABLE(c)) {
		LOG(L_ERR, "create_cache(): No memory left\n");
		sh_free(c);
		sh_free(CACHE_HTABLE(c));
		return NULL;
	}

	memcpy(CACHE_DB_TABLE(c), _table, strlen(_table) + 1);

	for(i = 0; i < s; i++) {
		init_slot(c, &CACHE_HTABLE(c)[i]);
	}

	CACHE_SIZE(c) = s;

	CACHE_FIRST_ELEM(c) = NULL;
	CACHE_LAST_ELEM(c) = NULL;
	CACHE_ELEM_COUNT(c) = 0;

	init_lock(CACHE_LOCK(c));

	return c;
}


/*
 * Release all memory associated with cache
 */
void free_cache(cache_t* _c)
{
	int i;

	get_lock(&(CACHE_LOCK(_c)));
	if (CACHE_HTABLE(_c)) {
		for(i = 0; i < CACHE_SIZE(_c); i++) {
			deinit_slot(&(CACHE_HTABLE(_c)[i]));
		}
		sh_free(CACHE_HTABLE(_c));
		sh_free(CACHE_DB_TABLE(_c));
	}
	release_lock(&(CACHE_LOCK(_c)));
        sh_free(_c);
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

	return res % CACHE_SIZE(_c);
}


/*
 * Put an element into cache
 */
int cache_insert(cache_t* _c, db_con_t* _con, location_t* _l)
{
	c_elem_t* ptr;
	c_slot_t* slot;
	int slot_num;

	ptr = create_element(_l);
	if (!ptr) {
		LOG(L_ERR, "cache_insert(): No memory left\n");
		return FALSE;
	}

	slot_num = hash_func(_c, _l->user.s, _l->user.len);

	if (slot_num == -1) {
		LOG(L_ERR, "cache_insert(): Error while hashing slot\n");
		free_element(ptr);
		return FALSE;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);
	get_lock(&(SLOT_LOCK(slot)));

	     /* Synchronize with database if we are using database */
	if (_con) {
		if (db_insert_location(_con, _l) == FALSE) {
			LOG(L_ERR, "cache_insert(): Error while inserting bindings into database\n");
			free_element(ptr);
			release_lock(&(SLOT_LOCK(slot)));
			return FALSE;
		}
	}

	slot_add_elem(slot, ptr);
	cache_add_elem(_c, ptr);

	release_lock(&(SLOT_LOCK(slot)));
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

	LOG(L_ERR,"usr_loc: p=%s\n", p);

	slot_num = hash_func(_c, p, _aor->len);
	if (slot_num == -1) {
		LOG(L_ERR, "cache_get(): Error while generating hash\n");
		pkg_free(p);
		return NULL;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);
	     /* FIXME: Tady by se mel zamykat jenom element */
	get_lock(&(SLOT_LOCK(slot)));

	count = SLOT_ELEM_COUNT(slot);
	el = SLOT_FIRST_ELEM(slot);

	for(i = 0; i < count; i++) {
		if (!cmp_location(ELEM_LOC(el), p)) {
			pkg_free(p);
			return el;
		}
		el = ELEM_SLOT_NEXT(el);
	}

	release_lock(&(SLOT_LOCK(slot)));
	pkg_free(p);
	return NULL;
}



void cache_release_elem(c_elem_t* _el)
{
	     /* FIXME: Tady by se mel uvolnovat lock elementu */
	release_lock(&(SLOT_LOCK(ELEM_SLOT(_el))));
}



void print_cache(cache_t* _c)
{
	c_elem_t* el;

	el = CACHE_FIRST_ELEM(_c);

	LOG(L_ERR, "=== Cache content:\n");

	while(el) {
		print_element(el);
		el = ELEM_CACHE_NEXT(el);
	}

	LOG(L_ERR, "=== End of cache content\n");
}



int clean_cache(cache_t* _c, db_con_t* _con)
{
	c_elem_t* el, *ptr;
	time_t t;
	fl_lock_t* l;

	el = CACHE_FIRST_ELEM(_c);
	t = time(NULL);

	LOG(L_ERR, "clean_cache(): Begin cache elements traversal\n");
	while(el) {
		get_lock(&(SLOT_LOCK(ELEM_SLOT(el))));
		LOG(L_ERR, "clean_cache(): Running clean_location: \"%s\"\n", ELEM_LOC(el)->user.s);
		if (clean_location(ELEM_LOC(el), _con, t) == FALSE) {
			LOG(L_ERR, "Error while cleaning cache: \"%s\"\n", ELEM_LOC(el)->user.s);
			release_lock(&(SLOT_LOCK(ELEM_SLOT(el))));
			return FALSE;
		}
		LOG(L_ERR, "clean_cache(): Clean location done: \"%s\"\n", ELEM_LOC(el)->user.s);
		ptr = ELEM_CACHE_NEXT(el);
		if (!(ELEM_LOC(el)->contacts)) {
			LOG(L_ERR, "clean_cache(): Location empty, will be removed: \"%s\"\n", ELEM_LOC(el)->user.s);
			l = &(SLOT_LOCK(ELEM_SLOT(el)));
			slot_rem_elem(el);
			cache_rem_elem(_c, el);
			release_lock(l);
			free_element(el);
		} else {
			LOG(L_ERR, "clean_cache(): Location not empty yet: \"%s\"\n", ELEM_LOC(el)->user.s);
			release_lock(&(SLOT_LOCK(ELEM_SLOT(el))));
		}
		el = ptr;
	}
	LOG(L_ERR, "clean_cache(): End cache elements traversal\n");
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

	get_lock(&(SLOT_LOCK(slot)));
	count = SLOT_ELEM_COUNT(slot);
	el = SLOT_FIRST_ELEM(slot);

	for(i = 0; i < count; i++) {
		if (!cmp_location(ELEM_LOC(el), p)) {
			if (_con) {
				if (db_remove_location(_con, ELEM_LOC(el)) == FALSE) {
					LOG(L_ERR, "cache_remove(): Error while removing bindings from database\n");
					pkg_free(p);
					release_lock(&(SLOT_LOCK(slot)));
					return FALSE;
				}
			}

			slot_rem_elem(el);
			cache_rem_elem(_c, el);
			release_lock(&(SLOT_LOCK(slot)));
			free_element(el);
			pkg_free(p);
			return TRUE;
		}
		el = ELEM_SLOT_NEXT(el);
	}
	release_lock(&(SLOT_LOCK(slot)));
	pkg_free(p);
	return TRUE;
}



int cache_update(cache_t* _c, db_con_t* _con, c_elem_t** _el, location_t* _loc, int* _sr)
{
        fl_lock_t* lock;
	if (update_location(_con, ELEM_LOC((*_el)), _loc, _sr) == FALSE) {
		LOG(L_ERR, "cache_update(): Error while updating location\n");
		return FALSE;
	}

	if (!(ELEM_LOC((*_el))->contacts)) {
	        lock = &(SLOT_LOCK(ELEM_SLOT((*_el))));
		slot_rem_elem((*_el));
		cache_rem_elem(_c, (*_el));
		release_lock(lock);
		free_element((*_el));
		*_el = NULL;
	}

	free_location(_loc);
	return TRUE;
}


int preload_cache(cache_t* _c, db_con_t* _con)
{
	db_key_t columns[6] = {USER_COLUMN, CONTACT_COLUMN, EXPIRES_COLUMN, Q_COLUMN, CALLID_COLUMN, CSEQ_COLUMN};
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

	if (db_query(_con, NULL, NULL, columns, 0, 6, USER_COLUMN, &res) == FALSE) {
		LOG(L_ERR, "preload_cache(): Error while doing db_query\n");
		return FALSE;
	}

	if (RES_ROW_N(res) == 0) {
		LOG(L_ERR, "preload_cache(): Table is empty\n");
		db_free_query(_con, res);
		return TRUE;
	}
    
	for(i = 0; i < RES_ROW_N(res); i++) {
		row = RES_ROWS(res) + i;
		cur_user = ROW_VALUES(row)[0].val.string_val;
		if (!i) user = cur_user;

		if ((strcmp(cur_user, user)) || (!i)) {
			LOG(L_ERR, "Preloading contacts for username %s\n", cur_user);
			user = cur_user;
			if (loc) {
				ptr = create_element(loc);
				slot_num = hash_func(_c, loc->user.s, loc->user.len);
				slot = CACHE_GET_SLOT(_c, slot_num);
				slot_add_elem(slot, ptr);
				cache_add_elem(_c, ptr);
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
		
		LOG(L_ERR, "    contact=%s expires=%d q=%3.2f callid=%s cseq=%d\n", contact, (unsigned int)expires, q, callid, cseq);
		add_contact(loc, contact, expires, q, callid, cseq);
	}

	if (loc) {
		ptr = create_element(loc);
		slot_num = hash_func(_c, loc->user.s, loc->user.len);
		slot = CACHE_GET_SLOT(_c, slot_num);
		slot_add_elem(slot, ptr);
		cache_add_elem(_c, ptr);
	}

	db_free_query(_con, res);
	return TRUE;
}


/*
 * Add an element to linked list of all elements in hash table
 */
static void cache_add_elem(cache_t* _c, c_elem_t* _el)
{
	get_lock(&(CACHE_LOCK(_c)));
	if (!CACHE_ELEM_COUNT(_c)++) {
		CACHE_FIRST_ELEM(_c) = _el;
		CACHE_LAST_ELEM(_c) = _el;
	} else {
		ELEM_CACHE_PREV(_el) = CACHE_LAST_ELEM(_c);
		ELEM_CACHE_NEXT(CACHE_LAST_ELEM(_c)) = _el;
		CACHE_LAST_ELEM(_c) = _el;
	}
	release_lock(&(CACHE_LOCK(_c)));
}



static c_elem_t* cache_rem_elem(cache_t* _c, c_elem_t* _el)
{
	get_lock(&(CACHE_LOCK(_c)));

	if (!CACHE_ELEM_COUNT(_c)) {
		release_lock(&(CACHE_LOCK(_c)));
		return NULL;
	}
		
	if (ELEM_CACHE_PREV(_el)) {
		ELEM_CACHE_NEXT(ELEM_CACHE_PREV(_el)) = ELEM_CACHE_NEXT(_el);
	} else {
		CACHE_FIRST_ELEM(_c) = ELEM_CACHE_NEXT(_el);
	}
	if (ELEM_CACHE_NEXT(_el)) {
		ELEM_CACHE_PREV(ELEM_CACHE_NEXT(_el)) = ELEM_CACHE_PREV(_el);
	} else {
		CACHE_LAST_ELEM(_c) = ELEM_CACHE_PREV(_el);
	}
	ELEM_CACHE_PREV(_el) = ELEM_CACHE_NEXT(_el) = NULL;
	CACHE_ELEM_COUNT(_c)--;

	release_lock(&(CACHE_LOCK(_c)));

	return _el;
}
		

