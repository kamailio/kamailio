/* functions.h v 0.1 2003/1/20
 *
 * Header file for local functions
 *
 * Copyright (C) 2008 Juha Heinanen
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef LOCAL_FUNCTIONS_H
#define LOCAL_FUNCTIONS_H


#include "../../core/parser/msg_parser.h"


/*
 * Database functions
 */
int local_db_bind(const str* db_url);
int local_db_init(const str* db_url);
void local_db_close();
int local_db_ver(str* name, int version);


/*
 * Check if domain given by pseudo variable parameter is a SEMS domain and
 * if so, returns code of the SEMS service.
 */
int is_domain_sems(struct sip_msg* _msg, char* _sp, char* _s2);


/*
 * Replaces Request URI with a possible forwarding URI returned by
 * previous radius_does_uri_exist function call and appends a Diversion
 * header to the request.  The condition of the forwarding URI must match
 * the one given as argument.
 */
int forwarding(struct sip_msg* _m, char* _condition, char* _s2);


/*
 * Checks if the request will be diverted (forwarded or redirected)
 * on a condition.
 */
int diverting_on(struct sip_msg* _m, char* _condition, char* _str2);


/*
 * Checks if the Request URI has been redirected under the condition given
 * as argument.  Possible conditions are "unconditonal", "unavailable",
 * "busy" and "no-answer".  If so, replaces Request-URI with the redirection
 * URI obtained by preceding successful radius_does_uri_exist call.
 */
int redirecting(struct sip_msg* _m, char* _condition, char* _str2);


#endif /* LOCAL_FUNCTIONS_H */
