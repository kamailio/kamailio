/* 
 * Diversion Header Field Support
 *
 * Copyright (C) 2004 FhG Fokus
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

#include <stdio.h>
#include <string.h>
#include "../../core/sr_module.h"
#include "../../core/error.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/data_lump.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/kemi.h"


MODULE_VERSION

#define DIVERSION_HF "Diversion"
#define DIVERSION_HF_LEN (sizeof(DIVERSION_HF) - 1)

#define DIVERSION_PREFIX DIVERSION_HF ": <"
#define DIVERSION_PREFIX_LEN (sizeof(DIVERSION_PREFIX) - 1)

#define DIVERSION_SUFFIX ">;reason="
#define DIVERSION_SUFFIX_LEN (sizeof(DIVERSION_SUFFIX) - 1)


str suffix = {"", 0};

int w_add_diversion(struct sip_msg *msg, char *r, char *u);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
		{"add_diversion", (cmd_function)w_add_diversion, 1, fixup_spve_null, 0,
				REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
		{"add_diversion", (cmd_function)w_add_diversion, 2, fixup_spve_spve, 0,
				REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
		{0, 0, 0, 0, 0, 0}};


/*
 * Exported parameters
 */
static param_export_t params[] = {{"suffix", PARAM_STR, &suffix}, {0, 0, 0}};


/*
 * Module interface
 */
struct module_exports exports = {
		"diversion",	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* RPC method exports */
		0,				 /* exported pseudo-variables */
		0,				 /* response handling function */
		0,				 /* module initialization function */
		0,				 /* per-child init function */
		0				 /* module destroy function */
};


static inline int add_diversion_helper(struct sip_msg *msg, str *s)
{
	char *ptr;
	int is_ref;

	struct lump *anchor = 0;

	if(!msg->diversion && parse_headers(msg, HDR_DIVERSION_F, 0) == -1) {
		LM_ERR("header parsing failed\n");
		return -1;
	}

	if(msg->diversion) {
		/* Insert just before the topmost Diversion header */
		ptr = msg->diversion->name.s;
	} else {
		/* Insert at the end */
		ptr = msg->unparsed;
	}

	anchor = anchor_lump2(msg, ptr - msg->buf, 0, 0, &is_ref);
	if(!anchor) {
		LM_ERR("can't get anchor\n");
		return -2;
	}

	if(!insert_new_lump_before(anchor, s->s, s->len, 0)) {
		LM_ERR("can't insert lump\n");
		return -3;
	}

	return 0;
}


int add_diversion_uri(sip_msg_t *msg, str *reason, str *uri)
{
	str div_hf;
	char *at;

	if(reason == NULL || reason->s == NULL || uri == NULL || uri->s == NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	div_hf.len = DIVERSION_PREFIX_LEN + uri->len + DIVERSION_SUFFIX_LEN
				 + reason->len + CRLF_LEN;
	div_hf.s = pkg_malloc(div_hf.len);
	if(!div_hf.s) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}

	at = div_hf.s;
	memcpy(at, DIVERSION_PREFIX, DIVERSION_PREFIX_LEN);
	at += DIVERSION_PREFIX_LEN;

	memcpy(at, uri->s, uri->len);
	at += uri->len;

	memcpy(at, DIVERSION_SUFFIX, DIVERSION_SUFFIX_LEN);
	at += DIVERSION_SUFFIX_LEN;

	memcpy(at, reason->s, reason->len);
	at += reason->len;

	memcpy(at, CRLF, CRLF_LEN);

	if(add_diversion_helper(msg, &div_hf) < 0) {
		pkg_free(div_hf.s);
		return -1;
	}

	return 1;
}

int w_add_diversion(struct sip_msg *msg, char *r, char *u)
{
	str uri;
	str reason;

	if(fixup_get_svalue(msg, (gparam_t *)r, &reason) < 0) {
		LM_ERR("cannot get the reason parameter\n");
		return -1;
	}

	if(u == NULL) {
		if(parse_sip_msg_uri(msg) < 0) {
			LM_ERR("failed to parse sip msg uri\n");
			return -1;
		}
		uri = msg->first_line.u.request.uri;
	} else {
		if(fixup_get_svalue(msg, (gparam_t *)u, &uri) < 0) {
			LM_ERR("cannot get the uri parameter\n");
			return -1;
		}
	}
	return add_diversion_uri(msg, &reason, &uri);
}

/**
 *
 */
static int ki_add_diversion(sip_msg_t *msg, str *reason)
{
	if(parse_sip_msg_uri(msg) < 0) {
		LM_ERR("failed to parse sip msg uri\n");
		return -1;
	}

	return add_diversion_uri(msg, reason, &msg->first_line.u.request.uri);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_diversion_exports[] = {
	{ str_init("diversion"), str_init("add_diversion"),
		SR_KEMIP_INT, ki_add_diversion,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("diversion"), str_init("add_diversion_uri"),
		SR_KEMIP_INT, add_diversion_uri,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_diversion_exports);
	return 0;
}
