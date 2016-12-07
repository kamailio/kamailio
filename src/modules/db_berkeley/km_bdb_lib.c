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


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../dprint.h"

#include "km_bdb_util.h"
#include "km_bdb_lib.h"
#include "km_bdb_val.h"

static database_p *_cachedb = NULL;
static db_parms_p _db_parms = NULL;

/**
 *
 */
int km_bdblib_init(db_parms_p _p) 
{
	if (!_cachedb)
	{
		_cachedb = pkg_malloc( sizeof(database_p) );
		if (!_cachedb) 
		{	LM_CRIT("not enough private memory\n");
			return -1;
		}
		
		*_cachedb = NULL;
		
		/*create default parms*/
		db_parms_p dp = (db_parms_p) pkg_malloc( sizeof(db_parms_t) );
		if (!dp) 
		{	LM_CRIT("not enough private memory\n");
			return -1;
		}
		
		if(_p)
		{
			dp->cache_size  = _p->cache_size;
			dp->auto_reload = _p->auto_reload;
			dp->log_enable  = _p->log_enable;
			dp->journal_roll_interval = _p->journal_roll_interval;
		}
		else
		{
			dp->cache_size = (4 * 1024 * 1024); //4Mb
			dp->auto_reload = 0;
			dp->log_enable = 0;
			dp->journal_roll_interval = 3600;
		}
		
		_db_parms = dp;
	}
	return 0;
}


/**
 * close all DBs and then the DBENV; free all memory
 */
int km_bdblib_destroy(void)
{
	if (_cachedb)	db_free(*_cachedb);
	if(_db_parms)	pkg_free(_db_parms);
	return 0;
}


/** closes the underlying Berkeley DB.
  assumes the lib data-structures are already initialzed;
  used to sync and reload the db file.
*/
int km_bdblib_close(char* _n)
{
	str s;
	int rc;
	tbl_cache_p _tbc;
	DB* _db = NULL;
	DB_ENV* _env = NULL;
	database_p _db_p = *_cachedb;
	
	if (!_cachedb || !_n)
		return -1;
	
	rc = 0;
	s.s = (char*)_n;
	s.len = strlen(_n);
	
	if (_db_p)
	{	
		_env = _db_p->dbenv;
		_tbc = _db_p->tables;
LM_DBG("ENV %.*s \n"
	, _db_p->name.len
	, _db_p->name.s);
		if(s.len == _db_p->name.len && 
		!strncasecmp(s.s, _db_p->name.s, _db_p->name.len))
		{
			//close the whole dbenv
			LM_DBG("ENV %.*s \n", s.len, s.s);
			while(_tbc)
			{
				if(_tbc->dtp)
				{
					lock_get(&_tbc->dtp->sem);
					_db = _tbc->dtp->db;
					if(_db)
						rc = _db->close(_db, 0);
					if(rc != 0)
						LM_CRIT("error closing %s\n", _tbc->dtp->name.s);
					_tbc->dtp->db = NULL;
					
					lock_release(&_tbc->dtp->sem);
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
	LM_DBG("checking DB %.*s \n"
		, _tbc->dtp->name.len
		, _tbc->dtp->name.s);
				
				if(_tbc->dtp->name.len == s.len && 
				!strncasecmp(_tbc->dtp->name.s, s.s, s.len ))
				{
					LM_DBG("DB %.*s \n", s.len, s.s);
					lock_get(&_tbc->dtp->sem);
					_db = _tbc->dtp->db;
					if(_db)
						rc = _db->close(_db, 0);
					if(rc != 0)
						LM_CRIT("error closing %s\n", _tbc->dtp->name.s);
					_tbc->dtp->db = NULL;
					lock_release(&_tbc->dtp->sem);
					return 0;
				}
			}
			_tbc = _tbc->next;
		}
	}
	LM_DBG("DB not found %.*s \n", s.len, s.s);
	return 1; /*table not found*/
}

/** opens the underlying Berkeley DB.
  assumes the lib data-structures are already initialzed;
  used to sync and reload the db file.
*/
int km_bdblib_reopen(char* _n)
{
	str s;
	int rc, flags;
	tbl_cache_p _tbc;
	DB* _db = NULL;
	DB_ENV* _env = NULL;
	database_p _db_p = *_cachedb;
	rc = flags = 0;
	_tbc = NULL;
	
	if (!_cachedb || !_n)
		return -1;

	s.s = (char*)_n;
	s.len = strlen(_n);
	
	if (_db_p)
	{
		_env = _db_p->dbenv;
		_tbc = _db_p->tables;
		
		if(s.len ==_db_p->name.len && 
		!strncasecmp(s.s, _db_p->name.s,_db_p->name.len))
		{
			//open the whole dbenv
			LM_DBG("-- km_bdblib_reopen ENV %.*s \n", s.len, s.s);
			if(!_db_p->dbenv)
			{	rc = km_bdblib_create_dbenv(&_env, _n);
				_db_p->dbenv = _env;
			}
			
			if(rc!=0) return rc;
			_env = _db_p->dbenv;
			_tbc = _db_p->tables;

			while(_tbc)
			{
				if(_tbc->dtp)
				{
					lock_get(&_tbc->dtp->sem);
					if(!_tbc->dtp->db)
					{
						if ((rc = db_create(&_db, _env, 0)) != 0)
						{	_env->err(_env, rc, "db_create");
							LM_CRIT("error in db_create, db error: %s.\n",db_strerror(rc));
							km_bdblib_recover(_tbc->dtp, rc);
						}
					}
					
					if ((rc = _db->open(_db, NULL, _n, NULL, DB_HASH, DB_CREATE, 0664)) != 0)
					{	_db->dbenv->err(_env, rc, "DB->open: %s", _n);
						LM_CRIT("error in db_open: %s.\n",db_strerror(rc));
						km_bdblib_recover(_tbc->dtp, rc);
					}
					
					_tbc->dtp->db = _db;
					lock_release(&_tbc->dtp->sem);
				}
				_tbc = _tbc->next;
			}
			_env->close(_env, 0);
			return rc;
		}
		
		//open a particular db
		while(_tbc)
		{
			if(_tbc->dtp)
			{
	LM_DBG("checking DB %.*s \n"
		, _tbc->dtp->name.len
		, _tbc->dtp->name.s);
				
				if(_tbc->dtp->name.len == s.len && 
				!strncasecmp(_tbc->dtp->name.s, s.s, s.len ))
				{
					LM_DBG("DB %.*s \n", s.len, s.s);
					lock_get(&_tbc->dtp->sem);
					if(!_tbc->dtp->db) 
					{
						if ((rc = db_create(&_db, _env, 0)) != 0)
						{	_env->err(_env, rc, "db_create");
							LM_CRIT("error in db_create, db error: %s.\n",db_strerror(rc));
							km_bdblib_recover(_tbc->dtp, rc);
						}
					}
					
					if ((rc = _db->open(_db, NULL, _n, NULL, DB_HASH, DB_CREATE, 0664)) != 0)
					{	_db->dbenv->err(_env, rc, "DB->open: %s", _n);
						LM_CRIT("bdb open: %s.\n",db_strerror(rc));
						km_bdblib_recover(_tbc->dtp, rc);
					}
					_tbc->dtp->db = _db;
					lock_release(&_tbc->dtp->sem);
					return rc;
				}
			}
			_tbc = _tbc->next;
		}
		
	}
	LM_DBG("DB not found %.*s \n", s.len, s.s);
	return 1; /*table not found*/
}


/**
 *
 */
int km_bdblib_create_dbenv(DB_ENV **_dbenv, char* _home)
{
	DB_ENV *env;
	char *progname;
	int rc, flags;
	
	progname = "kamailio";
	
	/* Create an environment and initialize it for additional error * reporting. */ 
	if ((rc = db_env_create(&env, 0)) != 0) 
	{
		LM_ERR("db_env_create failed! bdb error: %s.\n", db_strerror(rc)); 
		return (rc);
	}
 
	env->set_errpfx(env, progname);

	/*  Specify the shared memory buffer pool cachesize */ 
	if ((rc = env->set_cachesize(env, 0, _db_parms->cache_size, 0)) != 0) 
	{
		LM_ERR("dbenv set_cachsize failed! bdb error: %s.\n", db_strerror(rc));
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
		LM_ERR("dbenv is not initialized! bdb error: %s.\n",db_strerror(rc));
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
database_p km_bdblib_get_db(str *_s)
{
	int rc;
	database_p _db_p=NULL;
	char name[512];

	if(!_s || !_s->s || _s->len<=0 || _s->len > 512)
		return NULL;

	if( !_cachedb)
	{
		LM_ERR("db_berkeley cache is not initialized! Check if you loaded db_berkeley "
			"before any other module that uses it.\n");
		return NULL;
	}

	_db_p = *_cachedb;
	if(_db_p)
	{
		LM_DBG("db already cached!\n");
		return _db_p;
	}

	if(!km_bdb_is_database(_s))
	{	
		LM_ERR("database [%.*s] does not exists!\n" ,_s->len , _s->s);
		return NULL;
	}

	_db_p = (database_p)pkg_malloc(sizeof(database_t));
	if(!_db_p)
	{
		LM_ERR("no private memory for dbenv_t.\n");
		pkg_free(_db_p);
		return NULL;
	}

	_db_p->name.s = (char*)pkg_malloc(_s->len*sizeof(char));
	memcpy(_db_p->name.s, _s->s, _s->len);
	_db_p->name.len = _s->len;

	strncpy(name, _s->s, _s->len);
	name[_s->len] = 0;

	if ((rc = km_bdblib_create_dbenv(&(_db_p->dbenv), name)) != 0)
	{
		LM_ERR("km_bdblib_create_dbenv failed");
		pkg_free(_db_p->name.s);
		pkg_free(_db_p);
		return NULL;
	}

	_db_p->tables=NULL;
	*_cachedb = _db_p;

	return _db_p;
}


/**
 * look thru a linked list for the table. if dne, create a new one
 * and add to the list
*/
tbl_cache_p km_bdblib_get_table(database_p _db, str *_s)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;

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

	_tbc = (tbl_cache_p)pkg_malloc(sizeof(tbl_cache_t));
	if(!_tbc)
		return NULL;

	if(!lock_init(&_tbc->sem))
	{
		pkg_free(_tbc);
		return NULL;
	}

	_tp = km_bdblib_create_table(_db, _s);

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("table: %.*s\n", _s->len, _s->s);
#endif

	if(!_tp)
	{
		LM_ERR("failed to create table.\n");
		pkg_free(_tbc);
		return NULL;
	}

	lock_get(&_tbc->sem);
	_tbc->dtp = _tp;

	if(_db->tables)
		(_db->tables)->prev = _tbc;
	
	_tbc->next = _db->tables;
	_db->tables = _tbc;
	lock_release(&_tbc->sem);

	return _tbc;
}


void km_bdblib_log(int op, table_p _tp, char* _msg, int len)
{
	if(!_tp || !len) 		return;
	if(! _db_parms->log_enable) 	return;
	if (_tp->logflags == JLOG_NONE)	return;
	
	if ((_tp->logflags & op) == op)
	{	int op_len=7;
		char buf[MAX_ROW_SIZE + op_len];
		char *c;
		time_t now = time(NULL);
		
		if( _db_parms->journal_roll_interval)
		{
			if((_tp->t) && (now - _tp->t) > _db_parms->journal_roll_interval)
			{	/*try to roll logfile*/
				if(km_bdblib_create_journal(_tp))
				{
					LM_ERR("Journaling has FAILED !\n");
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
table_p km_bdblib_create_table(database_p _db, str *_s)
{

	int rc,i,flags;
	DB *bdb = NULL;
	table_p tp = NULL;
	char tblname[MAX_TABLENAME_SIZE]; 

	if(!_db || !_db->dbenv)
	{
		LM_ERR("no database_p or dbenv.\n");
		return NULL;
	}

	tp = (table_p)pkg_malloc(sizeof(table_t));
	if(!tp)
	{
		LM_ERR("no private memory for table_t.\n");
		return NULL;
	}

	if ((rc = db_create(&bdb, _db->dbenv, 0)) != 0)
	{ 
		_db->dbenv->err(_db->dbenv, rc, "database create");
		LM_ERR("error in db_create, bdb error: %s.\n",db_strerror(rc));
		pkg_free(tp);
		return NULL;
	}

	memset(tblname, 0, MAX_TABLENAME_SIZE);
	strncpy(tblname, _s->s, _s->len);

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("CREATE TABLE = %s\n", tblname);
#endif

	flags = DB_THREAD;

	if ((rc = bdb->open(bdb, NULL, tblname, NULL, DB_HASH, flags, 0664)) != 0)
	{ 
		_db->dbenv->err(_db->dbenv, rc, "DB->open: %s", tblname);
		LM_ERR("bdb open failed: %s.\n",db_strerror(rc));
		pkg_free(tp);
		return NULL;
	}

	if(!lock_init(&tp->sem))
	{
		goto error;
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
	rc = km_load_metadata_columns(tp);
	if(rc!=0)
	{
		LM_ERR("FAILED to load METADATA COLS in table: %s.\n", tblname);
		goto error;
	}
	
	/*initialize columns default values from metadata*/
	rc = km_load_metadata_defaults(tp);
	if(rc!=0)
	{
		LM_ERR("FAILED to load METADATA DEFAULTS in table: %s.\n", tblname);
		goto error;
	}
	
	rc = km_load_metadata_keys(tp);
	if(rc!=0)
	{
		LM_ERR("FAILED to load METADATA KEYS in table: %s.\n", tblname);
		/*will have problems later figuring column types*/
		goto error;
	}

	/*opened RW by default; Query to set the RO flag */
	rc = km_load_metadata_readonly(tp);
	if(rc!=0)
	{
		LM_INFO("No METADATA_READONLY in table: %s.\n", tblname);
		/*non-critical; table will default to READWRITE*/
	}

	if(tp->ro)
	{	
		/*schema defines this table RO readonly*/
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("TABLE %.*s is changing to READONLY mode\n"
			, tp->name.len, tp->name.s);
#endif
		
		if ((rc = bdb->close(bdb,0)) != 0)
		{ 
			_db->dbenv->err(_db->dbenv, rc, "DB->close: %s", tblname);
			LM_ERR("bdb close: %s.\n",db_strerror(rc));
			goto error;
		}
		
		bdb = NULL;
		if ((rc = db_create(&bdb, _db->dbenv, 0)) != 0)
		{ 
			_db->dbenv->err(_db->dbenv, rc, "database create");
			LM_ERR("error in db_create.\n");
			goto error;
		}
		
		flags = DB_THREAD | DB_RDONLY;
		if ((rc = bdb->open(bdb, NULL, tblname, NULL, DB_HASH, flags, 0664)) != 0)
		{ 
			_db->dbenv->err(_db->dbenv, rc, "DB->open: %s", tblname);
			LM_ERR("bdb open: %s.\n",db_strerror(rc));
			goto error;
		}
		tp->db=bdb;
	}
	
	/* set the journaling flags; flags indicate which operations
	   need to be journalled. (e.g possible to only journal INSERT.)
	*/
	rc = km_load_metadata_logflags(tp);
	if(rc!=0)
		LM_INFO("No METADATA_LOGFLAGS in table: %s.\n", tblname);
	
	if ((tp->logflags & JLOG_FILE) == JLOG_FILE)
		km_bdblib_create_journal(tp);
	
	return tp;
	
error:
	if(tp) 
	{
		pkg_free(tp->name.s);
		pkg_free(tp);
	}
	return NULL;
}

int km_bdblib_create_journal(table_p _tp)
{
	char *s;
	char fn[1024];
	char d[64];
	FILE *fp = NULL;
	struct tm *t;
	int bl;
	database_p _db_p = *_cachedb;
	time_t tim = time(NULL);
	
	if(! _db_p || ! _tp) return -1;
	if(! _db_parms->log_enable) return 0;
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
		{	LM_ERR("Failed to Close Log in table: %.*s .\n", _tp->name.len,
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
		LM_ERR("Failed to Open Log in table: %.*s .\n",_tp->name.len, _tp->name.s);
		return -1;
	}
	
	_tp->t = tim;
	return 0;

}

int km_load_metadata_columns(table_p _tp)
{
	int ret,n,len;
	char dbuf[MAX_ROW_SIZE];
	char *s = NULL;
	char cn[64], ct[16];
	DB *db = NULL;
	DBT key, data;
	column_p col;
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
		db->err(db, ret, "km_load_metadata_columns DB->get failed");
		LM_ERR("FAILED to find METADATA_COLUMNS in DB \n");
		return -1;
	}

	/* eg: dbuf = "table_name(str) table_version(int)" */
	s = strtok(dbuf, " ");
	while(s!=NULL && n<MAX_NUM_COLS) 
	{
		/* eg: meta[0]=table_name  meta[1]=str */
		sscanf(s,"%20[^(](%10[^)])[^\n]", cn, ct);
		
		/* create column*/
		col = (column_p) pkg_malloc(sizeof(column_t));
		if(!col)
		{	LM_ERR("out of private memory \n");
			return -1;
		}
		
		/* set name*/
		len = strlen( cn );
		col->name.s = (char*)pkg_malloc(len * sizeof(char));
		memcpy(col->name.s, cn, len );
		col->name.len = len;
		
		/*set column type*/
		if(strncmp(ct, "str", 3)==0)
		{	col->type = DB1_STRING;
		}
		else if(strncmp(ct, "int", 3)==0)
		{	col->type = DB1_INT;
		}
		else if(strncmp(ct, "double", 6)==0)
		{	col->type = DB1_DOUBLE;
		}
		else if(strncmp(ct, "datetime", 8)==0)
		{	col->type = DB1_DATETIME;
		}
		else
		{	col->type = DB1_STRING;
		}
		
		col->flag = 0;
		_tp->colp[n] = col;
		n++;
		_tp->ncols++;
		s=strtok(NULL, " ");
	}

	return 0;
}

int km_load_metadata_keys(table_p _tp)
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
		db->err(db, ret, "km_load_metadata_keys DB->get failed");
		LM_ERR("FAILED to find METADATA in table \n");
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


int km_load_metadata_readonly(table_p _tp)
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

int km_load_metadata_logflags(table_p _tp)
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

int km_load_metadata_defaults(table_p _tp)
{
	int ret,n,len;
	char dbuf[MAX_ROW_SIZE];
	char *s = NULL;
	char cv[64];
	DB *db = NULL;
	DBT key, data;
	column_p col;
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
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("NO DEFAULTS ; SETTING ALL columns to NULL! \n" );
#endif

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
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("COLUMN DEFAULT is %.*s for column[%.*s] \n"
			, col->dv.len , ZSW(col->dv.s)
			, col->name.len , ZSW(col->name.s)
			);
#endif

		}
		n++;
		s=strtok(NULL, DELIM);
	}
	
	return 0;
}


/*creates a composite key _k of length _klen from n values of _v;
  provide your own initialized memory for target _k and _klen;
  resulting value: _k = "KEY1 | KEY2"
  ko = key only
*/
int km_bdblib_valtochar(table_p _tp, int* _lres, char* _k, int* _klen, db_val_t* _v, int _n, int _ko)
{
	char *p; 
	char sk[MAX_ROW_SIZE]; // subkey(sk) val
	char* delim = DELIM;
	char* cNULL = "NULL";
	int  len, total, sum;
	int i, j, k;
	p =  _k;
	len = sum = total = 0;
	i = j = k = 0;
	
	if(!_tp) return -1;
	if(!_v || (_n<1) ) return -1;
	if(!_k || !_klen ) return -1;
	if( *_klen < 1)    return -1;
	
	memset(sk, 0, MAX_ROW_SIZE);
	total = *_klen;
	*_klen = 0; //sum
	
	if(! _lres)
	{	
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("schema has NOT specified any keys! \n");
#endif

		/* schema has not specified keys
		   just use the provided data in order provided
		*/
		for(i=0;i<_n;i++)
		{	len = total - sum;
			if ( km_bdb_val2str(&_v[i], sk, &len) != 0 ) 
			{	LM_ERR("error building composite key \n");
				return -2;
			}

			sum += len;
			if(sum > total)
			{	LM_ERR("Destination buffer too short for subval %s\n",sk);
				return -2;
			} 

			/* write sk */
			strncpy(p, sk, len);
			p += len;
			*_klen = sum;

			sum += DELIM_LEN;
			if(sum > total)
			{	LM_ERR("Destination buffer too short for delim \n");
				return -3;
			}
			
			/* write delim */
			strncpy(p, delim, DELIM_LEN);
			p += DELIM_LEN;
			*_klen = sum;;
		}
		return 0;
	}


	/*
	  schema has specified keys
	  verify all schema keys are provided
	  use 'NULL' for those that are missing.
	*/
	for(i=0; i<_tp->ncols; i++)
	{	/* i indexes columns in schema order */
		if( _ko)
		{	/* keymode; skip over non-key columns */
			if( ! _tp->colp[i]->flag) 
				continue; 
		}
		
		for(j=0; j<_n; j++)
		{	
			/*
			  j indexes the columns provided in _k
			  which may be less than the total required by
			  the schema. the app does not know the order
			  of the columns in our schema!
			 */
			k = (_lres) ? _lres[j] : j;
			
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
#ifdef BDB_EXTRA_DEBUG
				LM_DBG("KEY PROVIDED[%i]: %.*s.%.*s \n", i 
					, _tp->name.len , ZSW(_tp->name.s) 
					, _tp->colp[i]->name.len, ZSW(_tp->colp[i]->name.s)
				   );
#endif

				len = total - sum;
				if ( km_bdb_val2str(&_v[j], sk, &len) != 0)
				{	LM_ERR("Destination buffer too short for subval %s\n",sk);
					return -4;
				}
				
				sum += len;
				if(sum > total)
				{	LM_ERR("Destination buffer too short for subval %s\n",sk);
					return -5;
				}

				strncpy(p, sk, len);
				p += len;
				*_klen = sum;

				sum += DELIM_LEN;
				if(sum > total)
				{	LM_ERR("Destination buffer too short for delim \n");
					return -5;
				} 
				
				/* append delim */
				strncpy(p, delim, DELIM_LEN);
				p += DELIM_LEN;
				*_klen = sum;
				
				
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
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("Missing KEY[%i]: %.*s.%.*s using default [%.*s] \n", i
			, _tp->name.len , ZSW(_tp->name.s) 
			, _tp->colp[i]->name.len, ZSW(_tp->colp[i]->name.s)
			, _tp->colp[i]->dv.len , ZSW(_tp->colp[i]->dv.s)
		   );
#endif
		len = _tp->colp[i]->dv.len;
		sum += len;
		if(sum > total)
		{	LM_ERR("Destination buffer too short for subval %s\n",cNULL);
			return -5;
		}
		
		strncpy(p, _tp->colp[i]->dv.s, len);
		p += len;
		*_klen = sum;
		
		sum += DELIM_LEN;
		if(sum > total)
		{	LM_ERR("Destination buffer too short for delim \n");
			return -5;
		} 
		
		strncpy(p, delim, DELIM_LEN);
		p += DELIM_LEN;
		*_klen = sum;
next:
		continue;
	}



	return 0;
}


/**
 *
 */
int db_free(database_p _dbp)
{
	tbl_cache_p _tbc = NULL, _tbc0=NULL;
	if(!_dbp)
		return -1;

	_tbc = _dbp->tables;

	while(_tbc)
	{
		_tbc0 = _tbc->next;
		tbl_cache_free(_tbc);
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
int tbl_cache_free(tbl_cache_p _tbc)
{
	if(!_tbc)
		return -1;
	
	lock_get(&_tbc->sem);
	
	if(_tbc->dtp)
		tbl_free(_tbc->dtp);
	
	lock_destroy(&_tbc->sem);
	pkg_free(_tbc);

	return 0;
}


/**
 * close DB (sync data to disk) and free mem
 */
int tbl_free(table_p _tp)
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

int km_bdblib_recover(table_p _tp, int _rc)
{
	switch(_rc)
	{
		case DB_LOCK_DEADLOCK:
		LM_ERR("DB_LOCK_DEADLOCK detected !!\n");
		
		case DB_RUNRECOVERY:
		LM_ERR("DB_RUNRECOVERY detected !! \n");
		km_bdblib_destroy();
		exit(1);
		break;
	}
	
	return 0;
}
