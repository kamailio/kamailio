/**
 * $Id$
 *
 * Copyright (C) 2009
 *
 * This file is part of SIP-Router.org, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"
#include "../../dset.h"
#include "../../flags.h"
#include "../../pvar.h"
#include "../../lvalue.h"
#include "../../mod_fix.h"
#include "km_core.h"


int w_setdsturi(struct sip_msg *msg, char *uri, str *s2)
{
	str s;

	/* todo: fixup */
	s.s = uri;
	s.len = strlen(uri);
	
	if(set_dst_uri(msg, &s)!=0)
		return -1;
	/* dst_uri changes, so it makes sense to re-use the current uri for
		forking */
	ruri_mark_new(); /* re-use uri for serial forking */
	return 1;

}

int w_resetdsturi(struct sip_msg *msg, char *uri, str *s2)
{
	if(msg->dst_uri.s!=0)
		pkg_free(msg->dst_uri.s);
	msg->dst_uri.s = 0;
	msg->dst_uri.len = 0;
	return 1;
}

int w_isdsturiset(struct sip_msg *msg, char *uri, str *s2)
{
	if(msg->dst_uri.s==0 || msg->dst_uri.len<=0)
		return -1;
	return 1;
}

int pv_printf_fixup(void** param, int param_no)
{
	pv_spec_t *spec=NULL;
	pv_elem_t *pvmodel=NULL;
	str tstr;
	
	if(param_no==1)
	{
		spec = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
		if(spec==NULL)
		{
			LM_ERR("out of pkg\n");
			return -1;
		}
		memset(spec, 0, sizeof(pv_spec_t));
		tstr.s = (char*)(*param);
		tstr.len = strlen(tstr.s);
		if(pv_parse_spec(&tstr, spec)==NULL)
		{
			LM_ERR("unknown script variable in first parameter");
			pkg_free(spec);
			return -1;
		}
		if(!pv_is_w(spec))
		{
			LM_ERR("read-only script variable in first parameter");
			pkg_free(spec);
			return -1;
		}
		*param = spec;
	} else if(param_no==2) {
		pvmodel = 0;
		tstr.s = (char*)(*param);
		tstr.len = strlen(tstr.s);
		if(pv_parse_format(&tstr, &pvmodel)<0)
		{
			LM_ERR("error in second parameter");
			return -1;
		}
		*param = pvmodel;
	}
	return 0;
}

int w_pv_printf(struct sip_msg *msg, char *s1, str *s2)
{
	pv_spec_t *spec=NULL;
	pv_elem_t *model=NULL;
	pv_value_t val;

	spec = (pv_spec_t*)s1;

	model = (pv_elem_t*)s2;

	memset(&val, 0, sizeof(pv_value_t));
	if(pv_printf_s(msg, model, &val.rs)!=0)
	{
		LM_ERR("cannot eval second parameter\n");
		goto error;
	}
	val.flags = PV_VAL_STR;
	if(spec->setf(msg, &spec->pvp, EQ_T, &val)<0)
	{
		LM_ERR("setting PV failed\n");
		goto error;
	}

	return 1;
error:
	return -1;
}

