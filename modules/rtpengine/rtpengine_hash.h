#ifndef _RTPENGINE_HASH_H
#define _RTPENGINE_HASH_H

#include "../../str.h"
#include "../../locking.h"


/* table entry */
struct rtpengine_hash_entry {
	str callid;				// call callid
	str viabranch;				// call viabranch
	struct rtpp_node *node;			// call selected node

	unsigned int tout;			// call timeout
	struct rtpengine_hash_entry *next;	// call next
};

/* table */
struct rtpengine_hash_table {
	struct rtpengine_hash_entry **entry_list;	// hastable
	gen_lock_t **row_locks;				// hastable row locks
	unsigned int total;				// total number of entries in the hashtable
};


int rtpengine_hash_table_init(int size);
int rtpengine_hash_table_destroy();
int rtpengine_hash_table_insert(str callid, str viabranch, struct rtpengine_hash_entry *value);
int rtpengine_hash_table_remove(str callid, str viabranch);
struct rtpp_node *rtpengine_hash_table_lookup(str callid, str viabranch);
void rtpengine_hash_table_print();
unsigned int rtpengine_hash_table_total();

void rtpengine_hash_table_free_entry(struct rtpengine_hash_entry *entry);
void rtpengine_hash_table_free_entry_list(struct rtpengine_hash_entry *entry_list);

void rtpengine_hash_table_free_row_lock(gen_lock_t *lock);

#endif
