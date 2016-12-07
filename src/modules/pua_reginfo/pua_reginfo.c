/*
 * pua_reginfo module - Presence-User-Agent Handling of reg events
 *
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
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
 */

/* Bindings to PUA */
#include "../pua/pua_bind.h"
/* Bindings to usrloc */
#include "../usrloc/usrloc.h"
#include "../../lib/srutils/sruid.h"

#include "pua_reginfo.h"
#include "subscribe.h"
#include "notify.h"
#include "usrloc_cb.h"

MODULE_VERSION

usrloc_api_t ul; /*!< Structure containing pointers to usrloc functions*/
pua_api_t pua; /*!< Structure containing pointers to PUA functions*/

/* Default domain to be added, if none provided. */
str default_domain = {NULL, 0};
str outbound_proxy = {NULL, 0};
str server_address = {NULL, 0};

int publish_reginfo = 1;

sruid_t _reginfo_sruid;

int reginfo_use_domain = 0;

/** Fixup functions */
static int domain_fixup(void** param, int param_no);

/** module functions */
static int mod_init(void);

/* Commands */
static cmd_export_t cmds[] = {
	{"reginfo_subscribe", (cmd_function)reginfo_subscribe, 1, fixup_subscribe, 0, REQUEST_ROUTE|ONREPLY_ROUTE}, 	
	{"reginfo_subscribe", (cmd_function)reginfo_subscribe2, 2, fixup_subscribe, 0, REQUEST_ROUTE|ONREPLY_ROUTE}, 	
	{"reginfo_handle_notify", (cmd_function)reginfo_handle_notify, 1, domain_fixup, 0, REQUEST_ROUTE}, 	
	{0, 0, 0, 0, 0, 0} 
};

static param_export_t params[]={
 	{"default_domain", PARAM_STR, &default_domain},
	{"outbound_proxy", PARAM_STR, &outbound_proxy},
	{"server_address", PARAM_STR, &server_address},
	{"publish_reginfo", INT_PARAM, &publish_reginfo},
	{0, 0, 0}
};

struct module_exports exports= {
	"pua_reginfo",		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* exported functions */
	params,			/* exported parameters */
	0,			/* exported statistics */
	0,			/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,		/* module initialization function */
	0,			/* response handling function */
	0,			/* destroy function */
	NULL			/* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	bind_pua_t bind_pua;
	bind_usrloc_t bind_usrloc;

	if (publish_reginfo == 1) {
    /* Verify the default domain: */
    if(!default_domain.s || default_domain.len<=0) {
      LM_ERR("default domain parameter not set\n");
      return -1;
    }
	}

	if(!server_address.s || server_address.len<=0) {
		LM_ERR("server_address parameter not set\n");
		return -1;
	}

	if(!outbound_proxy.s || outbound_proxy.len<=0)
		LM_DBG("No outbound proxy set\n");
        
	/* Bind to PUA: */
	bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
	if (!bind_pua) {
		LM_ERR("Can't bind pua\n");
		return -1;
	}	
	if (bind_pua(&pua) < 0) {
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	/* Check for Publish/Subscribe methods */
	if(pua.send_publish == NULL) {
		LM_ERR("Could not import send_publish\n");
		return -1;
	}
	if(pua.send_subscribe == NULL) {
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}

	/* Bind to URSLOC: */
	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
	if (!bind_usrloc) {
		LM_ERR("Can't bind usrloc\n");
		return -1;
	}
	if (bind_usrloc(&ul) < 0) {
		LM_ERR("Can't bind usrloc\n");
		return -1;
	}
	if (publish_reginfo == 1) {
		if(ul.register_ulcb == NULL) {
			LM_ERR("Could not import ul_register_ulcb\n");
			return -1;
		}
		if(ul.register_ulcb(UL_CONTACT_INSERT, reginfo_usrloc_cb , 0)< 0) {
			LM_ERR("can not register callback for insert\n");
			return -1;
		}
		if(ul.register_ulcb(UL_CONTACT_EXPIRE, reginfo_usrloc_cb, 0)< 0) {	
			LM_ERR("can not register callback for expire\n");
			return -1;
		}
		if(ul.register_ulcb(UL_CONTACT_UPDATE, reginfo_usrloc_cb, 0)< 0) {	
			LM_ERR("can not register callback for update\n");
			return -1;
		}
		if(ul.register_ulcb(UL_CONTACT_DELETE, reginfo_usrloc_cb, 0)< 0) {	
			LM_ERR("can not register callback for delete\n");
			return -1;
		}
	}

	if(sruid_init(&_reginfo_sruid, (char)'-', "regi", SRUID_INC)<0)
		return -1;

	/*
	 * Import use_domain parameter from usrloc
	 */
	reginfo_use_domain = ul.use_domain;

	return 0;
}

/*! \brief
 * Convert char* parameter to udomain_t* pointer
 */
static int domain_fixup(void** param, int param_no)
{
	udomain_t* d;

	if (param_no == 1) {
		if (ul.register_udomain((char*)*param, &d) < 0) {
			LM_ERR("failed to register domain\n");
			return E_UNSPEC;
		}

		*param = (void*)d;
	}
	return 0;
}

