/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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

/* 
 *  2003-01-21  added rport parsing code, contributed by
 *               Maxim Sobolev  <sobomax@FreeBSD.org>
 *  2003-01-21  added extra via param parsing code (i=...), used
 *               by tcp to identify the sending socket, by andrei
 */



#ifndef PARSE_VIA_H
#define PARSE_VIA_H

#include "../str.h"

/* via param types
 * WARNING: keep in sync with parse_via.c FIN_HIDDEN... 
 */
enum {
	PARAM_HIDDEN=230, PARAM_TTL, PARAM_BRANCH, 
	PARAM_MADDR, PARAM_RECEIVED, PARAM_RPORT, PARAM_I, GEN_PARAM,
	PARAM_ERROR
};



struct via_param {
	int type;               /* Type of the parameter */
	str name;               /* Name of the parameter */
	str value;              /* Value of the parameter */
	int size;               /* total size*/
	struct via_param* next; /* Next parameter in the list */
};


/* Format: name/version/transport host:port;params comment */
struct via_body { 
	int error;
	str hdr;   /* Contains "Via" or "v" */
	str name;
	str version;   
	str transport;
	int proto; /* transport */
	str host;
	int port;
	str port_str;
	str params;
	str comment;
	int bsize;                    /* body size, not including hdr */
	struct via_param* param_lst;  /* list of parameters*/
	struct via_param* last_param; /*last via parameter, internal use*/

	     /* shortcuts to "important" params*/
	struct via_param* branch;
	str tid; /* transaction id, part of branch */
	struct via_param* received;
	struct via_param* rport;
	struct via_param* i;
	
	struct via_body* next; /* pointer to next via body string if
				  compact via or null */
};


/*
 * Main Via header field parser
 */
char* parse_via(char* buffer, char* end, struct via_body *vb);


/*
 * Free allocated memory
 */
void free_via_list(struct via_body *vb);


#endif /* PARSE_VIA_H */
