/*
 * $Id$
 *
 * Sanity Checks Module
 *
 * Copyright (C) 2006 iptelorg GbmH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "mod_sanity.h"
#include "sanity.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../error.h"

MODULE_VERSION

#define PROXY_REQUIRE_DEF 	""

str pr_str 	= STR_STATIC_INIT(PROXY_REQUIRE_DEF);

int default_checks = SANITY_DEFAULT_CHECKS;
strl* proxyrequire_list = NULL;

static int mod_init(void);
static int sanity_fixup(void** param, int param_no);
static int sanity_check(struct sip_msg* _msg, char* _foo, char* _bar);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sanity_check", (cmd_function)sanity_check, 0, 0, REQUEST_ROUTE},
	{"sanity_check", (cmd_function)sanity_check, 1, sanity_fixup, REQUEST_ROUTE},
	{0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_checks", 	PARAM_INT, 	&default_checks},
	{"proxy_require", 	PARAM_STR, 	&pr_str 	},
	{0, 0, 0}
};

/*
 * Module description
 */
struct module_exports exports = {
	"sanity",        /* Module name */
	cmds,            /* Exported functions */
	0,               /* RPC methods */
	params,          /* Exported parameters */
	mod_init,        /* Initialization function */
	0,               /* Response function */
	0,               /* Destroy function */
	0,               /* OnCancel function */
	0                /* Child init function */
};

/*
 * initialize module
 */
static int mod_init(void) {
	strl* ptr;

	DBG("sanity initializing\n");

	sl_reply = find_export("sl_send_reply", 2, 0);
	if (!sl_reply) {
		LOG(L_ERR, "sanity: this module requires sl module\n");
		return -1;
	}

	DBG("parsing proxy requires string:\n");
	ptr = parse_str_list(&pr_str);

	proxyrequire_list = ptr;

	while (ptr != NULL) {
		DBG("string: '%.*s', next: %p\n", ptr->string.len, ptr->string.s, ptr->next);
		ptr = ptr->next;
	}

	return 0;
}

static int sanity_fixup(void** param, int param_no) {
	int checks;
	str in;

	if (param_no == 1) {
		in.s = (char*)*param;
		in.len = strlen(in.s);
		if (str2int(&in, &checks) < 0) {
			LOG(L_ERR, "sanity: failed to convert input integer\n");
			return E_UNSPEC;
		}
		if ((checks < 1) || (checks > (SANITY_MAX_CHECKS))) {
			LOG(L_ERR, "sanity: input parameter (%i) outside of valid range 1-%i\n", checks, SANITY_MAX_CHECKS);
			return E_UNSPEC;
		}
		*param = (void*)checks;
	}
	return 0;
}

static int sanity_check(struct sip_msg* _msg, char* _number, char* _foo) {
	int check;

	if (_number == NULL) {
		check = default_checks;
	}
	else {
		check = (int)_number;
	}

	if (SANITY_RURI_SIP_VERSION & check &&
		check_ruri_sip_version(_msg) != 0) {
		return 1;
	}
	if (SANITY_RURI_SCHEME & check &&
		check_ruri_scheme(_msg) != 0) {
		return 1;
	}
	if (SANITY_REQUIRED_HEADERS & check &&
		check_required_headers(_msg) != 0) {
		return 1;
	}
	if (SANITY_VIA_SIP_VERSION & check &&
		check_via_sip_version(_msg) != 0) {
		return 1;
	}
	if (SANITY_VIA_PROTOCOL & check &&
		check_via_protocol(_msg) != 0) {
		return 1;
	}
	if (SANITY_CSEQ_METHOD & check &&
		check_cseq_method(_msg) != 0) {
		return 1;
	}
	if (SANITY_CSEQ_VALUE & check &&
		check_cseq_value(_msg) != 0) {
		return 1;
	}
	if (SANITY_CL & check &&
		check_cl(_msg) != 0) {
		return 1;
	}
	if (SANITY_EXPIRES_VALUE & check &&
		check_expires_value(_msg) != 0) {
		return 1;
	}
	if (SANITY_PROXY_REQUIRE & check &&
		check_proxy_require(_msg) != 0) {
		return 1;
	}

	DBG("all sanity checks passed\n");
	/* nobody complained so everything is fine */
	return -1;
}
