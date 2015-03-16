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

#include "my_res.h"

#include "my_cmd.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb2/db_gen.h"

#include <mysql/mysql.h>


void my_res_free(db_res_t* res, struct my_res* payload)
{
	struct my_cmd* mcmd;

	mcmd = DB_GET_PAYLOAD(res->cmd);

	if (mcmd->st && mysql_stmt_free_result(mcmd->st)) {
		ERR("mysql: Error while freeing MySQL result: %d, %s\n", 
			mysql_stmt_errno(mcmd->st), mysql_stmt_error(mcmd->st));
	}

	db_drv_free(&payload->gen);
	pkg_free(payload);
}


/*
 * Attach a mysql specific structure to db_res, this structure contains a pointer
 * to my_res_free which releases the mysql result stored in the mysql statement
 * and if there is a cursor open in the statement then it will be closed as well
 */
int my_res(db_res_t* res)
{
	struct my_res* mr;

	mr = (struct my_res*)pkg_malloc(sizeof(struct my_res));
	if (mr == NULL) {
		ERR("mysql: No memory left\n");
		return -1;
	}
	if (db_drv_init(&mr->gen, my_res_free) < 0) goto error;
	DB_SET_PAYLOAD(res, mr);
	return 0;
	
 error:
	if (mr) {
		db_drv_free(&mr->gen);
		pkg_free(mr);
	}
	return -1;
}
