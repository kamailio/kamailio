/*
 * $Id: mi_datagram.h 1133 2007-04-02 17:31:13Z ancuta_onofrei $
 *
 * Copyright (C) 2007 Voice Sistem SRL
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
 *  2007-06-25  first version (ancuta)
 */

/*!
 * \file
 * \brief MI_DATAGRAM :: Command parser
 * \ingroup mi
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../../str.h"
#include "../../dprint.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "datagram_fnc.h"
#include "mi_datagram.h"
#include "mi_datagram_parser.h"

/*static unsigned int mi_parse_buffer_len = 0;

int mi_datagram_parser_init( unsigned int size )
{
	mi_parse_buffer_len = size;

	return 0;
}*/



/*! \brief Parse MI command
		example: mi_parse_node(datagram, &buf, &name, &value)
 * returns -1 = error
 *          0 = ok
 *          1 = end of input
 */
static inline int mi_datagram_parse_node(datagram_stream * data, str *name, str *value)
{
	char *p, *pmax;
	char *start, *start1;
	char *mark_nsp;
	int newline_found = 0;

	LM_DBG("the remaining datagram to be parsed is %s and %i in length \n",
			data->current,data->len);

	p =data->current;
	start1 = start = p;
	if(data->len > DATAGRAM_SOCK_BUF_SIZE) {
		LM_ERR("overflow while parsing the received datagram\n");
		goto parse_err;
	}
	pmax  = start + data->len ;

	/* remove leading spaces */
	for( ; start<pmax && isspace((int)*start) ; start++ );
	/*no valuable data---end of input*/
	if(start == pmax)
		return 1;

	/* init */
	name->s = value->s = 0;
	name->len = value->len = 0;

	mark_nsp = 0;

	/* start parsing */
	if (*start!='"') {
		LM_DBG("the string is not just a quoted string\n");
		/* look for the atribute name */

		p = mark_nsp = start;
		while ( p!=pmax && (( *p!=MI_ATTR_VAL_SEP1) || p+1==pmax ||*(p+1)!=MI_ATTR_VAL_SEP2) ) {
			if (!isspace((int)*p)) {
				if (*p=='"') {
					LM_DBG("found \" before attr_separator\n");
					goto parse_err;
				}
				mark_nsp = p;
			}
			if(*p=='\n' && p!=(pmax -1)) {
				LM_DBG("found newline before attr_separator--we have just the "
					"attribute's value\n");
				mark_nsp++;
				pmax = ++p;
				break;
			}else if (p == (pmax-1)){
				mark_nsp++;
				pmax = ++p;
				LM_DBG("just a value, no new line");
				break;
			}
			p++;
		}

		if (p!=pmax) {
			/* we have found the separator */
			LM_DBG("we've found the attr_separator\n");
			if (p==start) {
				/* empty attr name */
				LM_DBG("empty attr_name\n");
			} else {
				name->s = start;
				name->len = mark_nsp - start+1;
				LM_DBG("attr name <%.*s> found\n",name->len, name->s);
			}

			p += 2; /* for separator */
				
			/* consume the trailing spaces */
			for( ; p!=pmax && isspace((int)*p) ; p++) {
				if(*p=='\n') {
					LM_DBG("empty value\n");
					/* empty value.....we are done */
					goto done;
				}
			}
			/*LM_DBG("p is %s case2\n",p);*/

			if(p==pmax && *p=='\n') {
				LM_DBG("empty value\n");
					/* empty value.....we are done */
				goto done;
			}
			/*LM_DBG("p is %s case1\n",p);*/
			/* value (only if not quoted ) */
			if (*p!='"') {
				LM_DBG("not quoted value, p is %c \n", *p);
				for( start=p ; p!=pmax ; p++ ) {
					if (!isspace((int)*p)) {
						if (*p=='"') {
							goto parse_err;
						}
						mark_nsp = p;
						LM_DBG("nsp is %p ,p is %p, pmax is %p and *p is %c\n",
							mark_nsp, p, pmax,*p);
					}
					if(*p=='\n') {	/*end of the node*/
						pmax = p;
						break;
					}
				}
			
				value->s = start;
				value->len = mark_nsp - start+1;
				LM_DBG("*start is %c and start is %p\n",*start, start);
				LM_DBG("attr value <%s> found\n"/*,value->len*/, value->s);
				goto done;
			}
			/* quoted value....continue */
		} else {
			/* we have an empty name ... and we read a non-quoted value */
			value->s = start;
			value->len = mark_nsp  - start;
			LM_DBG("empty name, attr not quoted value <%.*s> found\n",
					value->len, value->s);
			goto done;
		}
	} else {
		p = start; /*send the value only: as a quoted string*/
	}
	/*we have a quoted value*/
	LM_DBG("we have a  quoted value, %s\n", p);
	start = p+1; /*skip the first "*/
	value->s = start;

	p = start;
	/* parse the buffer and look for " */
	while (p<pmax) {
		if (*p=='"' && start!=p) { /*search the closing "*/

			LM_DBG("\" found p is %s\n",p);

			if (start+1!=p && *(p-1)=='\\') {
				LM_DBG("skipping %c",*p);
				/* skip current char */
				memmove( p-1, p, pmax-p);
				pmax--; 
			} else {
				LM_DBG("we have reached the end of attr value, p is %s\n", p);
				/* end of value */
				value->len = p - value->s;
				LM_DBG("attr value <%.*s> found\n",value->len, value->s);
				
				/* is the line ending propely (only spaces) ? */
				p++;
				for(; p!=pmax && isspace((int)*p) ; p++)
				{
					if(*p=='\n') {
						/*case : ""quoted string"  \n on a line */
						LM_DBG("line ended properly case1\n");
						pmax = p;
						break;
					}
				}
				if (p!=pmax )/*didn't find the second " on the current line*/
				{
					LM_ERR("didn't find newline case1 \n");
					goto parse_err;
				}
				newline_found = 1;
				/* found! */
				goto done;
			}
		} else {
			p++;
		}
	}

	if(p== pmax && !newline_found) {
		LM_ERR("didn't find newline case2\n");
		goto parse_err;
	}

done:
	/*set the current datagram's offset */
	LM_DBG("1 data->len is %i\n",data->len);
	data->len -= p-start1;
	LM_DBG("2 data->len is %i\n",data->len);
	data->current = p;
	return 0;
parse_err:
	LM_ERR("parse error around %c\n",*p);
	return -1;
}



/*! \brief parsing the datagram buffer*/
struct mi_root * mi_datagram_parse_tree(datagram_stream * datagram) {
	struct mi_root *root;
	struct mi_node *node;
	str name;
	str value;
	int ret;

	root = init_mi_tree(0,0,0);
	if (!root) {
		LM_ERR("the MI tree cannot be initialized!\n");
		goto error;
	}
	if(!datagram || datagram->current[0] == '\0')
	{
		LM_DBG("no data in the datagram\n");
		return root;
	}

	node = &root->node;

	name.s = value.s = 0;
	name.len = value.len = 0;

	/* every tree for a command ends with a \n that is alone on its line */
	while ((ret=mi_datagram_parse_node(datagram, &name, &value))>=0 ) {
		
		if(ret == 1)
			return root;
		LM_DBG("adding node <%.*s> ; val <%.*s>\n",
				name.len,name.s, value.len,value.s);

		if(!add_mi_node_child(node,0,name.s,name.len,value.s,value.len)){
			LM_ERR("cannot add the child node to the tree\n");
			goto error;
		}
		LM_DBG("the remaining datagram has %i bytes\n",datagram->len);
		/*end condition*/
		if(datagram->len == 0) {
			LM_DBG("found end of input\n");
			return root;
		}
	}

	LM_ERR("parse error!\n");
error:
	if (root)
		free_mi_tree(root);
	return 0;
}


