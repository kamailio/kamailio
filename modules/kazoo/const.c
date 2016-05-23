/*
 * $Id$
 *
 * Kazoo module interface
 *
 * Copyright (C) 2010-2014 2600Hz
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
 *
 * History:
 * --------
 * 2014-08  first version (2600hz)
 */

#include "../../str.h"

str str_event_message_summary = str_init("message-summary");
str str_event_dialog = str_init("dialog");
str str_event_presence = str_init("presence");

str str_username_col = str_init("username");
str str_domain_col = str_init("domain");
str str_body_col = str_init("body");
str str_expires_col = str_init("expires");
str str_received_time_col = str_init("received_time");
str str_presentity_uri_col = str_init("presentity_uri");
str str_priority_col = str_init("priority");

str str_event_col = str_init("event");
str str_contact_col = str_init("contact");
str str_callid_col = str_init("callid");
str str_from_tag_col = str_init("from_tag");
str str_to_tag_col = str_init("to_tag");
str str_etag_col = str_init("etag");
str str_sender_col = str_init("sender");

str str_presence_note_busy = str_init("Busy");
str str_presence_note_otp = str_init("On the Phone");
str str_presence_note_idle = str_init("Idle");
str str_presence_note_offline = str_init("Offline");
str str_presence_act_busy = str_init("<rpid:busy/>");
str str_presence_act_otp = str_init("<rpid:on-the-phone/>");
str str_presence_status_offline = str_init("closed");
str str_presence_status_online = str_init("open");

str str_null_string = str_init("NULL");
