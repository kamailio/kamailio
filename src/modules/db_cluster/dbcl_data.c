/*
 * DB CLuster core functions
 *
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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
 */

/*! \file
 *  \brief DB_CLUSTER :: Core
 *  \ingroup db_cluster
 *  Module: \ref db_cluster
 */

#include "../../core/parser/parse_param.h"
#include "../../core/dprint.h"
#include "../../core/hashes.h"
#include "../../core/trim.h"
#include "../../core/timer.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"

#include "dbcl_data.h"


static dbcl_con_t *_dbcl_con_root = NULL;
static dbcl_cls_t *_dbcl_cls_root = NULL;

dbcl_con_t *dbcl_get_connection(str *name)
{
	dbcl_con_t *sc;
	unsigned int conid;

	conid = core_case_hash(name, 0, 0);
	sc = _dbcl_con_root;
	while(sc)
	{
		if(conid==sc->conid && sc->name.len==name->len
				&& strncmp(sc->name.s, name->s, name->len)==0)
		{
			LM_DBG("connection found [%.*s]\n", name->len, name->s);
			return sc;
		}
		sc = sc->next;
	}
	return NULL;
}

dbcl_cls_t *dbcl_get_cluster(str *name)
{
	dbcl_cls_t *sc;
	unsigned int clsid;

	clsid = core_case_hash(name, 0, 0);
	sc = _dbcl_cls_root;
	while(sc)
	{
		if(clsid==sc->clsid && sc->name.len==name->len
				&& strncmp(sc->name.s, name->s, name->len)==0)
		{
			LM_DBG("cluster found [%.*s]\n", name->len, name->s);
			return sc;
		}
		sc = sc->next;
	}
	return NULL;
}


int dbcl_init_con(str *name, str *url)
{
	dbcl_con_t *sc;
	unsigned int conid;

	conid = core_case_hash(name, 0, 0);

	sc = _dbcl_con_root;
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
	sc = (dbcl_con_t*)pkg_malloc(sizeof(dbcl_con_t));
	if(sc==NULL)
	{
		LM_ERR("no pkg memory\n");
		return -1;
	}
	memset(sc, 0, sizeof(dbcl_con_t));
	sc->conid = conid;
	sc->name = *name;
	sc->db_url = *url;
	sc->sinfo = (dbcl_shared_t*)shm_malloc(sizeof(dbcl_shared_t));
	if(sc->sinfo==NULL)
	{
		LM_ERR("no shm memory\n");
		pkg_free(sc);
		return -1;
	}
	memset(sc->sinfo, 0, sizeof(dbcl_shared_t));
	sc->next = _dbcl_con_root;
	_dbcl_con_root = sc;

	return 0;
}

int dbcl_valid_con(dbcl_con_t *sc)
{
	if(sc==NULL || sc->flags==0 || sc->dbh==NULL)
		return -1;
	if(sc->sinfo==NULL)
		return 0;
	if(sc->sinfo->state & DBCL_CON_INACTIVE)
	{
		if(sc->sinfo->aticks==0)
			return -1;
		if(sc->sinfo->aticks>get_ticks())
			return -1;
		sc->sinfo->aticks = 0;
		sc->sinfo->state &= ~DBCL_CON_INACTIVE;
	}
	return 0;
}

extern int dbcl_inactive_interval;

int dbcl_inactive_con(dbcl_con_t *sc)
{
	if(sc==NULL || sc->sinfo==NULL)
		return -1;
	sc->sinfo->aticks = get_ticks() + dbcl_inactive_interval;
	sc->sinfo->state |= DBCL_CON_INACTIVE;
	return 0;
}

int dbcl_parse_con_param(char *val)
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

	LM_DBG("connection: [%.*s] url: [%.*s]\n", name.len, name.s, tok.len, tok.s);

	return dbcl_init_con(&name, &tok);
error:
	LM_ERR("invalid connection parameter [%.*s] at [%d]\n", in.len, in.s,
			(int)(p-in.s));
	return -1;
}

/**
 * cons: conid1=1s1p;...
 */
int dbcl_cls_set_connections(dbcl_cls_t *cls, str *cons)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	dbcl_con_t *sc;
	str s;
	int i;

	if(cls==NULL || cons==NULL)
		return -1;
	s = *cons;
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0)
		return -1;
	for (pit = params_list; pit; pit=pit->next)
	{
		sc = dbcl_get_connection(&pit->name);
		if(sc==NULL)
		{
			LM_ERR("invalid connection id [%.*s]\n",
					pit->name.len, pit->name.s);
			goto error;
		}
		s = pit->body;
		trim(&s);
		if(s.len!=4)
		{
			LM_ERR("invalid parameter [%.*s] for connection id [%.*s]\n",
					pit->body.len, pit->body.s,
					pit->name.len, pit->name.s);
			goto error;
		}
		if(s.s[0]<'0' || s.s[0]>'9')
		{
			LM_ERR("invalid parameter [%.*s] for connection id [%.*s]\n",
					pit->body.len, pit->body.s,
					pit->name.len, pit->name.s);
			goto error;
		}
		i = s.s[0] - '0';
		if(s.s[1]!='s' && s.s[1]!='S' && s.s[1]!='r' && s.s[1]!='R')
		{
			LM_ERR("invalid parameter [%.*s] for connection id [%.*s]\n",
					pit->body.len, pit->body.s,
					pit->name.len, pit->name.s);
			goto error;
		}
		if(cls->rlist[i].clen<DBCL_CLIST_SIZE)
		{
			if(cls->rlist[i].mode==0)
				cls->rlist[i].mode = s.s[1] | 32;
			cls->rlist[i].prio = i;
			cls->rlist[i].clist[cls->rlist[i].clen] = sc;
			LM_DBG("added con-id [%.*s] to rlist[%d] at [%d]\n",
					pit->name.len, pit->name.s, i, cls->rlist[i].clen);
			cls->rlist[i].clen++;
		} else {
			LM_WARN("too many read connections in cluster - con-id [%.*s]\n",
					pit->name.len, pit->name.s);
		}
		if(s.s[2]<'0' || s.s[2]>'9')
		{
			LM_ERR("invalid parameter [%.*s] for connection id [%.*s]\n",
					pit->body.len, pit->body.s,
					pit->name.len, pit->name.s);
			goto error;
		}
		i = s.s[2] - '0';
		if(s.s[3]!='s' && s.s[3]!='S' && s.s[3]!='r' && s.s[3]!='R'
				 && s.s[3]!='p' && s.s[3]!='P')
		{
			LM_ERR("invalid parameter [%.*s] for connection id [%.*s]\n",
					pit->body.len, pit->body.s,
					pit->name.len, pit->name.s);
			goto error;
		}
		if(cls->wlist[i].clen<DBCL_CLIST_SIZE)
		{
			if(cls->wlist[i].mode==0)
				cls->wlist[i].mode = s.s[3] | 32;
			cls->wlist[i].prio = i;
			cls->wlist[i].clist[cls->wlist[i].clen] = sc;
			LM_DBG("added con-id [%.*s] to wlist[%d] at [%d]\n",
					pit->name.len, pit->name.s, i, cls->wlist[i].clen);
			cls->wlist[i].clen++;
		} else {
			LM_WARN("too many write connections in cluster - con-id [%.*s]\n",
					pit->name.len, pit->name.s);
		}
	}
	return 0;
error:
	return -1;
}

int dbcl_init_cls(str *name, str *cons)
{
	dbcl_cls_t *sc;
	unsigned int clsid;

	clsid = core_case_hash(name, 0, 0);

	sc = _dbcl_cls_root;
	while(sc)
	{
		if(clsid==sc->clsid && sc->name.len==name->len
				&& strncmp(sc->name.s, name->s, name->len)==0)
		{
			LM_ERR("duplicate cluster name\n");
			return -1;
		}
		sc = sc->next;
	}
	sc = (dbcl_cls_t*)pkg_malloc(sizeof(dbcl_cls_t));
	if(sc==NULL)
	{
		LM_ERR("no pkg memory\n");
		return -1;
	}
	memset(sc, 0, sizeof(dbcl_cls_t));
	sc->clsid = clsid;
	sc->name = *name;
	/* parse cls con list */
	if(dbcl_cls_set_connections(sc, cons)<0)
	{
		LM_ERR("unable to add connections to cluster definition\n");
		pkg_free(sc);
		return -1;
	}
	sc->next = _dbcl_cls_root;
	_dbcl_cls_root = sc;

	return 0;
}

int dbcl_parse_cls_param(char *val)
{
	str name;
	str tok;
	str in;
	char *p;

	/* parse: name=>conlist*/
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

	LM_DBG("cluster: [%.*s] : con-list [%.*s]\n", name.len, name.s, tok.len, tok.s);

	return dbcl_init_cls(&name, &tok);
error:
	LM_ERR("invalid cluster parameter [%.*s] at [%d]\n", in.len, in.s,
			(int)(p-in.s));
	return -1;
}

int dbcl_init_dbf(dbcl_cls_t *cls)
{
	int i;
	int j;

	for(i=1; i<DBCL_PRIO_SIZE; i++)
	{
		for(j=0; j<cls->rlist[i].clen; j++)
		{
			if(cls->rlist[i].clist[j] != NULL && cls->rlist[i].clist[j]->flags==0)
			{
				if(db_bind_mod(&cls->rlist[i].clist[j]->db_url,
							&cls->rlist[i].clist[j]->dbf)<0)
				{	
					LM_ERR("unable to bind database module\n");
					return -1;
				}
				cls->rlist[i].clist[j]->flags = 1;
			}
		}
		for(j=0; j<cls->wlist[i].clen; j++)
		{
			if(cls->wlist[i].clist[j] != NULL && cls->wlist[i].clist[j]->flags==0)
			{
				if(db_bind_mod(&cls->wlist[i].clist[j]->db_url,
							&cls->wlist[i].clist[j]->dbf)<0)
				{	
					LM_ERR("unable to bind database module\n");
					return -1;
				}
				cls->wlist[i].clist[j]->flags = 1;
			}
		}

	}
	return 0;
}

int dbcl_init_connections(dbcl_cls_t *cls)
{
	int i;
	int j;

	for(i=1; i<DBCL_PRIO_SIZE; i++)
	{
		for(j=0; j<cls->rlist[i].clen; j++)
		{
			if(cls->rlist[i].clist[j] != NULL && cls->rlist[i].clist[j]->flags!=0
					&& cls->rlist[i].clist[j]->dbh==NULL)
			{
				LM_DBG("setting up read connection [%.*s]\n",
							cls->rlist[i].clist[j]->name.len,
							cls->rlist[i].clist[j]->name.s);
				cls->rlist[i].clist[j]->dbh = 
					cls->rlist[i].clist[j]->dbf.init(&cls->rlist[i].clist[j]->db_url);	
				if(cls->rlist[i].clist[j]->dbh==NULL)
				{
					LM_WARN("cannot connect to database - connection [%.*s]\n",
							cls->rlist[i].clist[j]->name.len,
							cls->rlist[i].clist[j]->name.s);
				}
			}
		}
		for(j=0; j<cls->wlist[i].clen; j++)
		{
			if(cls->wlist[i].clist[j] != NULL && cls->wlist[i].clist[j]->flags!=0
					&& cls->wlist[i].clist[j]->dbh==NULL)
			{
				LM_DBG("setting up write connection [%.*s]\n",
							cls->wlist[i].clist[j]->name.len,
							cls->wlist[i].clist[j]->name.s);
				cls->wlist[i].clist[j]->dbh = 
					cls->wlist[i].clist[j]->dbf.init(&cls->wlist[i].clist[j]->db_url);	
				if(cls->wlist[i].clist[j]->dbh==NULL)
				{
					LM_WARN("cannot connect to database - connection [%.*s]\n",
							cls->wlist[i].clist[j]->name.len,
							cls->wlist[i].clist[j]->name.s);
				}
			}
		}

	}
	return 0;
}

int dbcl_close_connections(dbcl_cls_t *cls)
{
	int i;
	int j;

	if(cls->ref > 0)
		return 0;
	for(i=1; i<DBCL_PRIO_SIZE; i++)
	{
		for(j=0; j<cls->rlist[i].clen; j++)
		{
			if(cls->rlist[i].clist[j] != NULL && cls->rlist[i].clist[j]->flags!=0
					&& cls->rlist[i].clist[j]->dbh != NULL)
			{
				
				cls->rlist[i].clist[j]->dbf.close(cls->rlist[i].clist[j]->dbh);
				cls->rlist[i].clist[j]->dbh = NULL;
			}
		}
		for(j=0; j<cls->wlist[i].clen; j++)
		{
			if(cls->wlist[i].clist[j] != NULL && cls->wlist[i].clist[j]->flags!=0
					&& cls->wlist[i].clist[j]->dbh != NULL)
			{
				cls->wlist[i].clist[j]->dbf.close(cls->wlist[i].clist[j]->dbh);
				cls->wlist[i].clist[j]->dbh = NULL;
			}
		}

	}
	return 0;
}
