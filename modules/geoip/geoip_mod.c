/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../pvar.h"
#include "../../mod_fix.h"

#include "geoip_pv.h"

MODULE_VERSION

static char *geoip_path = NULL;

static int  mod_init(void);
static void mod_destroy(void);

static int w_geoip_match(struct sip_msg* msg, char* str1, char* str2);
static int geoip_match(struct sip_msg *msg, gparam_t *target, gparam_t *pvname);

static pv_export_t mod_pvs[] = {
	{ {"gip", sizeof("git")-1}, PVT_OTHER, pv_get_geoip, 0,
		pv_parse_geoip_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static cmd_export_t cmds[]={
	{"geoip_match", (cmd_function)w_geoip_match, 2, fixup_spve_spve,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"path",     PARAM_STRING, &geoip_path},
	{0, 0, 0}
};

struct module_exports exports = {
	"geoip",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	mod_pvs,        /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	0               /* per child init function */
};



/**
 * init module function
 */
static int mod_init(void)
{

	if(geoip_path==NULL || strlen(geoip_path)==0)
	{
		LM_ERR("path to GeoIP database file not set\n");
		return -1;
	}

	if(geoip_init_pv(geoip_path)!=0)
	{
		LM_ERR("cannot init for database file at: %s\n", geoip_path);
		return -1;
	}
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	geoip_destroy_pv();
}

static int w_geoip_match(struct sip_msg* msg, char* str1, char* str2)
{
	return geoip_match(msg, (gparam_t*)str1, (gparam_t*)str2);
}

static int geoip_match(struct sip_msg *msg, gparam_t *target, gparam_t *pvname)
{
	str tomatch;
	str pvclass;
	
	if(msg==NULL)
	{
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg, target, &tomatch)<0)
	{
		LM_ERR("cannot get the address\n");
		return -1;
	}
	if(fixup_get_svalue(msg, pvname, &pvclass)<0)
	{
		LM_ERR("cannot get the pv class\n");
		return -1;
	}
	geoip_pv_reset(&pvclass);

	return geoip_update_pv(&tomatch, &pvclass);
}

