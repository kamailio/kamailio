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
#include "../presence/bind_presence.h"
#include "../xcap_client/xcap_functions.h"

typedef struct xcap_serv
{
	char *addr;
	unsigned int port;
	struct xcap_serv *next;
} xcap_serv_t;

extern sl_api_t slb;
extern presence_api_t psapi;

extern str pxml_xcap_table;
extern db1_con_t *pxml_db;
extern db_func_t pxml_dbf;
extern int pxml_force_active;
extern int pidf_manipulation;
extern int pxml_integrated_xcap_server;
extern xcap_serv_t *xs_list;
extern xcapGetNewDoc_t xcap_GetNewDoc;

extern unsigned int pxml_default_expires;

#endif
