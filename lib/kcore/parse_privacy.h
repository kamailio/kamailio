/*
 * Copyright (c) 2006 Juha Heinanen
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

/*!
 * \file
 * \brief Privacy value parser
 * \ingroup parser
 */

#ifndef PARSE_PRIVACY_H
#define PARSE_PRIVACY_H

#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"


/*! bitmap of Privacy header privacy values
 * (http://www.iana.org/assignments/sip-priv-values)
 */
enum privacy_value {
	PRIVACY_USER=1,
	PRIVACY_HEADER=2,
	PRIVACY_SESSION=4,
	PRIVACY_NONE=8,
	PRIVACY_CRITICAL=16,
	PRIVACY_ID=32,
	PRIVACY_HISTORY=64
};


/*!
 * casting macro for accessing enumeration of priv-values
 */
#define get_privacy_values(p_msg) \
	((unsigned int)(long)((p_msg)->privacy->parsed))


/*!
 * This method is used to parse Privacy HF body, which consist of
 * comma separated list of priv-values.  After parsing, msg->privacy->parsed
 * contains enum bits of privacy values defined in parse_privacy.h.
 * \return 0 on success and -1 on failure. 
 */
int parse_privacy(struct sip_msg *msg);


/*!
 * Parse a privacy value pointed by start that can be at most max_len long.
 * \return length of matched privacy value on success or NULL otherwise
 */
unsigned int parse_priv_value(char* start, unsigned int max_len,
			      unsigned int* value);


#endif /* PARSE_PRIVACY_H */
