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
#include <unistd.h>
#include <sys/stat.h>


#include "../../str.h"
#include "../../ut.h"
#include "../../mem/mem.h"

#include "../../sr_module.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_query.h"
#include "km_db_berkeley.h"
#include "km_bdb_lib.h"
#include "km_bdb_res.h"
#include "km_bdb_mi.h"
#include "bdb_mod.h"
#include "bdb_crs_compat.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp"
#endif

#define BDB_ID		"berkeley://"
#define BDB_ID_LEN	(sizeof(BDB_ID)-1)
#define BDB_PATH_LEN	256

#define BDB_KEY   1
#define BDB_VALUE 0

/*MODULE_VERSION*/

int bdb_bind_api(db_func_t *dbb);

/*
 * Exported functions
 */
static kam_cmd_export_t cmds[] = {
	{"db_bind_api",    (cmd_function)bdb_bind_api,   0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
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

/*
 * Exported MI functions
 */
static mi_export_t mi_cmds[] = {
	{ MI_BDB_RELOAD, mi_bdb_reload, 0, 0, 0 },
	{ 0, 0, 0, 0, 0}
};

struct kam_module_exports kam_exports = {	
	"db_berkeley",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	0,        /* exported statistics */
	mi_cmds,  /* exported MI functions */
	0,        /* exported pseudo-variables */
	0,        /* extra processes */
	km_mod_init, /* module initialization function */
	0,        /* response function*/
	km_destroy,  /* destroy function */
	0         /* per-child init function */
};

int km_mod_init(void)
{
	db_parms_t p;
	
	if(register_mi_mod(kam_exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	p.auto_reload = auto_reload;
	p.log_enable = log_enable;
	p.cache_size  = (4 * 1024 * 1024); //4Mb
	p.journal_roll_interval = journal_roll_interval;
	
	if(km_bdblib_init(&p))
		return -1;

	return 0;
}

void km_destroy(void)
{
	km_bdblib_destroy();
}

int bdb_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table   = bdb_use_table;
	dbb->init        = bdb_init;
	dbb->close       = bdb_close;
	dbb->query       = (db_query_f)km_bdb_query;
	dbb->free_result = bdb_free_query;
	dbb->insert      = (db_insert_f)bdb_insert;
	dbb->delete      = (db_delete_f)bdb_delete; 
	dbb->update      = (db_update_f)bdb_update;

	return 0;
}

int bdb_use_table(db1_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}

/*
 * Initialize database connection
 */
db1_con_t* bdb_init(const str* _sqlurl)
{
	db1_con_t* _res;
	str _s;
	char bdb_path[BDB_PATH_LEN];
	
	if (!_sqlurl || !_sqlurl->s) {
		LM_ERR("invalid parameter value\n");
		return 0;
	}
	
	_s.s = _sqlurl->s;
	_s.len = _sqlurl->len;
	if(_s.len <= BDB_ID_LEN || strncmp(_s.s, BDB_ID, BDB_ID_LEN)!=0)
	{
		LM_ERR("invalid database URL - should be:"
			" <%s[/]path/to/directory>\n", BDB_ID);
		return NULL;
	}
	_s.s   += BDB_ID_LEN;
	_s.len -= BDB_ID_LEN;
	
	if(_s.s[0]!='/')
	{
		if(sizeof(CFG_DIR)+_s.len+2 > BDB_PATH_LEN)
		{
			LM_ERR("path to database is too long\n");
			return NULL;
		}
		strcpy(bdb_path, CFG_DIR);
		bdb_path[sizeof(CFG_DIR)] = '/';
		strncpy(&bdb_path[sizeof(CFG_DIR)+1], _s.s, _s.len);
		_s.len += sizeof(CFG_DIR);
		_s.s = bdb_path;
	}
	
	_res = pkg_malloc(sizeof(db1_con_t)+sizeof(bdb_con_t));
	if (!_res)
	{
		LM_ERR("No private memory left\n");
		return NULL;
	}
	memset(_res, 0, sizeof(db1_con_t) + sizeof(bdb_con_t));
	_res->tail = (unsigned long)((char*)_res+sizeof(db1_con_t));

	LM_INFO("using database at: %.*s\n", _s.len, _s.s);
	BDB_CON_CONNECTION(_res) = km_bdblib_get_db(&_s);
	if (!BDB_CON_CONNECTION(_res))
	{
		LM_ERR("cannot get the link to database\n");
		return NULL;
	}

    return _res;
}


/*
 * Close a database connection
 */
void bdb_close(db1_con_t* _h)
{
	if(BDB_CON_RESULT(_h))
		db_free_result(BDB_CON_RESULT(_h));
	pkg_free(_h);
}

/* 
 * n can be the dbenv path or a table name
*/
int bdb_reload(char* _n)
{
	int rc = 0;
#ifdef BDB_EXTRA_DEBUG
	LM_DBG("[bdb_reload] Initiate RELOAD in %s\n", _n);
#endif

	if ((rc = km_bdblib_close(_n)) != 0) 
	{	LM_ERR("[bdb_reload] Error while closing db_berkeley DB.\n");
		return rc;
	}

	if ((rc = km_bdblib_reopen(_n)) != 0) 
	{	LM_ERR("[bdb_reload] Error while reopening db_berkeley DB.\n");
		return rc;
	}

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("[bdb_reload] RELOAD successful in %s\n", _n);
#endif

	return rc;
}

/*
 * Attempts to reload a Berkeley database; reloads when the inode changes
 */
void bdb_check_reload(db1_con_t* _con)
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
	db = BDB_CON_CONNECTION(_con);
	if(!db->dbenv)	return;
	s.s = db->name.s;
	s.len = db->name.len;
	len+=s.len;
	
	if(len > MAX_ROW_SIZE)
	{	LM_ERR("dbenv name too long \n");
		return;
	}
	
	strncpy(p, s.s, s.len);
	p+=s.len;
	
	len++;
	if(len > MAX_ROW_SIZE)
	{	LM_ERR("dbenv name too long \n");
		return;
	}
	
	/*append slash */
	*p = '/';
	p++;
	
	/*get table name*/
	s.s = CON_TABLE(_con)->s;
	s.len = CON_TABLE(_con)->len;
	len+=s.len;
	
	if((len>MAX_ROW_SIZE) || (s.len > MAX_TABLENAME_SIZE) )
	{	LM_ERR("table name too long \n");
		return;
	}

	strncpy(t, s.s, s.len);
	t[s.len] = 0;
	
	strncpy(p, s.s, s.len);
	p+=s.len;
	*p=0;
	
	if( (tbc = km_bdblib_get_table(db, &s)) == NULL)
		return;
	
	if( (tp = tbc->dtp) == NULL)
		return;
	
	LM_DBG("stat file [%.*s]\n", len, n);
	rc = stat(n, &st);
	if(!rc)
	{	if((tp->ino!=0) && (st.st_ino != tp->ino))
			bdb_reload(t); /*file changed on disk*/
		
		tp->ino = st.st_ino;
	}

}


/*
 * Free all memory allocated by get_result
 */
int bdb_free_query(db1_con_t* _h, db1_res_t* _r)
{
	if(_r)
		db_free_result(_r);
	if(_h)
		BDB_CON_RESULT(_h) = NULL;
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
int km_bdb_query(db1_con_t* _con, db_key_t* _k, db_op_t* _op, db_val_t* _v, 
			db_key_t* _c, int _n, int _nc, db_key_t _o, db1_res_t** _r)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	char kbuf[MAX_ROW_SIZE];
	char dbuf[MAX_ROW_SIZE];
	u_int32_t i, len, ret; 
	int klen=MAX_ROW_SIZE;
	int *lkey=NULL, *lres=NULL;
	DBT key, data;
	DB *db;
	DBC *dbcp;

	if ((!_con) || (!_r) || !CON_TABLE(_con))
	{
#ifdef BDB_EXTRA_DEBUG
		LM_ERR("Invalid parameter value\n");
#endif
		return -1;
	}
	*_r = NULL;
	
	/*check if underlying DB file has changed inode */
	if(auto_reload)
		bdb_check_reload(_con);

	_tbc = km_bdblib_get_table(BDB_CON_CONNECTION(_con), (str*)CON_TABLE(_con));
	if(!_tbc)
	{	LM_WARN("table does not exist!\n");
		return -1;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	LM_WARN("table not loaded!\n");
		return -1;
	}

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("QUERY in %.*s\n", _tp->name.len, _tp->name.s);

	if (_o)  LM_DBG("DONT-CARE : _o: order by the specified column \n");
	if (_op) LM_DBG("DONT-CARE : _op: operators for refining query \n");
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
	{	lres = bdb_get_colmap(_tbc->dtp, _c, _nc);
		if(!lres)
		{	ret = -1;
			goto error;
		}
	}
	
	if(_k)
	{	lkey = bdb_get_colmap(_tbc->dtp, _k, _n);
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

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("SELECT * FROM %.*s\n", _tp->name.len, _tp->name.s);
#endif

		/* Acquire a cursor for the database. */
		if ((ret = db->cursor(db, NULL, &dbcp, 0)) != 0) 
		{	LM_ERR("Error creating cursor\n");
			goto error;
		}
		
		/*count the number of records*/
		while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
		{	if(!strncasecmp((char*)key.data,"METADATA",8)) 
				continue;
			i++;
		}
		
		dbcp->CLOSE_CURSOR(dbcp);
		ret=0;
		
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("%i = SELECT COUNT(*) FROM %.*s\n", i, _tp->name.len, _tp->name.s);
#endif

		*_r = db_new_result();
		if (!*_r) 
		{	LM_ERR("no memory left for result \n");
			ret = -2;
			goto error;
		}
		
		if(i == 0)
		{	
			/*return empty table*/
			RES_ROW_N(*_r) = 0;
			BDB_CON_RESULT(_con) = *_r;
			return 0;
		}
		
		/*allocate N rows in the result*/
		RES_ROW_N(*_r) = i;
		len  = sizeof(db_row_t) * i;
		RES_ROWS(*_r) = (db_row_t*)pkg_malloc( len );
		memset(RES_ROWS(*_r), 0, len);
		
		/*fill in the column part of db1_res_t (metadata) */
		if ((ret = bdb_get_columns(_tbc->dtp, *_r, lres, _nc)) < 0) 
		{	LM_ERR("Error while getting column names\n");
			goto error;
		}
		
		/* Acquire a cursor for the database. */
		if ((ret = db->cursor(db, NULL, &dbcp, 0)) != 0) 
		{	LM_ERR("Error creating cursor\n");
			goto error;
		}

		/*convert each record into a row in the result*/
		i =0 ;
		while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
		{
			if(!strncasecmp((char*)key.data,"METADATA",8)) 
				continue;
			
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("KEY: [%.*s]\nDATA: [%.*s]\n"
			, (int)   key.size
			, (char *)key.data
			, (int)   data.size
			, (char *)data.data);
#endif

			/*fill in the row part of db1_res_t */
			if ((ret=bdb_append_row( *_r, dbuf, lres, i)) < 0) 
			{	LM_ERR("Error while converting row\n");
				goto error;
			}
			i++;
		}
		
		dbcp->CLOSE_CURSOR(dbcp);
		BDB_CON_RESULT(_con) = *_r;
		return 0; 
	}

	if ( (ret = km_bdblib_valtochar(_tp, lkey, kbuf, &klen, _v, _n, BDB_KEY)) != 0 ) 
	{	LM_ERR("error in query key \n");
		goto error;
	}

	key.data = kbuf;
	key.ulen = MAX_ROW_SIZE;
	key.flags = DB_DBT_USERMEM;
	key.size = klen;

	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;

	/*create an empty db1_res_t which gets returned even if no result*/
	*_r = db_new_result();
	if (!*_r) 
	{	LM_ERR("no memory left for result \n");
		ret = -2;
		goto error;
	}
	RES_ROW_N(*_r) = 0;
	BDB_CON_RESULT(_con) = *_r;

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("SELECT  KEY: [%.*s]\n"
			, (int)   key.size
			, (char *)key.data );
#endif

	/*query Berkely DB*/
	if ((ret = db->get(db, NULL, &key, &data, 0)) == 0) 
	{
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("RESULT\nKEY:  [%.*s]\nDATA: [%.*s]\n"
			, (int)   key.size
			, (char *)key.data
			, (int)   data.size
			, (char *)data.data);
#endif

		/*fill in the col part of db1_res_t */
		if ((ret = bdb_get_columns(_tbc->dtp, *_r, lres, _nc)) < 0) 
		{	LM_ERR("Error while getting column names\n");
			goto error;
		}
		/*fill in the row part of db1_res_t */
		if ((ret=bdb_convert_row( *_r, dbuf, lres)) < 0) 
		{	LM_ERR("Error while converting row\n");
			goto error;
		}
		
	}
	else
	{	
		/*Berkeley DB error handler*/
		switch(ret)
		{
		
		case DB_NOTFOUND:
		
#ifdef BDB_EXTRA_DEBUG
			LM_DBG("NO RESULT for QUERY \n");
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
			LM_CRIT("DB->get error: %s.\n", db_strerror(ret));
			km_bdblib_recover(_tp,ret);
			goto error;
		}
	}
	
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	
	return ret;
	
error:
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	if(*_r) 
		db_free_result(*_r);
	*_r = NULL;
	
	return ret;
}



/*
 * Raw SQL query
 */
int bdb_raw_query(db1_con_t* _h, char* _s, db1_res_t** _r)
{
	LM_CRIT("DB RAW QUERY not implemented!\n");
	return -1;
}

/*
 * Insert a row into table
 */
int bdb_insert(db1_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	char kbuf[MAX_ROW_SIZE];
	char dbuf[MAX_ROW_SIZE];
	int i, j, ret, klen, dlen;
	int *lkey=NULL;
	DBT key, data;
	DB *db;

	i = j = ret = 0;
	klen=MAX_ROW_SIZE;
	dlen=MAX_ROW_SIZE;

	if ((!_h) || (!_v) || !CON_TABLE(_h))
	{	return -1;
	}

	if (!_k)
	{
#ifdef BDB_EXTRA_DEBUG
	LM_ERR("DB INSERT without KEYs not implemented! \n");
#endif
		return -2;
	}

	_tbc = km_bdblib_get_table(BDB_CON_CONNECTION(_h), (str*)CON_TABLE(_h));
	if(!_tbc)
	{	LM_WARN("table does not exist!\n");
		return -3;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	LM_WARN("table not loaded!\n");
		return -4;
	}

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("INSERT in %.*s\n", _tp->name.len, _tp->name.s );
#endif
	
	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, klen);
	
	if(_tp->ncols<_n) 
	{	LM_WARN("more values than columns!!\n");
		return -5;
	}

	lkey = bdb_get_colmap(_tp, _k, _n);
	if(!lkey)  return -7;

	/* verify col types provided */
	for(i=0; i<_n; i++)
	{	j = (lkey)?lkey[i]:i;
		if(bdb_is_neq_type(_tp->colp[j]->type, _v[i].type))
		{
			LM_WARN("incompatible types v[%d] - c[%d]!\n", i, j);
			ret = -8;
			goto error;
		}
	}
	
	/* make the key */
	if ( (ret = km_bdblib_valtochar(_tp, lkey, kbuf, &klen, _v, _n, BDB_KEY)) != 0 ) 
	{	LM_ERR("Error in km_bdblib_valtochar  \n");
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

	if ( (ret = km_bdblib_valtochar(_tp, lkey, dbuf, &dlen, _v, _n, BDB_VALUE)) != 0 ) 
	{	LM_ERR("Error in km_bdblib_valtochar \n");
		ret = -9;
		goto error;
	}

	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;
	data.size = dlen;

	if ((ret = db->put(db, NULL, &key, &data, 0)) == 0) 
	{
		km_bdblib_log(JLOG_INSERT, _tp, dbuf, dlen);

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("INSERT\nKEY:  [%.*s]\nDATA: [%.*s]\n"
		, (int)   key.size
		, (char *)key.data
		, (int)   data.size
		, (char *)data.data);
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
			LM_CRIT("DB->put error: %s.\n", db_strerror(ret));
			km_bdblib_recover(_tp, ret);
			goto error;
		}
	}
	
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
int bdb_delete(db1_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	char kbuf[MAX_ROW_SIZE];
	int ret, klen;
	int *lkey=NULL;
	DBT key;
	DB *db;
	DBC *dbcp;

	ret = 0;
	klen=MAX_ROW_SIZE;

	if (_op)
		return ( _bdb_delete_cursor(_h, _k, _op, _v, _n) );

	if ((!_h) || !CON_TABLE(_h))
		return -1;

	_tbc = km_bdblib_get_table(BDB_CON_CONNECTION(_h), (str*)CON_TABLE(_h));
	if(!_tbc)
	{	LM_WARN("table does not exist!\n");
		return -3;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	LM_WARN("table not loaded!\n");
		return -4;
	}

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("DELETE in %.*s\n", _tp->name.len, _tp->name.s );
#endif

	db = _tp->db;
	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, klen);

	if(!_k || !_v || _n<=0)
	{
		/* Acquire a cursor for the database. */
		if ((ret = db->cursor(db, NULL, &dbcp, DB_WRITECURSOR) ) != 0) 
		{	LM_ERR("Error creating cursor\n");
			goto error;
		}
		
		while ((ret = dbcp->c_get(dbcp, &key, NULL, DB_NEXT)) == 0)
		{
			if(!strncasecmp((char*)key.data,"METADATA",8)) 
				continue;
#ifdef BDB_EXTRA_DEBUG
			LM_DBG("KEY: [%.*s]\n"
				, (int)   key.size
				, (char *)key.data);
#endif
			ret = dbcp->c_del(dbcp, 0);
		}
		
		dbcp->CLOSE_CURSOR(dbcp);
		return 0;
	}

	lkey = bdb_get_colmap(_tp, _k, _n);
	if(!lkey)  return -5;

	/* make the key */
	if ( (ret = km_bdblib_valtochar(_tp, lkey, kbuf, &klen, _v, _n, BDB_KEY)) != 0 ) 
	{	LM_ERR("Error in bdblib_makekey\n");
		ret = -6;
		goto error;
	}

	key.data = kbuf;
	key.ulen = MAX_ROW_SIZE;
	key.flags = DB_DBT_USERMEM;
	key.size = klen;

	if ((ret = db->del(db, NULL, &key, 0)) == 0)
	{
		km_bdblib_log(JLOG_DELETE, _tp, kbuf, klen);

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("DELETED ROW \n KEY: %s \n", (char *)key.data);
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
			LM_CRIT("DB->del error: %s.\n"
				, db_strerror(ret));
			km_bdblib_recover(_tp, ret);
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
_bdb_delete_cursor -- called from bdb_delete when the query involves operators 
  other than equal '='. Adds support for queries like this:
	DELETE from SomeTable WHERE _k[0] < _v[0]
  In this case, the keys _k are not the actually schema keys, so we need to 
  iterate via cursor to perform this operation.
*/
int _bdb_delete_cursor(db1_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n)
{
	tbl_cache_p _tbc = NULL;
	table_p _tp = NULL;
	db1_res_t* _r   = NULL;
	char kbuf[MAX_ROW_SIZE];
	char dbuf[MAX_ROW_SIZE];
	int ret, klen=MAX_ROW_SIZE;
	DBT key, data;
	DB *db;
	DBC *dbcp;
	int *lkey=NULL;
	
	ret = 0;
	
	if ((!_h) || !CON_TABLE(_h))
		return -1;

	_tbc = km_bdblib_get_table(BDB_CON_CONNECTION(_h), (str*)CON_TABLE(_h));
	if(!_tbc)
	{	LM_WARN("table does not exist!\n");
		return -3;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	LM_WARN("table not loaded!\n");
		return -4;
	}
	
#ifdef BDB_EXTRA_DEBUG
	LM_DBG("DELETE by cursor in %.*s\n", _tp->name.len, _tp->name.s );
#endif

	if(_k)
	{	lkey = bdb_get_colmap(_tp, _k, _n);
		if(!lkey) 
		{	ret = -1;
			goto error;
		}
	}
	
	/* create an empty db1_res_t which gets returned even if no result */
	_r = db_new_result();
	if (!_r) 
	{	LM_ERR("no memory for result \n");
	}
	
	RES_ROW_N(_r) = 0;
	
	/* fill in the col part of db1_res_t */
	if ((ret = bdb_get_columns(_tp, _r, 0, 0)) != 0) 
	{	LM_ERR("Error while getting column names\n");
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
	{	LM_ERR("Error creating cursor\n");
	}
	
	while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
	{
		if(!strncasecmp((char*)key.data,"METADATA",8))
			continue;
		
		/*fill in the row part of db1_res_t */
		if ((ret=bdb_convert_row( _r, dbuf, 0)) < 0) 
		{	LM_ERR("Error while converting row\n");
			goto error;
		}
		
		if(bdb_row_match(_k, _op, _v, _n, _r, lkey ))
		{

#ifdef BDB_EXTRA_DEBUG
			LM_DBG("DELETE ROW by KEY:  [%.*s]\n", (int) key.size, 
				(char *)key.data);
#endif

			if((ret = dbcp->c_del(dbcp, 0)) != 0)
			{	
				/* Berkeley DB error handler */
				LM_CRIT("DB->get error: %s.\n", db_strerror(ret));
				km_bdblib_recover(_tp,ret);
			}
			
		}
		
		memset(dbuf, 0, MAX_ROW_SIZE);
		db_free_rows( _r);
	}
	ret = 0;
	
error:
	if(dbcp)
		dbcp->CLOSE_CURSOR(dbcp);
	if(_r)
		db_free_result(_r);
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
int bdb_update(db1_con_t* _con, db_key_t* _k, db_op_t* _op, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
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

	_tbc = km_bdblib_get_table(BDB_CON_CONNECTION(_con), (str*)CON_TABLE(_con));
	if(!_tbc)
	{	LM_ERR("table does not exist\n");
		return -1;
	}

	_tp = _tbc->dtp;
	if(!_tp)
	{	LM_ERR("table not loaded\n");
		return -1;
	}
	
	db = _tp->db;
	if(!db)
	{	LM_ERR("DB null ptr\n");
		return -1;
	}
	
#ifdef BDB_EXTRA_DEBUG
	LM_DBG("UPDATE in %.*s\n", _tp->name.len, _tp->name.s);
	if (_op) LM_DBG("DONT-CARE : _op: operators for refining query \n");
#endif
	
	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, MAX_ROW_SIZE);
	memset(&qdata, 0, sizeof(DBT));
	memset(qbuf, 0, MAX_ROW_SIZE);
	
	qdata.data = qbuf;
	qdata.ulen = MAX_ROW_SIZE;
	qdata.flags = DB_DBT_USERMEM;
	
	if(_k)
	{	lkey = bdb_get_colmap(_tbc->dtp, _k, _n);
		if(!lkey) return -4;
	}
	else
	{
		LM_ERR("Null keys in update _k=0 \n");
		return -1;
	}
	
	len = MAX_ROW_SIZE;
	
	if ( (ret = km_bdblib_valtochar(_tp, lkey, kbuf, &len, _v, _n, BDB_KEY)) != 0 ) 
	{	LM_ERR("Error in query key \n");
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

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("RESULT\nKEY:  [%.*s]\nDATA: [%.*s]\n"
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
	lkey = bdb_get_colmap(_tbc->dtp, _uk, _un);
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
		{	LM_ERR("value too long for string \n");
			ret = -3;
			goto cleanup;
		}
		
		for(i=0;i<_un;i++)
		{
			k = lkey[i];
			if (qcol == k)
			{	/* update this col */
				int j = MAX_ROW_SIZE - sum;
				if( km_bdb_val2str( &_uv[i], t, &j) )
				{	LM_ERR("value too long for string \n");
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
		{	LM_ERR("value too long for string \n");
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

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("MODIFIED Data\nKEY:  [%.*s]\nDATA: [%.*s]\n"
		, (int)   key.size
		, (char *)key.data
		, (int)   udata.size
		, (char *)udata.data);
#endif
	/* stage 3: DELETE old row using key*/
	if ((ret = db->del(db, NULL, &key, 0)) == 0)
	{
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("DELETED ROW\nKEY: %s \n", (char *)key.data);
#endif
	}
	else
	{	goto db_error;
	}
	
	/* stage 4: INSERT new row with key*/
	if ((ret = db->put(db, NULL, &key, &udata, 0)) == 0) 
	{
		km_bdblib_log(JLOG_UPDATE, _tp, ubuf, sum);
#ifdef BDB_EXTRA_DEBUG
	LM_DBG("INSERT \nKEY:  [%.*s]\nDATA: [%.*s]\n"
		, (int)   key.size
		, (char *)key.data
		, (int)   udata.size
		, (char *)udata.data);
#endif
	}
	else
	{	goto db_error;
	}

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("UPDATE COMPLETE \n");
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
	
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("NO RESULT \n");
#endif
		return -1;
	
	/* The following are all critical/fatal */
	case DB_LOCK_DEADLOCK:	
	/* The operation was selected to resolve a deadlock. */
	case DB_SECONDARY_BAD:
	/* A secondary index references a nonexistent primary key.*/ 
	case DB_RUNRECOVERY:
	default:
		LM_CRIT("DB->get error: %s.\n", db_strerror(ret));
		km_bdblib_recover(_tp,ret);
	}
	
	if(lkey)
		pkg_free(lkey);
	
	return ret;
}
