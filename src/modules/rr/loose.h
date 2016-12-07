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

/*!
 * \file
 * \brief Route & Record-Route module, loose routing support
 * \ingroup rr
 */


#ifndef LOOSE_H
#define LOOSE_H


#include <regex.h>
#include "../../str.h"
#include "../../parser/msg_parser.h"

#define RR_FLOW_DOWNSTREAM  (1<<0)
#define RR_FLOW_UPSTREAM    (1<<1)


/*!
 * \brief Do loose routing as per RFC3261
 * \param _m SIP message
 * \return -1 on failure, 1 on success
 */
int loose_route(struct sip_msg* _m);


/*!
 * \brief Check if the route hdr has the required parameter
 *
 * The function checks for the request "msg" if the URI parameters
 * of the local Route header (corresponding to the local server)
 * matches the given regular expression "re". It must be call
 * after the loose_route was done.
 *
 * \param msg SIP message request that will has the Route header parameters checked
 * \param re compiled regular expression to be checked against the Route header parameters
 * \return -1 on failure, 1 on success
 */
int check_route_param(struct sip_msg *msg, regex_t* re);


/*!
 * \brief Check the direction of the message
 *
 * The function checks the flow direction of the request "msg". As
 * for checking it's used the "ftag" Route header parameter, the
 * append_fromtag module parameter must be enables.
 * Also this must be call only after the loose_route is done.

 * \param msg SIP message request that will have the direction checked
 * \param dir direction to be checked against. It may be RR_FLOW_UPSTREAM or RR_FLOW_DOWNSTREAM
 * \return 0 if the request flow direction is the same as the given direction, -1 otherwise
 */
int is_direction(struct sip_msg *msg, int dir);


/*!
 * \brief Gets the value of a route parameter
 *
 * The function search in to the "msg"'s Route header parameters
 * the parameter called "name" and returns its value into "val".
 * It must be call only after the loose_route is done.
 *
 * \param msg - request that will have the Route header parameter searched
 * \param name - contains the Route header parameter to be serached
 * \param val returns the value of the searched Route header parameter if found.
 * It might be an empty string if the parameter had no value.
 * \return 0 if parameter was found (even if it has no value), -1 otherwise
 */
int get_route_param( struct sip_msg *msg, str *name, str *val);


#endif /* LOOSE_H */
