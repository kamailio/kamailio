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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */
#include "../../dprint.h"
#include "../../locking.h"
#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../lib/kmi/mi.h"
#include "../../modules/auth/api.h"

#include "autheph_mod.h"
#include "authorize.h"
#include "checks.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);

static int secret_param(modparam_t _type, void *_val);
struct secret *secret_list = NULL;
static struct mi_root *mi_dump_secrets(struct mi_root *cmd, void *param);
static struct mi_root *mi_add_secret(struct mi_root *cmd, void *param);
static struct mi_root *mi_rm_secret(struct mi_root *cmd, void *param);
gen_lock_t *autheph_secret_lock = NULL;

autheph_username_format_t autheph_username_format = AUTHEPH_USERNAME_IETF;

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
	{0, 0, 0}
};

static mi_export_t mi_cmds[] =
{
	{ "autheph.add_secret",		mi_add_secret,	0, 0, 0 },
	{ "autheph.dump_secrets",	mi_dump_secrets,0, 0, 0 },
	{ "autheph.rm_secret",		mi_rm_secret,	0, 0, 0 },

	{ 0, 0, 0, 0, 0 }
};

struct module_exports exports=
{
	"auth_ephemeral", 
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* Exported functions */
	params,			/* Exported parameters */
	0,			/* exported statistics */
	mi_cmds,		/* exported MI functions */
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

	if (register_mi_mod(exports.name, mi_cmds) != 0)
	{
		LM_ERR("registering MI commands\n");
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

static str str_status_too_many_params = str_init("Too many parameters");
static str str_status_empty_param = str_init("Not enough parameters");
static str str_status_no_memory = str_init("Unable to allocate shared memory");
static str str_status_string_error = str_init("Error converting string to int");
static str str_status_adding_secret = str_init("Error adding secret");
static str str_status_removing_secret = str_init("Error removing secret");

static struct mi_root *mi_dump_secrets(struct mi_root *cmd, void *param)
{
	int pos = 0;
	struct secret *secret_struct = secret_list;
	struct mi_root *rpl_tree;

	if (cmd->node.kids != NULL)
	{
		LM_WARN("too many parameters\n");
		return init_mi_tree(400, str_status_too_many_params.s,
					str_status_too_many_params.len);
	}

	rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree == NULL)
		return 0;

	SECRET_LOCK;
	while (secret_struct != NULL)
	{
		if (addf_mi_node_child(&rpl_tree->node, 0, 0, 0,
					"ID %d: %.*s", pos++,
					secret_struct->secret_key.len,
					secret_struct->secret_key.s) == 0)
		{
			free_mi_tree(rpl_tree);
			SECRET_UNLOCK;
			return 0;
		}
		secret_struct = secret_struct->next;
	}
	SECRET_UNLOCK;

	return rpl_tree;
}

static struct mi_root *mi_add_secret(struct mi_root *cmd, void *param)
{
	str sval;
	struct mi_node *node = NULL;

	node = cmd->node.kids;
	if (node == NULL)
	{
		LM_WARN("no secret parameter\n");
		return init_mi_tree(400, str_status_empty_param.s,
						str_status_empty_param.len);
	}

	if (node->value.s == NULL || node->value.len == 0)
	{
		LM_WARN("empty secret parameter\n");
		return init_mi_tree(400, str_status_empty_param.s,
					str_status_empty_param.len);
	}

	if (node->next != NULL)
	{
		LM_WARN("too many parameters\n");
		return init_mi_tree(400, str_status_too_many_params.s,
					str_status_too_many_params.len);
	}

	sval.len = node->value.len;
	sval.s = shm_malloc(sizeof(char) * sval.len);
	if (sval.s == NULL)
	{
		LM_ERR("Unable to allocate shared memory\n");
		return init_mi_tree(400, str_status_no_memory.s,
					str_status_no_memory.len);
	}
	memcpy(sval.s, node->value.s, sval.len);

	if (add_secret(sval) == 0)
	{
		return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	}
	else
	{
		LM_ERR("Adding secret\n");
		return init_mi_tree(400, str_status_adding_secret.s,
					str_status_adding_secret.len);
	}
}

static struct mi_root *mi_rm_secret(struct mi_root *cmd, void *param)
{
	unsigned int id;
	struct mi_node *node = NULL;

	node = cmd->node.kids;
	if (node == NULL)
	{
		LM_WARN("no id parameter\n");
		return init_mi_tree(400, str_status_empty_param.s,
						str_status_empty_param.len);
	}

	if (node->value.s == NULL || node->value.len == 0)
	{
		LM_WARN("empty id parameter\n");
		return init_mi_tree(400, str_status_empty_param.s,
					str_status_empty_param.len);
	}

	if (str2int(&node->value, &id) < 0)
	{
		LM_ERR("converting string to int\n");
		return init_mi_tree(400, str_status_string_error.s,
					str_status_string_error.len);
	}

	if (node->next != NULL)
	{
		LM_WARN("too many parameters\n");
		return init_mi_tree(400, str_status_too_many_params.s,
					str_status_too_many_params.len);
	}

	if (rm_secret(id) == 0)
	{
		return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	}
	else
	{
		LM_ERR("Removing secret\n");
		return init_mi_tree(400, str_status_removing_secret.s,
					str_status_removing_secret.len);
	}

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}
