/*
 * PIKE module
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
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
#include "../../core/mem/shm_mem.h"
#include "../../core/timer.h"
#include "../../core/locking.h"
#include "../../core/kemi.h"
#include "ip_tree.h"
#include "timer.h"
#include "pike_funcs.h"
#include "../../core/rpc_lookup.h"
#include "pike_rpc.h"

MODULE_VERSION



static int pike_init(void);
void pike_exit(void);



/* parameters */
static int time_unit = 2;
static int max_reqs  = 30;
int timeout   = 120;
int pike_log_level = L_WARN;

/* global variables */
gen_lock_t*             timer_lock=0;
struct list_link*       timer = 0;


static cmd_export_t cmds[]={
	{"pike_check_req", (cmd_function)w_pike_check_req,  0,  0, 0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"sampling_time_unit",    INT_PARAM,  &time_unit},
	{"reqs_density_per_unit", INT_PARAM,  &max_reqs},
	{"remove_latency",        INT_PARAM,  &timeout},
	{"pike_log_level",        INT_PARAM, &pike_log_level},
	{0,0,0}
};


struct module_exports exports= {
	"pike",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	0,               /* exported pseudo-variables */
	0,               /* response handling function */
	pike_init,       /* module initialization function */
	0,               /* per-child init function */
	pike_exit        /* module exit function */
};




static int pike_init(void)
{
	LOG(L_INFO, "PIKE - initializing\n");

	if (rpc_register_array(pike_rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* alloc the timer lock */
	timer_lock=lock_alloc();
	if (timer_lock==0) {
		LM_ERR(" alloc locks failed!\n");
		goto error1;
	}
	/* init the lock */
	if (lock_init(timer_lock)==0){
		LM_ERR(" init lock failed\n");
		goto error1;
	}

	/* init the IP tree */
	if ( init_ip_tree(max_reqs)!=0 ) {
		LM_ERR(" ip_tree creation failed!\n");
		goto error2;
	}

	/* init timer list */
	timer = (struct list_link*)shm_malloc(sizeof(struct list_link));
	if (timer==0) {
		LM_ERR(" cannot alloc shm mem for timer!\n");
		goto error3;
	}
	timer->next = timer->prev = timer;

	/* registering timing functions  */
	register_timer( clean_routine , 0, 1 );
	register_timer( swap_routine , 0, time_unit );

	/* Register counter */
	pike_counter_init();

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



void pike_exit(void)
{
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

	return;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_pike_exports[] = {
	{ str_init("pike"), str_init("pike_check_req"),
		SR_KEMIP_INT, pike_check_req,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_pike_exports);
	return 0;
}
