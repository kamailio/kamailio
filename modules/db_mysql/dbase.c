/* 
 * $Id$ 
 *
 * MySQL module core functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "utils.h"
#include "val.h"
#include "my_con.h"
#include "my_pool.h"
#include "res.h"
#include "db_mod.h"
#include "dbase.h"


#define SQL_BUF_LEN 65536

static char sql_buf[SQL_BUF_LEN];


/*
 * Send an SQL query to the server
 */
static int submit_query(db_con_t* _h, const char* _s)
{	
	time_t t;
	int i, code;

	if ((!_h) || (!_s)) {
		LOG(L_ERR, "submit_query(): Invalid parameter value\n");
		return -1;
	}

	if (ping_interval) {
		t = time(0);
		if ((t - CON_TIMESTAMP(_h)) > ping_interval) {
			if (mysql_ping(CON_CONNECTION(_h))) {
				DBG("submit_query(): mysql_ping failed\n");
			}
		}
		CON_TIMESTAMP(_h) = t;
	}

	/* screws up the terminal when the query contains a BLOB :-( (by bogdan)
	 * DBG("submit_query(): %s\n", _s);
	 */

	/* When a server connection is lost and a query is attempted, most of
	 * the time the query will return a CR_SERVER_LOST, then at the second
	 * attempt to execute it, the mysql lib will reconnect and succeed.
	 * However is a few cases, the first attempt returns CR_SERVER_GONE_ERROR
	 * the second CR_SERVER_LOST and only the third succeeds.
	 * Thus the 3 in the loop count. Increasing the loop count over this
	 * value shouldn't be needed, but it doesn't hurt either, since the loop
	 * will most of the time stop at the second or sometimes at the third
	 * iteration.
     */
	for (i=0; i<(auto_reconnect ? 3 : 1); i++) {
		if (mysql_query(CON_CONNECTION(_h), _s)==0)
			return 0;
		code = mysql_errno(CON_CONNECTION(_h));
		if (code != CR_SERVER_GONE_ERROR && code != CR_SERVER_LOST) {
			break;
		}
	}
	LOG(L_ERR, "submit_query(): %s\n", mysql_error(CON_CONNECTION(_h)));
	return -2;
}


/*
 * Print list of columns separated by comma
 */
static int print_columns(char* _b, int _l, db_key_t* _c, int _n)
{
	int i;
	int res = 0;

	if ((!_c) || (!_n) || (!_b) || (!_l)) {
		LOG(L_ERR, "print_columns(): Invalid parameter value\n");
		return 0;
	}

	for(i = 0; i < _n; i++) {
		if (i == (_n - 1)) {
			res += snprintf(_b + res, _l - res, "%s ", _c[i]);
		} else {
			res += snprintf(_b + res, _l - res, "%s,", _c[i]);
		}
	}
	return res;
}


/*
 * Print list of values separated by comma
 */
static int print_values(MYSQL* _c, char* _b, int _l, db_val_t* _v, int _n)
{
	int i, res = 0, l;

	if (!_c || !_b || !_l || !_v || !_n) {
		LOG(L_ERR, "print_values(): Invalid parameter value\n");
		return 0;
	}

	for(i = 0; i < _n; i++) {
		l = _l - res;
		if (val2str(_c, _v + i, _b + res, &l) < 0) {
			LOG(L_ERR, "print_values(): Error while converting value to string\n");
			return 0;
		}
		res += l;
		if (i != (_n - 1)) {
			*(_b + res) = ',';
			res++;
		}
	}
	return res;
}


/*
 * Print where clause of SQL statement
 */
static int print_where(MYSQL* _c, char* _b, int _l, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n)
{
	int i;
	int res = 0;
	int l;

	if (!_c || !_b || !_l || !_k || !_v || !_n) {
		LOG(L_ERR, "print_where(): Invalid parameter value\n");
		return 0;
	}

	for(i = 0; i < _n; i++) {
		if (_o) {
			res += snprintf(_b + res, _l - res, "%s%s", _k[i], _o[i]);
		} else {
			res += snprintf(_b + res, _l - res, "%s=", _k[i]);
		}
		l = _l - res;
		val2str(_c, &(_v[i]), _b + res, &l);
		res += l;
		if (i != (_n - 1)) {
			res += snprintf(_b + res, _l - res, " AND ");
		}
	}
	return res;
}


/*
 * Print set clause of update SQL statement
 */
static int print_set(MYSQL* _c, char* _b, int _l, db_key_t* _k, db_val_t* _v, int _n)
{
	int i;
	int res = 0;
	int l;

	if (!_c || !_b || !_l || !_k || !_v || !_n) {
		LOG(L_ERR, "print_set(): Invalid parameter value\n");
		return 0;
	}

	for(i = 0; i < _n; i++) {
		res += snprintf(_b + res, _l - res, "%s=", _k[i]);
		l = _l - res;
		val2str(_c, &(_v[i]), _b + res, &l);
		res += l;
		if (i != (_n - 1)) {
			if ((_l - res) >= 1) {
				*(_b + res++) = ',';
			}
		}
	}
	return res;
}


/*
 * Initialize database module
 * No function should be called before this
 */
db_con_t* db_init(const char* _url)
{
	db_con_t* res;

	if (!_url) {
		LOG(L_ERR, "db_init(): Invalid parameter value\n");
		return 0;
	}

	res = pkg_malloc(sizeof(db_con_t) + sizeof(struct my_con*));
	if (!res) {
		LOG(L_ERR, "db_init(): No memory left\n");
		return 0;
	}
	memset(res, 0, sizeof(db_con_t) + sizeof(struct my_con*));

	(struct my_con*)res->tail = get_connection(_url);

	if (!res->tail) {
		LOG(L_ERR, "db_init(): Could not create a connection\n");
		pkg_free(res);
		return 0;
	}

	return res;
}


/*
 * Shut down database module
 * No function should be called after this
 */
void db_close(db_con_t* _h)
{
	if (!_h) {
		LOG(L_ERR, "db_close(): Invalid parameter value\n");
		return;
	}

	release_connection((struct my_con*)_h->tail);
	pkg_free(_h);
}


/*
 * Retrieve result set
 */
static int store_result(db_con_t* _h, db_res_t** _r)
{
	if ((!_h) || (!_r)) {
		LOG(L_ERR, "store_result(): Invalid parameter value\n");
		return -1;
	}

	*_r = new_result();
	if (*_r == 0) {
		LOG(L_ERR, "store_result(): No memory left\n");
		return -2;
	}

	CON_RESULT(_h) = mysql_store_result(CON_CONNECTION(_h));
	if (!CON_RESULT(_h)) {
		if (mysql_field_count(CON_CONNECTION(_h)) == 0) {
			(*_r)->col.n = 0;
			(*_r)->n = 0;
			return 0;
		} else {
			LOG(L_ERR, "store_result(): %s\n", mysql_error(CON_CONNECTION(_h)));
			free_result(*_r);
			*_r = 0;
			return -3;
		}
	}

        if (convert_result(_h, *_r) < 0) {
		LOG(L_ERR, "store_result(): Error while converting result\n");
		pkg_free(*_r);

		     /* This cannot be used because if convert_result fails,
		      * free_result will try to free rows and columns too 
		      * and free will be called two times
		      */
		     /* free_result(*_r); */
		return -4;
	}
	
	return 0;
}


/*
 * Release a result set from memory
 */
int db_free_result(db_con_t* _h, db_res_t* _r)
{
     if ((!_h) || (!_r)) {
	     LOG(L_ERR, "db_free_result(): Invalid parameter value\n");
	     return -1;
     }

     if (free_result(_r) < 0) {
	     LOG(L_ERR, "db_free_result(): Unable to free result structure\n");
	     return -1;
     }
     mysql_free_result(CON_RESULT(_h));
     CON_RESULT(_h) = 0;
     return 0;
}


/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_query(db_con_t* _h, db_key_t* _k, db_op_t* _op,
	     db_val_t* _v, db_key_t* _c, int _n, int _nc,
	     db_key_t _o, db_res_t** _r)
{
	int off;

	if (!_h) {
		LOG(L_ERR, "db_query(): Invalid parameter value\n");
		return -1;
	}

	if (!_c) {
		off = snprintf(sql_buf, SQL_BUF_LEN, "select * from %s ", CON_TABLE(_h));
	} else {
		off = snprintf(sql_buf, SQL_BUF_LEN, "select ");
		off += print_columns(sql_buf + off, SQL_BUF_LEN - off, _c, _nc);
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off, "from %s ", CON_TABLE(_h));
	}
	if (_n) {
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off, "where ");
		off += print_where(CON_CONNECTION(_h), sql_buf + off, SQL_BUF_LEN - off, _k, _op, _v, _n);
	}
	if (_o) {
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off, "order by %s", _o);
	}

	if (submit_query(_h, sql_buf) < 0) {
		LOG(L_ERR, "submit_query(): Error while submitting query\n");
		return -2;
	}

	return store_result(_h, _r);
}


/*
 * Execute a raw SQL query
 */
int db_raw_query(db_con_t* _h, char* _s, db_res_t** _r)
{
	if ((!_h) || (!_s)) {
		LOG(L_ERR, "db_raw_query(): Invalid parameter value\n");
		return -1;
	}

	if (submit_query(_h, _s) < 0) {
		LOG(L_ERR, "submit_query(): Error while submitting query\n");
		return -2;
	}

	if(_r)
	    return store_result(_h, _r);
	return 0;
}


/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	int off;

	if ((!_h) || (!_k) || (!_v) || (!_n)) {
		LOG(L_ERR, "db_insert(): Invalid parameter value\n");
		return -1;
	}

	off = snprintf(sql_buf, SQL_BUF_LEN, "insert into %s (", CON_TABLE(_h));
	off += print_columns(sql_buf + off, SQL_BUF_LEN - off, _k, _n);
	off += snprintf(sql_buf + off, SQL_BUF_LEN - off, ") values (");
	off += print_values(CON_CONNECTION(_h), sql_buf + off, SQL_BUF_LEN - off, _v, _n);
	*(sql_buf + off++) = ')';
	*(sql_buf + off) = '\0';

	if (submit_query(_h, sql_buf) < 0) {
	        LOG(L_ERR, "insert_row(): Error while submitting query\n");
		return -2;
	}
	return 0;
}


/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n)
{
	int off;

	if (!_h) {
		LOG(L_ERR, "db_delete(): Invalid parameter value\n");
		return -1;
	}

	off = snprintf(sql_buf, SQL_BUF_LEN, "delete from %s", CON_TABLE(_h));
	if (_n) {
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off, " where ");
		off += print_where(CON_CONNECTION(_h), sql_buf + off, SQL_BUF_LEN - off, _k, _o, _v, _n);
	}
	if (submit_query(_h, sql_buf) < 0) {
		LOG(L_ERR, "delete_row(): Error while submitting query\n");
		return -2;
	}
	return 0;
}


/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_update(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
	int off;

	if ((!_h) || (!_uk) || (!_uv) || (!_un)) {
		LOG(L_ERR, "db_update(): Invalid parameter value\n");
		return -1;
	}

	off = snprintf(sql_buf, SQL_BUF_LEN, "update %s set ", CON_TABLE(_h));
	off += print_set(CON_CONNECTION(_h), sql_buf + off, SQL_BUF_LEN - off, _uk, _uv, _un);
	if (_n) {
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off, " where ");
		off += print_where(CON_CONNECTION(_h), sql_buf + off, SQL_BUF_LEN - off, _k, _o, _v, _n);
		*(sql_buf + off) = '\0';
	}

	if (submit_query(_h, sql_buf) < 0) {
		LOG(L_ERR, "update_row(): Error while submitting query\n");
		return -2;
	}
	return 0;
}
