/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 *
 * Copyright (C) 2003 August.Net Services, LLC
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
 *
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 */

#define MAXCOLUMNS	512

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "db_utils.h"
#include "defs.h"
#include "dbase.h"
#include "con_postgres.h"
#include "aug_std.h"

long getpid();

static char sql_buf[SQL_BUF_LEN];

static int submit_query(db_con_t* _h, const char* _s);
static int connect_db(db_con_t* _h, const char* _db_url);
static int disconnect_db(db_con_t* _h);
static int free_query(db_con_t* _h);

/*
** connect_db	Connect to a database
**
**	Arguments :
**		db_con_t *	as previously supplied by db_init()
**		char *_db_url	the database to connect to
**
**	Returns :
**		0 upon success
**		negative number upon failure
**
**	Notes :
**		If currently connected, a disconnect is done first
**		if this process did the connection, otherwise the
**		disconnect is not done before the new connect.
**		This is important, as the process that owns the connection
**		should clean up after itself.
*/

static int connect_db(db_con_t* _h, const char* _db_url)
{
	char* user, *password, *host, *port, *database;

	if(! _h)
	{
		PLOG("connect_db", "must pass db_con_t!");
		return(-1);
	}

	if(CON_CONNECTED(_h))
	{
		DLOG("connect_db", "disconnect first!");
		disconnect_db(_h);
	}

	/*
	** CON_CONNECTED(_h) is now 0, set by disconnect_db()
	*/

	/*
	** Note :
	** Make a scratch pad copy of given SQL URL.
	** all memory allocated to this connection is rooted
	** from this.
	** This is an important concept.
	** as long as you always allocate memory using the function:
	** mem = aug_alloc(size, CON_SQLURL(_h)) or
	** str = aug_strdup(string, CON_SQLURL(_h))
	** where size is the amount of memory, then in the future
	** when CON_SQLURL(_h) is freed (in the function disconnect_db())
	** all other memory allocated in this manner is freed.
	** this will keep memory leaks from happening.
	*/
	CON_SQLURL(_h) = aug_strdup((char *) _db_url, (char *) _h);

	/*
	** get the connection parameters parsed from the db_url string
	** it looks like: postgres://username:userpass@dbhost:dbport/dbname
	** username/userpass : name and password for the database
	** dbhost :            the host name or ip address hosting the database
	** dbport :            the port to connect to database on
	** dbname :            the name of the database
	*/
	if(parse_sql_url(CON_SQLURL(_h),
		&user,&password,&host,&port,&database) < 0)
	{
		char buf[256];
		sprintf(buf, "Error while parsing %s", _db_url);
		PLOG("connect_db", buf);

		aug_free(CON_SQLURL(_h));
		return -3;
	}

	/*
	** finally, actually connect to the database
	*/
	CON_CONNECTION(_h) =
		PQsetdbLogin(host,port,NULL,NULL,database,user, password);

	if(CON_CONNECTION(_h) == 0
	    || PQstatus(CON_CONNECTION(_h)) != CONNECTION_OK)
	{
		PLOG("connect_db", PQerrorMessage(CON_CONNECTION(_h)));
		PQfinish(CON_CONNECTION(_h));
		aug_free(CON_SQLURL(_h));
		return -4;
	}

	CON_PID(_h) = getpid();

	/*
	** all is well, database was connected, we can now submit_query's
	*/
	CON_CONNECTED(_h) = 1;
	return 0;
}


/*
** disconnect_db	Disconnect a database
**
**	Arguments :
**		db_con_t *	as previously supplied by db_init()
**
**	Returns :
**		0 upon success
**		negative number upon failure
**
**	Notes :
**		All memory associated with CON_SQLURL is freed.
**		
*/

static int disconnect_db(db_con_t* _h)
{
	if(! _h)
	{
		DLOG("disconnect_db", "null db_con_t, ignored!\n");
		return(0);
	}

	/*
	** free lingering memory tree if it exists
	*/
	if(CON_SQLURL(_h))
	{
		aug_free(CON_SQLURL(_h));
		CON_SQLURL(_h) = (char *) 0;
	}

	/*
	** ignore if there is no current connection
	*/
	if(CON_CONNECTED(_h) != 1)
	{
		DLOG("disconnect_db", "not connected, ignored!\n");
		return 0;
	}

	/*
	** make sure we are trying to close a connection that was opened
	** by our process ID
	*/
	if(CON_PID(_h) == getpid())
	{
		PQfinish(CON_CONNECTION(_h));
		CON_CONNECTED(_h) = 0;
	}
	else
	{
		DLOG("disconnect_db",
			"attempt to release connection not owned, ignored!\n");
	}

	return 0;
}

/*
** db_init	initialize database for future queries
**
**	Arguments :
**		char *_sqlurl;	sql database to open
**
**	Returns :
**		db_con_t * NULL upon error
**		db_con_t * if successful
**
**	Notes :
**		db_init must be called prior to any database
**		functions.
*/

db_con_t *db_init(const char* _sqlurl)
{
	db_con_t* res;

	DLOG("db_init", "entry");

	/*
	** this is the root memory for this database connection.
	*/
	res = aug_alloc(sizeof(db_con_t) + sizeof(struct con_postgres), NULL);
	memset(res, 0, sizeof(db_con_t) + sizeof(struct con_postgres));

	if (connect_db(res, _sqlurl) < 0)
	{
		PLOG("db_init", "Error while trying to open database, FATAL\n");
		aug_free(res);
		return((db_con_t *) 0);
	}

	return res;
}


/*
** db_close	last function to call when db is no longer needed
**
**	Arguments :
**		db_con_t * the connection to shut down, as supplied by db_init()
**
**	Returns :
**		(void)
**
**	Notes :
**		All memory and resources are freed.
*/

void db_close(db_con_t* _h)
{
	DLOG("db_close", "entry");
	if(! _h)
	{
		PLOG("db_close", "no handle passed, ignored");
		return;
	}

	disconnect_db(_h);
	aug_free(_h);

}

/*
** submit_query	run a query
**
**	Arguments :
**		db_con_t *	as previously supplied by db_init()
**		char *_s	the text query to run
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

static int submit_query(db_con_t* _h, const char* _s)
{	
	int rv;

	/*
	** this bit of nonsense in case our connection get screwed up 
	*/
	switch(rv = PQstatus(CON_CONNECTION(_h)))
	{
		case CONNECTION_OK: break;
		case CONNECTION_BAD:
			PLOG("submit_query", "connection reset");
			PQreset(CON_CONNECTION(_h));
		break;
	}

	/*
	** free any previous query that is laying about
	*/
	if(CON_RESULT(_h))
	{
		free_query(_h);
	}

	/*
	** exec the query
	*/
	CON_RESULT(_h) = PQexec(CON_CONNECTION(_h), _s);

	rv = 0;
	if(PQresultStatus(CON_RESULT(_h)) == 0)
	{
		PLOG("submit_query", "initial failure, FATAL");
		/* 
		** terrible error??
		*/
		rv = -3;
	}
	else
	{
		/*
		** the query ran, get the status
		*/
		switch(PQresultStatus(CON_RESULT(_h)))
		{
			case PGRES_EMPTY_QUERY: rv = -9; break;
			case PGRES_COMMAND_OK: rv = 0; break;
			case PGRES_TUPLES_OK: rv = 0; break;
			case PGRES_COPY_OUT: rv = -4; break;
			case PGRES_COPY_IN: rv = -5; break;
			case PGRES_BAD_RESPONSE: rv = -6; break;
			case PGRES_NONFATAL_ERROR: rv = -7; break;
			case PGRES_FATAL_ERROR: rv = -8; break;
			default: rv = -2; break;
		}
	}
	if(rv < 0)
	{
		/*
		** log the error
		*/
		char buf[256];
		sprintf(buf, "query '%s', result '%s'\n",
			_s, PQerrorMessage(CON_CONNECTION(_h)));
		PLOG("submit_query", buf);
	}

	return(rv);
}

/*
** free_query	clear the db channel and clear any old query result status
**
**	Arguments :
**		db_con_t *	as previously supplied by db_init()
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

static int free_query(db_con_t* _h)
{
	if(CON_RESULT(_h))
	{
		PQclear(CON_RESULT(_h));
		CON_RESULT(_h) = 0;
	}

	return 0;
}

/*
** db_free_query	free the query and free the result memory
**
**	Arguments :
**		db_con_t *	as previously supplied by db_init()
**		db_res_t *	the result of a query
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

int db_free_query(db_con_t* _h, db_res_t* _r)
{
	free_query(_h);
	free_result(_r);

	return 0;
}

/*
** begin_transaction	begin transaction
**
**	Arguments :
**		db_con_t *	as previously supplied by db_init()
**		char *		this is just in case an error message
**				is printed, we will know which query
**				was going to be run, giving us a code debug hint
**
**	Returns :
**		0 upon success
**		negative number upon failure
**
**	Notes :
**		This function may be called with a messed up communication
**		channel.  Therefore, alot of this function is dealing with
**		that.  Wen the layering gets corrected later this stuff
**		should continue to work correctly, it will just be
**		way to defensive.
*/

static int begin_transaction(db_con_t * _h, char *_s)
{
	PGresult *mr;
	int rv;

	/*
	** Note:
	**  The upper layers of code may attempt a transaction
	**  before opening or having a valid connection to the
	**  database.  We try to sense this, and open the database
	**  if we have the sqlurl in the _h structure.  Otherwise,
	**  all we can do is return an error.
	*/

	if(_h)
	{
		if(CON_CONNECTED(_h))
		{
			mr = PQexec(CON_CONNECTION(_h), "BEGIN");
			if(!mr || PQresultStatus(mr) != PGRES_COMMAND_OK)
			{
				/*
				** We get here if the connection to the
				** db is corrupt, which can happen a few
				** different ways, but all of them are
				** related to the parent process forking,
				** or being forked.
				*/
				PLOG("begin_transaction","corrupt connection");
				CON_CONNECTED(_h) = 0;
			}
			else
			{
				/*
				** this is the normal way out.
				** the transaction ran fine.
				*/
				PQclear(mr);
				return(0);
			}
		}
		else
		{
			DLOG("begin_transaction", "called before db_init");
		}

		/*
		** if we get here we have a corrupt db connection,
		** but we probably have a valid db_con_t structure.
		** attempt to open the db.
		*/

		if((rv = connect_db(_h, CON_SQLURL(_h))) != 0)
		{
			/*
			** our attempt to fix the connection failed
			*/
			char buf[256];
			sprintf(buf, "no connection, FATAL %d!", rv);
			PLOG("begin_transaction",buf);
			return(rv);
		}
	}
	else
	{
		PLOG("begin_transaction","must call db_init first!");
		return(-1);
	}

	/*
	** we get here if the database connection was corrupt,
	** i didn't want to use recursion ...
	*/

	mr = PQexec(CON_CONNECTION(_h), "BEGIN");
	if(!mr || PQresultStatus(mr) != PGRES_COMMAND_OK)
	{
		char buf[256];
		sprintf("FATAL %s, '%s'!\n",
			PQerrorMessage(CON_CONNECTION(_h)), _s);
		PLOG("begin_transaction", buf);
		return(-1);
	}

	DLOG("begin_transaction", "db channel reset successful");

	PQclear(mr);
	return(0);
}

/*
** commit_transaction	any begin_transaction must be terminated with this
**
**	Arguments :
**		db_con_t *	as previously supplied by db_init()
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

static int commit_transaction(db_con_t * _h)
{
	PGresult *mr;

	mr = PQexec(CON_CONNECTION(_h), "COMMIT");
	if(!mr || PQresultStatus(mr) != PGRES_COMMAND_OK)
	{
		PLOG("commit_transaction", "error");
		return -1;
	}
	PQclear(mr);
	return(0);
}

/*
 * Print list of columns separated by comma
 */
static int print_columns(char* _b, int _l, db_key_t* _c, int _n)
{
	int i;
	int res = 0;
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
static int print_values(char* _b, int _l, db_val_t* _v, int _n)
{
	int i, res = 0, l;

	for(i = 0; i < _n; i++) {
		l = _l - res;
/*		LOG(L_ERR, "%d sizes l = _l - res %d = %d - %d\n", i, l,_l,res);
*/
		if (val2str(_v + i, _b + res, &l) < 0) {
			LOG(L_ERR,
			 "print_values(): Error converting value to string\n");
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
static int print_where(char* _b, int _l, db_key_t* _k,
	db_op_t* _o, db_val_t* _v, int _n)
{
	int i;
	int res = 0;
	int l;
	for(i = 0; i < _n; i++) {
		if (_o) {
			res += snprintf(_b + res, _l - res, "%s%s",
				_k[i], _o[i]);
		} else {
			res += snprintf(_b + res, _l - res, "%s=", _k[i]);
		}
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
 * Print set clause of update SQL statement
 */
static int print_set(char* _b, int _l, db_key_t* _k,
	db_val_t* _v, int _n)
{
	int i;
	int res = 0;
	int l;
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
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: nmber of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_query(db_con_t* _h, db_key_t* _k, db_op_t* _op,
	     db_val_t* _v, db_key_t* _c, int _n, int _nc,
	     db_key_t _o, db_res_t** _r)
{
	int off, rv;
	if (!_c) {
		off = snprintf(sql_buf, SQL_BUF_LEN,
			"select * from %s ", CON_TABLE(_h));
	} else {
		off = snprintf(sql_buf, SQL_BUF_LEN, "select ");
		off += print_columns(sql_buf + off, SQL_BUF_LEN - off, _c, _nc);
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off,
			"from %s ", CON_TABLE(_h));
	}
	if (_n) {
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off, "where ");
		off += print_where(sql_buf + off, SQL_BUF_LEN - off,
			_k, _op, _v, _n);
	}
	if (_o) {
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off,
			"order by %s", _o);
	}

	if(begin_transaction(_h, sql_buf)) return(-1);
	if (submit_query(_h, sql_buf) < 0) {
		LOG(L_ERR, "db_query(): Error while submitting query\n");
		return -2;
	}
	rv = get_result(_h, _r);
	free_query(_h);
	commit_transaction(_h);
	return(rv);
}


/*
 * Execute a raw SQL query
 */
int db_raw_query(db_con_t* _h, char* _s, db_res_t** _r)
{
	int rv;

	if(begin_transaction(_h, sql_buf)) return(-1);
	if (submit_query(_h, _s) < 0) {
		LOG(L_ERR, "db_raw_query(): Error while submitting query\n");
		return -2;
	}
	rv = get_result(_h, _r);
	free_query(_h);
	commit_transaction(_h);
	return(rv);
}

/*
 * Retrieve result set
 */
int get_result(db_con_t* _h, db_res_t** _r)
{
	*_r = new_result_pg(CON_SQLURL(_h));

	if (!CON_RESULT(_h)) {
		LOG(L_ERR, "get_result(): error");
		free_result(*_r);
		*_r = 0;
		return -3;
	}

        if (convert_result(_h, *_r) < 0) {
		LOG(L_ERR, "get_result(): Error while converting result\n");
		free_result(*_r);
		*_r = 0;

		return -4;
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
int db_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	int off;
	off = snprintf(sql_buf, SQL_BUF_LEN, "insert into %s (", CON_TABLE(_h));
	off += print_columns(sql_buf + off, SQL_BUF_LEN - off, _k, _n);
	off += snprintf(sql_buf + off, SQL_BUF_LEN - off, ") values (");
	off += print_values(sql_buf + off, SQL_BUF_LEN - off, _v, _n);
	*(sql_buf + off++) = ')';
	*(sql_buf + off) = '\0';

	if(begin_transaction(_h, sql_buf)) return(-1);
	if (submit_query(_h, sql_buf) < 0) {
		LOG(L_ERR, "db_insert(): Error while inserting\n");
		return -2;
	}
	free_query(_h);
	commit_transaction(_h);
	return(0);
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
	off = snprintf(sql_buf, SQL_BUF_LEN, "delete from %s", CON_TABLE(_h));
	if (_n) {
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off, " where ");
		off += print_where(sql_buf + off, SQL_BUF_LEN - off, _k,
			_o, _v, _n);
	}
	if(begin_transaction(_h, sql_buf)) return(-1);
	if (submit_query(_h, sql_buf) < 0) {
		LOG(L_ERR, "db_delete(): Error while deleting\n");
		return -2;
	}
	free_query(_h);
	commit_transaction(_h);
	return(0);
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
	off = snprintf(sql_buf, SQL_BUF_LEN, "update %s set ", CON_TABLE(_h));
	off += print_set(sql_buf + off, SQL_BUF_LEN - off, _uk, _uv, _un);
	if (_n) {
		off += snprintf(sql_buf + off, SQL_BUF_LEN - off, " where ");
		off += print_where(sql_buf + off, SQL_BUF_LEN - off, _k,
			_o, _v, _n);
		*(sql_buf + off) = '\0';
	}

	if(begin_transaction(_h, sql_buf)) return(-1);
	if (submit_query(_h, sql_buf) < 0) {
		LOG(L_ERR, "db_update(): Error while updating\n");
		return -2;
	}
	free_query(_h);
	commit_transaction(_h);
	return(0);
}
