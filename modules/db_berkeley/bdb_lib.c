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



#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../dprint.h"

#include "bdb_fld.h"
#include "bdb_lib.h"

static bdb_params_p _bdb_parms = NULL;

/**
 *
 */
int bdblib_init(bdb_params_p _p) 
{
	bdb_params_p dp = NULL;
	if (_bdb_parms != NULL)
		return 0;
		
		/*create default parms*/
	dp = (bdb_params_p) pkg_malloc( sizeof(bdb_params_t) );
	if (dp==NULL) 
	{
		ERR("not enough private memory\n");
		return -1;
	}
		
	if(_p!=NULL)
	{
		dp->cache_size  = _p->cache_size;
		dp->auto_reload = _p->auto_reload;
		dp->log_enable  = _p->log_enable;
		dp->journal_roll_interval = _p->journal_roll_interval;
	} else {
		dp->cache_size = (4 * 1024 * 1024); //4Mb
		dp->auto_reload = 0;
		dp->log_enable = 0;
		dp->journal_roll_interval = 3600;
	}
		
	_bdb_parms = dp;
	return 0;
}


/**
 * close all DBs and then the DBENV; free all memory
 */
int bdblib_destroy(void)
{
	if(_bdb_parms)	pkg_free(_bdb_parms);
	return 0;
}


/** closes the underlying Berkeley DB.
  assumes the lib data-structures are already initialzed;
  used to sync and reload the db file.
*/
int bdblib_close(bdb_db_p _db_p, str *dirpath)
{
	int rc;
	bdb_tcache_p _tbc;
	DB* _db = NULL;
	DB_ENV* _env = NULL;
	
	if (_db_p==NULL || dirpath==NULL)
		return -1;
	
	rc = 0;
	
	if (_db_p==NULL)
	{	
		DBG("DB not found %.*s \n", dirpath->len, dirpath->s);
		return 1; /*table not found*/
	}
	
	_env = _db_p->dbenv;
	_tbc = _db_p->tables;
	DBG("ENV %.*s \n", _db_p->name.len, _db_p->name.s);
	if(dirpath->len == _db_p->name.len && 
		!strncasecmp(dirpath->s, _db_p->name.s, _db_p->name.len))
	{
		//close the whole dbenv
		DBG("ENV %.*s \n", dirpath->len, dirpath->s);
		while(_tbc)
		{
			if(_tbc->dtp)
			{
				_db = _tbc->dtp->db;
				if(_db)
					rc = _db->close(_db, 0);
				if(rc != 0)
					ERR("error closing %s\n", _tbc->dtp->name.s);
				_tbc->dtp->db = NULL;
			}
			_tbc = _tbc->next;
		}
		_env->close(_env, 0);
		_db_p->dbenv = NULL;
		return 0;
	}
		
	//close a particular db
	while(_tbc)
	{
		if(_tbc->dtp)
		{
			DBG("checking DB %.*s \n", _tbc->dtp->name.len, _tbc->dtp->name.s);
				
			if(_tbc->dtp->name.len == dirpath->len && 
				!strncasecmp(_tbc->dtp->name.s, dirpath->s, dirpath->len ))
			{
				DBG("DB %.*s \n", dirpath->len, dirpath->s);
				_db = _tbc->dtp->db;
				if(_db)
					rc = _db->close(_db, 0);
				if(rc != 0)
					ERR("error closing %s\n", _tbc->dtp->name.s);
				_tbc->dtp->db = NULL;
				return 0;
			}
		}
		_tbc = _tbc->next;
	}
	DBG("DB not found %.*s \n", dirpath->len, dirpath->s);
	return 1; /*table not found*/	
}

/** opens the underlying Berkeley DB.
  assumes the lib data-structures are already initialzed;
  used to sync and reload the db file.
*/
int bdblib_reopen(bdb_db_p _db_p, str *dirpath)
{
	int rc, flags;
	bdb_tcache_p _tbc;
	DB* _db = NULL;
	DB_ENV* _env = NULL;
	rc = flags = 0;
	_tbc = NULL;
	
	if (_db_p==NULL || dirpath==NULL)
		return -1;

	
	if (_db_p)
	{
		DBG("bdb: DB not found %.*s \n", dirpath->len, dirpath->s);
		return 1; /*table not found*/
	}
	
	_env = _db_p->dbenv;
	_tbc = _db_p->tables;
		
	if(dirpath->len ==_db_p->name.len && 
		!strncasecmp(dirpath->s, _db_p->name.s, _db_p->name.len))
	{
		//open the whole dbenv
		DBG("-- bdblib_reopen ENV %.*s \n", dirpath->len, dirpath->s);
		if(!_db_p->dbenv)
		{
			rc = bdblib_create_dbenv(&_env, dirpath->s);
			_db_p->dbenv = _env;
		}
			
		if(rc!=0) return rc;

		_env = _db_p->dbenv;
		_tbc = _db_p->tables;

		while(_tbc)
		{
			if(_tbc->dtp)
			{
				if(!_tbc->dtp->db)
				{
					if ((rc = db_create(&_db, _env, 0)) != 0)
					{
						_env->err(_env, rc, "db_create");
						ERR("error in db_create, db error: %s.\n",
								db_strerror(rc));
						bdblib_recover(_tbc->dtp, rc);
					}
				}
					
				if ((rc = _db->open(_db, NULL, dirpath->s, NULL, DB_HASH,
								DB_CREATE, 0664)) != 0)
				{
					_db->dbenv->err(_env, rc, "DB->open: %s", dirpath->s);
					ERR("error in db_open: %s.\n",db_strerror(rc));
					bdblib_recover(_tbc->dtp, rc);
				}
					
				_tbc->dtp->db = _db;
			}
			_tbc = _tbc->next;
		}
		_env->close(_env, 0);
		return rc;
	}
		
	// open a particular db
	while(_tbc)
	{
		if(_tbc->dtp)
		{
			ERR("checking DB %.*s \n", _tbc->dtp->name.len, _tbc->dtp->name.s);
				
			if(_tbc->dtp->name.len == dirpath->len && 
				!strncasecmp(_tbc->dtp->name.s, dirpath->s, dirpath->len ))
			{
				ERR("DB %.*s \n", dirpath->len, dirpath->s);
				if(!_tbc->dtp->db) 
				{
					if ((rc = db_create(&_db, _env, 0)) != 0)
					{
						_env->err(_env, rc, "db_create");
						ERR("error in db_create, db error: %s.\n",
								db_strerror(rc));
						bdblib_recover(_tbc->dtp, rc);
					}
				}
					
				if ((rc = _db->open(_db, NULL, dirpath->s, NULL, DB_HASH,
								DB_CREATE, 0664)) != 0)
				{
					_db->dbenv->err(_env, rc, "DB->open: %s", dirpath->s);
					ERR("bdb open: %s.\n",db_strerror(rc));
					bdblib_recover(_tbc->dtp, rc);
				}
				_tbc->dtp->db = _db;
				return rc;
			}
		}
		_tbc = _tbc->next;
	}

	DBG("DB not found %.*s \n", dirpath->len, dirpath->s);
	return 1; /*table not found*/
}


/**
 *
 */
int bdblib_create_dbenv(DB_ENV **_dbenv, char* _home)
{
	DB_ENV *env;
	char *progname;
	int rc, flags;
	
	progname = "kamailio";
	
	/* Create an environment and initialize it for additional error * reporting. */ 
	if ((rc = db_env_create(&env, 0)) != 0) 
	{
		ERR("db_env_create failed! bdb error: %s.\n", db_strerror(rc)); 
		return (rc);
	}
 
	env->set_errpfx(env, progname);

	/*  Specify the shared memory buffer pool cachesize */ 
	if ((rc = env->set_cachesize(env, 0, _bdb_parms->cache_size, 0)) != 0) 
	{
		ERR("dbenv set_cachsize failed! bdb error: %s.\n", db_strerror(rc));
		env->err(env, rc, "set_cachesize"); 
		goto err; 
	}

	/* Concurrent Data Store flags */
	flags = DB_CREATE |
		DB_INIT_CDB |
		DB_INIT_MPOOL |
		DB_THREAD;
	
	/* Transaction Data Store flags ; not supported yet */
	/*
	flags = DB_CREATE |
		DB_RECOVER |
		DB_INIT_LOG | 
		DB_INIT_LOCK |
		DB_INIT_MPOOL |
		DB_THREAD |
		DB_INIT_TXN;
	*/
	
	/* Open the environment */ 
	if ((rc = env->open(env, _home, flags, 0)) != 0) 
	{ 
		ERR("dbenv is not initialized! bdb error: %s.\n",db_strerror(rc));
		env->err(env, rc, "environment open: %s", _home); 
		goto err; 
	}
	
	*_dbenv = env;
	return (0);

err: (void)env->close(env, 0);
	return (rc);
}


/**
 */
bdb_db_p bdblib_get_db(str *dirpath)
{
	int rc;
	bdb_db_p _db_p=NULL;

	if(dirpath==0 || dirpath->s==NULL || dirpath->s[0]=='\0')
		return NULL;

	if(_bdb_parms==NULL)
	{
		ERR("bdb: cache is not initialized! Check if you loaded bdb "
			"before any other module that uses it.\n");
		return NULL;
	}

	if(!bdb_is_database(dirpath->s))
	{	
		ERR("bdb: database [%.*s] does not exists!\n",
				dirpath->len , dirpath->s);
		return NULL;
	}

	_db_p = (bdb_db_p)pkg_malloc(sizeof(bdb_db_t));
	if(!_db_p)
	{
		ERR("no private memory for dbenv_t.\n");
		pkg_free(_db_p);
		return NULL;
	}

	_db_p->name.s = (char*)pkg_malloc(dirpath->len*sizeof(char));
	memcpy(_db_p->name.s, dirpath->s, dirpath->len);
	_db_p->name.len = dirpath->len;

	if ((rc = bdblib_create_dbenv(&(_db_p->dbenv), dirpath->s)) != 0)
	{
		ERR("bdblib_create_dbenv failed");
		pkg_free(_db_p->name.s);
		pkg_free(_db_p);
		return NULL;
	}

	_db_p->tables=NULL;

	return _db_p;
}


/**
 * look thru a linked list for the table. if dne, create a new one
 * and add to the list
*/
bdb_tcache_p bdblib_get_table(bdb_db_t *_db, str *_s)
{
	bdb_tcache_p _tbc = NULL;
	bdb_table_p _tp = NULL;

	if(!_db || !_s || !_s->s || _s->len<=0)
		return NULL;

	if(!_db->dbenv)
	{
		return NULL;
	}

	_tbc = _db->tables;
	while(_tbc)
	{
		if(_tbc->dtp)
		{

			if(_tbc->dtp->name.len == _s->len 
				&& !strncasecmp(_tbc->dtp->name.s, _s->s, _s->len ))
			{
				return _tbc;
			}
		}
		_tbc = _tbc->next;
	}

	_tbc = (bdb_tcache_p)pkg_malloc(sizeof(bdb_tcache_t));
	if(!_tbc)
		return NULL;

	_tp = bdblib_create_table(_db, _s);

	if(!_tp)
	{
		ERR("failed to create table.\n");
		pkg_free(_tbc);
		return NULL;
	}

	_tbc->dtp = _tp;

	if(_db->tables)
		(_db->tables)->prev = _tbc;
	
	_tbc->next = _db->tables;
	_db->tables = _tbc;

	return _tbc;
}


void bdblib_log(int op, bdb_db_p _db_p, bdb_table_p _tp, char* _msg, int len)
{
	if(!_tp || !len) 		return;
	if(! _bdb_parms->log_enable) 	return;
	if (_tp->logflags == JLOG_NONE)	return;
	
	if ((_tp->logflags & op) == op)
	{	int op_len=7;
		char buf[MAX_ROW_SIZE + op_len];
		char *c;
		time_t now = time(NULL);
		
		if( _bdb_parms->journal_roll_interval)
		{
			if((_tp->t) && (now - _tp->t) > _bdb_parms->journal_roll_interval)
			{	/*try to roll logfile*/
				if(bdblib_create_journal(_db_p, _tp))
				{
					ERR("Journaling has FAILED !\n");
					return;
				}
			}
		}
		
		c = buf;
		switch (op)
		{
		case JLOG_INSERT:
			strncpy(c, "INSERT|", op_len);
			break;
		case JLOG_UPDATE:
			strncpy(c, "UPDATE|", op_len);
			break;
		case JLOG_DELETE:
			strncpy(c, "DELETE|", op_len);
			break;
		}
		
		c += op_len;
		strncpy(c, _msg, len);
		c +=len;
		*c = '\n';
		c++;
		*c = '\0';
		
		if ((_tp->logflags & JLOG_STDOUT) == JLOG_STDOUT)
			puts(buf);
		
		if ((_tp->logflags & JLOG_SYSLOG) == JLOG_SYSLOG)
			syslog(LOG_LOCAL6, "%s", buf);
		
		if(_tp->fp) 
		{
			if(!fputs(buf, _tp->fp) )
				fflush(_tp->fp);
		}
	}
}

/**
 * The function is called to create a handle to a db table.
 * 
 * On startup, we do not create any of the db handles.
 * Instead it is done on first-use (lazy-initialized) to only create handles to 
 * files (db) that we require.
 * 
 * There is one db file per kamailio table (eg. acc), and they should exist
 * in your DB_PATH (refer to kamctlrc) directory.
 *
 * This function does _not_ create the underlying binary db tables.
 * Creating the tables MUST be manually performed before 
 * kamailio startup by 'kamdbctl create'
 *
 * Function returns NULL on error, which will cause kamailio to exit.
 *
 */
bdb_table_p bdblib_create_table(bdb_db_p _db, str *_s)
{

	int rc,i,flags;
	DB *bdb = NULL;
	bdb_table_p tp = NULL;
	char tblname[MAX_TABLENAME_SIZE]; 

	if(!_db || !_db->dbenv)
	{
		ERR("no bdb_db_p or dbenv.\n");
		return NULL;
	}

	tp = (bdb_table_p)pkg_malloc(sizeof(bdb_table_t));
	if(!tp)
	{
		ERR("no private memory for bdb_table_t.\n");
		return NULL;
	}

	if ((rc = db_create(&bdb, _db->dbenv, 0)) != 0)
	{ 
		_db->dbenv->err(_db->dbenv, rc, "database create");
		ERR("error in db_create, bdb error: %s.\n",db_strerror(rc));
		pkg_free(tp);
		return NULL;
	}

	memset(tblname, 0, MAX_TABLENAME_SIZE);
	strncpy(tblname, _s->s, _s->len);

	flags = DB_THREAD;

	if ((rc = bdb->open(bdb, NULL, tblname, NULL, DB_HASH, flags, 0664)) != 0)
	{ 
		_db->dbenv->err(_db->dbenv, rc, "DB->open: %s", tblname);
		ERR("bdb open failed: %s.\n",db_strerror(rc));
		pkg_free(tp);
		return NULL;
	}

	tp->name.s = (char*)pkg_malloc(_s->len*sizeof(char));
	memcpy(tp->name.s, _s->s, _s->len);
	tp->name.len = _s->len;
	tp->db=bdb;
	tp->ncols=0;
	tp->nkeys=0;
	tp->ro=0;    /*0=ReadWrite ; 1=ReadOnly*/
	tp->ino=0;   /*inode*/
	tp->logflags=0; /*bitmap; 4=Delete, 2=Update, 1=Insert, 0=None*/
	tp->fp=0;
	tp->t=0;
	
	for(i=0;i<MAX_NUM_COLS;i++)
		tp->colp[i] = NULL;

	/*load metadata; seeded\db_loaded when database are created*/
	
	/*initialize columns with metadata*/
	rc = load_metadata_columns(tp);
	if(rc!=0)
	{
		ERR("FAILED to load METADATA COLS in table: %s.\n", tblname);
		goto error;
	}
	
	/*initialize columns default values from metadata*/
	rc = load_metadata_defaults(tp);
	if(rc!=0)
	{
		ERR("FAILED to load METADATA DEFAULTS in table: %s.\n", tblname);
		goto error;
	}
	
	rc = load_metadata_keys(tp);
	if(rc!=0)
	{
		ERR("FAILED to load METADATA KEYS in table: %s.\n", tblname);
		/*will have problems later figuring column types*/
		goto error;
	}

	/*opened RW by default; Query to set the RO flag */
	rc = load_metadata_readonly(tp);
	if(rc!=0)
	{
		INFO("No METADATA_READONLY in table: %s.\n", tblname);
		/*non-critical; table will default to READWRITE*/
	}

	if(tp->ro)
	{	
		/*schema defines this table RO readonly*/
		
		if ((rc = bdb->close(bdb,0)) != 0)
		{ 
			_db->dbenv->err(_db->dbenv, rc, "DB->close: %s", tblname);
			ERR("bdb close: %s.\n",db_strerror(rc));
			goto error;
		}
		
		bdb = NULL;
		if ((rc = db_create(&bdb, _db->dbenv, 0)) != 0)
		{ 
			_db->dbenv->err(_db->dbenv, rc, "database create");
			ERR("error in db_create.\n");
			goto error;
		}
		
		flags = DB_THREAD | DB_RDONLY;
		if ((rc = bdb->open(bdb, NULL, tblname, NULL, DB_HASH, flags, 0664)) != 0)
		{ 
			_db->dbenv->err(_db->dbenv, rc, "DB->open: %s", tblname);
			ERR("bdb open: %s.\n",db_strerror(rc));
			goto error;
		}
		tp->db=bdb;
	}
	
	/* set the journaling flags; flags indicate which operations
	   need to be journalled. (e.g possible to only journal INSERT.)
	*/
	rc = load_metadata_logflags(tp);
	if(rc!=0)
		INFO("No METADATA_LOGFLAGS in table: %s.\n", tblname);
	
	if ((tp->logflags & JLOG_FILE) == JLOG_FILE)
		bdblib_create_journal(_db, tp);
	
	return tp;
	
error:
	if(tp) 
	{
		pkg_free(tp->name.s);
		pkg_free(tp);
	}
	return NULL;
}

int bdblib_create_journal(bdb_db_p _db_p, bdb_table_p _tp)
{
	char *s;
	char fn[1024];
	char d[64];
	FILE *fp = NULL;
	struct tm *t;
	int bl;
	time_t tim = time(NULL);
	
	if(! _db_p || ! _tp) return -1;
	if(! _bdb_parms->log_enable) return 0;
	/* journal filename ; e.g. '/var/kamailio/db/location-YYYYMMDDhhmmss.jnl' */
	s=fn;
	strncpy(s, _db_p->name.s, _db_p->name.len);
	s+=_db_p->name.len;
	
	*s = '/';
	s++;
	
	strncpy(s, _tp->name.s, _tp->name.len);
	s+=_tp->name.len;
	
	t = localtime( &tim );
	bl=strftime(d,128,"-%Y%m%d%H%M%S.jnl",t);
	strncpy(s, d, bl);
	s+= bl;
	*s = 0;
	
	if(_tp->fp)
	{	/* must be rolling. */
		if( fclose(_tp->fp) )
		{	ERR("Failed to Close Log in table: %.*s .\n", _tp->name.len,
			 _tp->name.s);
			return -1;
		}
	}
	
	if( (fp = fopen(fn, "w")) != NULL )
	{
		_tp->fp = fp;
	}
	else
	{
		ERR("Failed to Open Log in table: %.*s .\n",_tp->name.len, _tp->name.s);
		return -1;
	}
	
	_tp->t = tim;
	return 0;

}

int load_metadata_columns(bdb_table_p _tp)
{
	int ret,n,len;
	char dbuf[MAX_ROW_SIZE];
	char *s = NULL;
	char cn[64], ct[16];
	DB *db = NULL;
	DBT key, data;
	bdb_col_p col;
	ret = n = len = 0;
	
	if(!_tp || !_tp->db)
		return -1;
	
	if(_tp->ncols!=0)
		return 0;
	
	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);

	key.data = METADATA_COLUMNS;
	key.size = strlen(METADATA_COLUMNS);

	/*memory for the result*/
	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;
	
	if ((ret = db->get(db, NULL, &key, &data, 0)) != 0) 
	{
		db->err(db, ret, "load_metadata_columns DB->get failed");
		ERR("FAILED to find METADATA_COLUMNS in DB \n");
		return -1;
	}

	/* eg: dbuf = "bdb_table_name(str) bdb_table_version(int)" */
	s = strtok(dbuf, " ");
	while(s!=NULL && n<MAX_NUM_COLS) 
	{
		/* eg: meta[0]=table_name  meta[1]=str */
		sscanf(s,"%20[^(](%10[^)])[^\n]", cn, ct);
		
		/* create column*/
		col = (bdb_col_p) pkg_malloc(sizeof(bdb_col_t));
		if(!col)
		{	ERR("out of private memory \n");
			return -1;
		}
		
		/* set name*/
		len = strlen( cn );
		col->name.s = (char*)pkg_malloc(len * sizeof(char));
		memcpy(col->name.s, cn, len );
		col->name.len = len;
		
		/*set column type*/
		if(strncmp(ct, "str", 3)==0)
		{	col->type = DB_STR;
		}
		else if(strncmp(ct, "int", 3)==0)
		{	col->type = DB_INT;
		}
		else if(strncmp(ct, "double", 6)==0)
		{	col->type = DB_DOUBLE;
		}
		else if(strncmp(ct, "datetime", 8)==0)
		{	col->type = DB_DATETIME;
		}
		else
		{	col->type = DB_STR;
		}
		
		col->flag = 0;
		_tp->colp[n] = col;
		n++;
		_tp->ncols++;
		s=strtok(NULL, " ");
	}

	return 0;
}

int load_metadata_keys(bdb_table_p _tp)
{
	int ret,n,ci;
	char dbuf[MAX_ROW_SIZE];
	char *s = NULL;
	DB *db = NULL;
	DBT key, data;
	ret = n = ci = 0;
	
	if(!_tp || !_tp->db)
		return -1;
	
	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);
	key.data = METADATA_KEY;
	key.size = strlen(METADATA_KEY);
	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;
	
	if ((ret = db->get(db, NULL, &key, &data, 0)) != 0) 
	{
		db->err(db, ret, "load_metadata_keys DB->get failed");
		ERR("FAILED to find METADATA in table \n");
		return ret;
	}
	
	s = strtok(dbuf, " ");
	while(s!=NULL && n< _tp->ncols) 
	{	ret = sscanf(s,"%i", &ci);
		if(ret != 1) return -1;
		if( _tp->colp[ci] ) 
		{	_tp->colp[ci]->flag = 1;
			_tp->nkeys++;
		}
		n++;
		s=strtok(NULL, " ");
	}

	return 0;
}


int load_metadata_readonly(bdb_table_p _tp)
{
	int i, ret;
	char dbuf[MAX_ROW_SIZE];

	DB *db = NULL;
	DBT key, data;
	i = 0;
	
	if(!_tp || !_tp->db)
		return -1;
	
	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);
	key.data = METADATA_READONLY;
	key.size = strlen(METADATA_READONLY);
	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;
	
	if ((ret = db->get(db, NULL, &key, &data, 0)) != 0) 
	{	return ret;
	}
	
	if( 1 == sscanf(dbuf,"%i", &i) )
		_tp->ro=(i>0)?1:0;
	
	return 0;
}

int load_metadata_logflags(bdb_table_p _tp)
{
	int i, ret;
	char dbuf[MAX_ROW_SIZE];

	DB *db = NULL;
	DBT key, data;
	i = 0;
	
	if(!_tp || !_tp->db)
		return -1;
	
	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);
	key.data = METADATA_LOGFLAGS;
	key.size = strlen(METADATA_LOGFLAGS);
	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;
	
	if ((ret = db->get(db, NULL, &key, &data, 0)) != 0) 
	{	return ret;
	}
	
	if( 1 == sscanf(dbuf,"%i", &i) )
		_tp->logflags=i;
	
	return 0;
}

int load_metadata_defaults(bdb_table_p _tp)
{
	int ret,n,len;
	char dbuf[MAX_ROW_SIZE];
	char *s = NULL;
	char cv[64];
	DB *db = NULL;
	DBT key, data;
	bdb_col_p col;
	ret = n = len = 0;
	
	if(!_tp || !_tp->db)
		return -1;
	
	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);

	key.data = METADATA_DEFAULTS;
	key.size = strlen(METADATA_DEFAULTS);

	/*memory for the result*/
	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;
	
	if ((ret = db->get(db, NULL, &key, &data, 0)) != 0) 
	{
		/*no defaults in DB; make some up.*/
		for(n=0; n<_tp->ncols; n++)
		{
			col = _tp->colp[n];
			if( col ) 
			{	/*set all columns default value to 'NULL' */
				len = strlen("NULL");
				col->dv.s = (char*)pkg_malloc(len * sizeof(char));
				memcpy(col->dv.s, "NULL", len);
				col->dv.len = len;
			}
		}
		return 0;
	}
	
	/* use the defaults provided*/
	s = strtok(dbuf, DELIM);
	while(s!=NULL && n< _tp->ncols) 
	{	ret = sscanf(s,"%s", cv);
		if(ret != 1) return -1;
		col = _tp->colp[n];
		if( col ) 
		{	/*set column default*/
			len = strlen(s);
			col->dv.s = (char*)pkg_malloc(len * sizeof(char));
			memcpy(col->dv.s, cv, len);
			col->dv.len = len;
		}
		n++;
		s=strtok(NULL, DELIM);
	}
	
	return 0;
}

inline int bdb_int2str(int _v, char* _s, int* _l)
{
	int ret;

	if ((!_s) || (!_l) || (!*_l)) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-d", _v);
	if (ret < 0 || ret >= *_l) {
		ERR("Error in snprintf\n");
		return -1;
	}
	*_l = ret;

	return 0;
}

inline int bdb_double2str(double _v, char* _s, int* _l)
{
	int ret;

	if ((!_s) || (!_l) || (!*_l)) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-10.2f", _v);
	if (ret < 0 || ret >= *_l) {
		ERR("Error in snprintf\n");
		return -1;
	}
	*_l = ret;

	return 0;
}

inline int bdb_time2str(time_t _v, char* _s, int* _l)
{
	struct tm* t;
	int l;

	if ((!_s) || (!_l) || (*_l < 2)) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	*_s++ = '\'';

	/* Convert time_t structure to format accepted by the database */
	t = localtime(&_v);
	l = strftime(_s, *_l -1, "%Y-%m-%d %H:%M:%S", t);

	if (l == 0) {
		ERR("Error during time conversion\n");
		/* the value of _s is now unspecified */
		_s = NULL;
		_l = 0;
		return -1;
	}
	*_l = l;

	*(_s + l) = '\'';
	*_l = l + 2;
	return 0;
}

/*
 * Used when converting result from a query
 */
int bdb_val2str(db_fld_t *fld, char *sout, int *slen)
{
	int l;
	db_fld_val_t *val;

	if (fld->flags&DB_NULL) 
	{
		*slen = snprintf(sout, *slen, "NULL");
		return 0;
	}
	
	val = &(fld->v);
	switch(fld->type)
	{
		case DB_INT:
			if (bdb_int2str(val->int4, sout, slen) < 0) {
				ERR("Error while converting int to string\n");
				return -2;
			} else {
				DBG("Converted int to string\n");
				return 0;
			}
		break;

		case DB_BITMAP:
			if (bdb_int2str(val->bitmap, sout, slen) < 0) {
				ERR("Error while converting bitmap to string\n");
				return -3;
			} else {
				DBG("Converted bitmap to string\n");
				return 0;
			}
		break;

		case DB_DOUBLE:
			if (bdb_double2str(val->dbl, sout, slen) < 0) {
				ERR("Error while converting double  to string\n");
				return -3;
			} else {
				DBG("Converted double to string\n");
				return 0;
			}
		break;

		case DB_CSTR:
			l = strlen(val->cstr);
			if (*slen < l ) 
			{
				ERR("Destination buffer too short for string\n");
				return -4;
			} else {
				DBG("Converted string to string\n");
				strncpy(sout, val->cstr , l);
				sout[l] = 0;
				*slen = l;
				return 0;
			}
		break;

		case DB_STR:
			l = val->lstr.len;
			if (*slen < l) 
			{
				ERR("Destination buffer too short for str\n");
				return -5;
			} else {
				DBG("Converted str to string\n");
				strncpy(sout, val->lstr.s , val->lstr.len);
				*slen = val->lstr.len;
				return 0;
			}
		break;

		case DB_DATETIME:
			if (bdb_time2str(val->time, sout, slen) < 0) {
				ERR("Error while converting time_t to string\n");
				return -6;
			} else {
				DBG("Converted time_t to string\n");
				return 0;
			}
		break;

		case DB_BLOB:
			l = val->blob.len;
			if (*slen < l) 
			{
				ERR("Destination buffer too short for blob\n");
				return -7;
			} else {
				DBG("Converting BLOB [%s]\n", sout);
				memcpy(sout, val->blob.s , val->blob.len);
				*slen = l;
				return 0;
			}
		break;

		default:
			DBG("Unknown data type\n");
			return -8;
	}
}

/*creates a composite key _k of length _klen from n values of _v;
  provide your own initialized memory for target _k and _klen;
  resulting value: _k = "KEY1 | KEY2"
  ko = key only
*/

int bdblib_valtochar(bdb_table_p tp, db_fld_t *fld, int fld_count, char *kout,
		int *klen, int ktype)
{
	char *p; 
	static char sk[MAX_ROW_SIZE]; // subkey(sk) val
	char* delim = DELIM;
	char* cNULL = "NULL";
	int  len, total, sum;
	int i, j, k;
	bdb_fld_t *f;

	p =  kout;
	len = sum = total = 0;
	i = j = k = 0;
	
	if(tp==NULL) return -1;
	if(fld==NULL || fld_count<1) return -1;
	if(kout==NULL || klen==NULL ) return -1;
	if( *klen < 1)    return -1;
	
	memset(sk, 0, MAX_ROW_SIZE);
	total = *klen;
	*klen = 0; //sum
	
	/*
	  schema has specified keys
	  verify all schema keys are provided
	  use 'NULL' for those that are missing.
	*/
	for(i=0; i<tp->ncols; i++)
	{	/* i indexes columns in schema order */
		if(ktype)
		{	/* keymode; skip over non-key columns */
			if(tp->colp[i]->flag==0) 
				continue; 
		}
		
		for(j=0; j<fld_count; j++)
		{
			f = DB_GET_PAYLOAD(fld + j);
			/*
			  j indexes the columns provided in _k
			  which may be less than the total required by
			  the schema. the app does not know the order
			  of the columns in our schema!
			 */
			k = f->col_pos;
			
			/*
			 * k index will remap back to our schema order; like i
			 */
			if(i == k)
			{
				/*
				 KEY was provided; append to buffer;
				 _k[j] contains a key, but its a key that 
				 corresponds to column k of our schema.
				 now we know its a match, and we dont need
				 index k for anything else
				*/
				len = total - sum;
				if ( bdb_val2str((fld+j), sk, &len) != 0)
				{
					ERR("Destination buffer too short for subval %s\n",sk);
					return -4;
				}
				
				sum += len;
				if(sum > total)
				{
					ERR("Destination buffer too short for subval %s\n",sk);
					return -5;
				}

				strncpy(p, sk, len);
				p += len;
				*klen = sum;

				sum += DELIM_LEN;
				if(sum > total)
				{
					ERR("Destination buffer too short for delim \n");
					return -5;
				} 
				
				/* append delim */
				strncpy(p, delim, DELIM_LEN);
				p += DELIM_LEN;
				*klen = sum;
				
				
				/* take us out of inner for loop
				   and at the end of the outer loop
				   to look for our next schema key
				*/
				goto next;
			}
		}

		/*
		 NO KEY provided; use the column default value (dv)
		     i.e _tp->colp[i]->dv
		*/
		len = tp->colp[i]->dv.len;
		sum += len;
		if(sum > total)
		{
			ERR("Destination buffer too short for subval %s\n",cNULL);
			return -5;
		}
		
		strncpy(p, tp->colp[i]->dv.s, len);
		p += len;
		*klen = sum;
		
		sum += DELIM_LEN;
		if(sum > total)
		{
			ERR("Destination buffer too short for delim \n");
			return -5;
		} 
		
		strncpy(p, delim, DELIM_LEN);
		p += DELIM_LEN;
		*klen = sum;
next:
		continue;
	}

	return 0;
}


/**
 *
 */
int bdb_db_free(bdb_db_p _dbp)
{
	bdb_tcache_p _tbc = NULL, _tbc0=NULL;
	if(!_dbp)
		return -1;

	_tbc = _dbp->tables;

	while(_tbc)
	{
		_tbc0 = _tbc->next;
		bdb_tcache_free(_tbc);
		_tbc = _tbc0;
	}
	
	if(_dbp->dbenv)
		_dbp->dbenv->close(_dbp->dbenv, 0);
	
	if(_dbp->name.s)
		pkg_free(_dbp->name.s);
	
	pkg_free(_dbp);

	return 0;
}


/**
 *
 */
int bdb_tcache_free(bdb_tcache_p _tbc)
{
	if(!_tbc)
		return -1;
	
	/*while ??!? */	
	if(_tbc->dtp)
		bdb_table_free(_tbc->dtp);
	
	pkg_free(_tbc);

	return 0;
}


/**
 * close DB (sync data to disk) and free mem
 */
int bdb_table_free(bdb_table_p _tp)
{	int i;
	if(!_tp)
		return -1;

	if(_tp->db)
		_tp->db->close(_tp->db, 0);
	
	if(_tp->fp)
		fclose(_tp->fp);

	if(_tp->name.s)
		pkg_free(_tp->name.s);
	
	for(i=0;i<_tp->ncols;i++)
	{	if(_tp->colp[i])
		{	pkg_free(_tp->colp[i]->name.s);
			pkg_free(_tp->colp[i]->dv.s);
			pkg_free(_tp->colp[i]);
		}
	}

	pkg_free(_tp);
	return 0;
}

int bdblib_recover(bdb_table_p _tp, int _rc)
{
	switch(_rc)
	{
		case DB_LOCK_DEADLOCK:
		ERR("DB_LOCK_DEADLOCK detected !!\n");
		
		case DB_RUNRECOVERY:
		ERR("DB_RUNRECOVERY detected !! \n");
		bdblib_destroy();
		exit(1);
		break;
	}
	
	return 0;
}

/**
 *
 */
int bdb_is_database(char *dirpath)
{
	DIR *dirp = NULL;
	
	if(dirpath==NULL || dirpath[0]=='\0')
		return 0;
	dirp = opendir(dirpath);
	if(dirp==NULL)
		return 0;
	closedir(dirp);

	return 1;
}

int bdb_get_colpos(bdb_table_t *tp, char *name)
{
	str s;
	int i;

	if(tp==NULL || name==NULL)
	{
		ERR("bdb: bad parameters\n");
		return -1;
	}

	s.s = name;
	s.len = strlen(name);
	for(i=0; i<tp->ncols; i++) {
		if(tp->colp[i]->name.len == s.len
				&& !strncasecmp(s.s, tp->colp[i]->name.s, s.len))
			return i;
	}
	return -1;
}

int bdb_str2time(char *s, time_t *v)
{
	struct tm time;

	if ((!s) || (!v)) {
		ERR("bdb:invalid parameter value\n");
		return -1;
	}

	memset(&time, '\0', sizeof(struct tm));
	//if (strptime(s, "%Y-%m-%d %H:%M:%S", &time) == NULL) {
	//	ERR("Error during time conversion\n");
	//	return -1;
	//}

	time.tm_isdst = -1;
	*v = mktime(&time);

	return 0;
}

int bdb_str2double(char *s, double *v)
{
	if ((!s) || (!v)) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	*v = atof(s);
	return 0;
}

int bdb_str2int(char *s, int *v)
{
	long tmp;

	if (!s || !v) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	tmp = strtoul(s, 0, 10);
	if ((tmp == ULONG_MAX && errno == ERANGE) || 
	    (tmp < INT_MIN) || (tmp > UINT_MAX)) {
		ERR("Value out of range\n");
		return -1;
	}

	*v = (int)tmp;
	return 0;
}
