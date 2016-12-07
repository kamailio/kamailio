/* 
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006-2007 iptelorg GmbH
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

/*! \file
 * Berkeley DB : 
 *
 * \ingroup database
 */


#include <db.h>

#include "bdb_res.h"
#include "bdb_cmd.h"
#include "bdb_crs_compat.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb2/db_gen.h"


void bdb_res_free(db_res_t* res, bdb_res_t *payload)
{
	bdb_cmd_t *bcmd;

	bcmd = DB_GET_PAYLOAD(res->cmd);

	/* free bdb result */

	if(bcmd->dbcp!=NULL)
	{
		bcmd->dbcp->CLOSE_CURSOR(bcmd->dbcp);
		bcmd->dbcp = NULL;
	}
	db_drv_free(&payload->gen);
	pkg_free(payload);
}


/*
 * Attach bdb specific structure to db_res, this structure contains a pointer
 * to bdb_res_free which releases the result stored in the oracle statement
 * and if there is a cursor open in the statement then it will be closed as well
 */
int bdb_res(db_res_t* res)
{
	bdb_res_t *br;

	br = (bdb_res_t*)pkg_malloc(sizeof(bdb_res_t));
	if (br == NULL) {
		ERR("bdb: No memory left\n");
		return -1;
	}
	if (db_drv_init(&br->gen, bdb_res_free) < 0) goto error;
	DB_SET_PAYLOAD(res, br);
	return 0;
	
error:
	if (br) {
		db_drv_free(&br->gen);
		pkg_free(br);
	}
	return -1;
}
