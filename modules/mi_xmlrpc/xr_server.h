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
 *  2007-10-05  support for libxmlrpc-c3 version 1.x.x added (dragos)
 */


#ifndef _XR_SERVER_H_
#define _XR_SERVER_H_

#include <stdio.h>
#define XMLRPC_WANT_INTERNAL_DECLARATIONS
#include <xmlrpc.h>

#ifdef XMLRPC_OLD_VERSION

xmlrpc_value * default_method ( xmlrpc_env * env, char * host,
		 char * methodName, xmlrpc_value * paramArray, void * serverInfo );

#else

xmlrpc_value * default_method ( xmlrpc_env * env, const char * host,
		const char * methodName, xmlrpc_value * paramArray, void * serverInfo );

#endif
int set_default_method ( xmlrpc_env * env , xmlrpc_registry * registry);

int init_async_lock(void);

void destroy_async_lock(void);

#endif /* _XR_SERVER_H_ */
