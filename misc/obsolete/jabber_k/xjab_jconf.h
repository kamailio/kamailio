/*
 * $Id$
 *
 * eXtended JABber module
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


#ifndef _XJAB_JCONF_H_
#define _XHAB_JCONF_H_

#include "../../str.h"

#define XJ_JCONF_NULL		0
#define XJ_JCONF_READY		1
#define XJ_JCONF_WAITING	2
#define XJ_JCONF_AUTH		4

/**********             ***/

typedef struct _xj_jconf
{
	int jcid;
	int status;
	str uri;
	str room;
	str server;
	str nick;
	str passwd;
} t_xj_jconf, *xj_jconf;

xj_jconf xj_jconf_new(str *u);
int xj_jconf_init_sip(xj_jconf jcf, str *sid, char dl);
int xj_jconf_init_jab(xj_jconf jcf);

int xj_jconf_set_status(xj_jconf jcf, int s);

int xj_jconf_cmp(void *a, void *b);
int xj_jconf_free(xj_jconf jcf);
int xj_jconf_check_addr(str *addr, char dl);

#endif

