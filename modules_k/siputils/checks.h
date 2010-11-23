/*
 * $Id$
 *
 * Various URI checks
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-03-26 created by janakj
 * 2004-04-14 uri_param and add_uri_param introduced (jih)
 */

/*!
 * \file
 * \brief SIP-utils :: Various URI checks and Request URI manipulation
 * \ingroup siputils
 * - Module; \ref siputils
 */



#ifndef CHECKS_H
#define CHECKS_H

#include "../../parser/msg_parser.h"


/*
 * Check if given username matches those in digest credentials
 */
int is_user(struct sip_msg* _msg, char* _user, char* _str2);


/*
 * Check if message includes a to-tag
 */
int has_totag(struct sip_msg* _m, char* _foo, char* _bar);


/*
 * Find if Request URI has a given parameter with no value
 */
int uri_param_1(struct sip_msg* _msg, char* _param, char* _str2);


/*
 * Find if Request URI has a given parameter with matching value
 */
int uri_param_2(struct sip_msg* _msg, char* _param, char* _value);


/*
 * Adds a new parameter to Request URI
 */
int add_uri_param(struct sip_msg* _msg, char* _param, char* _s2);


/*
 * Converts Request-URI, if it is tel URI, to SIP URI.  Returns 1, if
 * conversion succeeded or if no conversion was needed, i.e., Request-URI
 * was not tel URI.  Returns -1, if conversion failed.
 */
int tel2sip(struct sip_msg* _msg, char* _s1, char* _s2);


/*
 * Check if user part of URI in pseudo variable is an e164 number
 */
int is_uri_user_e164(struct sip_msg* _m, char* _sp, char* _s2);

/*
 * Check if pseudo variable argument value is an e164 number
 */
int is_e164(struct sip_msg* _m, char* _sp, char* _s2);

/*
 * Set userpart of URI
 */
int set_uri_user(struct sip_msg* _m, char* _uri, char* _value);

#endif /* CHECKS_H */
