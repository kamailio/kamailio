/* 
 * Copyright (C) 2001-2003 FhG FOKUS
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

/** \ingroup DB_API 
 * @{ 
 */

#include "db_drv.h"

#include "db_gen.h"

#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../ut.h"

#include <string.h>
#include <stdlib.h>


int db_drv_init(db_drv_t* ptr, void* free_func)
{
	ptr->free = free_func;
	return 0;
}


void db_drv_free(db_drv_t* ptr)
{
	ptr->free = NULL;
}

/*
 * Find function with given name in module named <module>
 * RETURNS: -1 on error
 *           0 if a function has been found
 *           1 if no function has been found
 */
int db_drv_func(db_drv_func_t* func, str* module, char* func_name)
{
	static str prefix = STR_STATIC_INIT("db_");
	char* buf = NULL, *name;
	
	if ((buf = pkg_malloc(prefix.len + module->len + 1)) == NULL) {
		LOG(L_ERR, "db_drv_func: No memory left\n");
		goto error;
	}
	
	memcpy(buf, prefix.s, prefix.len);
	memcpy(buf + prefix.len, module->s, module->len);
	buf[prefix.len + module->len] = '\0';
	
	/* First try to find the module with prefix "db_" */
	name = buf;
	if (find_module_by_name(name) == 0) {
		/* Not found, so try without the prefix */
		name = buf + prefix.len;
		if (find_module_by_name(name) == 0) {
			ERR("db_drv_func: database driver for '%.*s' not found\n", STR_FMT(module));
			goto error;
		}
	}
	
	*func = (db_drv_func_t)find_mod_export(name, func_name, 0, 0);
	
	if (buf) pkg_free(buf);
	if (*func) return 0;
	else return 1;
	
error:
	if (buf) pkg_free(buf);
	return -1;
}



/*
 * Call function with name <func_name> in DB driver <module>, give
 * it pointer <db_struct> as the pointer to the corresponding DB structure
 * (type of the structure is function-specific) and <offset> is the offset
 * of the driver/connection within the context (used to make DB_DRV_ATTACH 
 * and DB_DRV_DATA macros work)
 */
int db_drv_call(str* module, char* func_name, void* db_struct, int offset)
{
	db_drv_func_t func;
	int ret;

	ret = db_drv_func(&func, module, func_name);
	if (ret < 0) {
		ERR("db: db_drv_call failed\n");
		return -1;
	}

	if (ret == 0) {
		db_payload_idx = offset;
		return func(db_struct);
	} else {
		DBG("db_drv_call: DB driver for %.*s does not export function %s\n",
			module->len, ZSW(module->s), func_name);
		return 1;
	}
}

/** @} */
