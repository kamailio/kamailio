#define __USE_XOPEN /* Because of strptime */
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dbcon_mysql.h"
#include "utils.h"
#include <mysql/mysql.h>


/*
 * Declarations
 */
static int        connect_db    (const char* _db_url);
static int        disconnect_db (void);
static inline int is_connected  (void);
static void       time2mysql    (time_t _time, char* _result, int _res_len);
static time_t     mysql2time    (const char* _str);


static char ins_buf   [SQL_BUF_LEN];
static char del_buf   [SQL_BUF_LEN];
static char query_buf [SQL_BUF_LEN];
static char update_buf[SQL_BUF_LEN];


/*
 * MySQL connection handle
 */
static MYSQL      db_con;
static MYSQL_RES* db_res;
static MYSQL_ROW  cur_row;


/*
 * MySQL connection handle
 */
static int connected = FALSE;


/*
 * Establish database connection,
 * returns 1 on success, 0 otherwise
 * _handle is a handle used in communication with database
 *
 * URL is in form sql://user:password@host:port/database
 */
static int connect_db(const char* _db_url)
{
	int res;
	int p;
	char* user, *password, *host, *port, *database;

	     /* Make a scratch pad copy of given SQL URL */
	char* buf = strdup(_db_url);

	mysql_init(&db_con);
	res = parse_sql_url(buf, &user, &password, &host, &port, &database);
	if (port) {
		p = atoi(port);
	} else {
		p = 0;
	}

	if (res == FALSE) {
		printf("connect_db(): Error while parsing SQL URL\n");
		mysql_close(&db_con);
		return FALSE;
	}
	if (!mysql_real_connect(&db_con, host, user, password, database, p, NULL, 0)) {
		printf("connect_db(): %s\n", mysql_error(&db_con));
		mysql_close(&db_con);
		return FALSE;
	}

	free(buf);
	connected = TRUE;
	return TRUE;
}


/*
 * Disconnect database connection
 *
 * disconnects database connection represented by _handle
 * returns 1 on success, 0 otherwise
 */
static int disconnect_db(void)
{
	if (is_connected()) {
		mysql_close(&db_con);
		connected = FALSE;
		return TRUE;
	} else {
		return FALSE;
	}
}


static inline int is_connected(void)
{
	return (connected == TRUE);
}


/*
 * Convert time_t structure to format accepted by MySQL database
 */
static void time2mysql(time_t _time, char* _result, int _res_len)
{
	struct tm* t;

	     /*
	       if (_time == MAX_TIME_T) {
	       snprintf(_result, _res_len, "0000-00-00 00:00:00");
	       }
	     */

	t = gmtime(&_time);
	strftime(_result, _res_len, "%Y-%m-%d %H:%M:%S", t);
}


/*
 * Convert MySQL time representation to time_t structure
 */
static time_t mysql2time(const char* _str)
{
	struct tm time;

	     /* It is neccessary to zero tm structure first */
	memset(&time, '\0', sizeof(struct tm));
	strptime(_str, "%Y-%m-%d %H:%M:%S", &time);
	return mktime(&time);
}


/*
 * Initialize database module
 * No function should be called before this
 */
int db_init(const char* _sqlurl)
{
	if (!connect_db(_sqlurl)) {
		printf("db_init(): Error while trying to connect database\n");
		return FALSE;
	}

	return TRUE;
}


/*
 * Shut down database module
 * No function should be called after this
 */
void db_close(void)
{
	disconnect_db();
	if (db_res) {
		mysql_free_result(db_res);
		db_res = NULL;
	}
}


/*
 * Find all contacts that belong to give Address Of Record
 */
int query_location(const char* _tab, const char* _aor, location_t** _res)
{
	int rows, res;

	*_res = NULL;

	snprintf(query_buf, SQL_BUF_LEN, 
		 "select contact,expire,q from %s where user='%s'", _tab, _aor);

	res = mysql_query(&db_con, query_buf);
	if (res) {
		printf("query_location(): %s\n", mysql_error(&db_con));
		return FALSE;
	}

	db_res = mysql_store_result(&db_con);
	if (!db_res) {
		printf("query_location(): %s\n", mysql_error(&db_con));
		return FALSE;
	}

	rows = mysql_num_rows(db_res);
	if (rows) {
		*_res = create_location(_aor, FALSE, 0);

		while ((cur_row = mysql_fetch_row(db_res))) {
			add_contact(*_res, cur_row[0],
				    mysql2time(cur_row[1]),
				    atof(cur_row[2]), FALSE, FALSE);
		}
		
	}
	mysql_free_result(db_res);

	return TRUE;
}


int insert_contact(const char* _tab, const contact_t* _con)
{
	char time[256];
	int res;

#ifdef PARANOID
	if (!_tab) return FALSE;
	if (!_con) return FALSE;
#endif

	time2mysql(_con->expire, time, 256);
	snprintf(ins_buf, SQL_BUF_LEN, 
		 " insert into %s (user,contact,expire,q) values ('%s','%s','%s',%10.2f)",
		 _tab, _con->aor->s, _con->c.s, time, _con->q);

	printf("SQL: %s\n", ins_buf);
	res = mysql_query(&db_con, ins_buf);
	if (res) {
		//		ERR("insert_contact(): %s\n", mysql_error(&db_con));
		return FALSE;
	}
	return TRUE;
}


int insert_location(const char* _tab, const location_t* _loc)
{
	int off, res;
	contact_t* ptr = _loc->contacts;
	char time[256];

	if (!ptr) {
		printf("insert_location(): Nothing to be inserted\n");
		return FALSE;
	}

	off = snprintf(ins_buf, SQL_BUF_LEN,  
		       "insert into %s (user,contact,expire,q) values ", _tab);
	
        while (ptr) {
		time2mysql(ptr->expire, time, 256);
		off += snprintf(ins_buf + off, SQL_BUF_LEN - off, 
				"('%s','%s','%s',%10.2f)",
				_loc->user.s, ptr->c.s, time, ptr->q);
		ptr = ptr->next;
		if (ptr) {
			ins_buf[off++] = ',';
		}
	}

	printf("SQL: %s\n", ins_buf);
	res = mysql_query(&db_con, ins_buf);
	if (res) {
		//		ERR("insert_location(): %s\n", mysql_error(&db_con));
		return FALSE;
	}

	return TRUE;
}


int delete_location(const char* _tab, const location_t* _loc)
{
	int res = 0;
	contact_t* ptr = _loc->contacts;

	if (_loc->star) {
		snprintf(del_buf, SQL_BUF_LEN,
			 "delete from %s where user='%s'",
			 _tab, _loc->user.s);
		printf("SQL: %s\n", del_buf);

		res = mysql_query(&db_con, del_buf);
		if (res) {
			printf("delete_location(): %s\n", mysql_error(&db_con));
			return FALSE;
		} else {
			return TRUE;
		}
	} else {
		while (ptr) {
			snprintf(del_buf, SQL_BUF_LEN,
				 "delete from %s where user='%s' and contact='%s'",
				 _tab, _loc->user.s, ptr->c.s); 
			printf("SQL: %s\n", del_buf);
			res |= mysql_query(&db_con, del_buf);
			ptr = ptr->next;
		}
	}

	if (res) {
		printf("delete_location(): %s\n", mysql_error(&db_con));
		return FALSE;
	} else {
		return TRUE;
	}
}



int update_location(const char* _tab, const location_t* _loc)
{
	int res = 0;
	contact_t* ptr = _loc->contacts;
	char time[256];

	while (ptr) {
		time2mysql(ptr->expire, time, 256);
		snprintf(update_buf, SQL_BUF_LEN,
			 "update %s set expire='%s', q=%10.2f where user='%s' and contact='%s'",
			 _tab, time, ptr->q, _loc->user.s, ptr->c.s); 

		printf("SQL: %s\n", update_buf);
		res  |= mysql_query(&db_con, update_buf);
		ptr = ptr->next;
	}
	
	if (res) {
		printf("update_location(): %s\n", mysql_error(&db_con));
		return FALSE;
	} else {
		return TRUE;
	}
}


int update_contact(const char* _tab, const contact_t* _con)
{
	char time[256];
	time2mysql(_con->expire, time, 256);
	snprintf(update_buf, SQL_BUF_LEN,
		 "update %s set expire='%s', q=%10.2f where user='%s' and contact='%s'",
		 _tab, time, _con->q, _con->aor->s, _con->c.s); 
	printf("SQL: %s\n", update_buf);
	if (mysql_query(&db_con, update_buf)) {
		printf("update_contact(): %s\n", mysql_error(&db_con));
		return FALSE;
	} else {
		return TRUE;
	}
}




/*
 * SQL URL parser
 */
int parse_sql_url(char* _url, char** _user, char** _pass, 
		  char** _host, char** _port, char** _db)
{
	char* slash, *dcolon, *at, *db_slash;

	*_user = '\0';
	*_pass = '\0';
	*_host = '\0';
	*_port = '\0';
	*_db   = '\0';

	     /* Remove any leading and trailing spaces and tab */
	_url = trim(_url);

	if (strlen(_url) < 6) return FALSE;

	if (*_url == '\0') return FALSE; /* Empty string */

	slash = strchr(_url, '/');
	if (!slash) return FALSE;   /* Invalid string, slashes not found */

	if ((*(++slash)) != '/') {  /* Invalid URL, 2nd slash not found */
		return FALSE;
	}

	slash++;

	at = strchr(slash, '@');

	db_slash = strchr(slash, '/');
	if (db_slash) {
		*db_slash++ = '\0';
		*_db = trim(db_slash);
	}

	if (!at) {
		dcolon = strchr(slash, ':');
		if (dcolon) {
			*dcolon++ = '\0';
			*_port = trim(dcolon);
		}
		*_host = trim(slash);
	} else {
		dcolon = strchr(slash, ':');
	        *at++ = '\0';
		if (dcolon) {
			*dcolon++ = '\0';
			if (dcolon < at) {   /* user:passwd */
				*_pass = trim(dcolon);
				dcolon = strchr(at, ':');
				if (dcolon) {  /* host:port */
					*dcolon++ = '\0';
					*_port = trim(dcolon);
				}
			} else {            /* host:port */
				*_port = trim(dcolon);
			}
		}
		*_host = trim(at);
		*_user = trim(slash);
	}

	return TRUE;
}

