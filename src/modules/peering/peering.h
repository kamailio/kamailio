/* 
 * Radius based peering module .h file
 *
 * Copyright (C) 2008 Juha Heinanen
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

/*! \file
 * \ingroup peering
 * \brief Peering:: Core
 *
 * - Module: \ref peering
 */


#ifndef _PEERING_H_
#define _PEERING_H_

extern struct attr attrs[];
extern struct val vals[];
extern void *rh;

extern int verify_destination_service_type;
extern int verify_source_service_type;

#endif /* _PEERING_H_ */
