/*
 * $Id$
 *
 * DNSSEC module
 *
 * Copyright (C) 2013 mariuszbi@gmail.com
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
 *
 * History:
 * --------
 *  2013-03	initial implementation
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../dns_func.h"

#include "dnssec_func.h"

MODULE_VERSION


static int dnssec_init(void);
static int dnssec_exit(void);


/* parameters */
static int time_unit = 2;

/* global variables */
gen_lock_t*             timer_lock=0;
struct list_link*       timer = 0;


static cmd_export_t cmds[]={
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"sampling_time_unit",    INT_PARAM,  &time_unit},
	{0,0,0}
};


static mi_export_t mi_cmds [] = {
	{0,0,0,0,0}
};


struct module_exports exports= {
	"dnssec",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,           /* exported statistics */
	mi_cmds,     /* exported MI functions */
	0,           /* exported pseudo-variables */
	0,           /* extra processes */
	dnssec_init,   /* module initialization function */
	0,
	(destroy_function) dnssec_exit,   /* module exit function */
	0  /* per-child init function */
};


static void load_dns(void)
{
	struct dns_func_t *f = pkg_malloc(sizeof(struct dns_func_t));
	f->sr_res_init = dnssec_res_init;
	f->sr_gethostbyname = dnssec_gethostbyname;
	f->sr_gethostbyname2 = dnssec_gethostbyname2;
	f->sr_res_search = dnssec_res_search;

	load_dnsfunc(f);
}

static int dnssec_init(void)
{
	LOG(L_INFO, "DNSSEC  - initializing\n");

/*	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
*/
	load_dns();
	{
		LM_ERR("loaded dnssec wrappers\n");
	}
	/* load dnssec resolver wrappers */
	return 0;
}



static int dnssec_exit(void)
{

	return 0;
}


