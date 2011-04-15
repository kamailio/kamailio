#include "dmqnode.h"
#include "dmq.h"

dmq_node_list_t* init_dmq_node_list() {
	dmq_node_list_t* node_list = shm_malloc(sizeof(dmq_node_list_t));
	memset(node_list, 0, sizeof(dmq_node_list_t));
	lock_init(&node_list->lock);
	return node_list;
}