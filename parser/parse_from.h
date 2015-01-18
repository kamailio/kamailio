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
 * \brief Parser :: SIP From header parsing
 *
 * \ingroup parser
 */


#ifndef _PARSE_FROM_H
#define _PARSE_FROM_H

#include "parse_to.h"
#include "msg_parser.h"


/* casting macro for accessing From body */
#define get_from(p_msg)  ((struct to_body*)(p_msg)->from->parsed)

#define free_from(_to_body_)  free_to(_to_body_)

#define GET_FROM_PURI(p_msg) \
	(&((struct to_body*)(p_msg)->from->parsed)->parsed_uri)

/*
 * From header field parser
 */
int parse_from_header( struct sip_msg *msg);

sip_uri_t *parse_from_uri(sip_msg_t *msg);

#endif
