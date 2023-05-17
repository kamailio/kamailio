/*
 * DBText module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2014 Edvina AB, Olle E. Johansson
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
 *
 */

#include <stdio.h>
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../core/rpc_lookup.h"
#include "db_text.h"
#include "dbt_lib.h"
#include "dbt_api.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);

#define DEFAULT_DB_TEXT_READ_BUFFER_SIZE 16384
#define DEFAULT_MAX_RESULT_ROWS 100000;

/*
 * Module parameter variables
 */
int db_mode = 0;	  /* Database usage mode: 0 = cache, 1 = no cache */
int empty_string = 0; /* Treat empty string as "" = 0, 1 = NULL */
int _db_text_read_buffer_size = DEFAULT_DB_TEXT_READ_BUFFER_SIZE;
int _db_text_max_result_rows = DEFAULT_MAX_RESULT_ROWS;
int _dbt_delim = ':';				/* ':' is the default delim */
str _dbt_delim_str = str_init(":"); /* ':' is the default delim */
str dbt_default_connection = str_init("");

int dbt_bind_api(db_func_t *dbb);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
		{"db_bind_api", (cmd_function)dbt_bind_api, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0}};


/*
 * Exported parameters
 */
static param_export_t params[] = {{"db_mode", INT_PARAM, &db_mode},
		{"emptystring", INT_PARAM, &empty_string},
		{"file_buffer_size", INT_PARAM, &_db_text_read_buffer_size},
		{"max_result_rows", INT_PARAM, &_db_text_max_result_rows},
		{"default_connection", PARAM_STR, &dbt_default_connection},
		{"db_delim", PARAM_STR, &_dbt_delim_str}, {0, 0, 0}};

static rpc_export_t rpc_methods[];

struct module_exports exports = {
		"db_text",		 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* cmd (cfg function) exports */
		params,			 /* param exports */
		0,				 /* RPC method exports */
		0,				 /* pseudo-variables exports */
		0,				 /* response handling function */
		mod_init,		 /* module init function */
		0,				 /* per-child init function */
		destroy			 /* module destroy function */
};

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(rpc_register_array(rpc_methods) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(db_api_init() < 0)
		return -1;
	return 0;
}

static int mod_init(void)
{
	if(_dbt_delim_str.len != 1) {
		LM_ERR("db_delim must be a character, defaulting to \":\"\n");
		pkg_free(_dbt_delim_str.s);
		_dbt_delim_str.s = ":";
		_dbt_delim_str.len = 1;
	}
	_dbt_delim = _dbt_delim_str.s[0];

	if(dbt_init_cache())
		return -1;
	/* return make_demo(); */

	return 0;
}

static void destroy(void)
{
	LM_DBG("destroy ...\n");
	dbt_cache_print2(0, 0);
	dbt_cache_destroy();
}


int dbt_bind_api(db_func_t *dbb)
{
	if(dbb == NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table = dbt_use_table;
	dbb->init = dbt_init;
	dbb->close = dbt_close;
	dbb->query = (db_query_f)dbt_query;
	dbb->fetch_result = (db_fetch_result_f)dbt_fetch_result;
	dbb->free_result = dbt_free_result;
	dbb->insert = (db_insert_f)dbt_insert;
	dbb->delete = (db_delete_f)dbt_delete;
	dbb->update = (db_update_f)dbt_update;
	dbb->replace = (db_replace_f)dbt_replace;
	dbb->affected_rows = (db_affected_rows_f)dbt_affected_rows;
	dbb->raw_query = (db_raw_query_f)dbt_raw_query;
	dbb->cap = DB_CAP_ALL | DB_CAP_AFFECTED_ROWS | DB_CAP_RAW_QUERY
			   | DB_CAP_REPLACE | DB_CAP_FETCH;

	return 0;
}

/* rpc function documentation */
static const char *rpc_dump_doc[2] = {"Write back to disk modified tables", 0};

/* rpc function implementations */
static void rpc_dump(rpc_t *rpc, void *c)
{
	if(0 != dbt_cache_print(0))
		rpc->rpl_printf(c, "Dump failed");
	else
		rpc->rpl_printf(c, "Dump OK");

	return;
}

static const char *rpc_query_doc[2] = {"Perform Live Query", 0};

/* rpc function implementations */
static void rpc_query(rpc_t *rpc, void *ctx)
{
#ifdef __OS_linux
	str sql;
	db1_con_t *con;
	db1_res_t *_r;
	int res;
	int n;
	char *buf;
	size_t len;
	FILE *stream;
	dbt_table_p tab;
	dbt_row_p rowp;

	rpc->scan(ctx, "S", &sql);

	con = dbt_init(&dbt_default_connection);
	if(con == NULL) {
		rpc->rpl_printf(
				ctx, "invalid connection : %s", dbt_default_connection.s);
		return;
	}

	res = dbt_raw_query(con, &sql, &_r);
	if(res != 0) {
		rpc->rpl_printf(ctx, "error executing sql statement");
		goto end;
	}


	if(_r) {
		tab = (dbt_table_p)_r->ptr;
		if(_r->n == 0) {
			rpc->rpl_printf(ctx, "statement returned 0 rows");
		} else {
			stream = open_memstream(&buf, &len);
			if(stream == NULL) {
				rpc->rpl_printf(ctx, "error opening stream");
				goto end;
			}
			dbt_print_table_header(tab, stream);
			fflush(stream);
			buf[len] = '\0';
			rpc->rpl_printf(ctx, "%s", buf);
			rowp = tab->rows;
			for(n = 0; n < _r->n; n++) {
				fseeko(stream, 0, SEEK_SET);
				dbt_print_table_row_ex(tab, rowp, stream, 0);
				fflush(stream);
				buf[len] = '\0';
				rpc->rpl_printf(ctx, "%s", buf);
				rowp = rowp->next;
			}
			fclose(stream);
			free(buf);
			rpc->rpl_printf(ctx, "\ntotal rows %d / %d", _r->n, tab->nrrows);
		}
	} else {
		rpc->rpl_printf(
				ctx, "%d affected rows", ((dbt_con_p)con->tail)->affected);
	}

	if(_r)
		dbt_free_result(con, _r);

end:
	dbt_close(con);
#else
	rpc->fault(ctx, 500, "Command available on Linux only");
#endif
}

static rpc_export_t rpc_methods[] = {
		{"db_text.dump", rpc_dump, rpc_dump_doc, 0},
		{"db_text.query", rpc_query, rpc_query_doc, 0}, {0, 0, 0, 0}};
