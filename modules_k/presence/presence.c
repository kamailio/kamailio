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
#include "../../locking.h"
#include "../../usr_avp.h"
#include "../../lock_ops.h"
#include "../tm/tm_load.h"
#include "../sl/sl_api.h"
#include "../../pt.h"
#include "publish.h"
#include "subscribe.h"
MODULE_VERSION

#define S_TABLE_VERSION 0

char *log_buf = NULL;
static int clean_period=100;

/* database connection */
db_con_t *pa_db = NULL;
db_func_t pa_dbf;
gen_lock_set_t* set;
char *presentity_table="presentity";
char *active_watchers_table = "active_watchers";
char *watchers_table= "watchers";  
char *xcap_table= "xcap_xml";  
int use_db=1;
str server_address;

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

//int handle_notify(struct sip_msg*, char*, char*);

int counter =0;
int pid = 0;
char prefix='a';
int startup_time=0;
str db_url = {0, 0};
int lock_set_size = 8;
int expires_offset = 0;
int force_active = 0;
int default_expires = 3600;
int max_expires = 3600;

void destroy(void);

static cmd_export_t cmds[]=
{
	{"handle_publish",	  handle_publish,		0,0, REQUEST_ROUTE},
	{"handle_subscribe",  handle_subscribe,		0,0, REQUEST_ROUTE},
//	{"handle_notify",	  handle_notify,		0,0, REQUEST_ROUTE},
	{"stored_pres_info",  stored_pres_info,		1,0,   0          },
	{0,0,0,0,0} 
};

static param_export_t params[]={
	{ "db_url",					STR_PARAM, &db_url.s},
	{ "presentity_table",		STR_PARAM, &presentity_table},
	{ "active_watchers_table", 	STR_PARAM, &active_watchers_table},
	{ "watchers_table",			STR_PARAM, &watchers_table},
	{ "xcap_table",				STR_PARAM, &xcap_table},
	{ "clean_period",			INT_PARAM, &clean_period },
	{ "to_tag_pref",			STR_PARAM, &to_tag_pref },
	{ "totag_avpid",			INT_PARAM, &reply_tag_avp_id },
	{ "lock_set_size",			INT_PARAM, &lock_set_size },
	{ "expires_offset",			INT_PARAM, &expires_offset },
	{ "force_active",			INT_PARAM, &force_active },
	{ "max_expires",			INT_PARAM, &max_expires  },
	{ "server_address",         STR_PARAM, &server_address.s},
	{0,0,0}
};

/** module exports */
struct module_exports exports= {
	"presence",					/* module name */
	DEFAULT_DLFLAGS,			/* dlopen flags */
	cmds,						/* exported functions */
	params,						/* exported parameters */
	0,							/* exported statistics */
	0  ,						/* exported MI functions */
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
//	str _s;
//	int ver = 0;

	DBG("PRESENCE: initializing module ...\n");

	if(lock_set_size<=2)
		lock_set_size = 8;

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

	set = lock_set_alloc(lock_set_size);
	if( set == NULL )
	{
		LOG(L_ERR, "PRESENCE:mod_init:ERROR while allocating lock_set \n");
		return -1;
	}

	if ( (set = lock_set_init(set))== 0 )
	{
		LOG(L_ERR, "PRESENCE:mod_init: ERROR while initializing lock \n");
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

	/*
	_s.s = presentity_table;
	_s.len = strlen(presentity_table);
	
	 * ver =  table_version(&pa_dbf, pa_db, &_s);
	if(ver!=S_TABLE_VERSION)
	{
		LOG(L_ERR,"PRESENCE:mod_init: Wrong version v%d for table <%s>,"
				" need v%d\n", ver, presentity_table, S_TABLE_VERSION);
		return -1;
	}
	*/	
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
			LOG(L_ERR, "PRESENCE: child %d: Error in use_table\n", rank);
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
	
	lock_set_destroy(set);
	
	if(pa_db && pa_dbf.close)
		pa_dbf.close(pa_db);

}


int stored_pres_info(struct sip_msg* msg, char* pres_uri, char* s)
{
	db_key_t keys[2];
	db_val_t vals[13];
	db_res_t* result= NULL;
	struct sip_uri uri;
	
	
	if(parse_uri(pres_uri, strlen(pres_uri), &uri)!=0)
	{
		LOG(L_ERR, "PRESENCE: stored_pres_info: bad URI!\n");
		return -1;
	}

	if(uri.user.len<=0 || uri.user.s==NULL || uri.host.len<=0 ||
			uri.host.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: stored_pres_uri: bad URI in To header!\n");
		return -1;
	}

	keys[0]= "username";
	vals[0].type= DB_STR;
	vals[0].nul = 0;
	vals[0].val.str_val=uri.user;

	keys[1]= "domain";
	vals[1].type= DB_STR;
	vals[1].nul = 0;
	vals[1].val.str_val=uri.host;

	if(pa_dbf.query(pa_db, keys, 0, vals, 0, 2, 0, 0, &result )< 0)
	{
		LOG(L_ERR, "PRESENCE:stored_pres_uri: Error while querrying database\n");
		return -1;
	}

	if(result && result->n > 0)
	{
		pa_dbf.free_result(pa_db, result);
		return 1;
	}

	if(result)
		pa_dbf.free_result(pa_db, result);

	return -1;
}


