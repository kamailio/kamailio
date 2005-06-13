/* 
 * $Id$
 *
 * ALIAS_DB Module
 *
 * Copyright (C) 2004 Voice Sistem
 *
 * This file is part of a module for SER, a free SIP server.
 *
 * ALIAS_DB module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use this software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact Voice Sistem by e-mail at the following address:
 *    office@voice-sistem.ro
 *
 * ALIAS_DB module is distributed in the hope that it will be useful,
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
#include "../../parser/parse_uri.h"
#include "../../db/db.h"

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

	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = _s;
	act.next = 0;
	
	if (do_action(&act, _m) < 0)
	{
		LOG(L_ERR, "alias_db:rewrite_ruri: Error in do_action\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int alias_db_lookup(struct sip_msg* _msg, char* _table, char* _str2)
{
	str user_s;
	db_key_t db_keys[2] = { alias_user_column, alias_domain_column };
	db_val_t db_vals[2];
	db_key_t db_cols[] = { user_column, domain_column };
	db_res_t* db_res = NULL;
	
	if (parse_sip_msg_uri(_msg) < 0)
	{
		LOG(L_ERR, "alias_db_lookup: Error while parsing Request-URI\n");

		if (sl_reply(_msg, (char*)400, "Bad Request-URI") == -1)
		{
			LOG(L_ERR, "alias_db_lookup: Error while sending reply\n");
		}
		return 0;
	}
	
	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = _msg->parsed_uri.user.s;
	db_vals[0].val.str_val.len = _msg->parsed_uri.user.len;

	if (use_domain)
	{
		db_vals[1].type = DB_STR;
		db_vals[1].nul = 0;
		db_vals[1].val.str_val.s = _msg->parsed_uri.host.s;
		db_vals[1].val.str_val.len = _msg->parsed_uri.host.len;
	
		if (dstrip_s.s!=NULL && dstrip_s.len>0
			&& dstrip_s.len<_msg->parsed_uri.host.len
			&& strncasecmp(_msg->parsed_uri.host.s,dstrip_s.s,dstrip_s.len)==0)
		{
			db_vals[1].val.str_val.s   += dstrip_s.len;
			db_vals[1].val.str_val.len -= dstrip_s.len;
		}
	}
	
	adbf.use_table(db_handle, _table);
	if(adbf.query(db_handle, db_keys, NULL, db_vals, db_cols,
		(use_domain)?2:1 /*no keys*/, 2 /*no cols*/, NULL, &db_res)!=0)
	{
		LOG(L_ERR, "alias_db_lookup: error querying database\n");
		goto err_server;
	}

	if (RES_ROW_N(db_res)<=0 || RES_ROWS(db_res)[0].values[0].nul != 0)
	{
		DBG("alias_db_lookup: no alias found for R-URI\n");
		if (db_res!=NULL && adbf.free_result(db_handle, db_res) < 0)
			DBG("alias_db_lookup: Error while freeing result of query\n");
		return -1;
	}

	memcpy(useruri_buf, "sip:", 4);
	user_s.len = 4;
	user_s.s = useruri_buf+4;
	switch(RES_ROWS(db_res)[0].values[0].type)
	{ 
		case DB_STRING:
			strcpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.string_val);
			user_s.len += strlen(user_s.s);
		break;
		case DB_STR:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.str_val.s,
				RES_ROWS(db_res)[0].values[0].val.str_val.len);
			user_s.len += RES_ROWS(db_res)[0].values[0].val.str_val.len;
		break;
		case DB_BLOB:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.blob_val.s,
				RES_ROWS(db_res)[0].values[0].val.blob_val.len);
			user_s.len += RES_ROWS(db_res)[0].values[0].val.blob_val.len;
		default:
			LOG(L_ERR, "alias_db_lookup: Unknown type of DB user column\n");
			if (db_res != NULL && adbf.free_result(db_handle, db_res) < 0)
			{
				DBG("alias_db_lookup: Error while freeing result of query\n");
			}
			goto err_server;
	}
	
	/* add the @*/
	useruri_buf[user_s.len] = '@';
	user_s.len++;
	
	/* add the domain */
	user_s.s = useruri_buf+user_s.len;
	switch(RES_ROWS(db_res)[0].values[1].type)
	{ 
		case DB_STRING:
			strcpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[1].val.string_val);
			user_s.len += strlen(user_s.s);
		break;
		case DB_STR:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[1].val.str_val.s,
				RES_ROWS(db_res)[0].values[1].val.str_val.len);
			user_s.len += RES_ROWS(db_res)[0].values[1].val.str_val.len;
			useruri_buf[user_s.len] = '\0';
		break;
		case DB_BLOB:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[1].val.blob_val.s,
				RES_ROWS(db_res)[0].values[1].val.blob_val.len);
			user_s.len += RES_ROWS(db_res)[0].values[1].val.blob_val.len;
			useruri_buf[user_s.len] = '\0';
		default:
			LOG(L_ERR, "alias_db_lookup: Unknown type of DB user column\n");
			if (db_res != NULL && adbf.free_result(db_handle, db_res) < 0)
			{
				DBG("alias_db_lookup: Error while freeing result of query\n");
			}
			goto err_server;
	}

	/**
	 * Free the result because we don't need it
	 * anymore
	 */
	if (db_res!=NULL && adbf.free_result(db_handle, db_res) < 0)
		DBG("alias_db_lookup: Error while freeing result of query\n");

	/* set the URI */
	DBG("alias_db_lookup: URI of alias from R-URI [%s]\n", useruri_buf);
	if(rewrite_ruri(_msg, useruri_buf)<0)
	{
		LOG(L_ERR, "alias_db_lookup: Cannot replace the R-URI\n");
		goto err_server;
	}

	return 1;

err_server:
	if (sl_reply(_msg, (char*)500, "Server Internal Error") == -1)
	{
		LOG(L_ERR, "alias_db_lookup: Error while sending reply\n");
	}
	return 0;
}

