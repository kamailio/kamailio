/**
 *
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/kemi.h"
#include "../../core/mod_fix.h"

#include "phonenum_pv.h"

MODULE_VERSION

static int phonenum_smode = 0;

static int mod_init(void);
static void mod_destroy(void);

static int w_phonenum_match(struct sip_msg *msg, char *str1, char *str2);
static int w_phonenum_match_cn(
		struct sip_msg *msg, char *str1, char *str2, char *str3);
static int phonenum_match(sip_msg_t *msg, str *tomatch, str *pvclass);

static int phonenum_resid_param(modparam_t type, void *val);

/* clang-format off */
static pv_export_t mod_pvs[] = {
	{ {"phn", sizeof("phn")-1}, PVT_OTHER, pv_get_phonenum, 0,
		pv_parse_phonenum_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"phonenum_match", (cmd_function)w_phonenum_match, 2, fixup_spve_spve,
		0, ANY_ROUTE},
	{"phonenum_match_cn", (cmd_function)w_phonenum_match_cn, 3, fixup_spve_all,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"smode", PARAM_INT, &phonenum_smode},
	{"resid", PARAM_STR|PARAM_USE_FUNC, &phonenum_resid_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"phonenum",      /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	0,               /* per-child init function */
	mod_destroy      /* module destroy function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{

	if(phonenum_init_pv(phonenum_smode) != 0) {
		LM_ERR("cannot do init\n");
		return -1;
	}
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	phonenum_destroy_pv();
}

/**
 *
 */
static int phonenum_resid_param(modparam_t type, void *val)
{
	str rname;

	rname = *((str *)val);
	if(sr_phonenum_add_resid(&rname) < 0) {
		LM_ERR("failed to register result container with id: %.*s\n", rname.len,
				rname.s);
		return -1;
	}
	return 0;
}

static int phonenum_match(sip_msg_t *msg, str *tomatch, str *pvclass)
{
	phonenum_pv_reset(pvclass);

	return phonenum_update_pv(tomatch, NULL, pvclass);
}

static int w_phonenum_match(sip_msg_t *msg, char *target, char *pvname)
{
	str tomatch = STR_NULL;
	str pvclass = STR_NULL;

	if(msg == NULL) {
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)target, &tomatch) < 0) {
		LM_ERR("cannot get the address\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pvname, &pvclass) < 0) {
		LM_ERR("cannot get the pv class\n");
		return -1;
	}

	return phonenum_match(msg, &tomatch, &pvclass);
}

static int phonenum_match_cn(
		sip_msg_t *msg, str *tomatch, str *cnc, str *pvclass)
{
	phonenum_pv_reset(pvclass);

	return phonenum_update_pv(tomatch, cnc, pvclass);
}

static int w_phonenum_match_cn(
		sip_msg_t *msg, char *target, char *cncstr, char *pvname)
{
	str tomatch = STR_NULL;
	str pvclass = STR_NULL;
	str cncval = STR_NULL;

	if(msg == NULL) {
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)target, &tomatch) < 0) {
		LM_ERR("cannot get the address\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)cncstr, &cncval) < 0) {
		LM_ERR("cannot get the country code\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pvname, &pvclass) < 0) {
		LM_ERR("cannot get the pv class\n");
		return -1;
	}

	return phonenum_match_cn(msg, &tomatch, &cncval, &pvclass);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_phonenum_exports[] = {
	{ str_init("phonenum"), str_init("match"),
		SR_KEMIP_INT, phonenum_match,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_phonenum_exports);
	return 0;
}
