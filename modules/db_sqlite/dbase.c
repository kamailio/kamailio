/*
 * $Id$
 *
 * SQlite module core functions
 *
 * Copyright (C) 2010 Timo Teräs
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

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_query.h"
#include "dbase.h"

static time_t sqlite_to_timet(double rT)
{
	return 86400.0*(rT - 2440587.5) + 0.5;
}

static double timet_to_sqlite(time_t t)
{
	return ((((double) t) - 0.5) / 86400.0) + 2440587.5;
}

/*
 * Initialize database module
 * No function should be called before this
 */

static struct sqlite_connection * db_sqlite_new_connection(const struct db_id* id)
{
	struct sqlite_connection *con;
	int rc;

	con = pkg_malloc(sizeof(*con));
	if (!con) {
		LM_ERR("failed to allocate driver connection\n");
		return NULL;
	}

	memset(con, 0, sizeof(*con));
	con->hdr.ref = 1;
	con->hdr.id = (struct db_id*) id; /* set here - freed on error */

	rc = sqlite3_open_v2(id->database, &con->conn,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rc != SQLITE_OK) {
		pkg_free(con);
		LM_ERR("failed to open sqlite database '%s'\n", id->database);
		return NULL;
	}

	return con;
}

db1_con_t* db_sqlite_init(const str* _url)
{
	return db_do_init(_url, (void *) db_sqlite_new_connection);
}


/*
 * Shut down database module
 * No function should be called after this
 */

static void db_sqlite_free_connection(struct sqlite_connection* con)
{
	if (!con) return;

	sqlite3_close(con->conn);
	free_db_id(con->hdr.id);
	pkg_free(con);
}

void db_sqlite_close(db1_con_t* _h)
{
	db_do_close(_h, db_sqlite_free_connection);
}

/*
 * Release a result set from memory
 */
int db_sqlite_free_result(db1_con_t* _h, db1_res_t* _r)
{
	if (!_h || !_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_free_result(_r) < 0)
	{
		LM_ERR("failed to free result structure\n");
		return -1;
	}
	return 0;
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_sqlite_use_table(db1_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}

/*
 * Reset query context
 */
static void db_sqlite_cleanup_query(const db1_con_t* _c)
{
	struct sqlite_connection *conn = CON_SQLITE(_c);
	int rc;

	if (conn->stmt != NULL) {
		rc = sqlite3_finalize(conn->stmt);
		if (rc != SQLITE_OK)
			LM_ERR("finalize failed: %s\n",
				sqlite3_errmsg(conn->conn));
	}

	conn->stmt = NULL;
	conn->bindpos = 0;
}

/*
 * Convert value to sql-string as db bind index
 */
static int db_sqlite_val2str(const db1_con_t* _c, const db_val_t* _v, char* _s, int* _len)
{
	struct sqlite_connection *conn;
	int ret;

	if (!_c || !_v || !_s || !_len || *_len <= 0) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	conn = CON_SQLITE(_c);
	if (conn->bindpos >= DB_SQLITE_MAX_BINDS) {
		LM_ERR("too many bindings, recompile with larger DB_SQLITE_MAX_BINDS\n");
		return -2;
	}

	conn->bindarg[conn->bindpos] = _v;
	ret = snprintf(_s, *_len, "?%u", ++conn->bindpos);
	if ((unsigned)ret >= (unsigned) *_len)
		return -11;

	*_len = ret;
	return 0;
}

/*
 * Send an SQL query to the server
 */
static int db_sqlite_submit_query(const db1_con_t* _h, const str* _s)
{
	struct sqlite_connection *conn = CON_SQLITE(_h);
	sqlite3_stmt *stmt;
	const db_val_t *val;
	int rc, i;

	LM_DBG("submit_query: %.*s\n", _s->len, _s->s);

	rc = sqlite3_prepare_v2(conn->conn, _s->s, _s->len, &stmt, NULL);
	if (rc != SQLITE_OK) {
		LM_ERR("failed to prepare statement: %s\n",
			sqlite3_errmsg(conn->conn));
		return -1;
	}
	conn->stmt = stmt;

	for (i = 1; i <= conn->bindpos; i++) {
		val = conn->bindarg[i-1];
		if (VAL_NULL(val)) {
			rc = sqlite3_bind_null(stmt, i);
		} else switch (VAL_TYPE(val)) {
		case DB1_INT:
			rc = sqlite3_bind_int(stmt, i, VAL_INT(val));
			break;
		case DB1_BIGINT:
			rc = sqlite3_bind_int64(stmt, i, VAL_BIGINT(val));
			break;
		case DB1_DOUBLE:
			rc = sqlite3_bind_double(stmt, i, VAL_DOUBLE(val));
			break;
		case DB1_STRING:
			rc = sqlite3_bind_text(stmt, i,
				VAL_STRING(val), -1, NULL);
			break;
		case DB1_STR:
			rc = sqlite3_bind_text(stmt, i,
				VAL_STR(val).s, VAL_STR(val).len, NULL);
			break;
		case DB1_DATETIME:
			rc = sqlite3_bind_double(stmt, i, timet_to_sqlite(VAL_TIME(val)));
			break;
		case DB1_BLOB:
			rc = sqlite3_bind_blob(stmt, i,
				VAL_BLOB(val).s, VAL_BLOB(val).len,
				NULL);
			break;
		case DB1_BITMAP:
			rc = sqlite3_bind_int(stmt, i, VAL_BITMAP(val));
			break;
		default:
			LM_ERR("unknown bind value type %d\n", VAL_TYPE(val));
			return -1;
		}
		if (rc != SQLITE_OK) {
			LM_ERR("Parameter bind failed: %s\n",
				sqlite3_errmsg(conn->conn));
			return -1;
		}
	}

	return 0;
}

#define H3(a,b,c)	((a<<16) + (b<<8) + c)
#define H4(a,b,c,d)	((a<<24) + (b<<16) + (c<<8) + d)

static int decltype_to_dbtype(const char *decltype)
{
	/* SQlite3 has dynamic typing. It does not store the actual
	 * exact type, instead it uses 'affinity' depending on the
	 * value. We have to go through the declaration type to see
	 * what to return.
	 * The loose string matching (4 letter substring match) is what
	 * SQlite does internally, but our return values differ as we want
	 * the more exact srdb type instead of the affinity. */

	uint32_t h = 0;

	for (; *decltype; decltype++) {
		h <<= 8;
		h += toupper(*decltype);

		switch (h & 0x00ffffff) {
		case H3('I','N','T'):
			return DB1_INT;
		}

		switch (h) {
		case H4('S','E','R','I'): /* SERIAL */
			return DB1_INT;
		case H4('B','I','G','I'): /* BIGINT */
			return DB1_BIGINT;
		case H4('C','H','A','R'):
		case H4('C','L','O','B'):
			return DB1_STRING;
		case H4('T','E','X','T'):
			return DB1_STR;
		case H4('R','E','A','L'):
		case H4('F','L','O','A'): /* FLOAT */
		case H4('D','O','U','B'): /* DOUBLE */
			return DB1_DOUBLE;
		case H4('B','L','O','B'):
			return DB1_BLOB;
		case H4('T','I','M','E'):
		case H4('D','A','T','E'):
			return DB1_DATETIME;
		}
	}

	LM_ERR("sqlite decltype '%s' not recognized, defaulting to int",
		decltype);
	return DB1_INT;
}

static int type_to_dbtype(int type)
{
	switch (type) {
	case SQLITE_INTEGER:
		return DB1_INT;
	case SQLITE_FLOAT:
		return DB1_DOUBLE;
	case SQLITE_TEXT:
		return DB1_STR;
	case SQLITE_BLOB:
		return DB1_BLOB;
	default:
		/* Unknown, or NULL column value. Assume this is a
		 * string. */
		return DB1_STR;
	}
}

static str* str_dup(const char *_s)
{
	str *s;
	int len = strlen(_s);

	s = (str*) pkg_malloc(sizeof(str)+len+1);
	if (!s)
		return NULL;

	s->len = len;
	s->s = ((char*)s) + sizeof(str);
	memcpy(s->s, _s, len);
	s->s[len] = '\0';

	return s;
}

static void str_assign(str* s, const char *_s, int len)
{
	s->s = (char *) pkg_malloc(len + 1);
	if (s->s) {
		s->len = len;
		memcpy(s->s, _s, len);
		s->s[len] = 0;
	}
}

/*
 * Read database answer and fill the structure
 */
int db_sqlite_store_result(const db1_con_t* _h, db1_res_t** _r)
{
	struct sqlite_connection *conn = CON_SQLITE(_h);
	db1_res_t *res;
	int i, rc, num_rows = 0, num_alloc = 0;
	db_row_t *rows = NULL, *row;
	db_val_t *val;

	res = db_new_result();
	if (res == NULL)
		goto no_mem;

	while (1) {
		rc = sqlite3_step(conn->stmt);
		if (rc == SQLITE_DONE) {
			*_r = res;
			return 0;
		}
		if (rc != SQLITE_ROW) {
			LM_INFO("sqlite3_step failed: %s\n", sqlite3_errmsg(conn->conn));
			goto err;
		}
		if (num_rows == 0) {
			/* get column types */
			rc = sqlite3_column_count(conn->stmt);
			if (db_allocate_columns(res, rc) != 0)
				goto err;
			RES_COL_N(res) = rc;

			for (i = 0; i < RES_COL_N(res); i++) {
				const char *decltype;
				int dbtype;

				RES_NAMES(res)[i] = str_dup(sqlite3_column_name(conn->stmt, i));
				if (RES_NAMES(res)[i] == NULL)
					goto no_mem;
				decltype = sqlite3_column_decltype(conn->stmt, i);
				if (decltype != NULL)
					dbtype = decltype_to_dbtype(decltype);
				else
					dbtype = type_to_dbtype(sqlite3_column_type(conn->stmt, i));
				RES_TYPES(res)[i] = dbtype;
			}
		}
		if (num_rows >= num_alloc) {
			if (num_alloc)
				num_alloc *= 2;
			else
				num_alloc = 8;
			rows = pkg_realloc(rows, sizeof(db_row_t) * num_alloc);
			if (rows == NULL)
				goto no_mem;
			RES_ROWS(res) = rows;
		}

		row = &RES_ROWS(res)[num_rows];
		num_rows++;
		RES_ROW_N(res) = num_rows;		/* rows in this result set */
		RES_NUM_ROWS(res) = num_rows;		/* rows in total */

		if (db_allocate_row(res, row) != 0)
			goto no_mem;

		for (i = 0, val = ROW_VALUES(row); i < RES_COL_N(res); i++, val++) {
			VAL_TYPE(val) = RES_TYPES(res)[i];
			VAL_NULL(val) = 0;
			VAL_FREE(val) = 0;
			if (sqlite3_column_type(conn->stmt, i) == SQLITE_NULL) {
				VAL_NULL(val) = 1;
			} else switch (VAL_TYPE(val)) {
			case DB1_INT:
				VAL_INT(val) = sqlite3_column_int(conn->stmt, i);
				break;
			case DB1_BIGINT:
				VAL_BIGINT(val) = sqlite3_column_int64(conn->stmt, i);
				break;
			case DB1_STRING:
				/* first field of struct str* is the char* so we can just
				 * do whatever DB1_STR case does */
			case DB1_STR:
				str_assign(&VAL_STR(val),
					(const char*) sqlite3_column_text(conn->stmt, i),
					sqlite3_column_bytes(conn->stmt, i));
				if (!VAL_STR(val).s)
					goto no_mem;
				VAL_FREE(val) = 1;
				break;
			case DB1_DOUBLE:
				VAL_DOUBLE(val) = sqlite3_column_double(conn->stmt, i);
				break;
			case DB1_DATETIME:
				VAL_TIME(val) = sqlite_to_timet(sqlite3_column_double(conn->stmt, i));
				break;
			case DB1_BLOB:
				str_assign(&VAL_BLOB(val),
					(const char*) sqlite3_column_blob(conn->stmt, i),
					sqlite3_column_bytes(conn->stmt, i));
				if (!VAL_STR(val).s)
					goto no_mem;
				VAL_FREE(val) = 1;
				break;
			default:
				LM_ERR("unhandled db-type\n");
				goto err;
			}
		}
	}

no_mem:
	LM_ERR("no private memory left\n");
err:
	if (res)
		db_free_result(res);
	return -1;
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
int db_sqlite_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, int _n, int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	int rc;

	rc = db_do_query(_h, _k, _op, _v, _c, _n, _nc, _o, _r,
			 db_sqlite_val2str,
			 db_sqlite_submit_query,
			 db_sqlite_store_result);
	db_sqlite_cleanup_query(_h);

	return rc;
}

static int db_sqlite_commit(const db1_con_t* _h)
{
	struct sqlite_connection *conn = CON_SQLITE(_h);
	int rc;

	rc = sqlite3_step(conn->stmt);
	if (rc != SQLITE_DONE && rc != SQLITE_OK) {
		LM_ERR("sqlite commit failed: %s\n",
		       sqlite3_errmsg(conn->conn));
		return -1;
	}

	return 0;
}

/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_sqlite_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		int _n)
{
	int rc = -1;

	rc = db_do_insert(_h, _k, _v, _n,
			  db_sqlite_val2str,
			  db_sqlite_submit_query);
	if (rc == 0)
		rc = db_sqlite_commit(_h);
	db_sqlite_cleanup_query(_h);

	return rc;
}


/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_sqlite_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, int _n)
{
	int rc;

	rc = db_do_delete(_h, _k, _o, _v, _n,
			  db_sqlite_val2str,
			  db_sqlite_submit_query);
	if (rc == 0)
		rc = db_sqlite_commit(_h);
	db_sqlite_cleanup_query(_h);

	return rc;
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
int db_sqlite_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv,
		int _n, int _un)
{
	int rc;

	rc = db_do_update(_h, _k, _o, _v, _uk, _uv, _n, _un,
			  db_sqlite_val2str,
			  db_sqlite_submit_query);
	if (rc == 0)
		rc = db_sqlite_commit(_h);
	db_sqlite_cleanup_query(_h);

	return rc;
}

int db_sqlite_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	int rc;

	rc = db_do_raw_query(_h, _s, _r,
			       db_sqlite_submit_query,
			       db_sqlite_store_result);
	db_sqlite_cleanup_query(_h);

	return rc;
}
