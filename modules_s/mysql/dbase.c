/* 
 * $Id$ 
 */

#include <mysql/mysql.h>
#include "db_utils.h"
#include "../../dprint.h"
#include "defs.h"
#include "../../mem/mem.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "dbase.h"



/*
 * Declarations
 */
static int connect_db    (db_con_t* _h, const char* _db_url);
static int disconnect_db (db_con_t* _h);
static int submit_query  (db_con_t* _h, const char* _s);
static int get_result    (db_con_t* _h, db_res_t** _r);
static int print_columns (char* _b, int _l, db_key_t* _c, int _n);
static int print_values  (char* _b, int _l, db_val_t* _v, int _n);
static int print_where   (char* _b, int _l, db_key_t* _k, db_val_t* _v, int _n);
static int print_set     (char* _b, int _l, db_key_t* _k, db_val_t* _v, int _n);


static char ins_buf   [SQL_BUF_LEN];
static char del_buf   [SQL_BUF_LEN];
static char query_buf [SQL_BUF_LEN];
static char update_buf[SQL_BUF_LEN];


/*
 * Establish database connection,
 * returns 1 on success, 0 otherwise
 * _h is a handle used in communication with database
 *
 * URL is in form sql://user:password@host:port/database
 */
static int connect_db(db_con_t* _h, const char* _db_url)
{
	int p, l, res;
	char* user, *password, *host, *port, *database;
	char* buf;

#ifdef PARANOID
	if ((!_h) || (!_db_url)) {
		LOG(L_ERR, "connect_db(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	CON_CONNECTED(_h) = FALSE;

	     /* Make a scratch pad copy of given SQL URL */
	l = strlen(_db_url);
	buf = (char*)pkg_malloc(l + 1);
	if (!buf) {
		LOG(L_ERR, "connect_db(): Not enough memory\n");
		return FALSE;
	}
	memcpy(buf, _db_url, l + 1);

	res = parse_sql_url(buf, &user, &password, &host, &port, &database);
	if (port) {
		p = atoi(port);
	} else {
		p = 0;
	}
	
	if (res == FALSE) {
		LOG(L_ERR, "connect_db(): Error while parsing SQL URL\n");
		pkg_free(buf);
		return FALSE;
	}

	CON_CONNECTION(_h) = (MYSQL*)pkg_malloc(sizeof(MYSQL));
	if (!CON_CONNECTION(_h)) {
		LOG(L_ERR, "connect_db(): No enough memory\n");
		pkg_free(buf);
		return FALSE;
	}

	mysql_init(CON_CONNECTION(_h));

	if (!mysql_real_connect(CON_CONNECTION(_h), host, user, password, database, p, NULL, 0)) {
		LOG(L_ERR, "connect_db(): %s\n", mysql_error(CON_CONNECTION(_h)));
		mysql_close(CON_CONNECTION(_h));
		pkg_free(buf);
		pkg_free(CON_CONNECTION(_h));
		return FALSE;
	}

	pkg_free(buf);
	CON_CONNECTED(_h) = TRUE;
	return TRUE;
}


/*
 * Disconnect database connection
 *
 * disconnects database connection represented by _handle
 * returns 1 on success, 0 otherwise
 */
static int disconnect_db(db_con_t* _h)
{
#ifdef PARANOID
	if (!_h) {
		LOG(L_ERR, "disconnect_db(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	if (CON_CONNECTED(_h) == TRUE) {
		mysql_close(CON_CONNECTION(_h));
		CON_CONNECTED(_h) = FALSE;
		pkg_free(CON_CONNECTION(_h));
		return TRUE;
	} else {
		return FALSE;
	}
}



/*
 * Initialize database module
 * No function should be called before this
 */
db_con_t* db_init(const char* _sqlurl)
{
	db_con_t* res;
#ifdef PARANOID
	if (!_sqlurl) {
		LOG(L_ERR, "db_init(): Invalid parameter value\n");
		return NULL;
	}
#endif

	res = pkg_malloc(sizeof(db_con_t));
	if (!res) {
		LOG(L_ERR, "db_init(): No memory left\n");
		return NULL;
	} else {
		memset(res, 0, sizeof(db_con_t));
	}

	if (!connect_db(res, _sqlurl)) {
		LOG(L_ERR, "db_init(): Error while trying to connect database\n");
		pkg_free(res);
		return NULL;
	}

	return res;
}


/*
 * Shut down database module
 * No function should be called after this
 */
void db_close(db_con_t* _h)
{
#ifdef PARANOID
	if (!_h) {
		LOG(L_ERR, "db_close(): Invalid parameter value\n");
		return;
	}
#endif
	disconnect_db(_h);
	if (CON_RESULT(_h)) {
		mysql_free_result(CON_RESULT(_h));
	}
	if (CON_TABLE(_h)) {
		pkg_free(CON_TABLE(_h));
	}
	pkg_free(_h);
}


static int submit_query(db_con_t* _h, const char* _s)
{	
#ifdef PARANOID
	if ((!_h) || (!_s)) {
		LOG(L_ERR, "submit_query(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	DBG("submit_query(): %s\n", _s);
	if (mysql_query(CON_CONNECTION(_h), _s)) {
		LOG(L_ERR, "submit_query(): %s\n", mysql_error(CON_CONNECTION(_h)));
		return FALSE;
	} else {
		return TRUE;
	}
}


int get_result(db_con_t* _h, db_res_t** _r)
{
#ifdef PARANOID
	if ((!_h) || (!_r)) {
		LOG(L_ERR, "get_result(): Invalid parameter value\n");
		return FALSE;
	}
#endif

	CON_RESULT(_h) = mysql_store_result(CON_CONNECTION(_h));
	if (!CON_RESULT(_h)) {
		LOG(L_ERR, "get_result(): %s\n", mysql_error(CON_CONNECTION(_h)));
		*_r = NULL;
		return FALSE;
	}

	*_r = new_result();
        if (convert_result(_h, *_r) == FALSE) {
		LOG(L_ERR, "get_result(): Error while converting result\n");
		free_result(*_r);
		return FALSE;
	}
	
	return TRUE;
}



int db_free_query(db_con_t* _h, db_res_t* _r)
{
#ifdef PARANOID
     if ((!_h) || (!_r)) {
	     LOG(L_ERR, "db_free_query(): Invalid parameter value\n");
	     return FALSE;
     }
#endif
     if (free_result(_r) == FALSE) {
	     LOG(L_ERR, "free_query(): Unable to free result structure\n");
	     return FALSE;
     }
     mysql_free_result(CON_RESULT(_h));
     CON_RESULT(_h) = NULL;
     return TRUE;
}


/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: nmber of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_query(db_con_t* _h, db_key_t* _k, 
	     db_val_t* _v, db_key_t* _c, int _n, int _nc,
	     db_key_t _o, db_res_t** _r)
{
	int off;
#ifdef PARANOID
	if ((!_h) || (!_r)) {
		LOG(L_ERR, "db_query(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	if (!_c) {
		off = snprintf(query_buf, SQL_BUF_LEN, "select * from %s ", CON_TABLE(_h));
	} else {
		off = snprintf(query_buf, SQL_BUF_LEN, "select ");
		off += print_columns(query_buf + off, SQL_BUF_LEN - off, _c, _nc);
		off += snprintf(query_buf + off, SQL_BUF_LEN - off, "from %s ", CON_TABLE(_h));
	}
	if (_n) {
		off += snprintf(query_buf + off, SQL_BUF_LEN - off, "where ");
		off += print_where(query_buf + off, SQL_BUF_LEN - off, _k, _v, _n);
	}
	if (_o) {
		off += snprintf(query_buf + off, SQL_BUF_LEN - off, "order by %s", _o);
	}

	if (submit_query(_h, query_buf) == FALSE) {
		LOG(L_ERR, "query_table(): Error while submitting query\n");
		return FALSE;
	}

	return get_result(_h, _r);
}


static int print_columns(char* _b, int _l, db_key_t* _c, int _n)
{
	int i;
	int res = 0;
#ifdef PARANOID
	if ((!_c) || (!_n) || (!_b) || (!_l)) {
		LOG(L_ERR, "print_columns(): Invalid parameter value\n");
		return 0;
	}
#endif
	for(i = 0; i < _n; i++) {
		if (i == (_n - 1)) {
			res += snprintf(_b + res, _l - res, "%s ", _c[i]);
		} else {
			res += snprintf(_b + res, _l - res, "%s,", _c[i]);
		}
	}
	return res;
}


static int print_values(char* _b, int _l, db_val_t* _v, int _n)
{
	int i, res = 0, l;
#ifdef PARANOID
	if ((!_b) || (!_l) || (!_v) || (!_n)) {
		LOG(L_ERR, "print_values(): Invalid parameter value\n");
		return 0;
	}
#endif

	for(i = 0; i < _n; i++) {
		l = _l - res;
		if (val2str(_v + i, _b + res, &l) == FALSE) {
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


static int print_where(char* _b, int _l, db_key_t* _k, db_val_t* _v, int _n)
{
	int i;
	int res = 0;
	int l;
#ifdef PARANOID
	if ((!_b) || (!_l) || (!_k) || (!_v) || (!_n)) {
		LOG(L_ERR, "print_where(): Invalid parameter value\n");
		return 0;
	}
#endif
	for(i = 0; i < _n; i++) {
		res += snprintf(_b + res, _l - res, "%s=", _k[i]);
		l = _l - res;
		val2str(&(_v[i]), _b + res, &l);
		res += l;
		if (i != (_n - 1)) {
			res += snprintf(_b + res, _l - res, " AND ");
		}
	}
	return res;
}


static int print_set(char* _b, int _l, db_key_t* _k, db_val_t* _v, int _n)
{
	int i;
	int res = 0;
	int l;
#ifdef PARANOID
	if ((!_b) || (!_l) || (!_k) || (!_v) || (!_n)) {
		LOG(L_ERR, "print_set(): Invalid parameter value\n");
		return 0;
	}
#endif
	for(i = 0; i < _n; i++) {
		res += snprintf(_b + res, _l - res, "%s=", _k[i]);
		l = _l - res;
		val2str(&(_v[i]), _b + res, &l);
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
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	int off;
#ifdef PARANOID
	if ((!_h) || (!_k) || (!_v) || (!_n)) {
		LOG(L_ERR, "db_insert(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	off = snprintf(ins_buf, SQL_BUF_LEN, "insert into %s (", CON_TABLE(_h));
	off += print_columns(ins_buf + off, SQL_BUF_LEN - off, _k, _n);
	off += snprintf(ins_buf + off, SQL_BUF_LEN - off, ") values (");
	off += print_values(ins_buf + off, SQL_BUF_LEN - off, _v, _n);
	*(ins_buf + off++) = ')';
	*(ins_buf + off) = '\0';

	if (submit_query(_h, ins_buf) == FALSE) {
	        LOG(L_ERR, "insert_row(): Error while submitting query\n");
		return FALSE;
	}
	return TRUE;
}


/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_delete(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	int off;
#ifdef PARANOID
	if (!_h) {
		LOG(L_ERR, "db_delete(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	off = snprintf(del_buf, SQL_BUF_LEN, "delete from %s", CON_TABLE(_h));
	if (_n) {
		off += snprintf(del_buf + off, SQL_BUF_LEN - off, " where ");
		off += print_where(del_buf + off, SQL_BUF_LEN - off, _k, _v, _n);
	}
	if (submit_query(_h, del_buf) == FALSE) {
		LOG(L_ERR, "delete_row(): Error while submitting query\n");
		return FALSE;
	}
	return TRUE;
}


/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_update(db_con_t* _h, db_key_t* _k, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
	int off;
#ifdef PARANOID
	if ((!_h) || (!_uk) || (!_uv) || (!_un)) {
		LOG(L_ERR, "db_update(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	off = snprintf(update_buf, SQL_BUF_LEN, "update %s set ", CON_TABLE(_h));
	off += print_set(update_buf + off, SQL_BUF_LEN - off, _uk, _uv, _un);
	if (_n) {
		off += snprintf(update_buf + off, SQL_BUF_LEN - off, " where ");
		off += print_where(update_buf + off, SQL_BUF_LEN - off, _k, _v, _n);
		*(update_buf + off) = '\0';
	}

	if (submit_query(_h, update_buf) == FALSE) {
		LOG(L_ERR, "update_row(): Error while submitting query\n");
		return FALSE;
	}
	return TRUE;
}

