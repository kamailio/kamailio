#ifndef DMQNODE_H
#define DMQNODE_H

#include <string.h>
#include <stdlib.h>
#include "../../lock_ops.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"

#define NBODY_LEN 1024

typedef struct dmq_node {
	int local; /* local type set means the dmq dmqnode == self */
	str orig_uri; /* original uri string - e.g. sip:127.0.0.1:5060;passive=true */
	struct sip_uri uri; /* parsed uri string */
	int status; /* reserved - maybe something like active,timeout,disabled */
	int last_notification; /* last notificatino receied from the node */
	struct dmq_node* next; /* pointer to the next struct dmq_node */
} dmq_node_t;

typedef struct dmq_node_list {
	gen_lock_t lock; /* lock for the list - must acquire before manipulating it */
	struct dmq_node* nodes; /* the nodes in the list */
	int count; /* the number of nodes in the list */
} dmq_node_list_t;

dmq_node_list_t* init_dmq_node_list();
int update_node_list(dmq_node_list_t* remote_list);
dmq_node_t* add_dmq_node(dmq_node_list_t* list, str* uri);
dmq_node_t* find_dmq_node(dmq_node_list_t* list, dmq_node_t* node);
int del_dmq_node(dmq_node_list_t* list, dmq_node_t* node);
int cmp_dmq_node(dmq_node_t* node, dmq_node_t* cmpnode);
dmq_node_t* shm_dup_node(dmq_node_t* node);
void shm_free_node(dmq_node_t* node);

extern dmq_node_t* self_node;
extern dmq_node_t* notification_node;	

#endif