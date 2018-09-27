/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/*!
 * \brief DNSsec support
 * \ingroup DNSsec
 * \author mariuszbi@gmail.com
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../core/sr_module.h"
#include "../../core/error.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/dns_func.h"

#include "dnssec_func.h"

MODULE_VERSION


static int dnssec_init(void);
static int dnssec_exit(void);


/* parameters */
static unsigned int flags=0;

/* global variables */
gen_lock_t*             timer_lock=0;
struct list_link*       timer = 0;


static param_export_t params[]={
	{"general_query_flags", INT_PARAM, &flags},
	{0,0,0}
};


struct module_exports exports= {
	"dnssec",			/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	0,					/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	dnssec_init,		/* module initialization function */
	0,					/* per-child init function */
	dnssec_exit			/* module destroy function */
};


static int load_dns(void)
{
	struct dns_func_t *f = pkg_malloc(sizeof(struct dns_func_t));
	if( NULL == f ) {
		return -1;
	}
	memset(f, 0, sizeof(struct dns_func_t));
	f->sr_res_init = dnssec_res_init;
	f->sr_gethostbyname = dnssec_gethostbyname;
	f->sr_gethostbyname2 = dnssec_gethostbyname2;
	f->sr_res_search = dnssec_res_search;

	load_dnsfunc(f);
	return 0;
}

static int dnssec_init(void)
{
	LOG(L_INFO, "DNSSEC  - initializing\n");

	//set parameters
	if(flags) set_context_flags(flags);

	if(load_dns() != 0) {
		LM_ERR("loaded dnssec wrappers failed\n");
	}
	/* load dnssec resolver wrappers */
	return 0;
}



static int dnssec_exit(void)
{
	(void)dnssec_res_destroy();
	return 0;
}


