/*
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/**
 * \file cr_db.c
 * \brief Functions for loading routing data from a database.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "carrierroute.h"
#include "cr_db.h"
#include "cr_carrier.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#define QUERY_LEN 2048

static int columns_load_num, failure_columns_load_num, load_comments;
static char query[QUERY_LEN];

str * columns[COLUMN_NUM] = { &carrierroute_id_col, &carrierroute_carrier_col,
	&carrierroute_domain_col,
	&carrierroute_scan_prefix_col,
	&carrierroute_flags_col,
	&carrierroute_mask_col,
	&carrierroute_prob_col,
	&carrierroute_rewrite_host_col,
	&carrierroute_strip_col,
	&carrierroute_rewrite_prefix_col,
	&carrierroute_rewrite_suffix_col,
	&carrierroute_description_col
};

str * carrier_name_columns[CARRIER_NAME_COLUMN_NUM] = {
	&carrier_name_id_col,
	&carrier_name_carrier_col
};

str * domain_name_columns[DOMAIN_NAME_COLUMN_NUM] = {
	&domain_name_id_col,
	&domain_name_domain_col
};

str * failure_columns[FAILURE_COLUMN_NUM] = {
	&carrierfailureroute_id_col,
	&carrierfailureroute_carrier_col,
	&carrierfailureroute_domain_col,
	&carrierfailureroute_scan_prefix_col,
	&carrierfailureroute_host_name_col,
	&carrierfailureroute_reply_code_col,
	&carrierfailureroute_flags_col,
	&carrierfailureroute_mask_col,
	&carrierfailureroute_next_domain_col,
	&carrierfailureroute_description_col
};


void set_load_comments_params(int lc) {
	load_comments = lc;
	columns_load_num = lc ? COLUMN_NUM : COLUMN_NUM_NO_COMMENT;
	failure_columns_load_num = lc ? FAILURE_COLUMN_NUM : FAILURE_COLUMN_NUM_NO_COMMENT;
}




static int load_carrier_map(struct route_data_t *rd) {
	db1_res_t * res = NULL;
	int i, count;
	if(!rd){
		LM_ERR("invalid parameter\n");
		return -1;
	}
	if (carrierroute_dbf.use_table(carrierroute_dbh, &carrier_name_table) < 0) {
		LM_ERR("couldn't use table\n");
		return -1;
	}

	if (carrierroute_dbf.query(carrierroute_dbh, 0, 0, 0, (db_key_t *)carrier_name_columns, 0, CARRIER_NAME_COLUMN_NUM, 0, &res) < 0) {
		LM_ERR("couldn't query table\n");
		return -1;
	}

	count = RES_ROW_N(res);
	if (count == 0) {
		LM_ERR("empty %.*s table", carrier_name_table.len, carrier_name_table.s);
		carrierroute_dbf.free_result(carrierroute_dbh, res);
		return 0;
	}

	rd->carrier_map = shm_malloc(sizeof(struct name_map_t) * count);
	if (rd->carrier_map == NULL) {
		SHM_MEM_ERROR;
		carrierroute_dbf.free_result(carrierroute_dbh, res);
		return -1;
	}
	memset(rd->carrier_map, 0, sizeof(struct name_map_t) * count);

	for (i=0; i<count; i++) {
		rd->carrier_map[i].id = res->rows[i].values[CARRIER_NAME_ID_COL].val.int_val;
		rd->carrier_map[i].name.len = strlen(res->rows[i].values[CARRIER_NAME_NAME_COL].val.string_val);
		rd->carrier_map[i].name.s = shm_malloc(rd->carrier_map[i].name.len);
		if (rd->carrier_map[i].name.s == NULL) {
			SHM_MEM_ERROR;
			carrierroute_dbf.free_result(carrierroute_dbh, res);
			shm_free(rd->carrier_map);
			rd->carrier_map = NULL;
			return -1;
		}
		memcpy(rd->carrier_map[i].name.s, res->rows[i].values[CARRIER_NAME_NAME_COL].val.string_val, rd->carrier_map[i].name.len);
	}

	/* sort carrier map by id for faster access */
	qsort(rd->carrier_map, count, sizeof(rd->carrier_map[0]), compare_name_map);

	carrierroute_dbf.free_result(carrierroute_dbh, res);
	return count;
}




static int load_domain_map(struct route_data_t *rd) {
	db1_res_t * res = NULL;
	int i, count;
	if(!rd){
		LM_ERR("invalid parameter\n");
		return -1;
	}
	if (carrierroute_dbf.use_table(carrierroute_dbh, &domain_name_table) < 0) {
		LM_ERR("couldn't use table\n");
		return -1;
	}

	if (carrierroute_dbf.query(carrierroute_dbh, 0, 0, 0, (db_key_t *)domain_name_columns, 0, DOMAIN_NAME_COLUMN_NUM, 0, &res) < 0) {
		LM_ERR("couldn't query table\n");
		return -1;
	}

	count = RES_ROW_N(res);
	if (count == 0) {
		LM_ERR("empty %.*s table", domain_name_table.len, domain_name_table.s);
		carrierroute_dbf.free_result(carrierroute_dbh, res);
		return 0;
	}

	rd->domain_map = shm_malloc(sizeof(struct name_map_t) * count);
	if (rd->domain_map == NULL) {
		SHM_MEM_ERROR;
		carrierroute_dbf.free_result(carrierroute_dbh, res);
		return -1;
	}
	memset(rd->domain_map, 0, sizeof(struct name_map_t) * count);

	for (i=0; i<count; i++) {
		rd->domain_map[i].id = res->rows[i].values[DOMAIN_NAME_ID_COL].val.int_val;
		rd->domain_map[i].name.len = strlen(res->rows[i].values[DOMAIN_NAME_NAME_COL].val.string_val);
		rd->domain_map[i].name.s = shm_malloc(rd->domain_map[i].name.len);
		if (rd->domain_map[i].name.s == NULL) {
			SHM_MEM_ERROR;
			carrierroute_dbf.free_result(carrierroute_dbh, res);
			shm_free(rd->domain_map);
			rd->domain_map = NULL;
			return -1;
		}
		memcpy(rd->domain_map[i].name.s, res->rows[i].values[DOMAIN_NAME_NAME_COL].val.string_val, rd->domain_map[i].name.len);
	}

	/* sort domain map by id for faster access */
	qsort(rd->domain_map, count, sizeof(rd->domain_map[0]), compare_name_map);

	carrierroute_dbf.free_result(carrierroute_dbh, res);
	return count;
}




int load_user_carrier(str * user, str * domain) {
	db1_res_t * res;
	db_key_t cols[1];
	db_key_t keys[2];
	db_val_t vals[2];
	db_op_t op[2];
	int id;
	int use_domain = cfg_get(carrierroute, carrierroute_cfg, use_domain);
	if (!user || (use_domain  && !domain)) {
		LM_ERR("NULL pointer in parameter\n");
		return -1;
	}

	cols[0] = subscriber_columns[SUBSCRIBER_CARRIER_COL];

	keys[0] = subscriber_columns[SUBSCRIBER_USERNAME_COL];
	op[0] = OP_EQ;
	VAL_TYPE(vals) = DB1_STR;
	VAL_NULL(vals) = 0;
	VAL_STR(vals) = *user;

	keys[1] = subscriber_columns[SUBSCRIBER_DOMAIN_COL];
	op[1] = OP_EQ;
	VAL_TYPE(vals+1) = DB1_STR;
	VAL_NULL(vals+1) = 0;
	VAL_STR(vals+1) = *domain;

	if (carrierroute_dbf.use_table(carrierroute_dbh, &subscriber_table) < 0) {
		LM_ERR("can't use table\n");
		return -1;
	}

	if (carrierroute_dbf.query(carrierroute_dbh, keys, op, vals, cols, use_domain ? 2 : 1, 1, NULL, &res) < 0) {
		LM_ERR("can't query database\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		carrierroute_dbf.free_result(carrierroute_dbh, res);
		return 0;
	}

	if (VAL_NULL(ROW_VALUES(RES_ROWS(res)))) {
		carrierroute_dbf.free_result(carrierroute_dbh, res);
		return 0;
	}

	id = VAL_INT(ROW_VALUES(RES_ROWS(res)));
	carrierroute_dbf.free_result(carrierroute_dbh, res);
	return id;
}




/**
 * Loads the routing data from the database given in global
 * variable db_url and stores it in routing tree rd.
 *
 * @param rd Pointer to the route data tree where the routing data
 * shall be loaded into
 *
 * @return 0 means ok, -1 means an error occurred
 *
 */
int load_route_data_db(struct route_data_t * rd) {
	db1_res_t * res = NULL;
	db1_res_t * prob_res = NULL;
	db_row_t * row = NULL;
	int i, ret;
	struct carrier_data_t * tmp_carrier_data;
	static str query_str;
	str tmp_scan_prefix, tmp_rewrite_host, tmp_rewrite_prefix,
		tmp_rewrite_suffix, tmp_host_name, tmp_reply_code, tmp_comment;
	str *p_tmp_comment;

	if( (strlen("SELECT DISTINCT  FROM  WHERE = ")
			+ carrierroute_table.len + columns[COL_DOMAIN]->len
			+ columns[COL_CARRIER]->len + 20) >  QUERY_LEN) {
		LM_ERR("query too long\n");
		return -1;
	}

	if((ret = load_carrier_map(rd)) <= 0){
		LM_ERR("error while retrieving carriers\n");
		goto errout;
	}
	rd->carrier_num = (size_t)ret;

	if((ret = load_domain_map(rd)) <= 0){
		LM_ERR("error while retrieving domains\n");
		goto errout;
	}
	rd->domain_num = (size_t)ret;

	if ((rd->carriers = shm_malloc(sizeof(struct carrier_data_t *) * rd->carrier_num)) == NULL) {
		SHM_MEM_ERROR;
		goto errout;
	}
	memset(rd->carriers, 0, sizeof(struct carrier_data_t *) * rd->carrier_num);

	for (i=0; i<rd->carrier_num; i++) {
		memset(query, 0, QUERY_LEN);
		ret = snprintf(query, QUERY_LEN, "SELECT DISTINCT %.*s FROM %.*s WHERE %.*s=%i",
		columns[COL_DOMAIN]->len, columns[COL_DOMAIN]->s, carrierroute_table.len,
		carrierroute_table.s, columns[COL_CARRIER]->len, columns[COL_CARRIER]->s, rd->carrier_map[i].id);
		if (ret < 0) {
			LM_ERR("error in snprintf");
			goto errout;
		}
		query_str.s = query;
		query_str.len = ret;

		if (carrierroute_dbf.raw_query(carrierroute_dbh, &query_str, &res) < 0) {
			LM_ERR("Failed to query database.\n");
			goto errout;
		}
		LM_INFO("carrier '%.*s' (id %i) has %i domains\n", rd->carrier_map[i].name.len, rd->carrier_map[i].name.s, rd->carrier_map[i].id, RES_ROW_N(res));
		tmp_carrier_data = create_carrier_data(rd->carrier_map[i].id, &rd->carrier_map[i].name, RES_ROW_N(res));
		if (tmp_carrier_data == NULL) {
			LM_ERR("can't create new carrier '%.*s'\n", rd->carrier_map[i].name.len, rd->carrier_map[i].name.s);
			goto errout;
		}
		if (add_carrier_data(rd, tmp_carrier_data) < 0) {
			LM_ERR("can't add carrier '%.*s'\n", rd->carrier_map[i].name.len, rd->carrier_map[i].name.s);
			destroy_carrier_data(tmp_carrier_data);
			goto errout;
		}
		carrierroute_dbf.free_result(carrierroute_dbh, res);
		res = NULL;
	}

	if (carrierroute_dbf.use_table(carrierroute_dbh, &carrierroute_table) < 0) {
		LM_ERR("Cannot set database table '%.*s'.\n", carrierroute_table.len, carrierroute_table.s);
		return -1;
	}

	if (DB_CAPABILITY(carrierroute_dbf, DB_CAP_FETCH)) {
		if (carrierroute_dbf.query(carrierroute_dbh, NULL, NULL, NULL, (db_key_t *) columns, 0,
				columns_load_num, NULL, NULL) < 0) {
			LM_ERR("Failed to query database to prepare fetch row.\n");
			return -1;
		}
		if(carrierroute_dbf.fetch_result(carrierroute_dbh, &res, cfg_get(carrierroute, carrierroute_cfg, fetch_rows)) < 0) {
			LM_ERR("Fetching rows failed\n");
			return -1;
		}
	} else {
		if (carrierroute_dbf.query(carrierroute_dbh, NULL, NULL, NULL, (db_key_t *) columns, 0,
				columns_load_num, NULL, &res) < 0) {
			LM_ERR("Failed to query database.\n");
			return -1;
		}
	}
	int n = 0;
	crboolean query_done = crfalse;
	do {
		LM_DBG("loading, cycle %d", n++);
		for (i = 0; i < RES_ROW_N(res); ++i) {
			row = &RES_ROWS(res)[i];
			tmp_scan_prefix.s=(char *)row->values[COL_SCAN_PREFIX].val.string_val;
			tmp_rewrite_host.s=(char *)row->values[COL_REWRITE_HOST].val.string_val;
			tmp_rewrite_prefix.s=(char *)row->values[COL_REWRITE_PREFIX].val.string_val;
			tmp_rewrite_suffix.s=(char *)row->values[COL_REWRITE_SUFFIX].val.string_val;
			if (tmp_scan_prefix.s==NULL) tmp_scan_prefix.s="";
			if (tmp_rewrite_host.s==NULL) tmp_rewrite_host.s="";
			if (tmp_rewrite_prefix.s==NULL) tmp_rewrite_prefix.s="";
			if (tmp_rewrite_suffix.s==NULL) tmp_rewrite_suffix.s="";
			tmp_scan_prefix.len=strlen(tmp_scan_prefix.s);
			tmp_rewrite_host.len=strlen(tmp_rewrite_host.s);
			tmp_rewrite_prefix.len=strlen(tmp_rewrite_prefix.s);
			tmp_rewrite_suffix.len=strlen(tmp_rewrite_suffix.s);

			p_tmp_comment = NULL;
			if (load_comments) {
				tmp_comment.s = (char *)row->values[COL_COMMENT].val.string_val;
				if (tmp_comment.s==NULL) tmp_comment.s="";
				tmp_comment.len=strlen(tmp_comment.s);
				p_tmp_comment = &tmp_comment;
			}

			if (add_route(rd,
					row->values[COL_CARRIER].val.int_val,
					row->values[COL_DOMAIN].val.int_val,
					&tmp_scan_prefix,
					row->values[COL_FLAGS].val.int_val,
					row->values[COL_MASK].val.int_val,
					0,
					row->values[COL_PROB].val.double_val,
					&tmp_rewrite_host,
					row->values[COL_STRIP].val.int_val,
					&tmp_rewrite_prefix,
					&tmp_rewrite_suffix,
					1,
					0,
					-1,
					NULL,
					p_tmp_comment) == -1) {
				goto errout;
			}
			if (row->values[COL_PROB].val.double_val == 0 && !query_done) {
				int ret_tmp;
				char query_tmp[QUERY_LEN];
				str query_tmp_str;

				memset(query_tmp, 0, QUERY_LEN);
				ret_tmp = snprintf(query_tmp, QUERY_LEN, "SELECT * FROM %.*s WHERE %.*s=%d and %.*s=%d and %.*s>%d",
						carrierroute_table.len, carrierroute_table.s, columns[COL_CARRIER]->len, columns[COL_CARRIER]->s, row->values[COL_CARRIER].val.int_val,
						columns[COL_DOMAIN]->len, columns[COL_DOMAIN]->s, row->values[COL_DOMAIN].val.int_val, columns[COL_PROB]->len, columns[COL_PROB]->s, 0);

				if (ret_tmp < 0) {
					LM_ERR("error in snprintf while querying prob column");
					goto errout;
				}
				query_tmp_str.s = query_tmp;
				query_tmp_str.len = ret_tmp;

				if (carrierroute_dbf.raw_query(carrierroute_dbh, &query_tmp_str, &prob_res) < 0) {
					LM_ERR("Failed to query carrierroute db table based on prob column.\n");
					goto errout;
				}
				if(RES_ROW_N(prob_res) == 0) {
					LM_ERR("Carrierroute db table contains route(s) with only 0 probability.\n");
					query_done = crtrue;
				}
				carrierroute_dbf.free_result(carrierroute_dbh, prob_res);
				prob_res = NULL;
			}

		}
		if (DB_CAPABILITY(carrierroute_dbf, DB_CAP_FETCH)) {
			if(carrierroute_dbf.fetch_result(carrierroute_dbh, &res,  cfg_get(carrierroute, carrierroute_cfg, fetch_rows)) < 0) {
				LM_ERR("fetching rows failed\n");
				carrierroute_dbf.free_result(carrierroute_dbh, res);
				return -1;
			}
		} else {
			break;
		}
	} while(RES_ROW_N(res) > 0);

	carrierroute_dbf.free_result(carrierroute_dbh, res);
	res = NULL;
	
	if (carrierroute_dbf.use_table(carrierroute_dbh, &carrierfailureroute_table) < 0) {
		LM_ERR("cannot set database table '%.*s'.\n",
				carrierfailureroute_table.len, carrierfailureroute_table.s);
		return -1;
	}
	if (carrierroute_dbf.query(carrierroute_dbh, NULL, NULL, NULL, (db_key_t *)failure_columns, 0,
			failure_columns_load_num, NULL, &res) < 0) {
		LM_ERR("failed to query database.\n");
		return -1;
	}
	for (i = 0; i < RES_ROW_N(res); ++i) {
		row = &RES_ROWS(res)[i];
		tmp_scan_prefix.s=(char *)row->values[FCOL_SCAN_PREFIX].val.string_val;
		tmp_host_name.s=(char *)row->values[FCOL_HOST_NAME].val.string_val;
		tmp_reply_code.s=(char *)row->values[FCOL_REPLY_CODE].val.string_val;
		if (tmp_scan_prefix.s==NULL) tmp_scan_prefix.s="";
		if (tmp_host_name.s==NULL) tmp_host_name.s="";
		if (tmp_reply_code.s==NULL) tmp_reply_code.s="";
		tmp_scan_prefix.len=strlen(tmp_scan_prefix.s);
		tmp_host_name.len=strlen(tmp_host_name.s);
		tmp_reply_code.len=strlen(tmp_reply_code.s);
		p_tmp_comment = NULL;

		if (load_comments) {
			tmp_comment.s = (char *)row->values[FCOL_COMMENT].val.string_val;
			if (tmp_comment.s==NULL) tmp_comment.s="";
			tmp_comment.len=strlen(tmp_comment.s);
			p_tmp_comment = &tmp_comment;
		}

		if (add_failure_route(rd,
				row->values[FCOL_CARRIER].val.int_val,
				row->values[COL_DOMAIN].val.int_val,
				&tmp_scan_prefix,
				&tmp_host_name,
				&tmp_reply_code,
				row->values[FCOL_FLAGS].val.int_val,
				row->values[FCOL_MASK].val.int_val,
				row->values[FCOL_NEXT_DOMAIN].val.int_val,
				p_tmp_comment) == -1) {
			goto errout;
		}
	}

	carrierroute_dbf.free_result(carrierroute_dbh, res);
	return 0;

errout:
	if (res) {
		carrierroute_dbf.free_result(carrierroute_dbh, res);
	}
	if (prob_res) {
		carrierroute_dbf.free_result(carrierroute_dbh, prob_res);
	}
	return -1;
}
