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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../pvar.h"

MODULE_VERSION

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static uuid_t _k_uuid_val;
static char   _k_uuid_str[40];

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
	"uuid",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	mod_pvs,        /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
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
			if(uuid_generate_time_safe(_k_uuid_val))
				return pv_get_null(msg, param, res);
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
