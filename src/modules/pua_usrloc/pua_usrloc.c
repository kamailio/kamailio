/*
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

#include "../../core/script_cb.h"
#include "../../core/sr_module.h"
#include "../../core/parser/parse_expires.h"
#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/str.h"
#include "../../core/mem/mem.h"
#include "../../core/pt.h"
#include "../../core/kemi.h"
#include "../usrloc/usrloc_mod.h"
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

pua_api_t _pu_pua;
str pres_prefix= STR_NULL;

/*! \brief Structure containing pointers to usrloc functions */
usrloc_api_t ul;

/** module functions */

static int mod_init(void);

static cmd_export_t cmds[]=
{
	{"pua_set_publish", (cmd_function)w_pua_set_publish, 0, 0, 0, REQUEST_ROUTE},
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
	"pua_usrloc",		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	0,					/* per-child init function */
	0					/* module destroy function */
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

	if (bind_pua(&_pu_pua) < 0)
	{
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	if(_pu_pua.send_publish == NULL)
	{
		LM_ERR("Could not import send_publish\n");
		return -1;
	}

	if(_pu_pua.send_subscribe == NULL)
	{
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}

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

	pxb->pua_set_publish = w_pua_set_publish;
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_pua_usrloc_exports[] = {
	{ str_init("pua"), str_init("pua_set_publish"),
		SR_KEMIP_INT, ki_pua_set_publish,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_pua_usrloc_exports);
	return 0;
}