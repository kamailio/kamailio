/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2006-11-30  first version (lavinia)
 */


#include <string.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "xr_parser.h"
#include "xr_parser_lib.h"
#include "mi_xmlrpc.h"


/*
 * Convert in argument string each LFLF to CRLF and return length of
 * the string not including the terminating `\0' character.
 * This is a hack that is needed as long as Abyss XML-RPC server "normalizes"
 * CRLF to LF in XML-RPC strings. 
 */
int lflf_to_crlf_hack(char *s) {

    unsigned int len;

    len = 0;

    while (*s) {
	if (*(s + 1) && (*s == '\n') && *(s + 1) == '\n') {
	    *s = '\r';
	    s = s + 2;
	    len = len + 2;
	} else {
	    s++;
	    len++;
	}
    }

    return len;
}


struct mi_root * xr_parse_tree( xmlrpc_env * env, xmlrpc_value * paramArray ) {

	struct mi_root * mi_root;
	
	int size, i;
	size_t length;
	void * cptrValue;
	xmlrpc_int32 intValue;
	xmlrpc_bool boolValue;
	double doubleValue;
	char * stringValue = 0, * byteStringValue = 0, * contents;
	xmlrpc_value * item;
	
	mi_root = init_mi_tree(0, 0, 0);
	
	if ( !mi_root ) {
		LM_ERR("the MI tree cannot be initialized!\n");
		goto error;
	}

	size = xmlrpc_array_size(env, paramArray);
	
	for (i=0 ; i< size ; i++) {

		item = xmlrpc_array_get_item(env, paramArray, i);
		if ( env->fault_occurred ) {
			LM_ERR("failed to get array item: %s\n", env->fault_string);
			goto error;
		}
		
		switch ( xmlrpc_value_type(item) ) {
		
		case (XMLRPC_TYPE_INT):
			
			validateType(env, item, XMLRPC_TYPE_INT);
			
			if ( env->fault_occurred ) {
				LM_ERR("failed to read the intValue: %s\n", env->fault_string);
				goto error;
			}
			
			intValue = item->_value.i;
			if (addf_mi_node_child(&mi_root->node,0,0,0,"%d",intValue)==NULL) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}
			break;

		case (XMLRPC_TYPE_BOOL):
			
			validateType(env, item, XMLRPC_TYPE_BOOL);
			
			if ( env->fault_occurred ) {
				LM_ERR("failed to read the boolValue: %s\n", env->fault_string);
				goto error;
			}
			
			boolValue = item->_value.b;
			if (addf_mi_node_child(&mi_root->node,0,0,0,"%u",boolValue)==NULL){
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}
			break;

		case (XMLRPC_TYPE_DOUBLE):
			
			validateType(env, item, XMLRPC_TYPE_DOUBLE);
			
			if ( env->fault_occurred ) {
				LM_ERR("failed to read the doubleValue:%s\n",env->fault_string);
				goto error;
			}

			doubleValue = item->_value.d;
			if ( addf_mi_node_child(&mi_root->node, 0, 0, 0, "%lf",
			doubleValue) == NULL ) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}
			break;

		case (XMLRPC_TYPE_STRING):
			
			#if HAVE_UNICODE_WCHAR
			char * stringValueW;
			xmlrpc_read_string_w(env, item, &stringValueW);
			
			if ( env->fault_occurred ) {
				LM_ERR("failed to read stringValueW: %s!\n", env->fault_string);
				goto error;
			}

			if ( add_mi_node_child(&mi_root->node, 0, 0, 0, stringValueW,
			strlen(stringValueW)) == NULL ) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}
			#else
			xmlrpc_read_string(env, item, &stringValue);
			
			if ( env->fault_occurred ) {
				LM_ERR("failed to read stringValue: %s!\n", env->fault_string);
				goto error;
			}
			if ( add_mi_node_child(&mi_root->node, 0, 0, 0,
					       stringValue,
					       lflf_to_crlf_hack(stringValue)) == NULL ) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}
			#endif
			
			break;

		case (XMLRPC_TYPE_BASE64):
			
			validateType(env, item, XMLRPC_TYPE_BASE64);
			
			if ( env->fault_occurred ) {
				LM_ERR("failed to read byteStringValue: %s!\n", 
						env->fault_string);
				goto error;
			}
			
			length = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, &item->_block);
			contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &item->_block);
			byteStringValue = pkg_malloc(length);
			
			if ( !byteStringValue ){
				xmlrpc_env_set_fault_formatted(env, XMLRPC_INTERNAL_ERROR,
					"Unable to allocate %u bytes for byte string.", length);
				LM_ERR("pkg_malloc cannot allocate any more memory!\n");
				goto error;
			} else
				memcpy(byteStringValue, contents, length);
			
			if ( add_mi_node_child(&mi_root->node, 0, 0, 0, byteStringValue,
			length) == NULL ) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}
			break;

		case (XMLRPC_TYPE_C_PTR):
			
			validateType(env, item, XMLRPC_TYPE_C_PTR);
			
			if ( env->fault_occurred ) {
				LM_ERR("failed to read the cptrValue: %s\n", env->fault_string);
				goto error;
			}

			cptrValue = item->_value.c_ptr;
			if ( add_mi_node_child(&mi_root->node, 0, 0, 0, (char*)cptrValue,
			strlen((char*)cptrValue)) == NULL ) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}
			break;
		}
	}
	
	return mi_root;

error:
	if ( mi_root ) free_mi_tree(mi_root);
	if ( byteStringValue ) pkg_free(byteStringValue);
	if ( stringValue ) pkg_free(stringValue);
	#if HAVE_UNICODE_WCHAR
		if ( stringValueW ) pkg_free(stringValueW);
	#endif
	return 0;
}


