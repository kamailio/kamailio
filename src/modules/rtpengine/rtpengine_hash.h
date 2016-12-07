#ifndef _RTPENGINE_HASH_H
#define _RTPENGINE_HASH_H

#include "../../core/str.h"
#include "../../core/locking.h"


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
	struct rtpengine_hash_entry **row_entry_list;	// vector of size pointers to entry
	gen_lock_t **row_locks;				// vector of size pointers to locks
	unsigned int *row_totals;			// vector of size numbers of entries in the hashtable rows
	unsigned int size;				// hash table size
};


int rtpengine_hash_table_init(int size);
int rtpengine_hash_table_destroy();
int rtpengine_hash_table_insert(str callid, str viabranch, struct rtpengine_hash_entry *value);
int rtpengine_hash_table_remove(str callid, str viabranch, enum rtpe_operation);
struct rtpp_node *rtpengine_hash_table_lookup(str callid, str viabranch, enum rtpe_operation);
void rtpengine_hash_table_print();
unsigned int rtpengine_hash_table_total();

void rtpengine_hash_table_free_entry(struct rtpengine_hash_entry *entry);
void rtpengine_hash_table_free_row_entry_list(struct rtpengine_hash_entry *row_entry_list);

int rtpengine_hash_table_sanity_checks();

#endif
