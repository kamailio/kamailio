/*
 * $Id$
 *
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
 * History
 * -------
 *  2007-12-03	Initial version (Miklos)
 */

#include "../../sr_module.h"
#include "../../cfg/cfg.h"
#include "../../cfg/cfg_ctx.h"
#include "../../rpc.h"

static cfg_ctx_t	*ctx = NULL;

MODULE_VERSION

/* module initialization function */
static int mod_init(void)
{
	if (cfg_register_ctx(&ctx, NULL)) {
		LOG(L_ERR, "cfg_rpc: failed to register cfg context\n");
		return -1;
	}

	return 0;
}

/* Set the group_id pointer based on the group string.
 * The string is either "group_name", or "group_name[group_id]"
 * *group_id is set to null in the former case.
 * Warning: changes the group string
 */
static int get_group_id(str *group, unsigned int **group_id)
{
	static unsigned int	id;
	str	s;

	if (!group->s || (group->s[group->len-1] != ']')) {
		*group_id = NULL;
		return 0;
	}

	s.s = group->s + group->len - 2;
	s.len = 0;
	while ((s.s > group->s) && (*s.s != '[')) {
		s.s--;
		s.len++;
	}
	if (s.s == group->s) /* '[' not found */
		return -1;
	group->len = s.s - group->s;
	s.s++;
	if (!group->len || !s.len)
		return -1;
	if (str2int(&s, &id))
		return -1;

	*group_id = &id;
	return 0;
}

static const char* rpc_set_now_doc[2] = {
        "Set the value of a configuration variable and commit the change immediately",
        0
};

static void rpc_set_now_int(rpc_t* rpc, void* c)
{
	str	group, var;
	int	i;
	unsigned int	*group_id;

	if (rpc->scan(c, "SSd", &group, &var, &i) < 3)
		return;

	if (get_group_id(&group, &group_id)) {
		rpc->fault(c, 400, "Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if (cfg_set_now_int(ctx, &group, group_id, &var, i)) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static void rpc_set_now_string(rpc_t* rpc, void* c)
{
	str	group, var;
	char	*ch;
	unsigned int	*group_id;

	if (rpc->scan(c, "SSs", &group, &var, &ch) < 3)
		return;

	if (get_group_id(&group, &group_id)) {
		rpc->fault(c, 400, "Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if (cfg_set_now_string(ctx, &group, group_id, &var, ch)) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static void rpc_set(rpc_t* rpc, void* c)
{
	str	group, var;
	int	i, err;
	char	*ch;
	unsigned int	*group_id;

	if (rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if (get_group_id(&group, &group_id)) {
		rpc->fault(c, 400, "Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if (rpc->scan(c, "d", &i) == 1)
		err = cfg_set_now_int(ctx, &group, group_id, &var, i);
	else if (rpc->scan(c, "s", &ch) == 1)
		err = cfg_set_now_string(ctx, &group, group_id, &var, ch);
	else
		return; /* error */

	if (err) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static const char* rpc_del_now_doc[2] = {
        "Delete the value of a configuration variable from a group instance and commit the change immediately",
        0
};

static void rpc_del(rpc_t* rpc, void* c)
{
	str	group, var;
	unsigned int	*group_id;

	if (rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if (get_group_id(&group, &group_id) || !group_id) {
		rpc->fault(c, 400, "Wrong group syntax. Use \"group[id]\"");
		return;
	}

	if (cfg_del_now(ctx, &group, group_id, &var)) {
		rpc->fault(c, 400, "Failed to delete the value");
		return;
	}
}

static const char* rpc_set_delayed_doc[2] = {
        "Prepare the change of a configuration variable, but does not commit the new value yet",
        0
};

static void rpc_set_delayed_int(rpc_t* rpc, void* c)
{
	str	group, var;
	int	i;
	unsigned int	*group_id;

	if (rpc->scan(c, "SSd", &group, &var, &i) < 3)
		return;

	if (get_group_id(&group, &group_id)) {
		rpc->fault(c, 400, "Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if (cfg_set_delayed_int(ctx, &group, group_id, &var, i)) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static void rpc_set_delayed_string(rpc_t* rpc, void* c)
{
	str	group, var;
	char	*ch;
	unsigned int	*group_id;

	if (rpc->scan(c, "SSs", &group, &var, &ch) < 3)
		return;

	if (get_group_id(&group, &group_id)) {
		rpc->fault(c, 400, "Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if (cfg_set_delayed_string(ctx, &group, group_id, &var, ch)) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static void rpc_set_delayed(rpc_t* rpc, void* c)
{
	str	group, var;
	int	i, err;
	char	*ch;
	unsigned int	*group_id;

	if (rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if (get_group_id(&group, &group_id)) {
		rpc->fault(c, 400, "Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	if (rpc->scan(c, "d", &i) == 1)
		err = cfg_set_delayed_int(ctx, &group, group_id, &var, i);
	else if (rpc->scan(c, "s", &ch) == 1)
		err = cfg_set_delayed_string(ctx, &group, group_id, &var, ch);
	else
		return; /* error */

	if (err) {
		rpc->fault(c, 400, "Failed to set the variable");
		return;
	}
}

static const char* rpc_del_delayed_doc[2] = {
        "Prepare the deletion of the value of a configuration variable from a group instance, but does not commit the change yet",
        0
};

static void rpc_del_delayed(rpc_t* rpc, void* c)
{
	str	group, var;
	unsigned int	*group_id;

	if (rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if (get_group_id(&group, &group_id) || !group_id) {
		rpc->fault(c, 400, "Wrong group syntax. Use \"group[id]\"");
		return;
	}

	if (cfg_del_delayed(ctx, &group, group_id, &var)) {
		rpc->fault(c, 400, "Failed to delete the value");
		return;
	}
}

static const char* rpc_commit_doc[2] = {
        "Commit the previously prepared configuration changes",
        0
};

static void rpc_commit(rpc_t* rpc, void* c)
{
	if (cfg_commit(ctx)) {
		rpc->fault(c, 400, "Failed to commit the changes");
		return;
	}
}

static const char* rpc_rollback_doc[2] = {
        "Drop the prepared configuration changes",
        0
};

static void rpc_rollback(rpc_t* rpc, void* c)
{
	if (cfg_rollback(ctx)) {
		rpc->fault(c, 400, "Failed to drop the changes");
		return;
	}
}

static const char* rpc_get_doc[2] = {
        "Get the value of a configuration variable",
        0
};

static void rpc_get(rpc_t* rpc, void* c)
{
	str	group, var;
	void	*val;
	unsigned int	val_type;
	int	ret;
	unsigned int	*group_id;

	if (rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if (get_group_id(&group, &group_id)) {
		rpc->fault(c, 400, "Wrong group syntax. Use either \"group\", or \"group[id]\"");
		return;
	}

	ret = cfg_get_by_name(ctx, &group, group_id, &var,
			&val, &val_type);
	if (ret < 0) {
		rpc->fault(c, 400, "Failed to get the variable");
		return;
		
	} else if (ret > 0) {
		rpc->fault(c, 400, "Variable exists, but it is not readable via RPC interface");
		return;
	}

	switch (val_type) {
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

}

static const char* rpc_help_doc[2] = {
        "Print the description of a configuration variable",
        0
};

static void rpc_help(rpc_t* rpc, void* c)
{
	str	group, var;
	char	*ch;
	unsigned int	input_type;

	if (rpc->scan(c, "SS", &group, &var) < 2)
		return;

	if (cfg_help(ctx, &group, &var,
			&ch, &input_type)
	) {
		rpc->fault(c, 400, "Failed to get the variable description");
		return;
	}
	rpc->add(c, "s", ch);

	switch (input_type) {
	case CFG_INPUT_INT:
		rpc->rpl_printf(c, "(parameter type is integer)");
		break;

	case CFG_INPUT_STRING:
	case CFG_INPUT_STR:
		rpc->rpl_printf(c, "(parameter type is string)");
		break;
	}	
}

static const char* rpc_list_doc[2] = {
        "List the configuration variables",
        0
};

static void rpc_list(rpc_t* rpc, void* c)
{
	void		*h;
	str		gname;
	cfg_def_t	*def;
	int		i;
	str		group;

	if (rpc->scan(c, "*S", &group) < 1) {
		group.s = NULL;
		group.len = 0;
	}

	cfg_get_group_init(&h);
	while(cfg_get_group_next(&h, &gname, &def))
		if (!group.len
			|| ((gname.len == group.len)
				&& (memcmp(gname.s, group.s, group.len) == 0))
		)
			for (i=0; def[i].name; i++)
				rpc->rpl_printf(c, "%.*s: %s", gname.len, gname.s, def[i].name);
}

static const char* rpc_diff_doc[2] = {
        "List the pending configuration changes that have not been committed yet",
        0
};

static void rpc_diff(rpc_t* rpc, void* c)
{
	void		*h;
	str		gname, vname;
	unsigned int	*gid;
	void		*old_val, *new_val;
	unsigned int	val_type;
	void		*rpc_handle;
	int		err;


	if (cfg_diff_init(ctx, &h)) {
		rpc->fault(c, 400, "Failed to get the changes");
		return;
	}
	while((err = cfg_diff_next(&h,
			&gname, &gid, &vname,
			&old_val, &new_val,
			&val_type)) > 0
	) {
		rpc->add(c, "{", &rpc_handle);
		if (gid)
			rpc->struct_add(rpc_handle, "SdS",
					"group name", &gname,
					"group id", *gid,
					"variable name", &vname);
		else
			rpc->struct_add(rpc_handle, "SS",
					"group name", &gname,
					"variable name", &vname);

		switch (val_type) {
		case CFG_VAR_INT:
			rpc->struct_add(rpc_handle, "dd",
					"old value", (int)(long)old_val,
					"new value", (int)(long)new_val);
			break;

		case CFG_VAR_STRING:
			rpc->struct_add(rpc_handle, "ss",
					"old value", (char *)old_val,
					"new value", (char *)new_val);
			break;

		case CFG_VAR_STR:
			rpc->struct_add(rpc_handle, "SS",
					"old value", (str *)old_val,
					"new value", (str *)new_val);
			break;

		case CFG_VAR_POINTER:
			/* cannot print out pointer value with struct_add() */
			break;

		}
	}
	cfg_diff_release(ctx);
	if (err)
		rpc->fault(c, 400, "Failed to get the changes");
}

static const char* rpc_add_group_inst_doc[2] = {
	"Add a new instance to an existing configuration group",
	0
};

static void rpc_add_group_inst(rpc_t* rpc, void* c)
{
	str	group;
	unsigned int	*group_id;

	if (rpc->scan(c, "S", &group) < 1)
		return;

	if (get_group_id(&group, &group_id) || !group_id) {
		rpc->fault(c, 400, "Wrong group syntax. Use \"group[id]\"");
		return;
	}

	if (cfg_add_group_inst(ctx, &group, *group_id)) {
		rpc->fault(c, 400, "Failed to add the group instance");
		return;
	}
}

static const char* rpc_del_group_inst_doc[2] = {
	"Delte an instance of a configuration group",
	0
};

static void rpc_del_group_inst(rpc_t* rpc, void* c)
{
	str	group;
	unsigned int	*group_id;

	if (rpc->scan(c, "S", &group) < 1)
		return;

	if (get_group_id(&group, &group_id) || !group_id) {
		rpc->fault(c, 400, "Wrong group syntax. Use \"group[id]\"");
		return;
	}

	if (cfg_del_group_inst(ctx, &group, *group_id)) {
		rpc->fault(c, 400, "Failed to delete the group instance");
		return;
	}
}

static rpc_export_t rpc_calls[] = {
	{"cfg.set",		rpc_set,		rpc_set_now_doc,	0},
	{"cfg.set_now_int",	rpc_set_now_int,	rpc_set_now_doc,	0},
	{"cfg.seti",	rpc_set_now_int,	rpc_set_now_doc,	0},
	{"cfg.set_now_string",	rpc_set_now_string,	rpc_set_now_doc,	0},
	{"cfg.sets",	rpc_set_now_string,	rpc_set_now_doc,	0},
	{"cfg.del",		rpc_del,		rpc_del_now_doc,	0},
	{"cfg.set_delayed",	rpc_set_delayed,	rpc_set_delayed_doc,	0},
	{"cfg.set_delayed_int",	rpc_set_delayed_int,	rpc_set_delayed_doc,	0},
	{"cfg.set_delayed_string",	rpc_set_delayed_string,	rpc_set_delayed_doc,	0},
	{"cfg.del_delayed",	rpc_del_delayed,	rpc_del_delayed_doc,	0},
	{"cfg.commit",		rpc_commit,		rpc_commit_doc,		0},
	{"cfg.rollback",	rpc_rollback,		rpc_rollback_doc,	0},
	{"cfg.get",		rpc_get,		rpc_get_doc,		0},
	{"cfg.help",		rpc_help,		rpc_help_doc,		0},
	{"cfg.list",		rpc_list,		rpc_list_doc,		0},
	{"cfg.diff",		rpc_diff,		rpc_diff_doc,		0},
	{"cfg.add_group_inst",	rpc_add_group_inst,	rpc_add_group_inst_doc,	0},	
	{"cfg.del_group_inst",	rpc_del_group_inst,	rpc_del_group_inst_doc,	0},	
	{0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
	"cfg_rpc",
	0,		/* Exported functions */
	rpc_calls,	/* RPC methods */
	0,		/* Exported parameters */
	mod_init,	/* module initialization function */
	0,		/* response function */
	0,		/* destroy function */
	0,		/* oncancel function */
	0		/* child initialization function */
};
