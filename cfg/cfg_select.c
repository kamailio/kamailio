/*
 * Copyright (C) 2008 iptelorg GmbH
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
#include <stdio.h>

#include "../select.h"
#include "../ut.h"
#include "cfg_struct.h"
#include "cfg_ctx.h"
#include "cfg_select.h"

/* It may happen that the select calls cannot be fixed up before shmizing
 * the config, because for example the mapping structures have not been
 * allocated for the dynamic groups yet. So we have to keep track of all the
 * selects that we failed to fix-up, and retry the fixup once more just
 * before forking */
typedef struct _cfg_selects {
	str	gname;
	str	vname;
	void	**group_p;
	void	**var_p;
	struct _cfg_selects	*next;
} cfg_selects_t;

/* linked list of non-fixed selects */
static cfg_selects_t	*cfg_non_fixed_selects = NULL;

/* add a new select item to the linked list */
static int cfg_new_select(str *gname, str *vname, void **group_p, void **var_p)
{
	cfg_selects_t	*sel;

	sel = (cfg_selects_t *)pkg_malloc(sizeof(cfg_selects_t));
	if (!sel) goto error;
	memset(sel, 0, sizeof(cfg_selects_t));

	sel->gname.s = (char *)pkg_malloc(sizeof(char)*gname->len);
	if (!sel->gname.s) goto error;
	memcpy(sel->gname.s, gname->s, gname->len);
	sel->gname.len = gname->len;
	sel->group_p = group_p;

	if (vname) {
		sel->vname.s = (char *)pkg_malloc(sizeof(char)*vname->len);
		if (!sel->vname.s) goto error;
		memcpy(sel->vname.s, vname->s, vname->len);
		sel->vname.len = vname->len;

		sel->var_p = var_p;
	}


	sel->next = cfg_non_fixed_selects;
	cfg_non_fixed_selects = sel;

	return 0;

error:
	LOG(L_ERR, "ERROR: cfg_new_select(): not enough memory\n");
	if (sel) {
		if (sel->gname.s) pkg_free(sel->gname.s);
		if (sel->vname.s) pkg_free(sel->vname.s);
		pkg_free(sel);
	}
	return -1;
}

/* free the list of not yet fixed selects */
void cfg_free_selects()
{
	cfg_selects_t	*sel, *next_sel;

	sel = cfg_non_fixed_selects;
	while (sel) {
		next_sel = sel->next;

		if (sel->gname.s) pkg_free(sel->gname.s);
		if (sel->vname.s) pkg_free(sel->vname.s);
		pkg_free(sel);

		sel = next_sel;
	}
	cfg_non_fixed_selects = NULL;
}

/* fix-up the select calls */
int cfg_fixup_selects()
{
	cfg_selects_t	*sel;
	cfg_group_t	*group;
	cfg_mapping_t	*var;

	for (sel=cfg_non_fixed_selects; sel; sel=sel->next) {

		if (sel->var_p) {
			if (cfg_lookup_var(&sel->gname, &sel->vname, &group, &var)) {
				LOG(L_ERR, "ERROR: cfg_parse_selects(): unknown variable: %.*s.%.*s\n",
					sel->gname.len, sel->gname.s,
					sel->vname.len, sel->vname.s);
				return -1;
			}
			*(sel->group_p) = (void *)group;
			*(sel->var_p) = (void *)var;
		} else {
			if (!(group = cfg_lookup_group(sel->gname.s, sel->gname.len))) {
				LOG(L_ERR, "ERROR: cfg_parse_selects(): unknown configuration group: %.*s\n",
					sel->gname.len, sel->gname.s);
				return -1;
			}
			*(sel->group_p) = (void *)group;
		}
	}
	/* the select list is not needed anymore */
	cfg_free_selects();
	return 0;
}

int select_cfg_var(str *res, select_t *s, struct sip_msg *msg)
{
	cfg_group_t	*group;
	cfg_mapping_t	*var;
	void		*p;
	int		i;
	static char	buf[INT2STR_MAX_LEN];

	if (msg == NULL) {
		/* fixup call */

		/* two parameters are mandatory, group name and variable name */
		if (s->n < 3) {
			LOG(L_ERR, "ERROR: select_cfg_var(): At least two parameters are expected\n");
			return -1;
		}

		if ((s->params[1].type != SEL_PARAM_STR)
		|| (s->params[2].type != SEL_PARAM_STR)) {
			LOG(L_ERR, "ERROR: select_cfg_var(): string parameters are expected\n");
			return -1;
		}

		/* look-up the group and the variable */
		if (cfg_lookup_var(&s->params[1].v.s, &s->params[2].v.s, &group, &var)) {
			if (cfg_shmized) {
				LOG(L_ERR, "ERROR: select_cfg_var(): unknown variable: %.*s.%.*s\n",
					s->params[1].v.s.len, s->params[1].v.s.s,
					s->params[2].v.s.len, s->params[2].v.s.s);
				return -1;
			}
			/* The variable was not found, add it to the non-fixed select list.
			 * So we act as if the fixup was successful, and we retry it later */
			if (cfg_new_select(&s->params[1].v.s, &s->params[2].v.s,
						&s->params[1].v.p, &s->params[2].v.p))
				return -1;

			LOG(L_DBG, "DEBUG: select_cfg_var(): select fixup is postponed: %.*s.%.*s\n",
				s->params[1].v.s.len, s->params[1].v.s.s,
				s->params[2].v.s.len, s->params[2].v.s.s);

			s->params[1].type = SEL_PARAM_PTR;
			s->params[1].v.p = NULL;

			s->params[2].type = SEL_PARAM_PTR;
			s->params[2].v.p = NULL;

			return 0;
		}

		if (var->def->on_change_cb) {
			/* fixup function is defined -- safer to return an error
			than an incorrect value */
			LOG(L_ERR, "ERROR: select_cfg_var(): variable cannot be retrieved\n");
			return -1;
		}

		s->params[1].type = SEL_PARAM_PTR;
		s->params[1].v.p = (void *)group;

		s->params[2].type = SEL_PARAM_PTR;
		s->params[2].v.p = (void *)var;
		return 1;
	}

	group = (cfg_group_t *)s->params[1].v.p;
	var = (cfg_mapping_t *)s->params[2].v.p;

	if (!group || !var) return -1;

	/* use the module's handle to access the variable, so the variables
	are read from the local config */
	p = *(group->handle) + var->offset;
	if (p == NULL)
		return -1;	/* The group is not yet ready.
				 * (Trying to read the value from the
				 * main process that has no local configuration) */

	switch (CFG_VAR_TYPE(var)) {
	case CFG_VAR_INT:
		i = *(int *)p;
		res->len = snprintf(buf, sizeof(buf)-1, "%d", i);
		buf[res->len] = '\0';
		res->s = buf;
		break;

	case CFG_VAR_STRING:
		res->s = *(char **)p;
		res->len = (res->s) ? strlen(res->s) : 0;
		break;

	case CFG_VAR_STR:
		memcpy(res, p, sizeof(str));
		break;

	default:
		LOG(L_DBG, "DEBUG: select_cfg_var(): unsupported variable type: %d\n",
			CFG_VAR_TYPE(var));
		return -1;
	}
	return 0;
}

/* fake function to eat the first parameter of @cfg_get */
ABSTRACT_F(select_cfg_var1)

/* fix-up function for read_cfg_var()
 *
 * return value:
 * >0 - success
 *  0 - the variable has not been declared yet, but it will be automatically
 *	fixed-up later.
 * <0 - error
 */
int read_cfg_var_fixup(char *gname, char *vname, struct cfg_read_handle *read_handle)
{
	cfg_group_t	*group;
	cfg_mapping_t	*var;
	str		group_name, var_name;

	if (!gname || !vname || !read_handle)
		return -1;

	group_name.s = gname;
	group_name.len = strlen(gname);
	var_name.s = vname;
	var_name.len = strlen(vname);

	/* look-up the group and the variable */
	if (cfg_lookup_var(&group_name, &var_name, &group, &var)) {
		if (cfg_shmized) {
			LOG(L_ERR, "ERROR: read_cfg_var_fixup(): unknown variable: %.*s.%.*s\n",
				group_name.len, group_name.s,
				var_name.len, var_name.s);
			return -1;
		}
		/* The variable was not found, add it to the non-fixed select list.
		 * So we act as if the fixup was successful, and we retry it later */
		if (cfg_new_select(&group_name, &var_name,
					&read_handle->group, &read_handle->var))
			return -1;

		LOG(L_DBG, "DEBUG: read_cfg_var_fixup(): cfg read fixup is postponed: %.*s.%.*s\n",
			group_name.len, group_name.s,
			var_name.len, var_name.s);

		read_handle->group = NULL;
		read_handle->var = NULL;
		return 0;
	}

	read_handle->group = (void *)group;
	read_handle->var = (void *)var;
	return 1;
}

/* read the value of a variable via a group and variable name previously fixed up
 * Returns the type of the variable
 */
unsigned int read_cfg_var(struct cfg_read_handle *read_handle, void **val)
{
	cfg_group_t	*group;
	cfg_mapping_t	*var;
	void		*p;
	static str	s;

	if (!val || !read_handle || !read_handle->group || !read_handle->var)
		return 0;

	group = (cfg_group_t *)(read_handle->group);
	var = (cfg_mapping_t *)(read_handle->var);


	/* use the module's handle to access the variable, so the variables
	are read from the local config */
	p = *(group->handle) + var->offset;
	if (p == NULL)
		return 0;	/* The group is not yet ready.
				 * (Trying to read the value from the
				 * main process that has no local configuration) */

	switch (CFG_VAR_TYPE(var)) {
	case CFG_VAR_INT:
		*val = (void *)(long)*(int *)p;
		break;

	case CFG_VAR_STRING:
		*val = (void *)*(char **)p;
		break;

	case CFG_VAR_STR:
		memcpy(&s, p, sizeof(str));
		*val = (void *)&s;
		break;

	case CFG_VAR_POINTER:
		*val = *(void **)p;
		break;

	}
	return CFG_VAR_TYPE(var);
}

/* wrapper function for read_cfg_var() -- convert the value to integer
 * returns -1 on error, 0 on success
 */
int read_cfg_var_int(struct cfg_read_handle *read_handle, int *val)
{
	unsigned int	type;
	void		*v1=NULL, *v2=NULL;

	if ((type = read_cfg_var(read_handle, &v1)) == 0)
		return -1;

	if (convert_val(type, v1, CFG_INPUT_INT, &v2))
		return -1;

	*val = (int)(long)(v2);
	return 0;
}

/* wrapper function for read_cfg_var() -- convert the value to str
 * returns -1 on error, 0 on success
 */
int read_cfg_var_str(struct cfg_read_handle *read_handle, str *val)
{
	unsigned int	type;
	void		*v1=NULL, *v2=NULL;

	if ((type = read_cfg_var(read_handle, &v1)) == 0)
		return -1;

	if (convert_val(type, v1, CFG_INPUT_STR, &v2))
		return -1;

	*val = *(str *)(v2);
	return 0;
}

/* return the selected group instance */
int cfg_selected_inst(str *res, select_t *s, struct sip_msg *msg)
{
	cfg_group_t	*group;
	cfg_group_inst_t	*inst;

	if (msg == NULL) {
		/* fixup call */

		/* one parameter is mandatory: group name */
		if (s->n != 2) {
			LOG(L_ERR, "ERROR: selected_inst(): One parameter is expected\n");
			return -1;
		}

		if (s->params[1].type != SEL_PARAM_STR) {
			LOG(L_ERR, "ERROR: selected_inst(): string parameter is expected\n");
			return -1;
		}

		/* look-up the group and the variable */
		if (!(group = cfg_lookup_group(s->params[1].v.s.s, s->params[1].v.s.len))) {
			if (cfg_shmized) {
				LOG(L_ERR, "ERROR: selected_inst(): unknown configuration group: %.*s\n",
					s->params[1].v.s.len, s->params[1].v.s.s);
				return -1;
			}
			/* The group was not found, add it to the non-fixed select list.
			 * So we act as if the fixup was successful, and we retry it later */
			if (cfg_new_select(&s->params[1].v.s, NULL,
						&s->params[1].v.p, NULL))
				return -1;

			LOG(L_DBG, "DEBUG: selected_inst(): select fixup is postponed: %.*s\n",
				s->params[1].v.s.len, s->params[1].v.s.s);

			s->params[1].type = SEL_PARAM_PTR;
			s->params[1].v.p = NULL;

			return 0;
		}

		s->params[1].type = SEL_PARAM_PTR;
		s->params[1].v.p = (void *)group;

		return 1;
	}

	group = (cfg_group_t *)s->params[1].v.p;
	if (!group) return -1;

	/* Get the current group instance from the group handle. */
	inst = CFG_HANDLE_TO_GINST(*(group->handle));

	if (inst) {
		res->s = int2str(inst->id, &res->len);
	} else {
		res->s = "";
		res->len = 0;
	}
	return 0;
}

