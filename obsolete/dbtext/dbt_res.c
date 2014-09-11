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
 * 2003-06-05 fixed bug: when comparing two values and the first was less than
 *           the second one, the result of 'dbt_row_match' was always true,
 *           thanks to Gabriel, (Daniel)
 * 2003-02-04 created by Daniel
 * 
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "../../mem/mem.h"

#include "dbt_res.h"

dbt_result_p dbt_result_new(dbt_table_p _dtp, int *_lres, int _sz)
{
	dbt_result_p _dres = NULL;
	int i, n;
	char *p;
	
	if(!_dtp || _sz < 0)
		return NULL;

	if(!_lres)
		_sz = _dtp->nrcols;
	
	_dres = (dbt_result_p)pkg_malloc(sizeof(dbt_result_t));
	if(!_dres)
		return NULL;
	_dres->colv = (dbt_column_p)pkg_malloc(_sz*sizeof(dbt_column_t));
	if(!_dres->colv)
	{
		DBG("DBT:dbt_result_new: no memory!\n");
		pkg_free(_dres);
		return NULL;
	}
	DBG("DBT:dbt_result_new: new res with %d cols\n", _sz);
	for(i = 0; i < _sz; i++)
	{
		n = (_lres)?_dtp->colv[_lres[i]]->name.len:_dtp->colv[i]->name.len;
		p = (_lres)?_dtp->colv[_lres[i]]->name.s:_dtp->colv[i]->name.s;
		_dres->colv[i].name.s = (char*)pkg_malloc((n+1)*sizeof(char));
		if(!_dres->colv[i].name.s)
		{
			DBG("DBT:dbt_result_new: no memory\n");
			goto clean;
		}
		_dres->colv[i].name.len = n;
		strncpy(_dres->colv[i].name.s, p, n);
		_dres->colv[i].name.s[n] = 0;
		_dres->colv[i].type =
				(_lres)?_dtp->colv[_lres[i]]->type:_dtp->colv[i]->type;
	}
	
	_dres->nrcols = _sz;
	_dres->nrrows = 0;
	_dres->rows = NULL;

	return _dres;
clean:
	while(i>=0)
	{
		if(_dres->colv[i].name.s)
			pkg_free(_dres->colv[i].name.s);
		i--;
	}
	pkg_free(_dres->colv);
	pkg_free(_dres);
	
	return NULL;
}

int dbt_result_free(dbt_result_p _dres)
{
	dbt_row_p _rp=NULL, _rp0=NULL;
	int i;

	if(!_dres)
		return -1;
	_rp = _dres->rows;
	while(_rp)
	{
		_rp0=_rp;
		if(_rp0->fields)
		{
			for(i=0; i<_dres->nrcols; i++)
			{
				if(_dres->colv[i].type==DB_STR 
						&& _rp0->fields[i].val.str_val.s)
					pkg_free(_rp0->fields[i].val.str_val.s);
			}
			pkg_free(_rp0->fields);
		}
		pkg_free(_rp0);
		_rp=_rp->next;
	}
	if(_dres->colv)
	{
		for(i=0; i<_dres->nrcols; i++)
		{
			if(_dres->colv[i].name.s)
				pkg_free(_dres->colv[i].name.s);
		}
		pkg_free(_dres->colv);
	}

	pkg_free(_dres);

	return 0;
}

int dbt_result_add_row(dbt_result_p _dres, dbt_row_p _drp)
{
	if(!_dres || !_drp)
		return -1;
	_dres->nrrows++;
	
	if(_dres->rows)
		(_dres->rows)->prev = _drp;
	_drp->next = _dres->rows;
	_dres->rows = _drp;

	return 0;
}

int* dbt_get_refs(dbt_table_p _dtp, db_key_t* _k, int _n)
{
	int i, j, *_lref=NULL;
	
	if(!_dtp || !_k || _n < 0)
		return NULL;

	_lref = (int*)pkg_malloc(_n*sizeof(int));
	if(!_lref)
		return NULL;

	for(i=0; i < _n; i++)
	{
		for(j=0; j<_dtp->nrcols; j++)
		{
			if(strlen(_k[i])==_dtp->colv[j]->name.len
				&& !strncasecmp(_k[i], _dtp->colv[j]->name.s,
						_dtp->colv[j]->name.len))
			{
				_lref[i] = j;
				break;
			}
		}
		if(j>=_dtp->nrcols)
		{
			DBG("DBT:dbt_get_refs: ERROR column <%s> not found\n", _k[i]);
			pkg_free(_lref);
			return NULL;
		}
	}
	return _lref;
}	


int dbt_row_match(dbt_table_p _dtp, dbt_row_p _drp, int* _lkey,
				 db_op_t* _op, db_val_t* _v, int _n)
{
	int i, res;
	if(!_dtp || !_drp)
		return 0;
	if(!_lkey)
		return 1;
	for(i=0; i<_n; i++)
	{
		res = dbt_cmp_val(&_drp->fields[_lkey[i]], &_v[i]);
		if(!_op || !strcmp(_op[i], OP_EQ))
		{
			if(res!=0)
				return 0;
		}else{
		if(!strcmp(_op[i], OP_LT))
		{
			if(res!=-1)
				return 0;
		}else{
		if(!strcmp(_op[i], OP_GT))
		{
			if(res!=1)
				return 0;
		}else{
		if(!strcmp(_op[i], OP_LEQ))
		{
			if(res==1)
				return 0;
		}else{
		if(!strcmp(_op[i], OP_GEQ))
		{
			if(res==-1)
				return 0;
		}else{
			return 0;
		}}}}}		
	}
	return 1;
}

int dbt_result_extract_fields(dbt_table_p _dtp, dbt_row_p _drp,
				int* _lres, dbt_result_p _dres)
{
	dbt_row_p _rp=NULL;
	int i, n;
	
	if(!_dtp || !_drp || !_dres || _dres->nrcols<=0)	
		return -1;
	
	_rp = dbt_result_new_row(_dres);
	if(!_rp)
		return -1;

	for(i=0; i<_dres->nrcols; i++)
	{
		n = (_lres)?_lres[i]:i;
		if(dbt_is_neq_type(_dres->colv[i].type, _dtp->colv[n]->type))
		{
			DBG("DBT:dbt_result_extract_fields: wrong types!\n");
			goto clean;
		}
		_rp->fields[i].nul = _drp->fields[n].nul;
		if(_rp->fields[i].nul)
		{
			memset(&(_rp->fields[i].val), 0, sizeof(_rp->fields[i].val));
			continue;
		}
		
		switch(_dres->colv[i].type)
		{
			case DB_INT:
			case DB_DATETIME:
			case DB_BITMAP:
				_rp->fields[i].type = DB_INT;
				_rp->fields[i].val.int_val = _drp->fields[n].val.int_val;
			break;
			case DB_FLOAT:
				_rp->fields[i].type = DB_FLOAT;
				_rp->fields[i].val.float_val=_drp->fields[n].val.float_val;
			break;
			case DB_DOUBLE:
				_rp->fields[i].type = DB_DOUBLE;
				_rp->fields[i].val.double_val=_drp->fields[n].val.double_val;
			break;
			case DB_STRING:
			case DB_STR:
			case DB_BLOB:
				_rp->fields[i].type = DB_STR;
				_rp->fields[i].val.str_val.len =
						_drp->fields[n].val.str_val.len;
				_rp->fields[i].val.str_val.s =(char*)pkg_malloc(sizeof(char)*
						(_drp->fields[n].val.str_val.len+1));
				if(!_rp->fields[i].val.str_val.s)
					goto clean;
				strncpy(_rp->fields[i].val.str_val.s,
						_drp->fields[n].val.str_val.s,
						_rp->fields[i].val.str_val.len);
				_rp->fields[i].val.str_val.s[_rp->fields[i].val.str_val.len]=0;
			break;
			default:
				goto clean;
		}
	}

	if(_dres->rows)
		(_dres->rows)->prev = _rp;
	_rp->next = _dres->rows;
	_dres->rows = _rp;
	_dres->nrrows++;

	return 0;

clean:
	DBG("DBT:dbt_result_extract_fields: make clean!\n");
	while(i>=0)
	{
		if(_rp->fields[i].type == DB_STR
				&& !_rp->fields[i].nul
				&& _rp->fields[i].val.str_val.s)
			pkg_free(_rp->fields[i].val.str_val.s);
				
		i--;
	}
	pkg_free(_rp->fields);
	pkg_free(_rp);

	return -1;
}

int dbt_result_print(dbt_result_p _dres)
{
#ifdef DBT_EXTRA_DEBUG
	int i;
	FILE *fout = stdout;
	dbt_row_p rowp = NULL;
	char *p;

	if(!_dres || _dres->nrcols<=0)
		return -1;

	fprintf(fout, "\nContent of result\n");
	
	for(i=0; i<_dres->nrcols; i++)
	{
		switch(_dres->colv[i].type)
		{
			case DB_INT:
				fprintf(fout, "%.*s(int", _dres->colv[i].name.len,
								_dres->colv[i].name.s);
				if(_dres->colv[i].flag & DBT_FLAG_NULL)
					fprintf(fout, ",null");
				fprintf(fout, ") ");
			break;
			case DB_FLOAT:
				fprintf(fout, "%.*s(float", _dres->colv[i].name.len,
							_dres->colv[i].name.s);
				if(_dres->colv[i].flag & DBT_FLAG_NULL)
					fprintf(fout, ",null");
				fprintf(fout, ") ");
			break;
			case DB_DOUBLE:
				fprintf(fout, "%.*s(double", _dres->colv[i].name.len,
							_dres->colv[i].name.s);
				if(_dres->colv[i].flag & DBT_FLAG_NULL)
					fprintf(fout, ",null");
				fprintf(fout, ") ");
			break;
			case DB_STR:
				fprintf(fout, "%.*s(str", _dres->colv[i].name.len,
						_dres->colv[i].name.s);
				if(_dres->colv[i].flag & DBT_FLAG_NULL)
					fprintf(fout, ",null");
				fprintf(fout, ") ");
			break;
			default:
				return -1;
		}
	}
	fprintf(fout, "\n");
	rowp = _dres->rows;
	while(rowp)
	{
		for(i=0; i<_dres->nrcols; i++)
		{
			switch(_dres->colv[i].type)
			{
				case DB_INT:
					if(rowp->fields[i].nul)
						fprintf(fout, "N ");
					else
						fprintf(fout, "%d ",
								rowp->fields[i].val.int_val);
				break;
				case DB_FLOAT:
					if(rowp->fields[i].nul)
						fprintf(fout, "N ");
					else
						fprintf(fout, "%.2f ",
								rowp->fields[i].val.float_val);
				break;
				case DB_DOUBLE:
					if(rowp->fields[i].nul)
						fprintf(fout, "N ");
					else
						fprintf(fout, "%.2f ",
								rowp->fields[i].val.double_val);
				break;
				case DB_STR:
					fprintf(fout, "\"");
					if(!rowp->fields[i].nul)
					{
						p = rowp->fields[i].val.str_val.s;
						while(p < rowp->fields[i].val.str_val.s
								+ rowp->fields[i].val.str_val.len)
						{
							switch(*p)
							{
								case '\n':
									fprintf(fout, "\\n");
								break;
								case '\r':
									fprintf(fout, "\\r");
								break;
								case '\t':
									fprintf(fout, "\\t");
								break;
								case '\\':
									fprintf(fout, "\\\\");
								break;
								case '"':
									fprintf(fout, "\\\"");
								break;
								case '\0':
									fprintf(fout, "\\0");
								break;
								default:
									fprintf(fout, "%c", *p);
							}
							p++;
						}
					}
					fprintf(fout, "\" ");
				break;
				default:
					return -1;
			}
		}
		fprintf(fout, "\n");
		rowp = rowp->next;
	}
#endif

	return 0;
}

int dbt_cmp_val(dbt_val_p _vp, db_val_t* _v)
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
		case DB_INT:
			return (_vp->val.int_val<_v->val.int_val)?-1:
					(_vp->val.int_val>_v->val.int_val)?1:0;
		case DB_FLOAT:
			return (_vp->val.float_val<_v->val.float_val)?-1:
					(_vp->val.float_val>_v->val.float_val)?1:0;
		case DB_DOUBLE:
			return (_vp->val.double_val<_v->val.double_val)?-1:
					(_vp->val.double_val>_v->val.double_val)?1:0;
		case DB_DATETIME:
			return (_vp->val.int_val<_v->val.time_val)?-1:
					(_vp->val.int_val>_v->val.time_val)?1:0;
		case DB_STRING:
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
		case DB_STR:
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
		case DB_BLOB:
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
		case DB_BITMAP:
			return (_vp->val.int_val<_v->val.bitmap_val)?-1:
				(_vp->val.int_val>_v->val.bitmap_val)?1:0;
	}
	return -2;
}

dbt_row_p dbt_result_new_row(dbt_result_p _dres)
{
	dbt_row_p _drp = NULL;
	if(!_dres || _dres->nrcols<=0)
		return NULL;
	
	_drp = (dbt_row_p)pkg_malloc(sizeof(dbt_row_t));
	if(!_drp)
		return NULL;
	memset(_drp, 0, sizeof(dbt_row_t));
	_drp->fields = (dbt_val_p)pkg_malloc(_dres->nrcols*sizeof(dbt_val_t));
	if(!_drp->fields)
	{
		pkg_free(_drp);
		return NULL;
	}
	memset(_drp->fields, 0, _dres->nrcols*sizeof(dbt_val_t));

	_drp->next = _drp->prev = NULL;

	return _drp;
}

int dbt_is_neq_type(db_type_t _t0, db_type_t _t1)
{
	// DBG("DBT:dbt_is_neq_type: t0=%d t1=%d!\n", _t0, _t1);
	if(_t0 == _t1)
		return 0;
	switch(_t1)
	{
		case DB_INT:
			if(_t0==DB_DATETIME || _t0==DB_BITMAP)
				return 0;
		case DB_DATETIME:
			if(_t0==DB_INT)
				return 0;
			if(_t0==DB_BITMAP)
				return 0;
		case DB_FLOAT:
			break;
		case DB_DOUBLE:
			break;
		case DB_STRING:
			if(_t0==DB_STR)
				return 0;
		case DB_STR:
			if(_t0==DB_STRING || _t0==DB_BLOB)
				return 0;
		case DB_BLOB:
			if(_t0==DB_STR)
				return 0;
		case DB_BITMAP:
			if (_t0==DB_INT)
				return 0;
	}
	return 1;
}

