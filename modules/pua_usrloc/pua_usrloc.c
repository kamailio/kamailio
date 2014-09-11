/*
 * $Id$
 *
 * pua_usrloc module - usrloc pua module
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
 * History:
 * --------
 *  2006-11-29  initial version (anca)
 */

/*!
 * \file
 * \brief SIP-router Presence :: Usrloc module
 * \ingroup core
 * Module: \ref core
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../script_cb.h"
#include "../../sr_module.h"
#include "../../parser/parse_expires.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../pt.h"
#include "../usrloc/ul_mod.h"
#include "../usrloc/usrloc.h"
#include "../usrloc/ul_callback.h"
#include "../pua/pua_bind.h"
#include "pua_usrloc.h"
#include "api.h"

MODULE_VERSION

str default_domain= STR_NULL;

int pua_ul_publish = 0;
int pua_ul_bflag = -1;
int pua_ul_bmask = 0;

pua_api_t pua;
str pres_prefix= STR_NULL;

/*! \brief Structure containing pointers to usrloc functions */
usrloc_api_t ul;

/** module functions */

static int mod_init(void);

int pua_set_publish(struct sip_msg* , char*, char*);

send_publish_t pua_send_publish;
send_subscribe_t pua_send_subscribe;

static cmd_export_t cmds[]=
{
	{"pua_set_publish", (cmd_function)pua_set_publish, 0, 0, 0, REQUEST_ROUTE},
	{"bind_pua_usrloc", (cmd_function)bind_pua_usrloc, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0} 
};

static param_export_t params[]={
	{"default_domain",	 PARAM_STR, &default_domain	 },
	{"entity_prefix",	 PARAM_STR, &pres_prefix		 },
	{"branch_flag",	     INT_PARAM, &pua_ul_bflag		 },
	{0,							 0,			0            }
};

struct module_exports exports= {
	"pua_usrloc",				/*!< module name */
	DEFAULT_DLFLAGS,			/*!< dlopen flags */
	cmds,						/*!< exported functions */
	params,						/*!< exported parameters */
	0,							/*!< exported statistics */
	0,							/*!< exported MI functions */
	0,							/*!< exported pseudo-variables */
	0,							/*!< extra processes */
	mod_init,					/*!< module initialization function */
	0,							/*!< response handling function */
	0,							/*!< destroy function */
	0							/*!< per-child init function */
};
	
/*! \brief
 * init module function
 */
static int mod_init(void)
{
	bind_usrloc_t bind_usrloc;
	bind_pua_t bind_pua;
	
	if(!default_domain.s || default_domain.len<=0)
	{	
		LM_ERR("default domain parameter not set\n");
		return -1;
	}
	
	if(!pres_prefix.s || pres_prefix.len<=0)
		LM_DBG("No pres_prefix configured\n");

	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
	if (!bind_usrloc)
	{
		LM_ERR("Can't bind usrloc\n");
		return -1;
	}
	if (bind_usrloc(&ul) < 0)
	{
		LM_ERR("Can't bind usrloc\n");
		return -1;
	}
	if(ul.register_ulcb == NULL)
	{
		LM_ERR("Could not import ul_register_ulcb\n");
		return -1;
	}

	if(ul.register_ulcb(UL_CONTACT_INSERT, ul_publish, 0)< 0)
	{
		LM_ERR("can not register callback for"
				" insert\n");
		return -1;
	}
	if(ul.register_ulcb(UL_CONTACT_EXPIRE, ul_publish, 0)< 0)
	{
		LM_ERR("can not register callback for"
				" expire\n");
		return -1;
	}
	
	if(ul.register_ulcb(UL_CONTACT_UPDATE, ul_publish, 0)< 0)
	{
		LM_ERR("can not register callback for update\n");
		return -1;
	}
	
	if(ul.register_ulcb(UL_CONTACT_DELETE, ul_publish, 0)< 0)
	{
		LM_ERR("can not register callback for delete\n");
		return -1;
	}
	
	bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
	if (!bind_pua)
	{
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	
	if (bind_pua(&pua) < 0)
	{
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	if(pua.send_publish == NULL)
	{
		LM_ERR("Could not import send_publish\n");
		return -1;
	}
	pua_send_publish= pua.send_publish;

	if(pua.send_subscribe == NULL)
	{
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}
	pua_send_subscribe= pua.send_subscribe;
	
	/* register post-script pua_unset_publish unset function */
	if(register_script_cb(pua_unset_publish, POST_SCRIPT_CB|REQUEST_CB, 0)<0)
	{
		LM_ERR("failed to register POST request callback\n");
		return -1;
	}

	if(pua_ul_bflag!=-1)
		pua_ul_bmask = 1 << pua_ul_bflag;

	return 0;
}

int bind_pua_usrloc(struct pua_usrloc_binds *pxb)
{
	if (pxb == NULL)
	{
		LM_WARN("bind_pua_usrloc: Cannot load pua_usrloc API into a NULL pointer\n");
		return -1;
	}

	pxb->pua_set_publish = pua_set_publish;
	return 0;
}
