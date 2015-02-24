/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief USERBLACKLIST :: database access
 * \ingroup userblacklist
 * - Module: \ref userblacklist
 */

#include "db.h"
#include "db_userblacklist.h"

#include "../../lib/srdb1/db.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../lib/trie/dtrie.h"



/**
 * Builds a d-tree using database entries.
 * \return negative on failure, postive on success, indicating the number of d-tree entries
 */

extern int match_mode;

int db_build_userbl_tree(const str *username, const str *domain, const str *table, struct dtrie_node_t *root, int use_domain)
{
	db_key_t columns[2] = { &userblacklist_prefix_col, &userblacklist_whitelist_col };
	db_key_t key[2] = { &userblacklist_username_col, &userblacklist_domain_col };

	db_val_t val[2];
	db1_res_t *res;
	int i;
	int n = 0;
	void *nodeflags;
	VAL_TYPE(val) = VAL_TYPE(val + 1) = DB1_STR;
	VAL_NULL(val) = VAL_NULL(val + 1) = 0;
	VAL_STR(val).s = username->s;
	VAL_STR(val).len = username->len;
	VAL_STR(val + 1).s = domain->s;
	VAL_STR(val + 1).len = domain->len;

	
	if (userblacklist_dbf.use_table(userblacklist_dbh, table) < 0) {
		LM_ERR("cannot use table '%.*s'.\n", table->len, table->s);
		return -1;
	}
	if (userblacklist_dbf.query(userblacklist_dbh, key, 0, val, columns, (!use_domain) ? (1) : (2), 2, 0, &res) < 0) {
		LM_ERR("error while executing query.\n");
		return -1;
	}

	dtrie_clear(root, NULL, match_mode);

	if (RES_COL_N(res) > 1) {
		for(i = 0; i < RES_ROW_N(res); i++) {
			if ((!RES_ROWS(res)[i].values[0].nul) && (!RES_ROWS(res)[i].values[1].nul)) {
				if ((RES_ROWS(res)[i].values[0].type == DB1_STRING) &&
					(RES_ROWS(res)[i].values[1].type == DB1_INT)) {

					/* LM_DBG("insert into tree prefix %s, whitelist %d",
						RES_ROWS(res)[i].values[0].val.string_val,
						RES_ROWS(res)[i].values[1].val.int_val); */
					if (RES_ROWS(res)[i].values[1].val.int_val == 0) {
						nodeflags=(void *)MARK_BLACKLIST;
					} else {
						nodeflags=(void *)MARK_WHITELIST;
					}
					if (dtrie_insert(root, RES_ROWS(res)[i].values[0].val.string_val, strlen(RES_ROWS(res)[i].values[0].val.string_val),
						nodeflags, match_mode) < 0) LM_ERR("could not insert values into trie.\n");
					n++;
				}
				else {
					LM_ERR("got invalid result type from query.\n");
				}
			}
		}
	}
	userblacklist_dbf.free_result(userblacklist_dbh, res);

	return n;
}


/**
 * Rebuild d-tree using database entries
 * \return negative on failure, positive on success, indicating the number of d-tree entries
 */
int db_reload_source(const str *table, struct dtrie_node_t *root)
{
	db_key_t columns[2] = { &globalblacklist_prefix_col, &globalblacklist_whitelist_col };
	db1_res_t *res;
	int i;
	int n = 0;
	void *nodeflags;
	
	if (userblacklist_dbf.use_table(userblacklist_dbh, table) < 0) {
		LM_ERR("cannot use table '%.*s'.\n", table->len, table->s);
		return -1;
	}
	if (userblacklist_dbf.query(userblacklist_dbh, NULL, NULL, NULL, columns, 0, 2, NULL, &res) < 0) {
		LM_ERR("error while executing query.\n");
		return -1;
	}

	dtrie_clear(root, NULL, match_mode);

	if (RES_COL_N(res) > 1) {
		for(i = 0; i < RES_ROW_N(res); i++) {
			if ((!RES_ROWS(res)[i].values[0].nul) && (!RES_ROWS(res)[i].values[1].nul)) {
				if ((RES_ROWS(res)[i].values[0].type == DB1_STRING) &&
					(RES_ROWS(res)[i].values[1].type == DB1_INT)) {

					/* LM_DBG("insert into tree prefix %s, whitelist %d",
						RES_ROWS(res)[i].values[0].val.string_val,
						RES_ROWS(res)[i].values[1].val.int_val); */
					if (RES_ROWS(res)[i].values[1].val.int_val == 0) nodeflags=(void *) MARK_BLACKLIST;
					else nodeflags=(void *)MARK_WHITELIST;
					if (dtrie_insert(root, RES_ROWS(res)[i].values[0].val.string_val, strlen(RES_ROWS(res)[i].values[0].val.string_val),
						nodeflags, match_mode) < 0) LM_ERR("could not insert values into trie.\n");
					n++;
				}
				else {
					LM_ERR("got invalid result type from query.\n");
				}
			}
		}
	}
	userblacklist_dbf.free_result(userblacklist_dbh, res);

	return n;
}
