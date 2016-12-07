/*
 * $Id: pua_bla.h 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_bla module - pua Bridged Line Appearance
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
 * History:
 * --------
 *  2007-03-30  initial version (anca)
 */

#ifndef PUA_BLA_H
#define PUA_BLA_H

#include "../pua/pua_bind.h"

extern int is_bla_aor;
extern str header_name;
extern str bla_outbound_proxy;
extern str server_address;
extern str reg_from_uri;

extern send_publish_t pua_send_publish;
extern send_subscribe_t pua_send_subscribe;
extern query_dialog_t pua_is_dialog;
extern int bla_handle_notify(struct sip_msg* msg, char* s1, char* s2);

#endif
