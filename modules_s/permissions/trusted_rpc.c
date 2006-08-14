/* 
 * allow_trusted related functions
 *
 * Copyright (C) 2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *   2006-08-14: db_mode is checked
 *               trusted_reload() connects to the DB -- child processes
 *               do not keep the connection open (Miklos)
 */

#include "../../dprint.h"
#include "../../db/db.h"
#include "permissions.h"
#include "hash.h"
#include "trusted.h"
#include "trusted_rpc.h"


static const char* trusted_reload_doc[2] = {
	"Reload trusted table from database.",
	0
};


/*
 * Fifo function to reload trusted table
 */
static void trusted_reload(rpc_t* rpc, void* ctx)
{
	if (db_mode != ENABLE_CACHE) {
		rpc->fault(ctx, 400, "Database cache is not enabled");
		return;
	}

	/* connect to the DB */
	db_handle = perm_dbf.init(db_url);
	if (!db_handle) {
		LOG(L_ERR, "ERROR: Unable to connect to database\n");
		rpc->fault(ctx, 400, "Trusted Table Reload Failed");
		return;
	}

	/* reload cache */
	if (reload_trusted_table() < 0) {
		rpc->fault(ctx, 400, "Trusted Table Reload Failed");
	}

	/* close DB connection */
	perm_dbf.close(db_handle);
	db_handle = 0;
}



static const char* trusted_dump_doc[2] = {
	"Return the contents of trusted table",
	0
};

/*
 * Fifo function to print entries from current hash table
 */
static void trusted_dump(rpc_t* rpc, void* ctx)
{
	if (db_mode != ENABLE_CACHE) {
		rpc->fault(ctx, 400, "Database cache is not enabled");
		return;
	}

	if (hash_table) {
		hash_table_print(*hash_table, rpc, ctx);
	}
}


rpc_export_t trusted_rpc[] = {
	{"trusted.reload", trusted_reload, trusted_reload_doc, 0},
	{"trusted.dump",   trusted_dump,   trusted_dump_doc,   RET_ARRAY},
	{0, 0, 0, 0}
};
