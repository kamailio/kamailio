/*
 * Sanity Checks Module
 *
 * Copyright (C) 2006 iptelorg GbmH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_cseq.h"
#include "../../core/parser/contact/contact.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_rr.h"

MODULE_VERSION

#define PROXY_REQUIRE_DEF ""

static str _sanity_prval = STR_STATIC_INIT(PROXY_REQUIRE_DEF);

int default_msg_checks = SANITY_DEFAULT_CHECKS;
int default_uri_checks = SANITY_DEFAULT_URI_CHECKS;
int _sanity_drop = 1;
int ksr_sanity_noreply = 0;

int sn_size_checks = 0;
int sn_size_ruri = 256;
int sn_size_from_uri = 256;
int sn_size_to_uri = 256;
int sn_size_contact_uri = 256;
int sn_size_route_uri = 256;
int sn_size_path_uri = 256;
int sn_size_header = 2048;
int sn_size_headers = 8192;
int sn_size_body = 8192;
int sn_size_message = 16384;
int sn_size_method = 32;

str_list_t *proxyrequire_list = NULL;

sl_api_t _sanity_slb;

static int mod_init(void);
static int w_sanity_check(sip_msg_t *_msg, char *_msg_check, char *_uri_check);
static int w_sanity_reply(sip_msg_t *_msg, char *_p1, char *_p2);
static int bind_sanity(sanity_api_t *api);

/* clang-format off */
/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sanity_check", (cmd_function)w_sanity_check, 0,
		0, 0, REQUEST_ROUTE | ONREPLY_ROUTE},
	{"sanity_check", (cmd_function)w_sanity_check, 1,
		fixup_igp_null, fixup_free_igp_null, REQUEST_ROUTE | ONREPLY_ROUTE},
	{"sanity_check", (cmd_function)w_sanity_check, 2,
		fixup_igp_igp, fixup_free_igp_igp, REQUEST_ROUTE | ONREPLY_ROUTE},
	{"sanity_reply", (cmd_function)w_sanity_reply, 0,
		0, 0, REQUEST_ROUTE | ONREPLY_ROUTE},
	{"bind_sanity", (cmd_function)bind_sanity, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_checks", PARAM_INT, &default_msg_checks},
	{"uri_checks", PARAM_INT, &default_uri_checks},
	{"size_checks", PARAM_INT, &sn_size_checks},
	{"size_message", PARAM_INT, &sn_size_message},
	{"size_method", PARAM_INT, &sn_size_method},
	{"size_ruri", PARAM_INT, &sn_size_ruri},
	{"size_from_uri", PARAM_INT, &sn_size_from_uri},
	{"size_to_uri", PARAM_INT, &sn_size_to_uri},
	{"size_contact_uri", PARAM_INT, &sn_size_contact_uri},
	{"size_route_uri", PARAM_INT, &sn_size_route_uri},
	{"size_path_uri", PARAM_INT, &sn_size_path_uri},
	{"size_header", PARAM_INT, &sn_size_header},
	{"size_headers", PARAM_INT, &sn_size_headers},
	{"size_body", PARAM_INT, &sn_size_body},
	{"proxy_require", PARAM_STR, &_sanity_prval},
	{"autodrop", PARAM_INT, &_sanity_drop},
	{"noreply", PARAM_INT, &ksr_sanity_noreply},
	{0, 0, 0}
};

/*
 * Module description
 */
struct module_exports exports = {
	"sanity",        /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* RPC method exports */
	0,               /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module initialization function */
	0,               /* per-child init function */
	0                /* module destroy function */
};
/* clang-format on */

/*
 * initialize module
 */
static int mod_init(void)
{
	str_list_t *ptr;

	LM_DBG("sanity initializing\n");

	ksr_sanity_info_init();

	/* bind the SL API */
	if(sl_load_api(&_sanity_slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	LM_DBG("parsing proxy requires string:\n");
	ptr = parse_str_list(&_sanity_prval);

	proxyrequire_list = ptr;

	while(ptr != NULL) {
		LM_DBG("string: '%.*s', next: %p\n", ptr->s.len, ptr->s.s, ptr->next);
		ptr = ptr->next;
	}

	return 0;
}

/**
 *
 */
int sanity_check_sizes(sip_msg_t *msg)
{
	str s;
	contact_t *cb = NULL;
	rr_t *rb = NULL;
	hdr_field_t *hf = NULL;

	if(sn_size_message > 0) {
		if(msg->len > sn_size_message) {
			return SANITY_CHECK_FAILED;
		}
	}
	if(sn_size_ruri > 0) {
		if(msg->first_line.type == SIP_REQUEST) {
			if(msg->new_uri.s) {
				if(msg->new_uri.len > sn_size_ruri) {
					return SANITY_CHECK_FAILED;
				}
			} else {
				if(msg->first_line.u.request.uri.len > sn_size_ruri) {
					return SANITY_CHECK_FAILED;
				}
			}
		}
	}
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse to end of headers\n");
		return SANITY_CHECK_FAILED;
	}
	if(sn_size_method > 0) {
		if(msg->first_line.type == SIP_REQUEST) {
			if(msg->first_line.u.request.method.len > sn_size_method) {
				return SANITY_CHECK_FAILED;
			}
		} else {
			if(msg->cseq != NULL) {
				if(get_cseq(msg)->method.len > sn_size_method) {
					return SANITY_CHECK_FAILED;
				}
			}
		}
	}
	if(sn_size_header > 0) {
		for(hf = msg->headers; hf != NULL; hf = hf->next) {
			if(hf->len > sn_size_header) {
				return SANITY_CHECK_FAILED;
			}
		}
	}
	if(sn_size_from_uri > 0) {
		if(parse_from_header(msg) < 0) {
			LM_WARN("cannot parse From header\n");
			return SANITY_CHECK_FAILED;
		}
		if(msg->from == NULL || get_from(msg) == NULL) {
			LM_WARN("no From header\n");
			return SANITY_CHECK_FAILED;
		}
		if(get_from(msg)->uri.len > sn_size_from_uri) {
			return SANITY_CHECK_FAILED;
		}
	}
	if(sn_size_to_uri > 0) {
		if(parse_to_header(msg) < 0) {
			LM_WARN("cannot parse To header\n");
			return SANITY_CHECK_FAILED;
		}
		if(msg->to == NULL || get_to(msg) == NULL) {
			LM_WARN("no To header\n");
			return SANITY_CHECK_FAILED;
		}
		if(get_to(msg)->uri.len > sn_size_to_uri) {
			return SANITY_CHECK_FAILED;
		}
	}
	if(sn_size_body > 0) {
		s.s = get_body(msg);
		if(s.s != NULL) {
			s.len = msg->buf + msg->len - s.s;
			if(s.len > sn_size_body) {
				return SANITY_CHECK_FAILED;
			}
		}
	}
	if(sn_size_headers > 0) {
		if(msg->unparsed == NULL) {
			return SANITY_CHECK_FAILED;
		}
		s.s = msg->buf + msg->first_line.len;
		s.len = msg->unparsed - s.s;
		if(s.len > sn_size_headers) {
			return SANITY_CHECK_FAILED;
		}
	}
	if(sn_size_contact_uri > 0) {
		if(msg->contact != NULL) {
			if(parse_contact_headers(msg) < 0) {
				LM_DBG("failed to parse Contact headers\n");
				return SANITY_CHECK_FAILED;
			}
			for(hf = msg->contact; hf != NULL; hf = hf->next) {
				if(hf->type == HDR_CONTACT_T) {
					for(cb = (((contact_body_t *)hf->parsed)->contacts);
							cb != NULL; cb = cb->next) {
						if(cb->uri.len > sn_size_contact_uri) {
							return SANITY_CHECK_FAILED;
						}
					}
				}
			}
		}
	}
	if(sn_size_route_uri > 0) {
		if(parse_route_headers(msg) < 0) {
			LM_DBG("failed to parse Route headers\n");
			return SANITY_CHECK_FAILED;
		}
		for(hf = msg->route; hf != NULL; hf = next_sibling_hdr(hf)) {
			if(hf->type == HDR_ROUTE_T) {
				for(rb = (rr_t *)hf->parsed; rb != NULL; rb = rb->next) {
					if(rb->nameaddr.uri.len > sn_size_route_uri) {
						return SANITY_CHECK_FAILED;
					}
				}
			}
		}
		if(parse_record_route_headers(msg) < 0) {
			LM_DBG("failed to parse Record-Route headers\n");
			return SANITY_CHECK_FAILED;
		}
		for(hf = msg->record_route; hf != NULL; hf = next_sibling_hdr(hf)) {
			if(hf->type == HDR_RECORDROUTE_T) {
				for(rb = (rr_t *)hf->parsed; rb != NULL; rb = rb->next) {
					if(rb->nameaddr.uri.len > sn_size_route_uri) {
						return SANITY_CHECK_FAILED;
					}
				}
			}
		}
	}
	if(sn_size_path_uri > 0) {
		if(parse_headers(msg, HDR_PATH_F, 0) < 0) {
			LM_DBG("failed to parse Path headers\n");
			return SANITY_CHECK_FAILED;
		}
		for(hf = msg->path; hf != NULL; hf = next_sibling_hdr(hf)) {
			if(hf->type == HDR_PATH_T) {
				if(parse_rr(hf) < 0) {
					LM_DBG("failed to parse Path header body\n");
					return SANITY_CHECK_FAILED;
				}
				for(rb = (rr_t *)hf->parsed; rb != NULL; rb = rb->next) {
					if(rb->nameaddr.uri.len > sn_size_path_uri) {
						return SANITY_CHECK_FAILED;
					}
				}
			}
		}
	}
	if(sn_size_header > 0) {
		for(hf = msg->headers; hf != NULL; hf = hf->next) {
			if(hf->len > sn_size_header) {
				return SANITY_CHECK_FAILED;
			}
		}
	}

	return SANITY_CHECK_PASSED;
}

/**
 * perform SIP message sanity check
 * @param _msg - SIP message structure
 * @param msg_checks - bitmask of sanity tests to perform over message
 * @param uri_checks - bitmask of sanity tests to perform over uri
 * @return -1 on error, 0 on tests failure, 1 on success
 */
int sanity_check(struct sip_msg *_msg, int msg_checks, int uri_checks)
{
	int ret;

	if(ksr_sanity_noreply != 0) {
		ksr_sanity_info_init();
	}

	ret = SANITY_CHECK_PASSED;

	if(((SANITY_CHECK_SIZES & msg_checks) || sn_size_checks)
			&& (ret = sanity_check_sizes(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_RURI_SIP_VERSION & msg_checks
			&& (ret = check_ruri_sip_version(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_RURI_SCHEME & msg_checks
			&& (ret = check_ruri_scheme(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_REQUIRED_HEADERS & msg_checks
			&& (ret = check_required_headers(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_VIA1_HEADER & msg_checks
			&& (ret = check_via1_header(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_RFC3261_COMPLIANCE & msg_checks
			&& (ret = check_rfc3261_compliance(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_VIA_SIP_VERSION & msg_checks
			&& (ret = check_via_sip_version(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_VIA_PROTOCOL & msg_checks
			&& (ret = check_via_protocol(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_CSEQ_METHOD & msg_checks
			&& (ret = check_cseq_method(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_CSEQ_VALUE & msg_checks
			&& (ret = check_cseq_value(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_CL & msg_checks
			&& (ret = check_cl(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_EXPIRES_VALUE & msg_checks
			&& (ret = check_expires_value(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_PROXY_REQUIRE & msg_checks
			&& (ret = check_proxy_require(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_PARSE_URIS & msg_checks
			&& (ret = check_parse_uris(_msg, uri_checks))
					   != SANITY_CHECK_PASSED) {
		goto done;
	}

	if(SANITY_CHECK_DIGEST & msg_checks
			&& (ret = check_digest(_msg, uri_checks)) != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_CHECK_AUTHORIZATION & msg_checks
			&& (ret = check_authorization(_msg, uri_checks))
					   != SANITY_CHECK_PASSED) {
		goto done;
	}
	if(SANITY_CHECK_DUPTAGS & msg_checks
			&& (ret = check_duptags(_msg)) != SANITY_CHECK_PASSED) {
		goto done;
	}

done:
	return ret;
}

/**
 * do default checks
 */
int sanity_check_defaults(struct sip_msg *msg)
{
	return sanity_check(msg, default_msg_checks, default_uri_checks);
}

/**
 * wrapper for sanity_check() to be used from config file
 */
static int w_sanity_check(sip_msg_t *_msg, char *_msg_check, char *_uri_check)
{
	int ret, msg_check, uri_check;

	if(_msg_check == NULL) {
		msg_check = default_msg_checks;
	} else {
		if(fixup_get_ivalue(_msg, (gparam_t *)_msg_check, &msg_check) < 0) {
			LM_ERR("failed to get msg check flags parameter\n");
			return -1;
		}
	}
	if(_uri_check == NULL) {
		uri_check = default_uri_checks;
	} else {
		if(fixup_get_ivalue(_msg, (gparam_t *)_uri_check, &uri_check) < 0) {
			LM_ERR("failed to get uri check flags parameter\n");
			return -1;
		}
	}

	if((msg_check < 1) || (msg_check >= (SANITY_MAX_CHECKS))) {
		LM_ERR("input parameter (%i) outside of valid range <1-%i)\n",
				msg_check, SANITY_MAX_CHECKS);
		return -1;
	}
	if((uri_check < 1) || (uri_check >= (SANITY_URI_MAX_CHECKS))) {
		LM_ERR("second input parameter (%i) outside of valid range <1-%i\n",
				uri_check, SANITY_URI_MAX_CHECKS);
		return -1;
	}

	ret = sanity_check(_msg, msg_check, uri_check);
	LM_DBG("sanity checks result: %d\n", ret);
	if(_sanity_drop != 0)
		return ret;
	return (ret == SANITY_CHECK_FAILED) ? -1 : ret;
}

/**
 *
 */
static int ki_sanity_check(sip_msg_t *msg, int mflags, int uflags)
{
	int ret;
	ret = sanity_check(msg, mflags, uflags);
	return (ret == SANITY_CHECK_FAILED) ? -1 : ret;
}

/**
 *
 */
static int ki_sanity_check_defaults(sip_msg_t *msg)
{
	int ret;
	ret = sanity_check(msg, default_msg_checks, default_uri_checks);
	return (ret == SANITY_CHECK_FAILED) ? -1 : ret;
}

/**
 *
 */
static int w_sanity_reply(sip_msg_t *_msg, char *_p1, char *_p2)
{
	return ki_sanity_reply(_msg);
}

/**
 * load sanity module API
 */
static int bind_sanity(sanity_api_t *api)
{
	if(!api) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}
	api->check = sanity_check;
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
