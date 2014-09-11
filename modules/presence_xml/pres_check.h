/*
 * Copyright (C) 2011 Crocodile RCS Ltd.
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


#include <stdio.h>
#include "../../parser/msg_parser.h"
#include "../presence/bind_presence.h"
#include "../presence/event_list.h"

int presxml_check_basic(struct sip_msg* msg, str presentity_uri, str status);
int presxml_check_activities(struct sip_msg* msg, str presentity_uri, str activity);
contains_event_t pres_contains_event;
pres_get_presentity_t pres_get_presentity;
pres_free_presentity_t pres_free_presentity;
