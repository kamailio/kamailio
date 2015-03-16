/*
 * pua module - presence user agent module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../parser/parse_expires.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../pt.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/kmi/mi.h"
#include "../../modules/tm/tm_load.h"
#include "pua.h"
#include "send_publish.h"
#include "send_subscribe.h"
#include "pua_bind.h"
#include "pua_callback.h"
#include "event_list.h"
#include "add_events.h"
#include "pidf.h"

MODULE_VERSION
#define PUA_TABLE_VERSION 7

struct tm_binds tmb;
htable_t* HashT= NULL;
int HASH_SIZE= -1;
extern int bind_pua(pua_api_t* api);
int min_expires= 0;
int default_expires=3600;
static str db_url = str_init(DEFAULT_DB_URL);
str db_table= str_init("pua");
int update_period= 100;
str outbound_proxy = {0, 0};
int check_remote_contact = 1;
int startup_time = 0;
int dlginfo_increase_version = 0;
int reginfo_increase_version = 0;
pua_event_t* pua_evlist= NULL;
int dbmode = 0;

int db_table_lock_write = 1;
db_locking_t db_table_lock = DB_LOCKING_WRITE;

int pua_fetch_rows = 500;

/* database connection */
db1_con_t *pua_db = NULL;
db_func_t pua_dbf;

/* database colums */
static str str_pres_uri_col = str_init("pres_uri");
static str str_pres_id_col = str_init("pres_id");
static str str_expires_col= str_init("expires");
static str str_flag_col= str_init("flag");
static str str_etag_col= str_init("etag");
static str str_tuple_id_col= str_init("tuple_id");
static str str_watcher_uri_col= str_init("watcher_uri");
static str str_call_id_col= str_init("call_id");
static str str_to_tag_col= str_init("to_tag");
static str str_from_tag_col= str_init("from_tag");
static str str_cseq_col= str_init("cseq");
static str str_event_col= str_init("event");
static str str_record_route_col= str_init("record_route");
static str str_contact_col= str_init("contact");
static str str_remote_contact_col= str_init("remote_contact");
static str str_extra_headers_col= str_init("extra_headers");
static str str_desired_expires_col= str_init("desired_expires");
static str str_version_col = str_init("version");

/* module functions */

static int mod_init(void);
static int child_init(int);
static int mi_child_init(void);
static void destroy(void);
static struct mi_root* mi_cleanup(struct mi_root* cmd, void* param);

static ua_pres_t* build_uppubl_cbparam(ua_pres_t* p);

static int db_restore(void);
static void db_update(unsigned int ticks,void *param);
static void hashT_clean(unsigned int ticks,void *param);

static cmd_export_t cmds[]=
{
	{"bind_libxml_api",          (cmd_function)bind_libxml_api, 1, 0, 0, 0},
	{"bind_pua",                 (cmd_function)bind_pua, 1, 0, 0, 0},
	{"pua_update_contact",       (cmd_function)update_contact, 0, 0, 0, REQUEST_ROUTE},
	{0,                          0,	0, 0, 0, 0} 
};

static param_export_t params[]={
	{"hash_size",                INT_PARAM, &HASH_SIZE},
	{"db_url",                   PARAM_STR, &db_url},
	{"db_table",                 PARAM_STR, &db_table},
	{"min_expires",	             INT_PARAM, &min_expires},
	{"default_expires",          INT_PARAM, &default_expires},
	{"update_period",            INT_PARAM, &update_period},
	{"outbound_proxy",           PARAM_STR, &outbound_proxy},
	{"dlginfo_increase_version", INT_PARAM, &dlginfo_increase_version},
	{"reginfo_increase_version", INT_PARAM, &reginfo_increase_version},
	{"check_remote_contact",     INT_PARAM, &check_remote_contact},
	{"db_mode",                  INT_PARAM, &dbmode},
	{"fetch_rows",               INT_PARAM, &pua_fetch_rows},
	{"db_table_lock_write",     INT_PARAM, &db_table_lock_write},
	{0,                          0,         0}
};

static mi_export_t mi_cmds[] = {
	{ "pua_cleanup",	mi_cleanup,		0,  0,  mi_child_init},
	{ 0,			0,			0,  0,  0}
};

/** module exports */
struct module_exports exports= {
	"pua",				/* module name */
	DEFAULT_DLFLAGS,		/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,				/* exported statistics */
	mi_cmds,			/* exported MI functions */
	0,				/* exported pseudo-variables */
	0,				/* extra processes */
	mod_init,			/* module initialization function */
	0,				/* response handling function */
	destroy,			/* destroy function */
	child_init			/* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	LM_DBG("...\n");

	if (register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if(min_expires< 0)
		min_expires= 0;

	if(default_expires< 600)
		default_expires= 3600;

	/* load TM API */
	if(load_tm_api(&tmb)==-1)
	{
		LM_ERR("can't load tm functions\n");
		return -1;
	}

	LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len, db_url.s);

	/* binding to database module  */
	if (db_bind_mod(&db_url, &pua_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}
	if (!DB_CAPABILITY(pua_dbf, DB_CAP_ALL)) {
		LM_ERR("Database module does not implement all functions needed"
				" by the module\n");
		return -1;
	}

	pua_db = pua_dbf.init(&db_url);
	if (!pua_db)
	{
		LM_ERR("while connecting database\n");
		return -1;
	}
	/* verify table version  */
	if(db_check_table_version(&pua_dbf, pua_db, &db_table, PUA_TABLE_VERSION) < 0) {
		LM_ERR("error during table version check.\n");
		return -1;
	}

	if (dbmode != PUA_DB_ONLY)
	{ 
		if(HASH_SIZE<=1)
			HASH_SIZE= 512;
		else
			HASH_SIZE = 1<<HASH_SIZE;

		HashT= new_htable();
		if(HashT== NULL)
		{
			LM_ERR("while creating new hash table\n");
			return -1;
		}
		if(db_restore()< 0)
		{
			LM_ERR("while restoring hash_table\n");
			return -1;
		}
	} 

	if (dbmode != PUA_DB_DEFAULT && dbmode != PUA_DB_ONLY)
	{
		dbmode = PUA_DB_DEFAULT;
		LM_ERR( "Invalid dbmode-using default mode\n" );
	}

	if(update_period<0)
	{
		LM_ERR("wrong clean_period\n");
		return -1;
	}
	if ( init_puacb_list() < 0)
	{
		LM_ERR("callbacks initialization failed\n");
		return -1;
	}
	pua_evlist= init_pua_evlist();
	if(pua_evlist==0)
	{
		LM_ERR("when initializing pua_evlist\n");
		return -1;
	}
	if(pua_add_events()< 0)
	{
		LM_ERR("while adding events\n");
		return -1;
	}

	if(check_remote_contact<0 || check_remote_contact>1)
	{
		LM_ERR("bad value for check_remote_contact\n");
		return -1;
	}

	startup_time = (int) time(NULL);

	if (update_period > 5)
		register_timer(hashT_clean, 0, update_period- 5);
	else if (update_period != 0)
	{
		LM_ERR("update_period must be 0 or > 5\n");
		return -1;
	}

	if (dbmode != PUA_DB_ONLY) 
	{        
		if (update_period > 0) 
			register_timer(db_update, 0, update_period);
	}

	if (db_table_lock_write != 1)
		db_table_lock = DB_LOCKING_NONE;
	
	if(pua_db)
		pua_dbf.close(pua_db);
	pua_db = NULL;
	
	return 0;
}

static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if (pua_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	/* In DB only mode do not pool the connections where possible. */
	if (dbmode == PUA_DB_ONLY && pua_dbf.init2)
		pua_db = pua_dbf.init2(&db_url, DB_POOLING_NONE);
	else
		pua_db = pua_dbf.init(&db_url);
	if (!pua_db)
	{
		LM_ERR("Child %d: connecting to database failed\n", rank);
		return -1;
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("child %d: Error in use_table pua\n", rank);
		return -1;
	}

	LM_DBG("child %d: Database connection opened successfully\n", rank);

	return 0;
}

static int mi_child_init(void)
{
	if (pua_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	/* In DB only mode do not pool the connections where possible. */
	if (dbmode == PUA_DB_ONLY && pua_dbf.init2)
		pua_db = pua_dbf.init2(&db_url, DB_POOLING_NONE);
	else
		pua_db = pua_dbf.init(&db_url);
	if (!pua_db)
	{
		LM_ERR("connecting to database failed\n");
		return -1;
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("Error in use_table pua\n");
		return -1;
	}

	LM_DBG("Database connection opened successfully\n");

	return 0;
}

static void destroy(void)
{
	if (puacb_list)
		destroy_puacb_list();

	/* if dbmode is PUA_DB_ONLY, then HashT will be NULL 
	   so db_update and destroy_htable won't get called */ 
	if(pua_db && HashT)
		db_update(0,0);

	if(HashT)
		destroy_htable();

	if(pua_db)
		pua_dbf.close(pua_db);
	if(pua_evlist)
		destroy_pua_evlist();

	return ;
}

static int db_restore(void)
{
	ua_pres_t* p= NULL;
	db_key_t result_cols[19]; 
	db1_res_t *res= NULL;
	db_row_t *row = NULL;	
	db_val_t *row_vals= NULL;
	str pres_uri, pres_id;
	str etag, tuple_id;
	str watcher_uri, call_id;
	str to_tag, from_tag, remote_contact;
	str record_route, contact, extra_headers;
	int size= 0, i;
	int n_result_cols= 0;
	int puri_col,pid_col,expires_col,flag_col,etag_col, desired_expires_col;
	int watcher_col,callid_col,totag_col,fromtag_col,cseq_col,remote_contact_col;
	int event_col,contact_col,tuple_col,record_route_col, extra_headers_col;
	int version_col;
	unsigned int hash_code;

	if (dbmode==PUA_DB_ONLY)
	{
		LM_ERR( "db_restore shouldn't be called in PUA_DB_ONLY mode\n" );
		return(-1);
	}

	result_cols[puri_col=n_result_cols++]	= &str_pres_uri_col;
	result_cols[pid_col=n_result_cols++]	= &str_pres_id_col;
	result_cols[expires_col=n_result_cols++]= &str_expires_col;
	result_cols[flag_col=n_result_cols++]	= &str_flag_col;
	result_cols[etag_col=n_result_cols++]	= &str_etag_col;
	result_cols[tuple_col=n_result_cols++]	= &str_tuple_id_col;
	result_cols[watcher_col=n_result_cols++]= &str_watcher_uri_col;
	result_cols[callid_col=n_result_cols++] = &str_call_id_col;
	result_cols[totag_col=n_result_cols++]	= &str_to_tag_col;
	result_cols[fromtag_col=n_result_cols++]= &str_from_tag_col;
	result_cols[cseq_col= n_result_cols++]	= &str_cseq_col;
	result_cols[event_col= n_result_cols++]	= &str_event_col;
	result_cols[record_route_col= n_result_cols++]	= &str_record_route_col;
	result_cols[contact_col= n_result_cols++]	= &str_contact_col;
	result_cols[remote_contact_col= n_result_cols++]	= &str_remote_contact_col;
	result_cols[extra_headers_col= n_result_cols++]	= &str_extra_headers_col;
	result_cols[desired_expires_col= n_result_cols++]	= &str_desired_expires_col;
	result_cols[version_col= n_result_cols++]	= &str_version_col;

	if(!pua_db)
	{
		LM_ERR("null database connection\n");
		return -1;
	}

	if(pua_dbf.use_table(pua_db, &db_table)< 0)
	{
		LM_ERR("in use table\n");
		return -1;
	}

	if(db_fetch_query(&pua_dbf, pua_fetch_rows, pua_db, 0, 0, 0, result_cols,
				0, n_result_cols, 0, &res)< 0)
	{
		LM_ERR("while querrying table\n");
		if(res)
		{
			pua_dbf.free_result(pua_db, res);
			res = NULL;
		}
		return -1;
	}
	if(res==NULL)
		return -1;

	if(res->n<=0)
	{
		LM_INFO("the query returned no result\n");
		pua_dbf.free_result(pua_db, res);
		res = NULL;
		return 0;
	}

	do {
		LM_DBG("found %d db entries\n", res->n);

		for(i =0 ; i< res->n ; i++)
		{
			row = &res->rows[i];
			row_vals = ROW_VALUES(row);

			pres_uri.s= (char*)row_vals[puri_col].val.string_val;
			pres_uri.len = strlen(pres_uri.s);

			LM_DBG("pres_uri= %.*s\n", pres_uri.len, pres_uri.s);

			memset(&etag,			 0, sizeof(str));
			memset(&tuple_id,		 0, sizeof(str));
			memset(&watcher_uri,	 0, sizeof(str));
			memset(&call_id,		 0, sizeof(str));
			memset(&to_tag,			 0, sizeof(str));
			memset(&from_tag,		 0, sizeof(str));
			memset(&record_route,	 0, sizeof(str));
			memset(&pres_id,         0, sizeof(str));
			memset(&contact,         0, sizeof(str));
			memset(&remote_contact,         0, sizeof(str));
			memset(&extra_headers,   0, sizeof(str));

			pres_id.s= (char*)row_vals[pid_col].val.string_val;
			if(pres_id.s)
				pres_id.len = strlen(pres_id.s);

			if(row_vals[etag_col].val.string_val)
			{
				etag.s= (char*)row_vals[etag_col].val.string_val;
				etag.len = strlen(etag.s);

				tuple_id.s= (char*)row_vals[tuple_col].val.string_val;
				tuple_id.len = strlen(tuple_id.s);
			}

			if(row_vals[watcher_col].val.string_val)
			{	
				watcher_uri.s= (char*)row_vals[watcher_col].val.string_val;
				watcher_uri.len = strlen(watcher_uri.s);

				call_id.s= (char*)row_vals[callid_col].val.string_val;
				call_id.len = strlen(call_id.s);

				to_tag.s= (char*)row_vals[totag_col].val.string_val;
				to_tag.len = strlen(to_tag.s);

				from_tag.s= (char*)row_vals[fromtag_col].val.string_val;
				from_tag.len = strlen(from_tag.s);

				if(row_vals[record_route_col].val.string_val)
				{
					record_route.s= (char*)row_vals[record_route_col].val.string_val;
					record_route.len= strlen(record_route.s);
				}	

				contact.s= (char*)row_vals[contact_col].val.string_val;
				contact.len = strlen(contact.s);

				remote_contact.s= (char*)row_vals[remote_contact_col].val.string_val;
				remote_contact.len = strlen(remote_contact.s);
			}
			extra_headers.s= (char*)row_vals[extra_headers_col].val.string_val;
			if(extra_headers.s)
				extra_headers.len= strlen(extra_headers.s);
			else
				extra_headers.len= 0;

			size= sizeof(ua_pres_t)+ sizeof(str)+ (pres_uri.len+ pres_id.len+
					tuple_id.len)* sizeof(char);
			if(extra_headers.s)
				size+= sizeof(str)+ extra_headers.len* sizeof(char);

			if(watcher_uri.s)
				size+= sizeof(str)+ (watcher_uri.len+ call_id.len+ to_tag.len+
						from_tag.len+ record_route.len+ contact.len)* sizeof(char);

			p= (ua_pres_t*)shm_malloc(size);
			if(p== NULL)
			{
				LM_ERR("no more share memmory");
				goto error;
			}
			memset(p, 0, size);
			size= sizeof(ua_pres_t);

			p->pres_uri= (str*)((char*)p+ size);
			size+= sizeof(str);
			p->pres_uri->s= (char*)p + size;
			memcpy(p->pres_uri->s, pres_uri.s, pres_uri.len);
			p->pres_uri->len= pres_uri.len;
			size+= pres_uri.len;

			if(pres_id.s)
			{	
				p->id.s= (char*)p + size;
				memcpy(p->id.s, pres_id.s, pres_id.len);
				p->id.len= pres_id.len;
				size+= pres_id.len;
			}
			if(tuple_id.s && tuple_id.len)
			{
				p->tuple_id.s= (char*)p + size;
				memcpy(p->tuple_id.s, tuple_id.s, tuple_id.len);
				p->tuple_id.len= tuple_id.len;
				size+= tuple_id.len;
			}	

			if(watcher_uri.s && watcher_uri.len)
			{	
				p->watcher_uri= (str*)((char*)p+ size);
				size+= sizeof(str);

				p->watcher_uri->s= (char*)p+ size;
				memcpy(p->watcher_uri->s, watcher_uri.s, watcher_uri.len);
				p->watcher_uri->len= watcher_uri.len;
				size+= watcher_uri.len;

				p->to_tag.s= (char*)p+ size;
				memcpy(p->to_tag.s, to_tag.s, to_tag.len);
				p->to_tag.len= to_tag.len;
				size+= to_tag.len;

				p->from_tag.s= (char*)p+ size;
				memcpy(p->from_tag.s, from_tag.s, from_tag.len);
				p->from_tag.len= from_tag.len;
				size+= from_tag.len;

				p->call_id.s= (char*)p + size;
				memcpy(p->call_id.s, call_id.s, call_id.len);
				p->call_id.len= call_id.len;
				size+= call_id.len;

				if(record_route.s && record_route.len)
				{
					p->record_route.s= (char*)p + size;
					memcpy(p->record_route.s, record_route.s, record_route.len);
					p->record_route.len= record_route.len;
					size+= record_route.len;
				}
				p->contact.s= (char*)p + size;
				memcpy(p->contact.s, contact.s, contact.len);
				p->contact.len= contact.len;
				size+= contact.len;

				p->cseq= row_vals[cseq_col].val.int_val;

				p->remote_contact.s= (char*)shm_malloc(remote_contact.len* sizeof(char));
				if(p->remote_contact.s== NULL)
				{
					LM_ERR("No more shared memory\n");
					goto error;
				}
				memcpy(p->remote_contact.s, remote_contact.s, remote_contact.len);
				p->remote_contact.len= remote_contact.len;

				p->version= row_vals[version_col].val.int_val;
			}

			if(extra_headers.s)
			{
				p->extra_headers= (str*)((char*)p+ size);
				size+= sizeof(str);
				p->extra_headers->s= (char*)p+ size;
				memcpy(p->extra_headers->s, extra_headers.s, extra_headers.len);
				p->extra_headers->len= extra_headers.len;
				size+= extra_headers.len;
			}

			LM_DBG("size= %d\n", size);
			p->event= row_vals[event_col].val.int_val;
			p->expires= row_vals[expires_col].val.int_val;
			p->desired_expires= row_vals[desired_expires_col].val.int_val;
			p->flag|=	row_vals[flag_col].val.int_val;

			memset(&p->etag, 0, sizeof(str));
			if(etag.s && etag.len)
			{
				/* alloc separately */
				p->etag.s= (char*)shm_malloc(etag.len* sizeof(char));
				if(p->etag.s==  NULL)
				{
					LM_ERR("no more share memory\n");
					goto error;
				}	
				memcpy(p->etag.s, etag.s, etag.len);
				p->etag.len= etag.len;
			}

			print_ua_pres(p);

			hash_code= core_hash(p->pres_uri, p->watcher_uri, HASH_SIZE);
			lock_get(&HashT->p_records[hash_code].lock);
			insert_htable(p, hash_code);
			lock_release(&HashT->p_records[hash_code].lock);
		}

	} while((db_fetch_next(&pua_dbf, pua_fetch_rows, pua_db, &res)==1)
			&& (RES_ROW_N(res)>0));

	pua_dbf.free_result(pua_db, res);
	res = NULL;

	if(pua_dbf.delete(pua_db, 0, 0 , 0, 0) < 0)
	{
		LM_ERR("while deleting information from db\n");
		goto error;
	}

	return 0;

error:
	if(res)
		pua_dbf.free_result(pua_db, res);

	if(p)
		shm_free(p);
	return -1;
}

static void hashT_clean(unsigned int ticks,void *param)
{
	int i;
	time_t now;
	ua_pres_t* p= NULL, *q= NULL;

	if (dbmode==PUA_DB_ONLY) 
	{
		clean_puadb(update_period, min_expires );
		return;
	}

	now = time(NULL);
	for(i= 0;i< HASH_SIZE; i++)
	{
		lock_get(&HashT->p_records[i].lock);
		p= HashT->p_records[i].entity->next;
		while(p)
		{	
			print_ua_pres(p);
			if(p->expires- update_period < now )
			{
				if((p->desired_expires> p->expires + min_expires) || 
						(p->desired_expires== 0 ))
				{
					if(update_pua(p)< 0)
					{
						LM_ERR("while updating record\n");
						lock_release(&HashT->p_records[i].lock);
						return;
					}
					p= p->next;
					continue;
				}	
				if(p->expires < now - 10)
				{
					q= p->next;
					LM_DBG("Found expired: uri= %.*s\n", p->pres_uri->len,
							p->pres_uri->s);
					delete_htable(p, i);
					p= q;
				}
				else
					p= p->next;
			}	
			else
				p= p->next;
		}
		lock_release(&HashT->p_records[i].lock);
	}


}

int update_pua(ua_pres_t* p)
{
	str* str_hdr= NULL;
	int expires;
	int result;
	uac_req_t uac_r;
	int ret_code = 0;
	dlg_t* td = NULL;

	if(p->desired_expires== 0)
		expires= 3600;
	else
		expires= p->desired_expires- (int)time(NULL);

	if(p->watcher_uri == NULL || p->watcher_uri->len == 0)
	{
		str met= {"PUBLISH", 7};
		ua_pres_t* cb_param;

		str_hdr = publ_build_hdr(expires, get_event(p->event), NULL,
				&p->etag, p->extra_headers, 0);
		if(str_hdr == NULL)
		{
			LM_ERR("while building extra_headers\n");
			ret_code = -1;
			goto done;
		}
		LM_DBG("str_hdr:\n%.*s\n ", str_hdr->len, str_hdr->s);

		cb_param= build_uppubl_cbparam(p);
		if(cb_param== NULL)
		{
			LM_ERR("while constructing publ callback param\n");
			ret_code = -1;
			goto done;
		}	

		set_uac_req(&uac_r, &met, str_hdr, 0, 0, TMCB_LOCAL_COMPLETED,
				publ_cback_func, (void*)cb_param);
		result= tmb.t_request(&uac_r,
				p->pres_uri,					/* Request-URI */
				p->pres_uri,					/* To */
				p->pres_uri,					/* From */
				&outbound_proxy					/* Outbound proxy*/
				);
		if(result< 0)
		{
			LM_ERR("in t_request function\n"); 
			shm_free(cb_param);
			ret_code = -1;
			goto done;
		}
	}
	else
	{
		str met= {"SUBSCRIBE", 9};
		ua_pres_t* cb_param= NULL;

		td= pua_build_dlg_t(p);
		if(td== NULL)
		{
			LM_ERR("while building tm dlg_t structure");		
			ret_code = -1;
			goto done;
		};

		str_hdr= subs_build_hdr(&p->contact, expires,p->event,p->extra_headers);
		if(str_hdr== NULL || str_hdr->s== NULL)
		{
			if(p->event!=0)
				LM_ERR("while building extra headers\n");
			ret_code = -1;
			goto done;
		}
		cb_param= subs_cbparam_indlg(p, expires, REQ_ME);
		if(cb_param== NULL)
		{
			LM_ERR("while constructing subs callback param\n");
			ret_code = -1;
			goto done;
		}	

		set_uac_req(&uac_r, &met, str_hdr, 0, td, TMCB_LOCAL_COMPLETED,
				subs_cback_func, (void*)cb_param);

		result= tmb.t_request_within(&uac_r);
		if(result< 0)
		{
			LM_ERR("in t_request function\n"); 
			shm_free(cb_param);
			ret_code = -1;
			goto done;
		}
	}
done:
	if(td!=NULL)
	{
		if(td->route_set)
			free_rr(&td->route_set);
		pkg_free(td);
		td= NULL;
	}
	if(str_hdr)
		pkg_free(str_hdr);

	return ret_code;

}

static void db_update(unsigned int ticks,void *param)
{
	ua_pres_t* p= NULL;
	db_key_t q_cols[20], result_cols[1];
	db1_res_t *res= NULL;
	db_key_t db_cols[5];
	db_val_t q_vals[20], db_vals[5];
	db_op_t  db_ops[1] ;
	int n_query_cols= 0, n_query_update= 0;
	int n_update_cols= 0;
	int i;
	int puri_col,pid_col,expires_col,flag_col,etag_col,tuple_col,event_col;
	int watcher_col,callid_col,totag_col,fromtag_col,record_route_col,cseq_col;
	int no_lock= 0, contact_col, desired_expires_col, extra_headers_col;
	int remote_contact_col, version_col;

	if (dbmode==PUA_DB_ONLY) return;

	if(ticks== 0 && param == NULL)
		no_lock= 1;

	/* cols and values used for insert */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	n_query_cols++;

	q_cols[pid_col= n_query_cols] = &str_pres_id_col;	
	q_vals[pid_col].type = DB1_STR;
	q_vals[pid_col].nul = 0;
	n_query_cols++;

	q_cols[flag_col= n_query_cols] = &str_flag_col;
	q_vals[flag_col].type = DB1_INT;
	q_vals[flag_col].nul = 0;
	n_query_cols++;

	q_cols[event_col= n_query_cols] = &str_event_col;
	q_vals[event_col].type = DB1_INT;
	q_vals[event_col].nul = 0;
	n_query_cols++;

	q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
	q_vals[watcher_col].type = DB1_STR;
	q_vals[watcher_col].nul = 0;
	n_query_cols++;

	q_cols[callid_col= n_query_cols] = &str_call_id_col;
	q_vals[callid_col].type = DB1_STR;
	q_vals[callid_col].nul = 0;
	n_query_cols++;

	q_cols[totag_col= n_query_cols] = &str_to_tag_col;
	q_vals[totag_col].type = DB1_STR;
	q_vals[totag_col].nul = 0;
	n_query_cols++;

	q_cols[fromtag_col= n_query_cols] = &str_from_tag_col;
	q_vals[fromtag_col].type = DB1_STR;
	q_vals[fromtag_col].nul = 0;
	n_query_cols++;

	q_cols[etag_col= n_query_cols] = &str_etag_col;
	q_vals[etag_col].type = DB1_STR;
	q_vals[etag_col].nul = 0;
	n_query_cols++;	

	q_cols[tuple_col= n_query_cols] = &str_tuple_id_col;
	q_vals[tuple_col].type = DB1_STR;
	q_vals[tuple_col].nul = 0;
	n_query_cols++;

	q_cols[cseq_col= n_query_cols]= &str_cseq_col;
	q_vals[cseq_col].type = DB1_INT;
	q_vals[cseq_col].nul = 0;
	n_query_cols++;

	q_cols[expires_col= n_query_cols] = &str_expires_col;
	q_vals[expires_col].type = DB1_INT;
	q_vals[expires_col].nul = 0;
	n_query_cols++;

	q_cols[desired_expires_col= n_query_cols] = &str_desired_expires_col;
	q_vals[desired_expires_col].type = DB1_INT;
	q_vals[desired_expires_col].nul = 0;
	n_query_cols++;

	q_cols[record_route_col= n_query_cols] = &str_record_route_col;
	q_vals[record_route_col].type = DB1_STR;
	q_vals[record_route_col].nul = 0;
	n_query_cols++;

	q_cols[contact_col= n_query_cols] = &str_contact_col;
	q_vals[contact_col].type = DB1_STR;
	q_vals[contact_col].nul = 0;
	n_query_cols++;

	q_cols[remote_contact_col= n_query_cols] = &str_remote_contact_col;
	q_vals[remote_contact_col].type = DB1_STR;
	q_vals[remote_contact_col].nul = 0;
	n_query_cols++;

	q_cols[version_col= n_query_cols] = &str_version_col;
	q_vals[version_col].type = DB1_INT;
	q_vals[version_col].nul = 0;
	n_query_cols++;

	/* must keep this the last  column to be inserted */
	q_cols[extra_headers_col= n_query_cols] = &str_extra_headers_col;
	q_vals[extra_headers_col].type = DB1_STR;
	q_vals[extra_headers_col].nul = 0;
	n_query_cols++;

	/* cols and values used for update */
	db_cols[0]= &str_expires_col;
	db_vals[0].type = DB1_INT;
	db_vals[0].nul = 0;

	db_cols[1]= &str_cseq_col;
	db_vals[1].type = DB1_INT;
	db_vals[1].nul = 0;

	db_cols[2]= &str_etag_col;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;

	db_cols[3]= &str_desired_expires_col;
	db_vals[3].type = DB1_INT;
	db_vals[3].nul = 0;

	db_cols[4]= &str_version_col;
	db_vals[4].type = DB1_INT;
	db_vals[4].nul = 0;

	result_cols[0]= &str_expires_col;

	if(pua_db== NULL)
	{
		LM_ERR("null database connection\n");
		return;
	}

	if(pua_dbf.use_table(pua_db, &db_table)< 0)
	{
		LM_ERR("in use table\n");
		return ;
	}

	for(i=0; i<HASH_SIZE; i++) 
	{
		if(!no_lock)
			lock_get(&HashT->p_records[i].lock);	

		p = HashT->p_records[i].entity->next;
		while(p)
		{
			if((int)p->expires - (int)time(NULL)< 0)	
			{
				p= p->next;
				continue;
			}

			switch(p->db_flag)
			{
				case NO_UPDATEDB_FLAG:
					{
						LM_DBG("NO_UPDATEDB_FLAG\n");
						break;			  
					}

				case UPDATEDB_FLAG:
					{
						LM_DBG("UPDATEDB_FLAG\n");
						n_update_cols= 0;
						n_query_update= 0;

						q_vals[puri_col].val.str_val = *(p->pres_uri);
						n_query_update++;

						q_vals[pid_col].val.str_val = p->id;
						n_query_update++;

						q_vals[flag_col].val.int_val = p->flag;
						n_query_update++;

						q_vals[event_col].val.int_val = p->event;
						n_query_update++;

						if(p->watcher_uri)
						{
							q_vals[watcher_col].val.str_val = *(p->watcher_uri);
							n_query_update++;

							q_vals[callid_col].val.str_val = p->call_id;
							n_query_update++;

							q_vals[totag_col].val.str_val = p->to_tag;
							n_query_update++;

							q_vals[fromtag_col].val.str_val = p->from_tag;
							n_query_update++;
						}

						db_vals[0].val.int_val= p->expires;
						n_update_cols++;

						db_vals[1].val.int_val= p->cseq	;
						n_update_cols++;

						db_vals[2].val.str_val= p->etag	;
						n_update_cols++;

						db_vals[3].val.int_val= p->desired_expires;
						n_update_cols++;

						db_vals[4].val.int_val= p->version;
						n_update_cols++;

						LM_DBG("Updating:n_query_update= %d\tn_update_cols= %d\n",
								n_query_update, n_update_cols);

						if(pua_dbf.query(pua_db, q_cols, 0, q_vals,
									result_cols, n_query_update, 1, 0, &res)< 0)
						{
							LM_ERR("while querying db table pua\n");
							if(!no_lock)
								lock_release(&HashT->p_records[i].lock);
							if(res)
								pua_dbf.free_result(pua_db, res);	
							return ;
						}
						if(res && res->n> 0)
						{																				
							if(pua_dbf.update(pua_db, q_cols, 0, q_vals, db_cols, 
										db_vals, n_query_update, n_update_cols)<0)
							{
								LM_ERR("while updating in database\n");
								if(!no_lock)
									lock_release(&HashT->p_records[i].lock);	
								pua_dbf.free_result(pua_db, res);
								res= NULL;
								return ;
							}
							pua_dbf.free_result(pua_db, res);
							res= NULL;		
						}
						else
						{
							if(res)
							{	
								pua_dbf.free_result(pua_db, res);
								res= NULL;
							}
							LM_DBG("UPDATEDB_FLAG and no record found\n");
							//	p->db_flag= INSERTDB_FLAG;
						}	
						break;	
					}
				case INSERTDB_FLAG:
					{	
						LM_DBG("INSERTDB_FLAG\n");
						q_vals[puri_col].val.str_val = *(p->pres_uri);
						q_vals[pid_col].val.str_val = p->id;
						q_vals[flag_col].val.int_val = p->flag;
						q_vals[callid_col].val.str_val = p->call_id;
						q_vals[fromtag_col].val.str_val = p->from_tag;
						q_vals[cseq_col].val.int_val= p->cseq;
						q_vals[expires_col].val.int_val = p->expires;
						q_vals[desired_expires_col].val.int_val = p->desired_expires;
						q_vals[event_col].val.int_val = p->event;
						q_vals[version_col].val.int_val = p->version;

						if((p->watcher_uri))
							q_vals[watcher_col].val.str_val = *(p->watcher_uri);
						else
						{
							q_vals[watcher_col].val.str_val.s = "";
							q_vals[watcher_col].val.str_val.len = 0;
						}

						if(p->tuple_id.s == NULL)
						{
							q_vals[tuple_col].val.str_val.s="";
							q_vals[tuple_col].val.str_val.len=0;
						}
						else
							q_vals[tuple_col].val.str_val = p->tuple_id;

						if(p->etag.s == NULL)
						{
							q_vals[etag_col].val.str_val.s="";
							q_vals[etag_col].val.str_val.len=0;
						}
						else
							q_vals[etag_col].val.str_val = p->etag;

						if (p->to_tag.s == NULL)
						{
							q_vals[totag_col].val.str_val.s="";
							q_vals[totag_col].val.str_val.len=0;
						}
						else
							q_vals[totag_col].val.str_val = p->to_tag;

						if(p->record_route.s== NULL)
						{
							q_vals[record_route_col].val.str_val.s= "";
							q_vals[record_route_col].val.str_val.len = 0;
						}
						else
							q_vals[record_route_col].val.str_val = p->record_route;

						if(p->contact.s == NULL)
						{
							q_vals[contact_col].val.str_val.s = "";
							q_vals[contact_col].val.str_val.len = 0;
						}
						else
							q_vals[contact_col].val.str_val = p->contact;

						if(p->remote_contact.s)
						{
							q_vals[remote_contact_col].val.str_val = p->remote_contact;
							LM_DBG("p->remote_contact = %.*s\n", p->remote_contact.len, p->remote_contact.s);
						}
						else
						{
							q_vals[remote_contact_col].val.str_val.s = "";
							q_vals[remote_contact_col].val.str_val.len = 0;
						}

						if(p->extra_headers)
							q_vals[extra_headers_col].val.str_val = *(p->extra_headers);
						else
						{
							q_vals[extra_headers_col].val.str_val.s = "";
							q_vals[extra_headers_col].val.str_val.len = 0;
						}

						if(pua_dbf.insert(pua_db, q_cols, q_vals,n_query_cols )<0)
						{
							LM_ERR("while inserting in db table pua\n");
							if(!no_lock)
								lock_release(&HashT->p_records[i].lock);
							return ;
						}
						break;
					}

			}
			p->db_flag= NO_UPDATEDB_FLAG;	
			p= p->next;
		}
		if(!no_lock)
			lock_release(&HashT->p_records[i].lock);	
	}

	db_vals[0].val.int_val= (int)time(NULL)- 10;
	db_ops[0]= OP_LT;
	if(pua_dbf.delete(pua_db, db_cols, db_ops, db_vals, 1) < 0)
	{
		LM_ERR("while deleting from db table pua\n");
	}

	return ;
}	

static ua_pres_t* build_uppubl_cbparam(ua_pres_t* p)
{
	publ_info_t publ;
	ua_pres_t* cb_param= NULL;

	memset(&publ, 0, sizeof(publ_info_t));
	publ.pres_uri= p->pres_uri;
	publ.content_type= p->content_type;
	publ.id= p->id;
	publ.expires= (p->desired_expires== 0) ?-1:p->desired_expires- (int)time(NULL);
	publ.flag= UPDATE_TYPE; 
	publ.source_flag= p->flag; 
	publ.event= p->event;
	publ.etag= &p->etag;
	publ.extra_headers= p->extra_headers;

	cb_param= publish_cbparam(&publ, NULL, &p->tuple_id, REQ_ME);
	if(cb_param== NULL)
	{
		LM_ERR("constructing callback parameter\n");
		return NULL;
	}
	return cb_param;
}

static struct mi_root* mi_cleanup(struct mi_root* cmd, void *param)
{
	LM_DBG("mi_cleanup:start\n");

	(void)hashT_clean(0,0);

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}
