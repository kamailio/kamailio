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

#ifdef _OBSOLETED

#include "parse_hostport.h"
#include <string.h>    /* strlen */
#include "../dprint.h"
#include "../ut.h"     /* str2s */

char* parse_hostport(char* buf, str* host, short int* port)
{
	char *tmp;
	int err;

	host->s=buf;
	for(tmp=buf;(*tmp)&&(*tmp!=':');tmp++);
	host->len=tmp-buf;
	if (*tmp==0) {
		*port=0;
	} else {
		*tmp=0;
		*port=str2s((unsigned char*)(tmp+1), strlen(tmp+1), &err);
		if (err ){
			LOG(L_INFO, 
			    "ERROR: hostport: trailing chars in port number: %s\n",
			    tmp+1);
			     /* report error? */
		}
	}

	return host->s;
}

#endif
