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
 *
 */

/**
 * \file lib/srdb1/db_query.c
 * \brief Query helper for database drivers
 * \ingroup db1
 *
 * This helper methods for database queries are used from the database
 * SQL driver to do the actual work. Each function uses some functions from
 * the actual driver with function pointers to the concrete, specific
 * implementation.
*/

#include <stdio.h>
#include <stdlib.h>
#include "../../dprint.h"
#include "db_ut.h"
#include "db_query.h"
#include "../../globals.h"
#include "../../timer.h"

static str  sql_str;
static char *sql_buf = NULL;

static inline int db_do_submit_query(const db1_con_t* _h, const str *_query,
		int (*submit_query)(const db1_con_t*, const str*))
{
	int ret;
	unsigned int ms = 0;

	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0))
		ms = TICKS_TO_MS(get_ticks_raw());

	ret = submit_query(_h, _query);

	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)) {
		ms = TICKS_TO_MS(get_ticks_raw()) - ms;
		if(ms >= cfg_get(core, core_cfg, latency_limit_action)) {
				LOG(cfg_get(core, core_cfg, latency_log),
					"alert - query execution too long [%u ms] for [%.*s]\n",
				   ms, _query->len<50?_query->len:50, _query->s);
		}
	}

	return ret;
}

static int db_do_query_internal(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	const db_key_t _o, db1_res_t** _r, int (*val2str) (const db1_con_t*,
	const db_val_t*, char*, int* _len), int (*submit_query)(const db1_con_t*,
	const str*), int (*store_result)(const db1_con_t* _h, db1_res_t** _r), int _l)
{
	int off, ret;

	if (!_h || !val2str || !submit_query || !store_result) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (!_c) {
		ret = snprintf(sql_buf, sql_buffer_size, "select * from %s%.*s%s ",
				CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
		if (ret < 0 || ret >= sql_buffer_size) goto error;
		off = ret;
	} else {
		ret = snprintf(sql_buf, sql_buffer_size, "select ");
		if (ret < 0 || ret >= sql_buffer_size) goto error;
		off = ret;

		ret = db_print_columns(sql_buf + off, sql_buffer_size - off, _c, _nc, CON_TQUOTESZ(_h));
		if (ret < 0) return -1;
		off += ret;

		ret = snprintf(sql_buf + off, sql_buffer_size - off, "from %s%.*s%s ",
				CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
		if (ret < 0 || ret >= (sql_buffer_size - off)) goto error;
		off += ret;
	}
	if (_n) {
		ret = snprintf(sql_buf + off, sql_buffer_size - off, "where ");
		if (ret < 0 || ret >= (sql_buffer_size - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off,
				sql_buffer_size - off, _k, _op, _v, _n, val2str);
		if (ret < 0) return -1;;
		off += ret;
	}
	if (_o) {
		ret = snprintf(sql_buf + off, sql_buffer_size - off, " order by %.*s", _o->len, _o->s);
		if (ret < 0 || ret >= (sql_buffer_size - off)) goto error;
		off += ret;
	}
	if (_l) {
		ret = snprintf(sql_buf + off, sql_buffer_size - off, " for update");
		if (ret < 0 || ret >= (sql_buffer_size - off)) goto error;
		off += ret;
	}
	/*
	 * Null-terminate the string for the postgres driver. Its query function
	 * don't support a length parameter, so they need this for the correct
	 * function of strlen. This zero is not included in the 'str' length.
	 * We need to check the length here, otherwise we could overwrite the buffer
	 * boundaries if off is equal to sql_buffer_size.
	 */
	if (off + 1 >= sql_buffer_size) goto error;
	sql_buf[off + 1] = '\0';
	sql_str.s = sql_buf;
	sql_str.len = off;

	if (db_do_submit_query(_h, &sql_str, submit_query) < 0) {
		LM_ERR("error while submitting query\n");
		return -2;
	}

	if(_r) {
		int tmp = store_result(_h, _r);
		if (tmp < 0) {
			LM_ERR("error while storing result");
			return tmp;
		}
	}
	return 0;

error:
	LM_ERR("error while preparing query\n");
	return -1;
}

int db_do_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	const db_key_t _o, db1_res_t** _r, int (*val2str) (const db1_con_t*,
	const db_val_t*, char*, int* _len), int (*submit_query)(const db1_con_t*,
	const str*), int (*store_result)(const db1_con_t* _h, db1_res_t** _r))
{
	return db_do_query_internal(_h, _k, _op, _v, _c, _n, _nc, _o, _r, val2str,
					submit_query, store_result, 0);
}

int db_do_query_lock(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	const db_key_t _o, db1_res_t** _r, int (*val2str) (const db1_con_t*,
	const db_val_t*, char*, int* _len), int (*submit_query)(const db1_con_t*,
	const str*), int (*store_result)(const db1_con_t* _h, db1_res_t** _r))
{
	return db_do_query_internal(_h, _k, _op, _v, _c, _n, _nc, _o, _r, val2str,
					submit_query, store_result, 1);
}


int db_do_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r,
	int (*submit_query)(const db1_con_t* _h, const str* _c),
	int (*store_result)(const db1_con_t* _h, db1_res_t** _r))
{
	if (!_h || !_s || !submit_query || !store_result) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_do_submit_query(_h, _s, submit_query) < 0) {
		LM_ERR("error while submitting query\n");
		return -2;
	}

	if(_r) {
		int tmp = store_result(_h, _r);
		if (tmp < 0) {
			LM_ERR("error while storing result");
			return tmp;
		}
	}
	return 0;
}


int db_do_insert_cmd(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db1_con_t*, const db_val_t*, char*, int*),
	int (*submit_query)(const db1_con_t* _h, const str* _c), int mode)
{
	int off, ret;

	if (!_h || !_k || !_v || !_n || !val2str || !submit_query) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if(mode==1)
		ret = snprintf(sql_buf, sql_buffer_size, "insert delayed into %s%.*s%s (",
				CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	else
		ret = snprintf(sql_buf, sql_buffer_size, "insert into %s%.*s%s (",
				CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	if (ret < 0 || ret >= sql_buffer_size) goto error;
	off = ret;

	ret = db_print_columns(sql_buf + off, sql_buffer_size - off, _k, _n, CON_TQUOTESZ(_h));
	if (ret < 0) return -1;
	off += ret;

	ret = snprintf(sql_buf + off, sql_buffer_size - off, ") values (");
	if (ret < 0 || ret >= (sql_buffer_size - off)) goto error;
	off += ret;

	ret = db_print_values(_h, sql_buf + off, sql_buffer_size - off, _v, _n, val2str);
	if (ret < 0) return -1;
	off += ret;

	if (off + 2 > sql_buffer_size) goto error;
	sql_buf[off++] = ')';
	sql_buf[off] = '\0';
	sql_str.s = sql_buf;
	sql_str.len = off;

	if (db_do_submit_query(_h, &sql_str, submit_query) < 0) {
	        LM_ERR("error while submitting query\n");
		return -2;
	}
	return 0;

error:
	LM_ERR("error while preparing insert operation\n");
	return -1;
}

int db_do_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db1_con_t*, const db_val_t*, char*, int*),
	int (*submit_query)(const db1_con_t* _h, const str* _c))
{
	return db_do_insert_cmd(_h, _k, _v, _n, val2str, submit_query, 0);
}

int db_do_insert_delayed(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db1_con_t*, const db_val_t*, char*, int*),
	int (*submit_query)(const db1_con_t* _h, const str* _c))
{
	return db_do_insert_cmd(_h, _k, _v, _n, val2str, submit_query, 1);
}

int db_do_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
	const db_val_t* _v, const int _n, int (*val2str) (const db1_con_t*,
	const db_val_t*, char*, int*), int (*submit_query)(const db1_con_t* _h,
	const str* _c))
{
	int off, ret;

	if (!_h || !val2str || !submit_query) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, sql_buffer_size, "delete from %s%.*s%s",
			CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	if (ret < 0 || ret >= sql_buffer_size) goto error;
	off = ret;

	if (_n) {
		ret = snprintf(sql_buf + off, sql_buffer_size - off, " where ");
		if (ret < 0 || ret >= (sql_buffer_size - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off,
				sql_buffer_size - off, _k, _o, _v, _n, val2str);
		if (ret < 0) return -1;
		off += ret;
	}
	if (off + 1 > sql_buffer_size) goto error;
	sql_buf[off] = '\0';
	sql_str.s = sql_buf;
	sql_str.len = off;

	if (db_do_submit_query(_h, &sql_str, submit_query) < 0) {
		LM_ERR("error while submitting query\n");
		return -2;
	}
	return 0;

error:
	LM_ERR("error while preparing delete operation\n");
	return -1;
}


int db_do_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
	const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n,
	const int _un, int (*val2str) (const db1_con_t*, const db_val_t*, char*, int*),
	int (*submit_query)(const db1_con_t* _h, const str* _c))
{
	int off, ret;

	if (!_h || !_uk || !_uv || !_un || !val2str || !submit_query) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, sql_buffer_size, "update %s%.*s%s set ",
			CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	if (ret < 0 || ret >= sql_buffer_size) goto error;
	off = ret;

	ret = db_print_set(_h, sql_buf + off, sql_buffer_size - off, _uk, _uv, _un, val2str);
	if (ret < 0) return -1;
	off += ret;

	if (_n) {
		ret = snprintf(sql_buf + off, sql_buffer_size - off, " where ");
		if (ret < 0 || ret >= (sql_buffer_size - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off, sql_buffer_size - off, _k, _o, _v, _n, val2str);
		if (ret < 0) return -1;
		off += ret;
	}
	if (off + 1 > sql_buffer_size) goto error;
	sql_buf[off] = '\0';
	sql_str.s = sql_buf;
	sql_str.len = off;

	if (db_do_submit_query(_h, &sql_str, submit_query) < 0) {
		LM_ERR("error while submitting query\n");
		return -2;
	}
	return 0;

error:
	LM_ERR("error while preparing update operation\n");
	return -1;
}


int db_do_replace(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db1_con_t*, const db_val_t*, char*,
	int*), int (*submit_query)(const db1_con_t* _h, const str* _c))
{
	int off, ret;

	if (!_h || !_k || !_v || !val2str|| !submit_query) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, sql_buffer_size, "replace %s%.*s%s (",
			CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	if (ret < 0 || ret >= sql_buffer_size) goto error;
	off = ret;

	ret = db_print_columns(sql_buf + off, sql_buffer_size - off, _k, _n, CON_TQUOTESZ(_h));
	if (ret < 0) return -1;
	off += ret;

	ret = snprintf(sql_buf + off, sql_buffer_size - off, ") values (");
	if (ret < 0 || ret >= (sql_buffer_size - off)) goto error;
	off += ret;

	ret = db_print_values(_h, sql_buf + off, sql_buffer_size - off, _v, _n,
	val2str);
	if (ret < 0) return -1;
	off += ret;

	if (off + 2 > sql_buffer_size) goto error;
	sql_buf[off++] = ')';
	sql_buf[off] = '\0';
	sql_str.s = sql_buf;
	sql_str.len = off;

	if (db_do_submit_query(_h, &sql_str, submit_query) < 0) {
	        LM_ERR("error while submitting query\n");
		return -2;
	}
	return 0;

 error:
	LM_ERR("error while preparing replace operation\n");
	return -1;
}

int db_query_init(void)
{
    if (sql_buf != NULL)
    {
        LM_DBG("sql_buf not NULL on init\n");
        return 0;
    }
    LM_DBG("About to allocate sql_buf size = %d\n", sql_buffer_size);
    sql_buf = (char*)malloc(sql_buffer_size);
    if (sql_buf == NULL)
    {
        LM_ERR("failed to allocate sql_buf\n");
        return -1;
    }
    return 0;
}

static int db_fetch_query_internal(db_func_t *dbf, int frows,
		db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r, db_query_f _query)
{

	int ret;

	if (!_query) {
		LM_ERR("bad query function pointer\n");
		goto error;
	}

	ret = 0;
	*_r = NULL;

	if (DB_CAPABILITY(*dbf, DB_CAP_FETCH)) {
		if(_query(_h, _k, _op, _v, _c, _n, _nc, _o, 0) < 0)
		{
			LM_ERR("unable to query db for fetch\n");
			goto error;
		}
		if(dbf->fetch_result(_h, _r, frows)<0)
		{
			LM_ERR("unable to fetch the db result\n");
			goto error;
		}
		ret = 1;
	} else {
		if(_query(_h, _k, _op, _v, _c, _n, _nc, _o, _r) < 0)
		{
			LM_ERR("unable to do full db querry\n");
			goto error;
		}
	}

	return ret;

error:
	if(*_r)
	{
		dbf->free_result(_h, *_r);
		*_r = NULL;
	}
	return -1;
}

/**
 * wrapper around db query to handle fetch capability
 * return: -1 error; 0 ok with no fetch capability; 1 ok with fetch capability
 */
int db_fetch_query(db_func_t *dbf, int frows,
		db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	return db_fetch_query_internal(dbf, frows, _h, _k, _op, _v, _c, _n,
					_nc, _o, _r, dbf->query);
}

/**
 * wrapper around db query_lock to handle fetch capability
 * return: -1 error; 0 ok with no fetch capability; 1 ok with fetch capability
 */
int db_fetch_query_lock(db_func_t *dbf, int frows,
		db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	if (!dbf->query_lock)
	{
		LM_ERR("query_lock not supported by this database module\n");
		return -1;
	}

	return db_fetch_query_internal(dbf, frows, _h, _k, _op, _v, _c, _n,
					_nc, _o, _r, dbf->query_lock);
}

/**
 * wrapper around db fetch to handle fetch capability
 * return: -1 error; 0 ok with no fetch capability; 1 ok with fetch capability
 */
int db_fetch_next(db_func_t *dbf, int frows, db1_con_t* _h,
		db1_res_t** _r)
{
	int ret;

	ret = 0;

	if (DB_CAPABILITY(*dbf, DB_CAP_FETCH)) {
		if(dbf->fetch_result(_h, _r, frows)<0) {
			LM_ERR("unable to fetch next rows\n");
			goto error;
		}
		ret = 1;
	}
	return ret;

error:
	if(*_r)
	{
		dbf->free_result(_h, *_r);
		*_r = NULL;
	}
	return -1;
}
