/*
 * $Id$
 *
 * recovery for berkeley_db module
 * 
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
 * History:
 * --------
 * 2007-09-19  genesis (wiquan)
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <db.h>

/*max number of journal files that we are reading*/
#define MAXFILES 64

/*max number of columns in a table*/
#define MAX_NUM_COLS 32

/*max char width of a table row*/
#define MAX_ROW_SIZE 2048

/*max char width of a table name*/
#define MAX_TABLENAME_SIZE 64

#define MAX_FILENAME_SIZE 512

#define METADATA_KEY "METADATA_KEY"
#define METADATA_COLUMNS "METADATA_COLUMNS"

/*operations*/
enum 
{
	INSERT,
	UPDATE,
	DELETE,
	UNKNOWN_OP
};


typedef struct _lnode
{
	char* p;
	struct _lnode *prev;
	struct _lnode *next;
} lnode_t, *lnode_p;


typedef struct _column
{
	char* name;
	char* type;
	int kflag;
} column_t, *column_p;


typedef struct _table
{
	char* name;
	column_p colp [MAX_NUM_COLS];
	int ncols;
	int nkeys;
	int ro;
	int logflags;
	DB* db;
} table_t, *table_p;


typedef struct _tbl_cache
{
	table_p dtp;
	struct _tbl_cache *prev;
	struct _tbl_cache *next;
} tbl_cache_t, *tbl_cache_p;


int usage(void);
DB* get_db(table_p tp);
int get_op(char* op, int len);
int delete(table_p tp, char* v, int len);
int insert(table_p tp, char* v, int len);
int _insert(DB* db, char* k, char* v, int klen, int vlen);
int update(table_p tp, char* v, int len);
int create(char* tn);
int _version(DB* db);
int create_all(void);
int recover(char* tn);
int recover_all(int lastn);
lnode_p file_list(char* d, char* tn);
int compare (const void *a, const void *b);
int extract_key(table_p tp, char* key, char* data);
int load_schema(char* dir);
tbl_cache_p get_table(char *s);
table_p create_table(char *_s);
int load_metadata_columns(table_p _tp, char* line);
int load_metadata_key(table_p _tp, char* line);
int import_schema(table_p tp);
void cleanup(void);
