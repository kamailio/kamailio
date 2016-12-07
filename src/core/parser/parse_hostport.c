/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \brief Parser :: Parse domain/hostname and port argument
 *
 * \ingroup parser
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
