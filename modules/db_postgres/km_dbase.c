/*
 * $Id$
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2006 Norman Brandinger
 * Copyright (C) 2008 1&1 Internet AG
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
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 * 2006-07-28 within pg_get_result(): added check to immediatly return of no 
 *            result set was returned added check to only execute 
 *            convert_result() if PGRES_TUPLES_OK added safety check to avoid 
 *            double pg_free_result() (norm)
 * 2006-08-07 Rewrote pg_get_result().
 *            Additional debugging lines have been placed through out the code.
 *            Added Asynchronous Command Processing (PQsendQuery/PQgetResult) 
 *            instead of PQexec. this was done in preparation of adding FETCH 
 *            support.  Note that PQexec returns a result pointer while 
 *            PQsendQuery does not.  The result set pointer is obtained from 
 *            a call (or multiple calls) to PQgetResult.
 *            Removed transaction processing calls (BEGIN/COMMIT/ROLLBACK) as 
 *            they added uneeded overhead.  Klaus' testing showed in excess of 
 *            1ms gain by removing each command.  In addition, Kamailio only 
 *            issues single queries and is not, at this time transaction aware.
 *            The transaction processing routines have been left in place 
 *            should this support be needed in the future.
 *            Updated logic in pg_query / pg_raw_query to accept a (0) result 
 *            set (_r) parameter.  In this case, control is returned
 *            immediately after submitting the query and no call to 
 *            pg_get_results() is performed. This is a requirement for 
 *            FETCH support. (norm)
 * 2006-10-27 Added fetch support (norm)
 *            Removed dependency on aug_* memory routines (norm)
 *            Added connection pooling support (norm)
 *            Standardized API routines to pg_* names (norm)
 * 2006-11-01 Updated pg_insert(), pg_delete(), pg_update() and 
 *            pg_get_result() to handle failed queries.  Detailed warnings 
 *            along with the text of the failed query is now displayed in the 
 *            log. Callers of these routines can now assume that a non-zero 
 *            rc indicates the query failed and that remedial action may need 
 *            to be taken. (norm)
 */

/*! \file
 *  \brief DB_POSTGRES :: Core
 *  \ingroup db_postgres
 *  Module: \ref db_postgres
 */

/*! maximum number of columns */
#define MAXCOLUMNS	512

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../lib/srdb1/db_query.h"
#include "../../locking.h"
#include "../../hashes.h"
#include "km_dbase.h"
#include "km_pg_con.h"
#include "km_val.h"
#include "km_res.h"
#include "pg_mod.h"

static gen_lock_set_t *_pg_lock_set = NULL;
static unsigned int _pg_lock_size = 0;

/*!
 * \brief init lock set used to implement SQL REPLACE via UPDATE/INSERT
 * \param sz power of two to compute the lock set size 
 * \return 0 on success, -1 on error
 */
int pg_init_lock_set(int sz)
{
	if(sz>0 && sz<=10)
	{
		_pg_lock_size = 1<<sz;
	} else {
		_pg_lock_size = 1<<4;
	}
	_pg_lock_set = lock_set_alloc(_pg_lock_size);
	if(_pg_lock_set==NULL || lock_set_init(_pg_lock_set)==NULL)
	{
		LM_ERR("cannot initiate lock set\n");
		return -1;
	}
	return 0;
}

void pg_destroy_lock_set(void)
{
	if(_pg_lock_set!=NULL)
	{
		lock_set_destroy(_pg_lock_set);
		lock_set_dealloc(_pg_lock_set);
		_pg_lock_set = NULL;
		_pg_lock_size = 0;
	}
}

static void db_postgres_free_query(const db1_con_t* _con);


/*!
 * \brief Initialize database for future queries
 * \param _url URL of the database that should be opened
 * \return database connection on success, NULL on error
 * \note this function must be called prior to any database functions
 */
db1_con_t *db_postgres_init(const str* _url)
{
	return db_do_init(_url, (void*) db_postgres_new_connection);
}

/*!
 * \brief Initialize database for future queries - no pooling
 * \param _url URL of the database that should be opened
 * \return database connection on success, NULL on error
 * \note this function must be called prior to any database functions
 */
db1_con_t *db_postgres_init2(const str* _url, db_pooling_t pooling)
{
	return db_do_init2(_url, (void*) db_postgres_new_connection, pooling);
}

/*!
 * \brief Close database when the database is no longer needed
 * \param _h closed connection, as returned from db_postgres_init
 * \note free all memory and resources
 */
void db_postgres_close(db1_con_t* _h)
{
	db_do_close(_h, db_postgres_free_connection);
}


/*!
 * \brief Submit_query, run a query
 * \param _con database connection
 * \param _s query string
 * \return 0 on success, negative on failure
 */
static int db_postgres_submit_query(const db1_con_t* _con, const str* _s)
{
	char *s=NULL;
	int i, retries;
	ExecStatusType pqresult;
	PGresult *res = NULL;
	int sock, ret;
	fd_set fds;
	time_t max_time;
	struct timeval wait_time;

	if(! _con || !_s || !_s->s)
	{
		LM_ERR("invalid parameter value\n");
		return(-1);
	}

	/* this bit of nonsense in case our connection get screwed up */
	switch(PQstatus(CON_CONNECTION(_con)))
	{
		case CONNECTION_OK: 
			break;
		case CONNECTION_BAD:
			LM_DBG("connection reset\n");
			PQreset(CON_CONNECTION(_con));
			break;
		case CONNECTION_STARTED:
		case CONNECTION_MADE:
		case CONNECTION_AWAITING_RESPONSE:
		case CONNECTION_AUTH_OK:
		case CONNECTION_SETENV:
		case CONNECTION_SSL_STARTUP:
		case CONNECTION_NEEDED:
		default:
			LM_ERR("%p PQstatus(%s) invalid: %.*s\n", _con,
				PQerrorMessage(CON_CONNECTION(_con)), _s->len, _s->s);
			return -1;
	}

	if (CON_TRANSACTION(_con) == 1)
		retries = 0;
	else
		retries = pg_retries;

	s = pkg_malloc((_s->len+1)*sizeof(char));
	if (s==NULL)
	{
		LM_ERR("%p db_postgres_submit_query Out of Memory: Query: %.*s\n", _con, _s->len, _s->s);
		return -1;
	}

	memcpy( s, _s->s, _s->len );
	s[_s->len] = '\0';

	for(i = 0; i <= retries; i++) {
		/* free any previous query that is laying about */
		db_postgres_free_query(_con);
		/* exec the query */

		if (PQsendQuery(CON_CONNECTION(_con), s)) {
			if (pg_timeout <= 0)
				goto do_read;

			max_time = time(NULL) + pg_timeout;

			while (1) {
				sock = PQsocket(CON_CONNECTION(_con));
				FD_ZERO(&fds);
				FD_SET(sock, &fds);

				wait_time.tv_usec = 0;
				wait_time.tv_sec = max_time - time(NULL);
				if (wait_time.tv_sec <= 0 || wait_time.tv_sec > 0xffffff)
					goto timeout;

				ret = select(sock + 1, &fds, NULL, NULL, &wait_time);
				if (ret < 0) {
					if (errno == EINTR)
						continue;
					LM_WARN("select() error\n");
					goto reset;
				}
				if (!ret) {
timeout:
					LM_WARN("timeout waiting for postgres reply\n");
					goto reset;
				}

				if (!PQconsumeInput(CON_CONNECTION(_con))) {
					LM_WARN("error reading data from postgres server: %s\n",
							PQerrorMessage(CON_CONNECTION(_con)));
					goto reset;
				}
				if (!PQisBusy(CON_CONNECTION(_con)))
					break;
			}

do_read:
			/* Get the result of the query */
			while ((res = PQgetResult(CON_CONNECTION(_con))) != NULL) {
				db_postgres_free_query(_con);
				CON_RESULT(_con) = res;
			}
			pqresult = PQresultStatus(CON_RESULT(_con));
			if((pqresult!=PGRES_FATAL_ERROR)
					&& (PQstatus(CON_CONNECTION(_con))==CONNECTION_OK))
			{
				LM_DBG("sending query ok: %p (%d) - [%.*s]\n",
						_con, pqresult, _s->len, _s->s);
				pkg_free(s);
				return 0;
			}
			LM_WARN("postgres result check failed with code %d (%s)\n",
					pqresult, PQresStatus(pqresult));
		}
		LM_WARN("postgres query command failed, connection status %d,"
				" error [%s]\n", PQstatus(CON_CONNECTION(_con)),
				PQerrorMessage(CON_CONNECTION(_con)));
		if(PQstatus(CON_CONNECTION(_con))!=CONNECTION_OK)
		{
reset:
			LM_DBG("reseting the connection to postgress server\n");
			PQreset(CON_CONNECTION(_con));
		}
	}
	LM_ERR("%p PQsendQuery Error: %s Query: %.*s\n", _con,
	PQerrorMessage(CON_CONNECTION(_con)), _s->len, _s->s);
	pkg_free(s);
	return -1;
}


/*!
 * \brief Gets a partial result set, fetch rows from a result
 *
 * Gets a partial result set, fetch a number of rows from a database result.
 * This function initialize the given result structure on the first run, and
 * fetches the nrows number of rows. On subsequenting runs, it uses the
 * existing result and fetches more rows, until it reaches the end of the
 * result set. Because of this the result needs to be null in the first
 * invocation of the function. If the number of wanted rows is zero, the
 * function returns anything with a result of zero.
 * \param _con database connection
 * \param _res result set
 * \param nrows number of fetches rows
 * \return 0 on success, negative on failure
 */
int db_postgres_fetch_result(const db1_con_t* _con, db1_res_t** _res, const int nrows)
{
	int rows;
	ExecStatusType pqresult;

	if (!_con || !_res || nrows < 0) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* exit if the fetch count is zero */
	if (nrows == 0) {
		if (*_res)
			db_free_result(*_res);

		*_res = 0;
		return 0;
	}

	if (*_res == NULL) {
		/* Allocate a new result structure */
		*_res = db_new_result();

		pqresult = PQresultStatus(CON_RESULT(_con));
		LM_DBG("%p PQresultStatus(%s) PQgetResult(%p)\n", _con,
			PQresStatus(pqresult), CON_RESULT(_con));

		switch(pqresult) {
			case PGRES_COMMAND_OK:
				/* Successful completion of a command returning no data
				 * (such as INSERT or UPDATE). */
				return 0;

			case PGRES_TUPLES_OK:
				/* Successful completion of a command returning data
				 * (such as a SELECT or SHOW). */
				if (db_postgres_get_columns(_con, *_res) < 0) {
					LM_ERR("failed to get column names\n");
					return -2;
				}
				break;

			case PGRES_FATAL_ERROR:
				LM_ERR("%p - invalid query, execution aborted\n", _con);
				LM_ERR("%p - PQresultStatus(%s)\n", _con,
						PQresStatus(pqresult));
				LM_ERR("%p: %s\n", _con,
						PQresultErrorMessage(CON_RESULT(_con)));
				if (*_res)
					db_free_result(*_res);
				*_res = 0;
				return -3;

			case PGRES_EMPTY_QUERY:
			/* notice or warning */
			case PGRES_NONFATAL_ERROR:
			/* status for COPY command, not used */
			case PGRES_COPY_OUT:
			case PGRES_COPY_IN:
			/* unexpected response */
			case PGRES_BAD_RESPONSE:
			default:
				LM_ERR("%p - probable invalid query\n", _con);
				LM_ERR("%p - PQresultStatus(%s)\n", _con, PQresStatus(pqresult));
				LM_ERR("%p: %s\n", _con, PQresultErrorMessage(CON_RESULT(_con)));
				if (*_res)
					db_free_result(*_res);
				*_res = 0;
				return -4;
		}

	} else {
		if(RES_ROWS(*_res) != NULL) {
			db_free_rows(*_res);
		}
		RES_ROWS(*_res) = 0;
		RES_ROW_N(*_res) = 0;
	}

	/* Get the number of rows (tuples) in the query result. */
	RES_NUM_ROWS(*_res) = PQntuples(CON_RESULT(_con));

	/* determine the number of rows remaining to be processed */
	rows = RES_NUM_ROWS(*_res) - RES_LAST_ROW(*_res);

	/* If there aren't any more rows left to process, exit */
	if (rows <= 0)
		return 0;

	/* if the fetch count is less than the remaining rows to process                 */
	/* set the number of rows to process (during this call) equal to the fetch count */
	if (nrows < rows)
		rows = nrows;

	RES_ROW_N(*_res) = rows;

	LM_DBG("converting row %d of %d count %d\n", RES_LAST_ROW(*_res),
			RES_NUM_ROWS(*_res), RES_ROW_N(*_res));

	if (db_postgres_convert_rows(_con, *_res) < 0) {
		LM_ERR("failed to convert rows\n");
		if (*_res)
			db_free_result(*_res);

		*_res = 0;
		return -3;
	}

	/* update the total number of rows processed */
	RES_LAST_ROW(*_res) += rows;
	return 0;
}


/*!
 * \brief Free database and any old query results
 * \param _con database connection
 */
static void db_postgres_free_query(const db1_con_t* _con)
{
	if(CON_RESULT(_con))
	{
		LM_DBG("PQclear(%p) result set\n", CON_RESULT(_con));
		PQclear(CON_RESULT(_con));
		CON_RESULT(_con) = 0;
	}
}


/*!
 * \brief Free the query and the result memory in the core
 * \param _con database connection
 * \param _r result set
 * \return 0 on success, -1 on failure
 */
int db_postgres_free_result(db1_con_t* _con, db1_res_t* _r)
{
     if ((!_con) || (!_r)) {
	     LM_ERR("invalid parameter value\n");
	     return -1;
     }
     if (db_free_result(_r) < 0) {
	     LM_ERR("unable to free result structure\n");
	     return -1;
     }
	db_postgres_free_query(_con);
	return 0;
}


/*!
 * \brief Query table for specified rows
 * \param _h structure representing database connection
 * \param _k key names
 * \param _op operators
 * \param _v values of the keys that must match
 * \param _c column names to return
 * \param _n nmber of key=values pairs to compare
 * \param _nc number of columns to return
 * \param _o order by the specified column
 * \param _r result set
 * \return 0 on success, negative on failure
 */
int db_postgres_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	     const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	     const db_key_t _o, db1_res_t** _r)
{
	return db_do_query(_h, _k, _op, _v, _c, _n, _nc, _o, _r, db_postgres_val2str,
		db_postgres_submit_query, db_postgres_store_result);
}


/*!
 * \brief Query table for specified rows and lock them
 * \param _h structure representing database connection
 * \param _k key names
 * \param _op operators
 * \param _v values of the keys that must match
 * \param _c column names to return
 * \param _n nmber of key=values pairs to compare
 * \param _nc number of columns to return
 * \param _o order by the specified column
 * \param _r result set
 * \return 0 on success, negative on failure
 */
int db_postgres_query_lock(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	     const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	     const db_key_t _o, db1_res_t** _r)
{
	if (CON_TRANSACTION(_h) == 0)
	{
		LM_ERR("transaction not in progress\n");
		return -1;
	}
	return db_do_query_lock(_h, _k, _op, _v, _c, _n, _nc, _o, _r, db_postgres_val2str,
		db_postgres_submit_query, db_postgres_store_result);
}


/*!
 * Execute a raw SQL query
 * \param _h database connection
 * \param _s raw query string
 * \param _r result set
 * \return 0 on success, negative on failure
 */
int db_postgres_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	return db_do_raw_query(_h, _s, _r, db_postgres_submit_query,
		db_postgres_store_result);
}


/*!
 * \brief Retrieve result set
 * \param _con structure representing the database connection
 * \param _r pointer to a structure represending the result set
 * \return 0 If the status of the last command produced a result set and,
 *   If the result set contains data or the convert_result() routine
 *   completed successfully. Negative if the status of the last command was
 * not handled or if the convert_result() returned an error.
 * \note A new result structure is allocated on every call to this routine.
 * If this routine returns 0, it is the callers responsbility to free the
 * result structure. If this routine returns < 0, then the result structure
 * is freed before returning to the caller.
 */
int db_postgres_store_result(const db1_con_t* _con, db1_res_t** _r)
{
	ExecStatusType pqresult;
	int rc = 0;

	*_r = db_new_result();
	if (*_r==NULL) {
		LM_ERR("failed to init new result\n");
		rc = -1;
		goto done;
	}

	pqresult = PQresultStatus(CON_RESULT(_con));
	
	LM_DBG("%p PQresultStatus(%s) PQgetResult(%p)\n", _con,
		PQresStatus(pqresult), CON_RESULT(_con));

	CON_AFFECTED(_con) = 0;

	switch(pqresult) {
		case PGRES_COMMAND_OK:
		/* Successful completion of a command returning no data
		 * (such as INSERT or UPDATE). */
		rc = 0;
		CON_AFFECTED(_con) = atoi(PQcmdTuples(CON_RESULT(_con)));
		break;

		case PGRES_TUPLES_OK:
			/* Successful completion of a command returning data
			 * (such as a SELECT or SHOW). */
			if (db_postgres_convert_result(_con, *_r) < 0) {
				LM_ERR("error while converting result\n");
				LM_DBG("freeing result set at %p\n", _r);
				pkg_free(*_r);
				*_r = 0;
				rc = -4;
				break;
			}
			rc =  0;
			CON_AFFECTED(_con) = atoi(PQcmdTuples(CON_RESULT(_con)));
			break;
		/* query failed */
		case PGRES_FATAL_ERROR:
			LM_ERR("invalid query, execution aborted\n");
			LM_ERR("driver error: %s, %s\n", PQresStatus(pqresult), PQresultErrorMessage(CON_RESULT(_con)));
			db_free_result(*_r);
			*_r = 0;
			rc = -3;
			break;

		case PGRES_EMPTY_QUERY:
		/* notice or warning */
		case PGRES_NONFATAL_ERROR:
		/* status for COPY command, not used */
		case PGRES_COPY_OUT:
		case PGRES_COPY_IN:
		/* unexpected response */
		case PGRES_BAD_RESPONSE:
		default:
			LM_ERR("probable invalid query, execution aborted\n");
			LM_ERR("driver message: %s, %s\n", PQresStatus(pqresult), PQresultErrorMessage(CON_RESULT(_con)));
			db_free_result(*_r);
			*_r = 0;
			rc = -4;
			break;
	}

done:
	db_postgres_free_query(_con);
	return (rc);
}


/*!
 * \brief Insert a row into specified table
 * \param _h structure representing database connection
 * \param _k key names
 * \param _v values of the keys
 * \param _n number of key=value pairs
 * \return 0 on success, negative on failure
 */
int db_postgres_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		const int _n)
{
	db1_res_t* _r = NULL;

	int ret = db_do_insert(_h, _k, _v, _n, db_postgres_val2str, db_postgres_submit_query);
	// finish the async query, otherwise the next query will not complete
	int tmp = db_postgres_store_result(_h, &_r);

	if (tmp < 0) {
		LM_WARN("unexpected result returned");
		ret = tmp;
	}
	
	if (_r)
		db_free_result(_r);

	return ret;
}


/*!
 * \brief Delete a row from the specified table
 * \param _h structure representing database connection
 * \param _k key names
 * \param _o operators
 * \param _v values of the keys that must match
 * \param _n number of key=value pairs
 * \return 0 on success, negative on failure
 */
int db_postgres_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const int _n)
{
	db1_res_t* _r = NULL;
	int ret = db_do_delete(_h, _k, _o, _v, _n, db_postgres_val2str,
		db_postgres_submit_query);
	int tmp = db_postgres_store_result(_h, &_r);

	if (tmp < 0) {
		LM_WARN("unexpected result returned");
		ret = tmp;
	}

	if (_r)
		db_free_result(_r);

	return ret;
}


/*!
 * Update some rows in the specified table
 * \param _h structure representing database connection
 * \param _k key names
 * \param _o operators
 * \param _v values of the keys that must match
 * \param _uk updated columns
 * \param _uv updated values of the columns
 * \param _n number of key=value pairs
 * \param _un number of columns to update
 * \return 0 on success, negative on failure
 */
int db_postgres_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n,
		const int _un)
{
	db1_res_t* _r = NULL;
	int ret = db_do_update(_h, _k, _o, _v, _uk, _uv, _n, _un, db_postgres_val2str,
		db_postgres_submit_query);
	int tmp = db_postgres_store_result(_h, &_r);

	if (tmp < 0) {
		LM_WARN("unexpected result returned");
		ret = tmp;
	}
	
	if (_r)
		db_free_result(_r);

	return ret;
}

/**
 * Returns the affected rows of the last query.
 * \param _h database handle
 * \return returns the affected rows as integer or -1 on error.
 */
int db_postgres_affected_rows(const db1_con_t* _h)
{
	if (!_h) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	return CON_AFFECTED(_h);
}

/**
 * Starts a single transaction that will consist of one or more queries (SQL BEGIN)
 * \param _h database handle
 * \return 0 on success, negative on failure
 */
int db_postgres_start_transaction(db1_con_t* _h, db_locking_t _l)
{
	db1_res_t *res = NULL;
	str begin_str = str_init("BEGIN");
	str lock_start_str = str_init("LOCK TABLE ");
	str lock_write_end_str = str_init(" IN EXCLUSIVE MODE");
	str lock_full_end_str = str_init(" IN ACCESS EXCLUSIVE MODE");
	str *lock_end_str = &lock_write_end_str;
	str lock_str = {0, 0};
	
	if (!_h) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (CON_TRANSACTION(_h) == 1) {
		LM_ERR("transaction already started\n");
		return -1;
	}

	if (db_postgres_raw_query(_h, &begin_str, &res) < 0)
	{
		LM_ERR("executing raw_query\n");
		return -1;
	}

	if (res) db_postgres_free_result(_h, res);

	CON_TRANSACTION(_h) = 1;

	switch(_l)
	{
	case DB_LOCKING_NONE:
		break;
	case DB_LOCKING_FULL:
		lock_end_str = &lock_full_end_str;
		/* Fall-thru */
	case DB_LOCKING_WRITE:
		if ((lock_str.s = pkg_malloc((lock_start_str.len + CON_TABLE(_h)->len + lock_end_str->len) * sizeof(char))) == NULL)
		{
			LM_ERR("allocating pkg memory\n");
			goto error;
		}

		memcpy(lock_str.s, lock_start_str.s, lock_start_str.len);
		lock_str.len += lock_start_str.len;
		memcpy(lock_str.s + lock_str.len, CON_TABLE(_h)->s, CON_TABLE(_h)->len);
		lock_str.len += CON_TABLE(_h)->len;
		memcpy(lock_str.s + lock_str.len, lock_end_str->s, lock_end_str->len);
		lock_str.len += lock_end_str->len;

		if (db_postgres_raw_query(_h, &lock_str, &res) < 0)
		{
			LM_ERR("executing raw_query\n");
			goto error;
		}

		if (res) db_postgres_free_result(_h, res);
		if (lock_str.s) pkg_free(lock_str.s);
		break;

	default:
		LM_WARN("unrecognised lock type\n");
		goto error;
	}

	return 0;

error:
	if (lock_str.s) pkg_free(lock_str.s);
	db_postgres_abort_transaction(_h);
	return -1;
}

/**
 * Ends a transaction and commits the changes (SQL COMMIT)
 * \param _h database handle
 * \return 0 on success, negative on failure
 */
int db_postgres_end_transaction(db1_con_t* _h)
{
	db1_res_t *res = NULL;
	str query_str = str_init("COMMIT");
	
	if (!_h) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (CON_TRANSACTION(_h) == 0) {
		LM_ERR("transaction not in progress\n");
		return -1;
	}

	if (db_postgres_raw_query(_h, &query_str, &res) < 0)
	{
		LM_ERR("executing raw_query\n");
		return -1;
	}

	if (res) db_postgres_free_result(_h, res);

	/* Only _end_ the transaction after the raw_query.  That way, if the
 	   raw_query fails, and the calling module does an abort_transaction()
	   to clean-up, a ROLLBACK will be sent to the DB. */
	CON_TRANSACTION(_h) = 0;
	return 0;
}

/**
 * Ends a transaction and rollsback the changes (SQL ROLLBACK)
 * \param _h database handle
 * \return 1 if there was something to rollback, 0 if not, negative on failure
 */
int db_postgres_abort_transaction(db1_con_t* _h)
{
	db1_res_t *res = NULL;
	str query_str = str_init("ROLLBACK");
	
	if (!_h) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (CON_TRANSACTION(_h) == 0) {
		LM_DBG("nothing to rollback\n");
		return 0;
	}

	/* Whether the rollback succeeds or not we need to _end_ the
 	   transaction now or all future starts will fail */
	CON_TRANSACTION(_h) = 0;

	if (db_postgres_raw_query(_h, &query_str, &res) < 0)
	{
		LM_ERR("executing raw_query\n");
		return -1;
	}

	if (res) db_postgres_free_result(_h, res);

	return 1;
}

/*!
 * Store name of table that will be used by subsequent database functions
 * \param _con database connection
 * \param _t table name
 * \return 0 on success, negative on error
 */
int db_postgres_use_table(db1_con_t* _con, const str* _t)
{
	return db_use_table(_con, _t);
}


/*!
 * \brief SQL REPLACE implementation
 * \param _h structure representing database connection
 * \param _k key names
 * \param _v values of the keys
 * \param _n number of key=value pairs
 * \param _un number of keys to build the unique key, starting from first
 * \param _m mode - first update, then insert, or first insert, then update
 * \return 0 on success, negative on failure
 */
int db_postgres_replace(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const int _n, const int _un, const int _m)
{
	unsigned int pos = 0;
	int i;

	if(_un > _n)
	{
		LM_ERR("number of columns for unique key is too high\n");
		return -1;
	}

	if(_un > 0)
	{
		for(i=0; i<_un; i++)
		{
			if(!VAL_NULL(&_v[i]))
			{
				switch(VAL_TYPE(&_v[i]))
				{
					case DB1_INT:
						pos += VAL_UINT(&_v[i]);
						break;
					case DB1_STR:
						pos += get_hash1_raw((VAL_STR(&_v[i])).s,
									(VAL_STR(&_v[i])).len);
						break;
					case DB1_STRING:
						pos += get_hash1_raw(VAL_STRING(&_v[i]),
									strlen(VAL_STRING(&_v[i])));
						break;
					default:
						break;
				}
			}
		}
		pos &= (_pg_lock_size-1);
		lock_set_get(_pg_lock_set, pos);
		if(db_postgres_update(_h, _k, 0, _v, _k + _un,
						_v + _un, _un, _n -_un)< 0)
		{
			LM_ERR("update failed\n");
			lock_set_release(_pg_lock_set, pos);
			return -1;
		}

		if (db_postgres_affected_rows(_h) <= 0)
		{
			if(db_postgres_insert(_h, _k, _v, _n)< 0)
			{
				LM_ERR("insert failed\n");
				lock_set_release(_pg_lock_set, pos);
				return -1;
			}
			LM_DBG("inserted new record in database table\n");
		} else {
			LM_DBG("updated record in database table\n");
		}
		lock_set_release(_pg_lock_set, pos);
	} else {
		if(db_postgres_insert(_h, _k, _v, _n)< 0)
		{
			LM_ERR("direct insert failed\n");
			return -1;
		}
		LM_DBG("directly inserted new record in database table\n");
	}
	return 0;
}

