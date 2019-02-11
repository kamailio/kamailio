/*
 * Contact header field body parser
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 *  2003-03-25 Adapted to use new parameter parser (janakj)
 */


#ifndef PARSE_CONTACT_H
#define PARSE_CONTACT_H

#include <stdio.h>
#include "../hf.h"
#include "../../str.h"
#include "../msg_parser.h"
#include "contact.h"


typedef struct contact_body {
	unsigned char star;    /* Star contact */
	contact_t* contacts;   /* List of contacts */
} contact_body_t;


/*
 * Parse contact header field body
 */
int parse_contact(struct hdr_field* _h);


/*
 * Free all memory
 */
void free_contact(contact_body_t** _c);


/*
 * Print structure, for debugging only
 */
void print_contact(FILE* _o, contact_body_t* _c);


/*
 * Contact header field iterator, returns next contact if any, it doesn't
 * parse message header if not absolutely necessary
 */
int contact_iterator(contact_t** c, struct sip_msg* msg, contact_t* prev);


#endif /* PARSE_CONTACT_H */
