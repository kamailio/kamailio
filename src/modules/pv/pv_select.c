/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/*!
 * \file
 * \brief Implementation for Select Pseudo-variables
 */

#include "../../select.h"
#include "pv_select.h"

int pv_parse_select_name(pv_spec_p sp, str *in)
{
	select_t *sel = 0;
	char c;
	char *p;
	if (in == NULL || in->s == NULL || sp == NULL)
		return -1;

	c = in->s[in->len];
	in->s[in->len] = '\0';
	p = in->s;
	if(parse_select(&p, &sel)<0)
	{
		LM_ERR("invalid select name [%.*s]\n",
				in->len, in->s);
		in->s[in->len] = c;
		return -1;
	}
	in->s[in->len] = c;
	sp->pvp.pvn.u.dname = (void*)sel;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	return 0;
}


int pv_get_select(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str s = {0, 0};
	select_t *sel = 0;

	sel = (select_t*)param->pvn.u.dname;

	if(sel==0 || run_select(&s, sel, msg)<0 || s.s==0)
		return pv_get_null(msg, param, res);
	return pv_get_strval(msg, param, res, &s);
}

