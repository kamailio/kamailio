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

#ifndef PRESENTITY_H
#define PRESENTITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <time.h>
#include "../../str.h"
#include "../../parser/msg_parser.h" 
#include "event_list.h"
#include "presence.h"

extern char prefix;

typedef struct presentity
{
	int presid;
	str user;
	str domain;
	pres_ev_t* event;
	str etag;
	str* sender;
	time_t expires;
	time_t received_time;
} presentity_t;

/* create new presentity */
presentity_t* new_presentity( str* domain,str* user,int expires, 
 		pres_ev_t* event, str* etag, str* sender);

/* update presentity in database */
int update_presentity(struct sip_msg* msg,presentity_t* p,str* body,int t_new,
		int* sent_reply);

/* free memory */
void free_presentity(presentity_t* p);

char* generate_ETag(int publ_count);

int pres_htable_restore(void);

#endif

