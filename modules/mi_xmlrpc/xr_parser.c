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
 *  2007-10-05  support for libxmlrpc-c3 version 1.x.x added (dragos)
 */


#include <string.h>
#include <stdlib.h>
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

	xmlrpc_int32 intValue;
	xmlrpc_bool boolValue;

	#ifdef XMLRPC_OLD_VERSION
	double doubleValue;
	char * contents;
	#else
	xmlrpc_double doubleValue;
	#endif

	char * stringValue = 0;
	char * byteStringValue =0;
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

			#ifdef XMLRPC_OLD_VERSION
			intValue = item->_value.i;
			#else 
			xmlrpc_read_int(env,item,&intValue);
			#endif

			if (addf_mi_node_child(&mi_root->node,0,0,0,"%d",intValue)==NULL) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}

			break;
		case (XMLRPC_TYPE_BOOL):

			#ifdef XMLRPC_OLD_VERSION
			boolValue = item->_value.b;
			#else
			xmlrpc_read_bool(env,item,&boolValue);
			#endif

			if (addf_mi_node_child(&mi_root->node,0,0,0,"%u",boolValue)==NULL){
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}

			break;

		case (XMLRPC_TYPE_DOUBLE):

			#ifdef XMLRPC_OLD_VERSION
			doubleValue = item->_value.d;
			#else
			xmlrpc_read_double(env,item,&doubleValue);
			#endif

			if ( addf_mi_node_child(&mi_root->node, 0, 0, 0, "%lf",
			doubleValue) == NULL ) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}

			break;

		case (XMLRPC_TYPE_STRING):

			#if HAVE_UNICODE_WCHAR
			
			#ifdef  XMLRPC_OLD_VERSION
			xmlrpc_read_string_w(env, item, &stringValue);
			#else
			xmlrpc_read_string_w(env, item , (const char **)&stringValue);
			#endif

			#else

			#ifdef  XMLRPC_OLD_VERSION
			xmlrpc_read_string(env, item, &stringValue);
			#else
			xmlrpc_read_string(env, item, (const char **)&stringValue);
			#endif

			#endif

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
			
			break;

		case (XMLRPC_TYPE_BASE64):

			#ifdef XMLRPC_OLD_VERSION

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

			#else

			xmlrpc_read_base64(env, item, &length,
				(const unsigned char **)(void*)&byteStringValue);

			if ( env->fault_occurred ) {
				LM_ERR("failed to read byteStringValue: %s!\n", 
						env->fault_string);
				goto error;
			}

			if ( add_mi_node_child(&mi_root->node, MI_DUP_VALUE, 0, 0, 
			byteStringValue, length) == NULL ) {
				LM_ERR("failed to add node to the MI tree.\n");
				goto error;
			}
			free(byteStringValue);

			#endif

			break;

		default :
			LM_ERR("unsupported node type %d\n",  xmlrpc_value_type(item)  );
			xmlrpc_env_set_fault_formatted( env, XMLRPC_TYPE_ERROR, 
				"Unsupported value of type %d supplied",
				xmlrpc_value_type(item));
			goto error;
		}
	}
	
	return mi_root;

error:
	if ( mi_root ) free_mi_tree(mi_root);
	if ( byteStringValue ) pkg_free(byteStringValue);
	return 0;
}
