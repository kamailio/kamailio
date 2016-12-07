/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*!
 * \file
 * \brief Kamailio RR module (record-routing) API
 *
 * Kamailio RR module (record-routing) API.
 * The RR module provides an internal API to be used by other
 * Kamailio modules. The API offers support for SIP dialog based
 * functionalities.

 * For internal(non-script) usage, the RR module offers to other
 * module the possibility to register callback functions to be
 * executed each time a local Route header is processed. The
 * callback function will receive as parameter the register
 * parameter and the Route header parameter string.
 * \ingroup rr
 */


#include "../../sr_module.h"
#include "loose.h"
#include "record.h"
#include "api.h"
#include "rr_cb.h"


/*! append from tag to record route */
extern int append_fromtag;


/*!
* \brief Function exported by module - it will load the other functions
 * \param rrb record-route API export binding
 * \return 1
 */
int load_rr( struct rr_binds *rrb )
{
	rrb->record_route_preset = record_route_preset;
	rrb->record_route_advertised_address = record_route_advertised_address;
	rrb->record_route      = record_route;
	rrb->loose_route       = loose_route;
	rrb->add_rr_param      = add_rr_param;
	rrb->check_route_param = check_route_param;
	rrb->is_direction      = is_direction;
	rrb->get_route_param   = get_route_param;
	rrb->register_rrcb     = register_rrcb;
	rrb->append_fromtag    = append_fromtag;

	return 1;
}
