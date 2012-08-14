/*
 * $Id$
 *
 * Copyright (C) 2012 Crocodile RCS Ltd
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

#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/mi.h"

#include "api.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

static int ob_force_bflag = -1;
static str ob_key = {0, 0};

static cmd_export_t cmds[]= 
{
	{ "bind_ob", (cmd_function) bind_ob,
	  1, 0, 0,
	  0 },
	{ 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[]=
{
	{ "force_outbound_bflag",	INT_PARAM, &ob_force_bflag },
	{ "flow_token_key",		STR_PARAM, &ob_key.s},
	{ 0, 0, 0 }
};

static stat_export_t stats[] =
{
	{ 0, 0, 0 }
};

static mi_export_t mi_cmds[] =
{
	{ 0, 0, 0, 0, 0 }
};

struct module_exports exports= 
{
	"outbound",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* Exported functions */
	params,			/* Exported parameters */
	stats,			/* exported statistics */
	mi_cmds,		/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,		/* module initialization function */
	0,			/* response function */
	destroy,		/* destroy function */
	child_init		/* per-child initialization function */
};

static int mod_init(void)
{
	if (register_module_stats(exports.name, stats) != 0)
	{
		LM_ERR("registering core statistics\n");
		return -1;
	}

	if (register_mi_mod(exports.name, mi_cmds) != 0)
	{
		LM_ERR("registering MI commands\n");
		return -1;
	}

	if (ob_force_bflag == -1)
	{
		LM_ERR("force_outbound_bflag not set\n");
		return -1;
	}

	if (ob_key.s == 0)
	{
		LM_ERR("flow_token_key not set\n");
		return -1;
	}
	else
		ob_key.len = strlen(ob_key.s);

	return 0;
}

static int child_init(int rank)
{
	/* TODO */
	return 0;
}

static void destroy(void)
{
	/* TODO */
}

int ob_fn1(int p1, int p2, int p3)
{
	return 0;
}

int ob_fn2(int p1, int p2, int p3)
{
	return 0;
}

int ob_fn3(int p1, int p2, int p3)
{
	return 0;
}

int bind_ob(struct ob_binds *pxb)
{
	if (pxb == NULL)
	{
		LM_WARN("bind_outbound: Cannot load outbound API into NULL pointer\n");
		return -1;
	}

	pxb->ob_fn1 = ob_fn1;
	pxb->ob_fn2 = ob_fn2;
	pxb->ob_fn3 = ob_fn3;

	return 0;
}
