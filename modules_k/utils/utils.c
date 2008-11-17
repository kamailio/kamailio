/*
 * $Id$
 *
 * Copyright (C) 2008-2009 1&1 Internet AG
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

#include "ring.h"


MODULE_VERSION

gen_lock_t *ring_lock = NULL;
unsigned int ring_timeout = 30;
unsigned int ring_activate = 0;

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);


static cmd_export_t cmds[]={
	{ "ring_insert_callid", (cmd_function)ring_insert_callid, 0, ring_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE },
	{0,0,0,0,0,0}
};

static param_export_t params[] = {
	{"ring_activate", INT_PARAM, &ring_activate },
	{"ring_timeout",  INT_PARAM, &ring_timeout  },
	{0, 0, 0}
};


struct module_exports exports= {
	"utils",
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
	child_init,      /* Child init function */
};


static int mod_init(void)
{
	return 0;
}


static int child_init(int rank)
{
	/*
	 * delay initialization, to avoid the overhead from the callback
	 * when the ringing functionality is not used
	 */
	if(rank == PROC_MAIN && ring_activate == 1) {
		ring_init_hashtable();

		ring_lock = lock_alloc();
		assert(ring_lock);
		if (lock_init(ring_lock) == 0) {
			LM_CRIT("cannot initialize lock.\n");
			return -1;
		}
		LM_ERR("init CB..");
		if (register_script_cb(ring_filter, PRE_SCRIPT_CB|RPL_TYPE_CB, 0) != 0) {
			LM_ERR("could not insert callback");
			return -1;
		}
	}

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
