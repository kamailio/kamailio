/*
 * $Id$
 *
 * XJAB module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#ifndef _XJAB_PRESENCE_H_
#define _XJAB_PRESENCE_H_

#include "../../str.h"
#include "xjab_base.h"

#define XJ_PS_CHECK			-1
#define XJ_PS_OFFLINE		0
#define XJ_PS_ONLINE		1
#define XJ_PS_TERMINATED	2
#define XJ_PS_REFUSED		3

#define XJ_PRES_STATUS_NULL		0
#define XJ_PRES_STATUS_SUBS		1
#define XJ_PRES_STATUS_WAIT		2

typedef struct _xj_pres_cell
{
	int key;
	str userid;
	int state;
	int status;
	pa_callback_f cbf;
	void *cbp;
	struct _xj_pres_cell *prev;
	struct _xj_pres_cell *next;
} t_xj_pres_cell, *xj_pres_cell;

typedef struct _xj_pres_list
{
	int nr;
	xj_pres_cell clist;
} t_xj_pres_list, *xj_pres_list;

xj_pres_cell xj_pres_cell_new(void);
void xj_pres_cell_free(xj_pres_cell);
void xj_pres_cell_free_all(xj_pres_cell);
int xj_pres_cell_init(xj_pres_cell, str*, pa_callback_f, void*);
int xj_pres_cell_update(xj_pres_cell, pa_callback_f, void*);

xj_pres_list xj_pres_list_init(void);
void xj_pres_list_free(xj_pres_list);
xj_pres_cell xj_pres_list_add(xj_pres_list, xj_pres_cell);
int xj_pres_list_del(xj_pres_list, str*);
xj_pres_cell xj_pres_list_check(xj_pres_list, str*);
void xj_pres_list_notifyall(xj_pres_list,int);

#endif

