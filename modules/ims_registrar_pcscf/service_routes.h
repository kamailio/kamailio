/** 
 * Functions to force or check the service-routes
 *
 * Copyright (c) 2012 Carsten Bock, ng-voice GmbH
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

#ifndef SERVICE_ROUTES_H
#define SERVICE_ROUTES_H

#include "../../parser/msg_parser.h"
#include "../ims_usrloc_pcscf/usrloc.h"

/**
 * Check, if a user-agent follows the indicated service-routes
 */
int check_service_routes(struct sip_msg* _m, udomain_t* _d);

/**
 * Force Service routes (upon request)
 */
int force_service_routes(struct sip_msg* _m, udomain_t* _d);

/**
 * Check, if source is registered.
 */
int is_registered(struct sip_msg* _m, udomain_t* _d);

/**
 * Get the current asserted identity for the user
 */
str * get_asserted_identity(struct sip_msg* _m);

/**
 * Get the contact used during registration of this user
 */
str * get_registration_contact(struct sip_msg* _m);

/**
 * Assert a given identity of a user
 */
int assert_identity(struct sip_msg* _m, udomain_t* _d, str identity);

/**
 * Assert a given called identity of a user
 */
int assert_called_identity(struct sip_msg* _m, udomain_t* _d);

#endif /* SERVICE_ROUTES_H */
