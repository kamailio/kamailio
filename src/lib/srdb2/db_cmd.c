/* 
 * Copyright (C) 2001-2005 FhG FOKUS
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

#include "db_cmd.h"

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h"

#include <string.h>
#include <stdarg.h>


db_cmd_t* db_cmd(enum db_cmd_type type, db_ctx_t* ctx, char* table, 
				 db_fld_t* result, db_fld_t* match, db_fld_t* values)
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

	/** FIXME: it is not clear now that this is necessary
	 *         when we have match and value separate arrays */
	if (result) {
		newp->result = db_fld_copy(result);
		if (newp->result == NULL) goto err;
	}

	if (match) {
		newp->match = db_fld_copy(match);
		if (newp->match == NULL) goto err;
	}

	if (values) {
		newp->vals = db_fld_copy(values);
		if (newp->vals == NULL) goto err;
	}

	/* FIXME: This should be redesigned so that we do not need to connect
	 * connections in context before comands are created, this takes splitting
	 * the command initializatio sequence in two steps, one would be creating
	 * all the data structures and the second would be checking corresponding
	 * fields and tables on the server.
	 */
	if (ctx->con_n == 0) {
		ERR("No connections found in context %.*s\n", STR_FMT(&ctx->id));
		goto err;
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
			newp->result_count = j;
		}

		if (!DB_FLD_EMPTY(newp->match)) {
			for(j = 0; !DB_FLD_LAST(newp->match[j]); j++) {
				if (func && func(newp->match + j, table) < 0) goto err;
			}
			newp->match_count = j;
		}

		if (!DB_FLD_EMPTY(newp->vals)) {
			for(j = 0; !DB_FLD_LAST(newp->vals[j]); j++) {
				if (func && func(newp->vals + j, table) < 0) goto err;
			}
			newp->vals_count = j;
		}

		r = db_drv_call(&con->uri->scheme, "db_cmd", newp, i);
		if (r < 0) {
			ERR("db_drv_call(\"db_cmd\") failed\n");
			goto err;
		}
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
			case DB_UPD: fname = "db_upd"; break;
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

		if (type == DB_GET || type == DB_SQL) {
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
	}
    return newp;

 err:
    ERR("db_cmd: Cannot create db_cmd structure\n");
    if (newp) {
		db_gen_free(&newp->gen);
		if (newp->result)  db_fld_free(newp->result);
		if (newp->match)   db_fld_free(newp->match);
		if (newp->vals)    db_fld_free(newp->vals);
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
	if (cmd->match)  db_fld_free(cmd->match);
	if (cmd->vals)   db_fld_free(cmd->vals);
    if (cmd->table.s) pkg_free(cmd->table.s);
    pkg_free(cmd);
}


int db_exec(db_res_t** res, db_cmd_t* cmd)
{
	db_res_t* r = NULL;
	int ret;
	
	if (res) {
		r = db_res(cmd);
		if (r == NULL) return -1;
	}

	/* FIXME */
	db_payload_idx = 0;
	ret = cmd->exec[0](r, cmd);
	if (ret < 0) {
		if (r) db_res_free(r);
		return ret;
	}

	if (res) *res = r;
	return ret;
}


int db_getopt(db_cmd_t* cmd, char* optname, ...)
{
	int i, r;
	db_drv_func_t func;
	db_con_t* con;
	va_list ap;
	
	for(i = 0; i < cmd->ctx->con_n; i++) {
		con = cmd->ctx->con[i];
		
		r = db_drv_func(&func, &con->uri->scheme, "db_getopt");
		if (r < 0) return -1;
		if (r > 0) func = NULL;
		db_payload_idx = i;
		
		va_start(ap, optname);
		if (func && func(cmd, optname, ap) < 0) {
			va_end(ap);
			return -1;
		}
		va_end(ap);
	}
	
	return 0;
}


int db_setopt(db_cmd_t* cmd, char* optname, ...)
{
       int i, r;
       db_drv_func_t func;
       db_con_t* con;
       va_list ap;

       for(i = 0; i < cmd->ctx->con_n; i++) {
               con = cmd->ctx->con[i];

               r = db_drv_func(&func, &con->uri->scheme, "db_setopt");
               if (r < 0) return -1;
               if (r > 0) func = NULL;
               db_payload_idx = i;

               va_start(ap, optname);
               if (func && func(cmd, optname, ap) < 0) {
                       va_end(ap);
                       return -1;
               }
               va_end(ap);
       }

       return 0;
}


/** @} */
