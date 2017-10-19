/* 
 * PostgreSQL Database Driver for Kamailio
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _PG_RES_H
#define _PG_RES_H

/** \addtogroup postgres
 * @{ 
 */

/** \file
 * Data structures and functions to convert results obtained from PostgreSQL
 * servers.
 */

#include "../../lib/srdb2/db_drv.h"
#include "../../lib/srdb2/db_res.h"

#include <libpq-fe.h>

struct pg_res
{
	db_drv_t gen;
	PGresult *res;
	int row, rows;
};

int pg_res(db_res_t *res);

/** @} */

#endif /* _PG_RES_H */
