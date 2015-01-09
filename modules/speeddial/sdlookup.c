/*
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * 
 */


#include <string.h>

#include "../../dprint.h"
#include "../../action.h"
#include "../../config.h"
#include "../../ut.h"
#include "../../mod_fix.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../lib/srdb1/db.h"

#include "speeddial.h"
#include "sdlookup.h"

#define MAX_USERURI_SIZE	256
static char useruri_buf[MAX_USERURI_SIZE];

/**
 * Rewrite Request-URI
 */
static inline int rewrite_ruri(struct sip_msg* _m, char* _s)
{
	struct action act;
	struct run_act_ctx ra_ctx;

	memset(&act, '\0', sizeof(act));
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
int sd_lookup(struct sip_msg* _msg, char* _table, char* _owner)
{
	str user_s, table_s, uri_s;
	int nr_keys;
	struct sip_uri *puri;
	struct sip_uri turi;
	db_key_t db_keys[4];
	db_val_t db_vals[4];
	db_key_t db_cols[1];
	db1_res_t* db_res = NULL;

	if(_table==NULL || fixup_get_svalue(_msg, (gparam_p)_table, &table_s)!=0)
	{
		LM_ERR("invalid table parameter");
		return -1;
	}

	/* init */
	nr_keys = 0;
	db_cols[0]=&new_uri_column;
	
	if(_owner)
	{
		memset(&turi, 0, sizeof(struct sip_uri));
		if(fixup_get_svalue(_msg, (gparam_p)_owner, &uri_s)!=0)
		{
			LM_ERR("invalid owner uri parameter");
			return -1;
		}
		if(parse_uri(uri_s.s, uri_s.len, &turi)!=0)
		{
			LM_ERR("bad owner SIP address!\n");
			goto err_server;
		}
		LM_DBG("using user id [%.*s]\n", uri_s.len, uri_s.s);
		puri = &turi;
	} else {
		/* take username@domain from From header */
		if ( (puri = parse_from_uri(_msg ))==NULL )
		{
			LM_ERR("failed to parse FROM header\n");
			goto err_server;
		}
	}
		
	db_keys[nr_keys]=&user_column;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = puri->user.s;
	db_vals[nr_keys].val.str_val.len = puri->user.len;
	nr_keys++;

	if(use_domain>=1)
	{
		db_keys[nr_keys]=&domain_column;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val.s = puri->host.s;
		db_vals[nr_keys].val.str_val.len = puri->host.len;
		nr_keys++;
		
		if (dstrip_s.s!=NULL && dstrip_s.len>0
			&& dstrip_s.len<puri->host.len
			&& strncasecmp(puri->host.s,dstrip_s.s,dstrip_s.len)==0)
		{
			db_vals[nr_keys].val.str_val.s   += dstrip_s.len;
			db_vals[nr_keys].val.str_val.len -= dstrip_s.len;
		}
	}
	/* take sd from r-uri */
	if (parse_sip_msg_uri(_msg) < 0)
	{
		LM_ERR("failed to parsing Request-URI\n");
		goto err_server;
	}
	
	db_keys[nr_keys]=&sd_user_column;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = _msg->parsed_uri.user.s;
	db_vals[nr_keys].val.str_val.len = _msg->parsed_uri.user.len;
	nr_keys++;
	
	if(use_domain>=2)
	{
		db_keys[nr_keys]=&sd_domain_column;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val.s = _msg->parsed_uri.host.s;
		db_vals[nr_keys].val.str_val.len = _msg->parsed_uri.host.len;
		nr_keys++;
		
		if (dstrip_s.s!=NULL && dstrip_s.len>0
			&& dstrip_s.len<_msg->parsed_uri.host.len
			&& strncasecmp(_msg->parsed_uri.host.s,dstrip_s.s,dstrip_s.len)==0)
		{
			db_vals[nr_keys].val.str_val.s   += dstrip_s.len;
			db_vals[nr_keys].val.str_val.len -= dstrip_s.len;
		}
	}
	
	db_funcs.use_table(db_handle, &table_s);
	if(db_funcs.query(db_handle, db_keys, NULL, db_vals, db_cols,
		nr_keys /*no keys*/, 1 /*no cols*/, NULL, &db_res)!=0)
	{
		LM_ERR("failed to query database\n");
		goto err_server;
	}

	if (RES_ROW_N(db_res)<=0 || RES_ROWS(db_res)[0].values[0].nul != 0)
	{
		LM_DBG("no sip addres found for R-URI\n");
		if (db_res!=NULL && db_funcs.free_result(db_handle, db_res) < 0)
			LM_DBG("failed to free result of query\n");
		return -1;
	}

	user_s.s = useruri_buf+4;
	switch(RES_ROWS(db_res)[0].values[0].type)
	{ 
		case DB1_STRING:
			strcpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.string_val);
			user_s.len = strlen(user_s.s);
		break;
		case DB1_STR:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.str_val.s,
				RES_ROWS(db_res)[0].values[0].val.str_val.len);
			user_s.len = RES_ROWS(db_res)[0].values[0].val.str_val.len;
			user_s.s[user_s.len] = '\0';
		break;
		case DB1_BLOB:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.blob_val.s,
				RES_ROWS(db_res)[0].values[0].val.blob_val.len);
			user_s.len = RES_ROWS(db_res)[0].values[0].val.blob_val.len;
			user_s.s[user_s.len] = '\0';
		default:
			LM_ERR("unknown type of DB new_uri column\n");
			if (db_res != NULL && db_funcs.free_result(db_handle, db_res) < 0)
			{
				LM_DBG("failed to free result of query\n");
			}
			goto err_server;
	}
	
	/* check 'sip:' */
	if(user_s.len<4 || strncmp(user_s.s, "sip:", 4))
	{
		memcpy(useruri_buf, "sip:", 4);
		user_s.s -= 4;
		user_s.len += 4;
	}

	/**
	 * Free the result because we don't need it anymore
	 */
	if (db_res!=NULL && db_funcs.free_result(db_handle, db_res) < 0)
		LM_DBG("failed to free result of query\n");

	/* set the URI */
	LM_DBG("URI of sd from R-URI [%s]\n", user_s.s);
	if(rewrite_ruri(_msg, user_s.s)<0)
	{
		LM_ERR("failed to replace the R-URI\n");
		goto err_server;
	}

	return 1;

err_server:
	return -1;
}

