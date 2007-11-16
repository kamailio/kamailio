/*
 * $Id: rls.c 2230 2007-06-06 07:13:20Z anca_vamanu $
 *
 * rls module - resource list server
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
 *  2007-09-11  initial version (anca)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../pt.h"
#include "../../db/db.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../tm/tm_load.h"
#include "../sl/sl_api.h"
#include "../presence/bind_presence.h"
#include "../presence/hash.h"
#include "../pua/pua_bind.h"
#include "../pua/pidf.h"
#include "../xcap_client/xcap_functions.h"
#include "rls.h"
#include "notify.h"
#include "resource_notify.h"

MODULE_VERSION

#define P_TABLE_VERSION 0
#define W_TABLE_VERSION 1

/** database connection */
db_con_t *rls_db = NULL;
db_func_t rls_dbf;

/** modules variables */
str server_address= {0, 0};
int waitn_time= 10;
char *rlsubs_table="rls_watchers";
char *rlpres_table= "rls_presentity";
char* rls_xcap_table= "xcap";

str db_url= {0, 0};
int hash_size= 512;
shtable_t rls_table;
int pid;
contains_event_t pres_contains_event;
search_event_t pres_search_event;
get_event_list_t pres_get_ev_list;
int clean_period= 100;
char* xcap_root;
unsigned int xcap_port= 80;
int rls_restore_db_subs(void);
int rls_integrated_xcap_server= 0;

/** libxml api */
xmlDocGetNodeByName_t XMLDocGetNodeByName;
xmlNodeGetNodeByName_t XMLNodeGetNodeByName;
xmlNodeGetNodeContentByName_t XMLNodeGetNodeContentByName;
xmlNodeGetAttrContentByName_t XMLNodeGetAttrContentByName;

/* functions imported from presence to handle subscribe hash table */
new_shtable_t pres_new_shtable;
insert_shtable_t pres_insert_shtable;
search_shtable_t pres_search_shtable;
update_shtable_t pres_update_shtable;
delete_shtable_t pres_delete_shtable;
destroy_shtable_t pres_destroy_shtable;
mem_copy_subs_t  pres_copy_subs;
update_db_subs_t pres_update_db_subs;
extract_sdialog_info_t pres_extract_sdialog_info;
int rls_events= EVENT_PRESENCE;
int to_presence_code= 1;
int rls_max_expires= 7200;

/* functions imported from xcap_client module */
xcapGetNewDoc_t xcap_GetNewDoc= 0;

/* functions imported from pua module*/
send_subscribe_t pua_send_subscribe;
get_record_id_t pua_get_record_id;

/* TM bind */
struct tm_binds tmb;
/* SL bind */
struct sl_binds slb;

/** module functions */

static int mod_init(void);
static int child_init(int);
int rls_handle_subscribe(struct sip_msg*, char*, char*);
void destroy(void);
int rlsubs_table_restore();
void rlsubs_table_update(unsigned int ticks,void *param);
int add_rls_event(modparam_t type, void* val);

static cmd_export_t cmds[]=
{
	{"rls_handle_subscribe",	rls_handle_subscribe,  0,	 0, 0, REQUEST_ROUTE},
	{"rls_handle_notify",   	rls_handle_notify,     0,	 0, 0, REQUEST_ROUTE},
	{0,				        		  0, 0,		       0,	 0,     0	     }	 
};

static param_export_t params[]={
	{ "server_address",         STR_PARAM,   &server_address.s			     },
	{ "db_url",					STR_PARAM,   &db_url.s					     },
	{ "rlsubs_table",	        STR_PARAM,   &rlsubs_table				     },
	{ "rlpres_table",			STR_PARAM,   &rlpres_table				     },
	{ "xcap_table",		    	STR_PARAM,   &rls_xcap_table    		     },
	{ "waitn_time",				INT_PARAM,   &waitn_time				     },
	{ "clean_period",			INT_PARAM,   &clean_period				     },
	{ "max_expires",			INT_PARAM,   &rls_max_expires			     },
	{ "hash_size",			    INT_PARAM,   &hash_size			             },
	{ "integrated_xcap_server",	INT_PARAM,   &rls_integrated_xcap_server     },	
	{ "to_presence_code",       INT_PARAM,   &to_presence_code               },
	{ "xcap_root",              STR_PARAM,   &xcap_root                      },
	/*address and port(default: 80):"http://192.168.2.132/xcap-root:800"*/
	{ "rls_event",              STR_PARAM|USE_FUNC_PARAM,(void*)add_rls_event},
	{0,							0,				0						     }
};

/** module exports */
struct module_exports exports= {
	"rls",  					/* module name */
	DEFAULT_DLFLAGS,			/* dlopen flags */
	cmds,						/* exported functions */
	params,						/* exported parameters */
	0,							/* exported statistics */
	0,      					/* exported MI functions */
	0,							/* exported pseudo-variables */
	0,							/* extra processes */
	mod_init,					/* module initialization function */
	(response_function) 0,      /* response handling function */
	(destroy_function) destroy, /* destroy function */
	child_init                  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	str _s;
	int ver = 0;
	bind_presence_t bind_presence;
	presence_api_t pres;
	bind_pua_t bind_pua;
	pua_api_t pua;
	bind_libxml_t bind_libxml;
	libxml_api_t libxml_api;
	bind_xcap_t bind_xcap;
	xcap_api_t xcap_api;
	char* sep;

	LM_DBG("start\n");

	if(server_address.s== NULL)
	{
		LM_DBG("server_address parameter not set in configuration file\n");
	}	
	if(server_address.s)
		server_address.len= strlen(server_address.s);
	else
		server_address.len= 0;
	
	if(xcap_root== NULL)
	{
		LM_ERR("xcap_root parameter not set\n");
		return -1;
	}
	/* extract port if any */
	sep= strchr(xcap_root, ':');
	if(sep)
	{
		char* sep2= NULL;
		sep2= strchr(sep+ 1, ':');
		if(sep2)
			sep= sep2;
		
		str port_str;

		port_str.s= sep+ 1;
		port_str.len= strlen(xcap_root)- (port_str.s-xcap_root);

		if(str2int(&port_str, &xcap_port)< 0)
		{
			LM_ERR("converting string to int [port]= %.*s\n",port_str.len,
					port_str.s);
			return -1;
		}
		if(xcap_port< 0 || xcap_port> 65535)
		{
			LM_ERR("wrong xcap server port\n");
			return -1;
		}
		*sep= '\0';
	}

	/* load SL API */
	if(load_sl_api(&slb)==-1)
	{
		LM_ERR("can't load sl functions\n");
		return -1;
	}

	/* load all TM stuff */
	if(load_tm_api(&tmb)==-1)
	{
		LM_ERR("can't load tm functions\n");
		return -1;
	}
	bind_presence= (bind_presence_t)find_export("bind_presence", 1,0);
	if (!bind_presence)
	{
		LM_ERR("Can't bind presence\n");
		return -1;
	}
	if (bind_presence(&pres) < 0)
	{
		LM_ERR("Can't bind presence\n");
		return -1;
	}
	pres_contains_event = pres.contains_event;
	pres_search_event   = pres.search_event;
	pres_get_ev_list    = pres.get_event_list;
	pres_new_shtable    = pres.new_shtable;
	pres_destroy_shtable= pres.destroy_shtable;
	pres_insert_shtable = pres.insert_shtable;
	pres_delete_shtable = pres.delete_shtable;
	pres_update_shtable = pres.update_shtable;
	pres_search_shtable = pres.search_shtable;
	pres_copy_subs      = pres.mem_copy_subs;
	pres_update_db_subs = pres.update_db_subs;
	pres_extract_sdialog_info= pres.extract_sdialog_info;

	if(!pres_contains_event || !pres_get_ev_list || !pres_new_shtable ||
		!pres_destroy_shtable || !pres_insert_shtable || !pres_delete_shtable
		 || !pres_update_shtable || !pres_search_shtable || !pres_copy_subs
		 || !pres_extract_sdialog_info)
	{
		LM_ERR("importing functions from presence module\n");
		return -1;
	}
	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len,
			db_url.s);
	
	/* binding to mysql module  */
	if (bind_dbmod(db_url.s, &rls_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}
	
	if (!DB_CAPABILITY(rls_dbf, DB_CAP_ALL)) {
		LM_ERR("Database module does not implement all functions"
				" needed by the module\n");
		return -1;
	}

	rls_db = rls_dbf.init(db_url.s);
	if (!rls_db)
	{
		LM_ERR("while connecting database\n");
		return -1;
	}
	/* verify table version */
	_s.s = rlsubs_table;
	_s.len = strlen(rlsubs_table);
	 ver =  table_version(&rls_dbf, rls_db, &_s);
	if(ver!=W_TABLE_VERSION)
	{
		LM_ERR("Wrong version v%d for table <%s>,"
				" need v%d\n", ver, _s.s, W_TABLE_VERSION);
		return -1;
	}
	
	_s.s = rlpres_table;
	_s.len = strlen(rlpres_table);
	 ver =  table_version(&rls_dbf, rls_db, &_s);
	if(ver!=P_TABLE_VERSION)
	{
		LM_ERR("Wrong version v%d for table <%s>,"
				" need v%d\n", ver, _s.s, P_TABLE_VERSION);
		return -1;
	}
	if(hash_size<=1)
		hash_size= 512;
	else
		hash_size = 1<<hash_size;

	rls_table= pres_new_shtable(hash_size);
	if(rls_table== NULL)
	{
		LM_ERR("while creating new hash table\n");
		return -1;
	}
	if(rls_restore_db_subs()< 0)
	{
		LM_ERR("while restoring rl watchers table\n");
		return -1;
	}

	if(rls_db)
		rls_dbf.close(rls_db);
	rls_db = NULL;

	if(waitn_time<= 0)
		waitn_time= 5;
	
	if(waitn_time<= 0)
		waitn_time= 100;

	/* bind libxml wrapper functions */

	if((bind_libxml=(bind_libxml_t)find_export("bind_libxml_api", 1, 0))== NULL)
	{
		LM_ERR("can't import bind_libxml_api\n");
		return -1;
	}
	if(bind_libxml(&libxml_api)< 0)
	{
		LM_ERR("can not bind libxml api\n");
		return -1;
	}
	XMLNodeGetAttrContentByName= libxml_api.xmlNodeGetAttrContentByName;
	XMLDocGetNodeByName= libxml_api.xmlDocGetNodeByName;
	XMLNodeGetNodeByName= libxml_api.xmlNodeGetNodeByName;
    XMLNodeGetNodeContentByName= libxml_api.xmlNodeGetNodeContentByName;

	if(XMLNodeGetAttrContentByName== NULL || XMLDocGetNodeByName== NULL ||
			XMLNodeGetNodeByName== NULL || XMLNodeGetNodeContentByName== NULL)
	{
		LM_ERR("libxml wrapper functions could not be bound\n");
		return -1;
	}

	/* bind pua */
	bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
	if (!bind_pua)
	{
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	
	if (bind_pua(&pua) < 0)
	{
		LM_ERR("mod_init Can't bind pua\n");
		return -1;
	}
	if(pua.send_subscribe == NULL)
	{
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}
	pua_send_subscribe= pua.send_subscribe;
	
	if(pua.get_record_id == NULL)
	{
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}
	pua_get_record_id= pua.get_record_id;

	if(!rls_integrated_xcap_server)
	{
		/* bind xcap */
		bind_xcap= (bind_xcap_t)find_export("bind_xcap", 1, 0);
		if (!bind_xcap)
		{
			LM_ERR("Can't bind xcap_client\n");
			return -1;
		}
	
		if (bind_xcap(&xcap_api) < 0)
		{
			LM_ERR("Can't bind xcap\n");
			return -1;
		}
		xcap_GetNewDoc= xcap_api.getNewDoc;
		if(xcap_GetNewDoc== NULL)
		{
			LM_ERR("Can't import xcap_client functions\n");
			return -1;
		}
	}
	register_timer(timer_send_notify,0, waitn_time);
	
	register_timer(rls_presentity_clean, 0, clean_period);
	
	register_timer(rlsubs_table_update, 0, clean_period);
	
	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	LM_DBG("child [%d]  pid [%d]\n", rank, getpid());
	if (rls_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	rls_db = rls_dbf.init(db_url.s);
	if (!rls_db)
	{
		LM_ERR("child %d: Error while connecting database\n",
				rank);
		return -1;
	}
	else
	{
		if (rls_dbf.use_table(rls_db, rlsubs_table) < 0)  
		{
			LM_ERR("child %d: Error in use_table rlsubs_table\n", rank);
			return -1;
		}
		if (rls_dbf.use_table(rls_db, rlpres_table) < 0)  
		{
			LM_ERR("child %d: Error in use_table rlpres_table\n", rank);
			return -1;
		}

		LM_DBG("child %d: Database connection opened successfully\n", rank);
	}

	pid= my_pid();
	return 0;
}

/*
 * destroy function
 */
void destroy(void)
{
	LM_DBG("start\n");
	
	if(rls_table)
	{
		if(rls_db)
			rlsubs_table_update(0, 0);
		pres_destroy_shtable(rls_table, hash_size);
	}
	if(rls_db && rls_dbf.close)
		rls_dbf.close(rls_db);
}

int handle_expired_record(subs_t* s)
{
	/* send Notify with state terminated*/	
	if( rls_send_notify(s, NULL, NULL, NULL)< 0)
	{
		LM_ERR("in function send_notify\n");
		return -1;
	}
	
	return 0;
}

void rlsubs_table_update(unsigned int ticks,void *param)
{
	int no_lock= 0;

	if(ticks== 0 && param == NULL)
		no_lock= 1;
	
	if(rls_dbf.use_table(rls_db, rlsubs_table)< 0)
	{
		LM_ERR("sql use table failed\n");
		return;
	}
	pres_update_db_subs(rls_db, rls_dbf, rls_table, hash_size, 
			no_lock, handle_expired_record);

}

int rls_restore_db_subs(void)
{
	db_key_t result_cols[22]; 
	db_res_t *res= NULL;
	db_row_t *row = NULL;	
	db_val_t *row_vals= NULL;
	int i;
	int n_result_cols= 0;
	int pres_uri_col, expires_col, from_user_col, from_domain_col,to_user_col; 
	int callid_col,totag_col,fromtag_col,to_domain_col,sockinfo_col,reason_col;
	int event_col,contact_col,record_route_col, event_id_col, status_col;
	int remote_cseq_col, local_cseq_col, local_contact_col, version_col;
	subs_t s;
	str ev_sname;
	pres_ev_t* event= NULL;
	event_t parsed_event;
	unsigned int expires;
	unsigned int hash_code;

	result_cols[pres_uri_col=n_result_cols++]	="presentity_uri";		
	result_cols[expires_col=n_result_cols++]="expires";
	result_cols[event_col=n_result_cols++]	="event";
	result_cols[event_id_col=n_result_cols++]="event_id";
	result_cols[to_user_col=n_result_cols++]	="to_user";
	result_cols[to_domain_col=n_result_cols++]	="to_domain";
	result_cols[from_user_col=n_result_cols++]	="watcher_username";
	result_cols[from_domain_col=n_result_cols++]="watcher_domain";
	result_cols[callid_col=n_result_cols++] ="callid";
	result_cols[totag_col=n_result_cols++]	="to_tag";
	result_cols[fromtag_col=n_result_cols++]="from_tag";
	result_cols[local_cseq_col= n_result_cols++]	="local_cseq";
	result_cols[remote_cseq_col= n_result_cols++]	="remote_cseq";
	result_cols[record_route_col= n_result_cols++]	="record_route";
	result_cols[sockinfo_col= n_result_cols++]	="socket_info";
	result_cols[contact_col= n_result_cols++]	="contact";
	result_cols[local_contact_col= n_result_cols++]	="local_contact";
	result_cols[version_col= n_result_cols++]	="version";
	result_cols[status_col= n_result_cols++]	="status";
	result_cols[reason_col= n_result_cols++]	="reason";
	
	if(!rls_db)
	{
		LM_ERR("null database connection\n");
		return -1;
	}
	if(rls_dbf.use_table(rls_db, rlsubs_table)< 0)
	{
		LM_ERR("in use table\n");
		return -1;
	}

	if(rls_dbf.query(rls_db,0, 0, 0, result_cols,0, n_result_cols, 0,&res)< 0)
	{
		LM_ERR("while querrying table\n");
		if(res)
		{
			rls_dbf.free_result(rls_db, res);
			res = NULL;
		}
		return -1;
	}
	if(res== NULL)
		return -1;

	if(res->n<=0)
	{
		LM_INFO("The query returned no result\n");
		rls_dbf.free_result(rls_db, res);
		res = NULL;
		return 0;
	}

	LM_DBG("found %d db entries\n", res->n);

	for(i =0 ; i< res->n ; i++)
	{
		row = &res->rows[i];
		row_vals = ROW_VALUES(row);
		memset(&s, 0, sizeof(subs_t));

		expires= row_vals[expires_col].val.int_val;
		
		if(expires< (int)time(NULL))
			continue;
	
		s.pres_uri.s= (char*)row_vals[pres_uri_col].val.string_val;
		s.pres_uri.len= strlen(s.pres_uri.s);
		
		s.to_user.s=(char*)row_vals[to_user_col].val.string_val;
		s.to_user.len= strlen(s.to_user.s);

		s.to_domain.s=(char*)row_vals[to_domain_col].val.string_val;
		s.to_domain.len= strlen(s.to_domain.s);

		s.from_user.s=(char*)row_vals[from_user_col].val.string_val;
		s.from_user.len= strlen(s.from_user.s);
		
		s.from_domain.s=(char*)row_vals[from_domain_col].val.string_val;
		s.from_domain.len= strlen(s.from_domain.s);

		s.to_tag.s=(char*)row_vals[totag_col].val.string_val;
		s.to_tag.len= strlen(s.to_tag.s);

		s.from_tag.s=(char*)row_vals[fromtag_col].val.string_val;
		s.from_tag.len= strlen(s.from_tag.s);

		s.callid.s=(char*)row_vals[callid_col].val.string_val;
		s.callid.len= strlen(s.callid.s);

		ev_sname.s= (char*)row_vals[event_col].val.string_val;
		ev_sname.len= strlen(ev_sname.s);
		
		event= pres_contains_event(&ev_sname, &parsed_event);
		if(event== NULL)
		{
			LM_ERR("event not found in list\n");
			goto error;
		}
		s.event= event;

		s.event_id.s=(char*)row_vals[event_id_col].val.string_val;
		if(s.event_id.s)
			s.event_id.len= strlen(s.event_id.s);

		s.remote_cseq= row_vals[remote_cseq_col].val.int_val;
		s.local_cseq= row_vals[local_cseq_col].val.int_val;
		s.version= row_vals[version_col].val.int_val;
		
		s.expires= expires- (int)time(NULL);
		s.status= row_vals[status_col].val.int_val;

		s.reason.s= (char*)row_vals[reason_col].val.string_val;
		if(s.reason.s)
			s.reason.len= strlen(s.reason.s);

		s.contact.s=(char*)row_vals[contact_col].val.string_val;
		s.contact.len= strlen(s.contact.s);

		s.local_contact.s=(char*)row_vals[local_contact_col].val.string_val;
		s.local_contact.len= strlen(s.local_contact.s);
	
		s.record_route.s=(char*)row_vals[record_route_col].val.string_val;
		if(s.record_route.s)
			s.record_route.len= strlen(s.record_route.s);
	
		s.sockinfo_str.s=(char*)row_vals[sockinfo_col].val.string_val;
		s.sockinfo_str.len= strlen(s.sockinfo_str.s);

		hash_code= core_hash(&s.pres_uri, &s.event->name, hash_size);
		if(pres_insert_shtable(rls_table, hash_code, &s)< 0)
		{
			LM_ERR("adding new record in hash table\n");
			goto error;
		}
	}

	rls_dbf.free_result(rls_db, res);

	/* delete all records */
	if(rls_dbf.delete(rls_db, 0,0,0,0)< 0)
	{
		LM_ERR("deleting all records from database table\n");
		return -1;
	}

	return 0;

error:
	if(res)
		rls_dbf.free_result(rls_db, res);
	return -1;

}

int add_rls_event(modparam_t type, void* val)
{
	char* event= (char*)val;
	event_t e;

	if(event_parser(event, strlen(event), &e)< 0)
	{
		LM_ERR("while parsing event = %s\n", event);
		return -1;
	}
	if(e.parsed & EVENT_OTHER)
	{
		LM_ERR("wrong event= %s\n", event);
		return -1;
	}

	rls_events|= e.parsed;

	return 0;

}
