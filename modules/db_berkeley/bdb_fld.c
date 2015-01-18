/*
 * BDB Database Driver for Kamailio
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** \addtogroup bdb
 * @{
 */

/** \file
 * Data field conversion and type checking functions.
 *
 * \ingroup database
 */

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <stdint.h>
#include <string.h>
#include <time.h>   /* strptime, XOPEN issue must be >= 4 */

#include "../../lib/srdb2/db_drv.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include "bdb_fld.h"

static void bdb_fld_free(db_fld_t* fld, bdb_fld_t* payload)
{
	db_drv_free(&payload->gen);
	if (payload->buf.s) pkg_free(payload->buf.s);
	if (payload->name) pkg_free(payload->name);
	pkg_free(payload);
}


int bdb_fld(db_fld_t* fld, char* table)
{
	bdb_fld_t *res;

	res = (bdb_fld_t*)pkg_malloc(sizeof(bdb_fld_t));
	if (res == NULL) {
		ERR("oracle: No memory left\n");
		return -1;
	}
	memset(res, '\0', sizeof(bdb_fld_t));
	res->col_pos = -1;
	if (db_drv_init(&res->gen, bdb_fld_free) < 0) goto error;

	DB_SET_PAYLOAD(fld, res);
	return 0;

error:
	if (res) pkg_free(res);
	return -1;
}


/** @} */
