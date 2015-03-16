/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "mod_sanity.h"
#include "sanity.h"
#include "api.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../error.h"

MODULE_VERSION

#define PROXY_REQUIRE_DEF 	""

str pr_str 	= STR_STATIC_INIT(PROXY_REQUIRE_DEF);

int default_msg_checks = SANITY_DEFAULT_CHECKS;
int default_uri_checks = SANITY_DEFAULT_URI_CHECKS;
int _sanity_drop = 1;

strl* proxyrequire_list = NULL;

sl_api_t slb;

static int mod_init(void);
static int sanity_fixup(void** param, int param_no);
static int w_sanity_check(struct sip_msg* _msg, char* _foo, char* _bar);
static int bind_sanity(sanity_api_t* api);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sanity_check", (cmd_function)w_sanity_check, 0, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE},
	{"sanity_check", (cmd_function)w_sanity_check, 1, sanity_fixup,
		REQUEST_ROUTE|ONREPLY_ROUTE},
	{"sanity_check", (cmd_function)w_sanity_check, 2, sanity_fixup,
		REQUEST_ROUTE|ONREPLY_ROUTE},
	{"bind_sanity",  (cmd_function)bind_sanity,    0, 0, 0},
	{0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_checks",	PARAM_INT,	&default_msg_checks	},
	{"uri_checks",		PARAM_INT,	&default_uri_checks	},
	{"proxy_require",	PARAM_STR,	&pr_str			},
	{"autodrop",		PARAM_INT,	&_sanity_drop	},
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

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
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
		if (str2int(&in, (unsigned int*)&checks) < 0) {
			LOG(L_ERR, "sanity: failed to convert input integer\n");
			return E_UNSPEC;
		}
		if ((checks < 1) || (checks >= (SANITY_MAX_CHECKS))) {
			LOG(L_ERR, "sanity: input parameter (%i) outside of valid range <1-%i)\n", checks, SANITY_MAX_CHECKS);
			return E_UNSPEC;
		}
		*param = (void*)(long)checks;
	}
	if (param_no == 2) {
		in.s = (char*)*param;
		in.len = strlen(in.s);
		if (str2int(&in, (unsigned int*)&checks) < 0) {
			LOG(L_ERR, "sanity: failed to convert second integer argument\n");
			return E_UNSPEC;
		}
		if ((checks < 1) || (checks >= (SANITY_URI_MAX_CHECKS))) {
			LOG(L_ERR, "sanity: second input parameter (%i) outside of valid range <1-%i\n", checks, SANITY_URI_MAX_CHECKS);
			return E_UNSPEC;
		}
		*param = (void*)(long)checks;
	}
	return 0;
}

/**
 * perform SIP message sanity check
 * @param _msg - SIP message structure
 * @param msg_checks - bitmask of sanity tests to perform over message
 * @param uri_checks - bitmask of sanity tests to perform over uri
 * @return -1 on error, 0 on tests failure, 1 on success
 */
int sanity_check(struct sip_msg* _msg, int msg_checks, int uri_checks)
{
	int ret;

	ret = SANITY_CHECK_PASSED;
	if (SANITY_RURI_SIP_VERSION & msg_checks &&
			(ret = check_ruri_sip_version(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_RURI_SCHEME & msg_checks &&
			(ret = check_ruri_scheme(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_REQUIRED_HEADERS & msg_checks &&
			(ret = check_required_headers(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_VIA_SIP_VERSION & msg_checks &&
			(ret = check_via_sip_version(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_VIA_PROTOCOL & msg_checks &&
			(ret = check_via_protocol(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_CSEQ_METHOD & msg_checks &&
			(ret = check_cseq_method(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_CSEQ_VALUE & msg_checks &&
			(ret = check_cseq_value(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_CL & msg_checks &&
			(ret = check_cl(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_EXPIRES_VALUE & msg_checks &&
			(ret = check_expires_value(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_PROXY_REQUIRE & msg_checks &&
			(ret = check_proxy_require(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_PARSE_URIS & msg_checks &&
			(ret = check_parse_uris(_msg, uri_checks)) != SANITY_CHECK_PASSED) {
		goto done;
	}

	if (SANITY_CHECK_DIGEST & msg_checks &&
			(ret = check_digest(_msg, uri_checks)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if (SANITY_CHECK_DUPTAGS & msg_checks &&
			(ret = check_duptags(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}

done:
	return ret;
}

/**
 * do default checks
 */
int sanity_check_defaults(struct sip_msg* msg)
{
	return sanity_check(msg, default_msg_checks, default_uri_checks);
}

/**
 * wrapper for sanity_check() to be used from config file
 */
static int w_sanity_check(struct sip_msg* _msg, char* _number, char* _arg) {
	int ret, check, arg;

	if (_number == NULL) {
		check = default_msg_checks;
	}
	else {
		check = (int)(long)_number;
	}
	if (_arg == NULL) {
		arg = default_uri_checks;
	}
	else {
		arg = (int)(long)_arg;
	}
	ret = sanity_check(_msg, check, arg);

	DBG("sanity checks result: %d\n", ret);
	if(_sanity_drop!=0)
		return ret;
	return (ret==SANITY_CHECK_FAILED)?-1:ret;
}

/**
 * load sanity module API
 */
static int bind_sanity(sanity_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->check          = sanity_check;
	api->check_defaults = sanity_check_defaults;

	return 0;
}
