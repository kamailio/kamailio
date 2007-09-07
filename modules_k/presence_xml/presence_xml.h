/*
 * $Id: presence_xml.h 2006-12-07 18:05:05Z anca_vamanu$
 *
 * presence_xml module - Presence Handling XML bodies module
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
 *  2007-04-18  initial version (anca)
 */

#ifndef _PRES_XML_H_
#define _PRES_XML_H_

#include "../../db/db.h"
#include "../sl/sl_api.h"
#include "../presence/event_list.h"
#include "../xcap_client/xcap_functions.h"

typedef struct xcap_serv
{
	char* addr;
	unsigned int port;
	struct xcap_serv* next;
}xcap_serv_t;

extern char *xcap_table;  
extern add_event_t pres_add_event;
extern db_con_t *pxml_db;
extern db_func_t pxml_dbf;
extern int force_active;
extern int pidf_manipulation;
extern int integrated_xcap_server;
extern xcap_serv_t* xs_list;
extern xcapGetNewDoc_t xcap_GetNewDoc;

/* SL bind */
extern struct sl_binds slb;

#endif
