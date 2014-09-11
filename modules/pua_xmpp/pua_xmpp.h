/*
 * $Id: pua_xmpp.h 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_xmpp module - presence SIP - XMPP Gateway
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
 *  2007-03-29  initial version (anca)
 */

#ifndef _XMMPP_TO_SIP_
#define _XMMPP_TO_SIP_

#include "../pua/pidf.h"
#include "../pua/pua_bind.h"
#include "../xmpp/xmpp_api.h"

extern str server_address;

extern send_subscribe_t pua_send_subscribe;
extern send_publish_t pua_send_publish;
extern query_dialog_t pua_is_dialog;

extern xmpp_send_xsubscribe_f xmpp_subscribe;
extern xmpp_send_xnotify_f xmpp_notify;
extern xmpp_send_xpacket_f xmpp_packet;
extern xmpp_translate_uri_f duri_sip_xmpp;
extern xmpp_translate_uri_f euri_sip_xmpp;
extern xmpp_translate_uri_f duri_xmpp_sip;
extern xmpp_translate_uri_f euri_xmpp_sip;


extern xmlNodeGetAttrContentByName_t XMLNodeGetAttrContentByName;
extern xmlDocGetNodeByName_t XMLDocGetNodeByName;
extern xmlNodeGetNodeByName_t XMLNodeGetNodeByName;
extern xmlNodeGetNodeContentByName_t XMLNodeGetNodeContentByName;

#endif
