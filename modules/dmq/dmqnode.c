/*
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../ut.h"
#include "../../resolve.h"
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

/**
 * @brief get the string status of the node
 */
str* get_status_str(int status)
{
	switch(status) {
		case DMQ_NODE_ACTIVE: {
			return &dmq_node_active_str;
		}
		case DMQ_NODE_DISABLED: {
			return &dmq_node_disabled_str;
		}
		case DMQ_NODE_TIMEOUT: {
			return &dmq_node_timeout_str;
		}
		default: {
			return 0;
		}
	}
}

/**
 * @brief initialize dmg node list
 */
dmq_node_list_t* init_dmq_node_list()
{
	dmq_node_list_t* node_list;
	node_list = shm_malloc(sizeof(dmq_node_list_t));
	if(node_list==NULL) {
		LM_ERR("no more shm\n");
		return NULL;
	}
	memset(node_list, 0, sizeof(dmq_node_list_t));
	lock_init(&node_list->lock);
	return node_list;
}

/**
 * @brief compare dmq node addresses
 */
int cmp_dmq_node(dmq_node_t* node, dmq_node_t* cmpnode)
{
	if(!node || !cmpnode) {
		LM_ERR("cmp_dmq_node - null node received\n");
		return -1;
	}
	return STR_EQ(node->uri.host, cmpnode->uri.host) &&
	       STR_EQ(node->uri.port, cmpnode->uri.port);
}

/**
 * @brief get the value of a parameter
 */
str* get_param_value(param_t* params, str* param)
{
	while (params) {
		if ((params->name.len == param->len) &&
		    (strncmp(params->name.s, param->s, param->len) == 0)) {
			return &params->body;
		}
		params = params->next;
	}
	return NULL;
}

/**
 * @brief set the parameters for the node
 */
int set_dmq_node_params(dmq_node_t* node, param_t* params)
{
	str* status;
	if(!params) {
		LM_DBG("no parameters given\n");
		return 0;
	}
	status = get_param_value(params, &dmq_node_status_str);
	if(status) {
		if(STR_EQ(*status, dmq_node_active_str)) {
			node->status = DMQ_NODE_ACTIVE;
		} else if(STR_EQ(*status, dmq_node_timeout_str)) {
			node->status = DMQ_NODE_TIMEOUT;
		} else if(STR_EQ(*status, dmq_node_disabled_str)) {
			node->status = DMQ_NODE_DISABLED;
		} else {
			LM_ERR("invalid status parameter: %.*s\n", STR_FMT(status));
			goto error;
		}
	}
	return 0;
error:
	return -1;
}

/**
 * @brief set default node params
 */
int set_default_dmq_node_params(dmq_node_t* node)
{
	node->status = DMQ_NODE_ACTIVE;
	return 0;
}

/**
 * @brief build a dmq node
 */
dmq_node_t* build_dmq_node(str* uri, int shm) {
	dmq_node_t* ret = NULL;
	param_hooks_t hooks;
	param_t* params;

        /* For DNS-Lookups */
        static char hn[256];
        struct hostent* he;

	LM_DBG("build_dmq_node %.*s with %s memory\n", STR_FMT(uri), shm?"shm":"private");
	
	if(shm) {
		ret = shm_malloc(sizeof(dmq_node_t));
		if(ret==NULL) {
			LM_ERR("no more shm\n");
			goto error;
		}
		memset(ret, 0, sizeof(dmq_node_t));
		if(shm_str_dup(&ret->orig_uri, uri)<0) {
			goto error;
		}
	} else {
		ret = pkg_malloc(sizeof(dmq_node_t));
		if(ret==NULL) {
			LM_ERR("no more pkg\n");
			goto error;
		}
		memset(ret, 0, sizeof(dmq_node_t));
		if(pkg_str_dup(&ret->orig_uri, uri)<0) {
			goto error;
		}
	}
	set_default_dmq_node_params(ret);
	if(parse_uri(ret->orig_uri.s, ret->orig_uri.len, &ret->uri) < 0 || ret->uri.host.len > 254) {
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
	/* resolve hostname */
	strncpy(hn, ret->uri.host.s, ret->uri.host.len);
	hn[ret->uri.host.len]='\0';
	he=resolvehost(hn);
	if (he==0) {
		LM_ERR("could not resolve %.*s\n", ret->uri.host.len, ret->uri.host.s);
		goto error;
	}
	hostent2ip_addr(&ret->ip_address, he, 0);

	return ret;

error:
	if(ret!=NULL) {
		destroy_dmq_node(ret, shm);
	}
	return NULL;
}

/**
 * @brief find dmq node by uri
 */
dmq_node_t* find_dmq_node_uri(dmq_node_list_t* list, str* uri)
{
	dmq_node_t *ret, find;

	memset(&find, 0, sizeof(find));
	if(parse_uri(uri->s, uri->len, &find.uri) < 0) {
		LM_ERR("error parsing uri\n");
		return NULL;
	}
	ret = find_dmq_node(list, &find);
	return ret;
}

dmq_node_t* find_dmq_node_uri2(str* uri)
{
	return find_dmq_node_uri(node_list, uri);
}

/**
 * @brief destroy dmq node
 */
void destroy_dmq_node(dmq_node_t* node, int shm)
{
	if(shm) {
		if (node->params!=NULL)
			shm_free_params(node->params);
		shm_free_node(node);
	} else {
		if (node->params!=NULL)
			free_params(node->params);
		pkg_free_node(node);
	}
}

/**
 * @brief find dmq node
 */
dmq_node_t* find_dmq_node(dmq_node_list_t* list, dmq_node_t* node)
{
	dmq_node_t* cur = list->nodes;
	while(cur) {
		if(cmp_dmq_node(cur, node)) {
			return cur;
		}
		cur = cur->next;
	}
	return NULL;
}

/**
 * @brief duplicate dmq node
 */
dmq_node_t* shm_dup_node(dmq_node_t* node)
{
	dmq_node_t* newnode;
	if (!node) {
		LM_ERR("node is null\n");
		return NULL;
	}
	if (!node->orig_uri.s) {
		LM_ERR("nod->orig_uri.s is null\n");
		return NULL;
	}

	newnode = shm_malloc(sizeof(dmq_node_t));
	if(newnode==NULL) {
		LM_ERR("no more shm\n");
		return NULL;
	}
	memcpy(newnode, node, sizeof(dmq_node_t));
	newnode->orig_uri.s = NULL;
	if(shm_str_dup(&newnode->orig_uri, &node->orig_uri)<0) {
		goto error;
	}
	if(parse_uri(newnode->orig_uri.s, newnode->orig_uri.len,
				&newnode->uri) < 0) {
		LM_ERR("error in parsing node uri\n");
		goto error;
	}
	return newnode;
error:
	destroy_dmq_node(newnode, 1);
	return NULL;
}

/**
 * @brief free shm dmq node
 */
void shm_free_node(dmq_node_t* node)
{
	if (node->orig_uri.s!=NULL) 
		shm_free(node->orig_uri.s);
	shm_free(node);
}

/**
 * @brief free pkg dmq node
 */
void pkg_free_node(dmq_node_t* node)
{
	if (node->orig_uri.s!=NULL) 
		pkg_free(node->orig_uri.s);
	pkg_free(node);
}

/**
 * @brief delete dmq node
 */
int del_dmq_node(dmq_node_list_t* list, dmq_node_t* node)
{
	dmq_node_t *cur, **prev;
	lock_get(&list->lock);
	cur = list->nodes;
	prev = &list->nodes;
	while(cur) {
		if(cmp_dmq_node(cur, node)) {
			*prev = cur->next;
			destroy_dmq_node(cur, 1);
			lock_release(&list->lock);
			return 1;
		}
		prev = &cur->next;
		cur = cur->next;
	}
	lock_release(&list->lock);
	return 0;
}

/**
 * @brief add dmq node
 */
dmq_node_t* add_dmq_node(dmq_node_list_t* list, str* uri)
{
	dmq_node_t* newnode;
	
	newnode = build_dmq_node(uri, 1);
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

/**
 * @brief build dmq node string
 */
int build_node_str(dmq_node_t* node, char* buf, int buflen) {
	/* sip:host:port;status=[status] */
	int len = 0;
	if(buflen < node->orig_uri.len + 32) {
		LM_ERR("no more space left for node string\n");
		return -1;
	}
	memcpy(buf + len, "sip:", 4);
	len += 4;
	memcpy(buf + len, node->uri.host.s, node->uri.host.len);
	len += node->uri.host.len;
	memcpy(buf + len, ":", 1);
	len += 1;
	memcpy(buf + len, node->uri.port.s, node->uri.port.len);
	len += node->uri.port.len;
	memcpy(buf + len, ";", 1);
	len += 1;
	memcpy(buf + len, "status=", 7);
	len += 7;
	memcpy(buf + len, get_status_str(node->status)->s,
			get_status_str(node->status)->len);
	len += get_status_str(node->status)->len;
	return len;
}

