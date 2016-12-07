/*
 * $Id$
 *
 * PIKE module
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-11  converted to the new locking interface: locking.h --
 *               major changes (andrei)
 *  2003-03-16  flags export parameter added (janakj)
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
#include "../../mem/shm_mem.h"
#include "../../timer.h"
#include "../../locking.h"
#include "ip_tree.h"
#include "timer.h"
#include "pike_funcs.h"
#include "rpc.h"

MODULE_VERSION



static int pike_init(void);
static int pike_exit(void);



/* parameters */
static unsigned int time_unit = 2;
static unsigned int max_reqs  = 30;
unsigned int timeout   = 120;

/* global variables */
gen_lock_t*             timer_lock=0;
struct list_link*       timer = 0;


static int fixup_str2int( void** param, int param_no)
{
	unsigned long go_to;
	int err;

	if (param_no == 1) {
		go_to = str2s(*param, strlen(*param), &err);
		if (err == 0) {
			pkg_free(*param);
			*param = (void *)go_to;
			return 0;
		} else {
			LOG(L_ERR, "ERROR: fixup_str2int: bad number <%s>\n",
				(char *)(*param));
			return E_CFG;
		}
	}
	return 0;
}


static int pike_check_req_0(struct sip_msg* msg, char* foo, char* bar)
{
	return pike_check_req(msg, (char*)(long)0,bar);
}


static cmd_export_t cmds[]={
	{"pike_check_req",  pike_check_req_0,  0,  0, REQUEST_ROUTE},
	{"pike_check_req",  pike_check_req,  1,  fixup_str2int, REQUEST_ROUTE},
	{0,0,0,0,0}
};

static param_export_t params[]={
	{"sampling_time_unit",    PARAM_INT,  &time_unit},
	{"reqs_density_per_unit", PARAM_INT,  &max_reqs},
	{"remove_latency",        PARAM_INT,  &timeout},
	{0,0,0}
};


struct module_exports exports= {
	"pike",
	cmds,
	pike_rpc_methods,           /* RPC methods */
	params,

	pike_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) pike_exit,   /* module exit function */
	0,
	0  /* per-child init function */
};




static int pike_init(void)
{
	LOG(L_INFO,"PIKE - initializing\n");

	/* alloc the timer lock */
	timer_lock=lock_alloc();
	if (timer_lock==0) {
		LOG(L_ERR,"ERROR:pike_init: alloc locks failed!\n");
		goto error1;
	}
	/* init the lock */
	if (lock_init(timer_lock)==0){
		LOG(L_ERR, "ERROR:pike_init: init lock failed\n");
		goto error1;
	}

	/* init the IP tree */
	if ( init_ip_tree(max_reqs)!=0 ) {
		LOG(L_ERR,"ERROR:pike_init: ip_tree creation failed!\n");
		goto error2;
	}

	/* init timer list */
	timer = (struct list_link*)shm_malloc(sizeof(struct list_link));
	if (timer==0) {
		LOG(L_ERR,"ERROR:pike_init: cannot alloc shm mem for timer!\n");
		goto error3;
	}
	timer->next = timer->prev = timer;

	/* registering timing functions  */
	register_timer( clean_routine , 0, 1 );
	register_timer( swap_routine , 0, time_unit );

	return 0;
error3:
	destroy_ip_tree();
error2:
	lock_destroy(timer_lock);
error1:
	if (timer_lock) lock_dealloc(timer_lock);
	timer_lock = 0;
	return -1;
}



static int pike_exit(void)
{
	LOG(L_INFO,"PIKE - destroying module\n");

	/* destroy semaphore */
	if (timer_lock) {
		lock_destroy(timer_lock);
		lock_dealloc(timer_lock);
	}

	/* empty the timer list head */
	if (timer) {
		shm_free(timer);
		timer = 0;
	}

	/* destroy the IP tree */
	destroy_ip_tree();

	return 0;
}


