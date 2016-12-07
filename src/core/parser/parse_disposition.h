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
 *
 */

/*! \file
 * \brief Parser :: Content-Disposition header
 *
 * \ingroup parser
 */


#ifndef _PARSE_DISPOSITION_H_
#define _PARSE_DISPOSITION_H_

#include "../str.h"
#include "msg_parser.h"


#define get_content_disposition(_msg_) \
	((struct disposition*)((_msg_)->content_disposition->parsed))


struct disposition_param {
	str name;
	str body;
	int is_quoted;
	struct disposition_param *next;
};


struct disposition {
	str type;
	struct disposition_param *params;
};


/*! \brief looks inside the message, gets the Content-Disposition hdr, parse it, builds
 * and fills a disposition structure for it what will be attached to hdr as
 * parsed link.
 * Returns:  -1 : error
 *            0 : success
 *            1 : hdr not found
 */
int parse_content_disposition( struct sip_msg *msg );


/*! \brief parse a string that supposed to be a disposition and fills up the structure
 * Returns: -1 : error
 *           o : success */
int parse_disposition( str *s, struct disposition *disp);


/*! \brief Frees the entire disposition structure (params + itself) */
void free_disposition( struct disposition **disp);

/*! \brief Prints recursive a disposition structure */
void print_disposition( struct disposition *disp);

#endif

