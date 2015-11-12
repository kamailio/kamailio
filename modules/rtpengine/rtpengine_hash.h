#ifndef _RTPENGINE_HASH_H
#define _RTPENGINE_HASH_H

#include "../../str.h"


/* table entry */
struct rtpengine_hash_entry {
	unsigned int tout;			// call timeout
	str callid;				// call callid
	struct rtpp_node *node;			// call selected node

	struct rtpengine_hash_entry *next;	// call next
};

/* table */
struct rtpengine_hash_table {
	struct rtpengine_hash_entry **entry_list;	// hastable
	unsigned int total;				// total number of entries in the hashtable
};


int rtpengine_hash_table_init(int size);
int rtpengine_hash_table_destroy();
int rtpengine_hash_table_insert(str *key, struct rtpengine_hash_entry *value);
int rtpengine_hash_table_remove(str *key);
struct rtpp_node *rtpengine_hash_table_lookup(str *key);
void rtpengine_hash_table_print();
unsigned int rtpengine_hash_table_total();

#endif
