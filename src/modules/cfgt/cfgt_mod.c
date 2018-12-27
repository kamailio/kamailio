/*
 * Copyright (C) 2015 Victor Seva (sipwise.com)
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
 *
 *
 */

#include "../../core/events.h"
#include "../../core/script_cb.h"
#include "../../core/sr_module.h"

#include "cfgt_mod.h"
#include "cfgt_int.h"
#include "cfgt_json.h"
#include "cfgt.h"

MODULE_VERSION

static int mod_init(void);		 /*!< Module initialization function */
static void destroy(void);		 /*!< Module destroy function */
static int child_init(int rank); /*!< Per-child init function */

extern int bind_cfgt(cfgt_api_t *api);

/*! flag to protect against wrong initialization */
unsigned int init_flag = 0;
extern int cfgt_mask;
extern str cfgt_basedir;
extern str cfgt_hdr_prefix;
/* clang-format off */
/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"cfgt_bind_cfgt", (cmd_function)bind_cfgt, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"basedir", PARAM_STR, &cfgt_basedir},
	{"mask", INT_PARAM, &cfgt_mask},
	{"callid_prefix", PARAM_STR, &cfgt_hdr_prefix},
	{0, 0, 0}
};

struct module_exports exports = {
	"cfgt",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	destroy          /* module destroy function */
};
/* clang-format on */
/*! \brief
 * Module initialization function
 */
static int mod_init(void)
{
	unsigned int ALL = REQUEST_CB + FAILURE_CB + ONREPLY_CB + BRANCH_CB
					   + ONSEND_CB + ERROR_CB + LOCAL_CB + EVENT_CB
					   + BRANCH_FAILURE_CB;
	if(cfgt_init() < 0)
		return -1;
	if(register_script_cb(cfgt_pre, PRE_SCRIPT_CB | ALL, 0) != 0) {
		LM_ERR("could not insert PRE_SCRIPT callback");
		return -1;
	}
	if(register_script_cb(cfgt_post, POST_SCRIPT_CB | ALL, 0) != 0) {
		LM_ERR("could not insert POST_SCRIPT callback");
		return -1;
	}

	init_flag = 1;
	return 0;
}

static int child_init(int _rank)
{
	return 0;
}

/*! \brief
 * Module destroy function
 */
static void destroy(void)
{}
