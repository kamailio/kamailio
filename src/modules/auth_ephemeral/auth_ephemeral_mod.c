/*
 * Copyright (C) 2013 Crocodile RCS Ltd
 * Copyright (C) 2017 ng-voice GmbH
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */
#include "../../core/dprint.h"
#include "../../core/locking.h"
#include "../../core/mod_fix.h"
#include "../../core/sr_module.h"
#include "../../core/str.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"
#include "../../modules/auth/api.h"

#include "auth_ephemeral_mod.h"
#include "authorize.h"
#include "checks.h"

MODULE_VERSION

static int autheph_init_rpc(void);

static int mod_init(void);
static void destroy(void);

static int secret_param(modparam_t _type, void *_val);
struct secret *secret_list = NULL;
gen_lock_t *autheph_secret_lock = NULL;

autheph_username_format_t autheph_username_format = AUTHEPH_USERNAME_IETF;

autheph_sha_alg_t autheph_sha_alg = AUTHEPH_SHA1;

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
	{ "autheph_authenticate", (cmd_function) autheph_authenticate,
	  2, fixup_var_str_12, 0,
	  REQUEST_ROUTE },
	{ "autheph_check_from", (cmd_function) autheph_check_from0,
	  0, 0, 0,
	  REQUEST_ROUTE },
	{ "autheph_check_from", (cmd_function) autheph_check_from1,
	  1, fixup_var_str_1, 0,
	  REQUEST_ROUTE },
	{ "autheph_check_to", (cmd_function) autheph_check_to0,
	  0, 0, 0,
	  REQUEST_ROUTE },
	{ "autheph_check_to", (cmd_function) autheph_check_to1,
	  1, fixup_var_str_1, 0,
	  REQUEST_ROUTE },
	{ "autheph_check_timestamp", (cmd_function) autheph_check_timestamp,
	  1, fixup_var_str_1, 0,
	  REQUEST_ROUTE },
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]=
{
	{ "secret",		PARAM_STRING|USE_FUNC_PARAM,
	  (void *) secret_param },
	{ "username_format",	INT_PARAM,
	  &autheph_username_format },
	{ "sha_algorithm",	INT_PARAM,
	  &autheph_sha_alg },
	{0, 0, 0}
};

struct module_exports exports=
{
	"auth_ephemeral",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* Exported functions */
	params,			/* Exported parameters */
	0,			/* exported RPC methods */
	0,			/* exported pseudo-variables */
	0,			/* response function */
	mod_init,		/* module initialization function */
	0,			/* child initialization function */
	destroy			/* destroy function */
};

static int mod_init(void)
{
	bind_auth_s_t bind_auth;

	if (autheph_init_rpc()<0)
	{
		LM_ERR("registering RPC commands\n");
		return -1;
	}

	if (secret_list == NULL)
	{
		LM_ERR("secret modparam not set\n");
		return -1;
	}

	switch(autheph_username_format)
	{
	case AUTHEPH_USERNAME_NON_IETF:
		LM_WARN("the %d value for the username_format modparam is "
			"deprecated. You should update the web-service that "
			"generates credentials to use the format specified in "
			"draft-uberti-rtcweb-turn-rest.\n",
			autheph_username_format);
		/* Fall-thru */
	case AUTHEPH_USERNAME_IETF:
		break;

	default:
		LM_ERR("bad value for username_format modparam: %d\n",
			autheph_username_format);
		return -1;
	}

	bind_auth = (bind_auth_s_t) find_export("bind_auth_s", 0, 0);
	if (bind_auth)
	{
		if (bind_auth(&eph_auth_api) < 0)
		{
			LM_ERR("unable to bind to auth module\n");
			return -1;
		}
	}
	else
	{
		memset(&eph_auth_api, 0, sizeof(auth_api_s_t));
		LM_INFO("auth module not loaded - digest authentication and "
			"check functions will not be available\n");
	}

	return 0;
}

static void destroy(void)
{
	struct secret *secret_struct;

	if (secret_list != NULL)
	{
		SECRET_UNLOCK;
		SECRET_LOCK;
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
		SECRET_UNLOCK;
	}

	if (autheph_secret_lock != NULL)
	{
		lock_destroy(autheph_secret_lock);
		lock_dealloc((void *) autheph_secret_lock);
	}
}

static inline int add_secret(str _secret_key)
{
	struct secret *secret_struct;

	if (autheph_secret_lock == NULL)
	{
		autheph_secret_lock = lock_alloc();
		if (autheph_secret_lock == NULL)
		{
			LM_ERR("allocating lock\n");
			return -1;
		}
		if (lock_init(autheph_secret_lock) == 0)
		{
			LM_ERR("initialising lock\n");
			return -1;
		}
	}

	secret_struct = (struct secret *) shm_malloc(sizeof(struct secret));
	if (secret_struct == NULL)
	{
		LM_ERR("unable to allocate shared memory\n");
		return -1;
	}

	memset(secret_struct, 0, sizeof (struct secret));
	secret_struct->secret_key = _secret_key;
	SECRET_LOCK;
	if (secret_list != NULL)
	{
		secret_list->prev = secret_struct;
	}
	secret_struct->next = secret_list;
	secret_list = secret_struct;
	SECRET_UNLOCK;

	return 0;
}

static inline int rm_secret(int _id)
{
	int pos = 0;
	struct secret *secret_struct;

	if (secret_list == NULL)
	{
		LM_ERR("secret list empty\n");
		return -1;
	}

	SECRET_LOCK;
	secret_struct = secret_list;
	while (pos <= _id && secret_struct != NULL)
	{
		if (pos == _id)
		{
			if (secret_struct->prev != NULL)
			{
				secret_struct->prev->next = secret_struct->next;
			}
			if (secret_struct->next != NULL)
			{
				secret_struct->next->prev = secret_struct->prev;
			}
			if (pos == 0)
			{
				secret_list = secret_struct->next;
			}
			SECRET_UNLOCK;
			shm_free(secret_struct->secret_key.s);
			shm_free(secret_struct);
			return 0;
		}

		pos++;
		secret_struct = secret_struct->next;

	}
	SECRET_UNLOCK;

	LM_ERR("ID %d not found\n", _id);
	return -1;
}

static int secret_param(modparam_t _type, void *_val)
{
	str sval;

	if (_val == NULL)
	{
		LM_ERR("bad parameter\n");
		return -1;
	}

	LM_INFO("adding %s to secret list\n", (char *) _val);

	sval.len = strlen((char *) _val);
	sval.s = (char *) shm_malloc(sizeof(char) * sval.len);
	if (sval.s == NULL)
	{
		LM_ERR("unable to allocate shared memory\n");
		return -1;
	}
	memcpy(sval.s, (char *) _val, sval.len);

	return add_secret(sval);
}

void autheph_rpc_dump_secrets(rpc_t* rpc, void* ctx)
{
	int pos = 0;
	struct secret *secret_struct = secret_list;

	SECRET_LOCK;
	while (secret_struct != NULL)
	{
		if (rpc->rpl_printf(ctx,
					"ID %d: %.*s", pos++,
					secret_struct->secret_key.len,
					secret_struct->secret_key.s) < 0)
		{
			rpc->fault(ctx, 500, "Faiure building the response");
			SECRET_UNLOCK;
			return;
		}
		secret_struct = secret_struct->next;
	}
	SECRET_UNLOCK;
}

void autheph_rpc_add_secret(rpc_t* rpc, void* ctx)
{
	str tval;
	str sval;

	if(rpc->scan(ctx, "S", &tval)<1) {
		LM_WARN("not enough parameters\n");
		rpc->fault(ctx, 500, "Not enough parameters");
		return;
	}
	sval.len = tval.len;
	sval.s = shm_malloc(sizeof(char) * sval.len);
	if (sval.s == NULL)
	{
		LM_ERR("Unable to allocate shared memory\n");
		rpc->fault(ctx, 500, "Not enough memory");
		return;
	}
	memcpy(sval.s, tval.s, sval.len);

	if (add_secret(sval) != 0) {
		LM_ERR("failed adding secret\n");
		rpc->fault(ctx, 500, "Failed adding secret");
		return;
	}
}

void autheph_rpc_rm_secret(rpc_t* rpc, void* ctx)
{
	unsigned int id;
	if(rpc->scan(ctx, "d", (int*)(&id))<1) {
		LM_WARN("no id parameter\n");
		rpc->fault(ctx, 500, "Not enough parameters");
		return;
	}
	if (rm_secret(id) != 0) {
		LM_ERR("failed removing secret\n");
		rpc->fault(ctx, 500, "Failed removing secret");
		return;
	}
}

static const char* autheph_rpc_dump_secrets_doc[2] = {
	"List secret tokens",
	0
};

static const char* autheph_rpc_add_secret_doc[2] = {
	"Add a secret",
	0
};

static const char* autheph_rpc_rm_secret_doc[2] = {
	"Remove a secret",
	0
};

rpc_export_t autheph_rpc_cmds[] = {
	{"autheph.dump_secrets", autheph_rpc_dump_secrets,
		autheph_rpc_dump_secrets_doc, RET_ARRAY},
	{"autheph.add_secret", autheph_rpc_add_secret,
		autheph_rpc_add_secret_doc, 0},
	{"autheph.rm_secret", autheph_rpc_rm_secret,
		autheph_rpc_rm_secret_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
static int autheph_init_rpc(void)
{
	if (rpc_register_array(autheph_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_auth_ephemeral_exports[] = {
	{ str_init("auth_ephemeral"), str_init("autheph_check"),
		SR_KEMIP_INT, ki_autheph_check,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth_ephemeral"), str_init("autheph_www"),
		SR_KEMIP_INT, ki_autheph_www,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth_ephemeral"), str_init("autheph_www_method"),
		SR_KEMIP_INT, ki_autheph_www_method,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth_ephemeral"), str_init("autheph_proxy"),
		SR_KEMIP_INT, ki_autheph_proxy,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth_ephemeral"), str_init("autheph_authenticate"),
		SR_KEMIP_INT, ki_autheph_authenticate,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_auth_ephemeral_exports);
	return 0;
}
