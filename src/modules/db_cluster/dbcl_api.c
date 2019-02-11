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

#include "../../core/dprint.h"
#include "../../core/hashes.h"
#include "../../core/trim.h"
#include "../../core/globals.h"
#include "../../lib/srdb1/db.h"
#include "../../core/timer.h"

#include "dbcl_data.h"
#include "dbcl_api.h"

extern int dbcl_max_query_length;

#define DBCL_READ(qfunc, command) \
	do {\
	int ret;\
	int i;\
	int j;\
	int k;\
	unsigned int sec = 0;\
	db1_con_t  *dbh=NULL;\
	dbcl_cls_t *cls=NULL;\
	cls = (dbcl_cls_t*)_h->tail;\
	ret = -1;\
	for(i=DBCL_PRIO_SIZE-1; i>0; i--)\
	{\
		if(cls->rlist[i].clen<=0) continue; \
		switch(cls->rlist[i].mode) {\
			case 's':\
			case 'S':\
				for(j=0; j<cls->rlist[i].clen; j++)\
				{\
					if(dbcl_valid_con(cls->rlist[i].clist[j])==0)\
					{\
						LM_DBG("serial operation - cluster [%.*s] (%d/%d)\n",\
								cls->name.len, cls->name.s, i, j);\
						sec = get_ticks();\
						dbh = cls->rlist[i].clist[j]->dbh;\
						if(cls->rlist[i].clist[j]->dbf.qfunc==NULL) {\
							LM_ERR("unsupported command by db connector\n");\
							return -1;\
						}\
						ret = cls->rlist[i].clist[j]->dbf.command;\
						if (ret==0) {\
							cls->usedcon = cls->rlist[i].clist[j];\
							return 0;\
						} else {\
							LM_DBG("serial operation - failre on cluster"\
									" [%.*s] (%d/%d)\n",\
									cls->name.len, cls->name.s, i, j);\
							sec = get_ticks() - sec;\
							if(sec >= dbcl_max_query_length){\
								dbcl_inactive_con(cls->rlist[i].clist[j]);\
							}\
						}\
					}\
				}\
				break;\
			case 'r':\
			case 'R':\
				for(k=0; k<cls->rlist[i].clen; k++)\
				{\
					j = (process_no + k + cls->rlist[i].crt) % cls->rlist[i].clen;\
					if(dbcl_valid_con(cls->rlist[i].clist[j])==0)\
					{\
						LM_DBG("round robin operation - cluster [%.*s] (%d/%d)\n",\
								cls->name.len, cls->name.s, i, j);\
						sec = get_ticks();\
						dbh = cls->rlist[i].clist[j]->dbh;\
						if(cls->rlist[i].clist[j]->dbf.qfunc==NULL) {\
							LM_ERR("unsupported command by db connector\n");\
							return -1;\
						}\
						ret = cls->rlist[i].clist[j]->dbf.command;\
						if (ret==0)\
						{\
							cls->usedcon = cls->rlist[i].clist[j];\
							cls->rlist[i].crt = (j+1) % cls->rlist[i].clen;\
							return 0;\
						} else {\
							LM_DBG("round robin operation - failre on cluster"\
									" [%.*s] (%d/%d)\n",\
									cls->name.len, cls->name.s, i, j);\
							sec = get_ticks() - sec;\
							if(sec >= dbcl_max_query_length){\
								dbcl_inactive_con(cls->rlist[i].clist[j]);\
							}\
						}\
					}\
				}\
				break;\
			default:\
				LM_ERR("invalid mode %c (%d)\n", cls->rlist[i].mode,\
							cls->rlist[i].mode);\
				return -1;\
		}\
	}\
	LM_DBG("no successful read on cluster [%.*s]\n",\
			cls->name.len, cls->name.s);\
	return ret;\
	} while(0)

#define DBCL_WRITE(qfunc, command) \
	do {\
	int ret;\
	int rc;\
	int rok;\
	int i;\
	int j;\
	int k;\
	unsigned int sec = 0;\
	db1_con_t  *dbh=NULL;\
	dbcl_cls_t *cls=NULL;\
	cls = (dbcl_cls_t*)_h->tail;\
	ret = -1;\
	rok = 0;\
	rc = 0;\
	for(i=DBCL_PRIO_SIZE-1; i>0; i--)\
	{\
		if(cls->wlist[i].clen<=0) continue; \
		switch(cls->wlist[i].mode) {\
			case 's':\
			case 'S':\
				for(j=0; j<cls->wlist[i].clen; j++)\
				{\
					if(dbcl_valid_con(cls->wlist[i].clist[j])==0)\
					{\
						LM_DBG("serial operation - cluster [%.*s] (%d/%d)\n",\
								cls->name.len, cls->name.s, i, j);\
						sec = get_ticks();\
						dbh = cls->wlist[i].clist[j]->dbh;\
						if(cls->wlist[i].clist[j]->dbf.qfunc==NULL) {\
							LM_ERR("unsupported command by db connector\n");\
							return -1;\
						}\
						ret = cls->wlist[i].clist[j]->dbf.command;\
						if (ret==0) {\
							cls->usedcon = cls->wlist[i].clist[j];\
							return 0;\
						} else {\
							LM_DBG("serial operation - failure on cluster"\
									" [%.*s] (%d/%d)\n",\
									cls->name.len, cls->name.s, i, j);\
							sec = get_ticks() - sec;\
							if(sec >= dbcl_max_query_length){\
								dbcl_inactive_con(cls->wlist[i].clist[j]);\
							}\
						}\
					}\
				}\
				break;\
			case 'r':\
			case 'R':\
				for(k=0; k<cls->wlist[i].clen; k++)\
				{\
					j = (process_no + k + cls->wlist[i].crt) % cls->wlist[i].clen;\
					if(dbcl_valid_con(cls->wlist[i].clist[j])==0)\
					{\
						LM_DBG("round robin operation - cluster [%.*s] (%d/%d)\n",\
								cls->name.len, cls->name.s, i, j);\
						sec = get_ticks();\
						dbh = cls->wlist[i].clist[j]->dbh;\
						if(cls->wlist[i].clist[j]->dbf.qfunc==NULL) {\
							LM_ERR("unsupported command by db connector\n");\
							return -1;\
						}\
						ret = cls->wlist[i].clist[j]->dbf.command;\
						if (ret==0)\
						{\
							cls->usedcon = cls->wlist[i].clist[j];\
							cls->wlist[i].crt = (j+1) % cls->wlist[i].clen;\
							return 0;\
						} else {\
							LM_DBG("round robin operation - failure on cluster"\
									" [%.*s] (%d/%d)\n",\
									cls->name.len, cls->name.s, i, j);\
							sec = get_ticks() - sec;\
							if(sec >= dbcl_max_query_length){\
								dbcl_inactive_con(cls->wlist[i].clist[j]);\
							}\
						}\
					}\
				}\
				break;\
			case 'p':\
			case 'P':\
				for(j=0; j<cls->wlist[i].clen; j++)\
				{\
					if(dbcl_valid_con(cls->wlist[i].clist[j])==0)\
					{\
						LM_DBG("parallel operation - cluster [%.*s] (%d/%d)\n",\
								cls->name.len, cls->name.s, i, j);\
						sec = get_ticks();\
						dbh = cls->wlist[i].clist[j]->dbh;\
						if(cls->wlist[i].clist[j]->dbf.qfunc==NULL) {\
							LM_ERR("unsupported command by db connector\n");\
							return -1;\
						}\
						rc = cls->wlist[i].clist[j]->dbf.command;\
						if(rc==0) {\
							cls->usedcon = cls->wlist[i].clist[j];\
							rok = 1;\
						} else {\
							LM_DBG("parallel operation - failure on cluster"\
									" [%.*s] (%d/%d)\n",\
									cls->name.len, cls->name.s, i, j);\
							sec = get_ticks() - sec;\
							if(sec >= dbcl_max_query_length){\
								dbcl_inactive_con(cls->wlist[i].clist[j]);\
							}\
						}\
						ret |= rc;\
					}\
				}\
				if (rok==1 && cls->wlist[i].clen>0)\
					return 0;\
				break;\
			default:\
				LM_ERR("invalid mode %c (%d)\n", cls->rlist[i].mode,\
						cls->rlist[i].mode);\
				return -1;\
		}\
	}\
	LM_DBG("no successful write on cluster [%.*s]\n",\
			cls->name.len, cls->name.s);\
	return ret;\
	} while(0)



/*! \brief
 * Initialize database connection
 */
db1_con_t* db_cluster_init(const str* _dburl)
{
	db1_con_t  *h=NULL;
	dbcl_cls_t *cls=NULL;
	str name;

	LM_DBG("initializing with cluster [%.*s]\n", _dburl->len, _dburl->s);
	if(_dburl->len<10 || strncmp(_dburl->s, "cluster://", 10)!=0)
	{
		LM_ERR("invlaid url for cluster module [%.*s]\n",
				_dburl->len, _dburl->s);
		return NULL;
	}
	name.s = _dburl->s + 10;
	name.len = _dburl->len - 10;
	trim(&name);
	cls = dbcl_get_cluster(&name);
	if(cls==NULL)
	{
		LM_ERR("cluster not found [%.*s]\n",
				_dburl->len, _dburl->s);
		return NULL;
	}
	if(dbcl_init_dbf(cls)<0)
	{
		LM_ERR("cluster [%.*s] - unable to bind to DB engines\n",
				_dburl->len, _dburl->s);
		return NULL;
	}
	dbcl_init_connections(cls);
	cls->ref++;
	h = (db1_con_t*)pkg_malloc(sizeof(db1_con_t));
	if (h==NULL) {
		LM_ERR("out of pkg\n");
		return NULL;
	}
	memset(h, 0, sizeof(db1_con_t));
	h->tail = (unsigned long)cls;
	return h;
}


/*! \brief
 * Close a database connection
 */
void db_cluster_close(db1_con_t* _h)
{
	dbcl_cls_t *cls=NULL;
	LM_DBG("executing db cluster close command\n");
	cls = (dbcl_cls_t*)_h->tail;
 	cls->ref--;
 	if(cls->ref <= 0) {
		/* close connections */
		dbcl_close_connections(cls);
	}
	/* free _h - allocated for each db_cluster_init() */
	pkg_free(_h);
	return;
}


/*! \brief
 * Free all memory allocated by get_result
 */
int db_cluster_free_result(db1_con_t* _h, db1_res_t* _r)
{
	dbcl_cls_t *cls=NULL;
	LM_DBG("executing db cluster free-result command\n");
	cls = (dbcl_cls_t*)_h->tail;
	if(cls->usedcon==NULL || cls->usedcon->dbh==NULL)
		return -1;
	return cls->usedcon->dbf.free_result(cls->usedcon->dbh, _r);
}


/*! \brief
 * Do a query
 */
int db_cluster_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	     const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	     const db_key_t _o, db1_res_t** _r)
{
	LM_DBG("executing db cluster query command\n");
	DBCL_READ(query, query(dbh, _k, _op, _v, _c, _n, _nc, _o, _r));
}


/*! \brief
 * fetch rows from a result
 */
int db_cluster_fetch_result(const db1_con_t* _h, db1_res_t** _r, const int nrows)
{
	dbcl_cls_t *cls=NULL;
	LM_DBG("executing db cluster fetch-result command\n");
	cls = (dbcl_cls_t*)_h->tail;
	if(cls->usedcon==NULL || cls->usedcon->dbh==NULL
			|| cls->usedcon->dbf.fetch_result==NULL)
		return -1;
	return cls->usedcon->dbf.fetch_result(cls->usedcon->dbh, _r, nrows);
}


/*! \brief
 * Raw SQL query
 */
int db_cluster_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	LM_DBG("executing db cluster raw query command\n");
	DBCL_READ(raw_query, raw_query(dbh, _s, _r));
}


/*! \brief
 * Insert a row into table
 */
int db_cluster_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n)
{
	LM_DBG("executing db cluster insert command\n");
	DBCL_WRITE(insert, insert(dbh, _k, _v, _n));
}


/*! \brief
 * Delete a row from table
 */
int db_cluster_delete(const db1_con_t* _h, const db_key_t* _k, const 
	db_op_t* _o, const db_val_t* _v, const int _n)
{
	LM_DBG("executing db cluster delete command\n");
	DBCL_WRITE(delete, delete(dbh, _k, _o, _v, _n));
}


/*! \brief
 * Update a row in table
 */
int db_cluster_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
	const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n,
	const int _un)
{
	LM_DBG("executing db cluster update command\n");
	DBCL_WRITE(update, update(dbh, _k, _o, _v, _uk, _uv, _n, _un));
}


/*! \brief
 * Just like insert, but replace the row if it exists
 */
int db_cluster_replace(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const int _n, const int _un, const int _m)
{
	LM_DBG("executing db cluster replace command\n");
	DBCL_WRITE(replace, replace(dbh, _k, _v, _n, _un, _m));
}

/*! \brief
 * Returns the last inserted ID
 */
int db_cluster_last_inserted_id(const db1_con_t* _h)
{
	dbcl_cls_t *cls=NULL;
	LM_DBG("executing db cluster last inserted id command\n");
	cls = (dbcl_cls_t*)_h->tail;
	if(cls->usedcon==NULL || cls->usedcon->dbh==NULL
			|| cls->usedcon->dbf.last_inserted_id==NULL)
		return -1;
	return cls->usedcon->dbf.last_inserted_id(cls->usedcon->dbh);
}


/*! \brief
 * Returns number of affected rows for last query
 */
int db_cluster_affected_rows(const db1_con_t* _h)
{
	dbcl_cls_t *cls=NULL;
	LM_DBG("executing db cluster affected-rows command\n");
	cls = (dbcl_cls_t*)_h->tail;
	if(cls->usedcon==NULL || cls->usedcon->dbh==NULL
			|| cls->usedcon->dbf.affected_rows==NULL)
		return -1;
	return cls->usedcon->dbf.affected_rows(cls->usedcon->dbh);
}


/*! \brief
 * Insert a row into table, update on duplicate key
 */
int db_cluster_insert_update(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n)
{
	LM_DBG("executing db cluster insert-update command\n");
	DBCL_WRITE(insert_update, insert_update(dbh, _k, _v, _n));
}


/*! \brief
 * Insert a row into table
 */
int db_cluster_insert_delayed(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const int _n)
{
	LM_DBG("executing db cluster insert delayed command\n");
	DBCL_WRITE(insert_delayed, insert_delayed(dbh, _k, _v, _n));
}


/*! \brief
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_cluster_use_table(db1_con_t* _h, const str* _t)
{
	int i;
	int j;
	int ret;
	dbcl_cls_t *cls=NULL;

	cls = (dbcl_cls_t*)_h->tail;

	ret = 0;
	LM_DBG("use table (%.*s) - cluster [%.*s]\n",
							_t->len, _t->s, cls->name.len, cls->name.s);
	for(i=DBCL_PRIO_SIZE-1; i>0; i--)
	{
		for(j=0; j<cls->rlist[i].clen; j++)
		{
			if(cls->rlist[i].clist[j] != NULL && cls->rlist[i].clist[j]->flags!=0
					&& cls->rlist[i].clist[j]->dbh != NULL)
			{
				LM_DBG("set read table (%.*s) - cluster [%.*s] (%d/%d)\n",
							_t->len, _t->s, cls->name.len, cls->name.s, i, j);
				ret |= cls->rlist[i].clist[j]->dbf.use_table(cls->rlist[i].clist[j]->dbh, _t);
			}
		}
		for(j=0; j<cls->wlist[i].clen; j++)
		{
			if(cls->wlist[i].clist[j] != NULL && cls->wlist[i].clist[j]->flags!=0
					&& cls->wlist[i].clist[j]->dbh != NULL)
			{
				LM_DBG("set write table (%.*s) - cluster [%.*s] (%d/%d)\n",
							_t->len, _t->s, cls->name.len, cls->name.s, i, j);
				ret |= cls->wlist[i].clist[j]->dbf.use_table(cls->wlist[i].clist[j]->dbh, _t);
			}
		}

	}

	return ret;
}
