/*
 * $Id$
 *
 * Copyright (C) 2008 Daniel-Constantin Mierla (asipto.com)
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


#include "../../dset.h"
#include "../../onsend.h"

#include "pv_branch.h"

int pv_get_branchx(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int idx = 0;
	int idxf = 0;
	str uri;
	str duri;
	int lq = 0;
	str path;
	unsigned int fl = 0;
	struct socket_info* fsocket = NULL;

	/* get the index */
	if(pv_get_spec_index(msg, param, &idx, &idxf)!=0)
	{
		LM_ERR("invalid index\n");
		return pv_get_null(msg, param, res);
	}

	uri.s = get_branch(idx, &uri.len, &lq, &duri, &path, &fl, &fsocket);

	/* branch(count) doesn't need a valid branch, everything else does */
	if(uri.s == 0 && ( param->pvn.u.isname.name.n != 5/* count*/ ))
	{
		LM_ERR("error accessing branch [%d]\n", idx);
		return pv_get_null(msg, param, res);
	}

	switch(param->pvn.u.isname.name.n)
	{
		case 1: /* dst uri */
			if(duri.len==0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &duri);
		case 2: /* path */
			if(path.len==0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &path);
		case 3: /* Q */
			if(lq == Q_UNSPECIFIED)
				return pv_get_null(msg, param, res);
			return pv_get_sintval(msg, param, res, lq);
		case 4: /* send socket */
			if(fsocket!=0)
				return pv_get_strval(msg, param, res, &fsocket->sock_str);
			return pv_get_null(msg, param, res);
		case 5: /* count */
			return pv_get_uintval(msg, param, res, nr_branches);
		case 6: /* flags */
			return pv_get_uintval(msg, param, res, fl);
		default:
			/* 0 - uri */
			return pv_get_strval(msg, param, res, &uri);
	}

	return 0;
}

int pv_set_branchx(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	/* tbd */
	return 0;
}

int pv_parse_branchx_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3: 
			if(strncmp(in->s, "uri", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 7: 
			if(strncmp(in->s, "dst_uri", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 4: 
			if(strncmp(in->s, "path", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		case 1: 
			if(*in->s=='q' || *in->s=='Q')
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		case 11: 
			if(strncmp(in->s, "send_socket", 11)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else goto error;
		break;
		case 5: 
			if(strncmp(in->s, "count", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "flags", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV time name %.*s\n", in->len, in->s);
	return -1;
}

int pv_get_snd(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct onsend_info* snd_inf;
	str s;

	snd_inf=get_onsend_info();
	if (! likely(snd_inf && snd_inf->send_sock))
		return pv_get_null(msg, param, res);

	switch(param->pvn.u.isname.name.n)
	{
		case 1: /* af */
			return pv_get_uintval(msg, param, res,
					(int)snd_inf->send_sock->address.af);
		case 2: /* port */
			return pv_get_uintval(msg, param, res,
					(int)snd_inf->send_sock->port_no);
		case 3: /* proto */
			return pv_get_uintval(msg, param, res,
					(int)snd_inf->send_sock->proto);
		case 4: /* buf */
			s.s   = snd_inf->buf;
			s.len = snd_inf->len;
			return pv_get_strval(msg, param, res, &s);
		case 5: /* len */
			return pv_get_uintval(msg, param, res,
					(int)snd_inf->len);
		default:
			/* 0 - ip */
			return pv_get_strval(msg, param, res,
					&snd_inf->send_sock->address_str);
	}

	return 0;
}

int pv_parse_snd_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 2:
			if(strncmp(in->s, "ip", 2)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "af", 2)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 3:
			if(strncmp(in->s, "buf", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else if(strncmp(in->s, "len", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else goto error;
		break;
		case 4:
			if(strncmp(in->s, "port", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		case 5:
			if(strncmp(in->s, "proto", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV time name %.*s\n", in->len, in->s);
	return -1;
}

