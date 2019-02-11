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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 * 2004-09-01: first version (ramona)
 */

#include <string.h>

#include "../../core/dprint.h"
#include "../../core/action.h"
#include "../../core/config.h"
#include "../../core/ut.h"
#include "../../core/parser/parse_uri.h"
#include "../../lib/srdb1/db.h"
#include "../../core/mod_fix.h"
#include "../../core/dset.h"
#include "../../core/lvalue.h"

#include "alias_db.h"
#include "alookup.h"

#define MAX_USERURI_SIZE	256

extern db_func_t adbf;  /* DB functions */

extern int alias_db_use_domain;

char useruri_buf[MAX_USERURI_SIZE];

typedef int (*set_alias_f)(struct sip_msg* _msg, str *alias, int no, void *p);

/**
 *
 */
static int alias_db_query(struct sip_msg* _msg, str table,
			struct sip_uri *puri, unsigned long flags,
			set_alias_f set_alias, void *param)
{
	str user_s;
	db_key_t db_keys[2];
	db_val_t db_vals[2];
	db_key_t db_cols[2];
	db1_res_t* db_res = NULL;
	int i;

	if (flags&ALIAS_REVERSE_FLAG)
	{
		/* revert lookup: user->alias */
		db_keys[0] = &user_column;
		db_keys[1] = &domain_column;
		db_cols[0] = &alias_user_column;
		db_cols[1] = &alias_domain_column;
	} else {
		/* normal lookup: alias->user */
		db_keys[0] = &alias_user_column;
		db_keys[1] = &alias_domain_column;
		db_cols[0] = &user_column;
		db_cols[1] = &domain_column;
	}

	db_vals[0].type = DB1_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = puri->user.s;
	db_vals[0].val.str_val.len = puri->user.len;

	if ( flags&ALIAS_DOMAIN_FLAG ) {
		db_vals[1].type = DB1_STR;
		db_vals[1].nul = 0;
		db_vals[1].val.str_val.s = puri->host.s;
		db_vals[1].val.str_val.len = puri->host.len;

		if (domain_prefix.s && domain_prefix.len>0
				&& domain_prefix.len<puri->host.len
				&& strncasecmp(puri->host.s,domain_prefix.s,
				domain_prefix.len)==0)
		{
			db_vals[1].val.str_val.s   += domain_prefix.len;
			db_vals[1].val.str_val.len -= domain_prefix.len;
		}
	}

	adbf.use_table(db_handle, &table);
	if(adbf.query( db_handle, db_keys, NULL, db_vals, db_cols,
			(flags&ALIAS_DOMAIN_FLAG)?2:1 /*no keys*/, 2 /*no cols*/,
			NULL, &db_res)!=0 || db_res==NULL)
	{
		LM_ERR("failed to query database\n");
		goto err_server;
	}

	if (RES_ROW_N(db_res)<=0 || RES_ROWS(db_res)[0].values[0].nul != 0)
	{
		LM_DBG("no alias found for R-URI\n");
		goto err_server;
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
				goto err_server;
		}
		user_s.s = useruri_buf;
		/* set the URI */
		LM_DBG("new URI [%d] is [%.*s]\n", i, user_s.len ,user_s.s );
		if (set_alias(_msg, &user_s, i, param)!=0) {
			LM_ERR("error while setting alias\n");
			goto err_server;
		}
	}

	/**
	 * Free the DB result
	 */
	if (adbf.free_result(db_handle, db_res) < 0) {
		LM_DBG("failed to freeing result of query\n");
	}

	return 1;

err_server:
	if (db_res!=NULL) {
		if(adbf.free_result(db_handle, db_res) < 0) {
			LM_DBG("failed to freeing result of query\n");
		}
	}
	return -1;
}

int set_alias_to_ruri(struct sip_msg* _msg, str *alias, int no, void *p)
{
	/* set the RURI */
	if(no==0)
	{
		if(rewrite_uri(_msg, alias)<0)
		{
			LM_ERR("cannot replace the R-URI\n");
			return -1;
		}
	} else if (ald_append_branches) {
		if (append_branch(_msg, alias, 0, 0, MIN_Q, 0, 0, 0, 0, 0, 0) == -1)
		{
			LM_ERR("error while appending branches\n");
			return -1;
		}
	}
	return 0;
}


int alias_db_lookup_ex(struct sip_msg* _msg, str table, unsigned long flags)
{
	if (parse_sip_msg_uri(_msg) < 0)
		return -1;

	return alias_db_query(_msg, table, &_msg->parsed_uri, flags,
			set_alias_to_ruri, NULL);
}

int alias_db_lookup(struct sip_msg* _msg, str table)
{
	unsigned long flags = 0;
	if(alias_db_use_domain) flags = ALIAS_DOMAIN_FLAG;
	return alias_db_lookup_ex(_msg, table, flags);
}

int set_alias_to_pvar(struct sip_msg* _msg, str *alias, int no, void *p)
{
	pv_value_t val;
	pv_spec_t *pvs=(pv_spec_t*)p;

	if(no && !ald_append_branches)
		return 0;

	/* set the PVAR */
	val.flags = PV_VAL_STR;
	val.ri = 0;
	val.rs = *alias;

	if(pv_set_spec_value(_msg, pvs, (int)(no?EQ_T:ASSIGN_T), &val)<0)
	{
		LM_ERR("setting PV AVP failed\n");
		return -1;
	}
	return 0;
}


int alias_db_find(struct sip_msg* _msg, str table, char* _in, char* _out,
	char* flags)
{
	pv_value_t val;
	struct sip_uri puri;

	/* get the input value */
	if (pv_get_spec_value(_msg, (pv_spec_t*)_in, &val)!=0)
	{
		LM_ERR("failed to get PV value\n");
		return -1;
	}
	if ( (val.flags&PV_VAL_STR)==0 )
	{
		LM_ERR("PV vals is not string\n");
		return -1;
	}
	if (parse_uri(val.rs.s, val.rs.len, &puri)<0)
	{
		LM_ERR("failed to parse uri %.*s\n",val.rs.len,val.rs.s);
		return -1;
	}

	return alias_db_query(_msg, table, &puri, (unsigned long)flags,
			set_alias_to_pvar, _out);
}

