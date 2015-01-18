/*
 * Copyright (c) 2007 iptelorg GmbH
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
 * \brief Parser :: Parse Identity-info header field
 *
 * \ingroup parser
 */

#ifndef PARSE_IDENTITYNFO
#define PARSE_IDENTITYNFO

#include "../str.h"
#include "msg_parser.h"

enum {
	II_START,
	II_URI_BEGIN,
	II_URI_DOMAIN,
	II_URI_IPV4,
	II_URI_IPV6,
	II_URI_PATH,
	II_URI_END,
	II_LWS,
	II_LWSCR,
	II_LWSCRLF,
	II_LWSCRLFSP,
	II_SEMIC,
	II_TAG,
	II_EQUAL,
	II_TOKEN,
	II_ENDHEADER
};

enum {
	II_M_START,
	II_M_URI_BEGIN,
	II_M_URI_END,
	II_M_SEMIC,
	II_M_TAG,
	II_M_EQUAL,
	II_M_TOKEN
};

#define ZSW(_c) ((_c)?(_c):"")

struct identityinfo_body {
	int error;  	/* Error code */
	str uri;    	/* URI */
	str domain; 	/* Domain part of the URI */
	str alg; 		/* Identity-Info header field MUST contain an 'alg' parameter */
};


/* casting macro for accessing IDENTITY-INFO body */
#define get_identityinfo(p_msg) ((struct identityinfo_body*)(p_msg)->identity_info->parsed)


/*
 * Parse Identity-Info header field
 */
void parse_identityinfo(char *buffer, char* end, struct identityinfo_body *ii_b);
int parse_identityinfo_header(struct sip_msg *msg);

/*
 * Free all associated memory
 */
void free_identityinfo(struct identityinfo_body *ii_b);


#endif
