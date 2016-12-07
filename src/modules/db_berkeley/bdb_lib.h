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
 * Berkeley DB : Library
 *
 * \ingroup database
 */



#ifndef _BDB_LIB_H_
#define _BDB_LIB_H_

#include <time.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <db.h>

#include "../../str.h"
#include "../../lib/srdb2/db.h"
#include "../../lib/srdb2/db_fld.h"

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

#define BDB_VALUE 0
#define BDB_KEY   1

typedef enum db_fld_type bdb_type_t;

typedef struct {
	bdb_type_t type; /**< Type of the value                              */
	int nul;		/**< Means that the column in database has no value */
	int free;		/**< Means that the value should be freed */
	/** Column value structure that holds the actual data in a union.  */
	union {
		int           int_val;    /**< integer value              */
		long long     ll_val;     /**< long long value            */
		double        double_val; /**< double value               */
		time_t        time_val;   /**< unix time_t value          */
		const char*   string_val; /**< zero terminated string     */
		str           str_val;    /**< str type string value      */
		str           blob_val;   /**< binary object data         */
		unsigned int  bitmap_val; /**< Bitmap data type           */
	} val;
} bdb_val_t, *bdb_val_p;

// typedef db_val_t bdb_val_t, *bdb_val_p;

typedef struct _bdb_row
{
	bdb_val_p fields;
	struct _bdb_row *prev;
	struct _bdb_row *next;
} bdb_row_t, *bdb_row_p;

typedef struct _bdb_col
{
	str name;
	str dv;     /* default value */
	int type;
	int flag;
} bdb_col_t, *bdb_col_p;

typedef struct _bdb_table
{
	str name;
	DB *db;
	bdb_col_p colp [MAX_NUM_COLS];
	int ncols;
	int nkeys;
	int ro;       /*db readonly flag*/
	int logflags; /*flags indication what-where to journal log */
	FILE* fp;     /*jlog file pointer */
	time_t t;     /*jlog creation time */
	ino_t ino;
} bdb_table_t, *bdb_table_p;

typedef struct _bdb_tcache
{
	bdb_table_p dtp;
	struct _bdb_tcache *prev;
	struct _bdb_tcache *next;
} bdb_tcache_t, *bdb_tcache_p;

typedef struct _bdb_db
{
	str name;
	DB_ENV *dbenv;
	bdb_tcache_p tables;
} bdb_db_t, *bdb_db_p;

typedef struct _bdb_params
{
	u_int32_t cache_size;
	int auto_reload;
	int log_enable;
	int journal_roll_interval;
} bdb_params_t, *bdb_params_p;


int bdblib_init(bdb_params_p _parms);
int bdblib_destroy(void);
int bdblib_close(bdb_db_p _db_p, str* _n);
int bdblib_reopen(bdb_db_p _db_p, str* _n);
int bdblib_recover(bdb_table_p _tp, int error_code);
void bdblib_log(int op, bdb_db_p _db, bdb_table_p _tp, char* _msg, int len);
int bdblib_create_dbenv(DB_ENV **dbenv, char* home);
int bdblib_create_journal(bdb_db_p _db_p, bdb_table_p _tp);
bdb_db_p  	bdblib_get_db(str *_s);
bdb_tcache_p 	bdblib_get_table(bdb_db_t *_db, str *_s);
bdb_table_p 	bdblib_create_table(bdb_db_t *_db, str *_s);

int bdb_db_free(bdb_db_p _dbp);
int bdb_tcache_free(bdb_tcache_p _tbc);
int bdb_table_free(bdb_table_p _tp);

int load_metadata_columns(bdb_table_p _tp);
int load_metadata_keys(bdb_table_p _tp);
int load_metadata_readonly(bdb_table_p _tp);
int load_metadata_logflags(bdb_table_p _tp);
int load_metadata_defaults(bdb_table_p _tp);

int bdblib_valtochar(bdb_table_p tp, db_fld_t *fld, int fld_count, char *kout,
		int *klen, int ktype);

int bdb_is_database(char *dirpath);
int bdb_get_colpos(bdb_table_t *tp, char *name);

int bdb_str2int(char *s, int *v);
int bdb_str2double(char *s, double *v);
int bdb_str2time(char *s, time_t *v);

#endif
