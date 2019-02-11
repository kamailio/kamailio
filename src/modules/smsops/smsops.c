/**
 * Copyright (C) 2015 Carsten Bock, ng-voice GmbH
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../core/sr_module.h"
#include "../../core/pvar.h"
#include "../../core/mod_fix.h"

#include "smsops_impl.h"

MODULE_VERSION

static pv_export_t mod_pvs[] = {
	{ {"smsack", sizeof("smsack")-1}, PVT_OTHER, pv_sms_ack, 0, 0, 0, 0, 0 },
	{ {"rpdata", sizeof("rpdata")-1}, PVT_OTHER, pv_get_sms, pv_set_sms,
		pv_parse_rpdata_name, 0, 0, 0 },
	{ {"tpdu", sizeof("tpdu")-1}, PVT_OTHER, pv_get_sms, pv_set_sms,
		pv_parse_tpdu_name, 0, 0, 0 },
	{ {"smsbody", sizeof("smsbody")-1}, PVT_OTHER, pv_sms_body, 0, 0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"smsdump",   (cmd_function)smsdump, 0, 0, 0, REQUEST_ROUTE},
	{"isRPDATA",  (cmd_function)isRPDATA, 0, 0, 0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};

/** module exports */
struct module_exports exports= {
	"smsops",    /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        /* exported functions */
	0,           /* exported parameters */
	0,           /* exported rpc functions */
	mod_pvs,     /* exported pseudo-variables */
	0,           /* response handling function*/
	0,           /* module init function */
	0,           /* per-child init function */
	0            /* module destroy function */
};


