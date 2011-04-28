#include "../../ut.h"
#include "dmqnode.h"
#include "dmq.h"

dmq_node_t* self_node;
dmq_node_t* notification_node;

/* name */
str dmq_node_status_str = str_init("status");
/* possible values */
str dmq_node_active_str = str_init("active");
str dmq_node_disabled_str = str_init("disabled");
str dmq_node_timeout_str = str_init("timeout");

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

static str* get_param_value(param_t* params, str* param) {
	while (params) {
		if ((params->name.len == param->len) &&
		    (strncmp(params->name.s, param->s, param->len) == 0)) {
			return &params->body;
		}
		params = params->next;
	}
	return NULL;
}

inline int set_dmq_node_params(dmq_node_t* node, param_t* params) {
	str* status;
	if(!params) {
		LM_DBG("no parameters given\n");
		return 0;
	}
	status = get_param_value(params, &dmq_node_status_str);
	if(status) {
		if(str_strcmp(status, &dmq_node_active_str)) {
			node->status = DMQ_NODE_ACTIVE;
		} else if(str_strcmp(status, &dmq_node_timeout_str)) {
			node->status = DMQ_NODE_ACTIVE;
		} else if(str_strcmp(status, &dmq_node_disabled_str)) {
			node->status = DMQ_NODE_ACTIVE;
		} else {
			LM_ERR("invalid status parameter: %.*s\n", STR_FMT(status));
			goto error;
		}
	}
	return 0;
error:
	return -1;
}

inline dmq_node_t* build_dmq_node(str* uri, int shm) {
	dmq_node_t* ret;
	param_hooks_t hooks;
	param_t* params;
	
	LM_DBG("build_dmq_node %.*s with %s memory\n", STR_FMT(uri), shm?"shm":"private");
	
	if(shm) {
		ret = shm_malloc(sizeof(*ret));
		memset(ret, 0, sizeof(*ret));
		shm_str_dup(&ret->orig_uri, uri);
	} else {
		ret = pkg_malloc(sizeof(*ret));
		memset(ret, 0, sizeof(*ret));
		pkg_str_dup(&ret->orig_uri, uri);
	}
	if(parse_uri(ret->orig_uri.s, ret->orig_uri.len, &ret->uri) < 0) {
		LM_ERR("error parsing uri\n");
		goto error;
	}
	/* if any parameters found, parse them */
	if(parse_params(&ret->uri.params, CLASS_ANY, &hooks, &params) < 0) {
		LM_ERR("error parsing params\n");
		goto error;
	}
	/* if any params found */
	if(params) {
		if(shm) {
			if(shm_duplicate_params(&ret->params, params) < 0) {
				LM_ERR("error duplicating params\n");
				free_params(params);
				goto error;
			}
			free_params(params);
		} else {
			ret->params = params;
		}
		if(set_dmq_node_params(ret, ret->params) < 0) {
			LM_ERR("error setting parameters\n");
			goto error;
		}
	} else {
		LM_DBG("no dmqnode params found\n");		
	}
	return ret;
error:
	return NULL;
}

inline dmq_node_t* find_dmq_node_uri(dmq_node_list_t* list, str* uri) {
	dmq_node_t *ret, *find;
	find =  build_dmq_node(uri, 0);
	ret = find_dmq_node(list, find);
	destroy_dmq_node(find, 0);
	return ret;
}

inline void destroy_dmq_node(dmq_node_t* node, int shm) {
	if(shm) {
		shm_free_node(node);
	} else {
		pkg_free_node(node);
	}
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

inline void pkg_free_node(dmq_node_t* node) {
	pkg_free(node->orig_uri.s);
	pkg_free(node);
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
	dmq_node_t* newnode = build_dmq_node(uri, 1);
	if(!newnode) {
		LM_ERR("error creating node\n");
		goto error;
	}
	LM_DBG("dmq node successfully created\n");
	lock_get(&list->lock);
	newnode->next = list->nodes;
	list->nodes = newnode;
	list->count++;
	lock_release(&list->lock);
	return newnode;
error:
	return NULL;
}