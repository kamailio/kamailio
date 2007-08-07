/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
 *  2006-08-15  initial version (anca)
 */

#ifndef SUBSCRIBE_H
#define SUBSCRIBE_H

#include "presence.h"
#include "../../str.h"

struct ev;

#include "event_list.h"

struct subscription
{
	str pres_user;
	str pres_domain;
	str to_user;
	str to_domain;
	str from_user;
	str from_domain;
	struct ev* event;
	str event_id;
	str to_tag;
	str from_tag;
	str callid;
	str sockinfo_str;
	str local_contact;
	unsigned int cseq; 
	str contact;
	str record_route;
	unsigned int expires;
	str status;
	str reason;
	int version;
	int send_on_cback;
/* flag to check whether the notify for presence is sent on the callback of
 * the notify for wather info
 */
};
typedef struct subscription subs_t;

void msg_active_watchers_clean(unsigned int ticks,void *param);

void msg_watchers_clean(unsigned int ticks,void *param);

int handle_subscribe(struct sip_msg*, char*, char*);

#endif
