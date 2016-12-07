/*
 * $Id: attr.c 4518 2008-07-28 15:39:28Z henningw $
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
 * \brief MI :: Attributes
 * \ingroup mi
 */


#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "../../dprint.h"
#include "mi_mem.h"
#include "attr.h"
#include "fmt.h"


extern char *mi_ap_buf;
extern int  mi_ap_buf_len;


struct mi_attr *add_mi_attr(struct mi_node *node, int flags,
						char *name, int name_len, char *value, int value_len)
{
	struct mi_attr *new, *p;
	int size_mem, name_pos, value_pos;

	if(!node)
		return NULL;

	if (!name) name_len=0;
	if (!name_len) name=0;
	if (!value) value_len=0;
	if (!value_len) value=0;

	if(!name && !value)
		return NULL;

	size_mem = sizeof(struct mi_attr);
	value_pos = name_pos = 0;

	if(name && (flags & MI_DUP_NAME)){
		name_pos = size_mem;
		size_mem += name_len;
	}
	if(value && (flags & MI_DUP_VALUE)){
		value_pos = size_mem;
		size_mem += value_len;
	}

	new = (struct mi_attr *)mi_malloc(size_mem);
	if (!new) {
		LM_ERR("no more pkg mem (%d)\n",size_mem);
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

	if(!(node->attributes)){
		new->next = NULL;
		return (node->attributes = new);
	}

	for(p = node->attributes ; p->next ; p = p->next);

	new->next = NULL;
	p->next = new;

	return new;
}



struct mi_attr *addf_mi_attr(struct mi_node *node, int flags,
							char *name, int name_len, char *fmt_val, ...)
{
	va_list ap;
	char *p;
	int  len = 0;

	va_start(ap, fmt_val);
	p = mi_print_fmt( fmt_val, ap, &len);
	va_end(ap);
	if (p==NULL)
		return 0;
	return add_mi_attr(node, flags|MI_DUP_VALUE, name, name_len, p, len);
}



struct mi_attr *get_mi_attr_by_name(struct mi_node *node, char *name, int len)
{
	struct mi_attr *head;

	if(!node || !name || !(node->attributes))
		return NULL;

	for(head = node->attributes ; head->next ; head = head->next)
		if(len == head->name.len 
		&& !strncasecmp(name, head->name.s, head->name.len))
			return head;

	return NULL;
}


void del_mi_attr_list(struct mi_node *node)
{
	struct mi_attr *p, *head;

	if(!node || !(node->attributes))
		return;

	for(head = node->attributes; head ;){
		p = head->next;
		mi_free(head);
		head = p;
	}

	node->attributes = NULL;
}

