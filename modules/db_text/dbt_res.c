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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <setjmp.h>

#include "../../mem/mem.h"

#include "dbt_res.h"

#define SIGN(_i) ((_i) > 0 ? 1 : ((_i) < 0 ? -1 : 0))

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
		LM_DBG("no pkg memory!\n");
		pkg_free(_dres);
		return NULL;
	}
	memset(_dres->colv, 0, _sz*sizeof(dbt_column_t));
	LM_DBG("new res with %d cols\n", _sz);
	for(i = 0; i < _sz; i++)
	{
		n = (_lres)?_dtp->colv[_lres[i]]->name.len:_dtp->colv[i]->name.len;
		p = (_lres)?_dtp->colv[_lres[i]]->name.s:_dtp->colv[i]->name.s;
		_dres->colv[i].name.s = (char*)pkg_malloc((n+1)*sizeof(char));
		if(!_dres->colv[i].name.s)
		{
			LM_DBG("no pkg memory\n");
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
		_rp=_rp->next;
		if(_rp0->fields)
		{
			for(i=0; i<_dres->nrcols; i++)
			{
				if((_dres->colv[i].type==DB1_STR 
							|| _dres->colv[i].type==DB1_STRING
							|| _dres->colv[i].type==DB1_BLOB
							)
						&& _rp0->fields[i].val.str_val.s)
					pkg_free(_rp0->fields[i].val.str_val.s);
			}
			pkg_free(_rp0->fields);
		}
		pkg_free(_rp0);
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
			if(_k[i]->len==_dtp->colv[j]->name.len
				&& !strncasecmp(_k[i]->s, _dtp->colv[j]->name.s,
						_dtp->colv[j]->name.len))
			{
				_lref[i] = j;
				break;
			}
		}
		if(j>=_dtp->nrcols)
		{
			LM_ERR("column <%.*s> not found\n", _k[i]->len, _k[i]->s);
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
		if(!strcmp(_op[i], OP_NEQ))
		{
			if(res==0)
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
		}}}}}}
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
			LM_DBG("wrong types!\n");
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
			case DB1_INT:
			case DB1_DATETIME:
			case DB1_BITMAP:
				_rp->fields[i].type = _dres->colv[i].type;
				_rp->fields[i].val.int_val = _drp->fields[n].val.int_val;
			break;
			case DB1_DOUBLE:
				_rp->fields[i].type = DB1_DOUBLE;
				_rp->fields[i].val.double_val=_drp->fields[n].val.double_val;
			break;
			case DB1_STRING:
			case DB1_STR:
			case DB1_BLOB:
				_rp->fields[i].type = _dres->colv[i].type;
				_rp->fields[i].val.str_val.len =
						_drp->fields[n].val.str_val.len;
				_rp->fields[i].val.str_val.s =(char*)pkg_malloc(sizeof(char)*
						(_drp->fields[n].val.str_val.len+1));
				if(!_rp->fields[i].val.str_val.s)
					goto clean;
				memcpy(_rp->fields[i].val.str_val.s,
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
	LM_DBG("make clean!\n");
	while(i>=0)
	{
		if((_rp->fields[i].type == DB1_STRING
					|| _rp->fields[i].type == DB1_STR
					|| _rp->fields[i].type == DB1_BLOB)
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
#if 0
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
			case DB1_INT:
				fprintf(fout, "%.*s(int", _dres->colv[i].name.len,
								_dres->colv[i].name.s);
				if(_dres->colv[i].flag & DBT_FLAG_NULL)
					fprintf(fout, ",null");
				fprintf(fout, ") ");
			break;
			case DB1_DOUBLE:
				fprintf(fout, "%.*s(double", _dres->colv[i].name.len,
							_dres->colv[i].name.s);
				if(_dres->colv[i].flag & DBT_FLAG_NULL)
					fprintf(fout, ",null");
				fprintf(fout, ") ");
			break;
			case DB1_STR:
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
				case DB1_INT:
					if(rowp->fields[i].nul)
						fprintf(fout, "N ");
					else
						fprintf(fout, "%d ",
								rowp->fields[i].val.int_val);
				break;
				case DB1_DOUBLE:
					if(rowp->fields[i].nul)
						fprintf(fout, "N ");
					else
						fprintf(fout, "%.2f ",
								rowp->fields[i].val.double_val);
				break;
				case DB1_STR:
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
		case DB1_INT:
			return (_vp->val.int_val<_v->val.int_val)?-1:
					(_vp->val.int_val>_v->val.int_val)?1:0;

		case DB1_BIGINT:
			LM_ERR("BIGINT not supported\n");
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
				return SIGN(_n);
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
				return SIGN(_n);
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
				return SIGN(_n);
			if(_vp->val.str_val.len == _v->val.blob_val.len)
				return 0;
			if(_l==_vp->val.str_val.len)
				return -1;
			return 1;
		case DB1_BITMAP:
			return (_vp->val.int_val<_v->val.bitmap_val)?-1:
				(_vp->val.int_val>_v->val.bitmap_val)?1:0;
		default:
			LM_ERR("invalid datatype %d\n", VAL_TYPE(_v));
			return -2;
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


/* The _o clause to query is not really a db_key_t, it is SQL (str).
 * db_mysql and db_postgres simply paste it into SQL, we need to parse it. */
/* Format of _o:  column1 [ASC|DESC], column2 [ASC|DESC], ... */
int dbt_parse_orderbyclause(db_key_t **_o_k, char **_o_op, int *_o_n, db_key_t _o)
{
	char *_po, *_ps, *_pe;
	char _c = '\0';
	char _d[8];
	int _n;
	int _i;
	str *_s;

	/* scan _o, count ',' -> upper bound for no of columns */
	_n = 1;
	for (_i=0; _i < _o->len; _i++)
		if (_o->s[_i] == ',')
			_n++;

    /* *_o_k will include the db_key_ts, the strs, a copy of _o and \0 */
	*_o_k = pkg_malloc((sizeof(db_key_t)+sizeof(str)) * _n + _o->len + 1);
	if (!*_o_k)
		return -1;
	_s = (str *)((char *)(*_o_k) + sizeof(db_key_t) * _n);
	for (_i=0; _i < _n; _i++)
	    (*_o_k)[_i] = &_s[_i];
	_po = (char *)(*_o_k) + (sizeof(db_key_t) + sizeof(str)) * _n;
	memcpy(_po, _o->s, _o->len);
	*(_po+_o->len) = '\0';

	*_o_op = pkg_malloc(sizeof(char) * _n);
	if (!*_o_op)
	{
		pkg_free(*_o_k);
		return -1;
	}

	*_o_n = 0;
	_ps = _po;
	while (*_o_n < _n)
	{
		while (*_ps == ' ') _ps++;
		if (*_ps == '\0')
			break;
		strcpy(_d, " \f\n\r\t\v,"); /* isspace() and comma */
		if (*_ps == '"' || *_ps == '\'') /* detect quote */
		{
			_d[0] = *_ps;
			_d[1] = '\0';
			_ps++;
		}
		_pe = strpbrk(_ps, _d); /* search quote, space, comma or eos */
		if (!_pe && _d[0] == ' ') /* if token is last token in string */
			_pe = _po + _o->len; /* point to end of string */
		if (! _pe) /* we were looking for quote but found none */
			goto parse_error;

		/* _ps points to start of column-name,
		 * _pe points after the column-name, on quote, space, comma, or '\0' */
		_c = *_pe;
		*_pe = '\0';
		(*_o_k)[*_o_n]->s = _ps;
		(*_o_k)[*_o_n]->len = _pe - _ps;
		(*_o_op)[*_o_n] = '<'; /* default */
		(*_o_n)++;

		if (_c == '\0')
			break;

		/* go beyond current token */
		_ps = _pe + 1;
		if (_c == ',')
			continue;
		while (*_ps == ' ') _ps++;
		if (*_ps == ',')
		{
			_ps++;
			continue;
		}
		if (*_ps == '\0')
			break;

		/* there is ASC OR DESC qualifier */
		if (strncasecmp(_ps, "DESC", 4) == 0)
		{
			(*_o_op)[*_o_n-1] = '>';
			_ps += 4;
		} else if (strncasecmp(_ps, "ASC", 3) == 0)
		{
			_ps += 3;
		} else goto parse_error;

		/* point behind qualifier */
		while (*_ps == ' ') _ps++;
		if (*_ps == ',')
		{
			_ps++;
			continue;
		}
		if (*_ps == '\0')
			break;
		goto parse_error;
	}

	if (*_ps != '\0' && _c != '\0')   /* that means more elements than _tbc->nrcols */
		goto parse_error;

	if (*_o_n == 0) /* there weren't actually any columns */
	{
		pkg_free(*_o_k);
		pkg_free(*_o_op);
		*_o_op = NULL;
		*_o_k = NULL;
		return 0; /* return success anyway */
	}

	return 0;

parse_error:
	pkg_free(*_o_k);
	pkg_free(*_o_op);
	*_o_op = NULL;
	*_o_k = NULL;
	*_o_n = 0;
	return -1;
}


/* lres/_nc is the selected columns, _o_l/_o_n is the order-by columns:
 *   All order-by columns need to be extracted along with the selected columns,
 *   so any column in _o_l and not lres needs to be added to lres. _o_nc keeps
 *   track of the number of columns added to lres. */
int dbt_mangle_columnselection(int **_lres, int *_nc, int *_o_nc, int *_o_l, int _o_n)
{
	int _i, _j;

	*_o_nc = 0;

	if (! *_lres)
		return 0; /* all columns selected, no need to worry */

	/* count how many columns are affected */
	for (_i=0; _i < _o_n; _i++) /* loop over order-by columns */
	{
		for (_j=0; _j < *_nc && (*_lres)[_j] != _o_l[_i]; _j++);
		if (_j == *_nc) /* order-by column not found in select columns */
			(*_o_nc)++;
	}

	if (*_o_nc == 0)
		return 0; /* all order-by columns also selected, we're fine */

	/* make _lres bigger */
	*_lres = pkg_realloc(*_lres, sizeof(int) * (*_nc + *_o_nc));
	if (! *_lres)
		return -1;

	/* add oder-by columns to select columns */
	for (_i=0; _i < _o_n; _i++) /* loop over order-by columns */
	{
		for (_j=0; _j < *_nc && (*_lres)[_j] != _o_l[_i]; _j++);
		if (_j == *_nc) /* order-by column not found in select columns */
		{
			(*_lres)[*_nc] = _o_l[_i];
			(*_nc)++;
		}
	}

	/* _lres, _nc modified, _o_nc returned */
	return 0;
}

/* globals for qsort */
dbt_result_p dbt_sort_dres;
int *dbt_sort_o_l;
char *dbt_sort_o_op;
int dbt_sort_o_n;
jmp_buf dbt_sort_jmpenv;


/* comparison function for qsort */
int dbt_qsort_compar(const void *_a, const void *_b)
{
	int _i, _j, _r;

	for (_i=0; _i<dbt_sort_o_n; _i++)
	{
		_j = dbt_sort_o_l[_i];
		_r = dbt_cmp_val(&(*(dbt_row_p *)_a)->fields[_j], &(*(dbt_row_p *)_b)->fields[_j]);
		if (_r == 0)
			continue; /* no result yet, compare next column */
		if (_r == +1 || _r == -1)
			return (dbt_sort_o_op[_i] == '<') ? _r : -_r; /* ASC OR DESC */
		/* error */
		longjmp(dbt_sort_jmpenv, _r);
	}

	/* no result after comparing all columns, same */
	return 0;
}


int dbt_sort_result(dbt_result_p _dres, int *_o_l, char *_o_op, int _o_n, int *_lres, int _nc)
{
	int _i, _j;
	dbt_row_p *_a;
	dbt_row_p _el;

	/* first we need to rewrite _o_l in terms of _lres */
	if (_lres)
	{
		for (_i=0; _i < _o_n; _i++) /* loop over order-by columns */
		{
			/* depends on correctness of dbt_mangle_columnselection */
			for (_j=0; _lres[_j] != _o_l[_i]; _j++ /*, assert(_j < _nc)*/);
			_o_l[_i] = _j;
		}
	}

	/* rewrite linked list to array */
	_a = pkg_malloc(sizeof(dbt_row_p) * _dres->nrrows);
	if (!_a)
		return -1;
	for (_el=_dres->rows, _i=0; _el != NULL; _el=_el->next, _i++)
		_a[_i] = _el;

	/* set globals */
	dbt_sort_dres = _dres;
	dbt_sort_o_l = _o_l;
	dbt_sort_o_op = _o_op;
	dbt_sort_o_n = _o_n;
	_i = setjmp(dbt_sort_jmpenv);  /* exception handling */
	if (_i)
	{
		/* error occured during qsort */
		LM_ERR("qsort aborted\n");
		pkg_free(_a);
		return _i;
	}

	qsort(_a, _dres->nrrows, sizeof(dbt_row_p), &dbt_qsort_compar);

	/* restore linked list */
	for (_i=0; _i < _dres->nrrows; _i++)
	{
		_a[_i]->prev = (_i > 0) ? _a[_i-1] : NULL;
		_a[_i]->next = (_i+1 < _dres->nrrows) ? _a[_i+1] : NULL;
	}
	_dres->rows = _a[0];

	pkg_free(_a);
	return 0;
}


/* Remove the columns that were added to the result to facilitate sorting.
 *   The additional columns constitute the end of the queue. Instead of
 *   actually removing them with realloc, they are simply kept around, but
 *   hidden. For string-columns, the allocated string is however freed. */
void dbt_project_result(dbt_result_p _dres, int _o_nc)
{
	int _i;
	dbt_row_p _drp;

	if (! _o_nc)
		return;

	/* check whether there are string columns, free them */
	for (_i = _dres->nrcols - _o_nc; _i < _dres->nrcols; _i++)
	{
		if (_dres->colv[_i].type == DB1_STRING ||
				_dres->colv[_i].type == DB1_STR ||
				_dres->colv[_i].type == DB1_BLOB)
		{
			for (_drp=_dres->rows; _drp != NULL; _drp = _drp->next)
			{
				if (! _drp->fields[_i].nul &&
					(_drp->fields[_i].type == DB1_STRING ||
					_drp->fields[_i].type == DB1_STR ||
					_drp->fields[_i].type == DB1_BLOB ))
				{
					pkg_free(_drp->fields[_i].val.str_val.s);
					_drp->fields[_i].val.str_val.s = NULL;
					_drp->fields[_i].val.str_val.len = 0;
				}
			}
		}

		/* free the string containing the column name */
		pkg_free(_dres->colv[_i].name.s);
		_dres->colv[_i].name.s = NULL;
		_dres->colv[_i].name.len = 0;
	}

	/* pretend the columns are gone, in dbt_free_query free will do the right thing */
	_dres->nrcols -= _o_nc;
}

