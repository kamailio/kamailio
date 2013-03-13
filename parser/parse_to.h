/*
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

/*! \file
 * \brief Parser :: Parse To: header
 *
 * \ingroup parser
 */

#ifndef PARSE_TO
#define PARSE_TO

#include "../str.h"
#include "msg_parser.h"
#include "parse_addr_spec.h"

/* casting macro for accessing To body */
#define get_to(p_msg)      ((struct to_body*)(p_msg)->to->parsed)

#define GET_TO_PURI(p_msg) \
	(&((struct to_body*)(p_msg)->to->parsed)->parsed_uri)

/*! \brief
 * To header field parser
 */
char* parse_to(char* const buffer, const char* const end, struct to_body* const to_b);

int parse_to_header(struct sip_msg* const msg);

sip_uri_t *parse_to_uri(struct sip_msg* const msg);

#endif
