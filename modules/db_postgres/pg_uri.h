/* 
 * $Id$ 
 *
 * PostgreSQL Database Driver for SER
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * For a license to use the ser software under conditions other than those
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

#ifndef _PG_URI_H
#define _PG_URI_H

/** \addtogroup postgres
 * @{ 
 */

/** \file 
 * The implementation of parser parsing postgres://.. URIs.
 */

#include "../../lib/srdb2/db_uri.h"
#include "../../lib/srdb2/db_drv.h"

/** PostgreSQL driver specific payload to attach to db_uri structures.
 * This is the PostgreSQL specific structure that will be attached
 * to generic db_uri structures in the database API in SER. The 
 * structure contains parsed elements of postgres:// uri.
 */
struct pg_uri {
	db_drv_t drv;
	char* username;
	char* password;
	char* host;
	unsigned short port;
	char* database;
};

/** Create a new pg_uri structure and parse the URI in parameter.
 * This function builds a new pg_uri structure from the body of
 * the generic URI given to it in parameter.
 * @param uri A generic db_uri structure.
 * @retval 0 on success
 * @retval A negative number on error.
 */
int pg_uri(db_uri_t* uri);

/** @} */

#endif /* _PG_URI_H */

