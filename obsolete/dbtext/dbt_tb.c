/*
 * $Id$
 *
 * DBText library
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
 * DBText library
 *   
 * 2003-02-03 created by Daniel
 * 
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../locking.h"

#include "dbt_util.h"
#include "dbt_lib.h"


/**
 *
 */
dbt_column_p dbt_column_new(char *_s, int _l)
{
	dbt_column_p dcp = NULL;
	if(!_s || _l <=0)
		return NULL;
	dcp = (dbt_column_p)shm_malloc(sizeof(dbt_column_t));
	if(!dcp)
		return NULL;
	dcp->name.s  = (char*)shm_malloc(_l*sizeof(char));
	if(!dcp->name.s)
	{
		shm_free(dcp);
		return NULL;
	}
	dcp->name.len = _l;
	strncpy(dcp->name.s, _s, _l);
	dcp->next = dcp->prev = NULL;
	dcp->type = 0;
	dcp->flag = DBT_FLAG_UNSET;

	return dcp;
}

/**
 *
 */
int dbt_column_free(dbt_column_p dcp)
{
	
	if(!dcp)
		return -1;
	if(dcp->name.s)
		shm_free(dcp->name.s);
	shm_free(dcp);
 
	return 0;
}

/**
 *
 */
dbt_row_p dbt_row_new(int _nf)
{
	int i;
	dbt_row_p _drp = NULL;

	_drp = (dbt_row_p)shm_malloc(sizeof(dbt_row_t));
	if(!_drp)
		return NULL;
	
	_drp->fields = (dbt_val_p)shm_malloc(_nf*sizeof(dbt_val_t));
	if(!_drp->fields)
	{
		shm_free(_drp);
		return NULL;
	}
	memset(_drp->fields, 0, _nf*sizeof(dbt_val_t));
	for(i=0; i<_nf; i++)
		_drp->fields[i].nul = 1;

	_drp->next = _drp->prev = NULL;

	return _drp;
}

/**
 *
 */
int dbt_row_free(dbt_table_p _dtp, dbt_row_p _drp)
{
	int i;
	
	if(!_dtp || !_drp)
		return -1;
	
	if(_drp->fields)
	{
		for(i=0; i<_dtp->nrcols; i++)
			if(_dtp->colv[i]->type==DB_STR
					&& _drp->fields[i].val.str_val.s)
				shm_free(_drp->fields[i].val.str_val.s);
		shm_free(_drp->fields);
	}
	shm_free(_drp);

	return 0;
}

/**
 *
 */
dbt_table_p dbt_table_new(char *_s, int _l)
{
	dbt_table_p dtp = NULL;
	if(!_s || _l <= 0)
		return NULL;
	
	dtp = (dbt_table_p)shm_malloc(sizeof(dbt_table_t));
	if(!dtp)
		goto done;
	dtp->name.s = (char*)shm_malloc(_l*sizeof(char));
	if(!dtp->name.s)
	{
		shm_free(dtp);
		dtp = NULL;
		goto done;
	}
	memcpy(dtp->name.s, _s, _l);
	dtp->name.len = _l;

	dtp->rows = NULL;
	dtp->cols = NULL;
	dtp->colv = NULL;
	dtp->mark = (int)time(NULL);
	dtp->flag = DBT_TBFL_ZERO;
	dtp->nrrows = dtp->nrcols = dtp->auto_val = 0;
	dtp->auto_col = -1;
	
done:
	return dtp;
}

/**
 *
 */
int dbt_table_free_rows(dbt_table_p _dtp)
{
	dbt_row_p _rp=NULL, _rp0=NULL;
	
	if(!_dtp || !_dtp->rows || !_dtp->colv)
		return -1;
	_rp = _dtp->rows;
	while(_rp)
	{
		_rp0=_rp;
		_rp=_rp->next;
		dbt_row_free(_dtp, _rp0);
	}
	
	dbt_table_update_flags(_dtp, DBT_TBFL_MODI, DBT_FL_SET, 1);
	
	_dtp->rows = NULL;
	_dtp->nrrows = 0;

	return 0;
}

/**
 *
 */
int dbt_table_add_row(dbt_table_p _dtp, dbt_row_p _drp)
{
	if(!_dtp || !_drp)
		return -1;
	
	if(dbt_table_check_row(_dtp, _drp))
		return -1;
	
	dbt_table_update_flags(_dtp, DBT_TBFL_MODI, DBT_FL_SET, 1);
	
	if(_dtp->rows)
		(_dtp->rows)->prev = _drp;
	_drp->next = _dtp->rows;
	_dtp->rows = _drp;
	_dtp->nrrows++;

	return 0;
}

/**
 *
 */
int dbt_table_free(dbt_table_p _dtp)
{
	dbt_column_p _cp=NULL, _cp0=NULL;
	
	if(!_dtp)
		return -1;

	if(_dtp->name.s)
		shm_free(_dtp->name.s);
	
	if(_dtp->rows && _dtp->nrrows>0)
		dbt_table_free_rows(_dtp);
	
	_cp = _dtp->cols;
	while(_cp)
	{
		_cp0=_cp;
		_cp=_cp->next;
		dbt_column_free(_cp0);
	}
	if(_dtp->colv)
		shm_free(_dtp->colv);

	shm_free(_dtp);

	return 0;
}

/**
 *
 */
int dbt_row_set_val(dbt_row_p _drp, dbt_val_p _vp, int _t, int _idx)
{
	if(!_drp || !_vp || _idx<0)
		return -1;
	
	_drp->fields[_idx].nul = _vp->nul;
	_drp->fields[_idx].type = _t;

	if(!_vp->nul)
	{
		switch(_t)
		{
			case DB_STR:
			case DB_BLOB:
				_drp->fields[_idx].type = DB_STR;
				_drp->fields[_idx].val.str_val.s = 
					(char*)shm_malloc(_vp->val.str_val.len*sizeof(char));
				if(!_drp->fields[_idx].val.str_val.s)
				{
					_drp->fields[_idx].nul = 1;
					return -1;
				}
				memcpy(_drp->fields[_idx].val.str_val.s, _vp->val.str_val.s,
					_vp->val.str_val.len);
				_drp->fields[_idx].val.str_val.len = _vp->val.str_val.len;
			break;
			
			case DB_STRING:
				_drp->fields[_idx].type = DB_STR;
				_drp->fields[_idx].val.str_val.len=strlen(_vp->val.string_val);
				
				_drp->fields[_idx].val.str_val.s = 
					(char*)shm_malloc(_drp->fields[_idx].val.str_val.len
									  *sizeof(char));
				if(!_drp->fields[_idx].val.str_val.s)
				{
					_drp->fields[_idx].nul = 1;
					return -1;
				}
				memcpy(_drp->fields[_idx].val.str_val.s, _vp->val.string_val,
					_drp->fields[_idx].val.str_val.len);
			break;

			case DB_FLOAT:
				_drp->fields[_idx].type = DB_FLOAT;
				_drp->fields[_idx].val.float_val = _vp->val.float_val;
			break;			

			case DB_DOUBLE:
				_drp->fields[_idx].type = DB_DOUBLE;
				_drp->fields[_idx].val.double_val = _vp->val.double_val;
			break;
			
			case DB_INT:
				_drp->fields[_idx].type = DB_INT;
				_drp->fields[_idx].val.int_val = _vp->val.int_val;
			break;
			
			case DB_DATETIME:
				_drp->fields[_idx].type = DB_INT;
				_drp->fields[_idx].val.int_val = (int)_vp->val.time_val;
			break;
			
			default:
				_drp->fields[_idx].nul = 1;
				return -1;
		}
	}
	
	return 0;
}

/**
 *
 */
int dbt_row_update_val(dbt_row_p _drp, dbt_val_p _vp, int _t, int _idx)
{
	if(!_drp || !_vp || _idx<0)
		return -1;
	
	_drp->fields[_idx].nul = _vp->nul;
	_drp->fields[_idx].type = _t;
	
	if(!_vp->nul)
	{
		switch(_t)
		{
			case DB_BLOB:
			case DB_STR:
				_drp->fields[_idx].type = DB_STR;
				// free if already exists
				if(_drp->fields[_idx].val.str_val.s)
					shm_free(_drp->fields[_idx].val.str_val.s);
			
				_drp->fields[_idx].val.str_val.s = 
					(char*)shm_malloc(_vp->val.str_val.len*sizeof(char));
				if(!_drp->fields[_idx].val.str_val.s)
				{
					_drp->fields[_idx].nul = 1;
					return -1;
				}
				memcpy(_drp->fields[_idx].val.str_val.s, _vp->val.str_val.s,
					_vp->val.str_val.len);
				_drp->fields[_idx].val.str_val.len = _vp->val.str_val.len;
			break;
			
			case DB_STRING:
				_drp->fields[_idx].type = DB_STR;
				/* free if already exists */
				if(_drp->fields[_idx].val.str_val.s)
					shm_free(_drp->fields[_idx].val.str_val.s);

				_drp->fields[_idx].type = DB_STR;
				_drp->fields[_idx].val.str_val.len=strlen(_vp->val.string_val);
				
				_drp->fields[_idx].val.str_val.s = 
					(char*)shm_malloc(_drp->fields[_idx].val.str_val.len
									  *sizeof(char));
				if(!_drp->fields[_idx].val.str_val.s)
				{
					_drp->fields[_idx].nul = 1;
					return -1;
				}
				memcpy(_drp->fields[_idx].val.str_val.s, _vp->val.string_val,
					_drp->fields[_idx].val.str_val.len);
			break;
			
			case DB_FLOAT:
				_drp->fields[_idx].type = DB_FLOAT;
				_drp->fields[_idx].val.float_val = _vp->val.float_val;
			break;

			case DB_DOUBLE:
				_drp->fields[_idx].type = DB_DOUBLE;
				_drp->fields[_idx].val.double_val = _vp->val.double_val;
			break;
			
			case DB_INT:
				_drp->fields[_idx].type = DB_INT;
				_drp->fields[_idx].val.int_val = _vp->val.int_val;
			break;
			
			case DB_DATETIME:
				_drp->fields[_idx].type = DB_INT;
				_drp->fields[_idx].val.int_val = (int)_vp->val.time_val;
			break;
			
			default:
				LOG(L_ERR,"ERROR:dbtext: unsupported type %d in update\n",_t);
				_drp->fields[_idx].nul = 1;
				return -1;
		}
	}
	
	return 0;
}

/**
 *
 */
int dbt_table_check_row(dbt_table_p _dtp, dbt_row_p _drp)
{
	int i;
	if(!_dtp || _dtp->nrcols <= 0 || !_drp)
		return -1;
	
	for(i=0; i<_dtp->nrcols; i++)
	{
		if(!_drp->fields[i].nul &&(_dtp->colv[i]->type!=_drp->fields[i].type))
		{
			DBG("DBT:dbt_table_check_row: incompatible types - field %d\n",i);
			return -1;
		}
		if(_dtp->colv[i]->flag & DBT_FLAG_NULL)
			continue;
		
		if(!_drp->fields[i].nul)
			continue;

		if(_dtp->colv[i]->type==DB_INT
			&& (_dtp->colv[i]->flag & DBT_FLAG_AUTO)
			&& i==_dtp->auto_col)
		{
			_drp->fields[i].nul = 0;
			_drp->fields[i].val.int_val = ++_dtp->auto_val;
			continue;
		}

		DBG("DBT:dbt_table_check_row: NULL value not allowed - field %d\n",i);
		return -1;
	}
	
	return 0;
}

/**
 *
 */
int dbt_table_update_flags(dbt_table_p _dtp, int _f, int _o, int _m)
{
	if(!_dtp)
		return -1;
	
	if(_o == DBT_FL_SET)
		_dtp->flag |= _f;
	else if(_o == DBT_FL_UNSET)
			_dtp->flag &= ~_f;
	
	if(_m)
		_dtp->mark = (int)time(NULL);
	
	return 0;
}

