#include "../../ut.h"
#include "dmqnode.h"
#include "dmq.h"

dmq_node_list_t* init_dmq_node_list() {
	dmq_node_list_t* node_list = shm_malloc(sizeof(dmq_node_list_t));
	memset(node_list, 0, sizeof(dmq_node_list_t));
	lock_init(&node_list->lock);
	return node_list;
}

inline int add_dmq_node(dmq_node_list_t* list, dmq_node_t* newnode) {
	dmq_node_t* copy = shm_malloc(sizeof(dmq_node_t));
	memcpy(copy, newnode, sizeof(dmq_node_t));
	shm_str_dup(&copy->orig_uri, &newnode->orig_uri);
	lock_get(&list->lock);
	copy->next = list->nodes;
	list->nodes = copy;
	list->count++;
	lock_release(&list->lock);
	return 0;
}