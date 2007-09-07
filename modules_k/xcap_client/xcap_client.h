/*
 * $Id: xcap_client.h 2230 2007-06-06 07:13:20Z anca_vamanu $
 *
 * xcap_client module - XCAP client for openser
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *  2007-08-20  initial version (anca)
 */

#ifndef XCAP_CL_H
#define XCAP_CL_H

#include "../../db/db.h"
#include "xcap_callbacks.h"

extern xcap_callback_t* xcapcb_list;
extern str xcap_db_url;
extern char* xcap_db_table;

/* database connection */
extern db_con_t *xcap_db;
extern db_func_t xcap_dbf;

extern int periodical_query;
extern unsigned int query_period;

#endif
