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

/** \ingroup DB_API 
 * @{ 
 */

#include <string.h>
#include "../dprint.h"
#include "../mem/mem.h"
#include "../ut.h"
#include "db_cmd.h"


db_cmd_t* db_cmd(enum db_cmd_type type, db_ctx_t* ctx, char* table, 
				 db_fld_t* result, db_fld_t* params)
{
	char* fname;
    db_cmd_t* newp;
	db_con_t* con;
	int i, r, j;
	db_drv_func_t func;

    newp = (db_cmd_t*)pkg_malloc(sizeof(db_cmd_t));
    if (newp == NULL) goto err;
    memset(newp, '\0', sizeof(db_cmd_t));
	if (db_gen_init(&newp->gen) < 0) goto err;
    newp->ctx = ctx;

	newp->table.len = strlen(table);
    newp->table.s = (char*)pkg_malloc(newp->table.len + 1);
    if (newp->table.s == NULL) goto err;
    memcpy(newp->table.s, table, newp->table.len + 1);

	newp->type = type;

	if (result) {
		newp->result = db_fld_copy(result);
		if (newp->result == NULL) goto err;
	}

	if (params) {
		newp->params = db_fld_copy(params);
		if (newp->params == NULL) goto err;
	}

	for(i = 0; i < ctx->con_n; i++) {
		con = ctx->con[i];

		r = db_drv_func(&func, &con->uri->scheme, "db_fld");
		if (r < 0) goto err;
		if (r > 0) func = NULL;
		db_payload_idx = i;

		if (!DB_FLD_EMPTY(newp->result)) {
			for(j = 0; !DB_FLD_LAST(newp->result[j]); j++) {
				if (func && func(newp->result + j, table) < 0) goto err;
			}
			newp->res_fields = j;
		}

		if (!DB_FLD_EMPTY(newp->params)) {
			for(j = 0; !DB_FLD_LAST(newp->params[j]); j++) {
				if (func && func(newp->params + j, table) < 0) goto err;
			}
			newp->param_fields = j;
		}

		r = db_drv_call(&con->uri->scheme, "db_cmd", newp, i);
		if (r < 0) goto err;
		if (r > 0) {
			ERR("DB driver %.*s does not implement mandatory db_cmd function\n",
				con->uri->scheme.len, ZSW(con->uri->scheme.s));
			goto err;
		}
		if (newp->exec[i] == NULL) {
			/* db_cmd in the db driver did not provide any runtime function, so try to lookup the
			 * default one through the module API
			 */
			switch(type) {
			case DB_PUT: fname = "db_put"; break;
			case DB_DEL: fname = "db_del"; break;
			case DB_GET: fname = "db_get"; break;
			case DB_SQL: fname = "db_sql"; break;
			default: ERR("db_cmd: Unsupported command type\n"); goto err;
			}

			r = db_drv_func((void*)&(newp->exec[i]), &con->uri->scheme, fname);
			if (r < 0) goto err;
			if (r > 0) {
				ERR("DB driver %.*s does not provide runtime execution function %s\n",
					con->uri->scheme.len, ZSW(con->uri->scheme.s), fname);
				goto err;
			}
		}

		r = db_drv_func((void*)(&newp->first[i]), &con->uri->scheme, "db_first");
		if (r < 0) goto err;
		if (r > 0) {
			ERR("DB driver %.*s does not implement mandatory db_first function\n",
				con->uri->scheme.len, ZSW(con->uri->scheme.s));
			goto err;
		}

		r = db_drv_func((void*)(&newp->next[i]), &con->uri->scheme, "db_next");
		if (r < 0) goto err;
		if (r > 0) {
			ERR("DB driver %.*s does not implement mandatory db_next function\n",
				con->uri->scheme.len, ZSW(con->uri->scheme.s));
			goto err;
		}


	}
    return newp;

 err:
    ERR("db_cmd: Cannot create db_cmd structure\n");
    if (newp) {
		db_gen_free(&newp->gen);
		if (newp->result) db_fld_free(newp->result);
		if (newp->params) db_fld_free(newp->params);
		if (newp->table.s) pkg_free(newp->table.s);
		pkg_free(newp);
	}
	return NULL;
}


void db_cmd_free(db_cmd_t* cmd)
{
    if (cmd == NULL) return;
	db_gen_free(&cmd->gen);

	if (cmd->result) db_fld_free(cmd->result);
	if (cmd->params) db_fld_free(cmd->params);
    if (cmd->table.s) pkg_free(cmd->table.s);
    pkg_free(cmd);
}


int db_exec(db_res_t** res, db_cmd_t* cmd)
{
	db_res_t* r = NULL;

	if (res) {
		r = db_res(cmd);
		if (r == NULL) return -1;
		if (res) *res = r;
	}

	/* FIXME */
	db_payload_idx = 0;
	return cmd->exec[0](r, cmd);
}

/** @} */
