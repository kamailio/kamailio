/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH 
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2006-02-02  created by andrei
 */


#ifndef _hashes_h
#define _hashes_h

#include "str.h"
#include "mem/mem.h"
#include "clist.h"



/* internal use: hash update
 * params: char* s   - string start,
 *         char* end - end
 *         char* p,  and unsigned v temporary vars (used)
 *         unsigned h - result
 * h should be initialized (e.g. set it to 0), the result in h */
#define hash_update_str(s, end, p, v, h) \
	do{ \
		for ((p)=(s); (p)<=((end)-4); (p)+=4){ \
			(v)=(*(p)<<24)+((p)[1]<<16)+((p)[2]<<8)+(p)[3]; \
			(h)+=(v)^((v)>>3); \
		} \
		(v)=0; \
		for (;(p)<(end); (p)++){ (v)<<=8; (v)+=*(p);} \
		(h)+=(v)^((v)>>3); \
	}while(0)


/* internal use: call it to adjust the h from hash_update_str */
#define hash_finish(h) (((h)+((h)>>11))+(((h)>>13)+((h)>>23)))



/* "raw" 2 strings hash
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash2_raw(str* key1, str* key2)
{
	char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_str(key1->s, key1->s+key1->len, p, v, h);
	hash_update_str(key2->s, key2->s+key2->len, p, v, h);
	return hash_finish(h);
}



/* "raw" 1 string hash
 * returns an unsigned int (which you can use modulo table_size as hash value)
 */
inline static unsigned int get_hash1_raw(char* s, int len)
{
	char* p;
	register unsigned v;
	register unsigned h;
	
	h=0;
	
	hash_update_str(s, s+len, p, v, h);
	return hash_finish(h);
}



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
	ht->table=pkg_malloc(sizeof(struct str_hash_head)*size);
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
									char* key, int len)
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
