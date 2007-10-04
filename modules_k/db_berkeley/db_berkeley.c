/*
 * $Id$
 *
 * db_berkeley module, portions of this code were templated using
 * the dbtext and postgres modules.

 * Copyright (C) 2007 Cisco Systems
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * History:
 * --------
 * 2007-09-19  genesis (wiquan)
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>


#include "../../str.h"
#include "../../mem/mem.h"

#include "../../sr_module.h"
#include "db_berkeley.h"
#include "bdb_lib.h"
#include "bdb_res.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp"
#endif

#define SC_ID		"db_berkeley://"
#define SC_ID_LEN	(sizeof(SC_ID)-1)
#define SC_PATH_LEN	256

#define SC_KEY   1
#define SC_VALUE 0

MODULE_VERSION

int auto_reload = 0;
int log_enable  = 0;
int journal_roll_interval = 0;

static int mod_init(void);
static void destroy(void);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"db_use_table",   (cmd_function)sc_use_table,  2, 0, 0},
	{"db_init",        (cmd_function)sc_init,       1, 0, 0},
	{"db_close",       (cmd_function)sc_close,      2, 0, 0},
	{"db_query",       (cmd_function)sc_query,      2, 0, 0},
	{"db_raw_query",   (cmd_function)sc_raw_query,  2, 0, 0},
	{"db_free_result", (cmd_function)sc_free_query, 2, 0, 0},
	{"db_insert",     (cmd_function)sc_insert,     2, 0, 0},
	{"db_delete",     (cmd_function)sc_delete,     2, 0, 0},
	{"db_update",     (cmd_function)sc_update,     2, 0, 0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"auto_reload",        INT_PARAM, &auto_reload },
	{"log_enable",         INT_PARAM, &log_enable  },
	{"journal_roll_interval", INT_PARAM, &journal_roll_interval  },
	{0, 0, 0}
};


struct module_exports exports = {	
	"db_berkeley",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	0,        /* exported statistics */
	0,        /* exported MI functions */
	0,        /* exported pseudo-variables */
	0,        /* extra processes */
	mod_init, /* module initialization function */
	0,        /* response function*/
	destroy,  /* destroy function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	db_parms_t p;
	
	p.auto_reload = auto_reload;
	p.log_enable = log_enable;
	p.cache_size  = (4 * 1024 * 1024); //4Mb
	p.journal_roll_interval = journal_roll_interval;
	
	if(sclib_init(&p))
		return -1;

	return 0;
}

static void destroy(void)
{
	sclib_destroy();
}

int sc_use_table(db_con_t* _h, const char* _t)
{
	if ((!_h) || (!_t))
		return -1;
	
	CON_TABLE(_h) = _t;
	return 0;
}

/*
 * Initialize database connection
 */
db_con_t* sc_init(const char* _sqlurl)
{
	db_con_t* _res;
	str _s;
	char sc_path[SC_PATH_LEN];
	
	if (!_sqlurl) 
		return NULL;
	
	_s.s = (char*)_sqlurl;
	_s.len = strlen(_sqlurl);
	if(_s.len <= SC_ID_LEN || strncmp(_s.s, SC_ID, SC_ID_LEN)!=0)
	{
		LOG(L_ERR, "sc_init: invalid database URL - should be:"
			" <%s[/]path/to/directory>\n", SC_ID);
		return NULL;
	}
	_s.s   += SC_ID_LEN;
	_s.len -= SC_ID_LEN;
	
	if(_s.s[0]!='/')
	{
		if(sizeof(CFG_DIR)+_s.len+2 > SC_PATH_LEN)
		{
			LOG(L_ERR, "sc_init: path to database is too long\n");
			return NULL;
		}
		strcpy(sc_path, CFG_DIR);
		sc_path[sizeof(CFG_DIR)] = '/';
		strncpy(&sc_path[sizeof(CFG_DIR)+1], _s.s, _s.len);
		_s.len += sizeof(CFG_DIR);
		_s.s = sc_path;
	}
	
	_res = pkg_malloc(sizeof(db_con_t)+sizeof(sc_con_t));
	if (!_res)
	{
		LOG(L_ERR, "sc_init: No memory left\n");
		return NULL;
	}
	memset(_res, 0, sizeof(db_con_t) + sizeof(sc_con_t));
	_res->tail = (unsigned long)((char*)_res+sizeof(db_con_t));
	
	SC_CON_CONNECTION(_res) = sclib_get_db(&_s);
	if (!SC_CON_CONNECTION(_res))
	{
		LOG(L_ERR, "sc_init: cannot get the link to database\n");
		return NULL;
	}

    return _res;
}


/*
 * Close a database connection
 */
void sc_close(db_con_t* _h)
{
	if(SC_CON_RESULT(_h))
		sc_free_result(SC_CON_RESULT(_h));
	pkg_free(_h);
}

/* 
 * n can be the dbenv path or a table name
*/
void sc_reload(char* _n)
{
	
#ifdef SC_EXTRA_DEBUG
	DBG("-------------------------------------------------\n");
	DBG("------- RELOAD in %s\n", _n);
	DBG("-------------------------------------------------\n");
#endif

	sclib_close(_n);
	sclib_reopen(_n);
}

/*
 * Attempts to reload a Berkeley database; reloads when the inode changes
 */
void sc_check_reload(db_con_t* _con)
{
	
	str s;
	char* p;
	int rc, len;
	struct stat st;
	database_p db;
	char n[MAX_ROW_SIZE];
	char t[MAX_TABLENAME_SIZE];
	table_p tp = NULL;
	tbl_cache_p tbc = NULL;
	
	p=n;
	rc = len = 0;
	
	/*get dbenv name*/
	db = SC_CON_CONNECTION(_con);
	if(!db->dbenv)	return;
	s.s = db->name.s;
	s.len = db->name.len;
	len+=s.len;
	
	if(len > MAX_ROW_SIZE)
	{	LOG(L_ERR, "sc_check_reload: dbenv name too long \n");
		return;
	}
	
	strncpy(p, s.s, s.len);
	p+=s.len;
	
	len++;
	if(len > MAX_ROW_SIZE)
	{	LOG(L_ERR, "sc_check_reload: dbenv name too long \n");
		return;
	}
	
	/*append slash */
	*p = '/';
	p++;
	
	/*get table name*/
	s.s = (char*)CON_TABLE(_con);
	s.len = strlen(CON_TABLE(_con));
	len+=s.len;
	
	if((len>MAX_ROW_SIZE) || (s.len > MAX_TABLENAME_SIZE) )
	{	LOG(L_ERR, "sc_check_reload: table name too long \n");
		return;
	}

	strncpy(t, s.s, s.len);
	t[s.len] = 0;
	
	strncpy(p, s.s, s.len);
	p+=s.len;
	*p=0;
	
	if( (tbc = sclib_get_table(db, &s)) == NULL)
		return;
	
	if( (tp = tbc->dtp) == NULL)
		return;
	
	DBG("sc_check_reload: stat file [%.*s]\n", len, n);
	rc = stat(n, &st);
	if(!rc)
	{	if((tp->ino!=0) && (st.st_ino != tp->ino))
			sc_reload(t); /*file changed on disk*/
		
		tp->ino = st.st_ino;
	}

}


/*
 * Free all memory allocated by get_result
 */
int sc_free_query(db_con_t* _h, db_res_t* _r)
{
	if(_r)
		sc_free_result(_r);
	if(_h)
		SC_CON_RESULT(_h) = NULL;
	return 0;
}


/*
 * Query table for specified rows
 * _con: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int sc_query(db_con_t* _con, db_key_t* _k, db_op_t* _op, db_val_t* _v, 
			db_key_t* _c, int _n, int _nc, db_key_t _o, db_res_t** _r)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	char kbuf[MAX_ROW_SIZE];
	char dbuf[MAX_ROW_SIZE];
	u_int32_t i, len, ret; 
	int klen=MAX_ROW_SIZE;
	int *lkey=NULL, *lres=NULL;
	str s;
	DBT key, data;
	DB *db;
	DBC *dbcp;

	if ((!_con) || (!_r) || !CON_TABLE(_con))
	{
#ifdef SC_EXTRA_DEBUG
		LOG(L_ERR, "sc_query: Invalid parameter value\n");
#endif
		return -1;
	}
	*_r = NULL;
	
	/*check if underlying DB file has changed inode */
	if(auto_reload)
		sc_check_reload(_con);

	s.s = (char*)CON_TABLE(_con);
	s.len = strlen(CON_TABLE(_con));

	_tbc = sclib_get_table(SC_CON_CONNECTION(_con), &s);
	if(!_tbc)
	{	DBG("sc_query: table does not exist!\n");
		return -1;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	DBG("sc_query: table not loaded!\n");
		return -1;
	}

#ifdef SC_EXTRA_DEBUG
	DBG("-------------------------------------------------\n");
	DBG("------- QUERY in %.*s\n", _tp->name.len, _tp->name.s);
	DBG("-------------------------------------------------\n");

	if (_o)  DBG("sc_query: DONT-CARE : _o: order by the specified column \n");
	if (_op) DBG("sc_query: DONT-CARE : _op: operators for refining query \n");
#endif
	
	db = _tp->db;
	if(!db) return -1;
	
	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, MAX_ROW_SIZE);
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);
	
	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;

	/* if _c is NULL and _nc is zero, you will get all table 
	   columns in the result
	*/
	if (_c)
	{	lres = sc_get_colmap(_tbc->dtp, _c, _nc);
		if(!lres)
		{	ret = -1;
			goto error;
		}
	}
	
	if(_k)
	{	lkey = sc_get_colmap(_tbc->dtp, _k, _n);
		if(!lkey) 
		{	ret = -1;
			goto error;
		}
	}
	else
	{
		DB_HASH_STAT st;
		memset(&st, 0, sizeof(DB_HASH_STAT));
		i =0 ;

#ifdef SC_EXTRA_DEBUG
		DBG("------------------------------------------------------\n");
		DBG("------- SELECT * FROM %.*s\n", _tp->name.len, _tp->name.s);
		DBG("------------------------------------------------------\n");
#endif

		/* Acquire a cursor for the database. */
		if ((ret = db->cursor(db, NULL, &dbcp, 0)) != 0) 
		{	LOG(L_ERR, "sc_query: Error creating cursor\n");
			goto error;
		}
		
		/*count the number of records*/
		while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
		{	if(!strncasecmp((char*)key.data,"METADATA",8)) 
				continue;
			i++;
		}
		
		dbcp->c_close(dbcp);
		ret=0;
		
#ifdef SC_EXTRA_DEBUG
		DBG("--- %i = SELECT COUNT(*) FROM %.*s\n", i, _tp->name.len, _tp->name.s);
#endif

		*_r = sc_result_new();
		if (!*_r) 
		{	LOG(L_ERR, "sc_query: no memory left for result \n");
			ret = -2;
			goto error;
		}
		
		if(i == 0)
		{	
			/*return empty table*/
			RES_ROW_N(*_r) = 0;
			SC_CON_RESULT(_con) = *_r;
			return 0;
		}
		
		/*allocate N rows in the result*/
		RES_ROW_N(*_r) = i;
		len  = sizeof(db_row_t) * i;
		RES_ROWS(*_r) = (db_row_t*)pkg_malloc( len );
		memset(RES_ROWS(*_r), 0, len);
		
		/*fill in the column part of db_res_t (metadata) */
		if ((ret = sc_get_columns(_tbc->dtp, *_r, lres, _nc)) < 0) 
		{	LOG(L_ERR, "sc_query: Error while getting column names\n");
			goto error;
		}
		
		/* Acquire a cursor for the database. */
		if ((ret = db->cursor(db, NULL, &dbcp, 0)) != 0) 
		{	LOG(L_ERR, "sc_query: Error creating cursor\n");
			goto error;
		}

		/*convert each record into a row in the result*/
		i =0 ;
		while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
		{
			if(!strncasecmp((char*)key.data,"METADATA",8)) 
				continue;
			
#ifdef SC_EXTRA_DEBUG
		DBG("     KEY:  [%.*s]\n     DATA: [%.*s]\n"
			, (int)   key.size
			, (char *)key.data
			, (int)   data.size
			, (char *)data.data);
#endif

			/*fill in the row part of db_res_t */
			if ((ret=sc_append_row( *_r, dbuf, lres, i)) < 0) 
			{	LOG(L_ERR, "sc_query: Error while converting row\n");
				goto error;
			}
			i++;
		}
		
		dbcp->c_close(dbcp);
		SC_CON_RESULT(_con) = *_r;
		return 0; 
	}

	if ( (ret = sclib_valtochar(_tp, lkey, kbuf, &klen, _v, _n, SC_KEY)) != 0 ) 
	{	LOG(L_ERR, "sc_query: error in query key \n");
		goto error;
	}

	key.data = kbuf;
	key.ulen = MAX_ROW_SIZE;
	key.flags = DB_DBT_USERMEM;
	key.size = klen;

	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;

	/*create an empty db_res_t which gets returned even if no result*/
	*_r = sc_result_new();
	if (!*_r) 
	{	LOG(L_ERR, "sc_convert_result: no memory left for result \n");
		ret = -2;
		goto error;
	}
	RES_ROW_N(*_r) = 0;
	SC_CON_RESULT(_con) = *_r;

#ifdef SC_EXTRA_DEBUG
		DBG("-------------------------------------------------\n");
		DBG("SELECT  KEY: [%.*s]\n"
			, (int)   key.size
			, (char *)key.data );
		DBG("-------------------------------------------------\n");
#endif

	/*query Berkely DB*/
	if ((ret = db->get(db, NULL, &key, &data, 0)) == 0) 
	{
#ifdef SC_EXTRA_DEBUG
		DBG("-------------------------------------------------\n");
		DBG("-- RESULT\n     KEY:  [%.*s]\n     DATA: [%.*s]\n"
			, (int)   key.size
			, (char *)key.data
			, (int)   data.size
			, (char *)data.data);
		DBG("-------------------------------------------------\n");
#endif

		/*fill in the col part of db_res_t */
		if ((ret = sc_get_columns(_tbc->dtp, *_r, lres, _nc)) < 0) 
		{	LOG(L_ERR, "sc_query: Error while getting column names\n");
			goto error;
		}
		/*fill in the row part of db_res_t */
		if ((ret=sc_convert_row( *_r, dbuf, lres)) < 0) 
		{	LOG(L_ERR, "sc_query: Error while converting row\n");
			goto error;
		}
		
		if(lkey)
			pkg_free(lkey);
		if(lres)
			pkg_free(lres);
	}
	else
	{	
		/*Berkeley DB error handler*/
		switch(ret)
		{
		
		case DB_NOTFOUND:
		
#ifdef SC_EXTRA_DEBUG
			DBG("------------------------------\n");
			DBG("-- NO RESULT for QUERY \n");
			DBG("------------------------------\n");
#endif
		
			ret=0;
			break;
		/*The following are all critical/fatal */
		case DB_LOCK_DEADLOCK:	
		// The operation was selected to resolve a deadlock. 
		case DB_SECONDARY_BAD:
		// A secondary index references a nonexistent primary key. 
		case DB_RUNRECOVERY:
		default:
			LOG(L_CRIT,"sc_query: DB->get error: %s.\n", db_strerror(ret));
			sclib_recover(_tp,ret);
			goto error;
		}
	}

	return ret;
	
error:
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	if(*_r) 
		sc_free_result(*_r);
	*_r = NULL;
	
	return ret;
}



/*
 * Raw SQL query
 */
int sc_raw_query(db_con_t* _h, char* _s, db_res_t** _r)
{
#ifdef SC_EXTRA_DEBUG
	DBG("-------------------------------------------------\n");
	DBG("------- Todo: Implement DB RAW QUERY \n");
	DBG("-------------------------------------------------\n");
#endif
	return -1;
}

/*
 * Insert a row into table
 */
int sc_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	char kbuf[MAX_ROW_SIZE];
	char dbuf[MAX_ROW_SIZE];
	int i, j, ret, klen, dlen;
	int *lkey=NULL;
	DBT key, data;
	DB *db;
	str s;

	i = j = ret = 0;
	klen=MAX_ROW_SIZE;
	dlen=MAX_ROW_SIZE;

	if ((!_h) || (!_v) || !CON_TABLE(_h))
	{	return -1;
	}

	if (!_k)
	{
#ifdef SC_EXTRA_DEBUG
	DBG("-------------------------------------------------\n");
	DBG("------- Todo: Implement DB INSERT w.o KEYs !! \n");
	DBG("-------------------------------------------------\n");
#endif
		return -2;
	}

	s.s = (char*)CON_TABLE(_h);
	s.len = strlen(CON_TABLE(_h));

	_tbc = sclib_get_table(SC_CON_CONNECTION(_h), &s);
	if(!_tbc)
	{	DBG("sc_insert: table does not exist!\n");
		return -3;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	DBG("sc_insert: table not loaded!\n");
		return -4;
	}

#ifdef SC_EXTRA_DEBUG
		DBG("---------------------------------------------------\n");
		DBG("------- INSERT in %.*s\n", _tp->name.len, _tp->name.s );
		DBG("---------------------------------------------------\n");
#endif
	
	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, klen);
	
	if(_tp->ncols<_n) 
	{	DBG("sc_insert: more values than columns!!\n");
		return -5;
	}

	if(_tp->ncols>_n) 
	{	DBG("sc_insert: not enough values(%i) to fill the columns(%i) !!\n", _n, _tp->ncols);
		return -6;
	}
	

	lkey = sc_get_colmap(_tp, _k, _n);
	if(!lkey)  return -7;

	/* verify col types provided */
	for(i=0; i<_n; i++)
	{	j = (lkey)?lkey[i]:i;
		if(sc_is_neq_type(_tp->colp[j]->type, _v[i].type))
		{
			DBG("sc_insert: incompatible types v[%d] - c[%d]!\n", i, j);
			ret = -8;
			goto error;
		}
	}
	
	/* make the key */
	if ( (ret = sclib_valtochar(_tp, lkey, kbuf, &klen, _v, _n, SC_KEY)) != 0 ) 
	{	LOG(L_ERR, "sc_insert: error in sclib_valtochar  \n");
		ret = -9;
		goto error;
	}
	
	key.data = kbuf;
	key.ulen = MAX_ROW_SIZE;
	key.flags = DB_DBT_USERMEM;
	key.size = klen;

	//make the value (row)
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);

	if ( (ret = sclib_valtochar(_tp, lkey, dbuf, &dlen, _v, _n, SC_VALUE)) != 0 ) 
	{	LOG(L_ERR, "sc_insert: error in sclib_valtochar \n");
		ret = -9;
		goto error;
	}

	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;
	data.size = dlen;

	if ((ret = db->put(db, NULL, &key, &data, 0)) == 0) 
	{
		sclib_log(JLOG_INSERT, _tp, dbuf, dlen);

#ifdef SC_EXTRA_DEBUG
	DBG("-------------------------------------------------\n");
	DBG("-- INSERT\n     KEY:  [%.*s]\n     DATA: [%.*s]\n"
		, (int)   key.size
		, (char *)key.data
		, (int)   data.size
		, (char *)data.data);
	DBG("-------------------------------------------------\n");
#endif
	}
	else
	{	/*Berkeley DB error handler*/
		switch(ret)
		{
		/*The following are all critical/fatal */
		case DB_LOCK_DEADLOCK:	
		/* The operation was selected to resolve a deadlock. */ 
		
		case DB_RUNRECOVERY:
		default:
			LOG(L_CRIT, "sc_insert: DB->put error: %s.\n", db_strerror(ret));
			sclib_recover(_tp, ret);
			goto error;
		}
	}

	return 0;
	
error:
	if(lkey)
		pkg_free(lkey);
	
	return ret;

}

/*
 * Delete a row from table
 *
 * To delete ALL rows:
 *   do Not specify any keys, or values, and _n <=0
 *
 */
int sc_delete(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	char kbuf[MAX_ROW_SIZE];
	int i, j, ret, klen;
	int *lkey=NULL;
	DBT key;
	DB *db;
	DBC *dbcp;
	str s;

	i = j = ret = 0;
	klen=MAX_ROW_SIZE;

	if (_op)
		return ( _sc_delete_cursor(_h, _k, _op, _v, _n) );

	if ((!_h) || !CON_TABLE(_h))
		return -1;

	s.s = (char*)CON_TABLE(_h);
	s.len = strlen(CON_TABLE(_h));

	_tbc = sclib_get_table(SC_CON_CONNECTION(_h), &s);
	if(!_tbc)
	{	DBG("sc_delete: table does not exist!\n");
		return -3;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	DBG("sc_delete: table not loaded!\n");
		return -4;
	}

#ifdef SC_EXTRA_DEBUG
		DBG("-------------------------------------------------\n");
		DBG("------- DELETE in %.*s\n", _tp->name.len, _tp->name.s );
		DBG("-------------------------------------------------\n");
#endif

	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, klen);

	if(!_k || !_v || _n<=0)
	{
		/* Acquire a cursor for the database. */
		if ((ret = db->cursor(db, NULL, &dbcp, DB_WRITECURSOR) ) != 0) 
		{	LOG(L_ERR, "sc_query: Error creating cursor\n");
			goto error;
		}
		
		while ((ret = dbcp->c_get(dbcp, &key, NULL, DB_NEXT)) == 0)
		{
			if(!strncasecmp((char*)key.data,"METADATA",8)) 
				continue;
#ifdef SC_EXTRA_DEBUG
			DBG("     KEY: [%.*s]\n "
				, (int)   key.size
				, (char *)key.data);
#endif
			ret = dbcp->c_del(dbcp, 0);
		}
		
		dbcp->c_close(dbcp);
		return 0;
	}

	lkey = sc_get_colmap(_tp, _k, _n);
	if(!lkey)  return -5;

	/* make the key */
	if ( (ret = sclib_valtochar(_tp, lkey, kbuf, &klen, _v, _n, SC_KEY)) != 0 ) 
	{	LOG(L_ERR, "sc_delete: error in sclib_makekey  \n");
		ret = -6;
		goto error;
	}

	key.data = kbuf;
	key.ulen = MAX_ROW_SIZE;
	key.flags = DB_DBT_USERMEM;
	key.size = klen;

	if ((ret = db->del(db, NULL, &key, 0)) == 0)
	{
		sclib_log(JLOG_DELETE, _tp, kbuf, klen);

#ifdef SC_EXTRA_DEBUG
		DBG("-------------------------------------------------\n");
		DBG("-- DELETED ROW \n KEY: %s \n", (char *)key.data);
		DBG("-------------------------------------------------\n");
#endif
	}
	else
	{	/*Berkeley DB error handler*/
		switch(ret){
			
		case DB_NOTFOUND:
			ret = 0;
			break;
			
		/*The following are all critical/fatal */
		case DB_LOCK_DEADLOCK:	
		/* The operation was selected to resolve a deadlock. */ 
		case DB_SECONDARY_BAD:
		/* A secondary index references a nonexistent primary key. */
		case DB_RUNRECOVERY:
		default:
			LOG(L_CRIT,"sc_delete: DB->del error: %s.\n"
				, db_strerror(ret));
			sclib_recover(_tp, ret);
			goto error;
		}
	}

	ret = 0;
	
error:
	if(lkey)
		pkg_free(lkey);
	
	return ret;

}

/*
_sc_delete_cursor -- called from sc_delete when the query involves operators 
  other than equal '='. Adds support for queries like this:
	DELETE from SomeTable WHERE _k[0] < _v[0]
  In this case, the keys _k are not the actually schema keys, so we need to 
  iterate via cursor to perform this operation.
*/
int _sc_delete_cursor(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	db_res_t* _r   = NULL;
	char kbuf[MAX_ROW_SIZE];
	char dbuf[MAX_ROW_SIZE];
	int i, ret, klen=MAX_ROW_SIZE;
	DBT key, data;
	DB *db;
	DBC *dbcp;
	int *lkey=NULL;
	str s;
	
	i = ret = 0;
	
	if ((!_h) || !CON_TABLE(_h))
		return -1;

	s.s = (char*)CON_TABLE(_h);
	s.len = strlen(CON_TABLE(_h));

	_tbc = sclib_get_table(SC_CON_CONNECTION(_h), &s);
	if(!_tbc)
	{	DBG("_sc_delete_cursor: table does not exist!\n");
		return -3;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	DBG("_sc_delete_cursor: table not loaded!\n");
		return -4;
	}
	
#ifdef SC_EXTRA_DEBUG
	DBG("-------------------------------------------------\n");
	DBG("------- DELETE by cursor in %.*s\n", _tp->name.len, _tp->name.s );
	DBG("-------------------------------------------------\n");
#endif

	if(_k)
	{	lkey = sc_get_colmap(_tp, _k, _n);
		if(!lkey) 
		{	ret = -1;
			goto error;
		}
	}
	
	/* create an empty db_res_t which gets returned even if no result */
	_r = sc_result_new();
	if (!_r) 
	{	LOG(L_ERR, "_sc_delete_cursor: no memory for result \n");
	}
	
	RES_ROW_N(_r) = 0;
	
	/* fill in the col part of db_res_t */
	if ((ret = sc_get_columns(_tp, _r, 0, 0)) != 0) 
	{	LOG(L_ERR, "_sc_delete_cursor: Error while getting column names\n");
		goto error;
	}
	
	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, klen);
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);
	
	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;
	
	/* Acquire a cursor for the database. */
	if ((ret = db->cursor(db, NULL, &dbcp, DB_WRITECURSOR)) != 0) 
	{	LOG(L_ERR, "_sc_delete_cursor: Error creating cursor\n");
	}
	
	while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
	{
		if(!strncasecmp((char*)key.data,"METADATA",8))
			continue;
		
		/*fill in the row part of db_res_t */
		if ((ret=sc_convert_row( _r, dbuf, 0)) < 0) 
		{	LOG(L_ERR, "_sc_delete_cursor: Error while converting row\n");
			goto error;
		}
		
		if(sc_row_match(_k, _op, _v, _n, _r, lkey ))
		{

#ifdef SC_EXTRA_DEBUG
			DBG("[_sc_delete_cursor] DELETE ROW by KEY:  [%.*s]\n"
				, (int) key.size, (char *)key.data);
#endif

			if((ret = dbcp->c_del(dbcp, 0)) != 0)
			{	
				/* Berkeley DB error handler */
				LOG(L_CRIT,"_sc_delete_cursor: DB->get error: %s.\n"
					, db_strerror(ret));
				sclib_recover(_tp,ret);
			}
			
		}
		
		sc_free_rows( _r);
	}
	ret = 0;
	
error:
	if(dbcp)
		dbcp->c_close(dbcp);
	if(_r)
		sc_free_result(_r);
	if(lkey)
		pkg_free(lkey);
	
	return ret;
}

/*
 * Updates a row in table
 * Limitation: only knows how to update a single row
 *
 * _con: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _uk: update keys; cols that need to be updated 
 * _uv: update values; col values that need to be commited
 * _un: number of rows to update
 */
int sc_update(db_con_t* _con, db_key_t* _k, db_op_t* _op, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
	str s;
	char *c, *t;
	int ret, i, qcol, len, sum;
	int *lkey=NULL;
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	char kbuf[MAX_ROW_SIZE];
	char qbuf[MAX_ROW_SIZE];
	char ubuf[MAX_ROW_SIZE];
	DBT key, qdata, udata;
	DB *db;
	
	sum = ret = i = qcol = len = 0;
	
	if (!_con || !CON_TABLE(_con) || !_uk || !_uv || _un <= 0)
		return -1;
	
	s.s = (char*)CON_TABLE(_con);
	s.len = strlen(CON_TABLE(_con));

	_tbc = sclib_get_table(SC_CON_CONNECTION(_con), &s);
	if(!_tbc)
	{	LOG(L_ERR, "ERROR: sc_update:: table does not exist\n");
		return -1;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	LOG(L_ERR, "ERROR: sc_update:: table not loaded\n");
		return -1;
	}
	
	db = _tp->db;
	if(!db)
	{	LOG(L_ERR, "ERROR: sc_update:: DB null ptr\n");
		return -1;
	}
	
#ifdef SC_EXTRA_DEBUG
	DBG("-------------------------------------------------\n");
	DBG("-- UPDATE in %.*s\n", _tp->name.len, _tp->name.s);
	DBG("-------------------------------------------------\n");
	if (_op) DBG("sc_update: DONT-CARE : _op: operators for refining query \n");
#endif
	
	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, MAX_ROW_SIZE);
	memset(&qdata, 0, sizeof(DBT));
	memset(qbuf, 0, MAX_ROW_SIZE);
	
	qdata.data = qbuf;
	qdata.ulen = MAX_ROW_SIZE;
	qdata.flags = DB_DBT_USERMEM;
	
	if(_k)
	{	lkey = sc_get_colmap(_tbc->dtp, _k, _n);
		if(!lkey) return -4;
	}
	else
	{
		LOG(L_ERR, "ERROR: sc_update:: Null keys in update _k=0 \n");
		return -1;
	}
	
	len = MAX_ROW_SIZE;
	
	if ( (ret = sclib_valtochar(_tp, lkey, kbuf, &len, _v, _n, SC_KEY)) != 0 ) 
	{	LOG(L_ERR, "sc_update: error in query key \n");
		goto cleanup;
	}
	
	if(lkey) pkg_free(lkey);
	
	key.data = kbuf;
	key.ulen = MAX_ROW_SIZE;
	key.flags = DB_DBT_USERMEM;
	key.size = len;
	
	/*stage 1: QUERY Berkely DB*/
	if ((ret = db->get(db, NULL, &key, &qdata, 0)) == 0) 
	{

#ifdef SC_EXTRA_DEBUG
		DBG("---1 uRESULT\n     KEY:  [%.*s]\n     DATA: [%.*s]\n"
			, (int)   key.size
			, (char *)key.data
			, (int)   qdata.size
			, (char *)qdata.data);
#endif

	}
	else
	{	goto db_error;
	}
	
	/* stage 2: UPDATE row with new values */
	
	/* map the provided keys to those in our schema */ 
	lkey = sc_get_colmap(_tbc->dtp, _uk, _un);
	if(!lkey) return -4;
	
	/* build a new row for update data (udata) */
	memset(&udata, 0, sizeof(DBT));
	memset(ubuf, 0, MAX_ROW_SIZE);
	
	/* loop over each column of the qbuf and copy it to our new ubuf unless
	   its a field that needs to update
	*/
	c = strtok(qbuf, DELIM);
	t = ubuf;
	while( c!=NULL)
	{	char* delim = DELIM;
		int k;
		
		len = strlen(c);
		sum+=len;
		
		if(sum > MAX_ROW_SIZE)
		{	LOG(L_ERR, "sc_update: value too long for string \n");
			ret = -3;
			goto cleanup;
		}
		
		for(i=0;i<_un;i++)
		{
			k = lkey[i];
			if (qcol == k)
			{	/* update this col */
				int j = MAX_ROW_SIZE - sum;
				if( sc_val2str( &_uv[i], t, &j) )
				{	LOG(L_ERR, "sc_update: value too long for string \n");
					ret = -3;
					goto cleanup;
				}

				goto next;
			}
			
		}
		
		/* copy original column to the new column */
		strncpy(t, c, len);

next:
		t+=len;
		
		/* append DELIM */
		sum += DELIM_LEN;
		if(sum > MAX_ROW_SIZE)
		{	LOG(L_ERR, "sc_update: value too long for string \n");
			ret = -3;
			goto cleanup;
		}
		
		strncpy(t, delim, DELIM_LEN);
		t += DELIM_LEN;
		
		c = strtok(NULL, DELIM);
		qcol++;
	}
	
	ubuf[sum]  = '0';
	udata.data = ubuf;
	udata.ulen  = MAX_ROW_SIZE;
	udata.flags = DB_DBT_USERMEM;
	udata.size  = sum;

#ifdef SC_EXTRA_DEBUG
	DBG("---2 MODIFIED Data\n     KEY:  [%.*s]\n     DATA: [%.*s]\n"
		, (int)   key.size
		, (char *)key.data
		, (int)   udata.size
		, (char *)udata.data);
#endif
	/* stage 3: DELETE old row using key*/
	if ((ret = db->del(db, NULL, &key, 0)) == 0)
	{
#ifdef SC_EXTRA_DEBUG
		DBG("---3 uDELETED ROW \n KEY: %s \n", (char *)key.data);
#endif
	}
	else
	{	goto db_error;
	}
	
	/* stage 4: INSERT new row with key*/
	if ((ret = db->put(db, NULL, &key, &udata, 0)) == 0) 
	{
		sclib_log(JLOG_UPDATE, _tp, ubuf, sum);
#ifdef SC_EXTRA_DEBUG
	DBG("---4 INSERT \n     KEY:  [%.*s]\n     DATA: [%.*s]\n"
		, (int)   key.size
		, (char *)key.data
		, (int)   udata.size
		, (char *)udata.data);
#endif
	}
	else
	{	goto db_error;
	}

#ifdef SC_EXTRA_DEBUG
	DBG("-------------------------------------------------\n");
	DBG("-- UPDATE COMPLETE \n");
	DBG("-------------------------------------------------\n");
#endif


cleanup:
	if(lkey)
		pkg_free(lkey);
	
	return ret;


db_error:

	/*Berkeley DB error handler*/
	switch(ret)
	{
	
	case DB_NOTFOUND:
	
#ifdef SC_EXTRA_DEBUG
		DBG("------------------------------\n");
		DBG("--- NO RESULT \n");
		DBG("------------------------------\n");
#endif
		return -1;
	
	/* The following are all critical/fatal */
	case DB_LOCK_DEADLOCK:	
	/* The operation was selected to resolve a deadlock. */
	case DB_SECONDARY_BAD:
	/* A secondary index references a nonexistent primary key.*/ 
	case DB_RUNRECOVERY:
	default:
		LOG(L_CRIT,"sc_update: DB->get error: %s.\n", db_strerror(ret));
		sclib_recover(_tp,ret);
	}
	
	if(lkey)
		pkg_free(lkey);
	
	return ret;
}
