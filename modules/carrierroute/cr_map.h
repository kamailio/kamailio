/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/**
 * \file cr_map.h
 * \brief Contains the functions to map domain and carrier names to ids.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_MAP_H
#define CR_MAP_H

#include "../../str.h"


/**
 * Tries to add a domain to the domain map. If the given domain doesn't
 * exist, it is added. Otherwise, nothing happens.
 *
 * @param domain the domain to be added
 *
 * @return values: on succcess the numerical index of the given domain,
 * -1 on failure
 */
int add_domain(const str * domain);


/**
 * Destroy the domain map by freeing its memory.
 */
void destroy_domain_map(void);


/**
 * Tries to add a carrier name to the carrier map. If the given carrier
 * doesn't exist, it is added. Otherwise, nothing happens.
 *
 * @param carrier_name the carrier name to be added
 * @param carrier_id the corresponding id
 *
 * @return values: on succcess the numerical index of the given carrier,
 * -1 on failure
 */
int add_carrier(const str * tree, int carrier_id);


/**
 * Searches for the ID for a Carrier-Name
 *
 * @param carrier_name the carrier name, we are looking for
 *
 * @return values: on succcess the id for this carrier name,
 * -1 on failure
 */
int find_carrier(const str * carrier_name);


/**
 * Destroy the carrier map by freeing its memory.
 */
void destroy_carrier_map(void);

#endif
