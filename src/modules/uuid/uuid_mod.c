/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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

#include <uuid/uuid.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/utils/sruid.h"

MODULE_VERSION

#ifdef UUID_LEN_STR
#define KSR_UUID_BSIZE (UUID_LEN_STR + 4)
#else
#define KSR_UUID_BSIZE 40
#endif

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static uuid_t _k_uuid_val;
static char   _k_uuid_str[KSR_UUID_BSIZE];

int pv_get_uuid(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);
int pv_parse_uuid_name(pv_spec_p sp, str *in);

static pv_export_t mod_pvs[] = {
	{ {"uuid", (sizeof("uuid")-1)}, PVT_OTHER, pv_get_uuid,
		0, pv_parse_uuid_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{0, 0, 0}
};

struct module_exports exports = {
	"uuid",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* exported rpc functions */
	mod_pvs,         /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per child init function */
	mod_destroy      /* destroy function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	uuid_generate(_k_uuid_val);

	_k_uuid_str[0] = '\0';
	uuid_unparse_lower(_k_uuid_val, _k_uuid_str);
	LM_DBG("uuid initialized - probing value [%s]\n", _k_uuid_str);
	uuid_clear(_k_uuid_val);
	_k_uuid_str[0] = '\0';

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	if (rank!=PROC_MAIN)
		return 0;

	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
	uuid_generate(_k_uuid_val);
	uuid_clear(_k_uuid_val);
}

/**
 * parse the name of the $uuid(name)
 */
int pv_parse_uuid_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;
	switch(in->s[0])
	{
		case 'g':
		case 'G':
			sp->pvp.pvn.u.isname.name.n = 0;
			break;
		case 'r':
		case 'R':
			sp->pvp.pvn.u.isname.name.n = 1;
			break;
		case 't':
		case 'T':
			sp->pvp.pvn.u.isname.name.n = 2;
			break;
		case 's':
		case 'S':
			sp->pvp.pvn.u.isname.name.n = 3;
			break;
		default:
			sp->pvp.pvn.u.isname.name.n = 0;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;
}

/**
 * return the value of $uuid(name)
 */
int pv_get_uuid(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(param==NULL)
		return -1;
	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			uuid_generate_random(_k_uuid_val);
		break;
		case 2:
			uuid_generate_time(_k_uuid_val);
		break;
		case 3:
#ifndef __OS_darwin
			if(uuid_generate_time_safe(_k_uuid_val)!=0) {
				LM_ERR("uuid not generated in a safe mode\n");
				return pv_get_null(msg, param, res);
			}
#else
			uuid_generate_time(_k_uuid_val);
#endif
		break;
		default:
			uuid_generate(_k_uuid_val);
	}
	uuid_unparse_lower(_k_uuid_val, _k_uuid_str);
	return pv_get_strzval(msg, param, res, _k_uuid_str);
}


/**
 * generate uuid value
 */
static int ksr_uuid_generate(char *out, int *len)
{
	if(out==NULL || len==NULL || *len<KSR_UUID_BSIZE) {
		return -1;
	}
	uuid_generate(_k_uuid_val);
	uuid_unparse_lower(_k_uuid_val, out);
	*len = strlen(out);
	return 0;
}

/**
 * generate uuid time value
 */
static int ksr_uuid_generate_time(char *out, int *len)
{
	if(out==NULL || len==NULL || *len<KSR_UUID_BSIZE) {
		return -1;
	}
#ifndef __OS_darwin
	if(uuid_generate_time_safe(_k_uuid_val)!=0) {
		LM_ERR("uuid not generated in a safe mode\n");
		return -1;
	}
#else
	uuid_generate_time(_k_uuid_val);
#endif

	uuid_unparse_lower(_k_uuid_val, out);
	*len = strlen(out);
	return 0;
}

/**
 * generate uuid random value
 */
static int ksr_uuid_generate_random(char *out, int *len)
{
	if(out==NULL || len==NULL || *len<KSR_UUID_BSIZE) {
		return -1;
	}
	uuid_generate_random(_k_uuid_val);
	uuid_unparse_lower(_k_uuid_val, out);
	*len = strlen(out);
	return 0;
}

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sruid_uuid_api_t sapi;
	memset(&sapi, 0, sizeof(sruid_uuid_api_t));
	sapi.fgenerate = ksr_uuid_generate;
	sapi.fgenerate_time = ksr_uuid_generate_time;
	sapi.fgenerate_random = ksr_uuid_generate_random;
	if(sruid_uuid_api_set(&sapi) < 0) {
		return -1;
	}
	return 0;
}
