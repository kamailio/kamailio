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
			LOG(L_ERR, "ERROR: select_cfg_var(): unknown variable\n");
			return -1;
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

	/* use the module's handle to access the variable, so the variables
	are read from private memory */
	p = *(group->handle) + var->offset;

	switch (CFG_VAR_TYPE(var)) {
	case CFG_VAR_INT:
		memcpy(&i, p, sizeof(int));
		res->len = snprintf(buf, sizeof(buf)-1, "%d", i);
		buf[res->len] = '\0';
		res->s = buf;
		break;

	case CFG_VAR_STRING:
		memcpy(&res->s, p, sizeof(char *));
		res->len = (res->s) ? strlen(res->s) : 0;
		break;

	case CFG_VAR_STR:
		memcpy(res, p, sizeof(str));
		break;

	}
	return 0;
}
