/*
 * $Id$
 *
 * Copyright (C) 2012 1&1 Internet Development
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
 * 2012-01 initial version (Anca Vamanu)
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../hashes.h"
#include "../../lock_ops.h"

#include "dbcassa_table.h"

#define DBCASSA_TABLE_SIZE 16

typedef struct  rw_lock {
	gen_lock_t lock;
	int reload_flag;
	int data_refcnt;
} rw_lock_t;

typedef struct _dbcassa_tbl_htable
{
	rw_lock_t lock;
	dbcassa_table_p dtp;
} dbcassa_tbl_htable_t, *dbcassa_tbl_htable_p;

static dbcassa_tbl_htable_p dbcassa_tbl_htable = NULL;
extern str dbcassa_schema_path;
static char full_path_buf[_POSIX_PATH_MAX + 1];

/**
 * Check if file modified from last read
 * -1 - error
 *  0 - no change
 *  1 - changed
 */
int dbcassa_check_mtime(time_t *mt)
{
	struct stat s;

	if(stat(full_path_buf, &s) == 0)
	{
		if((int)s.st_mtime > (int)*mt)
		{
			*mt = s.st_mtime;
			LM_DBG("[%s] was updated\n", full_path_buf);
			return 1;
		}
	} else {
		LM_DBG("stat failed on [%s]\n", full_path_buf);
		return -1;
	}
	return 0;
}

/*
 *	Create new table structure
 *
 * */

dbcassa_table_p dbcassa_table_new(const str *_tbname, const str *_dbname)
{
	struct stat s;
	dbcassa_table_p dtp = NULL;
	int size;

	if(!_tbname || !_dbname) {
		LM_ERR("Invalid parameters\n");
		return 0;
	}

	size = sizeof(dbcassa_table_t)+_tbname->len+_dbname->len;
	dtp = (dbcassa_table_p)shm_malloc(size);
	if(!dtp) {
		LM_ERR("No more shared memory\n");
		return 0;
	}

	memset(dtp, 0, size);
	size = sizeof(dbcassa_table_t);
	dtp->name.s = (char*)dtp + size;
	memcpy(dtp->name.s, _tbname->s, _tbname->len);
	dtp->name.len = _tbname->len;
	size+= _tbname->len;

	dtp->dbname.s = (char*)dtp + size;
	memcpy(dtp->dbname.s, _dbname->s, _dbname->len);
	dtp->dbname.len = _dbname->len;

	if(stat(full_path_buf, &s) == 0) {
		dtp->mt = s.st_mtime;
		LM_DBG("mtime is %d\n", (int)s.st_mtime);
	}

	return dtp;
}

dbcassa_column_p dbcassa_column_new(char *_s, int _l)
{
	dbcassa_column_p dcp;
	int size;

	size = sizeof(dbcassa_column_t) + _l+ 1;
	dcp = (dbcassa_column_p)shm_malloc(size);
	if(!dcp) {
		LM_ERR("No more shared memory\n");
		return 0;
	}
	memset(dcp, 0, size);
	dcp->name.s = (char*)dcp + sizeof(dbcassa_column_t);
	memcpy(dcp->name.s, _s, _l);
	dcp->name.len = _l;
	dcp->name.s[_l] = '\0';

	return dcp;
}

int dbcassa_column_free(dbcassa_column_p dcp)
{
	if(!dcp)
		return -1;
	shm_free(dcp);
	return 0;
}

int dbcassa_table_free(dbcassa_table_p _dtp)
{
	dbcassa_column_p _cp, _cp0;
	
	if(!_dtp)
		return -1;

	/* cols*/
	_cp = _dtp->cols;
	while(_cp) {
		_cp0=_cp;
		_cp=_cp->next;
		dbcassa_column_free(_cp0);
	}
	/* key */
	if(_dtp->key)
		shm_free(_dtp->key);
	if(_dtp->sec_key)
		shm_free(_dtp->sec_key);

	shm_free(_dtp);

	return 0;
}

/**
 * Load the table schema from file
 */
dbcassa_table_p dbcassa_load_file(str* dbn, str* tbn)
{
#define KEY_MAX_LEN 10
	FILE *fin=NULL;
	char buf[4096];
	int c, crow, ccol, bp, max_auto;
	dbcassa_table_p dtp = 0;
	dbcassa_column_p colp= 0;
	dbcassa_column_p key[KEY_MAX_LEN];
	dbcassa_column_p sec_key[KEY_MAX_LEN];

	enum {DBCASSA_FLINE_ST, DBCASSA_NLINE_ST, DBCASSA_NLINE2_ST} state;

	memset(key, 0, KEY_MAX_LEN*sizeof(dbcassa_column_p));
	memset(sec_key, 0, KEY_MAX_LEN*sizeof(dbcassa_column_p));

	LM_DBG("loading file [%s]\n", full_path_buf);
	fin = fopen(full_path_buf, "rt");
	if(!fin) {
		LM_ERR("Failed to open file\n");
		return 0;
	}

	dtp = dbcassa_table_new(tbn, dbn);
	if(!dtp)
		goto done;

	state = DBCASSA_FLINE_ST;
	crow = ccol = -1;
	c = fgetc(fin);
	max_auto = 0;
	while(c!=EOF) {
		switch(state) {
			case DBCASSA_FLINE_ST:
				bp = 0;
				while(c==DBCASSA_DELIM_C)
					c = fgetc(fin);
				if(c==DBCASSA_DELIM_R && !dtp->cols)
					goto clean;
				if(c==DBCASSA_DELIM_R) {
					if(dtp->nrcols <= 0)
						goto clean;
					
					state = DBCASSA_NLINE_ST;
					c = fgetc(fin);
					break;
				}
				while(c!=DBCASSA_DELIM_C && c!='(' && c!=DBCASSA_DELIM_R) {
					if(c==EOF)
						goto clean;
					buf[bp++] = c;
					c = fgetc(fin);
				}
				colp = dbcassa_column_new(buf, bp);
				if(!colp)
					goto clean;
				LM_DBG("new col [%.*s]\n", bp, buf);
				while(c==DBCASSA_DELIM_C)
					c = fgetc(fin);
				if(c!='(')
					goto clean;
				c = fgetc(fin);
				while(c==DBCASSA_DELIM_C)
					c = fgetc(fin);

				switch(c) {
					case 's':
					case 'S':
						colp->type = DB1_STR;
						LM_DBG("column[%d] is STR!\n", ccol+1);
					break;
					case 'i':
					case 'I':
						colp->type = DB1_INT;
						LM_DBG("column[%d] is INT!\n", ccol+1);
					break;
					case 'd':
					case 'D':
						colp->type = DB1_DOUBLE;
						LM_DBG("column[%d] is DOUBLE!\n", ccol+1);
					break;
					case 't':
					case 'T':
						colp->type = DB1_DATETIME;
						LM_DBG("column[%d] is TIME! Timestamp col has name [%s]\n", ccol+1, colp->name.s);
						if(dtp->ts_col) {
							LM_ERR("You can have only one column with type timestamp\n");
							goto clean;
						}
						dtp->ts_col = colp;
					break;
					default:
						LM_DBG("wrong column type!\n");
						goto clean;
				}

				while(c!='\n' && c!=EOF && c!=')' && c!= ',') {
					if(colp->type == DB1_STR && (c=='i'|| c=='I')) {
						colp->type = DB1_STRING;
						LM_DBG("column[%d] is actually STRING!\n", ccol+1);
					}
					c = fgetc(fin);
				}

				if(c == ')') {
					//LM_DBG("c=%c!\n", c);
					colp->next = dtp->cols;
					dtp->cols = colp;
					dtp->nrcols++;
					c = fgetc(fin);
				}
				else
					goto clean;
				ccol++;
			break;

			case DBCASSA_NLINE_ST:
			case DBCASSA_NLINE2_ST:
				// unique key
				while(c==DBCASSA_DELIM_C)
					c = fgetc(fin);
				if(c == DBCASSA_DELIM_R) {
					state = DBCASSA_NLINE2_ST;
					c = fgetc(fin);
					break;
				}

				if(c == EOF)
					break;
				bp= 0;
				while(c!=DBCASSA_DELIM_C && c!=DBCASSA_DELIM_R)
				{
					if(c==EOF)
						break;
					buf[bp++] = c;
					c = fgetc(fin);
				}
				colp = dtp->cols;
				while(colp) {
					if(bp==colp->name.len && strncmp(colp->name.s, buf, bp)==0) {
						if(state == DBCASSA_NLINE_ST)
							key[dtp->key_len++] = colp;
						else
							sec_key[dtp->seckey_len++] = colp;
						break;
					}
					colp = colp->next;
				}
				if(!colp) {
					LM_ERR("Undefined column in key [%.*s]\n", bp, buf);
					goto clean;
				}
				break;
		}
	}

	/* copy the keys into the table */
	if(dtp->key_len) {
		dtp->key = (dbcassa_column_p*)
				shm_malloc(dtp->key_len*sizeof(dbcassa_column_p));
		if(!dtp->key) {
			LM_ERR("No more share memory\n");
			goto clean;
		}
		for(ccol = 0; ccol< dtp->key_len; ccol++) {
			dtp->key[ccol] = key[ccol];
			LM_DBG("col [%.*s] in primary key\n", key[ccol]->name.len, key[ccol]->name.s);
		}
	}
	if(dtp->seckey_len) {
		dtp->sec_key = (dbcassa_column_p*)
				shm_malloc(dtp->seckey_len*sizeof(dbcassa_column_p));
		if(!dtp->sec_key) {
			LM_ERR("No more share memory\n");
			goto clean;
		}
		for(ccol = 0; ccol< dtp->seckey_len; ccol++) {
			dtp->sec_key[ccol] = sec_key[ccol];
			LM_DBG("col [%.*s] in secondary key\n", sec_key[ccol]->name.len, sec_key[ccol]->name.s);
		}
	}

done:
	if(fin)
		fclose(fin);
	return dtp;
clean:
	if(fin)
		fclose(fin);
	if(dtp)
		dbcassa_table_free(dtp);
	return NULL;
}


#define ref_read_data(rw_lock) \
do {\
	again:\
	lock_get( &rw_lock.lock ); \
	if (rw_lock.reload_flag) { \
		lock_release( &rw_lock.lock ); \
		usleep(5); \
		goto again; \
	} \
	rw_lock.data_refcnt++; \
	lock_release( &rw_lock.lock ); \
} while(0)


#define unref_read_data(rw_lock) \
do {\
	lock_get( &rw_lock.lock ); \
	rw_lock.data_refcnt--; \
	lock_release( &rw_lock.lock ); \
} while(0)


#define ref_write_data(rw_lock)\
do {\
	lock_get( &rw_lock.lock ); \
	rw_lock.reload_flag = 1; \
	lock_release( &rw_lock.lock ); \
	while (rw_lock.data_refcnt) \
		usleep(10); \
} while(0)


#define unref_write_data(rw_lock)\
	rw_lock.reload_flag = 0;

/*
 *	Search the table schema
 * */
dbcassa_table_p dbcassa_db_search_table(int hashidx, int hash,
		const str* dbn, const str *tbn)
{
	dbcassa_table_p tbc = NULL;
	ref_read_data(dbcassa_tbl_htable[hashidx].lock);

	tbc = dbcassa_tbl_htable[hashidx].dtp;
	while(tbc) {
		LM_DBG("found dbname=%.*s, table=%.*s\n", tbc->dbname.len, tbc->dbname.s, tbc->name.len, tbc->name.s);
		if(tbc->hash==hash && tbc->dbname.len == dbn->len
			&& tbc->name.len == tbn->len
			&& !strncasecmp(tbc->dbname.s, dbn->s, dbn->len)
			&& !strncasecmp(tbc->name.s, tbn->s, tbn->len))
			return tbc;
		tbc = tbc->next;
	}
	unref_read_data(dbcassa_tbl_htable[hashidx].lock);
	return NULL;
}


/**
 * Get the table schema. If the file was updated, update the table schema.
 */
dbcassa_table_p dbcassa_db_get_table(const str* dbn, const str *tbn)
{
	dbcassa_table_p tbc = NULL, old_tbc= NULL, new_tbc= NULL, prev_tbc= NULL;
	int hash;
	int hashidx;
	int len;

	if(!dbn || !tbn ) {
		LM_ERR("invalid parameter");
		return NULL;
	}

	hash = core_hash(dbn, tbn, DBCASSA_TABLE_SIZE);
	hashidx = hash % DBCASSA_TABLE_SIZE;

	ref_read_data(dbcassa_tbl_htable[hashidx].lock);

	tbc = dbcassa_tbl_htable[hashidx].dtp;

	while(tbc) {
		LM_DBG("found dbname=%.*s, table=%.*s\n", tbc->dbname.len, tbc->dbname.s, tbc->name.len, tbc->name.s);
		if(tbc->hash==hash && tbc->dbname.len == dbn->len
				&& tbc->name.len == tbn->len
				&& !strncasecmp(tbc->dbname.s, dbn->s, dbn->len)
				&& !strncasecmp(tbc->name.s, tbn->s, tbn->len)) {

			memcpy(full_path_buf + dbcassa_schema_path.len, dbn->s, dbn->len);
			len = dbcassa_schema_path.len + dbn->len;
			full_path_buf[len++] = '/';
			memcpy(full_path_buf + len, tbn->s, tbn->len);
			full_path_buf[len + tbn->len] = '\0';

			if(dbcassa_check_mtime(&tbc->mt) == 0)
				return tbc;
			old_tbc = tbc;
			break;
		}
		tbc = tbc->next;
	}
	unref_read_data(dbcassa_tbl_htable[hashidx].lock);
	if(!old_tbc)
		return NULL;

	/* the file has changed - load again the schema */
	new_tbc = dbcassa_load_file((str*)dbn, (str*)tbn);
	if(!new_tbc)
	{
		LM_ERR("could not load database from file [%.*s]\n", tbn->len, tbn->s);
		return NULL;
	}
	new_tbc->hash = hashidx;

	/* lock for write */
	ref_write_data(dbcassa_tbl_htable[hashidx].lock);
	tbc = dbcassa_tbl_htable[hashidx].dtp;

	while(tbc) {
		if(tbc == old_tbc)
			break;
		prev_tbc = tbc;
		tbc = tbc->next;
	}

	/* somebody else might have rewritten it in the mean time? just return the existing one */
	if(!tbc) {
		unref_write_data(dbcassa_tbl_htable[hashidx].lock);
		return dbcassa_db_search_table(hashidx, hash, dbn, tbn);
	}

	/* replace the table */
	new_tbc->next = old_tbc->next;
	if(prev_tbc)
		prev_tbc->next = new_tbc;
	else
		dbcassa_tbl_htable[hashidx].dtp = new_tbc;
	dbcassa_table_free(old_tbc);
	unref_write_data(dbcassa_tbl_htable[hashidx].lock);

	/* lock for read, search the table and return */
	return dbcassa_db_search_table(hashidx, hash, dbn, tbn);
}

/*
 *	Read all table schemas at startup
 * */
int dbcassa_read_table_schemas(void)
{
	int i, j;
	str db_name, tb_name;
	DIR* srcdir = opendir(dbcassa_schema_path.s);
	DIR* db_dir;
	struct dirent* dent;
	int fn_len = dbcassa_schema_path.len;
	struct stat fstat;
	int dir_len;
	dbcassa_table_p tbc;
	unsigned int hashidx;

	/* init tables' hash table */
	if (!dbcassa_tbl_htable) {
		dbcassa_tbl_htable = (dbcassa_tbl_htable_p)shm_malloc(DBCASSA_TABLE_SIZE*
					sizeof(dbcassa_tbl_htable_t));
		if(dbcassa_tbl_htable==NULL)
		{
			LM_CRIT("no enough shm mem\n");
			return -1;
		}
		memset(dbcassa_tbl_htable, 0, DBCASSA_TABLE_SIZE*sizeof(dbcassa_tbl_htable_t));
		for(i=0; i<DBCASSA_TABLE_SIZE; i++)
		{
			if (lock_init(&dbcassa_tbl_htable[i].lock.lock)==0)
			{
				LM_CRIT("cannot init tables' sem's\n");
				for(j=i-1; j>=0; j--)
					lock_destroy(&dbcassa_tbl_htable[j].rw_lock.lock);
				return -1;
			}
		}
	}

	memset(full_path_buf, 0, _POSIX_PATH_MAX);
	strcpy(full_path_buf, dbcassa_schema_path.s);
	if (full_path_buf[dbcassa_schema_path.len - 1] != '/') {
		full_path_buf[fn_len++]= '/';
		dbcassa_schema_path.len++;
	}

	if (srcdir == NULL) {
		perror("opendir");
		return -1;
	}
	LM_DBG("Full name= %.*s\n", fn_len, full_path_buf);

	while((dent = readdir(srcdir)) != NULL)
	{
		if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
			continue;

		/* Calculate full name, check we are in file length limits */
		if ((fn_len + strlen(dent->d_name) + 1) > _POSIX_PATH_MAX)
			continue;

		db_name.s = dent->d_name;
		db_name.len = strlen(dent->d_name);
		
		strcpy(full_path_buf+fn_len, dent->d_name);
		dir_len = fn_len + db_name.len;

		LM_DBG("Full dir name= %.*s\n", dir_len, full_path_buf);

		if (stat(full_path_buf, &fstat) < 0) {
			LM_ERR("stat failed %s\n", strerror(errno));
			continue;
		}

		if (!S_ISDIR(fstat.st_mode))  {
			LM_ERR("not a directory\n");
			continue;
		}

		/*
		if (fstatat(dirfd(srcdir), dent->d_name, &st) < 0)
		{
			perror(dent->d_name);
			continue;
		}
		*/

		LM_DBG("Found database %s\n", dent->d_name);
		db_dir = opendir(full_path_buf);
		if(!db_dir) {
			LM_ERR("Failed to open dictory %s\n", full_path_buf);
			continue;
		}
		full_path_buf[dir_len++]= '/';
		while((dent = readdir(db_dir)) != NULL)
		{
			if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
				continue;
			LM_DBG("database table %s\n", dent->d_name);
			if(dir_len + strlen(dent->d_name)+1 > _POSIX_PATH_MAX) {
				LM_ERR("File len too large\n");
				continue;
			}
			strcpy(full_path_buf+dir_len, dent->d_name);

			tb_name.s = dent->d_name;
			tb_name.len = strlen(dent->d_name);

			LM_DBG("File path= %s\n", full_path_buf);
			tbc = dbcassa_load_file(&db_name, &tb_name);
			if(!tbc)
			{
				LM_ERR("could not load database from file [%s]\n", tb_name.s);
				return -1;
			}
			hashidx = core_hash(&db_name, &tb_name, DBCASSA_TABLE_SIZE);
			tbc->hash = hashidx;
			tbc->next = dbcassa_tbl_htable[hashidx].dtp;
			dbcassa_tbl_htable[hashidx].dtp = tbc;
		}
		closedir(db_dir);
	}
	closedir(srcdir);


	return 0;
}

/*
 *	Destroy table schema table at shutdown
 * */
void dbcassa_destroy_htable(void)
{
	int i;
	dbcassa_table_p tbc, tbc0;

	/* destroy tables' hash table*/
	if(dbcassa_tbl_htable==0)
		return;

	for(i=0; i<DBCASSA_TABLE_SIZE; i++) {
		lock_destroy(&dbcassa_tbl_htable[i].rw_lock.lock);
		tbc = dbcassa_tbl_htable[i].dtp;
		while(tbc) {
			tbc0 = tbc;
			tbc = tbc->next;
			dbcassa_table_free(tbc0);
		}
	}
	shm_free(dbcassa_tbl_htable);
}

void dbcassa_lock_release(dbcassa_table_p tbc)
{
	unref_read_data(dbcassa_tbl_htable[tbc->hash].lock);
}
