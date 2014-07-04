/*
 * $Id$
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
 *  2006-09-25  first version (bogdan)
 */

/*!
 * \file
 * \brief MI Fifo :: Fifo writer
 * \ingroup mi
 */


#include <stdio.h>
#include <string.h>

#include "../../str.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "mi_fifo.h"
#include "fifo_fnc.h"
#include "mi_parser.h"


static char *mi_write_buffer = 0;
static unsigned int mi_write_buffer_len = 0;
static str  mi_fifo_indent;


int mi_writer_init( unsigned int size , char *indent)
{
	mi_write_buffer_len = size;

	mi_write_buffer = pkg_malloc(size);
	if(!mi_write_buffer){
		LM_ERR("pkg_malloc cannot allocate any more memory!\n");
		return -1;
	}

	if (indent==NULL || indent[0]==0 ) {
		mi_fifo_indent.s = 0;
		mi_fifo_indent.len = 0;
	} else {
		mi_fifo_indent.s = indent;
		mi_fifo_indent.len = strlen(indent);
	}

	return 0;
}


void mi_writer_destroy(void)
{
	pkg_free(mi_write_buffer);
}



static inline int mi_write_node(str *buf, struct mi_node *node, int level)
{
	struct mi_attr *attr;
	char *end;
	char *p;

	p = buf->s;
	end = buf->s + buf->len;

	/* write indents */
	if (mi_fifo_indent.s) {
		if (p + level*mi_fifo_indent.len>end)
			return -1;
		for( ; level>0 ; level-- ) {
			memcpy( p, mi_fifo_indent.s, mi_fifo_indent.len);
			p += mi_fifo_indent.len;
		}
	}

	/* name and value */
	if (node->name.s!=NULL) {
		if (p+node->name.len+3>end)
			return -1;
		memcpy(p,node->name.s,node->name.len);
		p += node->name.len;
		*(p++) = MI_ATTR_VAL_SEP1;
		*(p++) = MI_ATTR_VAL_SEP2;
		*(p++) = ' ';
	}
	if (node->value.s!=NULL) {
		if (p+node->value.len>end)
			return -1;
		memcpy(p,node->value.s,node->value.len);
		p += node->value.len;
	}

	/* attributes */
	for( attr=node->attributes ; attr!=NULL ; attr=attr->next ) {
		if (attr->name.s!=NULL) {
			if (p+attr->name.len+2>end)
				return -1;
			*(p++) = ' ';
			memcpy(p,attr->name.s,attr->name.len);
			p += attr->name.len;
			*(p++) = '=';
		}
		if (attr->value.s!=NULL) {
			if (p+attr->value.len>end)
				return -1;
			memcpy(p,attr->value.s,attr->value.len);
			p += attr->value.len;
		}
	}

	if (p+1>end)
		return -1;
	*(p++) = '\n';

	buf->len -= p-buf->s;
	buf->s = p;
	return 0;
}



static int recur_write_tree(FILE *stream, struct mi_node *tree, str *buf, int level)
{
	for( ; tree ; tree=tree->next ) {
		if (mi_write_node( buf, tree, level)!=0) {
			/* buffer is full -> write it and reset buffer */
			if (mi_fifo_reply( stream,"%.*s", buf->s-mi_write_buffer, mi_write_buffer)!=0)
				return -1;
			buf->s = mi_write_buffer;
			buf->len = mi_write_buffer_len;
			if (mi_write_node( buf, tree, level)!=0) {
				LM_ERR("failed to write MI tree - line too long!\n");
				return -1;
			}
		}
		if (tree->kids) {
			if (recur_write_tree(stream, tree->kids, buf, level+1)<0)
				return -1;
		}
	}
	return 0;
}



int mi_write_tree(FILE *stream, struct mi_root *tree)
{
	str buf;
	str code;

	buf.s = mi_write_buffer;
	buf.len = mi_write_buffer_len;

	/* write the root node */
	code.s = int2str((unsigned long)tree->code, &code.len);
	if (code.len+tree->reason.len+1>buf.len) {
		LM_ERR("failed to write - reason too long!\n");
		return -1;
	}
	memcpy( buf.s, code.s, code.len);
	buf.s += code.len;
	*(buf.s++) = ' ';
	if (tree->reason.len) {
		memcpy( buf.s, tree->reason.s, tree->reason.len);
		buf.s += tree->reason.len;
	}
	*(buf.s++) = '\n';
	buf.len -= code.len + 1 + tree->reason.len+1;

	if (recur_write_tree(stream, tree->node.kids, &buf, 0)!=0)
		return -1;

	if (buf.len<=0) {
		LM_ERR("failed to write - EOC does not fit in!\n");
		return -1;
	}
	*(buf.s++)='\n';
	buf.len--;

	if (mi_fifo_reply(stream,"%.*s",buf.s-mi_write_buffer,mi_write_buffer)!=0)
		return -1;

	return 0;
}
