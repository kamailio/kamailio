/*
 * $Id: utils.h 5318 2008-12-08 16:38:47Z henningw $
 *
 * SIPUTILS mangler module
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
 *
 */

#include "../../mod_fix.h"
#include "../../cmpapi.h"

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

