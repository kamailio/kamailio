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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../forward.h"
#include "../../flags.h"
#include "../../dset.h"
#include "../../mod_fix.h"
#include "../../parser/parse_uri.h"

#include "flags.h"
#include "km_core.h"
#include "mi_core.h"
#include "core_stats.h"


MODULE_VERSION


/** parameters */

/** module functions */
int w_is_myself(struct sip_msg *msg, char *uri, str *s2);

static int mod_init(void);
static void destroy(void);

static cmd_export_t cmds[]={
	{"setsflag", (cmd_function)w_setsflag,          1,fixup_igp_null,
			0, ANY_ROUTE },
	{"resetsflag", (cmd_function)w_resetsflag,      1,fixup_igp_null,
			0, ANY_ROUTE },
	{"issflagset", (cmd_function)w_issflagset,      1,fixup_igp_null,
			0, ANY_ROUTE },
	{"setbflag", (cmd_function)w_setbflag,          1,fixup_igp_null,
			0, ANY_ROUTE },
	{"setbflag", (cmd_function)w_setbflag,          2,fixup_igp_igp,
			0, ANY_ROUTE },
	{"resetbflag", (cmd_function)w_resetbflag,      1,fixup_igp_null,
			0, ANY_ROUTE },
	{"resetbflag", (cmd_function)w_resetbflag,      2,fixup_igp_igp,
			0, ANY_ROUTE },
	{"isbflagset", (cmd_function)w_isbflagset,      1,fixup_igp_null,
			0, ANY_ROUTE },
	{"isbflagset", (cmd_function)w_isbflagset,      2,fixup_igp_igp,
			0, ANY_ROUTE },
	{"km_append_branch", (cmd_function)w_km_append_branch, 0, 0,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"km_append_branch", (cmd_function)w_km_append_branch, 1, fixup_spve_null,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"setdsturi", (cmd_function)w_setdsturi,     1, 0,
			0, ANY_ROUTE },
	{"resetdsturi", (cmd_function)w_resetdsturi, 0, 0,
			0, ANY_ROUTE },
	{"isdsturiset", (cmd_function)w_isdsturiset, 0, 0,
			0, ANY_ROUTE },
	{"pv_printf", (cmd_function)w_pv_printf,    2, pv_printf_fixup,
			0, ANY_ROUTE },
	{"avp_printf", (cmd_function)w_pv_printf,   2, pv_printf_fixup,
			0, ANY_ROUTE },
	{"is_myself", (cmd_function)w_is_myself,    1, fixup_spve_null,
			0, ANY_ROUTE },

	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"kex",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	0           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	if(init_mi_core()<0)
		return -1;
#ifdef STATISTICS
	if(register_core_stats()<0)
		return -1;
	if(register_mi_stats()<0)
		return -1;
#endif
	return 0;
}

/**
 * destroy function
 */
static void destroy(void)
{
	return;
}


/**
 *
 */
int w_is_myself(struct sip_msg *msg, char *uri, str *s2)
{
	int ret;
	str suri;
	struct sip_uri puri;

	if(fixup_get_svalue(msg, (gparam_p)uri, &suri)!=0)
	{
		LM_ERR("cannot get the URI parameter\n");
		return -1;
	}
	if(suri.len>4 && (strncmp(suri.s, "sip:", 4)==0
				|| strncmp(suri.s, "sips:", 5)==0))
	{
		if(parse_uri(suri.s, suri.len, &puri)!=0)
		{
			LM_ERR("failed to parse uri [%.*s]\n", suri.len, suri.s);
			return -1;
		}
		ret = check_self(&puri.host, (puri.port.s)?puri.port_no:0,
				(puri.transport_val.s)?puri.proto:0);
	} else {
		ret = check_self(&suri, 0, 0);
	}
	if(ret!=1)
		return -1;
	return 1;
}

