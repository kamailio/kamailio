/*
 * pua module - presence user agent module
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
#ifndef _PUA_ADD_EV_H_
#define _PUA_ADD_EV_H_

#include "send_publish.h"
#include "../../str.h"

/*
 *  should return:   0  if not changed ( fin_body points to publ->body)
 *                   1  if changed ( must be freed)	
 * */
int pua_add_events(void);

int pres_process_body(struct publ_info* publ, str** fin_body, int ver, str** tuple);
int bla_process_body (struct publ_info* publ, str** fin_body, int ver, str** tuple);
int mwi_process_body (struct publ_info* publ, str** fin_body, int ver, str** tuple);
int dlg_process_body (struct publ_info* publ, str** fin_body, int ver, str** tuple);
int reginfo_process_body (struct publ_info* publ, str** fin_body, int ver, str** tuple);

#endif
