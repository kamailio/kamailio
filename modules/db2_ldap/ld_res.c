/* 
 * $Id$ 
 *
 * LDAP Database Driver for SER
 *
 * Copyright (C) 2008 iptelorg GmbH
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

/** \addtogroup ldap
 * @{ 
 */

/** \file
 * Functions working with result structures received from LDAP servers.
 */

#include <string.h>

#include "ld_res.h"
#include "ld_cmd.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb2/db_gen.h"


static void ld_res_free(db_res_t* res, struct ld_res* payload)
{
	db_drv_free(&payload->gen);
	if (payload->msg) ldap_msgfree(payload->msg);
	payload->msg = NULL;
	pkg_free(payload);
}


int ld_res(db_res_t* res)
{
	struct ld_res* lres;

	lres = (struct ld_res*)pkg_malloc(sizeof(struct ld_res));
	if (lres == NULL) {
		ERR("ldap: No memory left\n");
		return -1;
	}
	memset(lres, '\0', sizeof(struct ld_res));
	if (db_drv_init(&lres->gen, ld_res_free) < 0) goto error;
	DB_SET_PAYLOAD(res, lres);
	return 0;
	
 error:
	if (lres) {
		db_drv_free(&lres->gen);
		pkg_free(lres);
	}
	return -1;
}

/** @} */
