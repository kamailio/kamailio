/*
 * db_berkeley module, portions of this code were templated using
 * the dbtext and postgres modules.

 * Copyright (C) 2007 Cisco Systems
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

/*! \file
 * Berkeley DB : 
 *
 * \ingroup database
 */



#ifndef _KM_BDB_LIB_H_
#define _KM_BDB_LIB_H_

#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <db.h>

#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_val.h"
#include "../../locking.h"

/*max number of columns in a table*/
#define MAX_NUM_COLS 32

/*max char width of a table row*/
#define MAX_ROW_SIZE 2048

/*max char width of a table name*/
#define MAX_TABLENAME_SIZE 64

#define METADATA_COLUMNS "METADATA_COLUMNS"
#define METADATA_KEY "METADATA_KEY"
#define METADATA_READONLY "METADATA_READONLY"
#define METADATA_LOGFLAGS "METADATA_LOGFLAGS"
#define METADATA_DEFAULTS "METADATA_DEFAULTS"

/*journal logging flag masks */
#define JLOG_NONE   0
#define JLOG_INSERT 1
#define JLOG_DELETE 2
#define JLOG_UPDATE 4
#define JLOG_FILE   8
#define JLOG_STDOUT 16
#define JLOG_SYSLOG 32

#define DELIM "|"
#define DELIM_LEN (sizeof(DELIM)-1)

typedef db_val_t bdb_val_t, *bdb_val_p;

typedef struct _row
{
	bdb_val_p fields;
	struct _row *prev;
	struct _row *next;
} row_t, *row_p;

typedef struct _column
{
	str name;
	str dv;     /* default value */
	int type;
	int flag;
} column_t, *column_p;

typedef struct _table
{
	str name;
	DB *db;
	gen_lock_t sem;
	column_p colp [MAX_NUM_COLS];
	int ncols;
	int nkeys;
	int ro;       /*db readonly flag*/
	int logflags; /*flags indication what-where to journal log */
	FILE* fp;     /*jlog file pointer */
	time_t t;     /*jlog creation time */
	ino_t ino;
} table_t, *table_p;

typedef struct _tbl_cache
{
	gen_lock_t sem;
	table_p dtp;
	struct _tbl_cache *prev;
	struct _tbl_cache *next;
} tbl_cache_t, *tbl_cache_p;

typedef struct _database
{
	str name;
	DB_ENV *dbenv;
	tbl_cache_p tables;
} database_t, *database_p;

typedef struct _db_parms
{
	u_int32_t cache_size;
	int auto_reload;
	int log_enable;
	int journal_roll_interval;
} db_parms_t, *db_parms_p;


int km_bdblib_init(db_parms_p _parms);
int km_bdblib_destroy(void);
int km_bdblib_close(char* _n);
int km_bdblib_reopen(char* _n);
int km_bdblib_recover(table_p _tp, int error_code);
void km_bdblib_log(int op, table_p _tp, char* _msg, int len);
int km_bdblib_create_dbenv(DB_ENV **dbenv, char* home);
int km_bdblib_create_journal(table_p _tp);
database_p  	km_bdblib_get_db(str *_s);
tbl_cache_p 	km_bdblib_get_table(database_p _db, str *_s);
table_p 	km_bdblib_create_table(database_p _db, str *_s);

int db_free(database_p _dbp);
int tbl_cache_free(tbl_cache_p _tbc);
int tbl_free(table_p _tp);

int km_load_metadata_columns(table_p _tp);
int km_load_metadata_keys(table_p _tp);
int km_load_metadata_readonly(table_p _tp);
int km_load_metadata_logflags(table_p _tp);
int km_load_metadata_defaults(table_p _tp);

int km_bdblib_valtochar(table_p _tp, int* _lres, char* _k, int* _klen, db_val_t* _v, int _n, int _ko);

#endif
