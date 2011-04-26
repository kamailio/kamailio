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

inline int cmp_dmq_node(dmq_node_t* node, dmq_node_t* cmpnode) {
	if(!node || !cmpnode) {
		LM_ERR("cmp_dmq_node - null node received\n");
		return -1;
	}
	return STR_EQ(node->uri.host, cmpnode->uri.host) &&
	       STR_EQ(node->uri.port, cmpnode->uri.port);
}

inline dmq_node_t* find_dmq_node(dmq_node_list_t* list, dmq_node_t* node) {
	dmq_node_t* cur = list->nodes;
	while(cur) {
		if(cmp_dmq_node(cur, node)) {
			return cur;
		}
		cur = cur->next;
	}
	return NULL;
}

inline dmq_node_t* shm_dup_node(dmq_node_t* node) {
	dmq_node_t* newnode = shm_malloc(sizeof(dmq_node_t));
	memcpy(newnode, node, sizeof(dmq_node_t));
	shm_str_dup(&newnode->orig_uri, &node->orig_uri);
	if(parse_uri(newnode->orig_uri.s, newnode->orig_uri.len, &newnode->uri) < 0) {
		LM_ERR("error in parsing node uri\n");
		goto error;
	}
	return newnode;
error:
	shm_free(newnode->orig_uri.s);
	shm_free(newnode);
	return NULL;
}

inline void shm_free_node(dmq_node_t* node) {
	shm_free(node->orig_uri.s);
	shm_free(node);
}

inline int del_dmq_node(dmq_node_list_t* list, dmq_node_t* node) {
	dmq_node_t *cur, **prev;
	lock_get(&list->lock);
	cur = list->nodes;
	prev = &list->nodes;
	while(cur) {
		if(cmp_dmq_node(cur, node)) {
			*prev = cur->next;
			shm_free_node(cur);
			lock_release(&list->lock);
			return 1;
		}
		prev = &cur->next;
		cur = cur->next;
	}
	lock_release(&list->lock);
	return 0;
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
	shm_free(newnode->orig_uri.s);
	shm_free(newnode);
	return NULL;
}