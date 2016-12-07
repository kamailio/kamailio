/*
 * rls module - resource list server
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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

#ifndef RLS_SUBSCRIBE_H
#define RLS_SUBSCRIBE_H

#include <libxml/parser.h>
#include "../../parser/msg_parser.h"

int rls_handle_subscribe0(struct sip_msg* msg);
int w_rls_handle_subscribe(struct sip_msg* msg, char* watcher_uri);
int rls_handle_subscribe(struct sip_msg* msg, str watcher_user, str watcher_domain);

#endif
