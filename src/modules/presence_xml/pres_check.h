/*
 * Copyright (C) 2011 Crocodile RCS Ltd.
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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


#ifndef _PRES_CHECK_H_
#define _PRES_CHECK_H_

#include <stdio.h>
#include "../../core/parser/msg_parser.h"
#include "../presence/bind_presence.h"
#include "../presence/event_list.h"

int presxml_check_basic(sip_msg_t *msg, str presentity_uri, str status);
int presxml_check_activities(sip_msg_t *msg, str presentity_uri, str activity);

#endif
