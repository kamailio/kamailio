/**
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

/*!
 * @defgroup kex KEX :: Kamailio Extensions
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
#include "../../lib/srutils/sruid.h"

#include "flags.h"
#include "km_core.h"
#include "mi_core.h"
#include "core_stats.h"
#include "pkg_stats.h"


MODULE_VERSION


/** parameters */

/** module functions */
int w_is_myself(struct sip_msg *msg, char *uri, str *s2);
int w_setdebug(struct sip_msg *msg, char *level, str *s2);
int w_resetdebug(struct sip_msg *msg, char *uri, str *s2);

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);


static sruid_t _kex_sruid;

static int pv_get_sruid_val(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

static pv_export_t mod_pvs[] = {
	{ {"sruid", sizeof("sruid")-1}, PVT_OTHER, pv_get_sruid_val, 0,
		0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

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
	{"setdebug", (cmd_function)w_setdebug,      1, fixup_igp_null,
			0, ANY_ROUTE },
	{"resetdebug", (cmd_function)w_resetdebug,  0, 0,
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
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	if(sruid_init(&_kex_sruid, '-', NULL, 0)<0)
		return -1;
	if(init_mi_core()<0)
		return -1;
#ifdef STATISTICS
	if(register_core_stats()<0)
		return -1;
	if(register_mi_stats()<0)
		return -1;
#endif
	register_pkg_proc_stats();
	pkg_proc_stats_init_rpc();
	return 0;
}

/**
 *
 */
static int child_init(int rank)
{
	LM_DBG("rank is (%d)\n", rank);
	if(sruid_init(&_kex_sruid, '-', NULL, 0)<0)
		return -1;
	if (rank==PROC_INIT)
		return pkg_proc_stats_init();
	return pkg_proc_stats_myinit(rank);
}


/**
 * destroy function
 */
static void destroy(void)
{
	pkg_proc_stats_destroy();
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

int w_setdebug(struct sip_msg *msg, char *level, str *s2)
{
	int lval=0;
	if(fixup_get_ivalue(msg, (gparam_p)level, &lval)!=0)
	{
		LM_ERR("no debug level value\n");
		return -1;
	}
	set_local_debug_level(lval);
	return 1;
}

int w_resetdebug(struct sip_msg *msg, char *uri, str *s2)
{
	reset_local_debug_level();
	return 1;
}


static int pv_get_sruid_val(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(res==NULL)
		return -1;
	if(sruid_next(&_kex_sruid)<0)
		return pv_get_null(msg, param, res);
	return pv_get_strval(msg, param, res, &_kex_sruid.uid);
}

