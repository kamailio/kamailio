/*
 * $Id$
 *
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

#ifndef _PARSE_CONTENT_H
#define _PARSE_CONTENT_H

#include "msg_parser.h"


struct mime_type {
	unsigned short type;
	unsigned short subtype;
};



/*
 * Mimes types/subtypes that are recognize
 */
#define TYPE_TEXT            1
#define TYPE_MESSAGE         2
#define TYPE_APPLICATION     3
#define TYPE_ALL             0xfe
#define TYPE_UNKNOWN         0xff

#define SUBTYPE_PLAIN        1
#define SUBTYPE_CPIM         2
#define SUBTYPE_SDP          3
#define SUBTYPE_CPLXML       4
#define SUBTYPE_ALL          0xfe
#define SUBTYPE_UNKNOWN      0xff


/*
 * Maximum number of mimes allowed in Accept header 
 */
#define MAX_MIMES_NR         128

/*
 * returns the content-length value of a sip_msg as an integer
 */
#define get_content_length(_msg_)   ((long)((_msg_)->content_length->parsed))


/*
 * returns the content-type value of a sip_msg as an integer
 */
#define get_content_type(_msg_)   ((int)(long)((_msg_)->content_type->parsed))


/*
 * returns the accept values of a sip_msg as an null-terminated array
 * of integer
 */
#define get_accept(_msg_) ((int*)((_msg_)->accept->parsed))

/*
 * parse the the body of the Content-Type header. It's value is also converted
 * as int.
 * Returns:   n (n>0)  : the found type
 *            0        : hdr not found
 *           -1        : error (parse error )
 */
int parse_content_type_hdr( struct sip_msg *msg);

/*
 * parse the the body of the Accept header. It's values are also converted
 * as an null-terminated array of ints.
 * Returns:   1 : OK
 *            0 : hdr not found
 *           -1 : error (parse error)
 */
int parse_accept_hdr( struct sip_msg *msg );


/*
 *  parse the body of a Content_-Length header. Also tryes to recognize the
 *  type specified by this header (see th above defines).
 *  Returns the first chr after the end of the header.
 */
char* parse_content_length( char* buffer, char* end, int* len);

#endif
