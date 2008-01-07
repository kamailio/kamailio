/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/**
 * \file db/db_query.c
 * \brief Query helper for database drivers
 *
 * This helper methods for database queries are used from the database
 * SQL driver to do the actual work. Each function uses some functions from
 * the actual driver with function pointers to the concrete, specific
 * implementation.
*/

#include <stdio.h>
#include "../dprint.h"
#include "db_ut.h"
#include "db_query.h"

static char sql_buf[SQL_BUF_LEN];

int db_do_query( const db_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	const db_key_t _o, db_res_t** _r, int (*val2str) (const db_con_t*,
	const db_val_t*, char*, int* _len), int (*submit_query)(const db_con_t*,
	const char*), int (*store_result)(const db_con_t*, db_res_t**))
{
	int off, ret;

	if (!_h || !val2str || !submit_query || !store_result) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (!_c) {
		ret = snprintf(sql_buf, SQL_BUF_LEN, "select * from %s ", CON_TABLE(_h));
		if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
		off = ret;
	} else {
		ret = snprintf(sql_buf, SQL_BUF_LEN, "select ");
		if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
		off = ret;

		ret = db_print_columns(sql_buf + off, SQL_BUF_LEN - off, _c, _nc);
		if (ret < 0) return -1;
		off += ret;

		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, "from %s ", CON_TABLE(_h));
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;
	}
	if (_n) {
		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, "where ");
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off,
				SQL_BUF_LEN - off, _k, _op, _v, _n, val2str);
		if (ret < 0) return -1;;
		off += ret;
	}
	if (_o) {
		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, " order by %s", _o);
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;
	}
	
	*(sql_buf + off) = '\0';
	if (submit_query(_h, sql_buf) < 0) {
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


int db_do_raw_query(const db_con_t* _h, const char* _s, db_res_t** _r, 
	int (*submit_query)(const db_con_t* _h, const char* _c),
	int (*store_result)(const db_con_t* _h, db_res_t** _r))
{
	if (!_h || !_s || !submit_query || !store_result) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (submit_query(_h, _s) < 0) {
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


int db_do_insert(const db_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db_con_t*, const db_val_t*, char*, int*), 
	int (*submit_query)(const db_con_t* _h, const char* _c))
{
	int off, ret;

	if (!_h || !_k || !_v || !_n || !val2str || !submit_query) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, SQL_BUF_LEN, "insert into %s (", CON_TABLE(_h));
	if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
	off = ret;

	ret = db_print_columns(sql_buf + off, SQL_BUF_LEN - off, _k, _n);
	if (ret < 0) return -1;
	off += ret;

	ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, ") values (");
	if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
	off += ret;

	ret = db_print_values(_h, sql_buf + off, SQL_BUF_LEN - off, _v, _n, val2str);
	if (ret < 0) return -1;
	off += ret;

	*(sql_buf + off++) = ')';
	*(sql_buf + off) = '\0';

	if (submit_query(_h, sql_buf) < 0) {
	        LM_ERR("error while submitting query\n");
		return -2;
	}
	return 0;

error:
	LM_ERR("error while preparing insert operation\n");
	return -1;
}


int db_do_delete(const db_con_t* _h, const db_key_t* _k, const db_op_t* _o,
	const db_val_t* _v, const int _n, int (*val2str) (const db_con_t*,
	const db_val_t*, char*, int*), int (*submit_query)(const db_con_t* _h,
	const char* _c))
{
	int off, ret;

	if (!_h || !val2str || !submit_query) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, SQL_BUF_LEN, "delete from %s", CON_TABLE(_h));
	if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
	off = ret;

	if (_n) {
		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, " where ");
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off,
				SQL_BUF_LEN - off, _k, _o, _v, _n, val2str);
		if (ret < 0) return -1;
		off += ret;
	}

	*(sql_buf + off) = '\0';
	if (submit_query(_h, sql_buf) < 0) {
		LM_ERR("error while submitting query\n");
		return -2;
	}
	return 0;

error:
	LM_ERR("error while preparing delete operation\n");
	return -1;
}


int db_do_update(const db_con_t* _h, const db_key_t* _k, const db_op_t* _o,
	const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n,
	const int _un, int (*val2str) (const db_con_t*, const db_val_t*, char*, int*),
	int (*submit_query)(const db_con_t* _h, const char* _c))
{
	int off, ret;

	if (!_h || !_uk || !_uv || !_un || !val2str || !submit_query) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, SQL_BUF_LEN, "update %s set ", CON_TABLE(_h));
	if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
	off = ret;

	ret = db_print_set(_h, sql_buf + off, SQL_BUF_LEN - off, _uk, _uv, _un, val2str);
	if (ret < 0) return -1;
	off += ret;

	if (_n) {
		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, " where ");
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off, SQL_BUF_LEN - off, _k, _o, _v, _n, val2str);
		if (ret < 0) return -1;
		off += ret;

		*(sql_buf + off) = '\0';
	}

	if (submit_query(_h, sql_buf) < 0) {
		LM_ERR("error while submitting query\n");
		return -2;
	}
	return 0;

error:
	LM_ERR("error while preparing update operation\n");
	return -1;
}


int db_do_replace(const db_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db_con_t*, const db_val_t*, char*,
	int*), int (*submit_query)(const db_con_t* _h, const char* _c))
{
	int off, ret;

	if (!_h || !_k || !_v || !val2str|| !submit_query) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, SQL_BUF_LEN, "replace %s (", CON_TABLE(_h));
	if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
	off = ret;

	ret = db_print_columns(sql_buf + off, SQL_BUF_LEN - off, _k, _n);
	if (ret < 0) return -1;
	off += ret;

	ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, ") values (");
	if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
	off += ret;

	ret = db_print_values(_h, sql_buf + off, SQL_BUF_LEN - off, _v, _n,
	val2str);
	if (ret < 0) return -1;
	off += ret;

	*(sql_buf + off++) = ')';
	*(sql_buf + off) = '\0';

	if (submit_query(_h, sql_buf) < 0) {
	        LM_ERR("error while submitting query\n");
		return -2;
	}
	return 0;

 error:
	LM_ERR("error while preparing replace operation\n");
	return -1;
}
