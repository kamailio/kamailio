/* 
 * $Id$ 
 */

#include "cache.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "../../dprint.h"

#define DEFAULT_CACHE_SIZE 512


/*
 * Create a new cache, _size is size
 * of the cache, -1 means default size
 */
cache_t* create_cache(int _size)
{
	cache_t* c;
	int s, i;

	if (_size == -1) {
		s = DEFAULT_CACHE_SIZE;
	} else {
		s = _size;
	}

	c = (cache_t*)malloc(sizeof(cache_t));
	if (!c) {
		LOG(L_ERR, "create_cache(): No memory left\n");
		return NULL;
	}

	c->table = (c_slot_t*)malloc(sizeof(c_slot_t) * s);
	if (!c->table) {
		LOG(L_ERR, "create_cache(): No memory left\n");
		free(c);
		return NULL;
	}

	for(i = 0; i < s; i++) {
		init_slot(c, &c->table[i]);
	}

	c->size = s;

	c->c_ll.first = NULL;
	c->c_ll.last = NULL;
	c->c_ll.count = 0;

	c->mutex = 0;
	c->ref = 0;

	if (init_timer_new("location") == FALSE) {
		LOG(L_ERR, "create_cache(): Unable to initialize timer_new()\n");
	}

	if (init_timer_dirty("location") == FALSE) {
		LOG(L_ERR, "create_cache(): Unable to initialize timer_dirty()\n");
	}

	return c;
}



void free_cache(cache_t* _c)
{
	int i;

#ifdef PARANOID
	if (!_c) return;
#endif
	close_timer_dirty();
	close_timer_new();


	if (_c->table) {
		for(i = 0; i < _c->size; i++) {
			deinit_slot(&(_c->table[i]));
		}
		free(_c->table);
	}

	free(_c);
}


int hash_func(cache_t* _c, const char* _str)
{
	int res = 0;

#ifdef PARANOID
	if (!_str) return -1;
	if (!_c) return -1;
#endif

	while(*_str != '\0') {
		res += *_str++;
	}

	return res % _c->size;
}



int cache_put(cache_t* _c, location_t* _l)
{
	int count, i;
	c_elem_t* ptr, *el;
	c_slot_t* slot;
	int slot_num;

#ifdef PARANOID
	if (!_c) return FALSE;
	if (!_l) return FALSE;
#endif
	ptr = create_element(_l);

	if (!ptr) {
		LOG(L_ERR, "insert_location(): No memory left\n");
		return FALSE;
	}

	slot_num = hash_func(_c, _l->user.s);

	if (slot_num == -1) {
		LOG(L_ERR, "insert_location(): Errro while hashing slot\n");
		free_element(ptr);
		return FALSE;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);
	mutex_down(_c->mutex);
	slot->ref++;
	count = slot->ll.count;
	if (add_slot_elem(slot, ptr) != TRUE) {
		mutex_up(_c->mutex);
		LOG(L_ERR, "cache_put(): Error while adding element to collision slot list\n");
		free_element(ptr);
		return FALSE;
	}
	if (add_cache_elem(_c, ptr) != TRUE) {
		mutex_up(_c->mutex);
		LOG(L_ERR, "cache_put(): Errow while adding element to hash table list\n");
		rem_slot_elem(slot, ptr);
		free_element(ptr);
		return FALSE;
	}
	mutex_up(_c->mutex);

	el = SLOT_FIRST_ELEM(slot);
	
	for(i = 0; i < count; i++) {
		if (!cmp_location(_l, el->loc->user.s))
			if (((el->state.ref) || (!el->state.garbage)) && (!el->state.invisible)) break;
		el = SLOT_ELEM_NEXT(el);
	}
	if (i == count) el = NULL;

	if (el) {
		merge_location(ptr->loc, el->loc);
		el->state.garbage = TRUE;
	}

	ptr->state.invisible = FALSE;

	mutex_down(_c->mutex);
	slot->ref--;
	mutex_up(_c->mutex);
	
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
	if (!_c) return NULL;
	if (!_aor) return NULL;
#endif
	p = strdup(_aor);               //FIXME
	p = strlower(p, strlen(p));
	_aor = p;
	slot_num = hash_func(_c, _aor);
	if (slot_num == -1) {
		LOG(L_ERR, "cache_get(): Error while generating hash\n");
		return NULL;
	}

	slot = CACHE_GET_SLOT(_c, slot_num);

	mutex_down(_c->mutex);
	slot->ref++;
	count = SLOT_ELEM_COUNT(slot);
	mutex_up(_c->mutex);

	el = SLOT_FIRST_ELEM(slot);

	for(i = 0; i < count; i++) {
		if ((el->state.garbage) && (!el->state.ref)) goto skip;  // obsolete element
		if (el->state.invisible) goto skip;  // Are next and prev setup properly when invisible ?
		if (!cmp_location(el->loc, _aor)) {
			mutex_down(_c->mutex);
			el->state.ref++;
			slot->ref--;
			mutex_up(_c->mutex);
			return el;
		}
	skip:
		el = SLOT_ELEM_NEXT(el);
	}

	mutex_down(_c->mutex);
	slot->ref--;
	mutex_up(_c->mutex);
	return NULL; /* Supposes that we have all elements from database in cache */
}


int cache_release_elem(c_elem_t* _el)
{
	c_slot_t* slot;
	cache_t* cache;

#ifdef PARANOID
	if (!_el) return FALSE;
#endif

	slot = _el->ht_slot;
	cache = slot->cache;

	mutex_down(cache->mutex);
	if (_el->state.ref) _el->state.ref--;
	mutex_up(cache->mutex);
	return TRUE;
}



void print_cache(cache_t* _c)
{
	c_elem_t* el;
#ifdef PARANOID
	if (!_c) return;
#endif

	el = CACHE_FIRST_ELEM(_c);

	printf("Cache content:\n");

	while(el) {
		print_element(el);
		el = CACHE_NEXT_ELEM(el);
	}

	printf("End of cache content\n");
}

