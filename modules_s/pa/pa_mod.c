/*
 * Presence Agent, module interface
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * 2003-03-11  updated to the new module exports interface (andrei)
 * 2003-03-16  flags export parameter added (janakj)
 */


#include "../../sr_module.h"
#include "../../error.h"
#include "subscribe.h"
#include "dlist.h"
#include "pa_mod.h"


MODULE_VERSION


static int mod_init(void);  /* Module initialization function */
static void destroy(void);  /* Module destroy function */
static int subscribe_fixup(void** param, int param_no); /* domain name -> domain pointer */
static void timer(unsigned int ticks, void* param); /* Delete timer for all domains */

int default_expires = 3600;  /* Default expires value if not present in the message */
int timer_interval = 10;     /* Expiration timer interval in seconds */

/** TM bind */
struct tm_binds tmb;


/*
 * Exported functions
 */
static cmd_export_t cmds[]={
	{"handle_subscription",   handle_subscription,   1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"existing_subscription", existing_subscription, 1, subscribe_fixup, REQUEST_ROUTE                },
	{"pua_exists",            pua_exists,            1, subscribe_fixup, REQUEST_ROUTE                },
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[]={
	{"default_expires", INT_PARAM, &default_expires},
	{"timer_interval",  INT_PARAM, &timer_interval },
	{0, 0, 0}
};


struct module_exports exports = {
	"pa", 
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	mod_init, /* module initialization function */
	0,        /* response function*/
	destroy,  /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	load_tm_f load_tm;

	DBG("Presence Agent - initializing\n");

	     /* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "Can't import tm\n");
		return -1;
	}
	     /* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) {
		return -1;
	}
	
	     /* Register cache timer */
	register_timer(timer, 0, timer_interval);

	return 0;
}


static void destroy(void)
{
	free_all_pdomains();
}


/*
 * Convert char* parameter to udomain_t* pointer
 */
static int subscribe_fixup(void** param, int param_no)
{
	pdomain_t* d;

	if (param_no == 1) {
		if (register_pdomain((char*)*param, &d) < 0) {
			LOG(L_ERR, "subscribe_fixup(): Error while registering domain\n");
			return E_UNSPEC;
		}

		*param = (void*)d;
	}
	return 0;
}


/*
 * Timer handler
 */
static void timer(unsigned int ticks, void* param)
{
	if (timer_all_pdomains() != 0) {
		LOG(L_ERR, "timer(): Error while synchronizing domains\n");
	}
}
