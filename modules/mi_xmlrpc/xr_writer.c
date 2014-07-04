/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * History:
 * ---------
 *  2006-11-30  first version (lavinia)
 */


#include <string.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "xr_writer.h"
#include "mi_xmlrpc.h"

static char *reply_buffer = 0;
static unsigned int reply_buffer_len = 0;
static xmlrpc_value* reply_item;

int xr_writer_init( unsigned int size )
{
	reply_buffer_len = size;

	reply_buffer = pkg_malloc(size);
	if(!reply_buffer){
		LM_ERR("pkg_malloc cannot allocate any more memory!\n");
		return -1;
	}
	
	return 0;
}

static int xr_write_node(str * buf, struct mi_node * node)
{
	char *end;
	char *p;
    struct mi_attr* attr;

	p = buf->s;
	end = buf->s + buf->len -1;

	/* name and value */
	if ( node->name.s != NULL ) {
		if ( p+node->name.len+3 > end )
			return -1;
		memcpy(p, node->name.s, node->name.len);
		p += node->name.len;
		*(p++) = ':';
		*(p++) = ':';
		*(p++) = ' ';
	}
	if ( node->value.s != NULL ) {
		if ( p+node->value.len > end )
			return -1;
		memcpy(p, node->value.s, node->value.len);
		p += node->value.len;
	}
	
	/* attributes */
	for( attr=node->attributes ; attr!=NULL ; attr=attr->next ) {
		if ( attr->name.s != NULL ) {
			if ( p+attr->name.len+2 > end )
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


	if ( p+1 > end )
		return -1;
	*(p++) = '\n';

	buf->len -= p-buf->s; 
	buf->s = p;
	return 0;
}

static int recur_build_response_array( xmlrpc_env * env, struct mi_node * tree, str * buf )
{
	for ( ; tree ; tree = tree->next ) {
		
		if ( xr_write_node( buf, tree ) != 0 ) {
			LM_ERR("failed to get MI node data!\n");
			return -1;
		}	

		reply_buffer[reply_buffer_len-buf->len] = 0;
		reply_item = xmlrpc_build_value(env, "s", reply_buffer);
		xmlrpc_array_append_item(env, xr_response, reply_item);
		
		buf->s = reply_buffer; 
		buf->len = reply_buffer_len;

		if ( tree->kids ) {
			if ( recur_build_response_array(env, tree->kids, buf) != 0 )
				return -1;
		}
	}
	return 0;
}

int xr_build_response_array( xmlrpc_env * env, struct mi_root * tree )
{
	str buf;
    
	buf.s = reply_buffer;
	buf.len = reply_buffer_len;

	/* test if mi root value is 200 OK (if not no point to continue) */
	if ( tree->code<200 || tree->code>=300 ){
		LM_DBG("command processing failure: %s\n", tree->reason.s); 
		if (tree->reason.s)
			xmlrpc_env_set_fault(env, tree->code, tree->reason.s);
		else
			xmlrpc_env_set_fault(env, tree->code, "Error");
		goto error;
	}
	
	if ( recur_build_response_array(env, (&tree->node)->kids, &buf) != 0 ) {
		LM_ERR("failed to read from the MI tree!\n");
		xmlrpc_env_set_fault(env, 500, "Failed to write reply");
		goto error;
	}

	return 0;

error:
	if ( reply_buffer ) pkg_free(reply_buffer);
	return -1;
}

static int recur_build_response( xmlrpc_env * env, struct mi_node * tree, str * buf )
{
   	for ( ; tree ; tree = tree->next ) {
		
		if ( xr_write_node( buf, tree ) != 0 ) {
		
			reply_buffer = (char*) pkg_realloc ( reply_buffer, 2*reply_buffer_len);

			if ( !reply_buffer ){
				LM_ERR("pkg_realloc cannot reallocate any more memory!\n");
				return -1;
			}

			buf->s = reply_buffer +(reply_buffer_len - buf->len);
			buf->len += reply_buffer_len;
			reply_buffer_len *=2 ;

			if ( xr_write_node( buf, tree ) != 0 ) {
				LM_ERR("failed to get MI node data!\n");
				return -1;
			}
		}

		if ( tree->kids ) {
			if ( recur_build_response(env, tree->kids, buf) != 0 )
				return -1;
		}
	}

	return 0;
}

char* xr_build_response( xmlrpc_env * env, struct mi_root * tree )
{
	str buf;
	
	buf.s = reply_buffer;
	buf.len = reply_buffer_len;

	if ( tree->code<200 || tree->code>=300 ){
		LM_DBG("command processing failure: %s\n", tree->reason.s); 
		if (tree->reason.s)
			xmlrpc_env_set_fault(env, tree->code, tree->reason.s);
		else
			xmlrpc_env_set_fault(env, tree->code, "Error");
		return 0;
	}
	
	if ( recur_build_response(env, (&tree->node)->kids, &buf) != 0 ) {
		LM_ERR("failed to read from the MI tree!\n");
		xmlrpc_env_set_fault(env, 500, "Failed to build reply");
		return 0;
	}
	
	reply_buffer[reply_buffer_len-buf.len] = 0;

	return reply_buffer;
}
