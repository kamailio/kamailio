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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "../../parser/parse_uri.h"
#include "../../dset.h"
#include "../../onsend.h"
#include "../../socket_info.h"

#include "pv_core.h"
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
	str ruid;
	str location_ua;

	/* get the index */
	if(pv_get_spec_index(msg, param, &idx, &idxf)!=0)
	{
		LM_ERR("invalid index\n");
		return pv_get_null(msg, param, res);
	}

	uri.s = get_branch(idx, &uri.len, &lq, &duri, &path, &fl, &fsocket, &ruid, 0, &location_ua);

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
		case 7: /* ruid */
			if(ruid.len==0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &ruid);
		case 8: /* location_ua */
			if(location_ua.len==0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &location_ua);
		default:
			/* 0 - uri */
			return pv_get_strval(msg, param, res, &uri);
	}

	return 0;
}

int pv_set_branchx(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	int idx = 0;
	int idxf = 0;
	branch_t *br;
	struct socket_info *si;
	int port, proto;
	str host;
	char backup;

	if(msg==NULL || param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	/* get the index */
	if(pv_get_spec_index(msg, param, &idx, &idxf)!=0)
	{
		LM_ERR("invalid index\n");
		return -1;
	}

	br = get_sip_branch(idx);

	if(br==NULL)
	{
		LM_DBG("no branch to operate on\n");
		return -1;
	}

	switch(param->pvn.u.isname.name.n)
	{
		case 1: /* dst uri */
			if(val==NULL || (val->flags&PV_VAL_NULL))
			{
				br->dst_uri[0] = '\0';
				br->dst_uri_len = 0;
				break;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("str value required to set branch dst uri\n");
				return -1;
			}
			if(val->rs.len<=0)
			{
				br->dst_uri[0] = '\0';
				br->dst_uri_len = 0;
				break;
			}

			if (unlikely(val->rs.len > MAX_URI_SIZE - 1))
			{
				LM_ERR("too long dst uri: %.*s\n",
								val->rs.len, val->rs.s);
				return -1;
			}
			memcpy(br->dst_uri, val->rs.s, val->rs.len);
			br->dst_uri[val->rs.len] = 0;
			br->dst_uri_len = val->rs.len;
		break;
		case 2: /* path */
			if(val==NULL || (val->flags&PV_VAL_NULL))
			{
				br->path[0] = '\0';
				br->path_len = 0;
				break;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("str value required to set branch path\n");
				return -1;
			}
			if(val->rs.len<=0)
			{
				br->path[0] = '\0';
				br->path_len = 0;
				break;
			}

			if (unlikely(val->rs.len > MAX_PATH_SIZE - 1))
			{
				LM_ERR("path too long: %.*s\n",
							val->rs.len, val->rs.s);
				return -1;
			}
			memcpy(br->path, val->rs.s, val->rs.len);
			br->path[val->rs.len] = 0;
			br->path_len = val->rs.len;
		break;
		case 3: /* Q */
			if(val==NULL || (val->flags&PV_VAL_NULL))
			{
				br->q = Q_UNSPECIFIED;
				break;
			}
			if(!(val->flags&PV_VAL_INT))
			{
				LM_ERR("int value required to set branch q\n");
				return -1;
			}
			br->q = val->ri;
		break;
		case 4: /* send socket */
			if(val==NULL || (val->flags&PV_VAL_NULL))
			{
				br->force_send_socket = NULL;
				break;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("str value required to set branch send sock\n");
				return -1;
			}
			if(val->rs.len<=0)
			{
				br->force_send_socket = NULL;
				break;
			}
			backup = val->rs.s[val->rs.len];
			val->rs.s[val->rs.len] = '\0';
			if (parse_phostport(val->rs.s, &host.s, &host.len, &port,
						&proto) < 0)
			{
				LM_ERR("invalid socket specification\n");
				val->rs.s[val->rs.len] = backup;
				return -1;
			}
			val->rs.s[val->rs.len] = backup;
			si = grep_sock_info(&host, (unsigned short)port,
					(unsigned short)proto);
			if (si!=NULL)
			{
				br->force_send_socket = si;
			} else {
				LM_WARN("no socket found to match [%.*s]\n",
					val->rs.len, val->rs.s);
				br->force_send_socket = NULL;
			}
		break;
		case 5: /* count */
			/* do nothing - cannot set the branch counter */
		break;
		case 6: /* flags */
			if(val==NULL || (val->flags&PV_VAL_NULL))
			{
				br->flags = 0;
				break;
			}
			if(!(val->flags&PV_VAL_INT))
			{
				LM_ERR("int value required to set branch flags\n");
				return -1;
			}
			br->flags = val->ri;
		break;
		case 7: /* ruid */
			/* do nothing - cannot set the ruid */
		break;
		case 8: /* location_ua */
			/* do nothing - cannot set the location_ua */
		break;
		default:
			/* 0 - uri */
			if(val==NULL || (val->flags&PV_VAL_NULL))
			{
				drop_sip_branch(idx);
			} else {
				if(!(val->flags&PV_VAL_STR))
				{
					LM_ERR("str value required to set branch uri\n");
					return -1;
				}
				if(val->rs.len<=0)
				{
					drop_sip_branch(idx);
				} else {
					if (unlikely(val->rs.len > MAX_URI_SIZE - 1))
					{
						LM_ERR("too long r-uri: %.*s\n",
								val->rs.len, val->rs.s);
						return -1;
					}
					memcpy(br->uri, val->rs.s, val->rs.len);
					br->uri[val->rs.len] = 0;
					br->len = val->rs.len;
				}
			}
	}

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
			else if (strncmp(in->s, "ruid", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
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
			else if(strncmp(in->s, "location_ua", 11)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
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
	LM_ERR("unknown PV branch name %.*s\n", in->len, in->s);
	return -1;
}

int pv_get_sndfrom(struct sip_msg *msg, pv_param_t *param,
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

int pv_get_sndto(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct onsend_info* snd_inf;
	struct ip_addr ip;
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
					(int)su_getport(snd_inf->to));
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
			su2ip_addr(&ip, snd_inf->to);
			s.s = ip_addr2a(&ip);
			s.len = strlen(s.s);
			return pv_get_strval(msg, param, res, &s);
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
	LM_ERR("unknown PV snd name %.*s\n", in->len, in->s);
	return -1;
}

int pv_get_nh(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct sip_uri parsed_uri;
	str uri;

	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesnt have r/d-uri */
		return pv_get_null(msg, param, res);

    if (msg->dst_uri.s != NULL && msg->dst_uri.len>0)
	{
		uri = msg->dst_uri;
	} else {
		if (msg->new_uri.s!=NULL && msg->new_uri.len>0)
		{
			uri = msg->new_uri;
		} else {
			uri = msg->first_line.u.request.uri;
		}
	}
	if(param->pvn.u.isname.name.n==0) /* uri */
	{
		return pv_get_strval(msg, param, res, &uri);
	}
	if(parse_uri(uri.s, uri.len, &parsed_uri)!=0)
	{
		LM_ERR("failed to parse nh uri [%.*s]\n", uri.len, uri.s);
		return pv_get_null(msg, param, res);
	}
	if(param->pvn.u.isname.name.n==1) /* username */
	{
		if(parsed_uri.user.s==NULL || parsed_uri.user.len<=0)
			return pv_get_null(msg, param, res);
		return pv_get_strval(msg, param, res, &parsed_uri.user);
	} else if(param->pvn.u.isname.name.n==2) /* domain */ {
		if(parsed_uri.host.s==NULL || parsed_uri.host.len<=0)
			return pv_get_null(msg, param, res);
		return pv_get_strval(msg, param, res, &parsed_uri.host);
	} else if(param->pvn.u.isname.name.n==3) /* port */ {
		if(parsed_uri.port.s==NULL)
			return pv_get_5060(msg, param, res);
		return pv_get_strintval(msg, param, res, &parsed_uri.port,
				(int)parsed_uri.port_no);
	} else if(param->pvn.u.isname.name.n==4) /* protocol */ {
		if(parsed_uri.transport_val.s==NULL)
			return pv_get_udp(msg, param, res);
		return pv_get_strintval(msg, param, res, &parsed_uri.transport_val,
				(int)parsed_uri.proto);
	}
	LM_ERR("unknown specifier\n");
	return pv_get_null(msg, param, res);
}

int pv_parse_nh_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 1:
			if(strncmp(in->s, "u", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "U", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "d", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "p", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else if(strncmp(in->s, "P", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV nh name %.*s\n", in->len, in->s);
	return -1;
}

