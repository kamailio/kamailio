/*
 * PostgreSQL Database Driver for Kamailio
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/** \addtogroup postgres
 * @{
 */

/** \file
 * Postgres module interface.
 */

#include "pg_mod.h"
#include "pg_uri.h"
#include "pg_con.h"
#include "pg_cmd.h"
#include "pg_res.h"
#include "pg_fld.h"
#include "db_postgres.h"

#include "../../core/sr_module.h"
#include "../../core/parser/parse_param.h"

#ifdef PG_TEST
#include <limits.h>
#include <float.h>
#endif

MODULE_VERSION

static int pg_mod_init(void);
static void pg_mod_destroy(void);

int pg_retries =
		2; /* How many times should the module try re-execute failed commands.
					  * 0 disables reconnecting */

int pg_lockset = 4;
int pg_timeout = 0; /* default = no timeout */
int pg_keepalive = 0;
int pg_bytea_output_escape = 1;

pg_con_param_t *pg_con_param_list = 0;
static int pg_con_param(modparam_t type, void *val);
static int pg_init_com_params();

/*
 * Postgres module interface
 */
static cmd_export_t cmds[] = {{"db_ctx", (cmd_function)NULL, 0, 0, 0, 0},
		{"db_con", (cmd_function)pg_con, 0, 0, 0, 0},
		{"db_uri", (cmd_function)pg_uri, 0, 0, 0, 0},
		{"db_cmd", (cmd_function)pg_cmd, 0, 0, 0, 0},
		{"db_put", (cmd_function)pg_cmd_exec, 0, 0, 0, 0},
		{"db_del", (cmd_function)pg_cmd_exec, 0, 0, 0, 0},
		{"db_get", (cmd_function)pg_cmd_exec, 0, 0, 0, 0},
		{"db_upd", (cmd_function)pg_cmd_exec, 0, 0, 0, 0},
		{"db_sql", (cmd_function)pg_cmd_exec, 0, 0, 0, 0},
		{"db_res", (cmd_function)pg_res, 0, 0, 0, 0},
		{"db_fld", (cmd_function)pg_fld, 0, 0, 0, 0},
		{"db_first", (cmd_function)pg_cmd_first, 0, 0, 0, 0},
		{"db_next", (cmd_function)pg_cmd_next, 0, 0, 0, 0},
		{"db_setopt", (cmd_function)pg_setopt, 0, 0, 0, 0},
		{"db_getopt", (cmd_function)pg_getopt, 0, 0, 0, 0},
		{"db_bind_api", (cmd_function)db_postgres_bind_api, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0}};


/*
 * Exported parameters
 */
static param_export_t params[] = {{"retries", PARAM_INT, &pg_retries},
		{"lockset", PARAM_INT, &pg_lockset},
		{"timeout", PARAM_INT, &pg_timeout},
		{"tcp_keepalive", PARAM_INT, &pg_keepalive},
		{"bytea_output_escape", PARAM_INT, &pg_bytea_output_escape},
		{"con_param", PARAM_STRING | USE_FUNC_PARAM, (void *)pg_con_param},
		{0, 0, 0}};


struct module_exports exports = {
		"db_postgres",	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* exported RPC methods */
		0,				 /* exported pseudo-variables */
		0,				 /* response function*/
		pg_mod_init,	 /* module initialization function */
		0,				 /* per-child init function */
		pg_mod_destroy	 /* destroy function */
};

/*
CREATE TABLE test (
    col_bool BOOL,
    col_bytea BYTEA,
    col_char CHAR,
    col_int8 INT8,
    col_int4 INT4,
    col_int2 INT2,
    col_text TEXT,
    col_float4 FLOAT4,
    col_float8 FLOAT8,
    col_inet INET,
    col_bpchar BPCHAR,
    col_varchar VARCHAR,
    col_timestamp TIMESTAMP,
    col_timestamptz TIMESTAMPTZ,
    col_bit BIT(32),
    col_varbit VARBIT
);
*/


#ifdef PG_TEST
int pg_test(void)
{
	int i, row;
	db_ctx_t *db;
	db_cmd_t *put, *del, *get;
	db_res_t *result;
	db_rec_t *rec;
	char *times;

	db_fld_t int_vals[] = {{.name = "col_bool", .type = DB_INT},
			{.name = "col_int8", .type = DB_INT},
			{.name = "col_int4", .type = DB_INT},
			{.name = "col_inet", .type = DB_INT},
			{.name = "col_timestamp", .type = DB_INT},
			{.name = "col_timestamptz", .type = DB_INT},
			{.name = "col_bit", .type = DB_INT},
			{.name = "col_varbit", .type = DB_INT}, {.name = NULL}};

	db_fld_t datetime_vals[] = {{.name = "col_int8", .type = DB_INT},
			{.name = "col_int4", .type = DB_INT},
			{.name = "col_timestamp", .type = DB_INT},
			{.name = "col_timestamptz", .type = DB_INT}, {.name = NULL}};


	db_fld_t bitmap_vals[] = {{.name = "col_int8", .type = DB_INT},
			{.name = "col_int4", .type = DB_INT},
			{.name = "col_bit", .type = DB_INT},
			{.name = "col_varbit", .type = DB_INT}, {.name = NULL}};

	db_fld_t float_vals[] = {{.name = "col_float4", .type = DB_FLOAT},
			{.name = "col_float8", .type = DB_FLOAT}, {.name = NULL}};

	db_fld_t double_vals[] = {
			{.name = "col_float8", .type = DB_DOUBLE}, {.name = NULL}};

	db_fld_t str_vals[] = {{.name = "col_varchar", .type = DB_STR},
			{.name = "col_bytea", .type = DB_STR},
			{.name = "col_text", .type = DB_STR},
			{.name = "col_bpchar", .type = DB_STR},
			{.name = "col_char", .type = DB_STR}, {.name = NULL}};

	db_fld_t cstr_vals[] = {{.name = "col_varchar", .type = DB_CSTR},
			{.name = "col_bytea", .type = DB_CSTR},
			{.name = "col_text", .type = DB_CSTR},
			{.name = "col_bpchar", .type = DB_CSTR},
			{.name = "col_char", .type = DB_CSTR}, {.name = NULL}};

	db_fld_t blob_vals[] = {
			{.name = "col_bytea", .type = DB_BLOB}, {.name = NULL}};


	db_fld_t res[] = {{.name = "col_bool", .type = DB_INT},
			{.name = "col_bytea", .type = DB_BLOB},
			{.name = "col_char", .type = DB_STR},
			{.name = "col_int8", .type = DB_INT},
			{.name = "col_int4", .type = DB_INT},
			{.name = "col_int2", .type = DB_INT},
			{.name = "col_text", .type = DB_STR},
			{.name = "col_float4", .type = DB_FLOAT},
			{.name = "col_float8", .type = DB_DOUBLE},
			{.name = "col_inet", .type = DB_INT},
			{.name = "col_bpchar", .type = DB_STR},
			{.name = "col_varchar", .type = DB_STR},
			{.name = "col_timestamp", .type = DB_DATETIME},
			{.name = "col_timestamptz", .type = DB_DATETIME},
			{.name = "col_bit", .type = DB_BITMAP},
			{.name = "col_varbit", .type = DB_BITMAP}, {.name = NULL}};


	db = db_ctx("postgres");
	if(db == NULL) {
		ERR("Error while initializing database layer\n");
		goto error;
	}
	if(db_add_db(db, "postgres://janakj:heslo@localhost/ser") < 0)
		goto error;
	if(db_connect(db) < 0)
		goto error;

	del = db_cmd(DB_DEL, db, "test", NULL, NULL, NULL);
	if(del == NULL) {
		ERR("Error while building delete * query\n");
		goto error;
	}

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, int_vals);
	if(put == NULL) {
		ERR("Error while building test query\n");
		goto error;
	}

	if(db_exec(NULL, del)) {
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
	put->vals[7].v.int4 = 0xffffffff;

	if(db_exec(NULL, put)) {
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
	put->vals[7].v.int4 = 0;

	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, bitmap_vals);
	if(put == NULL) {
		ERR("Error while building bitmap test query\n");
		goto error;
	}

	put->vals[0].v.int4 = 0xffffffff;
	put->vals[1].v.int4 = 0xffffffff;
	put->vals[2].v.int4 = 0xffffffff;
	put->vals[3].v.int4 = 0xffffffff;
	put->vals[4].v.int4 = 0xffffffff;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.int4 = 0;
	put->vals[1].v.int4 = 0;
	put->vals[2].v.int4 = 0;
	put->vals[3].v.int4 = 0;
	put->vals[4].v.int4 = 0;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, float_vals);
	if(put == NULL) {
		ERR("Error while building float test query\n");
		goto error;
	}

	put->vals[0].v.flt = FLT_MAX;
	put->vals[1].v.flt = FLT_MAX;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.flt = FLT_MIN;
	put->vals[1].v.flt = FLT_MIN;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, double_vals);
	if(put == NULL) {
		ERR("Error while building double test query\n");
		goto error;
	}

	put->vals[0].v.dbl = DBL_MAX;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.dbl = DBL_MIN;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}


	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, str_vals);
	if(put == NULL) {
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
	if(db_exec(NULL, put)) {
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
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, cstr_vals);
	if(put == NULL) {
		ERR("Error while building cstr test query\n");
		goto error;
	}

	put->vals[0].v.cstr = "";
	put->vals[1].v.cstr = "";
	put->vals[2].v.cstr = "";
	put->vals[3].v.cstr = "";
	put->vals[4].v.cstr = "";
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.cstr = "def";
	put->vals[1].v.cstr = "def";
	put->vals[2].v.cstr = "def";
	put->vals[3].v.cstr = "def";
	put->vals[4].v.cstr = "d";
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, blob_vals);
	if(put == NULL) {
		ERR("Error while building blob test query\n");
		goto error;
	}

	put->vals[0].v.blob.s = "\0\0\0\0";
	put->vals[0].v.blob.len = 4;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}


	db_cmd_free(put);

	put = db_cmd(DB_PUT, db, "test", NULL, NULL, datetime_vals);
	if(put == NULL) {
		ERR("Error while building datetime test query\n");
		goto error;
	}

	put->vals[0].v.time = 0xffffffff;
	put->vals[1].v.time = 0xffffffff;
	put->vals[2].v.time = 0xffffffff;
	put->vals[3].v.time = 0xffffffff;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	put->vals[0].v.time = 0;
	put->vals[1].v.time = 0;
	put->vals[2].v.time = 0;
	put->vals[3].v.time = 0;
	if(db_exec(NULL, put)) {
		ERR("Error while executing database command\n");
		goto error;
	}

	if(put)
		db_cmd_free(put);
	if(del)
		db_cmd_free(del);
	put = NULL;
	del = NULL;


	get = db_cmd(DB_GET, db, "test", res, NULL, NULL);
	if(get == NULL) {
		ERR("Error while building select query\n");
		goto error;
	}

	if(db_exec(&result, get)) {
		ERR("Error while executing select query\n");
		goto error;
	}

	rec = db_first(result);
	row = 1;
	while(rec) {
		ERR("row: %d\n", row);
		for(i = 0; !DB_FLD_LAST(rec->fld[i]); i++) {
			if(rec->fld[i].flags & DB_NULL) {
				ERR("%s: NULL\n", rec->fld[i].name);
			} else {
				switch(rec->fld[i].type) {
					case DB_INT:
					case DB_BITMAP:
						ERR("%s: %d\n", rec->fld[i].name, rec->fld[i].v.int4);
						break;

					case DB_DATETIME:
						times = ctime(&rec->fld[i].v.time);
						ERR("%s: %d:%.*s\n", rec->fld[i].name,
								rec->fld[i].v.time, strlen(times) - 1, times);
						break;

					case DB_DOUBLE:
						ERR("%s: %f\n", rec->fld[i].name, rec->fld[i].v.dbl);
						break;

					case DB_FLOAT:
						ERR("%s: %f\n", rec->fld[i].name, rec->fld[i].v.flt);
						break;

					case DB_STR:
					case DB_BLOB:
						ERR("%s: %.*s\n", rec->fld[i].name,
								rec->fld[i].v.lstr.len, rec->fld[i].v.lstr.s);
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
	if(get)
		db_cmd_free(get);
	if(put)
		db_cmd_free(put);
	if(del)
		db_cmd_free(del);
	db_disconnect(db);
	db_ctx_free(db);
	return -1;
}
#endif /* PG_TEST */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(!pg_alloc_buffer()) {
		LM_ERR("failed too allocate buffer\n");
		return -1;
	}
	if(db_api_init() < 0)
		return -1;
	return 0;
}

static int pg_mod_init(void)
{
#ifdef PG_TEST
	if(pg_test() == 0) {
		ERR("postgres: Testing successful\n");
	} else {
		ERR("postgres: Testing failed\n");
	}
	return -1;
#endif /* PG_TEST */
	if(pg_init_lock_set(pg_lockset) < 0)
		return -1;

	if(pg_init_com_params() < 0) {
		return -1;
	}

	return km_postgres_mod_init();
}

static void pg_mod_destroy(void)
{
	pg_destroy_lock_set();
}

static void free_con_param_list()
{
	pg_con_param_t *tmp = NULL;
	pg_con_param_t *con_param = pg_con_param_list;
	while(con_param) {
		if(con_param->name) {
			shm_free(con_param->name);
		}
		if(con_param->value) {
			shm_free(con_param->value);
		}
		tmp = con_param->next;
		shm_free(con_param);
		con_param = tmp;
	}
}

static int add_con_param(str *name, str *value)
{
	/* malloc for param */
	pg_con_param_t *con_param =
			(pg_con_param_t *)shm_malloc(sizeof(pg_con_param_t));
	if(con_param == 0) {
		LM_ERR("no more shm memory\n");
		goto error;
	}

	/* parse name */
	con_param->name = (char *)shm_malloc(name->len + 1);
	if(con_param->name == NULL) {
		LM_ERR("no more shm memory while parsing name\n");
		goto error;
	}
	memcpy(con_param->name, name->s, name->len);
	con_param->name[name->len] = '\0';

	/* parse value */
	con_param->value = (char *)shm_malloc(value->len + 1);
	if(con_param->value == NULL) {
		LM_ERR("no more shm memory while parsing value\n");
		goto error;
	}
	memcpy(con_param->value, value->s, value->len);
	con_param->value[value->len] = '\0';

	/* add param to the linked list */
	con_param->next = pg_con_param_list;
	pg_con_param_list = con_param;
	return 0;

error:
	free_con_param_list();
	return -1;
}

static int pg_init_com_params()
{
	str connect_timeout_str = str_init("connect_timeout");
	int ret = 0;

	int connect_timeout_set = 0;
	if(pg_con_param_list != NULL) {
		LM_INFO("postgres connection params:");
		pg_con_param_t *con_param = pg_con_param_list;
		while(con_param) {
			LM_INFO("%s=%s", con_param->name, con_param->value);

			/* check if connect_timeout parameter is set */
			if(strncmp(con_param->name, connect_timeout_str.s,
					   connect_timeout_str.len)
					== 0) {
				connect_timeout_set = 1;
			}

			con_param = con_param->next;
		}
	}

	/* For backward compatibility take pg_timeout param into account */
	if(pg_timeout > 0 && connect_timeout_set == 0) {
		str connect_timeout_val_str;
		char timeout_val[16] = {0};

		snprintf(timeout_val, sizeof(timeout_val) - 1, "%d", pg_timeout);
		connect_timeout_val_str.s = timeout_val;
		connect_timeout_val_str.len = strlen(connect_timeout_val_str.s);

		/* add connect_timeout parameter with pg_timeout value */
		ret = add_con_param(&connect_timeout_str, &connect_timeout_val_str);
		LM_INFO("%.*s=%.*s added with given timeout param",
				connect_timeout_str.len, connect_timeout_str.s,
				connect_timeout_val_str.len, connect_timeout_val_str.s);
	}

	return ret;
}

static int pg_con_param(modparam_t type, void *val)
{
	param_t *params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	str s;

	if(val == NULL) {
		free_con_param_list();
		return -1;
	}
	s.s = (char *)val;
	s.len = strlen(s.s);
	if(s.s[s.len - 1] == ';') {
		s.len--;
	}

	if(parse_params(&s, CLASS_ANY, &phooks, &params_list) < 0) {
		free_con_param_list();
		return -1;
	}

	/* parse parameter values */
	for(pit = params_list; pit; pit = pit->next) {

		if(pit->name.len == 0 || pit->body.len == 0) {
			LM_ERR("invalid con_param parameter\n");
			free_con_param_list();
			return -1;
		}

		if(add_con_param(&pit->name, &pit->body) < 0) {
			free_con_param_list();
			return -1;
		}
	}
	return 0;
}

/** @} */
