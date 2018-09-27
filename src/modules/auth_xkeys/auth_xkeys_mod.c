/**
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/mod_fix.h"
#include "../../core/timer_proc.h"
#include "../../core/route_struct.h"
#include "../../core/kemi.h"

#include "auth_xkeys.h"

MODULE_VERSION


static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

int authx_xkey_param(modparam_t type, void* val);

static int w_auth_xkeys_add(sip_msg_t* msg, char* hdr, char* key,
		char* alg, char* data);
static int fixup_auth_xkeys_add(void** param, int param_no);
static int w_auth_xkeys_check(sip_msg_t* msg, char* hdr, char* key,
		char* alg, char* data);
static int fixup_auth_xkeys_check(void** param, int param_no);

/* timer for cleaning up the expired keys */
/* int auth_xkeys_timer_mode = 0; */


static cmd_export_t cmds[]={
	{"auth_xkeys_add", (cmd_function)w_auth_xkeys_add,
		4, fixup_auth_xkeys_add,
		0, ANY_ROUTE},
	{"auth_xkeys_check", (cmd_function)w_auth_xkeys_check,
		4, fixup_auth_xkeys_check,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"xkey",           PARAM_STRING|USE_FUNC_PARAM, (void*)authx_xkey_param},
	/* {"timer_mode",     PARAM_INT,   &auth_xkeys_timer_mode}, */
	{0, 0, 0}
};

struct module_exports exports = {
	"auth_xkeys",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy     /* destroy function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	if(auth_xkeys_init_rpc()<0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 *
 */
static int w_auth_xkeys_add(sip_msg_t* msg, char* hdr, char* key,
		char* alg, char* data)
{
	str shdr;
	str skey;
	str salg;
	str sdata;

	if(fixup_get_svalue(msg, (gparam_t*)hdr, &shdr)!=0)
	{
		LM_ERR("cannot get the header name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)key, &skey)!=0)
	{
		LM_ERR("cannot get the key id\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)alg, &salg)!=0)
	{
		LM_ERR("cannot get the algorithm\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)data, &sdata)!=0)
	{
		LM_ERR("cannot get the hasing data\n");
		return -1;
	}

	if(auth_xkeys_add(msg, &shdr, &skey, &salg, &sdata)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_auth_xkeys_add(sip_msg_t* msg, str *shdr, str *skey, str *salg,
			str *sdata)
{
	if(auth_xkeys_add(msg, shdr, skey, salg, sdata)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int w_auth_xkeys_check(sip_msg_t* msg, char* hdr, char* key,
		char* alg, char* data)
{
	str shdr;
	str skey;
	str salg;
	str sdata;

	if(fixup_get_svalue(msg, (gparam_t*)hdr, &shdr)!=0)
	{
		LM_ERR("cannot get the header name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)key, &skey)!=0)
	{
		LM_ERR("cannot get the key id\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)alg, &salg)!=0)
	{
		LM_ERR("cannot get the algorithm\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)data, &sdata)!=0)
	{
		LM_ERR("cannot get the hasing data\n");
		return -1;
	}

	if(auth_xkeys_check(msg, &shdr, &skey, &salg, &sdata)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_auth_xkeys_check(sip_msg_t* msg, str *shdr, str *skey, str *salg,
			str *sdata)
{
	if(auth_xkeys_check(msg, shdr, skey, salg, sdata)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int fixup_auth_xkeys_add(void** param, int param_no)
{
	if(fixup_spve_null(param, 1)<0)
		return -1;
	return 0;
}

/**
 *
 */
static int fixup_auth_xkeys_check(void** param, int param_no)
{
	if(fixup_spve_null(param, 1)<0)
		return -1;
	return 0;
}

/**
 *
 */
int authx_xkey_param(modparam_t type, void* val)
{
	str s;

	if(val==NULL)
		return -1;
	s.s = (char*)val;
	s.len = strlen(s.s);
	return authx_xkey_add_params(&s);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_auth_xkeys_exports[] = {
	{ str_init("auth_xkeys"), str_init("auth_xkeys_add"),
		SR_KEMIP_INT, ki_auth_xkeys_add,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth_xkeys"), str_init("auth_xkeys_check"),
		SR_KEMIP_INT, ki_auth_xkeys_check,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_auth_xkeys_exports);
	return 0;
}
