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

/** @file
 * @brief Parser :: Parse URI's
 *
 * @ingroup parser
 */

#ifndef PARSE_URI_H
#define PARSE_URI_H

/*
 * SIP URI parser
 */


#include "../str.h"
#include "../parser/msg_parser.h"

extern str	s_sip, s_sips, s_tel, s_tels, s_urn;

/* buf= pointer to begining of uri (sip:x@foo.bar:5060;a=b?h=i)
 * len= len of uri
 * returns: fills uri & returns <0 on error or 0 if ok 
 */
int parse_uri(char *buf, int len, struct sip_uri* uri);
int parse_sip_msg_uri(struct sip_msg* msg);
int parse_orig_ruri(struct sip_msg* msg);
int normalize_tel_user(char* res, str* src);
void uri_type_to_str(uri_type type, str *s);
void proto_type_to_str(unsigned short type, str *s);

#endif /* PARSE_URI_H */
