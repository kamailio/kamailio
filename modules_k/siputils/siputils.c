/*
 * $Id$
 *
 * Copyright (C) 2008-2009 1&1 Internet AG
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/**
 * \file
 * \brief Module with several utiltity functions related to SIP messages handling
 * \ingroup utils
 * - Module; \ref utils
 */

/*!
 * \defgroup utils UTILS :: Module definitions
 */

#include <assert.h>

#include "../../sr_module.h"
#include "../../script_cb.h"
#include "../../locking.h"
#include "../../ut.h"

#include "ring.h"
#include "options.h"


MODULE_VERSION

gen_lock_t *ring_lock = NULL;
unsigned int ring_timeout = 0;
/* for options functionality */
str opt_accept = str_init(ACPT_DEF);
str opt_accept_enc = str_init(ACPT_ENC_DEF);
str opt_accept_lang = str_init(ACPT_LAN_DEF);
str opt_supported = str_init(SUPT_DEF);
/** SL binds */
struct sl_binds opt_slb;

static int mod_init(void);
static void mod_destroy(void);


static cmd_export_t cmds[]={
	{"ring_insert_callid", (cmd_function)ring_insert_callid, 0, ring_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"options_reply", (cmd_function)opt_reply, 0, 0, 0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[] = {
	{"ring_timeout",    INT_PARAM, &ring_timeout},
	{"accept",          STR_PARAM, &opt_accept.s},
	{"accept_encoding", STR_PARAM, &opt_accept_enc.s},
	{"accept_language", STR_PARAM, &opt_accept_lang.s},
	{"support",         STR_PARAM, &opt_supported.s},
	{0, 0, 0}
};


struct module_exports exports= {
	"siputils",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* param exports */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,               /* extra processes */
	mod_init,        /* initialization function */
	0,               /* Response function */
	mod_destroy,     /* Destroy function */
	0,               /* Child init function */
};


static int mod_init(void)
{
	if(ring_timeout > 0) {
		ring_init_hashtable();

		ring_lock = lock_alloc();
		assert(ring_lock);
		if (lock_init(ring_lock) == 0) {
			LM_CRIT("cannot initialize lock.\n");
			return -1;
		}
		if (register_script_cb(ring_filter, PRE_SCRIPT_CB|RPL_TYPE_CB, 0) != 0) {
			LM_ERR("could not insert callback");
			return -1;
		}
	}

		/* load the SL API */
	if (load_sl_api(&opt_slb)!=0) {
		LM_ERR("can't load SL API\n");
		return -1;
	}

	opt_accept.len = strlen(opt_accept.s);
	opt_accept_enc.len = strlen(opt_accept_enc.s);
	opt_accept_lang.len = strlen(opt_accept_lang.s);
	opt_supported.len = strlen(opt_supported.s);

	return 0;
}


static void mod_destroy(void)
{
	if (ring_lock) {
		lock_destroy(ring_lock);
		lock_dealloc((void *)ring_lock);
		ring_lock = NULL;
	}

	ring_destroy_hashtable();
}
