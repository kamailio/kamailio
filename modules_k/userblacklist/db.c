/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief USERBLACKLIST :: database access
 * \ingroup userblacklist
 * - Module: \ref userblacklist
 */

#include "db.h"
#include "dt.h"

#include "../../db/db.h"
#include "../../mem/mem.h"
#include "../../ut.h"


static db_con_t *dbc;
static db_func_t dbf;

static str prefix_col = str_init("prefix");
static str whitelist_col = str_init("whitelist");
static str username_key = str_init("username");
static str domain_key = str_init("domain");


int db_bind(const str *url)
{
	if (db_bind_mod(url, &dbf) < 0) {
		LM_ERR("can't bind to database module.\n");
		return -1;
	}

	return 0;
}


int db_init(const str *url, const str *table)
{
	dbc = dbf.init(url);
	if (!dbc) {
		LM_ERR("child can't connect to database.\n");
		return -1;
	}
	if(db_check_table_version(&dbf, dbc, table, 1) < 0) {
		LM_ERR("during table version check.\n");
		return -1;
	}

	return 0;
}


void db_destroy(void)
{
	if (dbc) {
		dbf.close(dbc);
	}
}


/**
 * Builds a d-tree using database entries.
 * \return negative on failure, postive on success, indicating the number of d-tree entries
 */
int db_build_userbl_tree(const str *username, const str *domain, const str *table, struct dt_node_t *root, int use_domain)
{
	db_key_t columns[2] = { &prefix_col, &whitelist_col };
	db_key_t key[2] = { &username_key, &domain_key };

	db_val_t val[2];
	VAL_TYPE(val) = VAL_TYPE(val + 1) = DB_STR;
	VAL_NULL(val) = VAL_NULL(val + 1) = 0;
	VAL_STR(val).s = username->s;
	VAL_STR(val).len = username->len;
	VAL_STR(val + 1).s = domain->s;
	VAL_STR(val + 1).len = domain->len;

	db_res_t *res;
	int i;
	int n = 0;
	
	if (dbf.use_table(dbc, table) < 0) {
		LM_ERR("cannot use table '%.*s'.\n", table->len, table->s);
		return -1;
	}
	if (dbf.query(dbc, key, 0, val, columns, (!use_domain) ? (1) : (2), 2, 0, &res) < 0) {
		LM_ERR("error while executing query.\n");
		return -1;
	}

	dt_clear(root);

	if (RES_COL_N(res) > 1) {
		for(i = 0; i < RES_ROW_N(res); i++) {
			if ((!RES_ROWS(res)[i].values[0].nul) && (!RES_ROWS(res)[i].values[1].nul)) {
				if ((RES_ROWS(res)[i].values[0].type == DB_STRING) &&
					(RES_ROWS(res)[i].values[1].type == DB_INT)) {

					/* LM_DBG("insert into tree prefix %s, whitelist %d",
						RES_ROWS(res)[i].values[0].val.string_val,
						RES_ROWS(res)[i].values[1].val.int_val); */
					dt_insert(root, RES_ROWS(res)[i].values[0].val.string_val,
						RES_ROWS(res)[i].values[1].val.int_val);
					n++;
				}
				else {
					LM_ERR("got invalid result type from query.\n");
				}
			}
		}
	}
	dbf.free_result(dbc, res);

	return n;
}


/**
 * Rebuild d-tree using database entries
 * \return negative on failure, positive on success, indicating the number of d-tree entries
 */
int db_reload_source(const str *table, struct dt_node_t *root)
{
	db_key_t columns[2] = { &prefix_col, &whitelist_col };
	db_res_t *res;
	int i;
	int n = 0;
	
	if (dbf.use_table(dbc, table) < 0) {
		LM_ERR("cannot use table '%.*s'.\n", table->len, table->s);
		return -1;
	}
	if (dbf.query(dbc, NULL, NULL, NULL, columns, 0, 2, NULL, &res) < 0) {
		LM_ERR("error while executing query.\n");
		return -1;
	}

	dt_clear(root);

	if (RES_COL_N(res) > 1) {
		for(i = 0; i < RES_ROW_N(res); i++) {
			if ((!RES_ROWS(res)[i].values[0].nul) && (!RES_ROWS(res)[i].values[1].nul)) {
				if ((RES_ROWS(res)[i].values[0].type == DB_STRING) &&
					(RES_ROWS(res)[i].values[1].type == DB_INT)) {

					/* LM_DBG("insert into tree prefix %s, whitelist %d",
						RES_ROWS(res)[i].values[0].val.string_val,
						RES_ROWS(res)[i].values[1].val.int_val); */
					dt_insert(root, RES_ROWS(res)[i].values[0].val.string_val,
						RES_ROWS(res)[i].values[1].val.int_val);
					n++;
				}
				else {
					LM_ERR("got invalid result type from query.\n");
				}
			}
		}
	}
	dbf.free_result(dbc, res);

	return n;
}
