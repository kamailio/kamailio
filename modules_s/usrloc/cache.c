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

#define DEFAULT_CACHE_SIZE 512


static inline void cache_use_table(cache_t* _c);


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

	return c;
}


int cache_use_connection(cache_t* _c, db_con_t* _con)
{
#ifdef PARANOID
	if ((!_c) || (!_con)) {
		LOG(L_ERR, "cache_use_connection(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	_c->db_con = _con;
	return TRUE;
}


/*
 * Release all memory associated with cache
 */
void free_cache(cache_t* _c)
{
	int i;

#ifdef PARANOID
	if (!_c) return;
#endif
	if (_c->table) {
		for(i = 0; i < _c->size; i++) {
			deinit_slot(&(_c->table[i]));
		}
		pkg_free(_c->table);
		pkg_free(_c->db_table);
	}

	pkg_free(_c);
}


/*
 * Hash function
 * FIXME: Do some measurements and implement
 *        better hash function generating flat
 *        distribution
 */
int hash_func(cache_t* _c, const char* _str)
{
	int res = 0;

#ifdef PARANOID
	if ((!_str) || (!_c)) {
		LOG(L_ERR, "hash_func(): Invalid parameter value\n");
		return -1;
	}

#endif

	while(*_str != '\0') {
		res += *_str++;
	}

	return res % _c->size;
}


/*
 * Put an element into cache
 */
int cache_put(cache_t* _c, location_t* _l)
{
	c_elem_t* ptr, *el;
	c_slot_t* slot;
	int slot_num;

#ifdef PARANOID
	if ((!_c) || (!_l)) {
		LOG(L_ERR, "cache_put(): Invalid parameter value\n");
		return FALSE;
	}
#endif

	ptr = create_element(_l);

	if (!ptr) {
		LOG(L_ERR, "cache_put(): No memory left\n");
		return FALSE;
	}

	slot_num = hash_func(_c, _l->user.s);

	if (slot_num == -1) {
		LOG(L_ERR, "cache_put(): Errro while hashing slot\n");
		free_element(ptr);
		return FALSE;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);

#ifdef USE_DB
	cache_use_table(_c);
	if (db_insert_location(_c->db_con, _l) == FALSE) {
		LOG(L_ERR, "cache_put(): Error while inserting bindings into database\n");
		free_element(ptr);
		return FALSE;
	}
#endif

	if (add_slot_elem(slot, ptr) != TRUE) {
		LOG(L_ERR, "cache_put(): Error while adding element to collision slot list\n");
		free_element(ptr);
		return FALSE;
	}

	if (add_cache_elem(_c, ptr) != TRUE) {
		LOG(L_ERR, "cache_put(): Errow while adding element to hash table list\n");
		rem_slot_elem(slot, ptr);
		free_element(ptr);
		return FALSE;
	}

	return TRUE;
}



/* 
 * It is neccessary to call cache_release_el when you don't need element
 * returned by this function anymore
 */
c_elem_t* cache_get(cache_t* _c, const char* _aor)
{
	int slot_num, count, i;
	c_slot_t* slot;
	c_elem_t* el;
	char *p;

#ifdef PARANOID
	if ((!_c) || (!_aor)) {
		LOG(L_ERR, "cache_get(): Invalid _c parameter value\n");
		return NULL;
	}
#endif
	p = strdup(_aor);               //FIXME
	p = strlower(p, strlen(p));
	slot_num = hash_func(_c, p);
	if (slot_num == -1) {
		LOG(L_ERR, "cache_get(): Error while generating hash\n");
		free(p);
		return NULL;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);

	count = SLOT_ELEM_COUNT(slot);
	el = SLOT_FIRST_ELEM(slot);

	for(i = 0; i < count; i++) {
		if (!cmp_location(el->loc, p)) {
			return el;
		}
		el = SLOT_ELEM_NEXT(el);
	}

	return NULL;
}



int cache_release_elem(c_elem_t* _el)
{
	return TRUE;
}



void print_cache(cache_t* _c)
{
	c_elem_t* el;
#ifdef PARANOID
	if (!_c) return;
#endif

	el = CACHE_FIRST_ELEM(_c);

	printf("=== Cache content:\n");

	while(el) {
		print_element(el);
		el = CACHE_NEXT_ELEM(el);
	}

	printf("=== End of cache content\n");
}



/*
 * Remove one or more bindings from cache
 * If you want to remove all bindings for given
 * to, set the last parameter to NULL
 */
int cache_remove(cache_t* _c, const char* _aor)
{
	int slot_num, count, i;
	c_slot_t* slot;
	c_elem_t* el;
	char *p;

#ifdef PARANOID
	if ((!_c) || (!_aor)) {
		LOG(L_ERR, "cache_remove(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	p = strdup(_aor);               //FIXME
	p = strlower(p, strlen(p));
	slot_num = hash_func(_c, p);
	if (slot_num == -1) {
		LOG(L_ERR, "cache_remove(): Error while generating hash\n");
		free(p);
		return FALSE;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);

	count = SLOT_ELEM_COUNT(slot);
	el = SLOT_FIRST_ELEM(slot);

	for(i = 0; i < count; i++) {
		if (!cmp_location(el->loc, p)) {
#ifdef USE_DB
			cache_use_table(_c);
			if (db_remove_location(_c->db_con, el->loc) == FALSE) {
				LOG(L_ERR, "cache_remove(): Error while removing bindings from database\n");
				free_element(el);
				free(p);
				return FALSE;
			}
#endif
			rem_slot_elem(slot, el);
			rem_cache_elem(_c, el);
			free_element(el);
			free(p);
			return TRUE;
		}
		el = SLOT_ELEM_NEXT(el);
	}
	free(p);
	return TRUE;
}



int cache_update(cache_t* _c, c_elem_t* _el, location_t* _loc)
{
#ifdef PARANOID
	if ((!_c) || (!_el) || (!_loc)) {
		LOG(L_ERR, "cache_update(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	cache_use_table(_c);
	if (update_location(_c->db_con, _el->loc, _loc) == FALSE) {
		LOG(L_ERR, "cache_update(): Error while updating location\n");
		return FALSE;
	}

	if (!(_el->loc->contacts)) {
		rem_slot_elem(_el->ht_slot, _el);
		rem_cache_elem(_c, _el);
		free_element(_el);
	}

	free_location(_loc);
	
	return TRUE;
}



static inline void cache_use_table(cache_t* _c)
{
	db_use_table(_c->db_con, _c->db_table);
}



int preload_cache(cache_t* _c)
{
	db_key_t columns[6] = { "user", "contact", "expires", "q", "callid", "cseq" };
	db_res_t* res;
	int i, cseq, slot_num;
	location_t* loc = NULL;
	const char* cur_user, *user, *contact, *callid;
	db_row_t* row;
        double q;
	time_t expires;
	c_elem_t* ptr;
	c_slot_t* slot;
	time_t t;


#ifdef PARANOID
	if (!_c) {
		LOG(L_ERR, "preload_cache(): Invalid parameter value\n");
		return FALSE;
	}
#endif

	cache_use_table(_c);

	if (db_query(_c->db_con, NULL, NULL, columns, 0, 6, "user", &res) == FALSE) {
		LOG(L_ERR, "preload_cache(): Error while doing db_query\n");
		return FALSE;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("preload_cache(): Table is empty\n");
		db_free_query(_c->db_con, res);
		return TRUE;
	}

    
	t = time(NULL);
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
				slot_num = hash_func(_c, loc->user.s);
				slot = CACHE_GET_SLOT(_c, slot_num);
				add_slot_elem(slot, ptr);
				add_cache_elem(_c, ptr);
			}

			create_location(&loc, cur_user);
		}
		
		contact = ROW_VALUES(row)[1].val.string_val;
		expires = ROW_VALUES(row)[2].val.time_val - time(NULL);
		q = ROW_VALUES(row)[3].val.double_val;
		callid = ROW_VALUES(row)[4].val.string_val;
		cseq = ROW_VALUES(row)[5].val.int_val;
		
		DBG("    contact=%s expires=%d q=%3.2f callid=%s cseq=%d\n", contact, expires, q, callid, cseq);
		add_contact(loc, contact, expires + t, q, callid, cseq);
	}

	if (loc) {
		//		cache_put(_c, loc);
		ptr = create_element(loc);
		slot_num = hash_func(_c, loc->user.s);
		slot = CACHE_GET_SLOT(_c, slot_num);
		add_slot_elem(slot, ptr);
		add_cache_elem(_c, ptr);
	}

	db_free_query(_c->db_con, res);
	return TRUE;
}
