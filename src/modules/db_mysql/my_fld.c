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

#include "my_fld.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb2/db_gen.h"

#include <string.h>


static void my_fld_free(db_fld_t* fld, struct my_fld* payload)
{
	db_drv_free(&payload->gen);
	if (payload->buf.s) pkg_free(payload->buf.s);
	if (payload->name) pkg_free(payload->name);
	pkg_free(payload);
}


int my_fld(db_fld_t* fld, char* table)
{
	struct my_fld* res;

	res = (struct my_fld*)pkg_malloc(sizeof(struct my_fld));
	if (res == NULL) {
		ERR("mysql: No memory left\n");
		return -1;
	}
	memset(res, '\0', sizeof(struct my_fld));
	if (db_drv_init(&res->gen, my_fld_free) < 0) goto error;

	DB_SET_PAYLOAD(fld, res);
	return 0;

 error:
	if (res) pkg_free(res);
	return -1;
}
