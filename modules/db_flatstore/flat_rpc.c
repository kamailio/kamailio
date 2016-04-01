/*
 * $Id$
 *
 * Copyright (C) 2004 FhG FOKUS
 * Copyright (C) 2008 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/** \addtogroup flatstore
 * @{ 
 */

/** \file 
 * Flatstore management interface
 */

#include "flat_rpc.h"
#include "flatstore_mod.h"
#include "km_flatstore_mod.h"

#include <time.h>

/** Register a new file rotation request.
 * This function can be called through the management interface in SER and it
 * will register a new file rotation request. This function only registers the
 * request, it will be carried out next time SER attempts to write new data
 * into the file.
 */
static void rotate(rpc_t* rpc, void* c)
{
	*km_flat_rotate = time(0);
	*flat_rotate = time(0);
}


static const char* flat_rotate_doc[2] = {
	"Close and reopen flatrotate files during log rotation.",
	0
};


rpc_export_t flat_rpc[] = {
	{"flatstore.rotate", rotate, flat_rotate_doc, 0},
	{0, 0, 0, 0},
};

/** @} */
