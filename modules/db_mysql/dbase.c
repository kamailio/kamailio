#include "dbase.h"
#include "../../mem.h"
#include "../../dprint.h"
#include <mysql/mysql.h>
#include "db_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"


/*
 * Declarations
 */
static int        connect_db    (db_con_t* _h, const char* _db_url);
static int        disconnect_db (db_con_t* _h);
static int        submit_query  (db_con_t* _h, const char* _s);
static db_res_t*  get_result    (db_con_t* _h);
static int        free_query    (db_con_t* _h, db_res_t* _r);
static int        print_columns (char* _b, int _l, db_key_t* _c, int _n);
static int        print_values  (char* _b, int _l, db_val_t* _v, int _n);
static int        print_cond    (char* _b, int _l, db_key_t* _k, db_val_t* _v, int _n);


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
	int res;
	int p;
	char* user, *password, *host, *port, *database;
	char* buf;

#ifdef PARANOID
	if (!_h) return FALSE;
	if (!_db_url) return FALSE;
#endif
	     /* Make a scratch pad copy of given SQL URL */
	buf = pkg_malloc(strlen(_db_url) + 1);
	if (!buf) {
		log(L_ERR, "connect_db(): Not enough memory\n");
		return FALSE;
	}

	mysql_init(&(_h->con));
	res = parse_sql_url(buf, &user, &password, &host, &port, &database);
	if (port) {
		p = atoi(port);
	} else {
		p = 0;
	}
	
	if (res == FALSE) {
		log(L_ERR, "connect_db(): Error while parsing SQL URL\n");
		mysql_close(&(_h->con));
		return FALSE;
	}
	if (!mysql_real_connect(&(_h->con), host, user, password, database, p, NULL, 0)) {
		log(L_ERR, "connect_db(): %s\n", mysql_error(&(_h->con)));
		mysql_close(&(_h->con));
		return FALSE;
	}

	pkg_free(buf);
	_h->connected = TRUE;
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
	if (!_h) return FALSE;
#endif
	if (_h->connected == TRUE) {
		mysql_close(&(_h->con));
		_h->connected = FALSE;
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
	if (!_sqlurl) return NULL;
#endif

	res = pkg_malloc(sizeof(db_con_t));
	if (!res) {
		log(L_ERR, "db_init(): No memory left\n");
		return NULL;
	} else {
		memset(res, 0, sizeof(db_con_t));
	}

	if (!connect_db(res, _sqlurl)) {
		log(L_ERR, "db_init(): Error while trying to connect database\n");
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
	if (!_h) return;
#endif
	disconnect_db(_h);
	if (_h->res) {
		mysql_free_result(_h->res);
	}
	if (_h->table) {
		pkg_free(_h->table);
	}
	pkg_free(_h);
}


static int submit_query(db_con_t* _h, const char* _s)
{	
#ifdef PARANOID
	if (!_h) return FALSE;
	if (!_s) return FALSE;
#endif
	DBG("submit_query(): %s\n", _s);
	if (mysql_query(&(_h->con), _s)) {
		log(L_ERR, "submit_query(): %s\n", mysql_error(&(_h->con)));
		return FALSE;
	} else {
		return TRUE;
	}
}


db_res_t* get_result(db_con_t* _h)
{
	db_res_t* res;
	int n, i;
#ifdef PARANOID
	if (!_h) return NULL;
#endif

	_h->res = mysql_store_result(&(_h->con));
	if (!_h->res) {
		log(L_ERR,"get_result(): %s\n", mysql_error(&(_h->con)));
		return NULL;
	}

	res = new_result();
        if (convert_result(_h, res) == FALSE) {
		log(L_ERR, "get_result(): Error while converting result\n");
		free_result(res);
		return NULL;
	}
	
	return res;
}



int db_free_query(db_con_t* _h, db_res_t* _r)
{
#ifdef PARANOID
     if (!_h) return FALSE;
     if (!_r) return FALSE;
#endif
     if (free_result(_r) == FALSE) {
	     log(L_ERR, "free_query(): Unable to free result structure\n");
	     return FALSE;
     }
     mysql_free_result(_h->res);
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
db_res_t* db_query(db_con_t* _h, db_key_t* _k, 
		   db_val_t* _v, db_key_t* _c, int _n, int _nc)
{
	int off;
#ifdef PARANOID
	if (!_h) return NULL;
#endif
	if (!_c) {
		off = snprintf(query_buf, SQL_BUF_LEN, "select * from %s ", _h->table);
	} else {
		off = snprintf(query_buf, SQL_BUF_LEN, "select ");
		off += print_columns(query_buf + off, SQL_BUF_LEN - off, _c, _nc);
		off += snprintf(query_buf + off, SQL_BUF_LEN - off, "from %s ", _h->table);
	}
	if (_n) {
		off += snprintf(query_buf + off, SQL_BUF_LEN - off, "where ");
		off += print_cond(query_buf + off, SQL_BUF_LEN - off, _k, _v, _n);
	}
	if (submit_query(_h, query_buf) == FALSE) {
		log(L_ERR, "query_table(): Error while submitting query\n");
		return NULL;
	}

	return get_result(_h);
}


static int print_columns(char* _b, int _l, db_key_t* _c, int _n)
{
	int i;
	int res = 0;
#ifdef PARANOID
	if (!_c) return 0;
	if (!_n) return 0;
	if (!_b) return 0;
	if (!_l) return 0;
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
	if (!_b) return 0;
	if (!_l) return 0;
	if (!_v) return 0;
	if (!_n) return 0;
#endif

	for(i = 0; i < _n; i++) {
		l = _l - res;
		res += val2str(_v, _b + res, &l);
		res += l;
		if (i != (_n - 1)) {
			*(_b + res) = ',';
			res++;
		}
	}
	return res;
}


static int print_cond(char* _b, int _l, db_key_t* _k, db_val_t* _v, int _n)
{
	int i;
	int res = 0;
	int l;
#ifdef PARANOID
	if (!_b) return 0;
	if (!_l) return 0;
	if (!_k) return 0;
	if (!_v) return 0;
	if (!_n) return 0;
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
	if ((!_h) || (!_k) || (!_v) || (!_n)) return FALSE;
#endif
	off = snprintf(ins_buf, SQL_BUF_LEN, "insert into %s (", _h->table);
	off += print_columns(ins_buf + off, SQL_BUF_LEN - off, _k, _n);
	off += snprintf(ins_buf + off, SQL_BUF_LEN - off, ") values (");
	off += print_values(ins_buf + off, SQL_BUF_LEN - off, _v, _n);
	*(ins_buf + off++) = ')';
	*(ins_buf + off) = '\0';

	if (submit_query(_h, ins_buf) == FALSE) {
		log(L_ERR, "insert_row(): Error while submitting query\n");
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
	if ((!_h) || (!_k) || (!_v) || (!_n)) return FALSE;
#endif
	off = snprintf(del_buf, SQL_BUF_LEN, "delete from %s where ", _h->table);
	off += print_cond(del_buf + off, SQL_BUF_LEN - off, _k, _v, _n);
	
	if (submit_query(_h, del_buf) == FALSE) {
		log(L_ERR, "delete_row(): Error while submitting query\n");
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
	if ((!_h) || (!_uk) || (!_uv) || (!_un)) return FALSE;
#endif
	off = snprintf(update_buf, SQL_BUF_LEN, "update %s set ", _h->table);
	off += print_cond(update_buf + off, SQL_BUF_LEN - off, _uk, _uv, _un);
	if (_n) {
		off += snprintf(update_buf + off, SQL_BUF_LEN - off, " where ");
		off += print_cond(update_buf + off, SQL_BUF_LEN - off, _k, _v, _n);
	}

	if (submit_query(_h, update_buf) == FALSE) {
		log(L_ERR, "update_row(): Error while submitting query\n");
		return FALSE;
	}
	return TRUE;
}

