/*
 * $Id: utils.h 5318 2008-12-08 16:38:47Z henningw $
 *
 * SIPUTILS mangler module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!
 * \file
 * \brief SIP-utils :: Mangler Module
 * \ingroup siputils
 * - Module; \ref siputils
 */


#include "../../mod_fix.h"
#include "../../parser/parse_uri.h"
#include "../../lib/kcore/cmpapi.h"

#include "sipops.h"

int w_cmp_uri(struct sip_msg *msg, char *uri1, char *uri2)
{
	str s1;
	str s2;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)uri1, &s1)!=0)
	{
		LM_ERR("cannot get first parameter\n");
		return -8;
	}
	if(fixup_get_svalue(msg, (gparam_p)uri2, &s2)!=0)
	{
		LM_ERR("cannot get second parameter\n");
		return -8;
	}
	ret = cmp_uri_str(&s1, &s2);
	if(ret==0)
		return 1;
	if(ret>0)
		return -1;
	return -2;
}

int w_cmp_aor(struct sip_msg *msg, char *uri1, char *uri2)
{
	str s1;
	str s2;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)uri1, &s1)!=0)
	{
		LM_ERR("cannot get first parameter\n");
		return -8;
	}
	if(fixup_get_svalue(msg, (gparam_p)uri2, &s2)!=0)
	{
		LM_ERR("cannot get second parameter\n");
		return -8;
	}
	ret = cmp_aor_str(&s1, &s2);
	if(ret==0)
		return 1;
	if(ret>0)
		return -1;
	return -2;
}

int w_is_gruu(sip_msg_t *msg, char *uri1, char *p2)
{
        str s1, *s2;
	sip_uri_t turi;
	sip_uri_t *puri;

	if(uri1!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)uri1, &s1)!=0)
		{
			LM_ERR("cannot get first parameter\n");
			return -8;
		}
		if(parse_uri(s1.s, s1.len, &turi)!=0) {
		    LM_ERR("parsing of uri '%.*s' failed\n", s1.len, s1.s);
		    return -1;
		}
		puri = &turi;
	} else {
  	        if(parse_sip_msg_uri(msg)<0) {
		    s2 = GET_RURI(msg);
  		    LM_ERR("parsing of uri '%.*s' failed\n", s2->len, s2->s);
		    return -1;
		}
		puri = &msg->parsed_uri;
	}
	if(puri->gr.s!=NULL)
	{
		if(puri->gr_val.len>0)
			return 1;
		return 2;
	}
	return -1;
}
