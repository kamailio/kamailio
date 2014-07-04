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


#ifndef _XR_WRITER_H_
#define _XR_WRITER_H_

#include <stdio.h>
#define XMLRPC_WANT_INTERNAL_DECLARATIONS
#include <xmlrpc.h>
#include "../../lib/kmi/mi.h"

int xr_writer_init( unsigned int size );
char * xr_build_response( xmlrpc_env * env, struct mi_root * tree );
int xr_build_response_array( xmlrpc_env * env, struct mi_root * tree );


#endif /* _XR_WRITER_H_ */
