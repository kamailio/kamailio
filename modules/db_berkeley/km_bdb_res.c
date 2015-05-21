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
#include <sys/types.h>
#include "../../mem/mem.h"
#include "km_bdb_res.h"


int bdb_get_columns(table_p _tp, db1_res_t* _res, int* _lres, int _nc)
{
	int col;

	if (!_res) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (_nc < 0 ) {
		LM_ERR("_nc parameter cannot be negative \n");
		return -1;
	}
    /* the number of rows (tuples) in the query result. */
	RES_NUM_ROWS(_res) = 1;

	if (!_lres)
		_nc = _tp->ncols;

	/* Save number of columns in the result structure */
	RES_COL_N(_res) = _nc;

	if (db_allocate_columns(_res, RES_COL_N(_res)) != 0) {
		LM_ERR("could not allocate columns");
		return -2;
	}

	/*
	 * For each column both the name and the data type are saved.
	 */
	for(col = 0; col < RES_COL_N(_res); col++) {
		column_p cp = NULL;
		cp = (_lres) ? _tp->colp[_lres[col]] : _tp->colp[col];

		RES_NAMES(_res)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_res)[col]) {
			LM_ERR("no private memory left\n");
			db_free_columns(_res);
			return -3;
		}
		LM_DBG("allocate %lu bytes for RES_NAMES[%d] at %p\n",
			(unsigned long)sizeof(str), col, RES_NAMES(_res)[col]);

		/* The pointer that is here returned is part of the result structure. */
		RES_NAMES(_res)[col]->s = cp->name.s;
		RES_NAMES(_res)[col]->len = cp->name.len;

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(_res)[col]
			, col, RES_NAMES(_res)[col]->len, RES_NAMES(_res)[col]->s);

		RES_TYPES(_res)[col] = cp->type;
	}
	return 0;
}



/**
 * Convert rows from Berkeley DB to db API representation
 */
int bdb_convert_row(db1_res_t* _res, char *bdb_result, int* _lres)
{
	int col, len, i, j;
	char **row_buf, *s;
	db_row_t* row = NULL;
	col = len = i = j = 0;
	
	if (!_res) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	/* Allocate a single row structure */
	len = sizeof(db_row_t);
	row = (db_row_t*)pkg_malloc(len);
	if (!row) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate %d bytes for row %p\n", len, row);
	memset(row, 0, len);
	RES_ROWS(_res) = row;
	
	/* Save the number of rows in the current fetch */
	RES_ROW_N(_res) = 1;

	if (db_allocate_row(_res, row) != 0) {
		LM_ERR("could not allocate row");
		return -2;
	}
	/*
	 * Allocate an array of pointers one per column.
	 * It that will be used to hold the address of the string representation of each column.
	 */
	len = sizeof(char *) * RES_COL_N(_res);
	row_buf = (char **)pkg_malloc(len);
	if (!row_buf) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate for %d columns %d bytes in row buffer at %p\n", RES_COL_N(_res), len, row_buf);
	memset(row_buf, 0, len);

	/*populate the row_buf with bdb_result*/
	/*bdb_result is memory from our callers stack so we copy here*/
	s = strtok(bdb_result, DELIM);
	while( s!=NULL)
	{
		if(_lres) {
			/*only requested cols (_c was specified)*/
			for(i=0; i<ROW_N(row); i++)
			{	if (col == _lres[i]) {
					len = strlen(s);
					row_buf[i] = pkg_malloc(len+1);
					if (!row_buf[i]) {
						LM_ERR("no private memory left\n");
						return -1;
					}
					LM_DBG("allocated %d bytes for row_buf[%d] at %p\n", len, i, row_buf[i]);
					memset(row_buf[i], 0, len+1);
					strncpy(row_buf[i], s, len);
				}
				
			}
		}
		else {
			len = strlen(s);
			row_buf[col] = pkg_malloc(len+1);
			if (!row_buf[col]) {
				LM_ERR("no private memory left\n");
				return -1;
			}
				LM_DBG("allocated %d bytes for row_buf[%d] at %p\n", len, col, row_buf[col]);
			memset(row_buf[col], 0, len+1);
			strncpy(row_buf[col], s, len);
		}
		s = strtok(NULL, DELIM);
		col++;
	}

	/*do the type conversion per col*/
        for(col = 0; col < ROW_N(row); col++) {
		/*skip the unrequested cols (as already specified)*/
		if(!row_buf[col])  continue;

		/* Convert the string representation into the value representation */
		if (bdb_str2val(RES_TYPES(_res)[col], &(ROW_VALUES(row)[col])
				, row_buf[col], strlen(row_buf[col])) < 0) {
			LM_ERR("while converting value\n");
			LM_DBG("freeing row at %p\n", row);
			db_free_row(row);
			return -3;
		}
	}

	/* pkg_free() must be done for the above allocations now that the row has been converted.
	 * During bdb_convert_row (and subsequent bdb_str2val) processing, data types that don't need to be
	 * converted (namely STRINGS) have their addresses saved.  These data types should not have
	 * their pkg_malloc() allocations freed here because they are still needed.  However, some data types
	 * (ex: INT, DOUBLE) should have their pkg_malloc() allocations freed because during the conversion
	 * process, their converted values are saved in the union portion of the db_val_t structure.
	 *
	 * Warning: when the converted row is no longer needed, the data types whose addresses
	 * were saved in the db_val_t structure must be freed or a memory leak will happen.
	 * This processing should happen in the db_free_row() subroutine.  The caller of
	 * this routine should ensure that db_free_rows(), db_free_row() or db_free_result()
	 * is eventually called.
	 */
	for (col = 0; col < RES_COL_N(_res); col++) {
		switch (RES_TYPES(_res)[col]) 
		{
			case DB1_STRING:
			case DB1_STR:
				break;
			default:
			LM_DBG("col[%d] Col[%.*s] Type[%d] Freeing row_buf[%p]\n", col
				, RES_NAMES(_res)[col]->len, RES_NAMES(_res)[col]->s,
				  RES_TYPES(_res)[col], row_buf[col]);
			LM_DBG("freeing row_buf[%d] at %p\n", col, row_buf[col]);
			pkg_free(row_buf[col]);
		}
		/* The following housekeeping may not be technically required, but it is a good practice
		 * to NULL pointer fields that are no longer valid.  Note that DB1_STRING fields have not
		 * been pkg_free(). NULLing DB1_STRING fields would normally not be good to do because a memory
		 * leak would occur.  However, the pg_convert_row() routine has saved the DB1_STRING pointer
		 * in the db_val_t structure.  The db_val_t structure will eventually be used to pkg_free()
		 * the DB1_STRING storage.
		 */
		row_buf[col] = (char *)NULL;
	}
	LM_DBG("freeing row buffer at %p\n", row_buf);
	pkg_free(row_buf);
	row_buf = NULL;

	return 0;

}

/*rx is row index*/
int bdb_append_row(db1_res_t* _res, char *bdb_result, int* _lres, int _rx)
{
	int col, len, i, j;
	char **row_buf, *s;
	db_row_t* row = NULL;
	col = len = i = j = 0;
	
	if (!_res) {
		LM_ERR("invalid parameter");
		return -1;
	}
	
	row = &(RES_ROWS(_res)[_rx]);
	
	if (db_allocate_row(_res, row) != 0) {
		LM_ERR("could not allocate row");
		return -2;
	}
	
	/*
	 * Allocate an array of pointers one per column.
	 * It that will be used to hold the address of the string representation of each column.
	 */
	len = sizeof(char *) * RES_COL_N(_res);
	row_buf = (char **)pkg_malloc(len);
	if (!row_buf) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate for %d columns %d bytes in row buffer at %p\n", RES_COL_N(_res), len, row_buf);
	memset(row_buf, 0, len);
	
	/*populate the row_buf with bdb_result*/
	/*bdb_result is memory from our callers stack so we copy here*/
	s = strtok(bdb_result, DELIM);
	while( s!=NULL)
	{	
		if(_lres) {
			/*only requested cols (_c was specified)*/
			for(i=0; i<ROW_N(row); i++) {
				if (col == _lres[i]) {
					len = strlen(s);
					row_buf[i] = pkg_malloc(len+1);
					if (!row_buf[i]) {
						LM_ERR("no private memory left\n");
						return -1;
					}
					memset(row_buf[i], 0, len+1);
					strncpy(row_buf[i], s, len);
				}
			}
		}
		else {
			len = strlen(s);

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("col[%i] = [%.*s]\n", col , len, s );
#endif

			row_buf[col] = (char*)pkg_malloc(len+1);
			if (!row_buf[col]) {
				LM_ERR("no private memory left\n");
				return -1;
			}
			memset(row_buf[col], 0, len+1);
			strncpy(row_buf[col], s, len);
		}
		s = strtok(NULL, DELIM);
		col++;
	}
	
	/*do the type conversion per col*/
	for(col = 0; col < ROW_N(row); col++) {
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("tc 1: col[%i] == ", col );
#endif

		/*skip the unrequested cols (as already specified)*/
		if(!row_buf[col])  continue;

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("tc 2: col[%i] \n", col );
#endif

		/* Convert the string representation into the value representation */
		if (bdb_str2val(RES_TYPES(_res)[col], &(ROW_VALUES(row)[col])
				, row_buf[col], strlen(row_buf[col])) < 0) {
			LM_ERR("while converting value\n");
			LM_DBG("freeing row at %p\n", row);
			db_free_row(row);
			return -3;
		}
	}

	/* pkg_free() must be done for the above allocations now that the row has been converted.
	 * During bdb_convert_row (and subsequent bdb_str2val) processing, data types that don't need to be
	 * converted (namely STRINGS) have their addresses saved.  These data types should not have
	 * their pkg_malloc() allocations freed here because they are still needed.  However, some data types
	 * (ex: INT, DOUBLE) should have their pkg_malloc() allocations freed because during the conversion
	 * process, their converted values are saved in the union portion of the db_val_t structure.
	 *
	 * Warning: when the converted row is no longer needed, the data types whose addresses
	 * were saved in the db_val_t structure must be freed or a memory leak will happen.
	 * This processing should happen in the db_free_row() subroutine.  The caller of
	 * this routine should ensure that db_free_rows(), db_free_row() or db_free_result()
	 * is eventually called.
	 */
	for (col = 0; col < RES_COL_N(_res); col++) {
		if (RES_TYPES(_res)[col] != DB1_STRING) {
			LM_DBG("col[%d] Col[%.*s] Type[%d] Freeing row_buf[%p]\n", col
				, RES_NAMES(_res)[col]->len, RES_NAMES(_res)[col]->s,
				  RES_TYPES(_res)[col], row_buf[col]);
			LM_DBG("freeing row_buf[%d] at %p\n", col, row_buf[col]);
			pkg_free(row_buf[col]);
		}
		/* The following housekeeping may not be technically required, but it is a good practice
		 * to NULL pointer fields that are no longer valid.  Note that DB1_STRING fields have not
		 * been pkg_free(). NULLing DB1_STRING fields would normally not be good to do because a memory
		 * leak would occur.  However, the pg_convert_row() routine has saved the DB1_STRING pointer
		 * in the db_val_t structure.  The db_val_t structure will eventually be used to pkg_free()
		 * the DB1_STRING storage.
		 */
		row_buf[col] = (char *)NULL;
	}
	LM_DBG("freeing row buffer at %p\n", row_buf);
	pkg_free(row_buf);
	row_buf = NULL;
	return 0;
}



int* bdb_get_colmap(table_p _dtp, db_key_t* _k, int _n)
{
	int i, j, *_lref=NULL;
	
	if(!_dtp || !_k || _n < 0)
		return NULL;
	
	_lref = (int*)pkg_malloc(_n*sizeof(int));
	if(!_lref)
		return NULL;
	
	for(i=0; i < _n; i++)
	{
		for(j=0; j<_dtp->ncols; j++) {
			if(_k[i]->len==_dtp->colp[j]->name.len
			&& !strncasecmp(_k[i]->s, _dtp->colp[j]->name.s,
						_dtp->colp[j]->name.len)) {
				_lref[i] = j;
				break;
			}
		}
		if(i>=_dtp->ncols) {
			LM_DBG("ERROR column <%.*s> not found\n", _k[i]->len, _k[i]->s);
			pkg_free(_lref);
			return NULL;
		}
	}
	return _lref;
}


int bdb_is_neq_type(db_type_t _t0, db_type_t _t1)
{
	if(_t0 == _t1)	return 0;
	
	switch(_t1)
	{
		case DB1_INT:
			if(_t0==DB1_DATETIME || _t0==DB1_BITMAP)
				return 0;
		case DB1_BIGINT:
				LM_ERR("BIGINT not supported");
				return 0;
		case DB1_DATETIME:
			if(_t0==DB1_INT)
				return 0;
			if(_t0==DB1_BITMAP)
				return 0;
		case DB1_DOUBLE:
			break;
		case DB1_STRING:
			if(_t0==DB1_STR)
				return 0;
		case DB1_STR:
			if(_t0==DB1_STRING || _t0==DB1_BLOB)
				return 0;
		case DB1_BLOB:
			if(_t0==DB1_STR)
				return 0;
		case DB1_BITMAP:
			if (_t0==DB1_INT)
				return 0;
		default:
			break;
	}
	return 1;
}


/*
*/
int bdb_row_match(db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n, db1_res_t* _r, int* _lkey )
{
	int i, res;
	db_row_t* row = NULL;
	
	if(!_r || !_lkey)
		return 1;
	
	row = RES_ROWS(_r);
	
	for(i=0; i<_n; i++) {
		res = bdb_cmp_val(&(ROW_VALUES(row)[_lkey[i]]), &_v[i]);

		if(!_op || !strcmp(_op[i], OP_EQ)) {
			if(res!=0)
				return 0;
		} else {
		if(!strcmp(_op[i], OP_LT)) {
			if(res!=-1)
				return 0;
		} else {
		if(!strcmp(_op[i], OP_GT)) {
			if(res!=1)
				return 0;
		} else {
		if(!strcmp(_op[i], OP_LEQ)) {
			if(res==1)
				return 0;
		} else {
		if(!strcmp(_op[i], OP_GEQ)) {
			if(res==-1)
				return 0;
		} else {
			return res;
		}}}}}
	}
	
	return 1;
}

/*
*/
int bdb_cmp_val(db_val_t* _vp, db_val_t* _v)
{
	int _l, _n;
	
	if(!_vp && !_v)
		return 0;
	if(!_v)
		return 1;
	if(!_vp)
		return -1;
	if(_vp->nul && _v->nul)
		return 0;
	if(_v->nul)
		return 1;
	if(_vp->nul)
		return -1;
	
	switch(VAL_TYPE(_v))
	{
		case DB1_INT:
			return (_vp->val.int_val<_v->val.int_val)?-1:
					(_vp->val.int_val>_v->val.int_val)?1:0;
		case DB1_BIGINT:
			LM_ERR("BIGINT not supported");
			return -1;
		case DB1_DOUBLE:
			return (_vp->val.double_val<_v->val.double_val)?-1:
					(_vp->val.double_val>_v->val.double_val)?1:0;
		case DB1_DATETIME:
			return (_vp->val.int_val<_v->val.time_val)?-1:
					(_vp->val.int_val>_v->val.time_val)?1:0;
		case DB1_STRING:
			_l = strlen(_v->val.string_val);
			_l = (_l>_vp->val.str_val.len)?_vp->val.str_val.len:_l;
			_n = strncasecmp(_vp->val.str_val.s, _v->val.string_val, _l);
			if(_n)
				return _n;
			if(_vp->val.str_val.len == strlen(_v->val.string_val))
				return 0;
			if(_l==_vp->val.str_val.len)
				return -1;
			return 1;
		case DB1_STR:
			_l = _v->val.str_val.len;
			_l = (_l>_vp->val.str_val.len)?_vp->val.str_val.len:_l;
			_n = strncasecmp(_vp->val.str_val.s, _v->val.str_val.s, _l);
			if(_n)
				return _n;
			if(_vp->val.str_val.len == _v->val.str_val.len)
				return 0;
			if(_l==_vp->val.str_val.len)
				return -1;
			return 1;
		case DB1_BLOB:
			_l = _v->val.blob_val.len;
			_l = (_l>_vp->val.str_val.len)?_vp->val.str_val.len:_l;
			_n = strncasecmp(_vp->val.str_val.s, _v->val.blob_val.s, _l);
			if(_n)
				return _n;
			if(_vp->val.str_val.len == _v->val.blob_val.len)
				return 0;
			if(_l==_vp->val.str_val.len)
				return -1;
			return 1;
		case DB1_BITMAP:
			return (_vp->val.int_val<_v->val.bitmap_val)?-1:
				(_vp->val.int_val>_v->val.bitmap_val)?1:0;
		default:
			break;
	}
	return -2;
}
