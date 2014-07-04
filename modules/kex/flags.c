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
#include "../../flags.h"
#include "../../dset.h"
#include "../../mod_fix.h"
#include "flags.h"

int w_issflagset(struct sip_msg *msg, char *flag, str *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_p)flag, &fval)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(fval<0 || fval>31)
		return -1;
	return issflagset((flag_t)fval);
}

int w_resetsflag(struct sip_msg *msg, char *flag, str *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_p)flag, &fval)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(fval<0 || fval>31)
		return -1;
	return resetsflag((flag_t)fval);
}

int w_setsflag(struct sip_msg *msg, char *flag, char *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_p)flag, &fval)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(fval<0 || fval>31)
		return -1;
	return setsflag((flag_t)fval);
}

int w_isbflagset(struct sip_msg *msg, char *flag, str *idx)
{
	int fval=0;
	int ival=0;
	if(fixup_get_ivalue(msg, (gparam_p)flag, &fval)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(fval<0 || fval>31)
		return -1;
	if(idx!=0)
	{
		if(fixup_get_ivalue(msg, (gparam_p)idx, &ival)!=0)
		{
			LM_ERR("no idx value\n");
			return -1;
		}
		if(ival<0)
			return -1;
	}
	return isbflagset(ival, (flag_t)fval);
}

int w_resetbflag(struct sip_msg *msg, char *flag, str *idx)
{
		int fval=0;
	int ival=0;
	if(fixup_get_ivalue(msg, (gparam_p)flag, &fval)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(fval<0 || fval>31)
		return -1;
	if(idx!=0)
	{
		if(fixup_get_ivalue(msg, (gparam_p)idx, &ival)!=0)
		{
			LM_ERR("no idx value\n");
			return -1;
		}
		if(ival<0)
			return -1;
	}
	return resetbflag(ival, (flag_t)fval);

}

int w_setbflag(struct sip_msg *msg, char *flag, char *idx)
{
	int fval=0;
	int ival=0;
	if(fixup_get_ivalue(msg, (gparam_p)flag, &fval)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(fval<0 || fval>31)
		return -1;
	if(idx!=0)
	{
		if(fixup_get_ivalue(msg, (gparam_p)idx, &ival)!=0)
		{
			LM_ERR("no idx value\n");
			return -1;
		}
		if(ival<0)
			return -1;
	}
	return setbflag(ival, (flag_t)fval);
}

