/*
 * Copyright (C) 2006 iptelorg GmbH 
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _str_hashs_h
#define _str_hashs_h

#include "str.h"
#include "hashes.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "locking.h"
#include "clist.h"
#include <string.h>

#define MAX_STR_LOCKS  2048
#define MIN_STR_LOCKS  2
/* generic, simple str keyed hash */

struct str_hash_entry{
	struct str_hash_entry* next;
	struct str_hash_entry* prev;
	struct str_hash_head* head;
	str key;
	unsigned int flags;
	union{
		void* p;
		char* s;
		int   n;
		char  data[sizeof(void*)];
	}u;
};


struct str_hash_head{
	struct str_hash_entry* next;
	struct str_hash_entry* prev;
	unsigned int lock_idx;
};


struct str_hash_table{
	struct str_hash_head* table;
	int size;
	char memory;
	unsigned int locks_no;
	gen_lock_set_t* locks;
};


inline static int init_locks(struct str_hash_table* ht)
{
	unsigned int n;

	n = (ht->size < MAX_STR_LOCKS) ? ht->size : MAX_STR_LOCKS;
	for( ; n >= MIN_STR_LOCKS ; n-- ){
		ht->locks = lock_set_alloc(n);
		if (ht->locks == 0){
			continue;
		}
		if (lock_set_init(ht->locks) == 0) {
			lock_set_dealloc(ht->locks);
			ht->locks = 0;
			continue;
		}
		ht->locks_no = n;
		break;
	}

	if (ht->locks == 0) {
		LM_ERR("unable to allocate at least %d locks\n", MIN_STR_LOCKS);
		goto error;
	}
	return 0;
error:
	shm_free(ht);
	ht = NULL;
	return -1;
}

inline static void str_hash_lock(struct str_hash_table* ht, struct str_hash_head* h)
{
	if (ht->memory == 's'){
		lock_set_get(ht->locks, h->lock_idx);
	}
}

inline static void str_hash_unlock(struct str_hash_table* ht, struct str_hash_head* h)
{
	if (ht->memory == 's'){
		lock_set_release(ht->locks, h->lock_idx);
	}
}

/* returns 0 on success, <0 on failure */
inline static int str_hash_alloc(struct str_hash_table* ht, int size)
{
	ht->table=(struct str_hash_head*)pkg_malloc(sizeof(struct str_hash_head)*size);
	if (ht->table==0)
		return -1;
	ht->size=size;
	ht->memory = 'p';
	return 0;
}

/* returns 0 on success, <0 on failure 
 * table is allocated in shared memory
 */
inline static int str_hash_shm_alloc(struct str_hash_table* ht, int size)
{
	ht->table=(struct str_hash_head*)shm_malloc(sizeof(struct str_hash_head)*size);
	if (ht->table==0)
		return -1;
	ht->size=size;
	ht->memory = 's';
	return init_locks(ht);
}


inline static void str_hash_init(struct str_hash_table* ht)
{
	int r;
	
	for (r=0; r<ht->size; r++) clist_init(&(ht->table[r]), next, prev);
}



inline static void str_hash_add(struct str_hash_table* ht, 
								struct str_hash_entry* e)
{
	int h;
	
	h=get_hash1_raw(e->key.s, e->key.len) % ht->size;
	e->head = &ht->table[h];
	str_hash_lock(ht, e->head);
	clist_insert(&ht->table[h], e, next, prev);
	str_hash_unlock(ht, e->head);
}

/* If the table was allocated in shared memory, the head will be locked
 * so it's safe to read the values or delete the entry
 * REMEMBER to unlock the head when you finish with the entry
 */
inline static struct str_hash_entry* str_hash_get(struct str_hash_table* ht,
									const char* key, int len)
{
	int h;
	struct str_hash_entry* e;
	
	h=get_hash1_raw(key, len) % ht->size;

	str_hash_lock(ht, &ht->table[h]);
	clist_foreach(&ht->table[h], e, next){
		if ((e->key.len==len) && (memcmp(e->key.s, key, len)==0))
			return e;
	}
	str_hash_unlock(ht, &ht->table[h]);
	return 0;
}


#define str_hash_del(e) clist_rm(e, next, prev)

typedef int (*free_cb) (struct str_hash_entry* e);

inline static void str_hash_destroy(struct str_hash_table* ht, free_cb f)
{
	int h;
	struct str_hash_entry* e;

	for(h = 0; h < ht->size; h++){
		str_hash_lock(ht, &ht->table[h]);
		clist_foreach(&ht->table[h], e, next){
			f(e);
		}
		str_hash_unlock(ht, &ht->table[h]);
	}

	if(ht->memory == 's'){
		shm_free(ht->table);
	} else {
		pkg_free(ht->table);
	}
}

#endif
