/*
 * $Id$
 *
 * Sanity Checks Module
 * 
 * Copyright (C) 2006 iptelorg GbmH
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
 */


#include "mod_sanity.h"
#include "sanity.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../error.h"

MODULE_VERSION

#define PROXY_REQUIRE_DEF 	""

str pr_str 	= {PROXY_REQUIRE_DEF, sizeof(PROXY_REQUIRE_DEF)-1 };

int default_checks = SANITY_DEFAULT_CHECKS;
int uri_checks = SANITY_DEFAULT_URI_CHECKS;
strl* proxyrequire_list = NULL;

struct sl_binds sl;

static int mod_init(void);
static int sanity_fixup(void** param, int param_no);
static int sanity_check(struct sip_msg* _msg, char* _foo, char* _bar);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sanity_check", (cmd_function)sanity_check, 0, 0, 0, REQUEST_ROUTE},
	{"sanity_check", (cmd_function)sanity_check, 1, sanity_fixup, 0,
		REQUEST_ROUTE},
	{"sanity_check", (cmd_function)sanity_check, 2, sanity_fixup, 0,
		REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_checks", 	INT_PARAM, 	&default_checks},
	{"uri_checks",		INT_PARAM,  &uri_checks	},
	{"proxy_require", 	STR_PARAM, 	&pr_str 	},
	{0, 0, 0}
};

/*
 * Module description
 */
struct module_exports exports = {
	"sanity",        /* Module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,        /* Initialization function */
	0,               /* Response function */
	0,               /* Destroy function */
	0                /* Child init function */
};

/*
 * initialize module
 */
static int mod_init(void) {
	strl* ptr;

	/*
	 * We will need sl_send_reply from stateless
	 * module for sending replies
	 */
	if (load_sl_api(&sl)!=0) {
		LM_ERR("can't load SL API\n");
		return -1;
	}

	LM_DBG("parsing proxy requires string:\n");
	ptr = parse_str_list(&pr_str);

	proxyrequire_list = ptr;

	while (ptr != NULL) {
		LM_DBG("string: '%.*s', next: %p\n", ptr->string.len,
				ptr->string.s, ptr->next);
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
		if (str2int(&in, (unsigned int*)&checks) < 0) {
			LM_ERR("failed to convert input integer\n");
			return E_UNSPEC;
		}
		if ((checks < 1) || (checks >= (SANITY_MAX_CHECKS))) {
			LM_ERR("input parameter (%i) outside of valid range <1-%i)\n",
					checks, SANITY_MAX_CHECKS);
			return E_UNSPEC;
		}
		*param = (void*)(long)checks;
	}
	if (param_no == 2) {
		in.s = (char*)*param;
		in.len = strlen(in.s);
		if (str2int(&in, (unsigned int*)&checks) < 0) {
			LM_ERR("failed to convert second integer argument\n");
			return E_UNSPEC;
		}
		if ((checks < 1) || (checks > (SANITY_DEFAULT_URI_CHECKS))) {
			LM_ERR("second input parameter (%i) outside of valid range 1-%i\n",
					checks, SANITY_DEFAULT_URI_CHECKS);
			return E_UNSPEC;
		}
		*param = (void*)(long)checks;
	}
	return 0;
}

static int sanity_check(struct sip_msg* _msg, char* _number, char* _arg) {
	int ret, check, arg;

	if (_number == NULL) {
		check = default_checks;
	}
	else {
		check = (int)(long)_number;
	}
	if (_arg == NULL) {
		arg = uri_checks;
	}
	else {
		arg = (int)(long)_arg;
	}

	if (SANITY_RURI_SIP_VERSION & check &&
		(ret = check_ruri_sip_version(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_RURI_SCHEME & check &&
		(ret = check_ruri_scheme(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_REQUIRED_HEADERS & check &&
		(ret = check_required_headers(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_VIA_SIP_VERSION & check &&
		(ret = check_via_sip_version(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_VIA_PROTOCOL & check &&
		(ret = check_via_protocol(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_CSEQ_METHOD & check &&
		(ret = check_cseq_method(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_CSEQ_VALUE & check &&
		(ret = check_cseq_value(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_CL & check &&
		(ret = check_cl(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_EXPIRES_VALUE & check &&
		(ret = check_expires_value(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_PROXY_REQUIRE & check &&
		(ret = check_proxy_require(_msg)) != SANITY_CHECK_PASSED) {
		return ret;
	}
	if (SANITY_PARSE_URIS & check &&
		(ret = check_parse_uris(_msg, arg)) != SANITY_CHECK_PASSED) {
		return ret;
	}

	if (SANITY_CHECK_DIGEST & check &&
	        (ret = check_digest(_msg, arg)) != SANITY_CHECK_PASSED) {
	        return ret;
	}

	LM_DBG("all sanity checks passed\n");
	/* nobody complained so everything is fine */
	return 1;
}
