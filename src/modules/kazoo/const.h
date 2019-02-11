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

#ifndef DBK_CONST_H_
#define DBK_CONST_H_

#include "../../core/str.h"

extern str str_event_message_summary;
extern str str_event_dialog;
extern str str_event_presence;
extern str kz_app_name;

extern str str_username_col;
extern str str_domain_col;
extern str str_body_col;
extern str str_expires_col;
extern str str_received_time_col;
extern str str_presentity_uri_col;
extern str str_priority_col;

extern str str_event_col;
extern str str_contact_col;
extern str str_callid_col;
extern str str_from_tag_col;
extern str str_to_tag_col;
extern str str_etag_col;
extern str str_sender_col;

extern str str_presence_note_busy;
extern str str_presence_note_otp;
extern str str_presence_note_idle;
extern str str_presence_note_offline;
extern str str_presence_act_busy;
extern str str_presence_act_otp;
extern str str_presence_status_offline;
extern str str_presence_status_online;

extern str str_null_string;

extern char kz_json_escape_char;

extern int dbk_use_federated_exchange;
extern str dbk_federated_exchange;
extern str dbk_primary_zone_name;
extern int dbk_use_hearbeats;
extern int kz_cmd_pipe;
extern int kz_server_counter;
extern int kz_zone_counter;

#endif /* DBK_CONST_H_ */
