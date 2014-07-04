/*
 * $Id$
 *
 * DBText module core functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * DBText module interface
 *  
 * 2003-01-30 created by Daniel
 * 
 */

#include <string.h>

#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
 
#include "dbtext.h"
#include "dbt_res.h"
#include "dbt_api.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp"
#endif

#define DBT_ID		"dbtext://"
#define DBT_ID_LEN	(sizeof(DBT_ID)-1)
#define DBT_PATH_LEN	256
/*
 * Initialize database connection
 */
db_con_t* dbt_init(const char* _sqlurl)
{
	db_con_t* _res;
	str _s;
	char dbt_path[DBT_PATH_LEN];
	
	if (!_sqlurl) 
	{
#ifdef DBT_EXTRA_DEBUG
		LOG(L_ERR, "DBT:dbt_init: Invalid parameter value\n");
#endif
		return NULL;
	}
	_s.s = (char*)_sqlurl;
	_s.len = strlen(_sqlurl);
	if(_s.len <= DBT_ID_LEN || strncmp(_s.s, DBT_ID, DBT_ID_LEN)!=0)
	{
		LOG(L_ERR, "DBT:dbt_init: invalid database URL - should be:"
			" <%s[/]path/to/directory>\n", DBT_ID);
		return NULL;
	}
	_s.s   += DBT_ID_LEN;
	_s.len -= DBT_ID_LEN;
	if(_s.s[0]!='/')
	{
		if(sizeof(CFG_DIR)+_s.len+2 > DBT_PATH_LEN)
		{
			LOG(L_ERR, "DBT:dbt_init: path to database is too long\n");
			return NULL;
		}
		strcpy(dbt_path, CFG_DIR);
		dbt_path[sizeof(CFG_DIR)] = '/';
		strncpy(&dbt_path[sizeof(CFG_DIR)+1], _s.s, _s.len);
		_s.len += sizeof(CFG_DIR);
		_s.s = dbt_path;
	}
	
	_res = pkg_malloc(sizeof(db_con_t)+sizeof(dbt_con_t));
	if (!_res)
	{
		LOG(L_ERR, "DBT:dbt_init: No memory left\n");
		return NULL;
	}
	memset(_res, 0, sizeof(db_con_t) + sizeof(dbt_con_t));
	_res->tail = (unsigned long)((char*)_res+sizeof(db_con_t));
	
	DBT_CON_CONNECTION(_res) = dbt_cache_get_db(&_s);
	if (!DBT_CON_CONNECTION(_res))
	{
		LOG(L_ERR, "DBT:dbt_init: cannot get the link to database\n");
		return NULL;
	}

    return _res;
}


/*
 * Close a database connection
 */
void dbt_close(db_con_t* _h)
{
	if (!_h) 
	{
#ifdef DBT_EXTRA_DEBUG
		LOG(L_ERR, "DBT:dbt_close: Invalid parameter value\n");
#endif
		return;
	}
	
	if (DBT_CON_RESULT(_h)) 
		dbt_result_free(DBT_CON_RESULT(_h));
	
	pkg_free(_h);
    return;
}


/*
 * Free all memory allocated by get_result
 */
int dbt_free_query(db_con_t* _h, db_res_t* _r)
{
	if ((!_h) || (!_r))
	{
#ifdef DBT_EXTRA_DEBUG
		LOG(L_ERR, "DBT:dbt_free_query: Invalid parameter value\n");
#endif
		return -1;
	}

	if(dbt_free_result(_r) < 0) 
	{
		LOG(L_ERR,"DBT:dbt_free_query:Unable to free result structure\n");
		return -1;
	}

	
	if(dbt_result_free(DBT_CON_RESULT(_h)) < 0) 
	{
		LOG(L_ERR, "DBT:dbt_free_query: Unable to free internal structure\n");
		return -1;
	}
	DBT_CON_RESULT(_h) = NULL;
	return 0;
}


/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */

int dbt_query(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, 
			db_key_t* _c, int _n, int _nc, db_key_t _o, db_res_t** _r)
{
	tbl_cache_p _tbc = NULL;
	dbt_table_p _dtp = NULL;
	dbt_row_p _drp = NULL;
	dbt_result_p _dres = NULL;
	
	str stbl;
	int *lkey=NULL, *lres=NULL;
	
	if ((!_h) || (!_r) || !CON_TABLE(_h))
	{
#ifdef DBT_EXTRA_DEBUG
		LOG(L_ERR, "DBT:dbt_query: Invalid parameter value\n");
#endif
		return -1;
	}
	
	stbl.s = (char*)CON_TABLE(_h);
	stbl.len = strlen(CON_TABLE(_h));

	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), &stbl);
	if(!_tbc)
	{
		DBG("DBT:dbt_query: table does not exist!\n");
		return -1;
	}

	lock_get(&_tbc->sem);
	_dtp = _tbc->dtp;

	if(!_dtp || _dtp->nrcols < _nc)
	{
		DBG("DBT:dbt_query: table not loaded!\n");
		goto error;
	}
	if(_k)
	{
		lkey = dbt_get_refs(_dtp, _k, _n);
		if(!lkey)
			goto error;
	}
	if(_c)
	{
		lres = dbt_get_refs(_dtp, _c, _nc);
		if(!lres)
			goto error;
	}

	DBG("DBT:dbt_query: new res with %d cols\n", _nc);
	_dres = dbt_result_new(_dtp, lres, _nc);
	
	if(!_dres)
		goto error;
	
	_drp = _dtp->rows;
	while(_drp)
	{
		if(dbt_row_match(_dtp, _drp, lkey, _op, _v, _n))
		{
			if(dbt_result_extract_fields(_dtp, _drp, lres, _dres))
			{
				DBG("DBT:dbt_query: error extracting result fields!\n");
				goto clean;
			}
		}
		_drp = _drp->next;
	}

	dbt_table_update_flags(_dtp, DBT_TBFL_ZERO, DBT_FL_IGN, 1);
	
	lock_release(&_tbc->sem);

#ifdef DBT_EXTRA_DEBUG
	dbt_result_print(_dres);
#endif
	
	DBT_CON_RESULT(_h) = _dres;
	
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);

	return dbt_get_result(_h, _r);

error:
	lock_release(&_tbc->sem);
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	DBG("DBT:dbt_query: error while querying table!\n");
    
	return -1;

clean:
	lock_release(&_tbc->sem);
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	if(_dres)
		dbt_result_free(_dres);
	DBG("DBT:dbt_query: make clean\n");

	return -1;
}

/*
 * Raw SQL query -- is not the case to have this method
 */
int dbt_raw_query(db_con_t* _h, char* _s, db_res_t** _r)
{
	*_r = NULL;
    return -1;
}

/*
 * Insert a row into table
 */
int dbt_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	tbl_cache_p _tbc = NULL;
	dbt_table_p _dtp = NULL;
	dbt_row_p _drp = NULL;
	
	str stbl;
	int *lkey=NULL, i, j;
	
	if (!_h || !CON_TABLE(_h))
	{
#ifdef DBT_EXTRA_DEBUG
		LOG(L_ERR, "DBT:dbt_insert: Invalid parameter value\n");
#endif
		return -1;
	}
	if(!_k || !_v || _n<=0)
	{
#ifdef DBT_EXTRA_DEBUG
		DBG("DBT:dbt_insert: no key-value to insert\n");
#endif
		return -1;
	}
	
	stbl.s = (char*)CON_TABLE(_h);
	stbl.len = strlen(CON_TABLE(_h));

	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), &stbl);
	if(!_tbc)
	{
		DBG("DBT:db_insert: table does not exist!\n");
		return -1;
	}

	lock_get(&_tbc->sem);
	
	_dtp = _tbc->dtp;
	if(!_dtp)
	{
		DBG("DBT:db_insert: table does not exist!!\n");
		goto error;
	}
	
	if(_dtp->nrcols<_n)
	{
		DBG("DBT:db_insert: more values than columns!!\n");
		goto error;
	}
	
	if(_k)
	{
		lkey = dbt_get_refs(_dtp, _k, _n);
		if(!lkey)
			goto error;
	}
	_drp = dbt_row_new(_dtp->nrcols);
	if(!_drp)
	{
		DBG("DBT:db_insert: no memory for a new row!!\n");
		goto error;
	}
	
	for(i=0; i<_n; i++)
	{
		j = (lkey)?lkey[i]:i;
		if(dbt_is_neq_type(_dtp->colv[j]->type, _v[i].type))
		{
			DBG("DBT:db_insert: incompatible types v[%d] - c[%d]!\n", i, j);
			goto clean;
		}
		if(dbt_row_set_val(_drp, &(_v[i]), _v[i].type, j))
		{
			DBG("DBT:db_insert: cannot set v[%d] in c[%d]!\n", i, j);
			goto clean;
		}
		
	}

	if(dbt_table_add_row(_dtp, _drp))
	{
		DBG("DBT:db_insert: cannot insert the new row!!\n");
		goto clean;
	}

#ifdef DBT_EXTRA_DEBUG
	dbt_print_table(_dtp, NULL);
#endif
	
	lock_release(&_tbc->sem);

	if(lkey)
		pkg_free(lkey);

	DBG("DBT:db_insert: done!\n");

    return 0;
	
error:
	lock_release(&_tbc->sem);
	if(lkey)
		pkg_free(lkey);
	DBG("DBT:db_insert: error inserting row in table!\n");
    return -1;
	
clean:
	lock_release(&_tbc->sem);
	if(lkey)
		pkg_free(lkey);
	
	if(_drp) // free row
		dbt_row_free(_dtp, _drp);
	
	DBG("DBT:db_insert: make clean!\n");
    return -1;
}

/*
 * Delete a row from table
 */
int dbt_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n)
{
	tbl_cache_p _tbc = NULL;
	dbt_table_p _dtp = NULL;
	dbt_row_p _drp = NULL, _drp0 = NULL;
	int *lkey = NULL;
	str stbl;

	if (!_h || !CON_TABLE(_h))
	{
#ifdef DBT_EXTRA_DEBUG
		LOG(L_ERR, "DBT:dbt_delete: Invalid parameter value\n");
#endif
		return -1;
	}
	stbl.s = (char*)CON_TABLE(_h);
	stbl.len = strlen(CON_TABLE(_h));

	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), &stbl);
	if(!_tbc)
	{
		DBG("DBT:dbt_delete: error loading table <%s>!\n", CON_TABLE(_h));
		return -1;
	}

	lock_get(&_tbc->sem);
	_dtp = _tbc->dtp;

	if(!_dtp)
	{
		DBG("DBT:dbt_delete: table does not exist!!\n");
		goto error;
	}
	
	if(!_k || !_v || _n<=0)
	{
#ifdef DBT_EXTRA_DEBUG
		LOG(L_ERR, "DBT:dbt_delete: delete all values\n");
#endif
		dbt_table_free_rows(_dtp);
		lock_release(&_tbc->sem);
		return 0;
	}

	lkey = dbt_get_refs(_dtp, _k, _n);
	if(!lkey)
		goto error;
	
	_drp = _dtp->rows;
	while(_drp)
	{
		_drp0 = _drp->next;
		if(dbt_row_match(_dtp, _drp, lkey, _o, _v, _n))
		{
			// delete row
			DBG("DBT:dbt_delete: deleting a row!\n");
			if(_drp->prev)
				(_drp->prev)->next = _drp->next;
			else
				_dtp->rows = _drp->next;
			if(_drp->next)
				(_drp->next)->prev = _drp->prev;
			_dtp->nrrows--;
			// free row
			dbt_row_free(_dtp, _drp);
		}
		_drp = _drp0;
	}

	dbt_table_update_flags(_dtp, DBT_TBFL_MODI, DBT_FL_SET, 1);
	
#ifdef DBT_EXTRA_DEBUG
	dbt_print_table(_dtp, NULL);
#endif
	
	lock_release(&_tbc->sem);
	
	if(lkey)
		pkg_free(lkey);
	
	return 0;
	
error:
	lock_release(&_tbc->sem);
	DBG("DBT:dbt_delete: error deleting from table!\n");
    
	return -1;
}

/*
 * Update a row in table
 */
int dbt_update(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
	tbl_cache_p _tbc = NULL;
	dbt_table_p _dtp = NULL;
	dbt_row_p _drp = NULL;
	int i;	
	str stbl;
	int *lkey=NULL, *lres=NULL;

	if (!_h || !CON_TABLE(_h) || !_uk || !_uv || _un <= 0)
	{
#ifdef DBT_EXTRA_DEBUG
		LOG(L_ERR, "DBT:dbt_update: Invalid parameter value\n");
#endif
		return -1;
	}
	
	stbl.s = (char*)CON_TABLE(_h);
	stbl.len = strlen(CON_TABLE(_h));

	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), &stbl);
	if(!_tbc)
	{
		DBG("DBT:dbt_update: table does not exist!\n");
		return -1;
	}

	lock_get(&_tbc->sem);
	_dtp = _tbc->dtp;

	if(!_dtp || _dtp->nrcols < _un)
	{
		DBG("DBT:dbt_update: table not loaded or more values"
			" to update than columns!\n");
		goto error;
	}
	if(_k)
	{
		lkey = dbt_get_refs(_dtp, _k, _n);
		if(!lkey)
			goto error;
	}
	lres = dbt_get_refs(_dtp, _uk, _un);
	if(!lres)
		goto error;
	DBG("DBT:dbt_update: ---- \n");
	_drp = _dtp->rows;
	while(_drp)
	{
		if(dbt_row_match(_dtp, _drp, lkey, _o, _v, _n))
		{ // update fields
			for(i=0; i<_un; i++)
			{
				if(dbt_is_neq_type(_dtp->colv[lres[i]]->type, _uv[i].type))
				{
					DBG("DBT:dbt_update: incompatible types!\n");
					goto error;
				}
				
				if(dbt_row_update_val(_drp, &(_uv[i]), _uv[i].type, lres[i]))
				{
					DBG("DBT:dbt_update: cannot set v[%d] in c[%d]!\n",
							i, lres[i]);
					goto error;
				}
			}
		}
		_drp = _drp->next;
	}

	dbt_table_update_flags(_dtp, DBT_TBFL_MODI, DBT_FL_SET, 1);
	
#ifdef DBT_EXTRA_DEBUG
	dbt_print_table(_dtp, NULL);
#endif
	
	lock_release(&_tbc->sem);

	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);

    return 0;

error:
	lock_release(&_tbc->sem);
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	
	DBG("DBT:dbt_update: error while updating table!\n");
    
	return -1;
}

