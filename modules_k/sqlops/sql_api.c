/**
 * $Id$
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../hash_func.h"
#include "../../ut.h"

#include "sql_api.h"

sql_con_t *_sql_con_root = NULL;
sql_result_t *_sql_result_root = NULL;

static str _sql_empty_str = {"", 0};

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
		if (!DB_CAPABILITY(sc->dbf, DB_CAP_ALL))
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
	sr = (sql_result_t*)pkg_malloc(sizeof(sql_result_t));
	if(sr==NULL)
	{
		LM_ERR("no pkg memory\n");
		return NULL;
	}
	memset(sr, 0, sizeof(sql_result_t));
	sr->name = *name;
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
			for(j=0; j<res->ncols; j++)
			{
				if(res->vals[i][j].flags&PV_VAL_STR
						&& res->vals[i][j].value.s.len>0)
					pkg_free(res->vals[i][j].value.s.s);
			}
			pkg_free(res->vals[i]);
		}
		pkg_free(res->vals);
		res->vals = NULL;
	}
	res->nrows = 0;
	res->ncols = 0;
}

int sql_do_query(struct sip_msg *msg, sql_con_t *con, pv_elem_t *query,
		sql_result_t *res)
{
	db1_res_t* db_res = NULL;
	int i, j;
	str sq;

	if(msg==NULL || query==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	sql_reset_result(res);
	if(pv_printf_s(msg, query, &sq)!=0)
	{
		LM_ERR("cannot print the sql query\n");
		return -1;
	}
	if(con->dbf.raw_query(con->dbh, &sq, &db_res)!=0)
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
			switch(RES_ROWS(db_res)[i].values[j].type)
			{
				case DB1_STRING:
					res->vals[i][j].flags = PV_VAL_STR;
					sq.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.string_val;
					sq.len=strlen(sq.s);
				break;
				case DB1_STR:
					res->vals[i][j].flags = PV_VAL_STR;
					sq.len=
						RES_ROWS(db_res)[i].values[j].val.str_val.len;
					sq.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.str_val.s;
				break;
				case DB1_BLOB:
					res->vals[i][j].flags = PV_VAL_STR;
					sq.len=
						RES_ROWS(db_res)[i].values[j].val.blob_val.len;
					sq.s=
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
				default:
					res->vals[i][j].flags = PV_VAL_NULL;
			}
			if(res->vals[i][j].flags == PV_VAL_STR)
			{
				if(sq.len==0)
				{
					res->vals[i][j].value.s = _sql_empty_str;
					continue;
				}
				res->vals[i][j].value.s.s 
					= (char*)pkg_malloc(sq.len*sizeof(char));
				if(res->vals[i][j].value.s.s==NULL)
				{
					LM_ERR("no more memory\n");
					goto error;
				}
				memcpy(res->vals[i][j].value.s.s, sq.s, sq.len);
				res->vals[i][j].value.s.len = sq.len;
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
	LM_ERR("invalid htable parameter [%.*s] at [%d]\n", in.len, in.s,
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
}
