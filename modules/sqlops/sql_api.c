/*
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \ingroup sqlops
 * \brief Kamailio SQL-operations :: API
 *
 * - Module: \ref sqlops
 */


#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../hashes.h"
#include "../../ut.h"
#include "../../lib/srdb1/db_ut.h"
#ifdef WITH_XAVP
#include "../../xavp.h"
#endif

#include "sql_api.h"

sql_con_t *_sql_con_root = NULL;
sql_result_t *_sql_result_root = NULL;

static char _sql_empty_buf[1];

sql_con_t* sql_get_connection(str *name)
{
	sql_con_t *sc;
	unsigned int conid;

	conid = core_case_hash(name, 0, 0);

	sc = _sql_con_root;
	while(sc)
	{
		if(conid==sc->conid && sc->name.len==name->len
				&& strncmp(sc->name.s, name->s, name->len)==0)
			return sc;
		sc = sc->next;
	}
	return NULL;
}

int sql_init_con(str *name, str *url)
{
	sql_con_t *sc;
	unsigned int conid;

	*_sql_empty_buf = '\0';

	conid = core_case_hash(name, 0, 0);

	sc = _sql_con_root;
	while(sc)
	{
		if(conid==sc->conid && sc->name.len==name->len
				&& strncmp(sc->name.s, name->s, name->len)==0)
		{
			LM_ERR("duplicate connection name\n");
			return -1;
		}
		sc = sc->next;
	}
	sc = (sql_con_t*)pkg_malloc(sizeof(sql_con_t));
	if(sc==NULL)
	{
		LM_ERR("no pkg memory\n");
		return -1;
	}
	memset(sc, 0, sizeof(sql_con_t));
	sc->conid = conid;
	sc->name = *name;
	sc->db_url = *url;
	sc->next = _sql_con_root;
	_sql_con_root = sc;

	return 0;
}

int pv_parse_con_name(pv_spec_p sp, str *in)
{
	sql_con_t *con;

	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	con = sql_get_connection(in);
	if (con==NULL) {
		LM_ERR("invalid connection [%.*s]\n", in->len, in->s);
		return -1;
	}

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_VAL_STR;
	sp->pvp.pvn.u.isname.name.s = *in;
	return 0;
}

int pv_get_sqlrows(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	sql_con_t *con;
	str* sc;

	sc = &param->pvn.u.isname.name.s;
	con = sql_get_connection(sc);
	if(con==NULL)
	{
		LM_ERR("invalid connection [%.*s]\n", sc->len, sc->s);
		return -1;
	}

	if (!DB_CAPABILITY(con->dbf, DB_CAP_AFFECTED_ROWS))
	{
		LM_ERR("con: %p database module does not have DB_CAP_AFFECTED_ROWS [%.*s]\n",
		       con, sc->len, sc->s);
		return -1;
	}

	return pv_get_sintval(msg, param, res, con->dbf.affected_rows(con->dbh));
}

int sql_connect(void)
{
	sql_con_t *sc;
	sc = _sql_con_root;
	while(sc)
	{
		if (db_bind_mod(&sc->db_url, &sc->dbf))
		{
			LM_DBG("database module not found for [%.*s]\n",
					sc->name.len, sc->name.s);
			return -1;
		}
		if (!DB_CAPABILITY(sc->dbf, DB_CAP_RAW_QUERY))
		{
			LM_ERR("database module does not have DB_CAP_ALL [%.*s]\n",
					sc->name.len, sc->name.s);
			return -1;
		}
		sc->dbh = sc->dbf.init(&sc->db_url);
		if (sc->dbh==NULL)
		{
			LM_ERR("failed to connect to the database [%.*s]\n",
					sc->name.len, sc->name.s);
			return -1;
		}
		sc = sc->next;
	}
	return 0;
}

void sql_disconnect(void)
{
	sql_con_t *sc;
	sc = _sql_con_root;
	while(sc)
	{
		if (sc->dbh!=NULL)
			sc->dbf.close(sc->dbh);
		sc->dbh= NULL;
		sc = sc->next;
	}
}

sql_result_t* sql_get_result(str *name)
{
	sql_result_t *sr;
	unsigned int resid;

	resid = core_case_hash(name, 0, 0);

	sr = _sql_result_root;
	while(sr)
	{
		if(sr->resid==resid && sr->name.len==name->len
				&& strncmp(sr->name.s, name->s, name->len)==0)
			return sr;
		sr = sr->next;
	}
	sr = (sql_result_t*)pkg_malloc(sizeof(sql_result_t) + name->len);
	if(sr==NULL)
	{
		LM_ERR("no pkg memory\n");
		return NULL;
	}
	memset(sr, 0, sizeof(sql_result_t));
	memcpy(sr+1, name->s, name->len);
	sr->name.s = (char *)(sr + 1);
	sr->name.len = name->len;
	sr->resid = resid;
	sr->next = _sql_result_root;
	_sql_result_root = sr;
	return sr;
}

void sql_reset_result(sql_result_t *res)
{
	int i, j;
	if(res->cols)
	{
		for(i=0; i<res->ncols; i++)
			if(res->cols[i].name.s!=NULL)
				pkg_free(res->cols[i].name.s);
		pkg_free(res->cols);
		res->cols = NULL;
	}
	if(res->vals)
	{
		for(i=0; i<res->nrows; i++)
		{
			if(res->vals[i])
			{
				for(j=0; j<res->ncols; j++)
				{
					if(res->vals[i][j].flags&PV_VAL_STR
							&& res->vals[i][j].value.s.len>0)
						pkg_free(res->vals[i][j].value.s.s);
				}
				pkg_free(res->vals[i]);
			}
		}
		pkg_free(res->vals);
		res->vals = NULL;
	}
	res->nrows = 0;
	res->ncols = 0;
}

int sql_do_query(sql_con_t *con, str *query, sql_result_t *res)
{
	db1_res_t* db_res = NULL;
	int i, j;
	str sv;

	if(res) sql_reset_result(res);

	if(query==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	if(con->dbf.raw_query(con->dbh, query, &db_res)!=0)
	{
		LM_ERR("cannot do the query [%.*s]\n",
				(query->len>32)?32:query->len, query->s);
		return -1;
	}

	if(db_res==NULL || RES_ROW_N(db_res)<=0 || RES_COL_N(db_res)<=0)
	{
		LM_DBG("no result after query\n");
		con->dbf.free_result(con->dbh, db_res);
		return 2;
	}
	if(!res)
	{
		LM_DBG("no sqlresult parameter, ignoring result from query\n");
		con->dbf.free_result(con->dbh, db_res);
		return 3;
	}

	res->ncols = RES_COL_N(db_res);
	res->nrows = RES_ROW_N(db_res);
	LM_DBG("rows [%d] cols [%d]\n", res->nrows, res->ncols);

	res->cols = (sql_col_t*)pkg_malloc(res->ncols*sizeof(sql_col_t));
	if(res->cols==NULL)
	{
		res->ncols = 0;
		res->nrows = 0;
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(res->cols, 0, res->ncols*sizeof(sql_col_t));
	for(i=0; i<res->ncols; i++)
	{
		res->cols[i].name.len = (RES_NAMES(db_res)[i])->len;
		res->cols[i].name.s = (char*)pkg_malloc((res->cols[i].name.len+1)
				*sizeof(char));
		if(res->cols[i].name.s==NULL)
		{
			LM_ERR("no more memory\n");
			goto error;
		}
		memcpy(res->cols[i].name.s, RES_NAMES(db_res)[i]->s,
				res->cols[i].name.len);
		res->cols[i].name.s[res->cols[i].name.len]='\0';
		res->cols[i].colid = core_case_hash(&res->cols[i].name, 0, 0);
	}

	res->vals = (sql_val_t**)pkg_malloc(res->nrows*sizeof(sql_val_t*));
	if(res->vals==NULL)
	{
		LM_ERR("no more memory\n");
		goto error;
	}
	memset(res->vals, 0, res->nrows*sizeof(sql_val_t*));
	for(i=0; i<res->nrows; i++)
	{
		res->vals[i] = (sql_val_t*)pkg_malloc(res->ncols*sizeof(sql_val_t));
		if(res->vals[i]==NULL)
		{
			LM_ERR("no more memory\n");
			goto error;
		}
		memset(res->vals[i], 0, res->ncols*sizeof(sql_val_t));
		for(j=0; j<res->ncols; j++)
		{
			if(RES_ROWS(db_res)[i].values[j].nul)
			{
				res->vals[i][j].flags = PV_VAL_NULL;
				continue;
			}
			sv.s = NULL;
			sv.len = 0;
			switch(RES_ROWS(db_res)[i].values[j].type)
			{
				case DB1_STRING:
					res->vals[i][j].flags = PV_VAL_STR;
					sv.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.string_val;
					sv.len=strlen(sv.s);
				break;
				case DB1_STR:
					res->vals[i][j].flags = PV_VAL_STR;
					sv.len=
						RES_ROWS(db_res)[i].values[j].val.str_val.len;
					sv.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.str_val.s;
				break;
				case DB1_BLOB:
					res->vals[i][j].flags = PV_VAL_STR;
					sv.len=
						RES_ROWS(db_res)[i].values[j].val.blob_val.len;
					sv.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.blob_val.s;
				break;
				case DB1_INT:
					res->vals[i][j].flags = PV_VAL_INT;
					res->vals[i][j].value.n
						= (int)RES_ROWS(db_res)[i].values[j].val.int_val;
				break;
				case DB1_DATETIME:
					res->vals[i][j].flags = PV_VAL_INT;
					res->vals[i][j].value.n
						= (int)RES_ROWS(db_res)[i].values[j].val.time_val;
				break;
				case DB1_BITMAP:
					res->vals[i][j].flags = PV_VAL_INT;
					res->vals[i][j].value.n
						= (int)RES_ROWS(db_res)[i].values[j].val.bitmap_val;
				break;
				case DB1_BIGINT:
					res->vals[i][j].flags = PV_VAL_STR;
					res->vals[i][j].value.s.len = 21*sizeof(char);
					res->vals[i][j].value.s.s
						= (char*)pkg_malloc(res->vals[i][j].value.s.len);
					if(res->vals[i][j].value.s.s==NULL)
					{
						LM_ERR("no more memory\n");
						goto error;
					}
					db_longlong2str(RES_ROWS(db_res)[i].values[j].val.ll_val,
							res->vals[i][j].value.s.s, &res->vals[i][j].value.s.len);
				break;
				default:
					res->vals[i][j].flags = PV_VAL_NULL;
			}
			if(res->vals[i][j].flags == PV_VAL_STR && sv.s)
			{
				if(sv.len<=0)
				{
					res->vals[i][j].value.s.s = _sql_empty_buf;
					res->vals[i][j].value.s.len = 0;
					continue;
				}
				res->vals[i][j].value.s.s 
					= (char*)pkg_malloc(sv.len*sizeof(char));
				if(res->vals[i][j].value.s.s==NULL)
				{
					LM_ERR("no more memory\n");
					goto error;
				}
				memcpy(res->vals[i][j].value.s.s, sv.s, sv.len);
				res->vals[i][j].value.s.len = sv.len;
			}
		}
	}

	con->dbf.free_result(con->dbh, db_res);
	return 1;

error:
	con->dbf.free_result(con->dbh, db_res);
	sql_reset_result(res);
	return -1;
}

int sql_do_query_async(sql_con_t *con, str *query)
{
	if(query==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	if(con->dbf.raw_query_async==NULL) {
		LM_ERR("the db driver module doesn't support async query\n");
		return -1;
	}
	if(con->dbf.raw_query_async(con->dbh, query)!=0)
	{
		LM_ERR("cannot do the query\n");
		return -1;
	}
	return 1;
}

#ifdef WITH_XAVP
int sql_exec_xquery(struct sip_msg *msg, sql_con_t *con, str *query,
		str *xavp)
{
	db1_res_t* db_res = NULL;
	sr_xavp_t *row = NULL;
	sr_xval_t val;
	int i, j;

	if(msg==NULL || query==NULL || xavp==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(con->dbf.raw_query(con->dbh, query, &db_res)!=0)
	{
		LM_ERR("cannot do the query\n");
		return -1;
	}

	if(db_res==NULL || RES_ROW_N(db_res)<=0 || RES_COL_N(db_res)<=0)
	{
		LM_DBG("no result after query\n");
		con->dbf.free_result(con->dbh, db_res);
		return 2;
	}

	for(i=RES_ROW_N(db_res)-1; i>=0; i--)
	{
		row = NULL;
		for(j=RES_COL_N(db_res)-1; j>=0; j--)
		{
			if(RES_ROWS(db_res)[i].values[j].nul)
			{
				val.type = SR_XTYPE_NULL;
			} else
			{
				switch(RES_ROWS(db_res)[i].values[j].type)
				{
					case DB1_STRING:
						val.type = SR_XTYPE_STR;
						val.v.s.s=
							(char*)RES_ROWS(db_res)[i].values[j].val.string_val;
						val.v.s.len=strlen(val.v.s.s);
					break;
					case DB1_STR:
						val.type = SR_XTYPE_STR;
						val.v.s.len=
							RES_ROWS(db_res)[i].values[j].val.str_val.len;
						val.v.s.s=
							(char*)RES_ROWS(db_res)[i].values[j].val.str_val.s;
					break;
					case DB1_BLOB:
						val.type = SR_XTYPE_STR;
						val.v.s.len=
							RES_ROWS(db_res)[i].values[j].val.blob_val.len;
						val.v.s.s=
							(char*)RES_ROWS(db_res)[i].values[j].val.blob_val.s;
					break;
					case DB1_INT:
						val.type = SR_XTYPE_INT;
						val.v.i
							= (int)RES_ROWS(db_res)[i].values[j].val.int_val;
					break;
					case DB1_DATETIME:
						val.type = SR_XTYPE_INT;
						val.v.i
							= (int)RES_ROWS(db_res)[i].values[j].val.time_val;
					break;
					case DB1_BITMAP:
						val.type = SR_XTYPE_INT;
						val.v.i
							= (int)RES_ROWS(db_res)[i].values[j].val.bitmap_val;
					break;
					case DB1_BIGINT:
						val.type = SR_XTYPE_LLONG;
						val.v.ll
							= RES_ROWS(db_res)[i].values[j].val.ll_val;
					break;
					default:
						val.type = SR_XTYPE_NULL;
				}
			}
			/* Add column to current row, under the column's name */
			LM_DBG("Adding column: %.*s\n", RES_NAMES(db_res)[j]->len, RES_NAMES(db_res)[j]->s);
			xavp_add_value(RES_NAMES(db_res)[j], &val, &row);
		}
		/* Add row to result xavp */
		val.type = SR_XTYPE_XAVP;
		val.v.xavp = row;
		LM_DBG("Adding row\n");
		xavp_add_value(xavp, &val, NULL);
	}

	con->dbf.free_result(con->dbh, db_res);
	return 1;
}

int sql_do_xquery(struct sip_msg *msg, sql_con_t *con, pv_elem_t *query,
		pv_elem_t *res)
{
	str sv, xavp;
	if(msg==NULL || query==NULL || res==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	if(pv_printf_s(msg, query, &sv)!=0)
	{
		LM_ERR("cannot print the sql query\n");
		return -1;
	}

	if(pv_printf_s(msg, res, &xavp)!=0)
	{
		LM_ERR("cannot print the result parameter\n");
		return -1;
	}
	return sql_exec_xquery(msg, con, &sv, &xavp);
}

#endif


int sql_do_pvquery(struct sip_msg *msg, sql_con_t *con, pv_elem_t *query,
		pvname_list_t *res)
{
	db1_res_t* db_res = NULL;
	pvname_list_t* pv;
	str sv;
	int i, j;

	if(msg==NULL || query==NULL || res==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	if(pv_printf_s(msg, query, &sv)!=0)
	{
		LM_ERR("cannot print the sql query\n");
		return -1;
	}

	if(con->dbf.raw_query(con->dbh, &sv, &db_res)!=0)
	{
		LM_ERR("cannot do the query\n");
		return -1;
	}

	if(db_res==NULL || RES_ROW_N(db_res)<=0 || RES_COL_N(db_res)<=0)
	{
		LM_DBG("no result after query\n");
		con->dbf.free_result(con->dbh, db_res);
		return 2;
	}

	for(i=RES_ROW_N(db_res)-1; i>=0; i--)
	{
		pv = res;
		for(j=0; j<RES_COL_N(db_res); j++)
		{
			if (pv == NULL) {
				LM_ERR("Missing pv spec for column %d\n", j+1);
				goto error;
			}
			if (db_val2pv_spec(msg, &RES_ROWS(db_res)[i].values[j], &pv->sname) != 0) {
				LM_ERR("Failed to convert value for column %.*s (row %d)\n",
				       RES_NAMES(db_res)[j]->len, RES_NAMES(db_res)[j]->s, i);
				goto error;
			}
			pv = pv->next;
		}
	}

	con->dbf.free_result(con->dbh, db_res);
	return 1;

error:
	con->dbf.free_result(con->dbh, db_res);
	return -1;
}


int sql_parse_param(char *val)
{
	str name;
	str tok;
	str in;
	char *p;

	/* parse: name=>db_url*/
	in.s = val;
	in.len = strlen(in.s);
	p = in.s;

	while(p<in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in.s+in.len || *p=='\0')
		goto error;
	name.s = p;
	while(p < in.s + in.len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
			break;
		p++;
	}
	if(p>in.s+in.len || *p=='\0')
		goto error;
	name.len = p - name.s;
	if(*p!='=')
	{
		while(p<in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in.s+in.len || *p=='\0' || *p!='=')
			goto error;
	}
	p++;
	if(*p!='>')
		goto error;
	p++;
	while(p<in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	tok.s = p;
	tok.len = in.len + (int)(in.s - p);

	LM_DBG("cname: [%.*s] url: [%.*s]\n", name.len, name.s, tok.len, tok.s);

	return sql_init_con(&name, &tok);
error:
	LM_ERR("invalid sqlops parameter [%.*s] at [%d]\n", in.len, in.s,
			(int)(p-in.s));
	return -1;
}

void sql_destroy(void)
{
	sql_result_t *r;
	sql_result_t *r0;
	
	sql_disconnect();

	r=_sql_result_root;
	while(r)
	{
		r0 = r->next;
		sql_reset_result(r);
		pkg_free(r);
		r = r0;
	}
	_sql_result_root = NULL;
}

/**
 *
 */
int sqlops_do_query(str *scon, str *squery, str *sres)
{
	sql_con_t *con = NULL;
	sql_result_t *res = NULL;

	con = sql_get_connection(scon);
	if(con==NULL)
	{
		LM_ERR("invalid connection [%.*s]\n", scon->len, scon->s);
		goto error;
	}
	res = sql_get_result(sres);
	if(res==NULL)
	{
		LM_ERR("invalid result [%.*s]\n", sres->len, sres->s);
		goto error;
	}
	if(sql_do_query(con, squery, res)<0)
		goto error;

	return 0;
error:
	return -1;
}

/**
 *
 */
int sqlops_get_value(str *sres, int i, int j, sql_val_t **val)
{
	sql_result_t *res = NULL;

	res = sql_get_result(sres);
	if(res==NULL)
	{
		LM_ERR("invalid result [%.*s]\n", sres->len, sres->s);
		goto error;
	}
	if(i>=res->nrows)
	{
		LM_ERR("row index out of bounds [%d/%d]\n", i, res->nrows);
		goto error;
	}
	if(j>=res->ncols)
	{
		LM_ERR("column index out of bounds [%d/%d]\n", j, res->ncols);
		goto error;
	}
	*val = &res->vals[i][j];

	return 0;
error:
	return -1;
}

/**
 *
 */
int sqlops_is_null(str *sres, int i, int j)
{
	sql_result_t *res = NULL;

	res = sql_get_result(sres);
	if(res==NULL)
	{
		LM_ERR("invalid result [%.*s]\n", sres->len, sres->s);
		goto error;
	}
	if(i>=res->nrows)
	{
		LM_ERR("row index out of bounds [%d/%d]\n", i, res->nrows);
		goto error;
	}
	if(j>=res->ncols)
	{
		LM_ERR("column index out of bounds [%d/%d]\n", j, res->ncols);
		goto error;
	}
	if(res->vals[i][j].flags&PV_VAL_NULL)
		return 1;
	return 0;
error:
	return -1;
}

/**
 *
 */
int sqlops_get_column(str *sres, int i, str *col)
{
	sql_result_t *res = NULL;

	res = sql_get_result(sres);
	if(res==NULL)
	{
		LM_ERR("invalid result [%.*s]\n", sres->len, sres->s);
		goto error;
	}
	if(i>=res->ncols)
	{
		LM_ERR("column index out of bounds [%d/%d]\n", i, res->ncols);
		goto error;
	}
	*col = res->cols[i].name;
	return 0;
error:
	return -1;
}

/**
 *
 */
int sqlops_num_columns(str *sres)
{
	sql_result_t *res = NULL;

	res = sql_get_result(sres);
	if(res==NULL)
	{
		LM_ERR("invalid result [%.*s]\n", sres->len, sres->s);
		goto error;
	}
	return res->ncols;
error:
	return -1;
}

/**
 *
 */
int sqlops_num_rows(str *sres)
{
	sql_result_t *res = NULL;

	res = sql_get_result(sres);
	if(res==NULL)
	{
		LM_ERR("invalid result [%.*s]\n", sres->len, sres->s);
		goto error;
	}
	return res->nrows;
error:
	return -1;
}

/**
 *
 */
void sqlops_reset_result(str *sres)
{
	sql_result_t *res = NULL;

	res = sql_get_result(sres);
	if(res==NULL)
	{
		LM_ERR("invalid result [%.*s]\n", sres->len, sres->s);
		return;
	}
	sql_reset_result(res);

	return;
}

/**
 *
 */
int sqlops_do_xquery(sip_msg_t *msg, str *scon, str *squery, str *xavp)
{
	sql_con_t *con = NULL;

	con = sql_get_connection(scon);
	if(con==NULL)
	{
		LM_ERR("invalid connection [%.*s]\n", scon->len, scon->s);
		goto error;
	}
	if(sql_exec_xquery(msg, con, squery, xavp)<0)
		goto error;

	return 0;
error:
	return -1;
}

