/*
** $Id$
**
** Copyright (C) 2001 by First Peer, Inc. All rights reserved.
** Copyright (C) 2001 by Eric Kidd. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */

/* History:
 * ---------
 *  2006-11-30  imported from xmlrpc project, file /xmlrpc-c/src/xmlrpc_data.c,
 *              version 1.06.06 (lavinia)
 *  2007-10-05  support for libxmlrpc-c3 version 1.x.x added (dragos)
 */


#ifdef XMLRPC_OLD_VERSION

#define XMLRPC_WANT_INTERNAL_DECLARATIONS
#include <xmlrpc.h>

static char * xmlrpc_typeName ( xmlrpc_type type ) {

    switch(type) {

    case XMLRPC_TYPE_INT:      return "INT";
    case XMLRPC_TYPE_BOOL:     return "BOOL";
    case XMLRPC_TYPE_DOUBLE:   return "DOUBLE";
    case XMLRPC_TYPE_DATETIME: return "DATETIME";
    case XMLRPC_TYPE_STRING:   return "STRING";
    case XMLRPC_TYPE_BASE64:   return "BASE64";
    case XMLRPC_TYPE_ARRAY:    return "ARRAY";
    case XMLRPC_TYPE_STRUCT:   return "STRUCT";
    case XMLRPC_TYPE_C_PTR:    return "C_PTR";
    case XMLRPC_TYPE_DEAD:     return "DEAD";
    default:                   return "Unknown";
    }
}

static void validateType ( xmlrpc_env * env, xmlrpc_value * value, xmlrpc_type expectedType ) {
	
    if ( value->_type != expectedType ) {
    	xmlrpc_env_set_fault_formatted(
            env, XMLRPC_TYPE_ERROR, "Value of type %s supplied where type %s was expected.", 
            xmlrpc_typeName(value->_type), xmlrpc_typeName(expectedType));
    }
}


static void verifyNoNulls ( xmlrpc_env * env, char * content, unsigned int len ) {

    unsigned int i;

    for ( i = 0 ; i < len && !env->fault_occurred ; i++ )
        if ( content[i] == '\0' )
            xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR, "String must not contain NULL characters");
}

static void accessStringValue ( xmlrpc_env * env, xmlrpc_value * value, size_t * length, char ** contents ) {
    
    validateType(env, value, XMLRPC_TYPE_STRING);

    if ( !env->fault_occurred ) {

        unsigned int size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, &value->_block);
        char * content = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &value->_block);
        unsigned int len = size - 1;
        /* The memblock has a null character added to the end */
		verifyNoNulls(env, content, len);
		*length = len;
        *contents = content;
    } else {
		*length = 0;
		*contents = NULL;
	}
}

static void xmlrpc_read_string( xmlrpc_env * env, xmlrpc_value * value, char ** stringValue ) {

	size_t length;
    char * contents, *str;

    accessStringValue(env, value, &length, &contents);

    if ( !env->fault_occurred ) {
                     
        str = (char*) pkg_malloc (length+1);
        if ( str == NULL ) {
            xmlrpc_env_set_fault_formatted(env, XMLRPC_INTERNAL_ERROR, 
					"Unable to allocate space for %u-character string", length);
			LM_ERR("pkg_malloc cannot allocate any more memory!\n");
		}
        else {
            memcpy(str, contents, length);
            str[length] = '\0';
			*stringValue = str;
        }
    }
}

#if HAVE_UNICODE_WCHAR

static void verifyNoNullsW( xmlrpc_env * env, wchar_t * contents, unsigned int len ) {

    unsigned int i;

    for (i = 0; i < len && !env->fault_occurred; i++)
        if ( contents[i] == '\0' )
            xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR, 
					"String must not contain NULL characters.");
}

static void accessStringValueW (xmlrpc_env * env, xmlrpc_value * value, size_t * length, wchar_t ** stringValueW ) {

    validateType(env, value, XMLRPC_TYPE_STRING);

    if ( !env->fault_occurred ) {
      
        if ( !env->fault_occurred ) {
            wchar_t * wcontents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(wchar_t, value->_wcs_block);
            size_t len = XMLRPC_TYPED_MEM_BLOCK_SIZE(wchar_t, value->_wcs_block) - 1;
            
            verifyNoNullsW(env, wcontents, len);
            *length = len;
            *stringValueW = wcontents;
        }
    }
}

static void xmlrpc_read_string_w ( xmlrpc_env * env, xmlrpc_value * value, wchar_t ** stringValue ) {

    size_t length;
    wchar_t * wcontents, * str;
    
    accessStringValueW(env, value, &length, &wcontents);

    if ( !env->fault_occurred ) {
		if ( !value->_wcs_block ) {
        	char * contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &value->_block);
        	size_t len = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, &value->_block) - 1;
        	value->_wcs_block = xmlrpc_utf8_to_wcs(env, contents, len + 1);
    	}

       	str = (wchar_t*) pkg_malloc (length + 1);
        if ( str == NULL ){
			xmlrpc_env_set_fault_formatted(env, XMLRPC_INTERNAL_ERROR, 
					"Unable to allocate space for %u-byte string", length);
			LM_ERR("pkg_malloc cannot allocate any more memory!\n");
		}
        else {
            memcpy(str, wcontents, length * sizeof(wchar_t));
            str[length] = '\0';
            *stringValue = str;
        }
    }
}
#endif
#endif
