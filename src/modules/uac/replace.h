/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * UAC Kamailio-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC Kamailio-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#ifndef _UAC_REPLACE_H_
#define _UAC_REPLACE_H_

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../modules/tm/t_hooks.h"

#define UAC_NO_RESTORE      (0)
#define UAC_AUTO_RESTORE    (1)
#define UAC_MANUAL_RESTORE  (2)

void init_from_replacer(void);

int replace_uri( struct sip_msg *msg, str *display, str *uri,
					struct hdr_field *hdr, str *rr_param, str* restore_avp, int check_from);

int restore_uri( struct sip_msg *msg, str *rr_param, str* restore_avp, int check_from);

/* RR callback functions */
void rr_checker(struct sip_msg *msg, str *r_param, void *cb_param);


#endif
