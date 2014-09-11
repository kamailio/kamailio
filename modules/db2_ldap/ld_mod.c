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
 * LDAP module interface.
 */

#include "ld_mod.h"
#include "ld_uri.h"
#include "ld_cfg.h"
#include "ld_con.h"
#include "ld_cmd.h"
#include "ld_fld.h"
#include "ld_res.h"

#include "../../sr_module.h"

#ifdef LD_TEST
#include "../../lib/srdb2/db_cmd.h"
#include <limits.h>
#include <float.h>
#endif

#include <ldap.h>

str ld_cfg_file = STR_STATIC_INIT("ldap.cfg");
int ld_reconnect_attempt = 3;

static int ld_mod_init(void);
static void ld_mod_destroy(void);

MODULE_VERSION

/*
 * LDAP module interface
 */
static cmd_export_t cmds[] = {
	{"db_ctx",    (cmd_function)NULL, 0, 0, 0},
	{"db_con",    (cmd_function)ld_con, 0, 0, 0},
	{"db_uri",    (cmd_function)ld_uri, 0, 0, 0},
	{"db_cmd",    (cmd_function)ld_cmd, 0, 0, 0},
	{"db_put",    (cmd_function)ld_cmd_exec, 0, 0, 0},
	{"db_del",    (cmd_function)ld_cmd_exec, 0, 0, 0},
	{"db_get",    (cmd_function)ld_cmd_exec, 0, 0, 0},
	{"db_upd",    (cmd_function)ld_cmd_exec, 0, 0, 0},
	{"db_sql",    (cmd_function)ld_cmd_exec, 0, 0, 0},
	{"db_res",    (cmd_function)ld_res, 0, 0, 0},
	{"db_fld",    (cmd_function)ld_fld, 0, 0, 0},
	{"db_first",  (cmd_function)ld_cmd_first, 0, 0, 0},
	{"db_next",   (cmd_function)ld_cmd_next, 0, 0, 0},
	{"db_setopt", (cmd_function)ld_cmd_setopt, 0, 0, 0},
	{"db_getopt", (cmd_function)NULL, 0, 0, 0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"config", PARAM_STR, &ld_cfg_file},
	{"reconnect_attempt", PARAM_INT, &ld_reconnect_attempt},
	{0, 0, 0}
};


struct module_exports exports = {
	"db2_ldap",
	cmds,
	0,              /* RPC method */
	params,         /* module parameters */
	ld_mod_init,    /* module initialization function */
	0,              /* response function*/
	ld_mod_destroy, /* destroy function */
	0,              /* oncancel function */
	0               /* per-child init function */
};


#ifdef LD_TEST
int ldap_test(void)
{
	int i, row;
	db_ctx_t* db;
	db_cmd_t* put, *del, *get;
	db_res_t* result;
	db_rec_t* rec;
	char* times;

	db_fld_t int_vals[] = {
		{.name = "col_bool",      .type = DB_INT},
		{.name = "col_int8",      .type = DB_INT},
		{.name = "col_int4",      .type = DB_INT},
		{.name = "col_inet",      .type = DB_INT},
		{.name = "col_timestamp", .type = DB_INT},
		{.name = "col_bit",       .type = DB_INT},
		{.name = "col_varbit",    .type = DB_INT},
		{.name = NULL}
	};

	db_fld_t datetime_vals[] = {
		{.name = "col_int8",      .type = DB_INT},
		{.name = "col_int4",      .type = DB_INT},
		{.name = "col_timestamp", .type = DB_INT},
		{.name = NULL}
	};


	db_fld_t bitmap_vals[] = {
		{.name = "col_int8",      .type = DB_INT},
		{.name = "col_int4",      .type = DB_INT},
		{.name = "col_bit",       .type = DB_INT},
		{.name = "col_varbit",    .type = DB_INT},
		{.name = NULL}
	};

	db_fld_t float_vals[] = {
		{.name = "col_float4", .type = DB_FLOAT},
		{.name = "col_float8", .type = DB_FLOAT},
		{.name = NULL}
	};

	db_fld_t double_vals[] = {
		{.name = "col_float8", .type = DB_DOUBLE},
		{.name = NULL}
	};

	db_fld_t str_vals[] = {
		{.name = "col_varchar", .type = DB_STR},
		{.name = "col_bytea",   .type = DB_STR},
		{.name = "col_text",    .type = DB_STR},
		{.name = "col_bpchar",  .type = DB_STR},
		{.name = "col_char",    .type = DB_STR},
		{.name = NULL}
	};

	db_fld_t cstr_vals[] = {
		{.name = "col_varchar", .type = DB_CSTR},
		{.name = "col_bytea",   .type = DB_CSTR},
		{.name = "col_text",    .type = DB_CSTR},
		{.name = "col_bpchar",  .type = DB_CSTR},
		{.name = "col_char",    .type = DB_CSTR},
		{.name = NULL}
	};

	db_fld_t blob_vals[] = {
		{.name = "col_bytea",   .type = DB_BLOB},
		{.name = NULL}
	};


	db_fld_t res[] = {
		{.name = "col_bool",      .type = DB_INT},
		{.name = "col_bytea",     .type = DB_BLOB},
		{.name = "col_char",      .type = DB_STR},
		{.name = "col_int8",      .type = DB_INT},
		{.name = "col_int4",      .type = DB_INT},
		{.name = "col_int2",      .type = DB_INT},
		{.name = "col_text",      .type = DB_STR},
		{.name = "col_float4",    .type = DB_FLOAT},
		{.name = "col_float8",    .type = DB_DOUBLE},
		{.name = "col_inet",      .type = DB_INT},
		{.name = "col_bpchar",    .type = DB_STR},
		{.name = "col_varchar",   .type = DB_STR},
		{.name = "col_timestamp", .type = DB_DATETIME},
		{.name = "col_bit",       .type = DB_BITMAP},
		{.name = "col_varbit",    .type = DB_BITMAP},
		{.name = NULL}
	};

	db_fld_t cred[] = {
		{.name = "auth_username", .type = DB_CSTR},
		{.name = "realm", .type = DB_CSTR},
		{.name = NULL}
	};


	db = db_ctx("ldap");
	if (db == NULL) {
		ERR("Error while initializing database layer\n");
		goto error;
	}
	if (db_add_db(db, "ldap://127.0.0.1") < 0) goto error;

	if (db_connect(db) < 0) goto error;

	/*
	del = db_cmd(DB_DEL, db, "test", NULL, NULL, NULL);
	if (del == NULL) {
		ERR("Error while building delete * query\n");
		goto error;
	}

    put = db_cmd(DB_PUT, db, "test", NULL, NULL, int_vals);
	if (put == NULL) {
		ERR("Error while building test query\n");
		goto error;
	}

	if (db_exec(NULL, del)) {
		ERR("Error while deleting rows from test table\n");
		goto error;
	}

	put->vals[0].v.int4 = 0xffffffff;
	put->vals[1].v.int4 = 0xffffffff;
	put->vals[2].v.int4 = 0xffffffff;
	put->vals[3].v.int4 = 0xffffffff;
	put->vals[4].v.int4 = 0xffffffff;
	put->vals[5].v.int4 = 0xffffffff;
	put->vals[6].v.int4 = 0xffffffff;

	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.int4 = 0;
	put->vals[1].v.int4 = 0;
	put->vals[2].v.int4 = 0;
	put->vals[3].v.int4 = 0;
	put->vals[4].v.int4 = 0;
	put->vals[5].v.int4 = 0;
	put->vals[6].v.int4 = 0;

	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, bitmap_vals);
	if (put == NULL) {
		ERR("Error while building bitmap test query\n");
		goto error;
	}

	put->vals[0].v.int4 = 0xffffffff;
	put->vals[1].v.int4 = 0xffffffff;
	put->vals[2].v.int4 = 0xffffffff;
	put->vals[3].v.int4 = 0xffffffff;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.int4 = 0;
	put->vals[1].v.int4 = 0;
	put->vals[2].v.int4 = 0;
	put->vals[3].v.int4 = 0;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, float_vals);
	if (put == NULL) {
		ERR("Error while building float test query\n");
		goto error;
	}

	put->vals[0].v.flt = FLT_MAX;
	put->vals[1].v.flt = FLT_MAX;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.flt = FLT_MIN;
	put->vals[1].v.flt = FLT_MIN;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, double_vals);
	if (put == NULL) {
		ERR("Error while building double test query\n");
		goto error;
	}

	put->vals[0].v.dbl = DBL_MAX;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.dbl = DBL_MIN;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}


	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, str_vals);
	if (put == NULL) {
		ERR("Error while building str test query\n");
		goto error;
	}

	put->vals[0].v.lstr.s = "";
	put->vals[0].v.lstr.len = 0;
	put->vals[1].v.lstr.s = "";
	put->vals[1].v.lstr.len = 0;
	put->vals[2].v.lstr.s = "";
	put->vals[2].v.lstr.len = 0;
	put->vals[3].v.lstr.s = "";
	put->vals[3].v.lstr.len = 0;
	put->vals[4].v.lstr.s = "";
	put->vals[4].v.lstr.len = 0;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.lstr.s = "abc should not be there";
	put->vals[0].v.lstr.len = 3;
	put->vals[1].v.lstr.s = "abc should not be there";
	put->vals[1].v.lstr.len = 3;
	put->vals[2].v.lstr.s = "abc should not be there";
	put->vals[2].v.lstr.len = 3;
	put->vals[3].v.lstr.s = "abc should not be there";
	put->vals[3].v.lstr.len = 3;
	put->vals[4].v.lstr.s = "a should not be there";
	put->vals[4].v.lstr.len = 1;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, cstr_vals);
	if (put == NULL) {
		ERR("Error while building cstr test query\n");
		goto error;
	}

	put->vals[0].v.cstr = "";
	put->vals[1].v.cstr = "";
	put->vals[2].v.cstr = "";
	put->vals[3].v.cstr = "";
	put->vals[4].v.cstr = "";
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.cstr = "def";
	put->vals[1].v.cstr = "def";
	put->vals[2].v.cstr = "def";
	put->vals[3].v.cstr = "def";
	put->vals[4].v.cstr = "d";
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, blob_vals);
	if (put == NULL) {
		ERR("Error while building blob test query\n");
		goto error;
	}

	put->vals[0].v.blob.s = "\0\0\0\0";
	put->vals[0].v.blob.len = 4;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}


	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, datetime_vals);
	if (put == NULL) {
		ERR("Error while building datetime test query\n");
		goto error;
	}

	put->vals[0].v.time = 0xffffffff;
	put->vals[1].v.time = 0xffffffff;
	put->vals[2].v.time = 0xffffffff;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.time = 0;
	put->vals[1].v.time = 0;
	put->vals[2].v.time = 0;
	if (db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	if (put) db_cmd_free(put);
	if (del) db_cmd_free(del);
	put = NULL;
	del = NULL;

	*/

	get = db_cmd(DB_GET, db, "credentials", res, cred, NULL);
	if (get == NULL) {
		ERR("Error while building select query\n");
		goto error;
	}

	get->match[0].v.cstr = "jan";
	get->match[1].v.cstr = "iptel.org";

	if (db_exec(&result, get)) {
		ERR("Error while executing select query\n");
		goto error;
	}

	rec = db_first(result);
	while(rec) {
		rec = db_next(result);
	}

	return 0;

	rec = db_first(result);
	row = 1;
	while(rec) {
		ERR("row: %d\n", row);
		for(i = 0; !DB_FLD_LAST(rec->fld[i]); i++) {
			if (rec->fld[i].flags & DB_NULL) {
				ERR("%s: NULL\n", rec->fld[i].name);
			} else {
				switch(rec->fld[i].type) {
				case DB_INT:
				case DB_BITMAP:
					ERR("%s: %d\n", rec->fld[i].name, rec->fld[i].v.int4);
					break;

				case DB_DATETIME:
					times = ctime(&rec->fld[i].v.time);
					ERR("%s: %d:%.*s\n", rec->fld[i].name, rec->fld[i].v.time, strlen(times) - 1, times);
					break;

				case DB_DOUBLE:
					ERR("%s: %f\n", rec->fld[i].name, rec->fld[i].v.dbl);
					break;

				case DB_FLOAT:
					ERR("%s: %f\n", rec->fld[i].name, rec->fld[i].v.flt);
					break;

				case DB_STR:
				case DB_BLOB:
					ERR("%s: %.*s\n", rec->fld[i].name, rec->fld[i].v.lstr.len, rec->fld[i].v.lstr.s);
					break;

				case DB_CSTR:
					ERR("%s: %s\n", rec->fld[i].name, rec->fld[i].v.cstr);
					break;
				}
			}
		}
		ERR("\n");
		rec = db_next(result);
		row++;
	}

	db_res_free(result);

	db_cmd_free(get);
	db_disconnect(db);
	db_ctx_free(db);
	return 0;

 error:
	if (get) db_cmd_free(get);
	if (put) db_cmd_free(put);
	if (del) db_cmd_free(del);
	db_disconnect(db);
	db_ctx_free(db);
	return -1;
}
#endif /* LDAP_TEST */


static void ld_mod_destroy(void)
{
	ld_cfg_free();
}


static int ld_mod_init(void)
{
	if (ld_load_cfg(&ld_cfg_file)) {
			ERR("ldap: Error while loading configuration file\n");
			return -1;
	}

#ifdef LD_TEST
	if (ldap_test() == 0) {
		ERR("ldap: Testing successful\n");
	} else {
		ERR("ldap: Testing failed\n");
	}
	return -1;
#endif /* LDAP_TEST */
	return 0;
}

/** @} */
