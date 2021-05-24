/*
 * Copyright (C) 2007 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../core/sr_module.h"
#include "../../core/cfg/cfg.h"
#include "../../core/cfg/cfg_ctx.h"
#include "../../core/rpc.h"

MODULE_VERSION

static cfg_ctx_t *ctx = NULL;

/* module initialization function */
static int mod_init(void)
{
	if(cfg_register_ctx(&ctx, NULL)) {
		LOG(L_ERR, "cfg_rpc: failed to register cfg context\n");
		return -1;
	}

	return 0;
}

static const char *rpc_set_now_doc[2] = {
		"Set the value of a configuration variable and commit the change "
		"immediately",
		0};

static void rpc_set_now_int(rpc_t *rpc, void *c)
{
	str group, var;
	int i;
	unsigned int *group_id;

	if(rpc->scan(c, "SSd", &group, &var, &i) < 3)
		return;

	if(cfg_get_group_id(&group, &group_id)) {
		rpc->fault(c, 400,
				"Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if(cfg_set_now_int(ctx, &group, group_id, &var, i)) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static void rpc_set_now_string(rpc_t *rpc, void *c)
{
	str group, var;
	char *ch;
	unsigned int *group_id;

	if(rpc->scan(c, "SSs", &group, &var, &ch) < 3)
		return;

	if(cfg_get_group_id(&group, &group_id)) {
		rpc->fault(c, 400,
				"Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if(cfg_set_now_string(ctx, &group, group_id, &var, ch)) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static void rpc_set(rpc_t *rpc, void *c)
{
	str group, var;
	int i, err;
	char *ch;
	unsigned int *group_id;

	if(rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if(cfg_get_group_id(&group, &group_id)) {
		rpc->fault(c, 400,
				"Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if(rpc->scan(c, "d", &i) == 1)
		err = cfg_set_now_int(ctx, &group, group_id, &var, i);
	else if(rpc->scan(c, "s", &ch) == 1)
		err = cfg_set_now_string(ctx, &group, group_id, &var, ch);
	else
		return; /* error */

	if(err) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static const char *rpc_del_now_doc[2] = {
		"Delete the value of a configuration variable from a group instance "
		"and commit the change immediately",
		0};

static void rpc_del(rpc_t *rpc, void *c)
{
	str group, var;
	unsigned int *group_id;

	if(rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if(cfg_get_group_id(&group, &group_id) || !group_id) {
		rpc->fault(c, 400, "Wrong group syntax. Use \"group[id]\"");
		return;
	}

	if(cfg_del_now(ctx, &group, group_id, &var)) {
		rpc->fault(c, 400, "Failed to delete the value");
		return;
	}
}

static const char *rpc_set_delayed_doc[2] = {
		"Prepare the change of a configuration variable, but does not commit "
		"the new value yet",
		0};

static void rpc_set_delayed_int(rpc_t *rpc, void *c)
{
	str group, var;
	int i;
	unsigned int *group_id;

	if(rpc->scan(c, "SSd", &group, &var, &i) < 3)
		return;

	if(cfg_get_group_id(&group, &group_id)) {
		rpc->fault(c, 400,
				"Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if(cfg_set_delayed_int(ctx, &group, group_id, &var, i)) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static void rpc_set_delayed_string(rpc_t *rpc, void *c)
{
	str group, var;
	char *ch;
	unsigned int *group_id;

	if(rpc->scan(c, "SSs", &group, &var, &ch) < 3)
		return;

	if(cfg_get_group_id(&group, &group_id)) {
		rpc->fault(c, 400,
				"Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if(cfg_set_delayed_string(ctx, &group, group_id, &var, ch)) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static void rpc_set_delayed(rpc_t *rpc, void *c)
{
	str group, var;
	int i, err;
	char *ch;
	unsigned int *group_id;

	if(rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if(cfg_get_group_id(&group, &group_id)) {
		rpc->fault(c, 400,
				"Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if(rpc->scan(c, "d", &i) == 1)
		err = cfg_set_delayed_int(ctx, &group, group_id, &var, i);
	else if(rpc->scan(c, "s", &ch) == 1)
		err = cfg_set_delayed_string(ctx, &group, group_id, &var, ch);
	else
		return; /* error */

	if(err) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static const char *rpc_del_delayed_doc[2] = {
		"Prepare the deletion of the value of a configuration variable from a "
		"group instance, but does not commit the change yet",
		0};

static void rpc_del_delayed(rpc_t *rpc, void *c)
{
	str group, var;
	unsigned int *group_id;

	if(rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if(cfg_get_group_id(&group, &group_id) || !group_id) {
		rpc->fault(c, 400, "Wrong group syntax. Use \"group[id]\"");
		return;
	}

	if(cfg_del_delayed(ctx, &group, group_id, &var)) {
		rpc->fault(c, 400, "Failed to delete the value");
		return;
	}
}

static const char *rpc_commit_doc[2] = {
		"Commit the previously prepared configuration changes", 0};

static void rpc_commit(rpc_t *rpc, void *c)
{
	if(cfg_commit(ctx)) {
		rpc->fault(c, 400, "Failed to commit the changes");
		return;
	}
}

static const char *rpc_rollback_doc[2] = {
		"Drop the prepared configuration changes", 0};

static void rpc_rollback(rpc_t *rpc, void *c)
{
	if(cfg_rollback(ctx)) {
		rpc->fault(c, 400, "Failed to drop the changes");
		return;
	}
}

static const char *rpc_get_doc[2] = {
		"Get the value of a configuration variable", 0};

static void rpc_get(rpc_t *rpc, void *c)
{
	str group, var;
	void *val;
	unsigned int val_type;
	int ret, n;
	unsigned int *group_id;
	void *rh = NULL;

	n = rpc->scan(c, "S*S", &group, &var);
	/*  2: both group and variable name are present
	 * -1: only group is present, print all variables in the group */
	if(n < 1) {
		rpc->fault(c, 500, "Failed to get the parameters");
		return;
	}
	if(n == 1) {
		var.s = NULL;
		var.len = 0;
	}
	if(cfg_get_group_id(&group, &group_id)) {
		rpc->fault(c, 400,
				"Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}
	if(var.len != 0) {
		LM_DBG("getting value for variable: %.*s.%.*s\n", group.len, group.s,
				var.len, var.s);
		/* print value for one variable */
		val = NULL;
		ret = cfg_get_by_name(ctx, &group, group_id, &var, &val, &val_type);
		if(ret < 0) {
			rpc->fault(c, 400, "Failed to get the variable");
			return;
		} else if(ret > 0) {
			rpc->fault(c, 400,
					"Variable exists, but it is not readable via RPC "
					"interface");
			return;
		}
		switch(val_type) {
			case CFG_VAR_INT:
				rpc->add(c, "d", (int)(long)val);
				break;
			case CFG_VAR_STRING:
				rpc->add(c, "s", (char *)val);
				break;
			case CFG_VAR_STR:
				rpc->add(c, "S", (str *)val);
				break;
			case CFG_VAR_POINTER:
				rpc->rpl_printf(c, "%p", val);
				break;
		}
	} else {
		/* print values for all variables in the group */
		void *h;
		str gname;
		cfg_def_t *def;
		int i;
		char pbuf[32];
		int plen;
		LM_DBG("getting values for group: %.*s\n", group.len, group.s);
		rpc->add(c, "{", &rh);
		if(rh == NULL) {
			LM_ERR("failed to add root structure\n");
			rpc->fault(c, 500, "Failed to add root structure");
			return;
		}
		cfg_get_group_init(&h);
		while(cfg_get_group_next(&h, &gname, &def)) {
			if(((gname.len == group.len)
					   && (memcmp(gname.s, group.s, group.len) == 0))) {
				for(i = 0; def[i].name; i++) {
					var.s = def[i].name;
					var.len = (int)strlen(def[i].name);
					LM_DBG("getting value for variable: %.*s.%.*s\n", group.len,
							group.s, var.len, var.s);
					val = NULL;
					ret = cfg_get_by_name(
							ctx, &group, group_id, &var, &val, &val_type);
					if(ret < 0) {
						rpc->fault(c, 400, "Failed to get the variable");
						return;
					} else if(ret > 0) {
						LM_DBG("skipping dynamic (callback) value for variable:"
							   " %.*s.%.*s (%p/%d)\n",
								group.len, group.s, var.len, var.s, val, ret);
						continue;
					}
					switch(val_type) {
						case CFG_VAR_INT:
							rpc->struct_add(rh, "d", var.s, (int)(long)val);
							break;
						case CFG_VAR_STRING:
							rpc->struct_add(rh, "s", var.s, (char *)val);
							break;
						case CFG_VAR_STR:
							rpc->struct_add(rh, "S", var.s, (str *)val);
							break;
						case CFG_VAR_POINTER:
							plen = snprintf(pbuf, 32, "%p", val);
							if(plen > 0 && plen < 32) {
								rpc->struct_add(rh, "s", var.s, pbuf);
							} else {
								LM_ERR("error adding: %.*s.%s\n", group.len,
										group.s, var.s);
							}
							break;
					}
				}
			}
		}
	}
}
static const char *rpc_cfg_var_reset_doc[2] = {
		"Reset all the values of a configuration group and commit the change "
		"immediately",
		0};

static void rpc_cfg_var_reset(rpc_t *rpc, void *c)
{
	void *h;
	str gname, var;
	cfg_def_t *def;
	void *val;
	int i, ret;
	str group;
	char *ch;
	unsigned int *group_id;
	unsigned int val_type;
	unsigned int input_type;

	if(rpc->scan(c, "S", &group) < 1)
		return;

	if(cfg_get_group_id(&group, &group_id)) {
		rpc->fault(c, 400,
				"Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	cfg_get_group_init(&h);
	while(cfg_get_group_next(&h, &gname, &def))
		if(((gname.len == group.len)
				   && (memcmp(gname.s, group.s, group.len) == 0))) {
			for(i = 0; def[i].name; i++) {

				var.s = def[i].name;
				var.len = (int)strlen(def[i].name);
				ret = cfg_get_default_value_by_name(
						ctx, &gname, group_id, &var, &val, &val_type);

				if(ret != 0)
					continue;

				if(cfg_help(ctx, &group, &var, &ch, &input_type)) {
					rpc->fault(
							c, 400, "Failed to get the variable description");
					return;
				}

				if(input_type == CFG_INPUT_INT) {
					ret = cfg_set_now_int(
							ctx, &gname, group_id, &var, (int)(long)val);
				} else if(input_type == CFG_INPUT_STRING) {
					ret = cfg_set_now_string(ctx, &gname, group_id, &var, val);
				} else {
					rpc->fault(c, 500, "Unsupported input type");
					return;
				}
				if(ret < 0) {
					rpc->fault(c, 500, "Reset failed");
					return;
				} else if(ret == 1) {
					LM_WARN("unexpected situation - variable not found\n");
				}
			}
		}
}


static const char *rpc_help_doc[2] = {
		"Print the description of a configuration variable", 0};

static void rpc_help(rpc_t *rpc, void *c)
{
	str group, var;
	char *ch;
	unsigned int input_type;

	if(rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if(cfg_help(ctx, &group, &var, &ch, &input_type)) {
		rpc->fault(c, 400, "Failed to get the variable description");
		return;
	}
	rpc->add(c, "s", ch);

	switch(input_type) {
		case CFG_INPUT_INT:
			rpc->rpl_printf(c, "(parameter type is integer)");
			break;

		case CFG_INPUT_STRING:
		case CFG_INPUT_STR:
			rpc->rpl_printf(c, "(parameter type is string)");
			break;
	}
}

static const char *rpc_list_doc[2] = {"List the configuration variables", 0};

static void rpc_list(rpc_t *rpc, void *c)
{
	void *h;
	str gname;
	cfg_def_t *def;
	int i;
	str group;

	if(rpc->scan(c, "*S", &group) < 1) {
		group.s = NULL;
		group.len = 0;
	}

	cfg_get_group_init(&h);
	while(cfg_get_group_next(&h, &gname, &def))
		if(!group.len
				|| ((gname.len == group.len)
						&& (memcmp(gname.s, group.s, group.len) == 0)))
			for(i = 0; def[i].name; i++)
				rpc->rpl_printf(c, "%.*s: %s", gname.len, gname.s, def[i].name);
}

static const char *rpc_diff_doc[2] = {"List the pending configuration changes "
									  "that have not been committed yet",
		0};

static void rpc_diff(rpc_t *rpc, void *c)
{
	void *h;
	str gname, vname;
	unsigned int *gid;
	void *old_val, *new_val;
	unsigned int val_type;
	void *rpc_handle;
	int err;


	if(cfg_diff_init(ctx, &h)) {
		rpc->fault(c, 400, "Failed to get the changes");
		return;
	}
	while((err = cfg_diff_next(
				   &h, &gname, &gid, &vname, &old_val, &new_val, &val_type))
			> 0) {
		rpc->add(c, "{", &rpc_handle);
		if(gid)
			rpc->struct_add(rpc_handle, "SdS", "group name", &gname, "group id",
					*gid, "variable name", &vname);
		else
			rpc->struct_add(rpc_handle, "SS", "group name", &gname,
					"variable name", &vname);

		switch(val_type) {
			case CFG_VAR_INT:
				rpc->struct_add(rpc_handle, "dd", "old value",
						(int)(long)old_val, "new value", (int)(long)new_val);
				break;

			case CFG_VAR_STRING:
				rpc->struct_add(rpc_handle, "ss", "old value", (char *)old_val,
						"new value", (char *)new_val);
				break;

			case CFG_VAR_STR:
				rpc->struct_add(rpc_handle, "SS", "old value", (str *)old_val,
						"new value", (str *)new_val);
				break;

			case CFG_VAR_POINTER:
				/* cannot print out pointer value with struct_add() */
				break;
		}
	}
	cfg_diff_release(ctx);
	if(err)
		rpc->fault(c, 400, "Failed to get the changes");
}

static const char *rpc_add_group_inst_doc[2] = {
		"Add a new instance to an existing configuration group", 0};

static void rpc_add_group_inst(rpc_t *rpc, void *c)
{
	str group;
	unsigned int *group_id;

	if(rpc->scan(c, "S", &group) < 1)
		return;

	if(cfg_get_group_id(&group, &group_id) || !group_id) {
		rpc->fault(c, 400, "Wrong group syntax. Use \"group[id]\"");
		return;
	}

	if(cfg_add_group_inst(ctx, &group, *group_id)) {
		rpc->fault(c, 400, "Failed to add the group instance");
		return;
	}
}

static const char *rpc_del_group_inst_doc[2] = {
		"Delete an instance of a configuration group", 0};

static void rpc_del_group_inst(rpc_t *rpc, void *c)
{
	str group;
	unsigned int *group_id;

	if(rpc->scan(c, "S", &group) < 1)
		return;

	if(cfg_get_group_id(&group, &group_id) || !group_id) {
		rpc->fault(c, 400, "Wrong group syntax. Use \"group[id]\"");
		return;
	}

	if(cfg_del_group_inst(ctx, &group, *group_id)) {
		rpc->fault(c, 400, "Failed to delete the group instance");
		return;
	}
}

/* clang-format off */
static rpc_export_t rpc_calls[] = {{"cfg.set", rpc_set, rpc_set_now_doc, 0},
	{"cfg.set_now_int", rpc_set_now_int, rpc_set_now_doc, 0},
	{"cfg.seti", rpc_set_now_int, rpc_set_now_doc, 0},
	{"cfg.set_now_string", rpc_set_now_string, rpc_set_now_doc, 0},
	{"cfg.sets", rpc_set_now_string, rpc_set_now_doc, 0},
	{"cfg.del", rpc_del, rpc_del_now_doc, 0},
	{"cfg.set_delayed", rpc_set_delayed, rpc_set_delayed_doc, 0},
	{"cfg.set_delayed_int", rpc_set_delayed_int, rpc_set_delayed_doc, 0},
	{"cfg.set_delayed_string", rpc_set_delayed_string, rpc_set_delayed_doc, 0},
	{"cfg.del_delayed", rpc_del_delayed, rpc_del_delayed_doc, 0},
	{"cfg.commit", rpc_commit, rpc_commit_doc, 0},
	{"cfg.rollback", rpc_rollback, rpc_rollback_doc, 0},
	{"cfg.get", rpc_get, rpc_get_doc, 0},
	{"cfg.reset", rpc_cfg_var_reset, rpc_cfg_var_reset_doc, 0},
	{"cfg.help", rpc_help, rpc_help_doc, 0},
	{"cfg.list", rpc_list, rpc_list_doc, RET_ARRAY},
	{"cfg.diff", rpc_diff, rpc_diff_doc, 0},
	{"cfg.add_group_inst", rpc_add_group_inst, rpc_add_group_inst_doc, 0},
	{"cfg.del_group_inst", rpc_del_group_inst, rpc_del_group_inst_doc, 0},
	{0, 0, 0, 0}
};
/* clang-format on */

/* Module interface */
/* clang-format off */
struct module_exports exports = {
	"cfg_rpc",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	0,					/* exported functions */
	0,					/* exported parameters */
	rpc_calls,			/* RPC methods */
	0,					/* exported pseudo-variables */
	0,					/* response function */
	mod_init,			/* module initialization function */
	0,					/* child initialization function */
	0					/* destroy function */
};
/* clang-format on */
