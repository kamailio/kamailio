/*
 * Sanity Checks Module
 *
 * Copyright (C) 2006 iptelorg GbmH
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

#ifndef SANITY_CHK_H
#define SANITY_CHK_H

#include "mod_sanity.h"

#define SIP_VERSION_TWO_POINT_ZERO "2.0"
#define SIP_VERSION_TWO_POINT_ZERO_LENGTH 3

/* check if the given string is a valid unsigned int value
 * and converts it into _result. returns -1 on error and 0 on success*/
int str2valid_uint(str* _number, unsigned int* _result);

/* parses the given comma seperated string into a string list */
strl* parse_str_list(str* _string);

/* compare the protocol string in the Via header with the transport */
int check_via_protocol(struct sip_msg* _msg);

/* check if the SIP version in the Via header is 2.0 */
int check_via_sip_version(struct sip_msg* _msg);

/* compare the Content-Length value with the accutal body length */
int check_cl(struct sip_msg* _msg);

/* compare the method in the CSeq header with the request line value */
int check_cseq_method(struct sip_msg* _msg);

/* check the number within the CSeq header */
int check_cseq_value(struct sip_msg* _msg);

/* check the number within the Expires header */
int check_expires_value(struct sip_msg* _msg);

/* check for the presence of the minimal required headers */
int check_required_headers(struct sip_msg* _msg);

/* check the content of the Proxy-Require header */
int check_proxy_require(struct sip_msg* _msg);

/* check the SIP version in the request URI */
int check_ruri_sip_version(struct sip_msg* _msg);

/* check if the r-uri scheme */
int check_ruri_scheme(struct sip_msg* _msg);

/* check if the typical URIs are parseable */
int check_parse_uris(struct sip_msg* _msg, int checks);

/* Make sure that username attribute in all digest credentials
 * instances has a meaningful value
 */
int check_digest(struct sip_msg* _msg, int checks);

/* check if there are duplicate tag params in From/To headers */
int check_duptags(sip_msg_t* _msg);

#endif /* SANITY_CHK_H */
