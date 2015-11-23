#ifndef _RTPENGINE_HASH_H
#define _RTPENGINE_HASH_H

#include "../../str.h"

#define RTPENGINE_HASH_TABLE_SIZE       512

/* table entry */
struct rtpengine_hash_entry {
	unsigned int tout;			// call timeout
	str callid;				// call callid
	struct rtpp_node *node;			// call selected node

	struct rtpengine_hash_entry *next;	// next 
};

/* table */
struct rtpengine_hash_table {
	struct rtpengine_hash_entry *entry_list[RTPENGINE_HASH_TABLE_SIZE];
};


int rtpengine_hash_table_init();
int rtpengine_hash_table_destroy();
int rtpengine_hash_table_insert(void *key, void *value);
int rtpengine_hash_table_remove(void *key);
void* rtpengine_hash_table_lookup(void *key);
void rtpengine_hash_table_print() ;

#endif
