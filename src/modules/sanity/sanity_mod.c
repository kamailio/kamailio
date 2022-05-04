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

#include "sanity_mod.h"
#include "sanity.h"
#include "api.h"
#include "../../core/sr_module.h"
#include "../../core/ut.h"
#include "../../core/error.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

MODULE_VERSION

#define PROXY_REQUIRE_DEF 	""

str pr_str 	= STR_STATIC_INIT(PROXY_REQUIRE_DEF);

int default_msg_checks = SANITY_DEFAULT_CHECKS;
int default_uri_checks = SANITY_DEFAULT_URI_CHECKS;
int _sanity_drop = 1;
int ksr_sanity_noreply = 0;

strl* proxyrequire_list = NULL;

sl_api_t slb;

static int mod_init(void);
static int w_sanity_check(sip_msg_t* _msg, char* _msg_check, char* _uri_check);
static int w_sanity_reply(sip_msg_t* _msg, char* _p1, char* _p2);
static int bind_sanity(sanity_api_t* api);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sanity_check", (cmd_function)w_sanity_check, 0, 0, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE},
	{"sanity_check", (cmd_function)w_sanity_check, 1, fixup_igp_null, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE},
	{"sanity_check", (cmd_function)w_sanity_check, 2, fixup_igp_igp, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE},
	{"sanity_reply", (cmd_function)w_sanity_reply, 0, 0, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE},
	{"bind_sanity",  (cmd_function)bind_sanity,    0, 0, 0, 0 },
	{0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_checks",	PARAM_INT,	&default_msg_checks	},
	{"uri_checks",		PARAM_INT,	&default_uri_checks	},
	{"proxy_require",	PARAM_STR,	&pr_str			},
	{"autodrop",		PARAM_INT,	&_sanity_drop	},
	{"noreply",			PARAM_INT,	&ksr_sanity_noreply	},
	{0, 0, 0}
};

/*
 * Module description
 */
struct module_exports exports = {
	"sanity",        /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd exports */
	params,          /* exported parameters */
	0,               /* RPC methods */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module initialization function */
	0,               /* per-child init function */
	0                /* module destroy function */
};

/*
 * initialize module
 */
static int mod_init(void) {
	strl* ptr;

	LM_DBG("sanity initializing\n");

	ksr_sanity_info_init();

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
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

	if(ksr_sanity_noreply!=0) {
		ksr_sanity_info_init();
	}

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
	if (SANITY_VIA1_HEADER & msg_checks &&
			(ret = check_via1_header(_msg)) != SANITY_CHECK_PASSED) {
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
	if (SANITY_CHECK_AUTHORIZATION & msg_checks &&
			(ret = check_authorization(_msg, uri_checks)) != SANITY_CHECK_PASSED) {
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
static int w_sanity_check(sip_msg_t* _msg, char* _msg_check, char* _uri_check)
{
	int ret, msg_check, uri_check;

	if (_msg_check == NULL) {
		msg_check = default_msg_checks;
	} else {
		if(fixup_get_ivalue(_msg, (gparam_t*)_msg_check, &msg_check)<0) {
			LM_ERR("failed to get msg check flags parameter\n");
			return -1;
		}
	}
	if (_uri_check == NULL) {
		uri_check = default_uri_checks;
	} else {
		if(fixup_get_ivalue(_msg, (gparam_t*)_uri_check, &uri_check)<0) {
			LM_ERR("failed to get uri check flags parameter\n");
			return -1;
		}
	}

	if ((msg_check < 1) || (msg_check >= (SANITY_MAX_CHECKS))) {
		LM_ERR("input parameter (%i) outside of valid range <1-%i)\n",
				msg_check, SANITY_MAX_CHECKS);
		return -1;
	}
	if ((uri_check < 1) || (uri_check >= (SANITY_URI_MAX_CHECKS))) {
		LM_ERR("second input parameter (%i) outside of valid range <1-%i\n",
				uri_check, SANITY_URI_MAX_CHECKS);
		return -1;
	}

	ret = sanity_check(_msg, msg_check, uri_check);
	LM_DBG("sanity checks result: %d\n", ret);
	if(_sanity_drop!=0)
		return ret;
	return (ret==SANITY_CHECK_FAILED)?-1:ret;
}

/**
 *
 */
static int ki_sanity_check(sip_msg_t *msg, int mflags, int uflags)
{
	int ret;
	ret =  sanity_check(msg, mflags, uflags);
	return (ret==SANITY_CHECK_FAILED)?-1:ret;
}

/**
 *
 */
static int ki_sanity_check_defaults(sip_msg_t *msg)
{
	int ret;
	ret =  sanity_check(msg, default_msg_checks, default_uri_checks);
	return (ret==SANITY_CHECK_FAILED)?-1:ret;
}

/**
 *
 */
static int w_sanity_reply(sip_msg_t* _msg, char* _p1, char* _p2)
{
	return ki_sanity_reply(_msg);
}

/**
 * load sanity module API
 */
static int bind_sanity(sanity_api_t* api)
{
	if (!api) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}
	api->check          = sanity_check;
	api->check_defaults = sanity_check_defaults;

	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_sanity_exports[] = {
	{ str_init("sanity"), str_init("sanity_check"),
		SR_KEMIP_INT, ki_sanity_check,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sanity"), str_init("sanity_check_defaults"),
		SR_KEMIP_INT, ki_sanity_check_defaults,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sanity"), str_init("sanity_reply"),
		SR_KEMIP_INT, ki_sanity_reply,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_sanity_exports);
	return 0;
}
