/*
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2014-2015 Sipwise GmbH, http://www.sipwise.com
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


#ifndef _RTPENGINE_FUNCS_H
#define _RTPENGINE_FUNCS_H

#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/contact.h"

int extract_body(struct sip_msg * , str *);
int check_content_type(struct sip_msg * );
int get_callid(struct sip_msg *, str *);
int get_to_tag(struct sip_msg *, str *);
int get_from_tag(struct sip_msg *, str *);
int get_contact_uri(struct sip_msg *, struct sip_uri *, contact_t **);
int get_via_branch(struct sip_msg *, int, str *);

#endif
