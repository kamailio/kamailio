/* 
 * $Id$ 
 */

#ifndef __CACHE_H__
#define __CACHE_H__

#include "c_slot.h"
#include "c_elem.h"
#include "location.h"


struct c_slot;
struct c_elem;

/*
 * Hash table
 */
typedef struct cache {
	int size;                     /* Hash table size */
	struct c_slot* table;         /* Hash table */
	int mutex;                    /* Semaphore ID if we have enough semaphores */
	int ref;                      /* Refence count */

	struct {                      /* Linked list of all elements in the cache */
		int count;            /* Number of element in the linked list */
		struct c_elem* first; /* First element in the list */
		struct c_elem* last;  /* Last element in the list */
	} c_ll;
} cache_t;


/* Get first element in the hash table */
#define CACHE_FIRST_ELEM(cache) (cache->c_ll.first)

/* Get number of elements in the hash table */
#define CACHE_ELEM_COUNT(cache) (cache->c_ll.count)

/* Get next element in the hash table */
#define CACHE_NEXT_ELEM(elem)  (elem->c_ll.next)

/* Get last elements in the hash table */
#define CACHE_LAST_ELEM(cache) (cache->c_ll.last)

/* Get a collision slot in hash table */
#define CACHE_GET_SLOT(cache,id) (&(cache->table[id]))


cache_t*       create_cache(int _size);
void           free_cache(cache_t* _c);
int            cache_put(cache_t* _c, location_t* _l);
struct c_elem* cache_get(cache_t* _c, const char* _aor);
int            cache_release_elem(struct c_elem* _el);
int            hash_func(cache_t* _c, const char* _str);
void           print_cache(cache_t* _c);

#endif
