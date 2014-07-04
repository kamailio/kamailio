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
 * \brief MI Fifo :: Parser
 * \ingroup mi
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../../str.h"
#include "../../dprint.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "fifo_fnc.h"
#include "mi_fifo.h"
#include "mi_parser.h"

static char *mi_parse_buffer = 0;
static unsigned int mi_parse_buffer_len = 0;


int mi_parser_init( unsigned int size )
{
	mi_parse_buffer_len = size;
	mi_parse_buffer = pkg_malloc(size);

	if(!mi_parse_buffer){
		LM_ERR("pkg_malloc cannot allocate any more memory!\n");
		return -1;
	}

	return 0;
}



/* returns -1 = error
 *          0 = ok
 *          1 = end of stream
 */
static inline int mi_parse_node( FILE *stream, str *buf, str *name, str *value)
{
	char *p, *pmax;
	char *start;
	char *mark_nsp;
	int line_len;

	/* read one line */
	do {
		if (mi_read_line( buf->s, buf->len, stream, &line_len)<0) {
			LM_ERR("failed to read from fifo\n");
			return -1;
		}
		if (line_len == 1){
			LM_DBG("end of fifo input tree\n");
			return 1;
		}

		start = buf->s;
		pmax = buf->s + line_len - 1;

		/* remove leading spaces */
		for( ; start<pmax && isspace((int)*start) ; start++ );
	} while ( start==pmax );

	/* init */
	name->s = value->s = 0;
	name->len = value->len = 0;

	mark_nsp = 0;

	/* start parsing */
	if (*start!='"') {
		/* look for the atribute name */
		p = mark_nsp = start;
		while ( p!=pmax && ( (p[0]!=MI_ATTR_VAL_SEP1) || (p+1==pmax)
		|| p[1]!=MI_ATTR_VAL_SEP2) ) {
			if (!isspace((int)*p)) {
				if (*p=='"')
					goto parse_err;
				mark_nsp = p;
			}
			p++;
		}

		if (p!=pmax) {
			/* we have found the separator */
			if (p==start) {
				/* empty attr name */
			} else {
				name->s = start;
				name->len = mark_nsp - start + 1;
			}

			p += 2; /* for separator */

			LM_DBG("attr name <%.*s> found\n",
				name->len, name->s);

			/* consume the trailing spaces */
			for( ; p!=pmax && isspace((int)*p) ; p++);

			if (p==pmax) {
				/* empty value.....we are done */
				goto done;
			}

			/* value (only if not quoted ) */
			if (*p!='"') {
				for( start=p ; p!=pmax ; p++ ) {
					if (!isspace((int)*p)) {
						if (*p=='"')
							goto parse_err;
						mark_nsp = p;
					}
				}
				value->s = start;
				value->len = mark_nsp + 1  - start;
				goto done;
			}
			/* quoted value....continue */
		} else {
			/* we have an empty name ... and we read a non-quoted value */
			value->s = start;
			value->len = mark_nsp + 1 - start;
			goto done;
		}
	} else {
		p = start;
	}

	start = p+1;
	value->s = start;

	do {
		p = start;
		/* parse the buffer and look for " */
		while (p<pmax) {
			if (*p=='"') {
				if (start+1!=p && *(p-1)=='\\') {
					/* skip current char */
					memmove( p-1, p, pmax-p);
					pmax--;
				} else {
					/* end of value */
					value->len = p - value->s;
					/* is the line ending propely (only spaces) ? */
					for( p++ ; p!=pmax && isspace((int)*p) ; p++);
					if (p!=pmax)
						goto parse_err;
					/* found! */
					goto done;
				}
			} else {
				p++;
			}
		}

		/* adjust input buffer */
		p++;
		buf->len -= p - buf->s;
		buf->s = p;

		/*read one more line */
		if (mi_read_line( buf->s, buf->len, stream, &line_len)<0) {
			LM_ERR("failed to re-read from fifo\n");
			return -1;
		}
		if (line_len == 1) {
			LM_DBG("end of fifo input tree\n");
			return -2;
		}

		start = buf->s;
		pmax = buf->s + line_len - 1;
	} while(1);

done:
	buf->len -= p - buf->s;
	buf->s = p;
	return 0;
parse_err:
	LM_ERR("parse error around %c\n",*p);
	return -1;
}



struct mi_root * mi_parse_tree(FILE *stream) {
	struct mi_root *root;
	struct mi_node *node;
	str name;
	str value;
	str buf;
	int ret;

	buf.s = mi_parse_buffer;
	buf.len= mi_parse_buffer_len;

	root = init_mi_tree(0,0,0);
	if (!root) {
		LM_ERR("the MI tree cannot be initialized!\n");
		goto error;
	}
	node = &root->node;

	name.s = value.s = 0;
	name.len = value.len = 0;

	/* every tree for a command ends with a \n that is alone on its line */
	while ( (ret=mi_parse_node(stream, &buf, &name, &value))>=0 ) {
		if (ret==1)
			return root;

		LM_DBG("adding node <%.*s> ; val <%.*s>\n",
			name.len,name.s, value.len,value.s);

		if(!add_mi_node_child(node,0,name.s,name.len,value.s,value.len)){
			LM_ERR("cannot add the child node to the MI tree\n");
			goto error;
		}
	}

	LM_ERR("Parse error!\n");
	if (ret==-1) {
		/* consume the rest of the fifo request */
		do {
			mi_read_line(mi_parse_buffer,mi_parse_buffer_len,stream,&ret);
		}while(ret>1);
	}

error:
	if (root)
		free_mi_tree(root);
	return 0;
}


