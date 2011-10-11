/*$Id$
 *
 * Copyright (C) 2011 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include "sr_module.h"
#include "script_cb.h"
#include "route.h"
#include "modules/tm/tm_load.h"
#include "msg_shooter.h"
#include "msg_shooter_mod.h"

MODULE_VERSION

struct tm_binds	tmb;

/* Module management function prototypes */
static int mod_init(void);
static int w_smsg_destroy(struct sip_msg *_msg, unsigned int flags, void *_param);
static int fixup_smsg_on_reply(void** _param, int _param_no);

/* Exported functions */
static cmd_export_t cmds[] = {
	{"shoot_msg",	smsg,		0, 0,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},

	{"shoot_msg",	smsg,		1, fixup_var_str_1,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},

	{"shoot_msg",	smsg,		2, fixup_var_str_12,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},

	{"smsg_create",	smsg_create, 	1, fixup_str_1,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},

	{"smsg_from_to",	smsg_from_to, 2, fixup_var_str_12,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},

	{"smsg_append_hdrs", 	smsg_append_hdrs, 1, fixup_var_str_1,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},

	{"smsg_append_hdrs", 	smsg_append_hdrs, 2, fixup_var_str_12,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},

	{"smsg_on_reply", 	smsg_on_reply,	1, fixup_smsg_on_reply,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},

	{0, 0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
	"msg_shooter",
	cmds,		/* Exported functions */
	0,		/* RPC methods */	
	0,		/* Exported parameters */
	mod_init,	/* module initialization function */
	0,		/* response function */
	0,		/* destroy function */
	0,		/* oncancel function */
	0		/* child initialization function */
};

/* module initialization function */
static int mod_init(void)
{
	load_tm_f	load_tm;

	LOG(L_DBG, "DEBUG: mod_init(): Initializing msg_shooter module\n");

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "ERROR: mod_init(): can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) return -1;

	if (register_script_cb(w_smsg_destroy,
			REQUEST_CB | FAILURE_CB | ONREPLY_CB | BRANCH_CB | POST_SCRIPT_CB,
			0) < 0
	)
		return -1;

	return 0;
}

/* free allocated memory */
static int w_smsg_destroy(struct sip_msg *_msg, unsigned int flags, void *_param)
{
	smsg_destroy();
	return 1;
}

/* fixup function to convert route name to index */
static int fixup_smsg_on_reply(void** _param, int _param_no)
{
	int index;

	if (_param_no != 1) return 0;

	index = route_lookup(&onreply_rt, (char*)(*_param));
	if (index < 0) {
		LOG(L_ERR, "ERROR: fixup_smsg_on_reply(): unknown on_reply route name: %s\n",
				(char*)(*_param));
		return -1;
	}
	pkg_free(*_param);
	*_param = (void *)(long)index;
	return 0;
}
