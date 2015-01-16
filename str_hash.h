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
#include "clist.h"
#include <string.h>


/* generic, simple str keyed hash */

struct str_hash_entry{
	struct str_hash_entry* next;
	struct str_hash_entry* prev;
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
};


struct str_hash_table{
	struct str_hash_head* table;
	int size;
};



/* returns 0 on success, <0 on failure */
inline static int str_hash_alloc(struct str_hash_table* ht, int size)
{
	ht->table=(struct str_hash_head*)pkg_malloc(sizeof(struct str_hash_head)*size);
	if (ht->table==0)
		return -1;
	ht->size=size;
	return 0;
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
	clist_insert(&ht->table[h], e, next, prev);
}



inline static struct str_hash_entry* str_hash_get(struct str_hash_table* ht,
									const char* key, int len)
{
	int h;
	struct str_hash_entry* e;
	
	h=get_hash1_raw(key, len) % ht->size;
	clist_foreach(&ht->table[h], e, next){
		if ((e->key.len==len) && (memcmp(e->key.s, key, len)==0))
			return e;
	}
	return 0;
}


#define str_hash_del(e) clist_rm(e, next, prev)

#endif
