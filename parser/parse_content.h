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

/*
 * Types for Content-Type header that are recognize
 */
#define CONTENT_TYPE_UNPARSED        0
#define CONTENT_TYPE_TEXT_PLAIN      1
#define CONTENT_TYPE_MESSAGE_CPIM    2
#define CONTENT_TYPE_APPLICATION_SDP 3
#define CONTENT_TYPE_UNKNOWN         0x7fff



/*
 * returns the content-length value of a sip_msg as an integer
 */
#define get_content_length(_msg_)   ((long)((_msg_)->content_length->parsed))


/*
 * returns the content-type value of a sip_msg as an integer
 */
#define get_content_type(_msg_)   ((long)((_msg_)->content_type->parsed))



/*
 * parse the the body of the Content-Type header. It's value is also converted
 * as int.
 * Returns:   n (n>0)  : the found type
 *           -1        : error (parse error or hdr not found)
 */
int parse_content_type_hdr( struct sip_msg *msg);


/*
 *  parse the body of a Content_-Length header. Also tryes to recognize the
 *  type specified by this header (see th above defines).
 *  Returns the first chr after the end of the header.
 */
char* parse_content_length( char* buffer, char* end, int* len);

#endif
