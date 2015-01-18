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
 * \brief Parser :: Content
 *
 * \ingroup parser
 */


#ifndef _PARSE_CONTENT_H
#define _PARSE_CONTENT_H

#include "msg_parser.h"


struct mime_type {
	unsigned short type;
	unsigned short subtype;
};



/*! \name MimeTypes
 * Mimes types/subtypes that are recognized
 */
/*@{ */

#define TYPE_TEXT            1
#define TYPE_MESSAGE         2
#define TYPE_APPLICATION     3
#define TYPE_MULTIPART       4
#define TYPE_ALL             0xfe
#define TYPE_UNKNOWN         0xff

#define SUBTYPE_PLAIN        1
#define SUBTYPE_CPIM         2
#define SUBTYPE_SDP          3
#define SUBTYPE_CPLXML       4
#define SUBTYPE_PIDFXML      5
#define SUBTYPE_RLMIXML      6
#define SUBTYPE_RELATED      7
#define SUBTYPE_LPIDFXML     8
#define SUBTYPE_XPIDFXML     9
#define SUBTYPE_WATCHERINFOXML     10
#define SUBTYPE_EXTERNAL_BODY      11
#define SUBTYPE_XML_MSRTC_PIDF     12
#define SUBTYPE_CPIM_PIDFXML       13
#define SUBTYPE_MIXED              14
#define SUBTYPE_ISUP               15
#define SUBTYPE_ALL          0xfe
#define SUBTYPE_UNKNOWN      0xff

/*@} */

/*! \brief taken from PA module - will be useful here */
#define MIMETYPE(x_,y_) ((TYPE_##x_ << 16) | (SUBTYPE_##y_))

/*! \brief
 * Maximum number of mimes allowed in Accept header 
 */
#define MAX_MIMES_NR         128

/*! \brief
 * returns the content-length value of a sip_msg as an integer
 */
#define get_content_length(_msg_)   ((long)((_msg_)->content_length->parsed))


/*! \brief
 * returns the content-type value of a sip_msg as an integer
 */
#define get_content_type(_msg_)   ((int)(long)((_msg_)->content_type->parsed))


/*! \brief
 * returns the accept values of a sip_msg as an null-terminated array
 * of integer
 */
#define get_accept(_msg_) ((int*)((_msg_)->accept->parsed))

/*! \brief
 * parse the body of the Content-Type header. It's value is also converted
 * as int.
 * Returns:   n (n>0)  : the found type
 *            0        : hdr not found
 *           -1        : error (parse error )
 */
int parse_content_type_hdr(struct sip_msg* const msg);

int parse_accept_body(struct hdr_field* const hdr);

/*! \brief
 * parse the body of the Accept header. It's values are also converted
 * as an null-terminated array of ints.
 * Returns:   1 : OK
 *            0 : hdr not found
 *           -1 : error (parse error)
 */
int parse_accept_hdr(struct sip_msg* const msg);


/*! \brief
 *  parse the body of a Content_-Length header. Also tries to recognize the
 *  type specified by this header (see th above defines).
 *  Returns the first chr after the end of the header.
 */
char* parse_content_length(char* const buffer, const char* const end, int* const length);

/*! \brief
 * Sets the mime type from the body of a Content-Type header
 */
char* decode_mime_type(char* const start, const char* const end, unsigned int* const mime_type);

#endif
