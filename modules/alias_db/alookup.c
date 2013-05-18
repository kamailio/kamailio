/* 
 * $Id$
 *
 * ALIAS_DB Module
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of a module for Kamailio, a free SIP server.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2004-09-01: first version (ramona)
 */

#include <string.h>

#include "../../dprint.h"
#include "../../action.h"
#include "../../config.h"
#include "../../ut.h"
#include "../../parser/parse_uri.h"
#include "../../lib/srdb1/db.h"
#include "../../mod_fix.h"
#include "../../dset.h"

#include "alias_db.h"
#include "alookup.h"

#define MAX_USERURI_SIZE	256

extern db_func_t adbf;  /* DB functions */

char useruri_buf[MAX_USERURI_SIZE];

/**
 * Rewrite Request-URI
 */
static inline int rewrite_ruri(struct sip_msg* _m, char* _s)
{
	struct action act;
	struct run_act_ctx ra_ctx;

	memset(&act, 0, sizeof(act));
	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = _s;
	init_run_actions_ctx(&ra_ctx);
	if (do_action(&ra_ctx, &act, _m) < 0)
	{
		LM_ERR("do_action failed\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int alias_db_lookup(struct sip_msg* _msg, str table_s)
{
	str user_s;
	db_key_t db_keys[2] = {&alias_user_column, &alias_domain_column};
	db_val_t db_vals[2];
	db_key_t db_cols[] = {&user_column, &domain_column};
	db1_res_t* db_res = NULL;
	int i;

	if (parse_sip_msg_uri(_msg) < 0)
		return -1;
	
	db_vals[0].type = DB1_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = _msg->parsed_uri.user.s;
	db_vals[0].val.str_val.len = _msg->parsed_uri.user.len;

	if (use_domain)
	{
		db_vals[1].type = DB1_STR;
		db_vals[1].nul = 0;
		db_vals[1].val.str_val.s = _msg->parsed_uri.host.s;
		db_vals[1].val.str_val.len = _msg->parsed_uri.host.len;
	
		if (domain_prefix.s && domain_prefix.len>0
			&& domain_prefix.len<_msg->parsed_uri.host.len
			&& strncasecmp(_msg->parsed_uri.host.s,domain_prefix.s,
				domain_prefix.len)==0)
		{
			db_vals[1].val.str_val.s   += domain_prefix.len;
			db_vals[1].val.str_val.len -= domain_prefix.len;
		}
	}
	
	adbf.use_table(db_handle, &table_s);
	if(adbf.query(db_handle, db_keys, NULL, db_vals, db_cols,
		(use_domain)?2:1 /*no keys*/, 2 /*no cols*/, NULL, &db_res)!=0)
	{
		LM_ERR("failed to query database\n");
		goto err_server;
	}

	if (RES_ROW_N(db_res)<=0 || RES_ROWS(db_res)[0].values[0].nul != 0)
	{
		LM_DBG("no alias found for R-URI\n");
		if (db_res!=NULL && adbf.free_result(db_handle, db_res) < 0)
			LM_DBG("failed to freeing result of query\n");
		return -1;
	}

	memcpy(useruri_buf, "sip:", 4);
	for(i=0; i<RES_ROW_N(db_res); i++)
	{
		user_s.len = 4;
		user_s.s = useruri_buf+4;
		switch(RES_ROWS(db_res)[i].values[0].type)
		{ 
			case DB1_STRING:
				strcpy(user_s.s, 
					(char*)RES_ROWS(db_res)[i].values[0].val.string_val);
				user_s.len += strlen(user_s.s);
			break;
			case DB1_STR:
				strncpy(user_s.s, 
					(char*)RES_ROWS(db_res)[i].values[0].val.str_val.s,
					RES_ROWS(db_res)[i].values[0].val.str_val.len);
				user_s.len += RES_ROWS(db_res)[i].values[0].val.str_val.len;
			break;
			case DB1_BLOB:
				strncpy(user_s.s, 
					(char*)RES_ROWS(db_res)[i].values[0].val.blob_val.s,
					RES_ROWS(db_res)[i].values[0].val.blob_val.len);
				user_s.len += RES_ROWS(db_res)[i].values[0].val.blob_val.len;
			break;
			default:
				LM_ERR("unknown type of DB user column\n");
				if (db_res != NULL && adbf.free_result(db_handle, db_res) < 0)
				{
					LM_DBG("failed to freeing result of query\n");
				}
				goto err_server;
		}
	
		/* add the @*/
		useruri_buf[user_s.len] = '@';
		user_s.len++;
	
		/* add the domain */
		user_s.s = useruri_buf+user_s.len;
		switch(RES_ROWS(db_res)[i].values[1].type)
		{ 
			case DB1_STRING:
				strcpy(user_s.s, 
					(char*)RES_ROWS(db_res)[i].values[1].val.string_val);
				user_s.len += strlen(user_s.s);
			break;
			case DB1_STR:
				strncpy(user_s.s, 
					(char*)RES_ROWS(db_res)[i].values[1].val.str_val.s,
					RES_ROWS(db_res)[i].values[1].val.str_val.len);
				user_s.len += RES_ROWS(db_res)[i].values[1].val.str_val.len;
				useruri_buf[user_s.len] = '\0';
			break;
			case DB1_BLOB:
				strncpy(user_s.s, 
					(char*)RES_ROWS(db_res)[i].values[1].val.blob_val.s,
					RES_ROWS(db_res)[i].values[1].val.blob_val.len);
				user_s.len += RES_ROWS(db_res)[i].values[1].val.blob_val.len;
				useruri_buf[user_s.len] = '\0';
			break;
			default:
				LM_ERR("unknown type of DB user column\n");
				if (db_res != NULL && adbf.free_result(db_handle, db_res) < 0)
				{
					LM_DBG("failed to freeing result of query\n");
				}
				goto err_server;
		}
		/* set the URI */
		LM_DBG("new URI [%d] is [%s]\n", i, useruri_buf);
		if(i==0)
		{
			if(rewrite_ruri(_msg, useruri_buf)<0)
			{
				LM_ERR("cannot replace the R-URI\n");
				goto err_server;
			}
			if(ald_append_branches==0)
				break;
		} else {
			user_s.s = useruri_buf;
			if (append_branch(_msg, &user_s, 0, 0, MIN_Q, 0, 0,
					  0, 0, 0, 0) == -1)
			{
				LM_ERR("error while appending branches\n");
				goto err_server;
			}
		}
	}

	/**
	 * Free the DB result
	 */
	if (db_res!=NULL && adbf.free_result(db_handle, db_res) < 0)
		LM_DBG("failed to freeing result of query\n");

	return 1;

err_server:
	if (db_res!=NULL && adbf.free_result(db_handle, db_res) < 0)
		LM_DBG("failed to freeing result of query\n");
	return -1;
}
