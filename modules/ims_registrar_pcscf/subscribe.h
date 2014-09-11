/*
 * pua_reginfo module - Presence-User-Agent Handling of reg events
 *
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
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
 * * History:
 * ========
 * 
 * Nov 2013 Richard Good migrated pua_reginfo funtionality to ims_registrar_pcscf
 * 
 */

#ifndef SUBSCRIBE_H
#define SUBSCRIBE_H

#include "../../pvar.h"
#include "../../parser/msg_parser.h"




int reginfo_subscribe_real(struct sip_msg* msg, pv_elem_t* uri, str* service_routes, int expires);

#endif
