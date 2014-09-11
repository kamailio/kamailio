/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * History:
 * ---------
 *  2006-11-30  first version (lavinia)
 */




#ifndef _MI_XMLRPC_H_
#define _MI_XMLRPC_H_

#include <stdio.h>
#define XMLRPC_WANT_INTERNAL_DECLARATIONS
#include <xmlrpc.h>

extern xmlrpc_env env;
extern xmlrpc_value * xr_response;
extern int rpl_opt;

#define MAX_READ 8192

#endif /* _MI_XMLRPC_H_ */
