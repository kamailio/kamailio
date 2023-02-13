/*
 * DBText module core functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#include <string.h>

#include "../../core/str.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"

#include "db_text.h"
#include "dbt_res.h"
#include "dbt_api.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp"
#endif

#define DBT_ID		"text://"
#define DBT_ID_LEN	(sizeof(DBT_ID)-1)
#define DBT_PATH_LEN	256
/*
 * Initialize database connection
 */
db1_con_t* dbt_init(const str* _sqlurl)
{
	db1_con_t* _res;
	str _s;
	char dbt_path[DBT_PATH_LEN];

	if (!_sqlurl || !_sqlurl->s)
	{
		LM_ERR("invalid parameter value\n");
		return NULL;
	}
	LM_DBG("initializing for db url: [%.*s]\n", _sqlurl->len, _sqlurl->s);
	_s.s = _sqlurl->s;
	_s.len = _sqlurl->len;
	if(_s.len <= DBT_ID_LEN || strncmp(_s.s, DBT_ID, DBT_ID_LEN)!=0)
	{
		LM_ERR("invalid database URL - should be:"
			" <%s[/]path/to/directory> Current: %s\n", DBT_ID, _s.s);
		return NULL;
	}
	/*
	 * it would be possible to use the _sqlurl here, but the core API is
	 * defined with a const str*, so this code would be not valid.
	 */
	_s.s   += DBT_ID_LEN;
	_s.len -= DBT_ID_LEN;
	if(_s.s[0]!='/')
	{
		if(sizeof(CFG_DIR)+_s.len+2 >= DBT_PATH_LEN)
		{
			LM_ERR("path to database is too long\n");
			return NULL;
		}
		strcpy(dbt_path, CFG_DIR);
		if(dbt_path[sizeof(CFG_DIR)-2]!='/') {
			dbt_path[sizeof(CFG_DIR)-1] = '/';
			strncpy(&dbt_path[sizeof(CFG_DIR)], _s.s, _s.len);
			_s.len += sizeof(CFG_DIR);
		} else {
			strncpy(&dbt_path[sizeof(CFG_DIR)-1], _s.s, _s.len);
			_s.len += sizeof(CFG_DIR) - 1;
		}
		_s.s = dbt_path;
		LM_DBG("updated db url: [%.*s]\n", _s.len, _s.s);
	}

	_res = pkg_malloc(sizeof(db1_con_t)+sizeof(dbt_con_t));
	if (!_res)
	{
		LM_ERR("no pkg memory left\n");
		return NULL;
	}
	memset(_res, 0, sizeof(db1_con_t) + sizeof(dbt_con_t));
	_res->tail = (unsigned long)((char*)_res+sizeof(db1_con_t));

	DBT_CON_CONNECTION(_res) = dbt_cache_get_db(&_s);
	if (!DBT_CON_CONNECTION(_res))
	{
		LM_ERR("cannot get the link to database\n");
		pkg_free(_res);
		return NULL;
	}

	return _res;
}


/*
 * Close a database connection
 */
void dbt_close(db1_con_t* _h)
{
	if (!_h)
	{
		LM_ERR("invalid parameter value\n");
		return;
	}

	pkg_free(_h);
	return;
}


/*
 * Free all memory allocated by get_result
 */
int dbt_free_result(db1_con_t* _h, db1_res_t* _r)
{
	if ((!_h))
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (!_r)
		return 0;

	if(dbt_result_free(_h, (dbt_table_p)_r->ptr) < 0)
	{
		LM_ERR("unable to free internal structure\n");
	}

	if(db_free_result(_r) < 0)
	{
		LM_ERR("unable to free result structure\n");
		return -1;
	}

	return 0;
}

static dbt_table_p last_temp_table = NULL; 

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

int dbt_query(db1_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v,
			db_key_t* _c, int _n, int _nc, db_key_t _o, db1_res_t** _r)
{
	dbt_table_p _tbc = NULL;
	dbt_table_p _tbc_temp = NULL;
	dbt_row_p _drp = NULL;
	dbt_row_p *_res = NULL;
//	dbt_result_p _dres = NULL;
	int result = 0;
	int counter = 0;
	int i=0;

	int *lkey=NULL, *lres=NULL;

	db_key_t *_o_k=NULL;    /* columns in order-by */
	char *_o_op=NULL;       /* operators for oder-by */
	int _o_n;               /* no of elements in order-by */
	int *_o_l=NULL;         /* column selection for order-by */
//	int _o_nc;              /* no of elements in _o_l but not lres */

	if(_r)
		*_r = NULL;

	if ((!_h) || !CON_TABLE(_h))
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if (_o)
	{
		if (dbt_parse_orderbyclause(&_o_k, &_o_op, &_o_n, _o) < 0)
			return -1;
	}

	_tbc_temp = dbt_db_get_temp_table(DBT_CON_CONNECTION(_h));
	if(!_tbc_temp)
	{
		LM_ERR("unable to allocate temp table\n");
		if(_o_op) pkg_free(_o_op);
		return -1;
	}

	/* lock database */
	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
	if(!_tbc)
	{
		LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		dbt_db_del_table(DBT_CON_CONNECTION(_h), &_tbc_temp->name, 0);
		if(_o_op) pkg_free(_o_op);
		return -1;
	}


	if(_tbc->nrcols < _nc)
	{
		LM_ERR("table %s - too few columns (%d < %d)\n", CON_TABLE(_h)->s,
				_tbc->nrcols, _nc);
		goto error;
	}
	if(_k)
	{
		lkey = dbt_get_refs(_tbc, _k, _n);
		if(!lkey)
			goto error;
	}
	if(_c)
	{
		lres = dbt_get_refs(_tbc, _c, _nc);
		if(!lres)
			goto error;
	}
	if(_o_k)
	{
		_o_l = dbt_get_refs(_tbc, _o_k, _o_n);
		if (!_o_l)
			goto error;
		/* enlarge select-columns lres by all order-by columns, _o_nc is how many */
//		if (dbt_mangle_columnselection(&lres, &_nc, &_o_nc, _o_l, _o_n) < 0)
//			goto error;
	}

/*
	LM_DBG("new res with %d cols\n", _nc);
	_dres = dbt_result_new(_tbc, lres, _nc);

	if(!_dres)
		goto error;
*/

		dbt_column_p pPrevCol = NULL;
		_tbc_temp->colv = (dbt_column_p*) shm_malloc(_nc*sizeof(dbt_column_p));
		for(i=0; lres && i < _nc; i++) {
			dbt_column_p pCol = dbt_column_new(_tbc->colv[ lres[i] ]->name.s, _tbc->colv[ lres[i] ]->name.len);
			pCol->type = _tbc->colv[ lres[i] ]->type;
			pCol->flag = _tbc->colv[ lres[i] ]->flag;
			if(pPrevCol)
			{
				pCol->prev = pPrevCol;
				pPrevCol->next = pCol;
			}
			else
				_tbc_temp->cols = pCol;

			_tbc_temp->colv[i] = pCol;
			pPrevCol = pCol;
			_tbc_temp->nrcols++;
		}

	_res = (dbt_row_p*) pkg_malloc(_db_text_max_result_rows * sizeof(dbt_row_p));
	if(!_res) {
		LM_ERR("no more space to allocate for query rows\n");
		goto error;
	}


	_drp = _tbc->rows;
	while(_drp && counter < _db_text_max_result_rows)
	{
		if(dbt_row_match(_tbc, _drp, lkey, _op, _v, _n))
		{
			_res[counter] = _drp;
//			if(dbt_result_extract_fields(_tbc, _drp, lres, _dres))
//			{
//				LM_ERR("failed to extract result fields!\n");
//				goto clean;
//			}
			counter++;
		}
		_drp = _drp->next;
	}
	if (_drp && counter == _db_text_max_result_rows)
	{
		LM_ERR("Truncated table at [%d] rows. Please increase 'max_result_rows' param!\n", counter);
		goto error;
	}

	if (_o_l)
	{
		if (counter > 1)
		{
			if (dbt_sort_result_temp(_res, counter, _o_l, _o_op, _o_n) < 0)
				goto error;
		}

		/* last but not least, remove surplus columns */
//		if (_o_nc)
//			dbt_project_result(_dres, _o_nc);
	}

	// copy results to temp table
	_tbc_temp->rows = dbt_result_extract_results(_tbc, _res, counter, lres, _nc);
	_tbc_temp->nrrows = (_tbc_temp->rows == NULL ? 0 : counter);

	dbt_table_update_flags(_tbc, DBT_TBFL_ZERO, DBT_FL_IGN, 1);

	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

// 	DBT_CON_TEMP_TABLE(_h) = _tbc_temp;
	last_temp_table = _tbc_temp;
//	dbt_release_table(DBT_CON_CONNECTION(_h), &_tbc_temp->name);

//	dbt_result_print(_tbc_temp);

	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	if(_o_k)
 		pkg_free(_o_k);
 	if(_o_op)
 		pkg_free(_o_op);
 	if(_o_l)
 		pkg_free(_o_l);
 	if(_res)
 		pkg_free(_res);

 	if(_r) {
 		result = dbt_get_result(_r, _tbc_temp);
// 		dbt_db_del_table(DBT_CON_CONNECTION(_h), &_tbc_temp->name, 1);
		if(result != 0)
 			dbt_result_free(_h, _tbc_temp);
 	}

	return result;

error:
    /* unlock database */
    dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
    /* delete temp table */
    dbt_db_del_table(DBT_CON_CONNECTION(_h), &_tbc_temp->name, 1);
	if(_res)
		pkg_free(_res);
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	if(_o_k)
		pkg_free(_o_k);
	if(_o_op)
		pkg_free(_o_op);
	if(_o_l)
		pkg_free(_o_l);
	LM_ERR("failed to query the table!\n");

	return -1;

/*
clean:
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	if(_o_k)
		pkg_free(_o_k);
	if(_o_op)
		pkg_free(_o_op);
	if(_o_l)
		pkg_free(_o_l);

	return -1;
*/
}


int dbt_fetch_result(db1_con_t* _h, db1_res_t** _r, const int nrows)
{
	int rows;

	if (!_h || !_r || nrows < 0) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	/* exit if the fetch count is zero */
	if (nrows == 0) {
		dbt_free_result(_h, *_r);
		*_r = 0;
		return 0;
	}

	if(*_r==0) {
		/* Allocate a new result structure */
		dbt_init_result(_r, last_temp_table);
	} else {
		/* free old rows */
		if(RES_ROWS(*_r)!=0)
			db_free_rows(*_r);
		RES_ROWS(*_r) = 0;
		RES_ROW_N(*_r) = 0;
	}

	/* determine the number of rows remaining to be processed */
	rows = RES_NUM_ROWS(*_r) - RES_LAST_ROW(*_r);

	/* If there aren't any more rows left to process, exit */
	if(rows<=0)
		return 0;

	/* if the fetch count is less than the remaining rows to process                 */
	/* set the number of rows to process (during this call) equal to the fetch count */
	if(nrows < rows)
		rows = nrows;
	
	RES_ROW_N(*_r) = rows;

	return dbt_get_next_result(_r, RES_LAST_ROW(*_r), rows);
}

/*
 * Affected Rows
 */
int dbt_affected_rows(db1_con_t* _h)
{
	if (!_h || !CON_TABLE(_h))
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	return ((dbt_con_p)_h->tail)->affected;
}

/*
 * Insert a row into table
 */
int dbt_insert(db1_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	dbt_table_p _tbc = NULL;
	dbt_row_p _drp = NULL;

	int *lkey=NULL, i, j;

	if (!_h || !CON_TABLE(_h))
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	((dbt_con_p)_h->tail)->affected = 0;

	if(!_k || !_v || _n<=0)
	{
		LM_ERR("no key-value to insert\n");
		return -1;
	}

	/* lock database */
	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
	if(!_tbc)
	{
		LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		return -1;
	}

	if(_tbc->nrcols<_n)
	{
		LM_ERR("more values than columns!!\n");
		goto error;
	}

	if(_k)
	{
		lkey = dbt_get_refs(_tbc, _k, _n);
		if(!lkey)
			goto error;
	}
	_drp = dbt_row_new(_tbc->nrcols);
	if(!_drp)
	{
		LM_ERR("no shm memory for a new row!!\n");
		goto error;
	}

	for(i=0; i<_n; i++)
	{
		j = (lkey)?lkey[i]:i;
		if(dbt_is_neq_type(_tbc->colv[j]->type, _v[i].type))
		{
			LM_ERR("incompatible types v[%d] - c[%d]!\n", i, j);
			goto clean;
		}
		if(_v[i].type == DB1_STRING && !_v[i].nul)
			_v[i].val.str_val.len = strlen(_v[i].val.string_val);
		if(dbt_row_set_val(_drp, &(_v[i]), _tbc->colv[j]->type, j))
		{
			LM_ERR("cannot set v[%d] in c[%d]!\n", i, j);
			goto clean;
		}

	}

	if(dbt_table_add_row(_tbc, _drp))
	{
		LM_ERR("cannot insert the new row!!\n");
		goto clean;
	}

	((dbt_con_p)_h->tail)->affected = 1;

	/* dbt_print_table(_tbc, NULL); */

	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	if(lkey)
		pkg_free(lkey);

	return 0;

error:
	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
	if(lkey)
		pkg_free(lkey);
	LM_ERR("failed to insert row in table!\n");
	return -1;

clean:
	if(lkey)
		pkg_free(lkey);

	if(_drp) // free row
		dbt_row_free(_tbc, _drp);
	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	return -1;
}

/*
 * Delete a row from table
 */
int dbt_delete(db1_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n)
{
	dbt_table_p _tbc = NULL;
	dbt_row_p _drp = NULL, _drp0 = NULL;
	int *lkey = NULL;

	if (!_h || !CON_TABLE(_h))
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	((dbt_con_p)_h->tail)->affected = 0;

	/* lock database */
	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
	if(!_tbc)
	{
		LM_ERR("failed to load table <%.*s>!\n", CON_TABLE(_h)->len,
				CON_TABLE(_h)->s);
		return -1;
	}

	if(!_k || !_v || _n<=0)
	{
		LM_DBG("deleting all records\n");
		((dbt_con_p)_h->tail)->affected = _tbc->nrrows;
		dbt_table_free_rows(_tbc);
		/* unlock database */
		dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
		return 0;
	}

	lkey = dbt_get_refs(_tbc, _k, _n);
	if(!lkey)
		goto error;

	_drp = _tbc->rows;
	while(_drp)
	{
		_drp0 = _drp->next;
		if(dbt_row_match(_tbc, _drp, lkey, _o, _v, _n))
		{
			// delete row
			if(_drp->prev)
				(_drp->prev)->next = _drp->next;
			else
				_tbc->rows = _drp->next;
			if(_drp->next)
				(_drp->next)->prev = _drp->prev;
			_tbc->nrrows--;
			// free row
			dbt_row_free(_tbc, _drp);

			((dbt_con_p)_h->tail)->affected++;

		}
		_drp = _drp0;
	}

	if( ((dbt_con_p)_h->tail)->affected )
		dbt_table_update_flags(_tbc, DBT_TBFL_MODI, DBT_FL_SET, 1);

	/* dbt_print_table(_tbc, NULL); */

	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	if(lkey)
		pkg_free(lkey);

	return 0;

error:
	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	LM_ERR("failed to delete from table!\n");
	return -1;
}

/*
 * Update a row in table
 */
int dbt_update(db1_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
		db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
	dbt_table_p _tbc = NULL;
	dbt_row_p _drp = NULL;
	int i;
	int *lkey=NULL, *lres=NULL;

	if (!_h || !CON_TABLE(_h) || !_uk || !_uv || _un <= 0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	((dbt_con_p)_h->tail)->affected = 0;

	/* lock database */
	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
	if(!_tbc)
	{
		LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		return -1;
	}

	if(_k)
	{
		lkey = dbt_get_refs(_tbc, _k, _n);
		if(!lkey)
			goto error;
	}
	lres = dbt_get_refs(_tbc, _uk, _un);
	if(!lres)
		goto error;
	_drp = _tbc->rows;
	while(_drp)
	{
		if(dbt_row_match(_tbc, _drp, lkey, _o, _v, _n))
		{ // update fields
			for(i=0; i<_un; i++)
			{
				if(dbt_is_neq_type(_tbc->colv[lres[i]]->type, _uv[i].type))
				{
					LM_ERR("incompatible types!\n");
					goto error;
				}

				if(dbt_row_update_val(_drp, &(_uv[i]),
							_tbc->colv[lres[i]]->type, lres[i]))
				{
					LM_ERR("cannot set v[%d] in c[%d]!\n",
							i, lres[i]);
					goto error;
				}
			}

			((dbt_con_p)_h->tail)->affected++;

		}
		_drp = _drp->next;
	}

	if( ((dbt_con_p)_h->tail)->affected )
		dbt_table_update_flags(_tbc, DBT_TBFL_MODI, DBT_FL_SET, 1);

	/* dbt_print_table(_tbc, NULL); */

	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);

	return 0;

error:
	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);

	LM_ERR("failed to update the table!\n");

	return -1;
}

int dbt_replace(db1_con_t* _h, db_key_t* _k, db_val_t* _v,
		int _n, int _nk, int _m)
{
	dbt_table_p _tbc = NULL;
	dbt_row_p _drp = NULL;
	int i, j;
	int *lkey=NULL, *lres=NULL;

	if (!_h || !CON_TABLE(_h) || _nk <= 0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	((dbt_con_p)_h->tail)->affected = 0;

	/* lock database */
	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
	if(!_tbc)
	{
		LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		return -1;
	}

	if(_k)
	{
		lkey = dbt_get_refs(_tbc, _k, _nk);
		if(!lkey)
			goto error;
	}
	lres = dbt_get_refs(_tbc, _k, _n);
	if(!lres)
		goto error;
	_drp = _tbc->rows;
	while(_drp)
	{
		if(dbt_row_match(_tbc, _drp, lkey, NULL, _v, _nk))
		{ // update fields
			for(i=0; i<_n; i++)
			{
				if(dbt_is_neq_type(_tbc->colv[lres[i]]->type, _v[i].type))
				{
					LM_ERR("incompatible types!\n");
					goto error;
				}

				if(dbt_row_update_val(_drp, &(_v[i]),
							_tbc->colv[lres[i]]->type, lres[i]))
				{
					LM_ERR("cannot set v[%d] in c[%d]!\n",
							i, lres[i]);
					goto error;
				}
			}

			((dbt_con_p)_h->tail)->affected++;
			break;

		}
		_drp = _drp->next;
	}

	if(((dbt_con_p)_h->tail)->affected == 0) {
		_drp = dbt_row_new(_tbc->nrcols);
		if(!_drp)
		{
			LM_ERR("no shm memory for a new row!!\n");
			goto error;
		}

		for(i=0; i<_n; i++)
		{
			j = lres[i];
			if(dbt_is_neq_type(_tbc->colv[j]->type, _v[i].type))
			{
				LM_ERR("incompatible types v[%d] - c[%d]!\n", i, j);
				goto error;
			}
			if(_v[i].type == DB1_STRING && !_v[i].nul)
				_v[i].val.str_val.len = strlen(_v[i].val.string_val);
			if(dbt_row_set_val(_drp, &(_v[i]), _tbc->colv[j]->type, j))
			{
				LM_ERR("cannot set v[%d] in c[%d]!\n", i, j);
				goto error;
			}
		}

		if(dbt_table_add_row(_tbc, _drp))
		{
			LM_ERR("cannot insert the new row!!\n");
			goto error;
		}

		((dbt_con_p)_h->tail)->affected = 1;

	}

	if( ((dbt_con_p)_h->tail)->affected )
		dbt_table_update_flags(_tbc, DBT_TBFL_MODI, DBT_FL_SET, 1);

	/* dbt_print_table(_tbc, NULL); */

	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);

	return 0;

error:

	if(lkey)
		pkg_free(lkey);
	if(lres)
		pkg_free(lres);
	if(_drp) // free row
		dbt_row_free(_tbc, _drp);

	/* unlock database */
	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	LM_ERR("failed to update the table!\n");

	return -1;
}


