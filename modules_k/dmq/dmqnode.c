#include "../../ut.h"
#include "dmqnode.h"
#include "dmq.h"

dmq_node_t* self_node;
dmq_node_t* notification_node;	

dmq_node_list_t* init_dmq_node_list() {
	dmq_node_list_t* node_list = shm_malloc(sizeof(dmq_node_list_t));
	memset(node_list, 0, sizeof(dmq_node_list_t));
	lock_init(&node_list->lock);
	return node_list;
}

inline dmq_node_t* add_dmq_node(dmq_node_list_t* list, str* uri) {
	dmq_node_t* newnode = shm_malloc(sizeof(dmq_node_t));
	memset(newnode, 0, sizeof(dmq_node_t));
	shm_str_dup(&newnode->orig_uri, uri);
	if(parse_uri(newnode->orig_uri.s, newnode->orig_uri.len, &newnode->uri) < 0) {
		LM_ERR("error in parsing node uri\n");
		goto error;
	}
	lock_get(&list->lock);
	newnode->next = list->nodes;
	list->nodes = newnode;
	list->count++;
	lock_release(&list->lock);
	return newnode;
error:
	shm_free(newnode);
	return 0;
}