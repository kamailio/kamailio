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
 * \brief comparison functions
 * \ingroup libkcore
 */

#include "../../parser/parse_uri.h"
#include "../../parser/parse_param.h"
#include "cmpapi.h"

int cmp_str(str *s1, str *s2)
{
	int ret = 0;
	int len = 0;
	if(s1->len==0 && s2->len==0)
		return 0;
	if(s1->len==0)
		return -1;
	if(s2->len==0)
		return 1;
	len = (s1->len<s2->len)?s1->len:s2->len;
	ret = strncmp(s1->s, s2->s, len);
	if(ret==0)
	{
		if(s1->len==s2->len)
			return 0;
		if(s1->len<s2->len)
			return -1;
		return 1;
	}
	return ret;
}

int cmpi_str(str *s1, str *s2)
{
	int ret = 0;
	int len = 0;
	if(s1->len==0 && s2->len==0)
		return 0;
	if(s1->len==0)
		return -1;
	if(s2->len==0)
		return 1;
	len = (s1->len<s2->len)?s1->len:s2->len;
	ret = strncasecmp(s1->s, s2->s, len);
	if(ret==0)
	{
		if(s1->len==s2->len)
			return 0;
		if(s1->len<s2->len)
			return -1;
		return 1;
	}
	return ret;
}

int cmp_hdrname_str(str *s1, str *s2)
{
	/* todo: parse hdr name and compare with short/long alternative */
	return cmpi_str(s1, s2);
}

int cmp_hdrname_strzn(str *s1, char *s2, size_t n)
{
	str s;
	s.s = s2;
	s.len = n;
	return cmpi_str(s1, &s);
}

int cmp_str_params(str *s1, str *s2)
{
	param_t* pl1 = NULL;
	param_hooks_t phooks1;
	param_t *pit1=NULL;
	param_t* pl2 = NULL;
	param_hooks_t phooks2;
	param_t *pit2=NULL;
	
	if (parse_params(s1, CLASS_ANY, &phooks1, &pl1)<0)
		return -1;
	if (parse_params(s2, CLASS_ANY, &phooks2, &pl2)<0)
		return -1;
	for (pit1 = pl1; pit1; pit1=pit1->next)
	{
		for (pit2 = pl2; pit2; pit2=pit2->next)
		{
			if (pit1->name.len==pit2->name.len
				&& strncasecmp(pit1->name.s, pit2->name.s, pit2->name.len)==0)
			{
				if(pit1->body.len!=pit2->body.len
						|| strncasecmp(pit1->body.s, pit2->body.s,
							pit2->body.len)!=0)
					return 1;
			}
		}
	}
	return 0;
}

/**
 * Compare SIP URI as per RFC3261, 19.1.4
 * return:
 *	- 0: match
 *	- >0: no match
 *	- <0: error
 */
int cmp_uri(struct sip_uri *uri1, struct sip_uri *uri2)
{
	if(uri1->type!=uri2->type)
		return 1;
	/* quick check for length */
	if(uri1->user.len!=uri2->user.len
			|| uri1->host.len!=uri2->host.len
			|| uri1->port.len!=uri2->port.len
			|| uri1->passwd.len!=uri2->passwd.len)
		return 1;
	if(cmp_str(&uri1->user, &uri2->user)!=0)
		return 1;
	if(cmp_str(&uri1->port, &uri2->port)!=0)
		return 1;
	if(cmp_str(&uri1->passwd, &uri2->passwd)!=0)
		return 1;
	if(cmpi_str(&uri1->host, &uri2->host)!=0)
		return 1;
	/* if no params, we are done */
	if(uri1->params.len==0 && uri2->params.len==0)
		return 0;
	if(uri1->params.len==0)
	{
		if(uri2->user_param.len!=0)
			return 1;
		if(uri2->ttl.len!=0)
			return 1;
		if(uri2->method.len!=0)
			return 1;
		if(uri2->maddr.len!=0)
			return 1;
	}
	if(uri2->params.len==0)
	{
		if(uri1->user_param.len!=0)
			return 1;
		if(uri1->ttl.len!=0)
			return 1;
		if(uri1->method.len!=0)
			return 1;
		if(uri1->maddr.len!=0)
			return 1;
	}
	return cmp_str_params(&uri1->params, &uri2->params);
}

/**
 * return:
 *	- 0: match
 *	- >0: no match
 *	- <0: error
 */
int cmp_uri_str(str *s1, str *s2)
{
	struct sip_uri uri1;
	struct sip_uri uri2;

	/* todo: parse uri and compare the parts */
	if(parse_uri(s1->s, s1->len, &uri1)!=0)
		return -1;
	if(parse_uri(s2->s, s2->len, &uri2)!=0)
		return -1;
	return cmp_uri(&uri1, &uri2);
}

/**
 * Compare SIP AoR
 * - match user, host and port (if port missing, assume 5060)
 * return:
 *	- 0: match
 *	- >0: no match
 *	- <0: error
 */
int cmp_aor(struct sip_uri *uri1, struct sip_uri *uri2)
{
	/* quick check for length */
	if(uri1->user.len!=uri2->user.len
			|| uri1->host.len!=uri2->host.len)
		return 1;
	if(cmp_str(&uri1->user, &uri2->user)!=0)
		return 1;
	if(cmp_str(&uri1->port, &uri2->port)!=0)
	{
		if(uri1->port.len==0 && uri2->port_no!=5060)
			return 1;
		if(uri2->port.len==0 && uri1->port_no!=5060)
			return 1;
	}
	if(cmpi_str(&uri1->host, &uri2->host)!=0)
		return 1;
	return 0;
}

/**
 * return:
 *	- 0: match
 *	- >0: no match
 *	- <0: error
 */
int cmp_aor_str(str *s1, str *s2)
{
	struct sip_uri uri1;
	struct sip_uri uri2;

	/* todo: parse uri and compare the parts */
	if(parse_uri(s1->s, s1->len, &uri1)!=0)
		return -1;
	if(parse_uri(s2->s, s2->len, &uri2)!=0)
		return -1;
	return cmp_aor(&uri1, &uri2);
}

