/*
 * $Id: tree.c 4518 2008-07-28 15:39:28Z henningw $
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * History:
 * ---------
 *  2006-09-08  first version (bogdan)
 */

/*!
 * \file 
 * \brief MI :: Tree 
 * \ingroup mi
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "../../dprint.h"
#include "mi_mem.h"
#include "tree.h"
#include "fmt.h"


static int use_shm = 0;

struct mi_root *init_mi_tree(unsigned int code, char *reason, int reason_len)
{
	struct mi_root *root;

	if (use_shm)
		root = (struct mi_root *)shm_malloc(sizeof(struct mi_root));
	else
		root = (struct mi_root *)mi_malloc(sizeof(struct mi_root));
	if (!root) {
		LM_ERR("no more pkg mem\n");
		return NULL;
	}

	memset(root,0,sizeof(struct mi_root));
	root->node.next = root->node.last = &root->node;

	if (reason && reason_len) {
		root->reason.s = reason;
		root->reason.len = reason_len;
	}
	root->code = code;

	return root;
}


static void free_mi_node(struct mi_node *parent)
{
	struct mi_node *p, *q;

	for(p = parent->kids ; p ; ){
		q = p;
		p = p->next;
		free_mi_node(q);
	}

	if (use_shm) {
		shm_free(parent);
	} else {
		del_mi_attr_list(parent);
		mi_free(parent);
	}
}

void free_mi_tree(struct mi_root *parent)
{
	struct mi_node *p, *q;

	for(p = parent->node.kids ; p ; ){
		q = p;
		p = p->next;
		free_mi_node(q);
	}

	if (use_shm)
		shm_free(parent);
	else
		mi_free(parent);
}


static inline struct mi_node *create_mi_node(char *name, int name_len,
									char *value, int value_len, int flags)
{
	struct mi_node *new;
	int size_mem;
	int name_pos;
	int value_pos;

	if (!name) name_len=0;
	if (!name_len) name=0;
	if (!value) value_len=0;
	if (!value_len) value=0;

	if (!name && !value)
		return NULL;

	size_mem = sizeof(struct mi_node);
	value_pos = name_pos = 0;

	if (name && (flags & MI_DUP_NAME)){
		name_pos = size_mem;
		size_mem += name_len;
	}
	if (value && (flags & MI_DUP_VALUE)){
		value_pos = size_mem;
		size_mem += value_len;
	}

	if (use_shm)
		new = (struct mi_node *)shm_malloc(size_mem);
	else
		new = (struct mi_node *)mi_malloc(size_mem);
	if(!new) {
		LM_ERR("no more pkg mem\n");
		return NULL;
	}
	memset(new,0,size_mem);

	if (name) {
		new->name.len = name_len;
		if(flags & MI_DUP_NAME){
			new->name.s = ((char *)new) + name_pos;
			strncpy(new->name.s, name, name_len);
		} else{
			new->name.s = name;
		}
	}

	if (value) {
		new->value.len = value_len;
		if(flags & MI_DUP_VALUE){
			new->value.s = ((char *)new) + value_pos;
			strncpy(new->value.s, value, value_len);
		}else{
			new->value.s = value;
		}
	}
	new->last = new;

	return new;
}


static inline struct mi_node *add_next(struct mi_node *brother,
			char *name, int name_len, char *value, int value_len, int flags)
{
	struct mi_node *new;

	if(!brother)
		return NULL;
	
	new = create_mi_node(name, name_len, value, value_len, flags);
	if(!new)
		return NULL;

	brother->last->next = new;
	brother->last = new;

	return new;
}


struct mi_node *add_mi_node_sibling( struct mi_node *brother, int flags,
						char *name, int name_len, char *value, int value_len)
{
	return add_next(brother, name, name_len, value, value_len, flags);
}


struct mi_node *addf_mi_node_sibling(struct mi_node *brother, int flags,
							char *name, int name_len, char *fmt_val, ...)
{
	va_list ap;
	char *p;
	int  len;

	va_start(ap, fmt_val);
	p = mi_print_fmt( fmt_val, ap, &len);
	va_end(ap);
	if (p==NULL)
		return 0;
	return add_mi_node_sibling( brother, flags|MI_DUP_VALUE,
		name, name_len, p, len);
}


struct mi_node *add_mi_node_child( struct mi_node *parent, int flags,
						char *name, int name_len, char *value, int value_len)
{
	if(parent->kids){
		return add_next(parent->kids, name, name_len, value, value_len, flags);
	}else{
		parent->kids = create_mi_node(name, name_len, value, value_len, flags);
		return parent->kids;
	}
}


struct mi_node *addf_mi_node_child(struct mi_node *parent, int flags,
							char *name, int name_len, char *fmt_val, ...)
{
	va_list ap;
	char *p;
	int  len;

	va_start(ap, fmt_val);
	p = mi_print_fmt( fmt_val, ap, &len);
	va_end(ap);
	if (p==NULL)
		return 0;
	return add_mi_node_child( parent, flags|MI_DUP_VALUE,
		name, name_len, p, len);
}


static int clone_mi_node(struct mi_node *org, struct mi_node *parent)
{
	struct mi_node *p, *q;

	for(p = org->kids ; p ; p=p->next){
		q = add_mi_node_child( parent, MI_DUP_VALUE|MI_DUP_NAME,
			p->name.s, p->name.len, p->value.s, p->value.len);
		if (q==NULL)
			return -1;
		if (clone_mi_node( p, q)!=0)
			return -1;
	}
	return 0;
}


struct mi_root* clone_mi_tree(struct mi_root *org, int shm)
{
	struct mi_root *root;

	use_shm = shm?1:0;

	root = init_mi_tree( org->code, org->reason.s, org->reason.len);
	if (root==NULL)
		goto done;

	if (clone_mi_node( &(org->node), &(root->node) )!=0 ) {
		free_mi_tree(root);
		root = NULL;
		goto done;
	}

done:
	use_shm=0;
	return root;
}



void free_shm_mi_tree(struct mi_root *parent)
{
	use_shm = 1;
	free_mi_tree(parent);
	use_shm = 0;
}
