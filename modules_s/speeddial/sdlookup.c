/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * SPEEDDIAL SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * SPEEDDIAL SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *  
 *
 * History:
 * ---------
 * 
 */


#include <string.h>

#include "../../dprint.h"
#include "../../action.h"
#include "../../config.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../db/db.h"
#include "../../id.h"
#include "../../dset.h"

#include "speeddial.h"
#include "sdlookup.h"

#define MAX_USERURI_SIZE	256

static char useruri_buf[MAX_USERURI_SIZE];


/**
 *
 */
int sd_lookup(struct sip_msg* _msg, char* _table, char* _str2)
{
	str user_s, uid, did;
	int nr_keys;
	db_key_t db_keys[4];
	db_val_t db_vals[4];
	db_key_t db_cols[1];
	db_res_t* db_res = NULL;

	/* init */
	nr_keys = 0;
	db_cols[0]=new_uri_column;

	     /* Retrieve the owner of the record */
	if (get_from_uid(&uid, _msg) < 0) {
		LOG(L_ERR, "sd_lookup: Unable to get user identity\n");
		return -1;
	}

	     /* Retrieve the called domain id */
	if (get_to_did(&did, _msg) < 0) {
		LOG(L_ERR, "sd_lookup: Destination domain ID not known\n");
		return -1;
	}

	db_keys[nr_keys]=uid_column;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val = uid;
	nr_keys++;

	db_keys[nr_keys]=dial_did_column;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val = did;
	nr_keys++;
	
	/* Get the called username */
	if (parse_sip_msg_uri(_msg) < 0)
	{
		LOG(L_ERR, "sd_lookup: Error while parsing Request-URI\n");
		goto err_badreq;
	}
	
	db_keys[nr_keys]=dial_username_column;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val = _msg->parsed_uri.user;
	nr_keys++;

	DBG("speeddial:sd_lookup: Looking up (uid:%.*s,dial_username:%.*s,dial_did:%.*s)\n", uid.len, uid.s,
	    _msg->parsed_uri.user.len, _msg->parsed_uri.user.s,
	    did.len, did.s);
	
	db_funcs.use_table(db_handle, _table);
	if(db_funcs.query(db_handle, db_keys, NULL, db_vals, db_cols,
		nr_keys, 1, NULL, &db_res) < 0)
	{
		LOG(L_ERR, "sd_lookup: Error querying database\n");
		goto err_server;
	}

	if (RES_ROW_N(db_res)<=0 || RES_ROWS(db_res)[0].values[0].nul != 0)
	{
		DBG("sd_lookup: no sip addres found for R-URI\n");
		if (db_res!=NULL && db_funcs.free_result(db_handle, db_res) < 0)
			DBG("sd_lookup: Error while freeing result of query\n");
		return -1;
	}

	user_s.s = useruri_buf+4;
	switch(RES_ROWS(db_res)[0].values[0].type)
	{ 
		case DB_STRING:
			strcpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.string_val);
			user_s.len = strlen(user_s.s);
		break;
		case DB_STR:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.str_val.s,
				RES_ROWS(db_res)[0].values[0].val.str_val.len);
			user_s.len = RES_ROWS(db_res)[0].values[0].val.str_val.len;
			user_s.s[user_s.len] = '\0';
		break;
		case DB_BLOB:
			strncpy(user_s.s, 
				(char*)RES_ROWS(db_res)[0].values[0].val.blob_val.s,
				RES_ROWS(db_res)[0].values[0].val.blob_val.len);
			user_s.len = RES_ROWS(db_res)[0].values[0].val.blob_val.len;
			user_s.s[user_s.len] = '\0';
		default:
			LOG(L_ERR, "sd_lookup: Unknown type of DB new_uri column\n");
			if (db_res != NULL && db_funcs.free_result(db_handle, db_res) < 0)
			{
				DBG("sd_lookup: Error while freeing result of query\n");
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
		DBG("sd_lookup: Error while freeing result of query\n");

	/* set the URI */
	DBG("sd_lookup: URI of sd from R-URI [%s]\n", user_s.s);
	if(rewrite_uri(_msg, &user_s)<0)
	{
		LOG(L_ERR, "sd_lookup: Cannot replace the R-URI\n");
		goto err_server;
	}

	return 1;

err_server:
	if (sl.reply(_msg, 500, "Server Internal Error") == -1)
	{
		LOG(L_ERR, "sd_lookup: Error while sending reply\n");
	}
	return 0;
err_badreq:
	if (sl.reply(_msg, 400, "Bad Request") == -1)
	{
		LOG(L_ERR, "sd_lookup: Error while sending reply\n");
	}
	return 0;
}
