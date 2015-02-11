/*
 * Copyright (C) 2007 Elena-Ramona Modroiu
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

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "../../dprint.h"
#include "../../globals.h"
#include "../../pvar.h"

#include "pv_time.h"

int pv_parse_time_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3: 
			if(strncmp(in->s, "sec", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "min", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "mon", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else goto error;
		break;
		case 4: 
			if(strncmp(in->s, "hour", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "mday", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else if(strncmp(in->s, "year", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "wday", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else if(strncmp(in->s, "yday", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
			else goto error;
		break;
		case 5: 
			if(strncmp(in->s, "isdst", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
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

int pv_parse_strftime_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	sp->pvp.pvn.u.isname.name.s.s = as_asciiz(in);
	if(sp->pvp.pvn.u.isname.name.s.s==NULL)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	sp->pvp.pvn.u.isname.name.s.len = in->len;
#if 0
	/* to-do: free function for pv name structure */
	sp->pvp.pvn.nfree = pkg_free;
#endif
	return 0;
}

static struct tm _cfgutils_ts;
static msg_ctx_id_t _cfgutils_msgid = { 0 };

/**
 * return broken-down time attributes
 */
int pv_get_time(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || param==NULL)
		return -1;

	if(msg_ctx_id_match(msg, &_cfgutils_msgid)!=1)
	{
		msg_set_time(msg);
		msg_ctx_id_set(msg, &_cfgutils_msgid);
		if(localtime_r(&msg->tval.tv_sec, &_cfgutils_ts) == NULL)
		{
			LM_ERR("unable to break time to attributes\n");
			return -1;
		}
	}
	
	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			return pv_get_uintval(msg, param, res, (unsigned int)_cfgutils_ts.tm_min);
		case 2:
			return pv_get_uintval(msg, param, res, (unsigned int)_cfgutils_ts.tm_hour);
		case 3:
			return pv_get_uintval(msg, param, res, (unsigned int)_cfgutils_ts.tm_mday);
		case 4:
			return pv_get_uintval(msg, param, res, 
					(unsigned int)(_cfgutils_ts.tm_mon+1));
		case 5:
			return pv_get_uintval(msg, param, res,
					(unsigned int)(_cfgutils_ts.tm_year+1900));
		case 6:
			return pv_get_uintval(msg, param, res, 
					(unsigned int)(_cfgutils_ts.tm_wday+1));
		case 7:
			return pv_get_uintval(msg, param, res, 
					(unsigned int)(_cfgutils_ts.tm_yday+1));
		case 8:
			return pv_get_sintval(msg, param, res, _cfgutils_ts.tm_isdst);
		default:
			return pv_get_uintval(msg, param, res, (unsigned int)_cfgutils_ts.tm_sec);
	}
}

/**
 * return strftime() formatted time
 */
int pv_get_strftime(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str s;
#define PV_STRFTIME_BUF_SIZE	64
	static char _pv_strftime_buf[PV_STRFTIME_BUF_SIZE];

	if(msg==NULL || param==NULL)
		return -1;

	if(msg_ctx_id_match(msg, &_cfgutils_msgid)!=1)
	{
		msg_set_time(msg);
		msg_ctx_id_set(msg, &_cfgutils_msgid);
		if(localtime_r(&msg->tval.tv_sec, &_cfgutils_ts) == NULL)
		{
			LM_ERR("unable to break time to attributes\n");
			return -1;
		}
	}
	s.len = strftime(_pv_strftime_buf, PV_STRFTIME_BUF_SIZE,
			param->pvn.u.isname.name.s.s,  &_cfgutils_ts);
	if(s.len<=0)
		return pv_get_null(msg, param, res);
	s.s = _pv_strftime_buf;
	return pv_get_strval(msg, param, res, &s);
}


int pv_get_timenows(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	time_t t;
	t = time(NULL);
	return pv_get_uintval(msg, param, res, (unsigned int)t);
}


int pv_get_timenowf(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str s;
	time_t t;
	
	t = time(NULL);
	s.s = ctime(&t);
	s.len = strlen(s.s)-1;
	return pv_get_strintval(msg, param, res, &s, (int)t);
}

int pv_get_times(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL)
		return -1;

	msg_set_time(msg);
	return pv_get_uintval(msg, param, res, (unsigned int)msg->tval.tv_sec);
}


int pv_get_timef(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str s;
	
	if(msg==NULL)
		return -1;

	msg_set_time(msg);
	
	s.s = ctime(&msg->tval.tv_sec);
	s.len = strlen(s.s)-1;
	return pv_get_strintval(msg, param, res, &s, (int)msg->tval.tv_sec);
}

int pv_get_timeb(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL)
		return -1;
	return pv_get_uintval(msg, param, res, (unsigned int)up_since);
}

static struct timeval _timeval_ts = {0};
static char _timeval_ts_buf[32];

int pv_get_timeval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct timeval tv;
	str s;

	if(msg==NULL || param==NULL)
		return -1;

	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			msg_set_time(msg);
			return pv_get_uintval(msg, param, res, (unsigned int)msg->tval.tv_usec);
		case 2:
			if(gettimeofday(&_timeval_ts, NULL)!=0)
			{
				LM_ERR("unable to get time val attributes\n");
				return pv_get_null(msg, param, res);
			}
			return pv_get_uintval(msg, param, res, (unsigned int)_timeval_ts.tv_sec);
		case 3:
			return pv_get_uintval(msg, param, res, (unsigned int)_timeval_ts.tv_usec);
		case 4:
			if(gettimeofday(&tv, NULL)!=0)
			{
				LM_ERR("unable to get time val attributes\n");
				return pv_get_null(msg, param, res);
			}
			s.len = snprintf(_timeval_ts_buf, 32, "%u.%06u",
					(unsigned int)tv.tv_sec, (unsigned int)tv.tv_usec);
			if(s.len<0)
				return pv_get_null(msg, param, res);
			s.s = _timeval_ts_buf;
			return pv_get_strval(msg, param, res, &s);
		default:
			msg_set_time(msg);
			return pv_get_uintval(msg, param, res, (unsigned int)msg->tval.tv_sec);
	}
}

int pv_parse_timeval_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 1:
			if(strncmp(in->s, "s", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "u", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 2:
			if(strncmp(in->s, "sn", 2)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "un", 2)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else if(strncmp(in->s, "Sn", 2)==0)
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
	LM_ERR("unknown PV timeval name %.*s\n", in->len, in->s);
	return -1;
}


