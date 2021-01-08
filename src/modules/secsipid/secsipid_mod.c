/**
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <secsipid.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/data_lump.h"
#include "../../core/kemi.h"

MODULE_VERSION

static int secsipid_expire = 300;
static int secsipid_timeout = 5;

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_secsipid_check_identity(sip_msg_t *msg, char *pkeypath, char *str2);
static int w_secsipid_add_identity(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeypath);


/* clang-format off */
static cmd_export_t cmds[]={
	{"secsipid_check_identity", (cmd_function)w_secsipid_check_identity, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"secsipid_add_identity", (cmd_function)w_secsipid_add_identity, 6,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"expire",     PARAM_INT,   &secsipid_expire},
	{"timeout",    PARAM_INT,   &secsipid_timeout},
	{0, 0, 0}
};

struct module_exports exports = {
	"secsipid",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy    	/* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

#define SECSIPID_HDR_IDENTITY "Identity"
#define SECSIPID_HDR_IDENTITY_LEN (sizeof(SECSIPID_HDR_IDENTITY) - 1)

/**
 *
 */
static int ki_secsipid_check_identity(sip_msg_t *msg, str *keypath)
{
	int ret = 1;
	str ibody = STR_NULL;
	hdr_field_t *hf;

	for (hf=msg->headers; hf; hf=hf->next) {
		if (hf->name.len==SECSIPID_HDR_IDENTITY_LEN
				&& strncasecmp(hf->name.s, SECSIPID_HDR_IDENTITY,
					SECSIPID_HDR_IDENTITY_LEN)==0)
			break;
	}

	if(hf == NULL) {
		LM_DBG("no identity header\n");
		return -1;
	}

	ibody = hf->body;

	ret = SecSIPIDCheckFull(ibody.s, ibody.len, secsipid_expire, keypath->s,
			secsipid_timeout);

	if(ret==0) {
		LM_DBG("identity check: ok\n");
		return 1;
	}

	LM_DBG("identity check: failed\n");
	return -1;
}

/**
 *
 */
static int w_secsipid_check_identity(sip_msg_t *msg, char *pkeypath, char *str2)
{
	str keypath = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pkeypath, &keypath)<0) {
		LM_ERR("failed to get keypath parameter\n");
		return -1;
	}

	return ki_secsipid_check_identity(msg, &keypath);
}

/**
 *
 */
static int ki_secsipid_add_identity(sip_msg_t *msg, str *origtn, str *desttn,
			str *attest, str *origid, str *x5u, str *keypath)
{
	str ibody = STR_NULL;
	str hdr = STR_NULL;
	sr_lump_t *anchor = NULL;

	ibody.len = SecSIPIDGetIdentity(origtn->s, desttn->s, attest->s, origid->s,
			x5u->s, keypath->s, &ibody.s);

	if(ibody.len<=0) {
		goto error;
	}

	LM_DBG("appending identity: %.*s\n", ibody.len, ibody.s);
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		goto error;
	}

	hdr.len = SECSIPID_HDR_IDENTITY_LEN + 1 + 1 + ibody.len + 2;
	hdr.s = (char*)pkg_malloc(hdr.len + 1);
	if(hdr.s==NULL) {
		PKG_MEM_ERROR;
		goto error;
	}
	memcpy(hdr.s, SECSIPID_HDR_IDENTITY, SECSIPID_HDR_IDENTITY_LEN);
	*(hdr.s + SECSIPID_HDR_IDENTITY_LEN) = ':';
	*(hdr.s + SECSIPID_HDR_IDENTITY_LEN + 1) = ' ';

	memcpy(hdr.s + SECSIPID_HDR_IDENTITY_LEN + 2, ibody.s, ibody.len);
	*(hdr.s + SECSIPID_HDR_IDENTITY_LEN + ibody.len + 2) = '\r';
	*(hdr.s + SECSIPID_HDR_IDENTITY_LEN + ibody.len + 3) = '\n';

	/* anchor after last header */
	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if((anchor==NULL)
			|| (insert_new_lump_before(anchor, hdr.s, hdr.len, 0) == 0)) {
		LM_ERR("cannot insert identity header\n");
		pkg_free(hdr.s);
		goto error;
	}

	if(ibody.s) {
		free(ibody.s);
	}
	return 1;

error:
	if(ibody.s) {
		free(ibody.s);
	}
	return -1;
}

/**
 *
 */
static int w_secsipid_add_identity(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeypath)
{
	str origtn = STR_NULL;
	str desttn = STR_NULL;
	str attest = STR_NULL;
	str origid = STR_NULL;
	str x5u = STR_NULL;
	str keypath = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)porigtn, &origtn)<0) {
		LM_ERR("failed to get origtn parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pdesttn, &desttn)<0) {
		LM_ERR("failed to get desttn parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pattest, &attest)<0) {
		LM_ERR("failed to get attest parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)porigid, &origid)<0) {
		LM_ERR("failed to get origid parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)px5u, &x5u)<0) {
		LM_ERR("failed to get x5u parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pkeypath, &keypath)<0) {
		LM_ERR("failed to get keypath parameter\n");
		return -1;
	}

	return ki_secsipid_add_identity(msg, &origtn, &desttn,
			&attest, &origid, &x5u, &keypath);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_secsipid_exports[] = {
	{ str_init("secsipid"), str_init("secsipid_check_identity"),
		SR_KEMIP_INT, ki_secsipid_check_identity,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("secsipid"), str_init("secsipid_add_identity"),
		SR_KEMIP_INT, ki_secsipid_add_identity,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_secsipid_exports);
	return 0;
}