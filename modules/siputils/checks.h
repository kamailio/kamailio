/*
 * Various URI checks
 *
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
 *
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
 * Converts URI, if it is tel URI, to SIP URI.  Returns 1, if
 * conversion succeeded or if no conversion was needed, i.e., URI was not
 * tel URI.  Returns -1, if conversion failed.  Takes SIP URI hostpart from
 * second parameter and (if needed) writes the result to third paramater.
 */
int tel2sip(struct sip_msg* _msg, char* _uri, char* _hostpart, char* _res);


/*
 * Check if user part of URI in pseudo variable is an e164 number
 */
int w_is_uri_user_e164(struct sip_msg* _m, char* _sp, char* _s2);
int is_uri_user_e164(str *uri);

/*
 * Check if pseudo variable argument value is an e164 number
 */
int is_e164(struct sip_msg* _m, char* _sp, char* _s2);

/*
 * Set userpart of URI
 */
int set_uri_user(struct sip_msg* _m, char* _uri, char* _value);

/*
 * Set hostpart of URI
 */
int set_uri_host(struct sip_msg* _m, char* _uri, char* _value);

/*
 * Return true (1) if SIP message is request, otherwise false (-1)
 */
int w_is_request(struct sip_msg* msg, char *foo, char *bar);

/*
 * Return true (1) if SIP message is reply, otherwise false (-1)
 */
int w_is_reply(struct sip_msg* msg, char *foo, char *bar);

/*
 * Find if Request URI has a given parameter with matching value
 */
int get_uri_param(struct sip_msg* _msg, char* _param, char* _value);

/*
 * Check if parameter value has a telephone number format
 */
int is_tel_number(sip_msg_t *msg, char *_sp, char* _s2);

/*
 * Check if parameter value consists solely of decimal digits
 */
int is_numeric(sip_msg_t *msg, char *_sp, char* _s2);

#endif /* CHECKS_H */
