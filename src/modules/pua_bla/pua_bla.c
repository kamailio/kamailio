/*
 * $Id: pua_bla.c 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_bla module - pua Bridged Line Appearance
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
 * History:
 * --------
 *  2007-03-30  initial version (anca)
 */

#include<stdio.h>
#include<stdlib.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../usrloc/usrloc.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "pua_bla.h"
#include "registrar_cb.h"

MODULE_VERSION
/* Structure containing pointers to pua functions */
pua_api_t pua;
/* Structure containing pointers to usrloc functions */
usrloc_api_t ul;
str default_domain= {NULL, 0};
str header_name= {0, 0};
str bla_outbound_proxy= {0, 0};
int is_bla_aor= 0;
str reg_from_uri= {0, 0};
static int mod_init(void);

send_publish_t pua_send_publish;
send_subscribe_t pua_send_subscribe;
query_dialog_t pua_is_dialog;
int bla_set_flag(struct sip_msg* , char*, char*);
str server_address= {0, 0};
static cmd_export_t cmds[]=
{
	{"bla_set_flag", (cmd_function)bla_set_flag,		 0, 0, 0, REQUEST_ROUTE},
	{"bla_handle_notify", (cmd_function)bla_handle_notify,   0, 0, 0, REQUEST_ROUTE}, 	
	{0, 0, 0, 0, 0, 0}
};
static param_export_t params[]=
{
	{"server_address",	 PARAM_STR, &server_address	 },
	{"default_domain",	 PARAM_STR, &default_domain	 },
	{"header_name",      PARAM_STR, &header_name       },
	{"outbound_proxy",   PARAM_STR, &bla_outbound_proxy    },
	{0,							 0,			0            }
};

/** module exports */
struct module_exports exports= {
	"pua_bla",					/* module name */
	DEFAULT_DLFLAGS,            /* dlopen flags */
	cmds,						/* exported functions */
	params,						/* exported parameters */
	0,							/* exported statistics */
	0,							/* exported MI functions */
	0,							/* exported pseudo-variables */
	0,							/* extra processes */
	mod_init,					/* module initialization function */
	0,							/* response handling function */
	0,							/* destroy function */
	0							/* per-child init function */
};
	
/**
 * init module function
 */
static int mod_init(void)
{
	bind_pua_t bind_pua;
	bind_usrloc_t bind_usrloc;

	if(!server_address.s || server_address.len<=0)
	{
		LM_ERR("compulsory 'server_address' parameter not set!");
		return -1;
	}

	if(!default_domain.s || default_domain.len<=0)
	{	
		LM_ERR("default domain not found\n");
		return -1;
	}

	if(!header_name.s || header_name.len<=0)
	{	
		LM_ERR("header_name parameter not set\n");
		return -1;
	}

	if(!bla_outbound_proxy.s || bla_outbound_proxy.len<=0)
	{	
		LM_DBG("No outbound proxy set\n");
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

	if(pua.is_dialog == NULL)
	{
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}
	pua_is_dialog= pua.is_dialog;
	
	if(pua.register_puacb== NULL)
	{
		LM_ERR("Could not import register callback\n");
		return -1;
	}	

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

	if(ul.register_ulcb(UL_CONTACT_INSERT, bla_cb , 0)< 0)
	{
		LM_ERR("can not register callback for"
				" insert\n");
		return -1;
	}
	if(ul.register_ulcb(UL_CONTACT_EXPIRE, bla_cb, 0)< 0)
	{	
		LM_ERR("can not register callback for"
				" insert\n");
		return -1;
	}
	if(ul.register_ulcb(UL_CONTACT_UPDATE, bla_cb, 0)< 0)
	{	
		LM_ERR("can not register callback for"
				" update\n");
		return -1;
	}
	if(ul.register_ulcb(UL_CONTACT_DELETE, bla_cb, 0)< 0)
	{	
		LM_ERR("can not register callback for"
				" delete\n");
		return -1;
	}

	return 0;
}


int bla_set_flag(struct sip_msg* msg , char* s1, char* s2)
{
	LM_DBG("mark as bla aor\n");
	
	is_bla_aor= 1;
	
	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		return -1;
	}
	

	if (msg->from->parsed == NULL)
	{
		if ( parse_from_header( msg )<0 ) 
		{
			LM_DBG("cannot parse From header\n");
			return -1;
		}
	}

	reg_from_uri= ((struct to_body*)(msg->from->parsed))->uri;

	return 1;
}	

