/*
 * BDB Database Driver for Kamailio
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*! \addtogroup bdb
 * @{
 */

/*! \file
 * Berkeley DB : Implementation of functions related to database commands.
 *
 * \ingroup database
 */

#include <string.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include "bdb_cmd.h"
#include "bdb_fld.h"
#include "bdb_con.h"
#include "bdb_uri.h"
#include "bdb_res.h"
#include "bdb_lib.h"
#include "bdb_crs_compat.h"

#define BDB_BUF_SIZE 1024

/** Destroys a bdb_cmd structure.
 * This function frees all memory used by ld_cmd structure.
 * @param cmd A pointer to generic db_cmd command being freed.
 * @param payload A pointer to ld_cmd structure to be freed.
 */
static void bdb_cmd_free(db_cmd_t* cmd, bdb_cmd_t* payload)
{
	db_drv_free(&payload->gen);
	if (payload->dbcp)
		payload->dbcp->CLOSE_CURSOR(payload->dbcp);
	if(payload->skey.s)
		pkg_free(payload->skey.s);
	pkg_free(payload);
}

int bdb_prepare_query(db_cmd_t* cmd, bdb_cmd_t *bcmd)
{
	bdb_tcache_t *tbc = NULL;
	bdb_table_t *tp = NULL;
	bdb_fld_t *f;
	db_fld_t *fld;
	int mode;
	int i;

	if(bcmd->bcon==NULL || bcmd->bcon->dbp==NULL)
		return -1;
	
	tbc = bdblib_get_table(bcmd->bcon->dbp, &cmd->table);
	if(tbc==NULL)
	{
		ERR("bdb: table does not exist!\n");
		return -1;
	}

	tp = tbc->dtp;
	if(tp==NULL || tp->db==NULL)
	{
		ERR("bdb: table not loaded!\n");
		return -1;
	}

	mode = 0;
	if (!DB_FLD_EMPTY(cmd->result)) 
	{ /* columns to be returned provided */
		if (cmd->result_count > tp->ncols)
		{
			ERR("bdb: too many columns in query\n");
			goto error;
		}
	} else {
		mode = 1;
		cmd->result = db_fld(tp->ncols + 1);
		cmd->result_count = tp->ncols;
		for(i = 0; i < cmd->result_count; i++) {
			if (bdb_fld(cmd->result + i, cmd->table.s) < 0)
				goto error;
		}		
	}
	
	for (i = 0; i < cmd->result_count; i++) {
		fld = cmd->result + i;
		f = DB_GET_PAYLOAD(fld);
		if(mode==1)
		{
			DBG("bdb: column name [%.*s]\n", tp->colp[i]->name.len,
					tp->colp[i]->name.s);
		
			f->name = pkg_malloc(tp->colp[i]->name.len+1);
			if (f->name == NULL) {
				ERR("bdb: Out of private memory\n");
				goto error;
			}
			strncpy(f->name, tp->colp[i]->name.s, tp->colp[i]->name.len);
			f->name[tp->colp[i]->name.len] = '\0';
			fld->name = f->name;
			fld->type = tp->colp[i]->type;
			f->col_pos = i;
		} else {
			f->col_pos = bdb_get_colpos(tp, fld->name);
			if(f->col_pos == -1)
			{
				ERR("bdb: Column not found\n");
				goto error;
			}
		}
		switch(fld->type) {
			case DB_INT:
			case DB_BITMAP:
			case DB_FLOAT:
			case DB_DOUBLE:
			case DB_DATETIME:
			case DB_STR:
				if (!f->buf.s) f->buf.s = pkg_malloc(BDB_BUF_SIZE);
				if (f->buf.s == NULL) {
					ERR("bdb: No memory left\n");
					goto error;
				}
				fld[i].v.lstr.s = f->buf.s;
			break;

			case DB_CSTR:
				if (!f->buf.s) f->buf.s = pkg_malloc(BDB_BUF_SIZE);
				if (f->buf.s == NULL) {
					ERR("bdb: No memory left\n");
					goto error;
				}
				fld[i].v.cstr = f->buf.s;
			break;

		case DB_BLOB:
				if (!f->buf.s) f->buf.s = pkg_malloc(BDB_BUF_SIZE);
				if (f->buf.s == NULL) {
					ERR("mysql: No memory left\n");
					goto error;
				}
				fld[i].v.blob.s = f->buf.s;
			break;

		case DB_NONE:
			/* Eliminates gcc warning */
			break;
		}
	}
	
	if (!DB_FLD_EMPTY(cmd->match))
	{
		if (cmd->match_count > tp->ncols)
		{
			ERR("bdb: too many columns in match struct of query\n");
			goto error;
		}
		for (i = 0; i < cmd->match_count; i++) {
			fld = cmd->result + i;
			f = DB_GET_PAYLOAD(fld);
			f->col_pos = bdb_get_colpos(tp, fld->name);
			if(f->col_pos == -1)
			{
				ERR("bdb: Match column not found\n");
				goto error;
			}
		}
	}

	return 0;

error:
	return -1;
}

int bdb_query(db_cmd_t* cmd, bdb_cmd_t *bcmd)
{
	DBT key;
	DB *db;
	static char kbuf[MAX_ROW_SIZE];
	int klen;

	bdb_tcache_t *tbc = NULL;
	bdb_table_t *tp = NULL;

	if(bcmd->bcon==NULL || bcmd->bcon->dbp==NULL)
		return -1;
	
	tbc = bdblib_get_table(bcmd->bcon->dbp, &cmd->table);
	if(tbc==NULL)
	{
		ERR("bdb: table does not exist!\n");
		return -1;
	}

	tp = tbc->dtp;
	if(tp==NULL)
	{
		ERR("bdb: table not loaded!\n");
		return -1;
	}
	db = tp->db;
	if(db==NULL)
	{
		ERR("bdb: db structure not intialized!\n");
		return -1;
	}

	if (DB_FLD_EMPTY(cmd->match))
	{ /* no match constraint */
		if (db->cursor(db, NULL, &bcmd->dbcp, 0) != 0) 
		{
			ERR("bdb: error creating cursor\n");
			goto error;
		}
		bcmd->skey.len = 0;
		return 0;
	}

	memset(&key, 0, sizeof(DBT));
	memset(kbuf, 0, MAX_ROW_SIZE);
	
	klen=MAX_ROW_SIZE;
	if(bdblib_valtochar(tp, cmd->match, cmd->match_count,
			kbuf, &klen, BDB_KEY)!=0)
	{
		ERR("bdb: error creating key\n");
		goto error;
	}
	
	if(klen > bcmd->skey_size || bcmd->skey.s==NULL)
	{
		if(bcmd->skey.s!=NULL)
			pkg_free(bcmd->skey.s);
		bcmd->skey.s = (char*)pkg_malloc(klen*sizeof(char));
		if(bcmd->skey.s == NULL)
		{
			ERR("bdb: no pkg memory\n");
			goto error;
		}
		bcmd->skey_size = klen;
	}
	memcpy(bcmd->skey.s, kbuf, klen);
	bcmd->skey.len = klen;

	return 0;
error:
	return -1;
}

int bdb_cmd(db_cmd_t* cmd)
{
	bdb_cmd_t *bcmd;
	db_con_t  *con;
	bdb_con_t *bcon;

	bcmd = (bdb_cmd_t*)pkg_malloc(sizeof(bdb_cmd_t));
	if (bcmd == NULL) {
		ERR("bdb: No memory left\n");
		goto error;
	}
	memset(bcmd, '\0', sizeof(bdb_cmd_t));
	if (db_drv_init(&bcmd->gen, bdb_cmd_free) < 0) goto error;

	con = cmd->ctx->con[db_payload_idx];
	bcon = DB_GET_PAYLOAD(con);
	bcmd->bcon = bcon;

	switch(cmd->type) {
	case DB_PUT:
	case DB_DEL:
	case DB_UPD:
		ERR("bdb: The driver does not support DB modifications yet.\n");
		goto error;
		break;

	case DB_GET:
		if(bdb_prepare_query(cmd, bcmd)!=0)
		{
			ERR("bdb: error preparing query.\n");
			goto error;
		}
		break;

	case DB_SQL:
		ERR("bdb: The driver does not support raw queries yet.\n");
		goto error;
	}

	DB_SET_PAYLOAD(cmd, bcmd);
	return 0;

error:
	if (bcmd) {
		DB_SET_PAYLOAD(cmd, NULL);
		db_drv_free(&bcmd->gen);
		pkg_free(bcmd);
	}
	return -1;
}


int bdb_cmd_exec(db_res_t* res, db_cmd_t* cmd)
{
	db_con_t* con;
	bdb_cmd_t *bcmd;
	bdb_con_t *bcon;

	/* First things first: retrieve connection info from the currently active
	 * connection and also mysql payload from the database command
	 */
	con = cmd->ctx->con[db_payload_idx];
	bcmd = DB_GET_PAYLOAD(cmd);
	bcon = DB_GET_PAYLOAD(con);

	if ((bcon->flags & BDB_CONNECTED)==0) {
		ERR("bdb: not connected\n");
		return -1;
	}
	bcmd->next_flag = -1;
	switch(cmd->type) {
		case DB_DEL:
		case DB_PUT:
		case DB_UPD:
				/* no result expected - cleanup */
				DBG("bdb: query with no result.\n");
			break;
		case DB_GET:
				return bdb_query(cmd, bcmd);
			break;
		default:
				/* result expected - no cleanup */
				DBG("bdb: query with result.\n");
	}

	return 0;
}

int bdb_update_result(db_cmd_t *cmd, DBT *data)
{
	bdb_fld_t *f;
	db_fld_t *fld;
	int i;
	int col;
	char *s;
	static str col_map[MAX_NUM_COLS];

	memset(col_map, 0, MAX_NUM_COLS*sizeof(str));

	col = 0;
	s = (char*)data->data;
	col_map[col].s = s;
	while(*s!='\0')
	{
		if(*s == *DELIM)
		{
			col_map[col].len = s - col_map[col].s;
			col++;
			col_map[col].s = s+1;
		}
		s++;
	}
	col_map[col].len = s - col_map[col].s;

	for (i = 0; i < cmd->result_count; i++) {
		fld = cmd->result + i;
		f = DB_GET_PAYLOAD(fld);
		if(col_map[f->col_pos].len == 0)
		{
			fld->flags |= DB_NULL;
			continue;
		}
		fld->flags &= ~DB_NULL;

		switch(fld->type) {
			case DB_STR:
				fld->v.lstr.s = f->buf.s;
				if(col_map[f->col_pos].len < BDB_BUF_SIZE)
				{
					fld->v.lstr.len = col_map[f->col_pos].len;
				} else {
					/* truncate ?!? */
					fld->v.lstr.len = BDB_BUF_SIZE - 1;
				}
				memcpy(fld->v.lstr.s, col_map[f->col_pos].s, fld->v.lstr.len);
			break;

			case DB_BLOB:
				fld->v.blob.s = f->buf.s;
				if(col_map[f->col_pos].len < BDB_BUF_SIZE)
				{
					fld->v.blob.len = col_map[f->col_pos].len;
				} else {
					/* truncate ?!? */
					fld->v.blob.len = BDB_BUF_SIZE - 1;
				}
				memcpy(fld->v.blob.s, col_map[f->col_pos].s, fld->v.blob.len);

			break;

			case DB_CSTR:
				fld->v.cstr = f->buf.s;
				if(col_map[f->col_pos].len < BDB_BUF_SIZE)
				{
					memcpy(fld->v.cstr, col_map[f->col_pos].s,
							col_map[f->col_pos].len);
					fld->v.cstr[col_map[f->col_pos].len] = '\0';
				} else {
					/* truncate ?!? */
					memcpy(fld->v.cstr, col_map[f->col_pos].s,
							BDB_BUF_SIZE - 1);
					fld->v.cstr[BDB_BUF_SIZE - 1] = '\0';;
				}

			break;
			
			case DB_DATETIME:
				/* str to time */
				col_map[f->col_pos].s[col_map[f->col_pos].len]='\0';
				if (bdb_str2time(col_map[f->col_pos].s, &fld->v.time) < 0)
				{
					ERR("Error while converting INT value from string\n");
					return -1;
				}
			break;

			case DB_INT:
				/* str to int */
				col_map[f->col_pos].s[col_map[f->col_pos].len]='\0';
				if (bdb_str2int(col_map[f->col_pos].s, &fld->v.int4) < 0)
				{
					ERR("Error while converting INT value from string\n");
					return -1;
				}
			break;

			case DB_FLOAT:
			case DB_DOUBLE:
				/* str to dowuble */
				col_map[f->col_pos].s[col_map[f->col_pos].len]='\0';
				if (bdb_str2double(col_map[f->col_pos].s, &fld->v.dbl) < 0)
				{
					ERR("Error while converting DOUBLE value from string\n");
					return -1;
				}
			break;

			case DB_BITMAP:
				/* str to int */
				col_map[f->col_pos].s[col_map[f->col_pos].len]='\0';
				if (bdb_str2int(col_map[f->col_pos].s, &fld->v.int4) < 0)
				{
					ERR("Error while converting BITMAP value from string\n");
					return -1;
				}
			break;

			case DB_NONE:
			break;
		}
	}
	return 0;

}

int bdb_cmd_first(db_res_t* res)
{
	bdb_cmd_t *bcmd;

	bcmd = DB_GET_PAYLOAD(res->cmd);
	switch (bcmd->next_flag) {
		case -2: /* table is empty */
			return 1;
		case 0:  /* cursor position is 0 */
			return 0;
		case 1:  /* next row */
		case 2:  /* EOF */
			ERR("bdb: no next row.\n");
			return -1;
		default:
			return bdb_cmd_next(res);
	}
}


int bdb_cmd_next(db_res_t* res)
{
	bdb_cmd_t *bcmd;
	DBT key, data;
	int ret;
	static char dbuf[MAX_ROW_SIZE];

	bcmd = DB_GET_PAYLOAD(res->cmd);

	if (bcmd->next_flag == 2 || bcmd->next_flag == -2) return 1;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	memset(dbuf, 0, MAX_ROW_SIZE);
	
	data.data = dbuf;
	data.ulen = MAX_ROW_SIZE;
	data.flags = DB_DBT_USERMEM;

	ret = 0;
	if(bcmd->skey.len==0)
	{
		while((ret = bcmd->dbcp->c_get(bcmd->dbcp, &key, &data, DB_NEXT))==0)
		{
			if(!strncasecmp((char*)key.data,"METADATA",8)) 
				continue;
			break;
		}
		if(ret!=0)
		{
			bcmd->next_flag =  bcmd->next_flag<0?-2:2;
			return 1;
		}
	} else {
		key.data = bcmd->skey.s;
		key.ulen = bcmd->skey_size;
		key.flags = DB_DBT_USERMEM;
		key.size = bcmd->skey.len;
		ret = bcmd->dbcp->c_get(bcmd->dbcp, &key, &data, DB_NEXT);
		if(ret!=0)
		{
			bcmd->next_flag = bcmd->next_flag<0?-2:2;
			return 1;
		}
	}

	if (bcmd->next_flag <= 0) {
		bcmd->next_flag++;
	}

	if (bdb_update_result(res->cmd, &data) < 0) {
		return -1;
	}

	res->cur_rec->fld = res->cmd->result;
	return 0;
}


/** @} */
