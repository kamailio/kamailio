/*
 * $Id$
 *
 * Copyright (c) 2004 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "../mem/mem.h"
#include "parse_allow.h"
#include "parse_methods.h"
#include "msg_parser.h"

 
/*
 * This method is used to parse Allow header.
 *
 * params: msg : sip msg
 * returns 0 on success,
 *        -1 on failure.
 */
int parse_allow_header(struct hdr_field* _hf)
{
	unsigned int* methods;
	
	if (!_hf) {
		LOG(L_ERR, "parse_allow_header: Invalid parameter value\n");
		return -1;
	}
	
	     /* maybe the header is already parsed! */
 	if (_hf->parsed) {
 		return 0;
	}

	     /* bad luck! :-( - we have to parse it */
	methods = pkg_malloc(sizeof(unsigned int));
 	if (methods == 0) {
 		LOG(L_ERR, "ERROR:parse_allow_header: Out of pkg_memory\n");
 		return -1;
 	}

	if (!parse_methods(&(_hf->body), methods)) {
 		LOG(L_ERR, "ERROR:parse_allow_header: Bad allow header\n"); 
 		pkg_free(methods);
		return -1;
 	}

 	_hf->parsed = methods;
 	return 0;
}


/*
 * Release memory
 */
void free_allow(unsigned int** _methods)
{
	if (_methods && *_methods) pkg_free(*_methods);
	*_methods = 0;
}
