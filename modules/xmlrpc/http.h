/*
 * $Id$
 *
 * Copyright (C) 2005 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * For a license to use the SER software under conditions other than those
 * described here, or to purchase support for this software, please contact
 * iptel.org by e-mail at the following addresses: info@iptel.org
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _XMLRPC_HTTP_H_
#define _XMLRPC_HTTP_H_

#include "../../parser/msg_parser.h"

#ifdef XMLRPC_SYSTEM_MALLOC
#include <stdlib.h>
#define mxr_malloc malloc
#define mxr_realloc realloc
#define mxr_free free
#else
#include "../../mem/mem.h"
#define mxr_malloc pkg_malloc
#define mxr_realloc pkg_realloc
#define mxr_free pkg_free
#endif

/*
 * Create a faked Via header field in HTTP requests
 */
int create_via(sip_msg_t* msg, char* s1, char* s2);


#endif
