/*
 * $Id$
 *
 * Route & Record-Route module, loose routing support
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

/*! \brief
 * Do loose routing as per RFC3261
 */
int loose_route(struct sip_msg* _m, char* _s1, char* _s2);


/*! \brief
 * Check if the our route hdr has required params
 */
int check_route_param(struct sip_msg *msg, regex_t* re);


/*! \brief
 * Check the direction of the message
 */
int is_direction(struct sip_msg *msg, int dir);


/*! \brief
 * Gets the value of a route parameter
 */
int get_route_param( struct sip_msg *msg, str *name, str *val);

#endif /* LOOSE_H */
