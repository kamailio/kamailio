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

/** \addtogroup postgres
 * @{ 
 */

/** \file
 * Functions working with result structures received from PostgreSQL servers.
 */

#include "pg_res.h"
#include "pg_cmd.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb2/db_gen.h"


static void pg_res_free(db_res_t* res, struct pg_res* payload)
{
	db_drv_free(&payload->gen);
	if (payload->res) PQclear(payload->res);
	pkg_free(payload);
}


int pg_res(db_res_t* res)
{
	struct pg_res* pres;

	pres = (struct pg_res*)pkg_malloc(sizeof(struct pg_res));
	if (pres == NULL) {
		ERR("postgres: No memory left\n");
		return -1;
	}
	if (db_drv_init(&pres->gen, pg_res_free) < 0) goto error;
	DB_SET_PAYLOAD(res, pres);
	return 0;
	
 error:
	if (pres) {
		db_drv_free(&pres->gen);
		pkg_free(pres);
	}
	return -1;
}

/** @} */
