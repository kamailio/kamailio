#ifndef DMQNODE_H
#define DMQNODE_H

#include <string.h>
#include <stdlib.h>
#include "../../lock_ops.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"

typedef struct dmq_node {
	struct sip_uri* uri;
	int status;
	int last_notification;
	struct dmqnode* next;
} dmq_node_t;

typedef struct dmq_node_list {
	gen_lock_t lock;
	struct dmq_node* nodes;
	int count;
} dmq_node_list_t;

dmq_node_list_t* init_dmq_node_list();
int update_node_list(dmq_node_list_t* remote_list);

#endif