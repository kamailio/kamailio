/*
 * $Id$
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief Implementation for Stats Pseudo-variables
 */

#include "../../lib/kcore/statistics.h"
#include "pv_stats.h"

int pv_parse_stat_name(pv_spec_p sp, str *in)
{
	if (in == NULL || in->s == NULL || sp == NULL)
		return -1;
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *in;
	return 0;
}


int pv_get_stat(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	stat_var *stat;

	stat = get_stat(&param->pvn.u.isname.name.s);
	if (stat == NULL) {
		LM_WARN("No stat variable ``%.*s''\n",
			param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);
		return pv_get_null(msg, param, res);
	}
	return pv_get_uintval(msg, param, res,
			(unsigned int)get_stat_val(stat));
}

