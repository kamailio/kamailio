/*
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *  2007-04-18  initial version (anca)
 */

/*! \file
 * \brief Kamailio Presence_XML :: Core
 * \ref presence_xml.c
 * \ingroup presence_xml
 */


#ifndef _PRES_XML_H_
#define _PRES_XML_H_

#include "../../lib/srdb1/db.h"
#include "../../modules/sl/sl.h"
#include "../presence/event_list.h"
#include "../presence/presence.h"
#include "../presence/presentity.h"
#include "../xcap_client/xcap_functions.h"

typedef struct xcap_serv
{
	char* addr;
	unsigned int port;
	struct xcap_serv* next;
} xcap_serv_t;

extern sl_api_t slb;

extern str xcap_table;
extern add_event_t pres_add_event;
extern db1_con_t *pxml_db;
extern db_func_t pxml_dbf;
extern int force_active;
extern int pidf_manipulation;
extern int integrated_xcap_server;
extern xcap_serv_t* xs_list;
extern xcapGetNewDoc_t xcap_GetNewDoc;
extern pres_get_sphere_t pres_get_sphere;

#endif
