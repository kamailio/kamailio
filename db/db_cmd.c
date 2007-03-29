/* 
 * $Id$ 
 *
 * Copyright (C) 2001-2005 FhG FOKUS
 * Copyright (C) 2006-2007 iptelorg GmbH
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
 */

#include <string.h>
#include "../dprint.h"
#include "../mem/mem.h"
#include "../ut.h"
#include "db_cmd.h"


db_cmd_t* db_cmd(enum db_cmd_type type, db_ctx_t* ctx, char* table, db_fld_t* match, db_fld_t* fld)
{
	char* fname;
    db_cmd_t* res;
	db_con_t* con;
	int i, r;

    res = (db_cmd_t*)pkg_malloc(sizeof(db_cmd_t));
    if (res == NULL) goto err;
    memset(res, '\0', sizeof(db_cmd_t));
	if (db_gen_init(&res->gen) < 0) goto err;
    res->ctx = ctx;

	res->table.len = strlen(table);
    res->table.s = (char*)pkg_malloc(res->table.len);
    if (res->table.s == NULL) goto err;
    memcpy(res->table.s, table, res->table.len);
	res->type = type;
	res->match = match;
	res->fld = fld;

	i = 0;
	DBLIST_FOREACH(con, &ctx->con) {
		r = db_drv_call(&con->uri->scheme, "db_cmd", res, i);
		if (r < 0) goto err;
		if (r > 0) {
			ERR("DB driver %.*s does not implement mandatory db_cmd function\n",
				con->uri->scheme.len, ZSW(con->uri->scheme.s));
			goto err;
		}
		if (res->exec[i] == NULL) {
			/* db_cmd in the db driver did not provide any runtime function, so try to lookup the
			 * default one through the module API
			 */
			switch(type) {
			case DB_PUT: fname = "db_put"; break;
			case DB_DEL: fname = "db_del"; break;
			case DB_GET: fname = "db_get"; break;
			default: ERR("db_cmd: Unsupported command type\n"); goto err;
			}

			r = db_drv_func(&(res->exec[i]), &con->uri->scheme, fname);
			if (r < 0) goto err;
			if (r > 0) {
				ERR("DB driver %.*s does not provide runtime execution function %s\n",
					con->uri->scheme.len, ZSW(con->uri->scheme.s), fname);
				goto err;
			}
		}
		i++;
	}
    return res;

 err:
    ERR("db_cmd: Cannot create db_cmd structure\n");
	db_gen_free(&res->gen);
    if (res == NULL) return NULL;
    if (res->table.s) pkg_free(res->table.s);
    pkg_free(res);
    return NULL;
}


void db_cmd_free(db_cmd_t* cmd)
{
    if (cmd == NULL) return;
	db_gen_free(&cmd->gen);
    if (cmd->table.s) pkg_free(cmd->table.s);
    pkg_free(cmd);
}


int db_exec(db_res_t** res, db_cmd_t* cmd)
{
	db_con_t* con;
	int i;

	i = 0;
	DBLIST_FOREACH(con, &cmd->ctx->con) {
		db_payload_idx = i;
		if (cmd->exec[i](cmd) < 0) return -1;
		i++;
	}
	return 0;
}
