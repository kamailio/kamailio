/*
 * $Id$
 *
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

#ifndef _FLAT_URI_H
#define _FLAT_URI_H

/** \addtogroup flatstore
 * @{ 
 */

/** \file 
 * The functions parsing and interpreting flatstore: URIs.
 */

#include "../../lib/srdb2/db_uri.h"
#include "../../lib/srdb2/db_drv.h"

/** Flatstore driver specific payload to attach to db_uri structures.  
 * This is the flatstore specific structure that will be attached to generic
 * db_uri structures in the database API in SER. The structure is used to
 * convert relative pathnames in flatstore URIs to absolute.
 */
struct flat_uri {
	db_drv_t drv;
	/** Absolute pathname to the database directory, zero terminated */
    str path;  
};


/** Create a new flat_uri structure and convert the path in parameter.
 * This function builds a new flat_uri structure from the body of
 * the generic URI given to it in parameter.
 * @param uri A generic db_uri structure.
 * @retval 0 on success
 * @retval A negative number on error.
 */
int flat_uri(db_uri_t* uri);


/** @} */

#endif /* _FLAT_URI_H */
