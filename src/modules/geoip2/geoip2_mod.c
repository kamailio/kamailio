/**
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

#include "geoip2_pv.h"

MODULE_VERSION

static char *geoip2_path = NULL;

static int  mod_init(void);
static void mod_destroy(void);

static int w_geoip2_match(struct sip_msg* msg, char* str1, char* str2);
static int geoip2_match(sip_msg_t *msg, str *tomatch, str *pvclass);

static pv_export_t mod_pvs[] = {
	{ {"gip2", sizeof("gip2")-1}, PVT_OTHER, pv_get_geoip2, 0,
		pv_parse_geoip2_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static cmd_export_t cmds[]={
	{"geoip2_match", (cmd_function)w_geoip2_match, 2, fixup_spve_spve,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"path",     PARAM_STRING, &geoip2_path},
	{0, 0, 0}
};

struct module_exports exports = {
	"geoip2",			/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	mod_pvs,			/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	0,					/* per-child init function */
	mod_destroy			/* module destroy function */
};


/**
 * init module function
 */
static int mod_init(void)
{
	LM_INFO("using GeoIP library version %s\n", MMDB_lib_version());

	if(geoip2_path==NULL || strlen(geoip2_path)==0)
	{
		LM_ERR("path to GeoIP database file not set\n");
		return -1;
	}

	if(geoip2_init_pv(geoip2_path)!=0)
	{
		LM_ERR("cannot init for database file at: %s\n", geoip2_path);
		return -1;
	}
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	geoip2_destroy_pv();
}

static int geoip2_match(sip_msg_t *msg, str *tomatch, str *pvclass)
{
	geoip2_pv_reset(pvclass);

	return geoip2_update_pv(tomatch, pvclass);
}

static int w_geoip2_match(sip_msg_t* msg, char* target, char* pvname)
{
	str tomatch = STR_NULL;
	str pvclass = STR_NULL;

	if(msg==NULL) {
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)target, &tomatch)<0) {
		LM_ERR("cannot get the address\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pvname, &pvclass)<0) {
		LM_ERR("cannot get the pv class\n");
		return -1;
	}

	return geoip2_match(msg, &tomatch, &pvclass);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_geoip2_exports[] = {
    { str_init("geoip2"), str_init("match"),
        SR_KEMIP_INT, geoip2_match,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2) {
    sr_kemi_modules_add(sr_kemi_geoip2_exports);
    return 0;
}