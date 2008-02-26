/*
 * $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History
 * -------
 *  2008-01-10	Initial version (Miklos)
 */
#include <stdio.h>

#include "../select.h"
#include "../ut.h"
#include "cfg_struct.h"
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

	sel->vname.s = (char *)pkg_malloc(sizeof(char)*vname->len);
	if (!sel->vname.s) goto error;
	memcpy(sel->vname.s, vname->s, vname->len);
	sel->vname.len = vname->len;

	sel->group_p = group_p;
	sel->var_p = var_p;

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

		if (cfg_lookup_var(&sel->gname, &sel->vname, &group, &var)) {
			LOG(L_ERR, "ERROR: cfg_parse_selects(): unknown variable: %.*s.%.*s\n",
				sel->gname.len, sel->gname.s,
				sel->vname.len, sel->vname.s);
			return -1;
		}
		*(sel->group_p) = (void *)group;
		*(sel->var_p) = (void *)var;
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
		if (s->n != 3) {
			LOG(L_ERR, "ERROR: select_cfg_var(): two parameters are expected\n");
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
	are read from private memory */
	p = *(group->handle) + var->offset;

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

	}
	return 0;
}
