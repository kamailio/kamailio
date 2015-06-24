/*
 * rls module - resource list server
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../pt.h"
#include "../../lib/srdb1/db.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../timer_proc.h"
#include "../../hashes.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../presence/bind_presence.h"
#include "../presence/hash.h"
#include "../pua/pua_bind.h"
#include "../pua/pidf.h"
#include "../xcap_client/xcap_functions.h"
#include "rls.h"
#include "notify.h"
#include "resource_notify.h"
#include "api.h"
#include "subscribe.h"
#include "../../mod_fix.h"

MODULE_VERSION

#define P_TABLE_VERSION 1
#define W_TABLE_VERSION 3
#define X_TABLE_VERSION 4

/** database connection */
db1_con_t *rls_db = NULL;
db_func_t rls_dbf;
db1_con_t *rlpres_db = NULL;
db_func_t rlpres_dbf;
db1_con_t *rls_xcap_db = NULL;
db_func_t rls_xcap_dbf;

/** modules variables */
str rls_server_address = {0, 0};
int rls_expires_offset=0;
int waitn_time = 5;
int rls_notifier_poll_rate = 10;
int rls_notifier_processes = 1;
str rlsubs_table = str_init("rls_watchers");
str rlpres_table = str_init("rls_presentity");
str rls_xcap_table = str_init("xcap");

int *rls_notifier_id = NULL;

str db_url = str_init(DEFAULT_DB_URL);
str xcap_db_url = str_init("");
str rlpres_db_url = str_init("");
int hash_size = 9;
shtable_t rls_table;
contains_event_t pres_contains_event;
search_event_t pres_search_event;
get_event_list_t pres_get_ev_list;
int clean_period = 100;
int rlpres_clean_period = -1;

/* Lock for rls_update_subs */
gen_lock_t *rls_update_subs_lock = NULL;


/* address and port(default: 80):"http://192.168.2.132:8000/xcap-root"*/
char* xcap_root;
unsigned int xcap_port = 8000;
int rls_restore_db_subs(void);
int rls_integrated_xcap_server = 0;

/** libxml api */
xmlDocGetNodeByName_t XMLDocGetNodeByName;
xmlNodeGetNodeByName_t XMLNodeGetNodeByName;
xmlNodeGetNodeContentByName_t XMLNodeGetNodeContentByName;
xmlNodeGetAttrContentByName_t XMLNodeGetAttrContentByName;

/* functions imported from presence to handle subscribe hash table */
extern shtable_t rls_new_shtable(int hash_size);
extern void rls_destroy_shtable(shtable_t htable, int hash_size);
extern int rls_insert_shtable(shtable_t htable,unsigned int hash_code, subs_t* subs);
extern subs_t* rls_search_shtable(shtable_t htable,str callid,str to_tag,
		str from_tag,unsigned int hash_code);
extern int rls_delete_shtable(shtable_t htable,unsigned int hash_code, subs_t* subs);
extern int rls_update_shtable(shtable_t htable,unsigned int hash_code, 
		subs_t* subs, int type);
extern void rls_update_db_subs_timer(db1_con_t *db,db_func_t dbf, shtable_t hash_table,
	int htable_size, int no_lock, handle_expired_func_t handle_expired_func);

new_shtable_t pres_new_shtable;
insert_shtable_t pres_insert_shtable;
search_shtable_t pres_search_shtable;
update_shtable_t pres_update_shtable;
delete_shtable_t pres_delete_shtable;
destroy_shtable_t pres_destroy_shtable;
mem_copy_subs_t  pres_copy_subs;
update_db_subs_t pres_update_db_subs_timer;
extract_sdialog_info_t pres_extract_sdialog_info;
int rls_events= EVENT_PRESENCE;
int to_presence_code = 1;
int rls_max_expires = 7200;
int rls_reload_db_subs = 0;
int rls_max_notify_body_len = 0;
int dbmode = 0;

/* functions imported from xcap_client module */
xcapGetNewDoc_t xcap_GetNewDoc = 0;

/* functions imported from pua module*/
send_subscribe_t pua_send_subscribe;
get_record_id_t pua_get_record_id;
get_subs_list_t pua_get_subs_list;

/* TM bind */
struct tm_binds tmb;
/** SL API structure */
sl_api_t slb;

str str_rlsubs_did_col = str_init("rlsubs_did");
str str_resource_uri_col = str_init("resource_uri");
str str_updated_col = str_init("updated");
str str_auth_state_col = str_init("auth_state");
str str_reason_col = str_init("reason");
str str_content_type_col = str_init("content_type");
str str_presence_state_col = str_init("presence_state");
str str_expires_col = str_init("expires");
str str_presentity_uri_col = str_init("presentity_uri");
str str_event_col = str_init("event");
str str_event_id_col = str_init("event_id");
str str_to_user_col = str_init("to_user");
str str_to_domain_col = str_init("to_domain");
str str_from_user_col = str_init("from_user");
str str_from_domain_col = str_init("from_domain");
str str_watcher_username_col = str_init("watcher_username");
str str_watcher_domain_col = str_init("watcher_domain");
str str_callid_col = str_init("callid");
str str_to_tag_col = str_init("to_tag");
str str_from_tag_col = str_init("from_tag");
str str_local_cseq_col = str_init("local_cseq");
str str_remote_cseq_col = str_init("remote_cseq");
str str_record_route_col = str_init("record_route");
str str_socket_info_col = str_init("socket_info");
str str_contact_col = str_init("contact");
str str_local_contact_col = str_init("local_contact");
str str_version_col = str_init("version");
str str_status_col = str_init("status");
str str_username_col = str_init("username");
str str_domain_col = str_init("domain");
str str_doc_type_col = str_init("doc_type");
str str_etag_col = str_init("etag");
str str_doc_col = str_init("doc");
str str_doc_uri_col = str_init("doc_uri");

/* outbound proxy address */
str rls_outbound_proxy = {0, 0};

int rls_fetch_rows = 500;

int rls_disable_remote_presence = 0;
int rls_max_backend_subs = 0;

/** module functions */

static int mod_init(void);
static int child_init(int);
static int mi_child_init(void);
static void destroy(void);
int rlsubs_table_restore();
void rlsubs_table_update(unsigned int ticks,void *param);
int add_rls_event(modparam_t type, void* val);
int rls_update_subs(struct sip_msg *msg, char *puri, char *pevent);
int fixup_update_subs(void** param, int param_no);
static struct mi_root* mi_cleanup(struct mi_root* cmd, void* param);

static cmd_export_t cmds[]=
{
	{"rls_handle_subscribe",  (cmd_function)rls_handle_subscribe0,  0,
			0, 0, REQUEST_ROUTE},
	{"rls_handle_subscribe",  (cmd_function)w_rls_handle_subscribe, 1,
			fixup_spve_null, 0, REQUEST_ROUTE},
	{"rls_handle_notify",     (cmd_function)rls_handle_notify,      0,
			0, 0, REQUEST_ROUTE},
	{"rls_update_subs",       (cmd_function)rls_update_subs,	2,
			fixup_update_subs, 0, ANY_ROUTE},
	{"bind_rls",              (cmd_function)bind_rls,		1,
			0, 0, 0},
	{0, 0, 0, 0, 0, 0 }
};

static param_export_t params[]={
	{ "server_address",         PARAM_STR,   &rls_server_address           },
	{ "db_url",                 PARAM_STR,   &db_url                       },
	{ "rlpres_db_url",          PARAM_STR,   &rlpres_db_url	         },
	{ "xcap_db_url",            PARAM_STR,   &xcap_db_url                  },
	{ "rlsubs_table",           PARAM_STR,   &rlsubs_table                 },
	{ "rlpres_table",           PARAM_STR,   &rlpres_table                 },
	{ "xcap_table",             PARAM_STR,   &rls_xcap_table               },
	{ "waitn_time",             INT_PARAM,   &waitn_time                     },
	{ "notifier_poll_rate",     INT_PARAM,   &rls_notifier_poll_rate         },
	{ "notifier_processes",     INT_PARAM,   &rls_notifier_processes         },
	{ "clean_period",           INT_PARAM,   &clean_period                   },
	{ "rlpres_clean_period",    INT_PARAM,   &rlpres_clean_period            },
	{ "max_expires",            INT_PARAM,   &rls_max_expires                },
	{ "hash_size",              INT_PARAM,   &hash_size                      },
	{ "integrated_xcap_server", INT_PARAM,   &rls_integrated_xcap_server     },
	{ "to_presence_code",       INT_PARAM,   &to_presence_code               },
	{ "xcap_root",              PARAM_STRING,   &xcap_root                      },
	{ "rls_event",              PARAM_STRING|USE_FUNC_PARAM,(void*)add_rls_event},
	{ "outbound_proxy",         PARAM_STR,   &rls_outbound_proxy           },
	{ "reload_db_subs",         INT_PARAM,   &rls_reload_db_subs             },
	{ "max_notify_body_length", INT_PARAM,	 &rls_max_notify_body_len	 },
	{ "db_mode",                INT_PARAM,	 &dbmode                         },
	{ "expires_offset",         INT_PARAM,	 &rls_expires_offset             },
	{ "fetch_rows",             INT_PARAM,   &rls_fetch_rows                 },
	{ "disable_remote_presence",INT_PARAM,   &rls_disable_remote_presence    },
	{ "max_backend_subs",       INT_PARAM,   &rls_max_backend_subs           },
	{0,                         0,           0                               }
};

static mi_export_t mi_cmds[] = {
	{ "rls_cleanup",	mi_cleanup,		0,  0,  mi_child_init},
	{ 0,			0,			0,  0,  0}
};

/** module exports */
struct module_exports exports= {
	"rls",  			/* module name */
	DEFAULT_DLFLAGS,		/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,				/* exported statistics */
	mi_cmds,      			/* exported MI functions */
	0,				/* exported pseudo-variables */
	0,				/* extra processes */
	mod_init,			/* module initialization function */
	0,				/* response handling function */
	(destroy_function) destroy,	/* destroy function */
	child_init			/* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
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

	if (register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if (dbmode <RLS_DB_DEFAULT || dbmode > RLS_DB_ONLY)
	{
		LM_ERR( "Invalid dbmode-set to default mode\n" );
		dbmode = 0;
	}

	if(!rls_server_address.s || rls_server_address.len<=0)
	{
		LM_ERR("server_address parameter not set in configuration file\n");
		return -1;
	}	
	
	if(!rls_integrated_xcap_server && xcap_root== NULL)
	{
		LM_ERR("xcap_root parameter not set\n");
		return -1;
	}
	/* extract port if any */
	if(xcap_root)
	{
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
				LM_ERR("converting string to int [port]= %.*s\n",
					port_str.len, port_str.s);
				return -1;
			}
			if(xcap_port< 1 || xcap_port> 65535)
			{
				LM_ERR("wrong xcap server port\n");
				return -1;
			}
			*sep= '\0';
		}
	}

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
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

	if (rls_expires_offset < 0 ) 
	{
		LM_ERR( "Negative expires_offset, defaulted to zero\n" );
		rls_expires_offset = 0; 
	}

	if (dbmode == RLS_DB_ONLY)
	{
		pres_new_shtable          = rls_new_shtable;
		pres_destroy_shtable      = rls_destroy_shtable;
		pres_insert_shtable       = rls_insert_shtable;
		pres_delete_shtable       = rls_delete_shtable;
		pres_update_shtable       = rls_update_shtable;
		pres_search_shtable       = rls_search_shtable;
		pres_update_db_subs_timer = rls_update_db_subs_timer;
	}
	else
	{
		pres_new_shtable          = pres.new_shtable;
		pres_destroy_shtable      = pres.destroy_shtable;
		pres_insert_shtable       = pres.insert_shtable;
		pres_delete_shtable       = pres.delete_shtable;
		pres_update_shtable       = pres.update_shtable;
		pres_search_shtable       = pres.search_shtable;
		pres_update_db_subs_timer = pres.update_db_subs_timer;
	}

	pres_copy_subs      = pres.mem_copy_subs;
	pres_extract_sdialog_info= pres.extract_sdialog_info;

	if(!pres_contains_event || !pres_get_ev_list || !pres_new_shtable ||
		!pres_destroy_shtable || !pres_insert_shtable || !pres_delete_shtable
		 || !pres_update_shtable || !pres_search_shtable || !pres_copy_subs
		 || !pres_extract_sdialog_info)
	{
		LM_ERR("importing functions from presence module\n");
		return -1;
	}

	LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len, db_url.s);

	if(xcap_db_url.len==0)
	{
		xcap_db_url.s = db_url.s;
		xcap_db_url.len = db_url.len;
	}

	LM_DBG("db_url=%s/%d/%p\n", ZSW(xcap_db_url.s), xcap_db_url.len, xcap_db_url.s);

	if(rlpres_db_url.len==0)
	{
		rlpres_db_url.s = db_url.s;
		rlpres_db_url.len = db_url.len;
	}

	LM_DBG("db_url=%s/%d/%p\n", ZSW(rlpres_db_url.s), rlpres_db_url.len, rlpres_db_url.s);

	
	/* binding to mysql module  */

	if (db_bind_mod(&db_url, &rls_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}

	if (db_bind_mod(&rlpres_db_url, &rlpres_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}
	
	if (db_bind_mod(&xcap_db_url, &rls_xcap_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(rls_dbf, DB_CAP_ALL)) {
		LM_ERR("Database module does not implement all functions"
				" needed by the module\n");
		return -1;
	}

	if (!DB_CAPABILITY(rlpres_dbf, DB_CAP_ALL)) {
		LM_ERR("Database module does not implement all functions"
				" needed by the module\n");
		return -1;
	}

	if (!DB_CAPABILITY(rls_xcap_dbf, DB_CAP_ALL)) {
		LM_ERR("Database module does not implement all functions"
				" needed by the module\n");
		return -1;
	}

	rls_db = rls_dbf.init(&db_url);
	if (!rls_db)
	{
		LM_ERR("while connecting database\n");
		return -1;
	}

	rlpres_db = rlpres_dbf.init(&rlpres_db_url);
	if (!rlpres_db)
	{
		LM_ERR("while connecting database\n");
		return -1;
	}

	rls_xcap_db = rls_xcap_dbf.init(&xcap_db_url);
	if (!rls_xcap_db)
	{
		LM_ERR("while connecting database\n");
		return -1;
	}

	/* verify table version */
	if(db_check_table_version(&rls_dbf, rls_db, &rlsubs_table, W_TABLE_VERSION) < 0) {
			LM_ERR("error during table version check.\n");
			return -1;
	}

	/* verify table version */
	if(db_check_table_version(&rlpres_dbf, rlpres_db, &rlpres_table, P_TABLE_VERSION) < 0) {
			LM_ERR("error during table version check.\n");
			return -1;
	}

	/* verify table version */
	if(db_check_table_version(&rls_xcap_dbf, rls_xcap_db, &rls_xcap_table, X_TABLE_VERSION) < 0)
	{
			LM_ERR("error during table version check.\n");
			return -1;
	}

	if (dbmode != RLS_DB_ONLY)
	{
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
		if(rls_reload_db_subs!=0)
		{
			if(rls_restore_db_subs()< 0)
			{
				LM_ERR("while restoring rl watchers table\n");
				return -1;
			}
		}
	}

	if(rls_db)
		rls_dbf.close(rls_db);
	rls_db = NULL;

	if(rlpres_db)
		rlpres_dbf.close(rlpres_db);
	rlpres_db = NULL;

	if(rls_xcap_db)
		rls_xcap_dbf.close(rls_xcap_db);
	rls_xcap_db = NULL;

	if(waitn_time<= 0)
		waitn_time= 5;

	if(rls_notifier_poll_rate<= 0)
		rls_notifier_poll_rate= 10;

	if(rls_notifier_processes<= 0)
		rls_notifier_processes= 1;

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
		LM_ERR("Could not import get_record_id\n");
		return -1;
	}
	pua_get_record_id= pua.get_record_id;

	if(pua.get_subs_list == NULL)
	{
		LM_ERR("Could not import get_subs_list\n");
		return -1;
	}
	pua_get_subs_list= pua.get_subs_list;

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

	if (rlpres_clean_period < 0)
		rlpres_clean_period = clean_period;

	if (clean_period > 0)		
		register_timer(rlsubs_table_update, 0, clean_period);
	
	if (rlpres_clean_period > 0)
		register_timer(rls_presentity_clean, 0, rlpres_clean_period);

	if(dbmode == RLS_DB_ONLY)
	{
		if ((rls_notifier_id = shm_malloc(sizeof(int) * rls_notifier_processes)) == NULL)
		{
			LM_ERR("allocating shared memory\n");
			return -1;
		}

		register_basic_timers(rls_notifier_processes);
	}
	else
		register_timer(timer_send_notify, 0, waitn_time);

	if ((rls_update_subs_lock = lock_alloc()) == NULL)
	{
		LM_ERR("Failed to alloc rls_update_subs_lock\n");
		return -1;
	}
	if (lock_init(rls_update_subs_lock) == NULL)
	{
		LM_ERR("Failed to init rls_updae_subs_lock\n");
		return -1;
	}

	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;

	if (rank==PROC_MAIN && dbmode == RLS_DB_ONLY)
	{
		int i;

		for (i = 0; i < rls_notifier_processes; i++)
		{
			char tmp[16];
			snprintf(tmp, 16, "RLS NOTIFIER %d", i);
			rls_notifier_id[i] = i;

			if (fork_basic_utimer(PROC_TIMER, tmp, 1,
						timer_send_notify,
						&rls_notifier_id[i],
						1000000/rls_notifier_poll_rate) < 0)
			{
				LM_ERR("Failed to start RLS NOTIFIER %d\n", i);
				return -1;
			}
		}

		return 0;
	}

	LM_DBG("child [%d]  pid [%d]\n", rank, getpid());

	if (rls_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	/* In DB only mode do not pool the connections where possible. */
	if (dbmode == RLS_DB_ONLY && rls_dbf.init2)
		rls_db = rls_dbf.init2(&db_url, DB_POOLING_NONE);
	else
		rls_db = rls_dbf.init(&db_url);
	if (!rls_db)
	{
		LM_ERR("child %d: Error while connecting database\n",
				rank);
		return -1;
	}
	else
	{
		if (rls_dbf.use_table(rls_db, &rlsubs_table) < 0)  
		{
			LM_ERR("child %d: Error in use_table rlsubs_table\n", rank);
			return -1;
		}

		LM_DBG("child %d: Database connection opened successfully\n", rank);
	}

	if (rlpres_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	/* In DB only mode do not pool the connections where possible. */
	if (dbmode == RLS_DB_ONLY && rlpres_dbf.init2)
		rlpres_db = rlpres_dbf.init2(&db_url, DB_POOLING_NONE);
	else
		rlpres_db = rlpres_dbf.init(&db_url);
	if (!rlpres_db)
	{
		LM_ERR("child %d: Error while connecting database\n",
				rank);
		return -1;
	}
	else
	{
		if (rlpres_dbf.use_table(rlpres_db, &rlpres_table) < 0)  
		{
			LM_ERR("child %d: Error in use_table rlpres_table\n", rank);
			return -1;
		}

		LM_DBG("child %d: Database connection opened successfully\n", rank);
	}

	if (rls_xcap_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	rls_xcap_db = rls_xcap_dbf.init(&xcap_db_url);
	if (!rls_xcap_db)
	{
		LM_ERR("child %d: Error while connecting database\n", rank);
		return -1;
	}
	else
	{
		if (rls_xcap_dbf.use_table(rls_xcap_db, &rls_xcap_table) < 0)  
		{
			LM_ERR("child %d: Error in use_table rls_xcap_table\n", rank);
			return -1;
		}

		LM_DBG("child %d: Database connection opened successfully\n", rank);
	}

	return 0;
}

static int mi_child_init(void)
{
	if (rls_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	/* In DB only mode do not pool the connections where possible. */
	if (dbmode == RLS_DB_ONLY && rls_dbf.init2)
		rls_db = rls_dbf.init2(&db_url, DB_POOLING_NONE);
	else
		rls_db = rls_dbf.init(&db_url);
	if (!rls_db)
	{
		LM_ERR("Error while connecting database\n");
		return -1;
	}
	else
	{
		if (rls_dbf.use_table(rls_db, &rlsubs_table) < 0)  
		{
			LM_ERR("Error in use_table rlsubs_table\n");
			return -1;
		}

		LM_DBG("Database connection opened successfully\n");
	}

	if (rlpres_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	/* In DB only mode do not pool the connections where possible. */
	if (dbmode == RLS_DB_ONLY && rlpres_dbf.init2)
		rlpres_db = rlpres_dbf.init2(&db_url, DB_POOLING_NONE);
	else
		rlpres_db = rlpres_dbf.init(&db_url);
	if (!rlpres_db)
	{
		LM_ERR("Error while connecting database\n");
		return -1;
	}
	else
	{
		if (rlpres_dbf.use_table(rlpres_db, &rlpres_table) < 0)  
		{
			LM_ERR("Error in use_table rlpres_table\n");
			return -1;
		}

		LM_DBG("Database connection opened successfully\n");
	}

	if (rls_xcap_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	rls_xcap_db = rls_xcap_dbf.init(&xcap_db_url);
	if (!rls_xcap_db)
	{
		LM_ERR("Error while connecting database\n");
		return -1;
	}
	else
	{
		if (rls_xcap_dbf.use_table(rls_xcap_db, &rls_xcap_table) < 0)  
		{
			LM_ERR("Error in use_table rls_xcap_table\n");
			return -1;
		}

		LM_DBG("Database connection opened successfully\n");
	}

	return 0;

}

/*
 * destroy function
 */
static void destroy(void)
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
	if(rlpres_db && rlpres_dbf.close)
		rlpres_dbf.close(rlpres_db);
	if(rls_xcap_db && rls_xcap_dbf.close)
		rls_xcap_dbf.close(rls_xcap_db);

	if (rls_update_subs_lock != NULL)
	{
		lock_destroy(rls_update_subs_lock);
		lock_dealloc(rls_update_subs_lock);
	}

	if (rls_notifier_id != NULL)
		shm_free(rls_notifier_id);
}

int handle_expired_record(subs_t* s)
{
	int ret;
	int tmp;
	/* send NOTIFY with state terminated - make sure exires value is 0 */
	tmp = s->expires;
	s->expires = 0;
	ret = rls_send_notify(s, NULL, NULL, NULL);
	s->expires = tmp;
	if(ret <0)
	{
		LM_ERR("in function send_notify\n");
		return -1;
	}
	
	return 0;
}

void rlsubs_table_update(unsigned int ticks,void *param)
{
	int no_lock= 0;

	if (dbmode==RLS_DB_ONLY) { delete_expired_subs_rlsdb(); return; }

	if(ticks== 0 && param == NULL)
		no_lock= 1;
	
	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("sql use table failed\n");
		return;
	}
	pres_update_db_subs_timer(rls_db, rls_dbf, rls_table, hash_size, 
			no_lock, handle_expired_record);

}

int rls_restore_db_subs(void)
{
	db_key_t result_cols[24]; 
	db1_res_t *res= NULL;
	db_row_t *row = NULL;	
	db_val_t *row_vals= NULL;
	int i;
	int n_result_cols= 0;
	int pres_uri_col, expires_col, from_user_col, from_domain_col,to_user_col; 
	int callid_col,totag_col,fromtag_col,to_domain_col,sockinfo_col,reason_col;
	int event_col,contact_col,record_route_col, event_id_col, status_col;
	int remote_cseq_col, local_cseq_col, local_contact_col, version_col;
	int watcher_user_col, watcher_domain_col;
	subs_t s;
	str ev_sname;
	pres_ev_t* event= NULL;
	event_t parsed_event;
	unsigned int expires;
	unsigned int hash_code;

	result_cols[pres_uri_col=n_result_cols++] = &str_presentity_uri_col;
	result_cols[expires_col=n_result_cols++] = &str_expires_col;
	result_cols[event_col=n_result_cols++] = &str_event_col;
	result_cols[event_id_col=n_result_cols++] = &str_event_id_col;
	result_cols[to_user_col=n_result_cols++] = &str_to_user_col;
	result_cols[to_domain_col=n_result_cols++] = &str_to_domain_col;
	result_cols[watcher_user_col=n_result_cols++] = &str_watcher_username_col;
	result_cols[watcher_domain_col=n_result_cols++] = &str_watcher_domain_col;
	result_cols[from_user_col=n_result_cols++] = &str_from_user_col;
	result_cols[from_domain_col=n_result_cols++] = &str_from_domain_col;
	result_cols[callid_col=n_result_cols++] = &str_callid_col;
	result_cols[totag_col=n_result_cols++] = &str_to_tag_col;
	result_cols[fromtag_col=n_result_cols++] = &str_from_tag_col;
	result_cols[local_cseq_col= n_result_cols++] = &str_local_cseq_col;
	result_cols[remote_cseq_col= n_result_cols++] = &str_remote_cseq_col;
	result_cols[record_route_col= n_result_cols++] = &str_record_route_col;
	result_cols[sockinfo_col= n_result_cols++] = &str_socket_info_col;
	result_cols[contact_col= n_result_cols++] = &str_contact_col;
	result_cols[local_contact_col= n_result_cols++] = &str_local_contact_col;
	result_cols[version_col= n_result_cols++] = &str_version_col;
	result_cols[status_col= n_result_cols++] = &str_status_col;
	result_cols[reason_col= n_result_cols++] = &str_reason_col;
	
	if(!rls_db)
	{
		LM_ERR("null database connection\n");
		return -1;
	}
	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("in use table\n");
		return -1;
	}

	if(db_fetch_query(&rls_dbf, rls_fetch_rows, rls_db, 0, 0, 0,
				result_cols,0, n_result_cols, 0, &res)< 0)
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

	do {
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

			s.watcher_user.s=(char*)row_vals[watcher_user_col].val.string_val;
			s.watcher_user.len= strlen(s.watcher_user.s);
		
			s.watcher_domain.s=(char*)row_vals[watcher_domain_col].val.string_val;
			s.watcher_domain.len= strlen(s.watcher_domain.s);


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
	} while((db_fetch_next(&rls_dbf, rls_fetch_rows, rls_db, &res)==1)
			&& (RES_ROW_N(res)>0));

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
	if(e.type & EVENT_OTHER)
	{
		LM_ERR("wrong event= %s\n", event);
		return -1;
	}

	rls_events|= e.type;

	return 0;

}

int bind_rls(struct rls_binds *pxb)
{
		if (pxb == NULL)
		{
				LM_WARN("bind_rls: Cannot load rls API into a NULL pointer\n");
				return -1;
		}

		pxb->rls_handle_subscribe = rls_handle_subscribe;
		pxb->rls_handle_subscribe0 = rls_handle_subscribe0;
		pxb->rls_handle_notify = rls_handle_notify;
		return 0;
}

static struct mi_root* mi_cleanup(struct mi_root* cmd, void *param)
{
	LM_DBG("mi_cleanup:start\n");

	(void)rlsubs_table_update(0,0);
	(void)rls_presentity_clean(0,0);

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}
