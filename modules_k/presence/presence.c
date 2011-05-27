/*
 * $Id$
 *
 * presence module - presence server implementation
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-08-15  initial version (anca)
 */

/*! \defgroup presence Presence :: A generic implementation of the SIP event package (PUBLISH, SUBSCRIBE, NOTIFY)
 *
 *	   The Kamailio presence module is a generic module for SIP event packages, which is much more than presence.
 *	   It is extensible by developing other modules that use the internal developer API.
 *	   Examples:
 *	   - \ref presence_mwi
 *	   - \ref presence_xml
 */

/*! \file
 * \brief Kamailio presence module
 * 
 * \ingroup presence 
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../lib/srdb1/db.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h" 
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../usr_avp.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../pt.h"
#include "../../lib/kmi/mi.h"
#include "../../lib/kcore/hash_func.h"
#include "../pua/hash.h"
#include "../dmq/dmq.h"
#include "presence.h"
#include "publish.h"
#include "subscribe.h"
#include "event_list.h"
#include "bind_presence.h"
#include "notify.h"
#include "../../mod_fix.h"

MODULE_VERSION

#define S_TABLE_VERSION  3
#define P_TABLE_VERSION  3
#define ACTWATCH_TABLE_VERSION 9

char *log_buf = NULL;
static int clean_period=100;
static int db_update_period=100;

/* database connection */
db1_con_t *pa_db = NULL;
db_func_t pa_dbf;
str presentity_table= str_init("presentity");
str active_watchers_table = str_init("active_watchers");
str watchers_table= str_init("watchers");

int library_mode= 0;
str server_address= {0, 0};
evlist_t* EvList= NULL;

/* to tag prefix */
char* to_tag_pref = "10";

/* TM bind */
struct tm_binds tmb;
/* SL API structure */
sl_api_t slb;
/* dmq API structure */
dmq_api_t dmq;
register_dmq_peer_t register_dmq;

/** module functions */

static int mod_init(void);
static int child_init(int);
static void destroy(void);
int stored_pres_info(struct sip_msg* msg, char* pres_uri, char* s);
static int fixup_presence(void** param, int param_no);
static int fixup_subscribe(void** param, int param_no);
static struct mi_root* mi_refreshWatchers(struct mi_root* cmd, void* param);
static struct mi_root* mi_cleanup(struct mi_root* cmd, void* param);
static int update_pw_dialogs(subs_t* subs, unsigned int hash_code,
		subs_t** subs_array);
int update_watchers_status(str pres_uri, pres_ev_t* ev, str* rules_doc);
static int mi_child_init(void);
static int pres_auth_status(struct sip_msg* _msg, char* _sp1, char* _sp2);
static int w_pres_refresh_watchers(struct sip_msg *msg, char *puri,
		char *pevent, char *ptype);
static int w_pres_update_watchers(struct sip_msg *msg, char *puri,
		char *pevent);
static int fixup_refresh_watchers(void** param, int param_no);
static int fixup_update_watchers(void** param, int param_no);

int counter =0;
int pid = 0;
char prefix='a';
int startup_time=0;
str db_url = {0, 0};
int expires_offset = 0;
int max_expires= 3600;
int shtable_size= 9;
shtable_t subs_htable= NULL;
int dbmode = 0;
int fallback2db = 0;
int sphere_enable= 0;
int timeout_rm_subs = 1;

int phtable_size= 9;
phtable_t* pres_htable;

static cmd_export_t cmds[]=
{
	{"handle_publish",        (cmd_function)handle_publish,          0,
		fixup_presence,0, REQUEST_ROUTE},
	{"handle_publish",        (cmd_function)handle_publish,          1,
		fixup_presence, 0, REQUEST_ROUTE},
	{"handle_subscribe",      (cmd_function)handle_subscribe,        0,
		fixup_subscribe,0, REQUEST_ROUTE},
	{"pres_auth_status",      (cmd_function)pres_auth_status,        2,
		fixup_pvar_pvar, fixup_free_pvar_pvar, REQUEST_ROUTE},
	{"pres_refresh_watchers", (cmd_function)w_pres_refresh_watchers, 3,
		fixup_refresh_watchers, 0, ANY_ROUTE},
	{"pres_update_watchers",  (cmd_function)w_pres_update_watchers,  2,
		fixup_update_watchers, 0, ANY_ROUTE},
	{"bind_presence",         (cmd_function)bind_presence,           1,
		0, 0, 0},
	{ 0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{ "db_url",                 STR_PARAM, &db_url.s},
	{ "presentity_table",       STR_PARAM, &presentity_table.s},
	{ "active_watchers_table",  STR_PARAM, &active_watchers_table.s},
	{ "watchers_table",         STR_PARAM, &watchers_table.s},
	{ "clean_period",           INT_PARAM, &clean_period },
	{ "db_update_period",       INT_PARAM, &db_update_period },
	{ "to_tag_pref",            STR_PARAM, &to_tag_pref },
	{ "expires_offset",         INT_PARAM, &expires_offset },
	{ "max_expires",            INT_PARAM, &max_expires },
	{ "server_address",         STR_PARAM, &server_address.s},
	{ "subs_htable_size",       INT_PARAM, &shtable_size},
	{ "pres_htable_size",       INT_PARAM, &phtable_size},
	{ "dbmode",                 INT_PARAM, &dbmode},
	{ "fallback2db",            INT_PARAM, &fallback2db},
	{ "enable_sphere_check",    INT_PARAM, &sphere_enable},
	{ "timeout_rm_subs",        INT_PARAM, &timeout_rm_subs},
    {0,0,0}
};

static mi_export_t mi_cmds[] = {
	{ "refreshWatchers", mi_refreshWatchers,    0,  0,  mi_child_init},
	{ "cleanup",         mi_cleanup,            0,  0,  mi_child_init},
	{  0,                0,                     0,  0,  0}
};

/** module exports */
struct module_exports exports= {
	"presence",			/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* exported statistics */
	mi_cmds,   			/* exported MI functions */
	0,					/* exported pseudo-variables */
	0,					/* extra processes */
	mod_init,			/* module initialization function */
	0,   				/* response handling function */
	(destroy_function) destroy, 	/* destroy function */
	child_init                  	/* per-child init function */
};

int dmq_presence_callback(struct sip_msg* msg, peer_reponse_t* resp) {
	LM_ERR("it worked - dmq module triggered the presence callback [%ld %d]\n", time(0), my_pid());
	if(update_presentity(msg, 0, (str*)msg->body, 0, 0, 0) <0)
	{
		LM_ERR("when updating presentity\n");
		return -1;
	}
	str ct = str_init("text/xml");
	str reason = str_init("200 OK");
	resp->content_type = ct;
	resp->reason = reason;
	resp->body.s = 0;
	resp->resp_code = 200;
	return 0;
}

static void add_dmq_peer() {
	dmq_peer_t presence_peer;
	presence_peer.peer_id.s = "presence";
	presence_peer.peer_id.len = 8;
	presence_peer.description.s = "presence";
	presence_peer.description.len = 8;
	presence_peer.callback = dmq_presence_callback;
	register_dmq(&presence_peer);
}

/**
 * init module function
 */
static int mod_init(void)
{
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len,db_url.s);
	presentity_table.len = strlen(presentity_table.s);
	active_watchers_table.len = strlen(active_watchers_table.s);
	watchers_table.len = strlen(watchers_table.s);

	if(db_url.s== NULL)
		library_mode= 1;

	if(library_mode== 1)
	{
		LM_DBG("Presence module used for API library purpose only\n");
		EvList= init_evlist();
		if(!EvList)
		{
			LM_ERR("unsuccessful initialize event list\n");
			return -1;
		}
		return 0;
	}

	if(expires_offset<0)
		expires_offset = 0;
	
	if(to_tag_pref==NULL || strlen(to_tag_pref)==0)
		to_tag_pref="10";

	if(max_expires<= 0)
		max_expires = 3600;

	if(server_address.s== NULL)
		LM_DBG("server_address parameter not set in configuration file\n");
	
	if(server_address.s)
		server_address.len= strlen(server_address.s);
	else
		server_address.len= 0;

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* load all TM stuff */
	if(load_tm_api(&tmb)==-1)
	{
		LM_ERR("Can't load tm functions. Module TM not loaded?\n");
		return -1;
	}
	
	if(dmq_load_api(&dmq) < 0) {
		LM_ERR("cannot load dmq api\n");
		return -1;
	} else {
		register_dmq = dmq.register_dmq_peer;
		add_dmq_peer();
		LM_DBG("presence-dmq loaded\n");
	}
	
	if(db_url.s== NULL)
	{
		LM_ERR("database url not set!\n");
		return -1;
	}

	/* binding to database module  */
	if (db_bind_mod(&db_url, &pa_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}
	

	if (!DB_CAPABILITY(pa_dbf, DB_CAP_ALL))
	{
		LM_ERR("Database module does not implement all functions"
				" needed by presence module\n");
		return -1;
	}

	pa_db = pa_dbf.init(&db_url);
	if (!pa_db)
	{
		LM_ERR("Connection to database failed\n");
		return -1;
	}
	
	/*verify table versions */
	if((db_check_table_version(&pa_dbf, pa_db, &presentity_table, P_TABLE_VERSION) < 0) ||
		(db_check_table_version(&pa_dbf, pa_db, &active_watchers_table, ACTWATCH_TABLE_VERSION) < 0) ||
		(db_check_table_version(&pa_dbf, pa_db, &watchers_table, S_TABLE_VERSION) < 0)) {
			LM_ERR("error during table version check\n");
			return -1;
	}

	EvList= init_evlist();
	if(!EvList)
	{
		LM_ERR("initializing event list\n");
		return -1;
	}

	if(shtable_size< 1)
		shtable_size= 512;
	else
		shtable_size= 1<< shtable_size;

	subs_htable= new_shtable(shtable_size);
	if(subs_htable== NULL)
	{
		LM_ERR(" initializing subscribe hash table\n");
		return -1;
	}

	if(dbmode != DB_ONLY)
	{
		if(restore_db_subs()< 0)
		{
			LM_ERR("restoring subscribe info from database\n");
			return -1;
		}
	}

	if(phtable_size< 1)
		phtable_size= 256;
	else
		phtable_size= 1<< phtable_size;

	pres_htable= new_phtable();
	if(pres_htable== NULL)
	{
		LM_ERR("initializing presentity hash table\n");
		return -1;
	}

	if(pres_htable_restore()< 0)
	{
		LM_ERR("filling in presentity hash table from database\n");
		return -1;
	}

	startup_time = (int) time(NULL);
	
	if(clean_period>0)
	{
		register_timer(msg_presentity_clean, 0, clean_period);
		register_timer(msg_watchers_clean, 0, clean_period);
	}
	
	if(db_update_period>0)
		register_timer(timer_db_update, 0, db_update_period);

	if(pa_db)
		pa_dbf.close(pa_db);
	pa_db = NULL;
	
	/* for legacy, we also keep the fallback2db parameter, but make sure for consistency */
	if(fallback2db)
	{
		dbmode = DB_FALLBACK;
	}

	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	pid = my_pid();
	
	if(library_mode)
		return 0;

	if (pa_dbf.init==0)
	{
		LM_CRIT("child_init: database not bound\n");
		return -1;
	}
	if (pa_db)
		return 0;
	pa_db = pa_dbf.init(&db_url);
	if (!pa_db)
	{
		LM_ERR("child %d: unsuccessful connecting to database\n", rank);
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, &presentity_table) < 0)  
	{
		LM_ERR( "child %d:unsuccessful use_table presentity_table\n", rank);
		return -1;
	}

	if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0)  
	{
		LM_ERR( "child %d:unsuccessful use_table active_watchers_table\n",
				rank);
		return -1;
	}

	if (pa_dbf.use_table(pa_db, &watchers_table) < 0)  
	{
		LM_ERR( "child %d:unsuccessful use_table watchers_table\n", rank);
		return -1;
	}

	LM_DBG("child %d: Database connection opened successfully\n", rank);
	
	return 0;
}

static int mi_child_init(void)
{
	if(library_mode)
		return 0;

	if (pa_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	if (pa_db)
		return 0;
	pa_db = pa_dbf.init(&db_url);
	if (!pa_db)
	{
		LM_ERR("connecting database\n");
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, &presentity_table) < 0)
	{
		LM_ERR( "unsuccessful use_table presentity_table\n");
		return -1;
	}

	if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0)
	{
		LM_ERR( "unsuccessful use_table active_watchers_table\n");
		return -1;
	}

	if (pa_dbf.use_table(pa_db, &watchers_table) < 0)
	{
		LM_ERR( "unsuccessful use_table watchers_table\n");
		return -1;
	}

	LM_DBG("Database connection opened successfully\n");
	return 0;
}


/*
 * destroy function
 */
static void destroy(void)
{
	if(subs_htable && pa_db)
		timer_db_update(0, 0);

	if(subs_htable)
		destroy_shtable(subs_htable, shtable_size);
	
	if(pres_htable)
		destroy_phtable();

	if(pa_db && pa_dbf.close)
		pa_dbf.close(pa_db);
	
	destroy_evlist();
}

static int fixup_presence(void** param, int param_no)
{
 	pv_elem_t *model;
	str s;

	if(library_mode)
	{
		LM_ERR("Bad config - you can not call 'handle_publish' function"
				" (db_url not set)\n");
		return -1;
	}
	if(param_no== 0)
		return 0;

	if(*param)
 	{
		s.s = (char*)(*param); s.len = strlen(s.s);
 		if(pv_parse_format(&s, &model)<0)
 		{
 			LM_ERR( "wrong format[%s]\n",(char*)(*param));
 			return E_UNSPEC;
 		}
 
 		*param = (void*)model;
 		return 0;
 	}
 	LM_ERR( "null format\n");
 	return E_UNSPEC;
}

static int fixup_subscribe(void** param, int param_no)
{

	if(library_mode)
	{
		LM_ERR("Bad config - you can not call 'handle_subscribe' function"
				" (db_url not set)\n");
		return -1;
	}
	return 0;
}

int pres_refresh_watchers(str *pres, str *event, int type)
{
	pres_ev_t *ev;
	struct sip_uri uri;
	str *rules_doc= NULL;
	int result;

	ev= contains_event(event, NULL);
	if(ev==NULL)
	{
		LM_ERR("wrong event parameter\n");
		return -1;
	}

	if(type==0)
	{
		/* if a request to refresh watchers authorization */
		if(ev->get_rules_doc==NULL)
		{
			LM_ERR("wrong request for a refresh watchers authorization status"
					"for an event that does not require authorization\n");
			goto error;
		}

		if(parse_uri(pres->s, pres->len, &uri)<0)
		{
			LM_ERR("parsing uri [%.*s]\n", pres->len, pres->s);
			goto error;
		}

		result= ev->get_rules_doc(&uri.user, &uri.host, &rules_doc);
		if(result<0 || rules_doc==NULL || rules_doc->s==NULL)
		{
			LM_ERR("no rules doc found for the user\n");
			goto error;
		}

		if(update_watchers_status(*pres, ev, rules_doc)<0)
		{
			LM_ERR("failed to update watchers\n");
			goto error;
		}

		pkg_free(rules_doc->s);
		pkg_free(rules_doc);
		rules_doc = NULL;

	} else {
		/* if a request to refresh notified info */
		if(query_db_notify(pres, ev, NULL)< 0)
		{
			LM_ERR("sending Notify requests\n");
			goto error;
		}

	}
	return 0;

error:
	if(rules_doc)
	{
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	return -1;
}

/*! \brief
 *  mi cmd: refreshWatchers
 *			\<presentity_uri> 
 *			\<event>
 *          \<refresh_type> // can be:  = 0 -> watchers autentification type or
 *									  != 0 -> publish type //		   
 *		* */

static struct mi_root* mi_refreshWatchers(struct mi_root* cmd, void* param)
{
	struct mi_node* node= NULL;
	str pres_uri, event;
	unsigned int refresh_type;

	LM_DBG("start\n");
	
	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	/* Get presentity URI */
	pres_uri = node->value;
	if(pres_uri.s == NULL || pres_uri.len== 0)
	{
		LM_ERR( "empty uri\n");
		return init_mi_tree(404, "Empty presentity URI", 20);
	}
	
	node = node->next;
	if(node == NULL)
		return 0;
	event= node->value;
	if(event.s== NULL || event.len== 0)
	{
		LM_ERR( "empty event parameter\n");
		return init_mi_tree(400, "Empty event parameter", 21);
	}
	LM_DBG("event '%.*s'\n",  event.len, event.s);
	
	node = node->next;
	if(node == NULL)
		return 0;
	if(node->value.s== NULL || node->value.len== 0)
	{
		LM_ERR( "empty refresh type parameter\n");
		return init_mi_tree(400, "Empty refresh type parameter", 28);
	}
	if(str2int(&node->value, &refresh_type)< 0)
	{
		LM_ERR("converting string to int\n");
		goto error;
	}

	if(node->next!= NULL)
	{
		LM_ERR( "Too many parameters\n");
		return init_mi_tree(400, "Too many parameters", 19);
	}

	if(pres_refresh_watchers(&pres_uri, &event, refresh_type)<0)
		return 0;
	
	return init_mi_tree(200, "OK", 2);

error:
	return 0;
}

/* 
 *  mi cmd: cleanup
 *		* */

static struct mi_root* mi_cleanup(struct mi_root* cmd, void* param)
{
	LM_DBG("mi_cleanup:start\n");
	
	(void)msg_watchers_clean(0,0);
	(void)msg_presentity_clean(0,0);
		
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

int pres_update_status(subs_t subs, str reason, db_key_t* query_cols,
        db_val_t* query_vals, int n_query_cols, subs_t** subs_array)
{
	db_key_t update_cols[5];
	db_val_t update_vals[5];
	int n_update_cols= 0;
	int u_status_col, u_reason_col, q_wuser_col, q_wdomain_col;
	int status;
	query_cols[q_wuser_col=n_query_cols]= &str_watcher_username_col;
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB1_STR;
	n_query_cols++;

	query_cols[q_wdomain_col=n_query_cols]= &str_watcher_domain_col;
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB1_STR;
	n_query_cols++;

	update_cols[u_status_col= n_update_cols]= &str_status_col;
	update_vals[u_status_col].nul= 0;
	update_vals[u_status_col].type= DB1_INT;
	n_update_cols++;

	update_cols[u_reason_col= n_update_cols]= &str_reason_col;
	update_vals[u_reason_col].nul= 0;
	update_vals[u_reason_col].type= DB1_STR;
	n_update_cols++;

	status= subs.status;
	if(subs.event->get_auth_status(&subs)< 0)
	{
		LM_ERR( "getting status from rules document\n");
		return -1;
	}
	LM_DBG("subs.status= %d\n", subs.status);
	if(get_status_str(subs.status)== NULL)
	{
		LM_ERR("wrong status: %d\n", subs.status);
		return -1;
	}

	if(subs.status!= status || reason.len!= subs.reason.len ||
		(reason.s && subs.reason.s && strncmp(reason.s, subs.reason.s,
											  reason.len)))
	{
		/* update in watchers_table */
		query_vals[q_wuser_col].val.str_val= subs.from_user; 
		query_vals[q_wdomain_col].val.str_val= subs.from_domain; 

		update_vals[u_status_col].val.int_val= subs.status;
		update_vals[u_reason_col].val.str_val= subs.reason;
		
		if (pa_dbf.use_table(pa_db, &watchers_table) < 0) 
		{
			LM_ERR( "in use_table\n");
			return -1;
		}

		if(pa_dbf.update(pa_db, query_cols, 0, query_vals, update_cols,
					update_vals, n_query_cols, n_update_cols)< 0)
		{
			LM_ERR( "in sql update\n");
			return -1;
		}
		/* save in the list all affected dialogs */
		/* if status switches to terminated -> delete dialog */
		if(update_pw_dialogs(&subs, subs.db_flag, subs_array)< 0)
		{
			LM_ERR( "extracting dialogs from [watcher]=%.*s@%.*s to"
				" [presentity]=%.*s\n",	subs.from_user.len, subs.from_user.s,
				subs.from_domain.len, subs.from_domain.s, subs.pres_uri.len,
				subs.pres_uri.s);
			return -1;
		}
	}
    return 0;
}

int pres_db_delete_status(subs_t* s)
{
    int n_query_cols= 0;
    db_key_t query_cols[5];
    db_val_t query_vals[5];

    if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0) 
    {
        LM_ERR("sql use table failed\n");
        return -1;
    }

    query_cols[n_query_cols]= &str_event_col;
    query_vals[n_query_cols].nul= 0;
    query_vals[n_query_cols].type= DB1_STR;
    query_vals[n_query_cols].val.str_val= s->event->name ;
    n_query_cols++;

    query_cols[n_query_cols]= &str_presentity_uri_col;
    query_vals[n_query_cols].nul= 0;
    query_vals[n_query_cols].type= DB1_STR;
    query_vals[n_query_cols].val.str_val= s->pres_uri;
    n_query_cols++;

    query_cols[n_query_cols]= &str_watcher_username_col;
    query_vals[n_query_cols].nul= 0;
    query_vals[n_query_cols].type= DB1_STR;
    query_vals[n_query_cols].val.str_val= s->from_user;
    n_query_cols++;

    query_cols[n_query_cols]= &str_watcher_domain_col;
    query_vals[n_query_cols].nul= 0;
    query_vals[n_query_cols].type= DB1_STR;
    query_vals[n_query_cols].val.str_val= s->from_domain;
    n_query_cols++;

    if(pa_dbf.delete(pa_db, query_cols, 0, query_vals, n_query_cols)< 0)
    {
        LM_ERR("sql delete failed\n");
        return -1;
    }
    return 0;

}

int update_watchers_status(str pres_uri, pres_ev_t* ev, str* rules_doc)
{
	subs_t subs;
	db_key_t query_cols[6], result_cols[5];
	db_val_t query_vals[6];
	int n_result_cols= 0, n_query_cols = 0;
	db1_res_t* result= NULL;
	db_row_t *row;
	db_val_t *row_vals ;
	int i;
	str w_user, w_domain, reason= {0, 0};
	unsigned int status;
	int status_col, w_user_col, w_domain_col, reason_col;
	subs_t* subs_array= NULL,* s;
	unsigned int hash_code;
	int err_ret= -1;
	int n= 0;

	typedef struct ws
	{
		int status;
		str reason;
		str w_user;
		str w_domain;
	}ws_t;
	ws_t* ws_list= NULL;

    LM_DBG("start\n");

	if(ev->content_type.s== NULL)
	{
		ev= contains_event(&ev->name, NULL);
		if(ev== NULL)
		{
			LM_ERR("wrong event parameter\n");
			return 0;
		}
	}

	subs.pres_uri= pres_uri;
	subs.event= ev;
	subs.auth_rules_doc= rules_doc;

	/* update in watchers_table */
	query_cols[n_query_cols]= &str_presentity_uri_col;
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB1_STR;
	query_vals[n_query_cols].val.str_val= pres_uri;
	n_query_cols++;

	query_cols[n_query_cols]= &str_event_col;
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB1_STR;
	query_vals[n_query_cols].val.str_val= ev->name;
	n_query_cols++;

	result_cols[status_col= n_result_cols++]= &str_status_col;
	result_cols[reason_col= n_result_cols++]= &str_reason_col;
	result_cols[w_user_col= n_result_cols++]= &str_watcher_username_col;
	result_cols[w_domain_col= n_result_cols++]= &str_watcher_domain_col;

	if (pa_dbf.use_table(pa_db, &watchers_table) < 0) 
	{
		LM_ERR( "in use_table\n");
		goto done;
	}

	if(pa_dbf.query(pa_db, query_cols, 0, query_vals, result_cols,n_query_cols,
				n_result_cols, 0, &result)< 0)
	{
		LM_ERR( "in sql query\n");
		goto done;
	}
	if(result== NULL)
		return 0;

	if(result->n<= 0)
	{
		err_ret= 0;
		goto done;
	}

    LM_DBG("found %d record-uri in watchers_table\n", result->n);
	hash_code= core_hash(&pres_uri, &ev->name, shtable_size);
	subs.db_flag= hash_code;

    /*must do a copy as sphere_check requires database queries */
	if(sphere_enable)
	{
        n= result->n;
		ws_list= (ws_t*)pkg_malloc(n * sizeof(ws_t));
		if(ws_list== NULL)
		{
			LM_ERR("No more private memory\n");
			goto done;
		}
		memset(ws_list, 0, n * sizeof(ws_t));

		for(i= 0; i< result->n ; i++)
		{
			row= &result->rows[i];
			row_vals = ROW_VALUES(row);

			status= row_vals[status_col].val.int_val;
	
			reason.s= (char*)row_vals[reason_col].val.string_val;
			reason.len= reason.s?strlen(reason.s):0;

			w_user.s= (char*)row_vals[w_user_col].val.string_val;
			w_user.len= strlen(w_user.s);

			w_domain.s= (char*)row_vals[w_domain_col].val.string_val;
			w_domain.len= strlen(w_domain.s);

			if(reason.len)
			{
				ws_list[i].reason.s = (char*)pkg_malloc(reason.len* sizeof(char));
				if(ws_list[i].reason.s== NULL)
				{  
					LM_ERR("No more private memory\n");
					goto done;
				}
				memcpy(ws_list[i].reason.s, reason.s, reason.len);
				ws_list[i].reason.len= reason.len;
			}
			else
				ws_list[i].reason.s= NULL;
            
			ws_list[i].w_user.s = (char*)pkg_malloc(w_user.len* sizeof(char));
			if(ws_list[i].w_user.s== NULL)
			{
				LM_ERR("No more private memory\n");
				goto done;

			}
			memcpy(ws_list[i].w_user.s, w_user.s, w_user.len);
			ws_list[i].w_user.len= w_user.len;
		
			 ws_list[i].w_domain.s = (char*)pkg_malloc(w_domain.len* sizeof(char));
			if(ws_list[i].w_domain.s== NULL)
			{
				LM_ERR("No more private memory\n");
				goto done;
			}
			memcpy(ws_list[i].w_domain.s, w_domain.s, w_domain.len);
			ws_list[i].w_domain.len= w_domain.len;
			
			ws_list[i].status= status;
		}

		pa_dbf.free_result(pa_db, result);
		result= NULL;

		for(i=0; i< n; i++)
		{
			subs.from_user = ws_list[i].w_user;
			subs.from_domain = ws_list[i].w_domain;
			subs.status = ws_list[i].status;
			memset(&subs.reason, 0, sizeof(str));

			if( pres_update_status(subs, reason, query_cols, query_vals,
					n_query_cols, &subs_array)< 0)
			{
				LM_ERR("failed to update watcher status\n");
				goto done;
			}

		}
        
		for(i=0; i< n; i++)
		{
			pkg_free(ws_list[i].w_user.s);
			pkg_free(ws_list[i].w_domain.s);
			if(ws_list[i].reason.s)
				pkg_free(ws_list[i].reason.s);
		}
		ws_list= NULL;

		goto send_notify;

	}
	
	for(i = 0; i< result->n; i++)
	{
		row= &result->rows[i];
		row_vals = ROW_VALUES(row);

		status= row_vals[status_col].val.int_val;
	
		reason.s= (char*)row_vals[reason_col].val.string_val;
		reason.len= reason.s?strlen(reason.s):0;

		w_user.s= (char*)row_vals[w_user_col].val.string_val;
		w_user.len= strlen(w_user.s);

		w_domain.s= (char*)row_vals[w_domain_col].val.string_val;
		w_domain.len= strlen(w_domain.s);

		subs.from_user= w_user;
		subs.from_domain= w_domain;
		subs.status= status;
		memset(&subs.reason, 0, sizeof(str));

 		if( pres_update_status(subs,reason, query_cols, query_vals,
					n_query_cols, &subs_array)< 0)
		{
			LM_ERR("failed to update watcher status\n");
			goto done;
		}
    }

	pa_dbf.free_result(pa_db, result);
	result= NULL;

send_notify:

	s= subs_array;

	while(s)
	{

		if(notify(s, NULL, NULL, 0)< 0)
		{
			LM_ERR( "sending Notify request\n");
			goto done;
		}

        /* delete from database also */
        if(s->status== TERMINATED_STATUS)
        {
            if(pres_db_delete_status(s)<0)
            {
                err_ret= -1;
                LM_ERR("failed to delete terminated dialog from database\n");
                goto done;
            }
        }

        s= s->next;
	}

	free_subs_list(subs_array, PKG_MEM_TYPE, 0);
	return 0;

done:
	if(result)
		pa_dbf.free_result(pa_db, result);
	free_subs_list(subs_array, PKG_MEM_TYPE, 0);
	if(ws_list)
	{
		for(i= 0; i< n; i++)
		{
			if(ws_list[i].w_user.s)
				pkg_free(ws_list[i].w_user.s);
			else
				break;
			if(ws_list[i].w_domain.s)
				pkg_free(ws_list[i].w_domain.s);
			if(ws_list[i].reason.s)
				pkg_free(ws_list[i].reason.s);
		}
	}
	return err_ret;
}

static int update_pw_dialogs(subs_t* subs, unsigned int hash_code, subs_t** subs_array)
{
	subs_t* s, *ps, *cs;
	int i= 0;

    LM_DBG("start\n");
	lock_get(&subs_htable[hash_code].lock);
	
    ps= subs_htable[hash_code].entries;
	
	while(ps && ps->next)
	{
		s= ps->next;

		if(s->event== subs->event && s->pres_uri.len== subs->pres_uri.len &&
			s->from_user.len== subs->from_user.len && 
			s->from_domain.len==subs->from_domain.len &&
			strncmp(s->pres_uri.s, subs->pres_uri.s, subs->pres_uri.len)== 0 &&
			strncmp(s->from_user.s, subs->from_user.s, s->from_user.len)== 0 &&
			strncmp(s->from_domain.s,subs->from_domain.s,s->from_domain.len)==0)
		{
			i++;
			s->status= subs->status;
			s->reason= subs->reason;
			s->db_flag= UPDATEDB_FLAG;

			cs= mem_copy_subs(s, PKG_MEM_TYPE);
			if(cs== NULL)
			{
				LM_ERR( "copying subs_t stucture\n");
                lock_release(&subs_htable[hash_code].lock);
                return -1;
			}
			cs->expires-= (int)time(NULL);
			cs->next= (*subs_array);
			(*subs_array)= cs;
			if(subs->status== TERMINATED_STATUS)
			{
				ps->next= s->next;
				shm_free(s->contact.s);
                shm_free(s);
                LM_DBG(" deleted terminated dialog from hash table\n");
            }
			else
				ps= s;

			printf_subs(cs);
		}
		else
			ps= s;
	}
	
    LM_DBG("found %d matching dialogs\n", i);
    lock_release(&subs_htable[hash_code].lock);
	
    return 0;
}

static int pres_auth_status(struct sip_msg* _msg, char* _sp1, char* _sp2)
{
    pv_spec_t *sp;
    pv_value_t pv_val;
    str watcher_uri, presentity_uri, event;
    struct sip_uri uri;
    pres_ev_t* ev;
    str* rules_doc = NULL;
    subs_t subs;
    int res;

    sp = (pv_spec_t *)_sp1;

    if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    watcher_uri = pv_val.rs;
	    if (watcher_uri.len == 0 || watcher_uri.s == NULL) {
		LM_ERR("missing watcher uri\n");
		return -1;
	    }
	} else {
	    LM_ERR("watcher pseudo variable value is not string\n");
	    return -1;
	}
    } else {
	LM_ERR("cannot get watcher pseudo variable value\n");
	return -1;
    }

    sp = (pv_spec_t *)_sp2;

    if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    presentity_uri = pv_val.rs;
	    if (presentity_uri.len == 0 || presentity_uri.s == NULL) {
		LM_DBG("missing presentity uri\n");
		return -1;
	    }
	} else {
	    LM_ERR("presentity pseudo variable value is not string\n");
	    return -1;
	}
    } else {
	LM_ERR("cannot get presentity pseudo variable value\n");
	return -1;
    }

    event.s = "presence";
    event.len = 8;

    ev = contains_event(&event, NULL);
    if (ev == NULL) {
	LM_ERR("event is not registered\n");
	return -1;
    }
    if (ev->get_rules_doc == NULL) {
	LM_DBG("event does not require authorization");
	return ACTIVE_STATUS;
    }
    if (parse_uri(presentity_uri.s, presentity_uri.len, &uri) < 0) {
	LM_ERR("failed to parse presentity uri\n");
	return -1;
    }
    res = ev->get_rules_doc(&uri.user, &uri.host, &rules_doc);
    if ((res < 0) || (rules_doc == NULL) || (rules_doc->s == NULL)) {
	LM_DBG( "no xcap rules doc found for presentity uri\n");
	return PENDING_STATUS;
    }

    if (parse_uri(watcher_uri.s, watcher_uri.len, &uri) < 0) {
	LM_ERR("failed to parse watcher uri\n");
	goto err;
    }

    subs.from_user = uri.user;
    subs.from_domain = uri.host;
    subs.pres_uri = presentity_uri;
    subs.auth_rules_doc = rules_doc;
    if (ev->get_auth_status(&subs) < 0) {
	LM_ERR( "getting status from rules document\n");
	goto err;
    }
    LM_DBG("auth status of watcher <%.*s> on presentity <%.*s> is %d\n",
	   watcher_uri.len, watcher_uri.s, presentity_uri.len, presentity_uri.s,
	   subs.status);
    pkg_free(rules_doc->s);
    pkg_free(rules_doc);
    return subs.status;

 err:
    pkg_free(rules_doc->s);
    pkg_free(rules_doc);
    return -1;
}

/**
 * wrapper for pres_refresh_watchers to use in config
 */
static int w_pres_refresh_watchers(struct sip_msg *msg, char *puri,
		char *pevent, char *ptype)
{
	str pres_uri;
	str event;
	int refresh_type;

	if(fixup_get_svalue(msg, (gparam_p)puri, &pres_uri)!=0)
	{
		LM_ERR("invalid uri parameter");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pevent, &event)!=0)
	{
		LM_ERR("invalid uri parameter");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)ptype, &refresh_type)!=0)
	{
		LM_ERR("no type value\n");
		return -1;
	}

	if(pres_refresh_watchers(&pres_uri, &event, refresh_type)<0)
		return -1;

	return 1;
}

/**
 * fixup for w_pres_refresh_watchers
 */
static int fixup_refresh_watchers(void** param, int param_no)
{
	if(param_no==1)
	{
		return fixup_spve_null(param, 1);
	} else if(param_no==2) {
		return fixup_spve_null(param, 1);
	} else if(param_no==3) {
		return fixup_igp_null(param, 1);
	}
	return 0;
}


/**
 * wrapper for update_watchers_status to use in config
 */
static int w_pres_update_watchers(struct sip_msg *msg, char *puri,
		char *pevent)
{
	str pres_uri;
	str event;
	pres_ev_t* ev;
	struct sip_uri uri;
	str* rules_doc = NULL;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)puri, &pres_uri)!=0)
	{
		LM_ERR("invalid uri parameter");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pevent, &event)!=0)
	{
		LM_ERR("invalid uri parameter");
		return -1;
	}

	ev = contains_event(&event, NULL);
	if(ev==NULL)
	{
		LM_ERR("event %.*s is not registered\n",
				event.len, event.s);
		return -1;
	}
	if(ev->get_rules_doc==NULL)
	{
		LM_DBG("event  %.*s does not provide rules doc API\n",
				event.len, event.s);
		return -1;
	}
	if(parse_uri(pres_uri.s, pres_uri.len, &uri)<0)
	{
		LM_ERR("failed to parse presentity uri [%.*s]\n",
				pres_uri.len, pres_uri.s);
		return -1;
	}
	ret = ev->get_rules_doc(&uri.user, &uri.host, &rules_doc);
	if((ret < 0) || (rules_doc==NULL) || (rules_doc->s==NULL))
	{
		LM_DBG("no xcap rules doc found for presentity uri [%.*s]\n",
				pres_uri.len, pres_uri.s);
		if(rules_doc != NULL)
			pkg_free(rules_doc);
		return -1;
	}
	ret = 1;
	if(update_watchers_status(pres_uri, ev, rules_doc)<0)
	{
		LM_ERR("updating watchers in presence\n");
		ret = -1;
	}

	pkg_free(rules_doc->s);
	pkg_free(rules_doc);

	return ret;
}

/**
 * fixup for w_pres_update_watchers
 */
static int fixup_update_watchers(void** param, int param_no)
{
	if(param_no==1)
	{
		return fixup_spve_null(param, 1);
	} else if(param_no==2) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}
