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
#include "../../dset.h"
#include "../../ip_addr.h"
#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/mi.h"

#include "../nathelper/nat_uac_test.h"

#include "api.h"

MODULE_VERSION

static int mod_init(void);

static unsigned int ob_force_bflag = (unsigned int) -1;
static str ob_key = {0, 0};

static cmd_export_t cmds[]= 
{
	{ "ob_nat_uac_test", (cmd_function) nat_uac_test_f,
	  1, fixup_uint_null, 0,
	  REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
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

struct module_exports exports= 
{
	"outbound",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* Exported functions */
	params,			/* Exported parameters */
	0,			/* exported statistics */
	0,			/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,		/* module initialization function */
	0,			/* response function */
	0,			/* destroy function */
	0			/* per-child initialization function */
};

static int mod_init(void)
{
	if (ob_force_bflag == (unsigned int) -1)
		ob_force_bflag = 0;
	else if (ob_force_bflag >= 8 * sizeof (ob_force_bflag)) {
		LM_ERR("force_outbound_bflag (%d) too big!\n", ob_force_bflag);
		return -1;
	} else
		ob_force_bflag = 1 << ob_force_bflag;

	if (ob_key.s == 0)
	{
		LM_ERR("flow_token_key not set\n");
		return -1;
	}
	else
		ob_key.len = strlen(ob_key.s);

	return 0;
}

int encode_flow_token(str *flow_token, struct receive_info rcv)
{
	
	return 0;
}

int decode_flow_token(struct receive_info *rcv, str flow_token)
{
	return 0;
}

int use_outbound(struct sip_msg *msg)
{
	/* If Outbound is forced return success without any further checks */
	if (isbflagset(0, ob_force_bflag) > 0)
		return 1;

	/* Use Outbound when:
	    # It's an initial request (out-of-dialog INVITE, REGISTER,
	      SUBSCRIBE, or REFER), with
	    # A single Via:, and
	    # Top Route: points to us and has ;ob parameter _OR_ Contact: has
	      ;ob parameter _OR_ it's a REGISTER with ;+sip.instance
	*/

	return 0;
}

int bind_ob(struct ob_binds *pxb)
{
	if (pxb == NULL)
	{
		LM_WARN("bind_outbound: Cannot load outbound API into NULL "
			"pointer\n");
		return -1;
	}

	pxb->encode_flow_token = encode_flow_token;
	pxb->decode_flow_token = decode_flow_token;
	pxb->use_outbound = use_outbound;

	return 0;
}
