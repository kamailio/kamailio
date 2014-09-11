/*
 * $Id: pua_xmpp.c 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_xmpp module - presence SIP - XMPP Gateway
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
 *  2007-03-29  initial version (anca)
 */

/*! \file
 * \brief Kamailio presence gateway: SIP/SIMPLE -- XMPP (pua_xmpp)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../pt.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_expires.h"
#include "../../parser/msg_parser.h"
#include "../../modules/tm/tm_load.h"
#include "../xmpp/xmpp_api.h"
#include "../pua/pua_bind.h"

#include "pua_xmpp.h"
#include "xmpp2simple.h"
#include "simple2xmpp.h"
#include "request_winfo.h"

MODULE_VERSION

struct tm_binds tmb;

/* functions imported from pua module*/
pua_api_t pua;
send_publish_t pua_send_publish;
send_subscribe_t pua_send_subscribe;
query_dialog_t pua_is_dialog;

/* functions imported from xmpp module*/
xmpp_api_t xmpp_api;
xmpp_send_xsubscribe_f xmpp_subscribe;
xmpp_send_xnotify_f xmpp_notify;
xmpp_send_xpacket_f xmpp_packet;
xmpp_translate_uri_f duri_sip_xmpp;
xmpp_translate_uri_f euri_sip_xmpp;
xmpp_translate_uri_f duri_xmpp_sip;
xmpp_translate_uri_f euri_xmpp_sip;

/* libxml wrapper functions */
xmlNodeGetAttrContentByName_t XMLNodeGetAttrContentByName;
xmlDocGetNodeByName_t XMLDocGetNodeByName;
xmlNodeGetNodeByName_t XMLNodeGetNodeByName;
xmlNodeGetNodeContentByName_t XMLNodeGetNodeContentByName;

str server_address= STR_NULL;

/** module functions */

static int mod_init(void);
static int child_init(int);

static int fixup_pua_xmpp(void** param, int param_no);

static cmd_export_t cmds[]=
{
	{"pua_xmpp_notify", (cmd_function)Notify2Xmpp,	0, 0, 0, REQUEST_ROUTE},
	{"pua_xmpp_req_winfo", (cmd_function)request_winfo, 2, fixup_pua_xmpp, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"server_address",     PARAM_STR,	&server_address	},
	{0,			0,		0	}
};

/*! \brief module exports */
struct module_exports exports= {
	"pua_xmpp",			/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported  parameters */
	0,					/* exported statistics */
	0,					/* exported MI functions*/
	0,					/* exported pseudo-variables */
	0,					/* extra processes */
	mod_init,			/* module initialization function */
	0,					/* response handling function */
	0,					/* destroy function */
	child_init			/* per-child init function */
};

/*! \brief
 * init module function
 */
static int mod_init(void)
{
	load_tm_f  load_tm;
	bind_pua_t bind_pua;
	bind_xmpp_t bind_xmpp;
	bind_libxml_t bind_libxml;
	libxml_api_t libxml_api;

	/* check if compulsory parameter server_address is set */
	if(!server_address.s || server_address.len<=0)
	{
		LM_ERR("compulsory 'server_address' parameter not set!");
		return -1;
	}

	/* import the TM auto-loading function */
	if((load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))==NULL)
	{
		LM_ERR("can't import load_tm\n");
		return -1;
	}

	/* let the auto-loading function load all TM stuff */

	if(load_tm(&tmb)==-1)
	{
		LM_ERR("can't load tm functions\n");
		return -1;
	}

	/* bind libxml wrapper functions */
	if((bind_libxml= (bind_libxml_t)find_export("bind_libxml_api", 1, 0))== NULL)
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


	/* bind xmpp */
	bind_xmpp= (bind_xmpp_t)find_export("bind_xmpp", 0,0);
	if (!bind_xmpp)
	{
		LM_ERR("Can't bind to the XMPP module.\n");
		return -1;
	}
	if(bind_xmpp(&xmpp_api)< 0)
	{
		LM_ERR("Can't bind to the XMPP module.\n");
		return -1;
	}
	if(xmpp_api.xsubscribe== NULL)
	{
		LM_ERR("Could not import xsubscribe from the XMPP module. Version mismatch?\n");
		return -1;
	}
	xmpp_subscribe= xmpp_api.xsubscribe;

	if(xmpp_api.xnotify== NULL)
	{
		LM_ERR("Could not import xnotify from the XMPP module. Version mismatch?\n");
		return -1;
	}
	xmpp_notify= xmpp_api.xnotify;
	
	if(xmpp_api.xpacket== NULL)
	{
		LM_ERR("Could not import xnotify from the XMPP module. Version mismatch?\n");
		return -1;
	}
	xmpp_packet= xmpp_api.xpacket;

	if(xmpp_api.register_callback== NULL)
	{
		LM_ERR("Could not import register_callback"
				" to xmpp\n");
		return -1;
	}
	if(xmpp_api.register_callback(XMPP_RCV_PRESENCE, pres_Xmpp2Sip, NULL)< 0)
	{
		LM_ERR("ERROR while registering callback"
				" to xmpp\n");
		return -1;
	}
	if(xmpp_api.decode_uri_sip_xmpp== NULL)
	{
		LM_ERR("Could not import decode_uri_sip_xmpp"
				" from xmpp\n");
		return -1;
	}
	duri_sip_xmpp= xmpp_api.decode_uri_sip_xmpp;

	if(xmpp_api.encode_uri_sip_xmpp== NULL)
	{
		LM_ERR("Could not import encode_uri_sip_xmpp"
				" from xmpp\n");
		return -1;
	}
	euri_sip_xmpp= xmpp_api.encode_uri_sip_xmpp;

	if(xmpp_api.decode_uri_xmpp_sip== NULL)
	{
		LM_ERR("Could not import decode_uri_xmpp_sip"
				" from xmpp\n");
		return -1;
	}
	duri_xmpp_sip= xmpp_api.decode_uri_xmpp_sip;

	if(xmpp_api.encode_uri_xmpp_sip== NULL)
	{
		LM_ERR("Could not import encode_uri_xmpp_sip"
				" from xmpp\n");
		return -1;
	}
	euri_xmpp_sip= xmpp_api.encode_uri_xmpp_sip;

	/* bind pua */
	bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
	if (!bind_pua)
	{
		LM_ERR("Can't bind to PUA module\n");
		return -1;
	}
	
	if (bind_pua(&pua) < 0)
	{
		LM_ERR("Can't bind to PUA module\n");
		return -1;
	}
	if(pua.send_publish == NULL)
	{
		LM_ERR("Could not import send_publish() in module PUA. Version mismatch?\n");
		return -1;
	}
	pua_send_publish= pua.send_publish;

	if(pua.send_subscribe == NULL)
	{
		LM_ERR("Could not import send_publish() in module PUA. Version mismatch?\n");
		return -1;
	}
	pua_send_subscribe= pua.send_subscribe;
	
	if(pua.is_dialog == NULL)
	{
		LM_ERR("Could not import send_subscribe() in module PUA. Version mismatch?\n");
		return -1;
	}
	pua_is_dialog= pua.is_dialog;

	if(pua.register_puacb(XMPP_INITIAL_SUBS, Sipreply2Xmpp, NULL)< 0)
	{
		LM_ERR("Could not register PUA callback\n");
		return -1;
	}	

	return 0;
}

static int child_init(int rank)
{
	LM_DBG("child [%d]  pid [%d]\n", rank, getpid());
	return 0;
}

static int fixup_pua_xmpp(void** param, int param_no)
{
	pv_elem_t *model;
	str s;
	if(*param)
	{
		s.s = (char*)(*param); s.len = strlen(s.s);
		if(pv_parse_format(&s, &model)<0)
		{
			LM_ERR("wrong format[%s]\n",(char*)(*param));
			return E_UNSPEC;
		}
			
		*param = (void*)model;
		return 0;
	}
	LM_ERR("null format\n");
	return E_UNSPEC;
}

