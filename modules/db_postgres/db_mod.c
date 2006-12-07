/*
 * $Id$
 *
 * Postgres module interface
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005 iptelorg GmbH
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

#include <stdio.h>
#include "../../sr_module.h"
#include "dbase.h"

MODULE_VERSION

int connect_timeout = 0; /* Default is unlimited */
int reconnect_attempts = 2; /* How many times should the module try to reconnect if
			     * the connection is lost. 0 disables reconnecting */

/*
 * Postgres database module interface
 */
static cmd_export_t cmds[]={
	{"db_use_table",   (cmd_function)pg_use_table,      2, 0, 0},
	{"db_init",        (cmd_function)pg_init,           1, 0, 0},
	{"db_close",       (cmd_function)pg_close,          2, 0, 0},
	{"db_query",       (cmd_function)pg_query,          2, 0, 0},
	{"db_raw_query",   (cmd_function)pg_raw_query,      2, 0, 0},
	{"db_free_result", (cmd_function)pg_db_free_result, 2, 0, 0},
	{"db_insert",      (cmd_function)pg_insert,         2, 0, 0},
	{"db_delete",      (cmd_function)pg_delete,         2, 0, 0},
	{"db_update",      (cmd_function)pg_update,         2, 0, 0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"connect_timeout",    PARAM_INT, &connect_timeout   },
	{"reconnect_attempts", PARAM_INT, &reconnect_attempts},
	{0, 0, 0}
};


/*
 * create table test (
 *	bool_col BOOL NULL,
 *	int2_col INT2 NULL,
 *	int4_col INT4 NULL,
 *	int8_col INT8 NULL,
 *	float4_col FLOAT4 NULL,
 *	float8_col FLOAT8 NULL,
 *	timestamp_col TIMESTAMP NULL,
 *	char_col CHAR NULL,
 *	text_col TEXT NULL,
 *	bpchar_col BPCHAR NULL,
 *	varchar_col VARCHAR(255) NULL,
 *	bytea_col BYTEA NULL,
 *	bit_col BIT(32) NULL,
 *	varbit_col VARBIT NULL
 * );
 *
 * insert into test (bool_col, int2_col, int4_col, int8_col, float4_col, float8_col,
 *                   timestamp_col, char_col, text_col, bpchar_col, varchar_col,
 *                   bytea_col, bit_col, varbit_col)
 *                   values
 *                   (true, 22, 23, 24, 25.21, 25.22, '1999-10-18 21:35:00', 'a',
 *                   'ab', 'a', 'abcde', 'abcdddd', B'00110011001100110011001100110011',
 *                    b'10101010101010101010101010101010');
 */
#if 0
static int pg_test(void)
{
	int row, col;
	db_res_t* res;
	db_con_t* con;
	struct tm* tt;
	db_key_t keys[1];
	db_val_t vals[1];

	con = pg_init("postgres://ser:heslo@localhost/ser");
	if (!con) {
		ERR("Unable to connect database\n");
		return -1;
	}
	INFO("Successfuly connected\n");
	pg_use_table(con, "test");

	keys[0] = "int4_col";
	vals[0].type = DB_INT;
	vals[0].nul = 1;
	vals[0].val.int_val = 1;

	pg_query(con, keys, 0, vals, 0, 1, 0, 0, &res);
	if (!res) {
		ERR("No result received\n");
		return -1;
	}
	if (!res->n) {
		ERR("Result contains no rows\n");
		return -1;
	} else {
		INFO("Result contains %d rows\n", res->n);
	}

	INFO("Result contains %d columns\n", res->col.n);

	for(row = 0; row < res->n; row++) {
		for(col = 0; col < res->col.n; col++) {
			switch(res->col.types[col]) {
			case DB_INT:
				INFO("INT(%d)", res->rows[row].values[col].val.int_val);
				break;

			case DB_FLOAT:
				INFO("FLOAT(%f)", res->rows[row].values[col].val.float_val);
				break;

			case DB_DOUBLE:
				INFO("DOUBLE(%f)", res->rows[row].values[col].val.double_val);
				break;

			case DB_STRING:
				INFO("STRING(%s)", res->rows[row].values[col].val.string_val);
				break;

			case DB_STR:
				INFO("STR(%.*s)", res->rows[row].values[col].val.str_val.len, res->rows[row].values[col].val.str_val.s);
				break;

			case DB_DATETIME:
				tt = gmtime(&res->rows[row].values[col].val.time_val);
				INFO("DATETIME(%s)", asctime(tt));
				break;

			case DB_BLOB:
				INFO("BLOB(%.*s)", res->rows[row].values[col].val.str_val.len, res->rows[row].values[col].val.str_val.s);
				break;

			case DB_BITMAP:
				INFO("INT(%x)", res->rows[row].values[col].val.bitmap_val);
				break;

			default:
				ERR("Unsupported column type\n");
				return -1;
			}
		}
	}

	pg_close(con);
	return -1;
}
#endif


struct module_exports exports = {
	"postgres",
	cmds,
	0,         /* RPC methods */
	params,    /*  module parameters */
        0,         /* module initialization function */
	0,         /* response function*/
	0,         /* destroy function */
	0,         /* oncancel function */
	0          /* per-child init function */
};
