/* 
 * $Id$
 *
 * Copyright (C) 2013 Crocodile RCS Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../modules/auth/api.h"

#include "autheph_mod.h"
#include "authorize.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);

static int secret_param(modparam_t type, void* param);
struct secret *secret_list = NULL;

auth_api_s_t eph_auth_api;

static cmd_export_t cmds[]=
{
	{ "autheph_check", (cmd_function) autheph_check,
	  1, fixup_var_str_1, 0,
	  REQUEST_ROUTE },
	{ "autheph_www", (cmd_function) autheph_www,
	  1, fixup_var_str_1, 0,
	  REQUEST_ROUTE },
	{ "autheph_www", (cmd_function) autheph_www2,
	  2, fixup_var_str_12, 0,
	  REQUEST_ROUTE },
	{ "autheph_proxy", (cmd_function) autheph_proxy,
	  1, fixup_var_str_1, 0,
	  REQUEST_ROUTE },

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]=
{
	{ "secret",		STR_PARAM|USE_FUNC_PARAM,
	  (void *) secret_param },
	{0, 0, 0}
};

struct module_exports exports=
{
	"auth_ephemeral", 
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* Exported functions */
	params,			/* Exported parameters */
	0,			/* exported statistics */
	0,			/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,		/* module initialization function */
	0,			/* response function */
	destroy,		/* destroy function */
	0			/* child initialization function */
};

static int mod_init(void)
{
	bind_auth_s_t bind_auth;

	if (secret_list == NULL)
	{
		LM_ERR("secret modparam not set\n");
		return -1;
	}

	bind_auth = (bind_auth_s_t) find_export("bind_auth_s", 0, 0);
	if (!bind_auth)
	{
		LM_ERR("unable to find bind_auth function. Check if you have"
			" loaded the auth module.\n");
		return -2;
	}

	if (bind_auth(&eph_auth_api) < 0)
	{
		LM_ERR("unable to bind to auth module\n");
		return -3;
	}

	return 0;
}

static void destroy(void)
{
	struct secret *secret_struct;

	while (secret_list != NULL)
	{
		secret_struct = secret_list;
		secret_list = secret_struct->next;

		if (secret_struct->secret_key.s != NULL)
		{
			shm_free(secret_struct->secret_key.s);
		}
		shm_free(secret_struct);
	}
}

static int add_secret(str secret_key)
{
	struct secret *secret_struct;

	secret_struct = (struct secret *) shm_malloc(sizeof(struct secret));
	if (secret_struct == NULL)
	{
		LM_ERR("unable to allocate shared memory\n");
		return -1;
	}

	memset(secret_struct, 0, sizeof (struct secret));
	secret_struct->next = secret_list;
	secret_list = secret_struct;
	secret_struct->secret_key = secret_key;

	return 0;
}

static int secret_param(modparam_t type, void *val)
{
	str sval;

	if (val == NULL)
	{
		LM_ERR("bad parameter\n");
		return -1;
	}

	LM_INFO("adding %s to secret list\n", (char *) val);

	sval.len = strlen((char *) val);
	sval.s = (char *) shm_malloc(sizeof(char) * sval.len);
	if (sval.s == NULL)
	{
		LM_ERR("unable to allocate shared memory\n");
		return -1;
	}
	memcpy(sval.s, (char *) val, sval.len);

	return add_secret(sval);
}
