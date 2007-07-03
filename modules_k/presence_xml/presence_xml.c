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
#include "../../parser/msg_parser.h"
#include "../../mem/mem.h"
#include "../presence/bind_presence.h"
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

/** module variables ***/
event_api_t pres;
add_event_t pres_add_event;
char* xcap_table="xcap_xml";  
str db_url = {0, 0};
int force_active= 0;
int pidf_manipulation= 0;
/* SL bind */
struct sl_binds slb;

/* database connection */
db_con_t *pxml_db = NULL;
db_func_t pxml_dbf;


static cmd_export_t cmds[]=
{
	{"bind_libxml_api",					(cmd_function)bind_libxml_api,			   1, 0, 0},	    
	{"xmlDocGetNodeByName",				(cmd_function)xmlDocGetNodeByName,		   1, 0, 0},
	{"xmlNodeGetNodeByName",			(cmd_function)xmlNodeGetNodeByName,        1, 0, 0},
	{"xmlNodeGetNodeContentByName",		(cmd_function)xmlNodeGetNodeContentByName, 1, 0, 0},
	{"xmlNodeGetAttrContentByName",     (cmd_function)xmlNodeGetAttrContentByName, 1, 0, 0},
	{	    0,								0,						               0, 0, 0}
};

static param_export_t params[]={
	{ "db_url",					STR_PARAM,  &db_url.s},
	{ "xcap_table",				STR_PARAM,  &xcap_table},
	{ "force_active",			INT_PARAM,  &force_active },
	{ "pidf_manipulation",      INT_PARAM,  &pidf_manipulation}, 
	{  0,						0,							 0}
};
	/** module exports */
struct module_exports exports= {
	"presence_xml",				/* module name */
	 DEFAULT_DLFLAGS,           /* dlopen flags */
	 cmds,						/* exported functions */
	 params,					/* exported parameters */
	 0,							/* exported statistics */
	 0,							/* exported MI functions */
	 0,							/* exported pseudo-variables */
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
	
	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	DBG("presence_xml:mod_init: db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len,
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
	if (add_event == NULL)
	{
		LOG(L_ERR, "presence_xml:mod_init Could not import add_event\n");
		return -1;
	}
	if(xml_add_events()< 0)
	{
		LOG(L_ERR, "presence_xml:mod_init: ERROR while adding xml events\n");
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

	return ;
}

