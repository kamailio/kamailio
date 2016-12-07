/*
 * Copyright (c) 2004 Juha Heinanen
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
 * \brief Parser :: Allow header
 *
 * \ingroup parser
 */

 
#ifndef PARSE_ALLOW_H
#define PARSE_ALLOW_H
 
#include "hf.h"
#include "msg_parser.h"

 
/*! \brief
 * casting macro for accessing Allow body 
 */
#define get_allow_methods(p_msg)							\
	(((struct allow_body*)(p_msg)->allow->parsed)->allow_all)


struct allow_body {
	unsigned int allow;     /*!< allow mask for the current hdr */
	unsigned int allow_all; /*!< allow mask for the all allow hdr - it's
							 * set only for the first hdr in sibling
							 * list*/
};


/*! \brief
 * Parse all Allow HFs
 */
int parse_allow(struct sip_msg *msg);


/*! \brief
 * This method is used to parse Allow header.
 *
 * \param _hf message header field
 * \return 0 on success, -1 on failure.
 */
int parse_allow_header(struct hdr_field* _hf);


/*! \brief
 * Release memory
 */
void free_allow_body(struct allow_body **ab);

void free_allow_header(struct hdr_field* hf);



#endif /* PARSE_ALLOW_H */
