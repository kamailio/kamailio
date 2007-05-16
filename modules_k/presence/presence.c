/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *  2006-08-15  initial version (anca)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../db/db.h"
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
#include "../tm/tm_load.h"
#include "../sl/sl_api.h"
#include "../../pt.h"
#include "../../mi/mi.h"
#include "publish.h"
#include "subscribe.h"
#include "event_list.h"
#include "bind_presence.h"
#include "notify.h"

MODULE_VERSION

#define S_TABLE_VERSION 1
#define ACTWATCH_TABLE_VERSION 4

char *log_buf = NULL;
static int clean_period=100;

/* database connection */
db_con_t *pa_db = NULL;
db_func_t pa_dbf;
char *presentity_table="presentity";
char *active_watchers_table = "active_watchers";
char *watchers_table= "watchers";  

int use_db=1;
str server_address= {0, 0};
evlist_t* EvList= NULL;

/* to tag prefix */
char* to_tag_pref = "10";

/* the avp storing the To tag for replies */
int reply_tag_avp_id = 25;

/* TM bind */
struct tm_binds tmb;
/* SL bind */
struct sl_binds slb;

/** module functions */

static int mod_init(void);
static int child_init(int);
int handle_publish(struct sip_msg*, char*, char*);
int handle_subscribe(struct sip_msg*, char*, char*);
int stored_pres_info(struct sip_msg* msg, char* pres_uri, char* s);
static int fixup_presence(void** param, int param_no);
struct mi_root* refreshWatchers(struct mi_root* cmd, void* param);

int counter =0;
int pid = 0;
char prefix='a';
int startup_time=0;
str db_url = {0, 0};
int expires_offset = 0;
int default_expires = 3600;
int max_expires = 3600;


void destroy(void);

static cmd_export_t cmds[]=
{
	{"handle_publish",		handle_publish,	     0,	   0,        REQUEST_ROUTE},
	{"handle_publish",		handle_publish,	     1,fixup_presence,REQUEST_ROUTE},
	{"handle_subscribe",	handle_subscribe,	 0,	   0,         REQUEST_ROUTE},
	{"bind_presence",(cmd_function)bind_presence,1,    0,            0         },
	{"add_event",    (cmd_function)add_event,    1,    0,            0         },
	{0,						0,				     0,	   0,            0	       }	 
};

static param_export_t params[]={
	{ "db_url",					STR_PARAM, &db_url.s},
	{ "presentity_table",		STR_PARAM, &presentity_table},
	{ "active_watchers_table", 	STR_PARAM, &active_watchers_table},
	{ "watchers_table",			STR_PARAM, &watchers_table},
	{ "clean_period",			INT_PARAM, &clean_period },
	{ "to_tag_pref",			STR_PARAM, &to_tag_pref },
	{ "totag_avpid",			INT_PARAM, &reply_tag_avp_id },
	{ "expires_offset",			INT_PARAM, &expires_offset },
	{ "max_expires",			INT_PARAM, &max_expires  },
	{ "server_address",         STR_PARAM, &server_address.s},
	{0,0,0}
};

static mi_export_t mi_cmds[] = {
	{ "refreshWatchers", refreshWatchers,    0,  0,  0},
	{ 0, 0, 0, 0}
};

/** module exports */
struct module_exports exports= {
	"presence",					/* module name */
	DEFAULT_DLFLAGS,			/* dlopen flags */
	cmds,						/* exported functions */
	params,						/* exported parameters */
	0,							/* exported statistics */
	mi_cmds,   					/* exported MI functions */
	0,							/* exported pseudo-variables */
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

	DBG("PRESENCE: initializing module ...\n");

	if(expires_offset<0)
		expires_offset = 0;
	
	if(to_tag_pref==NULL || strlen(to_tag_pref)==0)
		to_tag_pref="10";

	if(max_expires<= 0)
		max_expires = 3600;

	if(server_address.s== NULL)
	{
		DBG("PRESENCE:mod_init: server_address parameter not set in"
				" configuration file\n");
	}
	
	if(server_address.s)
		server_address.len= strlen(server_address.s);
	else
		server_address.len= 0;

	/* load SL API */
	if(load_sl_api(&slb)==-1)
	{
		LOG(L_ERR, "PRESENCE:mod_init:ERROR can't load sl functions\n");
		return -1;
	}

	/* load all TM stuff */
	if(load_tm_api(&tmb)==-1)
	{
		LOG(L_ERR, "PRESENCE:mod_init:ERROR can't load tm functions\n");
		return -1;
	}

	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	DBG("presence:mod_init: db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len,
			db_url.s);
	
	/* binding to mysql module  */
	if (bind_dbmod(db_url.s, &pa_dbf))
	{
		DBG("PRESENCE:mod_init: ERROR: Database module not found\n");
		return -1;
	}
	

	if (!DB_CAPABILITY(pa_dbf, DB_CAP_ALL)) {
		LOG(L_ERR,"PRESENCE:mod_init: ERROR Database module does not implement "
		    "all functions needed by the module\n");
		return -1;
	}

	pa_db = pa_dbf.init(db_url.s);
	if (!pa_db)
	{
		LOG(L_ERR,"PRESENCE:mod_init: Error while connecting database\n");
		return -1;
	}
	// verify table version 
	_s.s = presentity_table;
	_s.len = strlen(presentity_table);
	 ver =  table_version(&pa_dbf, pa_db, &_s);
	if(ver!=S_TABLE_VERSION)
	{
		LOG(L_ERR,"PRESENCE:mod_init: Wrong version v%d for table <%s>,"
				" need v%d\n", ver, _s.s, S_TABLE_VERSION);
		return -1;
	}
	
	_s.s = active_watchers_table;
	_s.len = strlen(active_watchers_table);
	 ver =  table_version(&pa_dbf, pa_db, &_s);
	if(ver!=ACTWATCH_TABLE_VERSION)
	{
		LOG(L_ERR,"PRESENCE:mod_init: Wrong version v%d for table <%s>,"
				" need v%d\n", ver, _s.s, ACTWATCH_TABLE_VERSION);
		return -1;
	}

	_s.s = watchers_table;
	_s.len = strlen(watchers_table);
	 ver =  table_version(&pa_dbf, pa_db, &_s);
	if(ver!=S_TABLE_VERSION)
	{
		LOG(L_ERR,"PRESENCE:mod_init: Wrong version v%d for table <%s>,"
				" need v%d\n", ver, _s.s, S_TABLE_VERSION);
		return -1;
	}

	EvList= init_evlist();
	if(!EvList)
	{
		LOG(L_ERR,"PRESENCE:mod_init: ERROR while initializing event list\n");
		if(pa_db)
			pa_dbf.close(pa_db);
		pa_db = NULL;
		return -1;
	}	

	if(clean_period<=0)
	{
		DBG("PRESENCE: ERROR: mod_init: wrong clean_period \n");
		if(pa_db)
			pa_dbf.close(pa_db);
		pa_db = NULL;
		return -1;
	}

	startup_time = (int) time(NULL);
	
	register_timer(msg_presentity_clean, 0, clean_period);
	
	register_timer(msg_active_watchers_clean, 0, clean_period);

	register_timer(msg_watchers_clean, 0, clean_period);

	if(pa_db)
		pa_dbf.close(pa_db);
	pa_db = NULL;
	
	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	DBG("PRESENCE: init_child [%d]  pid [%d]\n", rank, getpid());
	if (pa_dbf.init==0)
	{
		LOG(L_CRIT, "BUG: PRESENCE: child_init: database not bound\n");
		return -1;
	}
	pa_db = pa_dbf.init(db_url.s);
	if (!pa_db)
	{
		LOG(L_ERR,"PRESENCE: child %d: Error while connecting database\n",
				rank);
		return -1;
	}
	else
	{
		if (pa_dbf.use_table(pa_db, presentity_table) < 0)  
		{
			LOG(L_ERR, "PRESENCE: child %d: Error in use_table presentity_table\n", rank);
			return -1;
		}
		if (pa_dbf.use_table(pa_db, active_watchers_table) < 0)  
		{
			LOG(L_ERR, "PRESENCE: child %d: Error in use_table active_watchers_table\n", rank);
			return -1;
		}
		if (pa_dbf.use_table(pa_db, watchers_table) < 0)  
		{
			LOG(L_ERR, "PRESENCE: child %d: Error in use_table watchers_table\n", rank);
			return -1;
		}

		DBG("PRESENCE: child %d: Database connection opened successfully\n", rank);
	}
	pid = my_pid();
	return 0;
}

/*
 * destroy function
 */
void destroy(void)
{
	DBG("PRESENCE: destroy module ...\n");
	
	if(pa_db && pa_dbf.close)
		pa_dbf.close(pa_db);
	destroy_evlist();
}

static int fixup_presence(void** param, int param_no)
{
 	xl_elem_t *model;
 	if(*param)
 	{
 		if(xl_parse_format((char*)(*param), &model, XL_DISABLE_COLORS)<0)
 		{
 			LOG(L_ERR, "PRESENCE:fixup_presence: ERROR wrong format[%s]\n",
 				(char*)(*param));
 			return E_UNSPEC;
 		}
 
 		*param = (void*)model;
 		return 0;
 	}
 	LOG(L_ERR, "PRESENCE:fixup_presence: ERROR null format\n");
 	return E_UNSPEC;
}
/* 
 *  mi cmd: refreshWatchers
 *			<presentity_uri> 
 *			<event>
 * */

struct mi_root* refreshWatchers(struct mi_root* cmd, void* param)
{
	struct mi_node* node= NULL;
	str pres_uri, event;
	ev_t* ev;
	struct sip_uri uri;

	DBG("PRESENCE:refreshWatchers: start\n");
	
	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	/* Get presentity URI */
	pres_uri = node->value;
	if(pres_uri.s == NULL || pres_uri.s== 0)
	{
		LOG(L_ERR, "PRESENCE:refreshWatchers: empty uri\n");
		return init_mi_tree(404, "Empty presentity URI", 20);
	}
	if(parse_uri(pres_uri.s, pres_uri.len, &uri)<0 )
	{
		LOG(L_ERR, "PRESENCE:refreshWatchers: bad uri\n");
		return init_mi_tree(404, "Bad presentity URI", 18);
	}
	DBG("PRESENCE:refreshWatchers: pres_uri '%.*s'\n",
	    pres_uri.len, pres_uri.s);
	
	node = node->next;
	if(node == NULL)
		return 0;
	event= node->value;
	if(event.s== NULL || event.len== 0)
	{
		LOG(L_ERR, "PRESENCE:refreshWatchers: "
		    "empty event parameter\n");
		return init_mi_tree(400, "Empty event parameter", 21);
	}
	DBG("PRESENCE:refreshWatchers: event '%.*s'\n",
	    event.len, event.s);
	
	if(node->next!= NULL)
	{
		LOG(L_ERR, "PRESENCE:refreshWatchers: Too many parameters\n");
		return init_mi_tree(400, "Too many parameters", 19);
	}

	ev= contains_event(&event, NULL);
	if(ev== NULL)
	{
		LOG(L_ERR, "PRESENCE:refreshWatchers: ERROR wrong event parameter\n");
		return 0;
	}	
	if(query_db_notify(&uri.user, &uri.host, ev, NULL)< 0)
	{
		LOG(L_ERR, "PRESENCE:refreshWatchers: ERROR while sending notify");
		return 0;
	}	
	
	return init_mi_tree(200, "OK", 2);
}	
