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
 * \brief Implementation for Stats Pseudo-variables
 */

#include "../../lib/kcore/statistics.h"
#include "../../ver.h"
#include "pv_stats.h"

/**
 *
 */
int pv_parse_stat_name(pv_spec_p sp, str *in)
{
	if (in == NULL || in->s == NULL || sp == NULL)
		return -1;
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *in;
	return 0;
}


/**
 *
 */
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

/**
 *
 */
int pv_parse_sr_version_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3:
			if(strncmp(in->s, "num", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 4:
			if(strncmp(in->s, "full", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "hash", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV version name %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
int pv_get_sr_version(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	if(param==NULL)
		return -1;

	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			return pv_get_strzval(msg, param, res, (char*)full_version);
		case 2:
			return pv_get_strzval(msg, param, res, (char*)ver_id);
		default:
			return pv_get_strzval(msg, param, res, (char*)ver_version);
	}
}

