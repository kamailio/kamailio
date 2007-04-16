/*
 * $Id: pua_bla.c 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_bla module - pua Bridged Line Appearance
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
 *  2007-03-30  initial version (anca)
 */

#include<stdio.h>
#include<stdlib.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../usrloc/usrloc.h"
#include "pua_bla.h"
#include "registrar_cb.h"

MODULE_VERSION
/* Structure containing pointers to pua functions */
pua_api_t pua;
/* Structure containing pointers to usrloc functions */
usrloc_api_t ul;
str default_domain= {NULL, 0};
str header_name= {0, 0};
str outbound_proxy= {0, 0};
int is_bla_aor= 0;
static int mod_init(void);
static int child_init(int);
static void destroy(void);

send_publish_t pua_send_publish;
send_subscribe_t pua_send_subscribe;
query_dialog_t pua_is_dialog;
int bla_set_flag(struct sip_msg* , char*, char*);

static cmd_export_t cmds[]=
{
	{"bla_set_flag",            bla_set_flag,		 0, 0, REQUEST_ROUTE},
	{"bla_handle_notify",		bla_handle_notify,   0, 0, REQUEST_ROUTE}, 	
	{0,							0,					 0, 0, 0} 
};
static param_export_t params[]=
{
	{"default_domain",	 STR_PARAM, &default_domain.s	 },
	{"header_name",      STR_PARAM, &header_name.s       },
	{"outbound_proxy",   STR_PARAM, &outbound_proxy.s    },
	{0,							 0,			0            }
};

/** module exports */
struct module_exports exports= {
	"pua_bla",					/* module name */
	DEFAULT_DLFLAGS,            /* dlopen flags */
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
	DBG("pua_bla: mod_init...\n");
	bind_pua_t bind_pua;
	bind_usrloc_t bind_usrloc;
	
	if(default_domain.s == NULL )
	{	
		LOG(L_ERR, "pua_bla: default domain not found\n");
		return -1;
	}
	default_domain.len= strlen(default_domain.s);

	if(header_name.s == NULL )
	{	
		LOG(L_ERR, "pua_bla: default domain not found\n");
		return -1;
	}
	header_name.len= strlen(header_name.s);

	if(outbound_proxy.s == NULL )
	{	
		DBG("pua_bla: No outbound proxy set\n");
	}
	else
		outbound_proxy.len= strlen(outbound_proxy.s);

	bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
	if (!bind_pua)
	{
		LOG(L_ERR, "pua_bla:mod_init: Can't bind pua\n");
		return -1;
	}
	
	if (bind_pua(&pua) < 0)
	{
		LOG(L_ERR, "pua_bla:mod_init Can't bind pua\n");
		return -1;
	}
	if(pua.send_publish == NULL)
	{
		LOG(L_ERR, "pua_bla:mod_init Could not import send_publish\n");
		return -1;
	}
	pua_send_publish= pua.send_publish;

	if(pua.send_subscribe == NULL)
	{
		LOG(L_ERR, "pua_bla:mod_init Could not import send_subscribe\n");
		return -1;
	}
	pua_send_subscribe= pua.send_subscribe;

	if(pua.is_dialog == NULL)
	{
		LOG(L_ERR, "pua_bla:mod_init Could not import send_subscribe\n");
		return -1;
	}
	pua_is_dialog= pua.is_dialog;
	
	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
	if (!bind_usrloc)
	{
		LOG(L_ERR, "pua_bla:mod_init:ERROR Can't bind usrloc\n");
		return -1;
	}
	if (bind_usrloc(&ul) < 0)
	{
		LOG(L_ERR, "pua_bla:mod_init Can't bind usrloc\n");
		return -1;
	}
	if(ul.register_ulcb == NULL)
	{
		LOG(L_ERR, "pua_bla:mod_init Could not import ul_register_ulcb\n");
		return -1;
	}

	if(ul.register_ulcb(UL_CONTACT_INSERT, bla_cb , 0)< 0)
	{
		LOG(L_ERR, "pua_bla:mod_init can not register callback for"
				" insert\n");
		return -1;
	}
	if(ul.register_ulcb(UL_CONTACT_EXPIRE, bla_cb, 0)< 0)
	{	
		LOG(L_ERR, "pua_bla:mod_init can not register callback for"
				" insert\n");
		return -1;
	}
	if(ul.register_ulcb(UL_CONTACT_UPDATE, bla_cb, 0)< 0)
	{	
		LOG(L_ERR, "pua_bla:mod_init can not register callback for"
				" update\n");
		return -1;
	}
	if(ul.register_ulcb(UL_CONTACT_DELETE, bla_cb, 0)< 0)
	{	
		LOG(L_ERR, "pua_bla:mod_init can not register callback for"
				" delete\n");
		return -1;
	}

	return 0;
}

static int child_init(int rank)
{
	DBG("pua_bla: init_child [%d]  pid [%d]\n", rank, getpid());
	return 0;
}	

static void destroy(void)
{	
	DBG("pua_bla: destroying module ...\n");

	return ;
}

int bla_set_flag(struct sip_msg* msg , char* s1, char* s2)
{
	DBG("pua_bla: bla_set_flag: mark as bla aor\n");
	is_bla_aor= 1;
	return 1;
}	

