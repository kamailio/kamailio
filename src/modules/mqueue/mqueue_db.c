/**
 * Copyright (C) 2020 Julien Chavanton
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../lib/srdb1/db.h"
#include "mqueue_api.h"

/** database connection */
db1_con_t *mqueue_db_con = NULL;
db_func_t mq_dbf;

/** db parameters */
str mqueue_db_url = {0, 0};
str mq_db_key_column = str_init("key");
str mq_db_val_column = str_init("val");
str mq_db_id_column = str_init("id");

/**
 * initialize database connection
 */
int mqueue_db_init_con(void)
{
	if(mqueue_db_url.len <= 0) {
		LM_ERR("failed to connect to the database, no db url\n");
		return -1;
	}
	/* binding to DB module */
	if(db_bind_mod(&mqueue_db_url, &mq_dbf)) {
		LM_ERR("database module not found\n");
		return -1;
	}

	if(!DB_CAPABILITY(mq_dbf, DB_CAP_ALL)) {
		LM_ERR("database module does not "
			   "implement all functions needed by the module\n");
		return -1;
	}
	return 0;
}

/**
 * open database connection
 */
int mqueue_db_open_con(void)
{
	if(mqueue_db_init_con() == 0) {
		mqueue_db_con = mq_dbf.init(&mqueue_db_url);
		if(mqueue_db_con == NULL) {
			LM_ERR("failed to connect to the database\n");
			return -1;
		}
		LM_DBG("database connection opened successfully\n");
		return 0;
	}
	return 0;
}

/**
 * close database connection
 */
int mqueue_db_close_con(void)
{
	if(mqueue_db_con != NULL && mq_dbf.close != NULL)
		mq_dbf.close(mqueue_db_con);
	mqueue_db_con = NULL;
	return 0;
}

int mqueue_db_load_queue(str *name)
{
	int ncols = 2;
	db1_res_t *db_res = NULL;
	db_key_t db_cols[2] = {&mq_db_key_column, &mq_db_val_column};
	db_key_t db_ord = &mq_db_id_column;
	int mq_fetch_rows = 100;
	int ret = 0;
	str val = str_init("");
	str key = str_init("");
	int i;
	int cnt = 0;

	if(mqueue_db_open_con() != 0) {
		LM_ERR("no db connection\n");
		return -1;
	}

	if(mq_dbf.use_table(mqueue_db_con, name) < 0) {
		LM_ERR("failed to use_table\n");
		goto error;
	}

	LM_INFO("=============== loading queue table [%.*s] from database\n",
			name->len, name->s);

	if(DB_CAPABILITY(mq_dbf, DB_CAP_FETCH)) {
		if(mq_dbf.query(mqueue_db_con, 0, 0, 0, db_cols, 0, ncols, db_ord, 0)
				< 0) {
			LM_ERR("Error while querying db\n");
			goto error;
		}
		if(mq_dbf.fetch_result(mqueue_db_con, &db_res, mq_fetch_rows) < 0) {
			LM_ERR("Error while fetching result\n");
			if(db_res)
				mq_dbf.free_result(mqueue_db_con, db_res);
			goto error;
		} else {
			if(RES_ROW_N(db_res) == 0) {
				mq_dbf.free_result(mqueue_db_con, db_res);
				LM_DBG("Nothing to be loaded in queue\n");
				mqueue_db_close_con();
				return 0;
			}
		}
	} else {
		if((ret = mq_dbf.query(mqueue_db_con, NULL, NULL, NULL, db_cols, 0,
					ncols, 0, &db_res))
						!= 0
				|| RES_ROW_N(db_res) <= 0) {
			if(ret == 0) {
				mq_dbf.free_result(mqueue_db_con, db_res);
				mqueue_db_close_con();
				return 0;
			} else {
				goto error;
			}
		}
	}

	do {
		for(i = 0; i < RES_ROW_N(db_res); i++) {
			if(VAL_NULL(&RES_ROWS(db_res)[i].values[0])) {
				LM_ERR("mqueue [%.*s] row [%d] has NULL key string\n",
						name->len, name->s, i);
				goto error;
			}
			if(VAL_NULL(&RES_ROWS(db_res)[i].values[1])) {
				LM_ERR("mqueue [%.*s] row [%d] has NULL value string\n",
						name->len, name->s, i);
				goto error;
			}
			switch(RES_ROWS(db_res)[i].values[0].type) {
				case DB1_STR:
					key.s = (RES_ROWS(db_res)[i].values[0].val.str_val.s);
					if(key.s == NULL) {
						LM_ERR("mqueue [%.*s] row [%d] has NULL key\n",
								name->len, name->s, i);
						goto error;
					}
					key.len = (RES_ROWS(db_res)[i].values[0].val.str_val.len);
					break;
				case DB1_BLOB:
					key.s = (RES_ROWS(db_res)[i].values[0].val.blob_val.s);
					if(key.s == NULL) {
						LM_ERR("mqueue [%.*s] row [%d] has NULL key\n",
								name->len, name->s, i);
						goto error;
					}
					key.len = (RES_ROWS(db_res)[i].values[0].val.blob_val.len);
					break;
				case DB1_STRING:
					key.s = (char *)(RES_ROWS(db_res)[i]
											 .values[0]
											 .val.string_val);
					if(key.s == NULL) {
						LM_ERR("mqueue [%.*s] row [%d] has NULL key\n",
								name->len, name->s, i);
						goto error;
					}
					key.len = strlen(key.s);
					break;
				default:
					LM_ERR("key type must be string (type=%d)\n",
							RES_ROWS(db_res)[i].values[0].type);
					goto error;
			}
			switch(RES_ROWS(db_res)[i].values[1].type) {
				case DB1_STR:
					val.s = (RES_ROWS(db_res)[i].values[1].val.str_val.s);
					if(val.s == NULL) {
						LM_ERR("mqueue [%.*s] row [%d] has NULL value\n",
								name->len, name->s, i);
						goto error;
					}
					val.len = (RES_ROWS(db_res)[i].values[1].val.str_val.len);
					break;
				case DB1_BLOB:
					val.s = (RES_ROWS(db_res)[i].values[1].val.blob_val.s);
					if(val.s == NULL) {
						LM_ERR("mqueue [%.*s] row [%d] has NULL value\n",
								name->len, name->s, i);
						goto error;
					}
					val.len = (RES_ROWS(db_res)[i].values[1].val.blob_val.len);
					break;
				case DB1_STRING:
					val.s = (char *)(RES_ROWS(db_res)[i]
											 .values[1]
											 .val.string_val);
					if(val.s == NULL) {
						LM_ERR("mqueue [%.*s] row [%d] has NULL value\n",
								name->len, name->s, i);
						goto error;
					}
					val.len = strlen(val.s);
					break;
				default:
					LM_ERR("key type must be string (type=%d)\n",
							RES_ROWS(db_res)[i].values[1].type);
					goto error;
			}
			cnt++;
			LM_DBG("adding item[%d] key[%.*s] value[%.*s]\n", cnt, key.len,
					key.s, val.len, val.s);
			mq_item_add(name, &key, &val);
		}

		if(DB_CAPABILITY(mq_dbf, DB_CAP_FETCH)) {
			if(mq_dbf.fetch_result(mqueue_db_con, &db_res, mq_fetch_rows) < 0) {
				LM_ERR("Error while fetching!\n");
				goto error;
			}
		} else {
			break;
		}
	} while(RES_ROW_N(db_res) > 0);

	mq_dbf.free_result(mqueue_db_con, db_res);

	if(mq_dbf.delete(mqueue_db_con, 0, 0, 0, 0) < 0) {
		LM_ERR("failed to clear table\n");
		goto error;
	}

	LM_DBG("loaded %d values in queue\n", cnt);
	mqueue_db_close_con();
	return 0;
error:
	mqueue_db_close_con();
	return -1;
}

int mqueue_db_save_queue(str *name)
{
	int ncols = 2;
	db_key_t db_cols[2] = {&mq_db_key_column, &mq_db_val_column};
	db_val_t db_vals[2];
	int i;
	int mqueue_sz = 0;
	int ret = 0;

	if(mqueue_db_open_con() != 0) {
		LM_ERR("no db connection\n");
		return -1;
	}

	if(mq_dbf.use_table(mqueue_db_con, name) < 0) {
		LM_ERR("failed to use_table\n");
		goto error;
	}

	if(name->len <= 0 || name->s == NULL) {
		LM_ERR("bad mqueue name\n");
		goto error;
	}

	mqueue_sz = _mq_get_csize(name);

	if(mqueue_sz < 0) {
		LM_ERR("no such mqueue\n");
		goto error;
	}
	for(i = 0; i < mqueue_sz; i++) {
		ret = mq_head_fetch(name);
		if(ret != 0)
			break;
		str *key = NULL;
		str *val = NULL;
		key = get_mqk(name);
		val = get_mqv(name);
		LM_DBG("inserting mqueue[%.*s] name[%.*s] value[%.*s]\n", name->len,
				name->s, key->len, key->s, val->len, val->s);
		db_vals[0].type = DB1_STR;
		db_vals[0].nul = 0;
		db_vals[0].val.str_val.s = key->s;
		db_vals[0].val.str_val.len = key->len;
		db_vals[1].type = DB1_STR;
		db_vals[1].nul = 0;
		db_vals[1].val.str_val.s = val->s;
		db_vals[1].val.str_val.len = val->len;
		if(mq_dbf.insert(mqueue_db_con, db_cols, db_vals, ncols) < 0) {
			LM_ERR("failed to store key [%.*s] val [%.*s]\n", key->len, key->s,
					val->len, val->s);
		}
	}

	LM_INFO("queue [%.*s] saved in db\n", name->len, name->s);
	mqueue_db_close_con();
	return 0;
error:
	mqueue_db_close_con();
	return -1;
}
