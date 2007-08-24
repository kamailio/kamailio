/*
 * $Id: presence_xml.c 2006-12-07 18:05:05Z anca_vamanu$
 *
 * presence_xml module - Presence Handling XML bodies module
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
 *  2007-04-12  initial version (anca)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../mem/mem.h"
#include "../presence/bind_presence.h"
#include "../presence/hash.h"
#include "../presence/notify.h"
#include "../xcap_client/xcap_functions.h"
#include "../sl/sl_api.h"
#include "pidf.h"
#include "add_events.h"
#include "presence_xml.h"

MODULE_VERSION
#define S_TABLE_VERSION 1

/** module functions */

static int mod_init(void);
static int child_init(int);
static void destroy(void);
int pxml_add_xcap_server( modparam_t type, void* val);
int shm_copy_xcap_list();
void free_xs_list(xcap_serv_t* xs_list, int mem_type);
int xcap_doc_updated(int doc_type, str xid, char* doc);

/** module variables ***/
add_event_t pres_add_event;
update_watchers_t pres_update_watchers;

char* xcap_table="xcap_xml";  
str db_url = {0, 0};
int force_active= 0;
int pidf_manipulation= 0;
int integrated_xcap_server= 0;
xcap_serv_t* xs_list= NULL;

/* SL bind */
struct sl_binds slb;

/* database connection */
db_con_t *pxml_db = NULL;
db_func_t pxml_dbf;

/* functions imported from xcap_client module */

xcap_get_elem_t xcap_GetElem;

static param_export_t params[]={
	{ "db_url",					STR_PARAM,                          &db_url.s},
	{ "xcap_table",				STR_PARAM,                        &xcap_table},
	{ "force_active",			INT_PARAM,                     &force_active },
	{ "pidf_manipulation",      INT_PARAM,                 &pidf_manipulation}, 
	{ "integrated_xcap_server", INT_PARAM,            &integrated_xcap_server}, 
	{ "xcap_server",     STR_PARAM|USE_FUNC_PARAM,(void*)pxml_add_xcap_server},
	{  0,						0,										    0}
};
	/** module exports */
struct module_exports exports= {
	"presence_xml",				/* module name */
	 DEFAULT_DLFLAGS,           /* dlopen flags */
	 0,  						/* exported functions */
	 params,					/* exported parameters */
	 0,							/* exported statistics */
	 0,							/* exported MI functions */
	 0,							/* exported pseudo-variables */
	 0,							/* extra processes */
	 mod_init,					/* module initialization function */
	 (response_function) 0,		/* response handling function */
 	 destroy,					/* destroy function */
	 child_init                 /* per-child init function */
};
	
/**
 * init module function
 */
static int mod_init(void)
{
	str _s;
	int ver = 0;
	DBG("presence_xml: mod_init...\n");
	bind_presence_t bind_presence;
	presence_api_t pres;
		
	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	DBG("presence_xml:mod_init: db_url=%s/%d/%p\n",ZSW(db_url.s),db_url.len,
			db_url.s);
	
	/* binding to mysql module  */
	if (bind_dbmod(db_url.s, &pxml_dbf))
	{
		DBG("presence_xml:mod_init: ERROR: Database module not found\n");
		return -1;
	}
	
	if (!DB_CAPABILITY(pxml_dbf, DB_CAP_ALL)) {
		LOG(L_ERR,"presence_xml:mod_init: ERROR Database module does not implement "
		    "all functions needed by the module\n");
		return -1;
	}

	pxml_db = pxml_dbf.init(db_url.s);
	if (!pxml_db)
	{
		LOG(L_ERR,"presence_xml:mod_init: Error while connecting database\n");
		return -1;
	}

	_s.s = xcap_table;
	_s.len = strlen(xcap_table);
	 ver =  table_version(&pxml_dbf, pxml_db, &_s);
	if(ver!=S_TABLE_VERSION)
	{
		LOG(L_ERR,"presence_xml:mod_init: Wrong version v%d for table <%s>,"
				" need v%d\n", ver, _s.s, S_TABLE_VERSION);
		return -1;
	}
	/* load SL API */
	if(load_sl_api(&slb)==-1)
	{
		LOG(L_ERR, "presence_xml:mod_init:ERROR can't load sl functions\n");
		return -1;
	}

	bind_presence= (bind_presence_t)find_export("bind_presence", 1,0);
	if (!bind_presence)
	{
		LOG(L_ERR, "presence_xml:mod_init: Can't bind presence\n");
		return -1;
	}
	if (bind_presence(&pres) < 0)
	{
		LOG(L_ERR, "presence_xml:mod_init Can't bind pua\n");
		return -1;
	}

	pres_add_event= pres.add_event;
	pres_update_watchers= pres.update_watchers_status;
	if (pres_add_event == NULL || pres_update_watchers== NULL)
	{
		LOG(L_ERR, "presence_xml:mod_init Could not import add_event\n");
		return -1;
	}
	if(xml_add_events()< 0)
	{
		LOG(L_ERR, "presence_xml:mod_init: ERROR while adding xml events\n");
		return -1;		
	}
	
	if(force_active== 0 && !integrated_xcap_server )
	{
		xcap_api_t xcap_api;
		bind_xcap_t bind_xcap;

		/* bind xcap */
		bind_xcap= (bind_xcap_t)find_export("bind_xcap", 1, 0);
		if (!bind_xcap)
		{
			LOG(L_ERR, "presence_xml:mod_init: Can't bind xcap_client\n");
			return -1;
		}
	
		if (bind_xcap(&xcap_api) < 0)
		{
			LOG(L_ERR, "presence_xml:mod_init: Can't bind xcap\n");
			return -1;
		}
		xcap_GetElem= xcap_api.get_elem;
		if(xcap_GetElem== NULL)
		{
			LOG(L_ERR, "presence_xml:mod_init:erorr NULL could not import"
					" get_elem from xcap_client module\n");
			return -1;
		}

		if(xcap_api.register_xcb(PRES_RULES, xcap_doc_updated)< 0)
		{
			LOG(L_ERR,"presence_xml:mod_init:ERROR registering xcap"
					" callback function\n");
			return -1;
		}
	}

	if(shm_copy_xcap_list()< 0)
	{
		LOG(L_ERR, "presence_xml:mod_init:erorr copying xcap server list"
				" in share memory\n");
		return -1;
	}

	if(pxml_db)
		pxml_dbf.close(pxml_db);
	pxml_db = NULL;

	return 0;
}

static int child_init(int rank)
{
	DBG("presence_xml: init_child [%d]  pid [%d]\n", rank, getpid());
	
	if (pxml_dbf.init==0)
	{
		LOG(L_CRIT, "BUG: PRESENCE_XML: child_init: database not bound\n");
		return -1;
	}
	pxml_db = pxml_dbf.init(db_url.s);
	if (!pxml_db)
	{
		LOG(L_ERR,"PRESENCE_XML: child %d: Error while connecting database\n",
				rank);
		return -1;
	}
	else
	{
		if (pxml_dbf.use_table(pxml_db, xcap_table) < 0)  
		{
			LOG(L_ERR, "PRESENCE_XML: child %d: Error in use_table\n", rank);
			return -1;
		}
		
		DBG("PRESENCE_XML: child %d: Database connection opened successfully\n", rank);
	}

	return 0;
}	

static void destroy(void)
{	
	DBG("presence_xml: destroying module ...\n");
	if(pxml_db && pxml_dbf.close)
		pxml_dbf.close(pxml_db);

	free_xs_list(xs_list, SHM_MEM_TYPE);

	return ;
}

int pxml_add_xcap_server( modparam_t type, void* val)
{
	xcap_serv_t* xs;
	int size;
	char* serv_addr= (char*)val;

	size= sizeof(xcap_serv_t)+ (strlen(serv_addr)+ 1)* sizeof(char);
	xs= (xcap_serv_t*)pkg_malloc(size);
	if(xs== NULL)
	{
		ERR_MEM("PRESENCE_XML", "pxml_add_xcap_server");
	}
	memset(xs, 0, size);
	size= sizeof(xcap_serv_t);

	xs->addr= (char*)xs+ size;
	strcpy(xs->addr, serv_addr);
	/* check for duplicates */
	xs->next= xs_list;
	xs_list= xs;
	return 0;

error:
	free_xs_list(xs_list, PKG_MEM_TYPE);
	return -1;
}

int shm_copy_xcap_list()
{
	xcap_serv_t* xs, *shm_xs, *prev_xs;
	int size;

	xs= xs_list;
	if(xs== NULL)
	{
		if(force_active== 0 && !integrated_xcap_server)
		{
			LOG(L_ERR, "PRESENCE_XML:shm_copy_xcap_list: ERROR"
				" no xcap_server parameter set\n");
			return -1;
		}
		return 0;
	}
	xs_list= NULL;
	size= sizeof(xcap_serv_t);
	
	while(xs)
	{
		size+= (strlen(xs->addr)+ 1)* sizeof(char);
		shm_xs= (xcap_serv_t*)shm_malloc(size);
		if(shm_xs== NULL)
		{
			ERR_MEM("PRESENCE_XML", "pxml_add_xcap_server");
		}
		memset(shm_xs, 0, size);
		size= sizeof(xcap_serv_t);

		shm_xs->addr= (char*)shm_xs+ size;
		strcpy(shm_xs->addr, xs->addr);
		shm_xs->next= xs_list; 
		xs_list= shm_xs;

		prev_xs= xs;
		xs= xs->next;

		pkg_free(prev_xs);
	}
	return 0;

error:
	free_xs_list(xs_list, SHM_MEM_TYPE);
	return -1;
}

void free_xs_list(xcap_serv_t* xsl, int mem_type)
{
	xcap_serv_t* xs, *prev_xs;

	xs= xsl;

	while(xs)
	{
		prev_xs= xs;
		xs= xs->next;
		if(mem_type & SHM_MEM_TYPE)
			shm_free(prev_xs);
		else
			pkg_free(prev_xs);
	}
	xsl= NULL;
}

int xcap_doc_updated(int doc_type, str xid, char* doc)
{
	struct sip_uri uri;
	db_key_t query_cols[3], update_cols[3];
	db_val_t query_vals[3], update_vals[3];
	int n_query_cols= 0;
	pres_ev_t ev;
	str rules_doc;

	if(parse_uri(xid.s, xid.len, &uri)< 0)
	{
		LOG(L_ERR, "PRESENCE_XML:xcap_doc_updated:ERROR parsing uri\n");
		return -1;
	}

	/* update in xml table */
	query_cols[n_query_cols]= "username";
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB_STR;
	query_vals[n_query_cols].val.str_val= uri.user;
	n_query_cols++;

	query_cols[n_query_cols]= "domain";
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB_STR;
	query_vals[n_query_cols].val.str_val= uri.host;
	n_query_cols++;

	query_cols[n_query_cols]= "doc_type";
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB_INT;
	query_vals[n_query_cols].val.int_val= doc_type;
	n_query_cols++;

	update_cols[0]= "xcap";
	update_vals[0].nul= 0;
	update_vals[0].type= DB_STR;
	update_vals[0].val.string_val= doc;

	if(pxml_dbf.use_table(pxml_db, xcap_table)< 0)
	{
		LOG(L_ERR, "PRESENCE_XML:xcap_doc_updated:ERROR in sql use table\n");
		return -1;
	}
	if(pxml_dbf.update(pxml_db, query_cols, 0, query_vals, update_cols, 
				update_vals, n_query_cols, 1)< 0)
	{
		LOG(L_ERR, "PRESENCE_XML:xcap_doc_updated:ERROR in sql update\n");
		return -1;	
	}


	/* call updating watchers */
	ev.name.s= "presence";
	ev.name.len= PRES_LEN;

	rules_doc.s= doc;
	rules_doc.len= strlen(doc);

	if(pres_update_watchers(xid, &ev, &rules_doc)< 0)
	{
		LOG(L_ERR, "PRESENCE_XML:xcap_doc_updated:ERROR updating watchers"
				" in presence\n");
		return -1;	
	}
	return 0;

}
