/*
 * $Id$
 *
 * PIKE module
 *
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
/* History:
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
#include "ip_tree.h"
#include "pike_funcs.h"



static int pike_init(void);
static int pike_exit(void);



/* parameters */
static int time_unit = 2;
static int max_reqs  = 30;
static int timeout   = 120;

/* global variables */
struct ip_node          *tree;
gen_lock_t*             timer_lock=0;
gen_lock_t*             tree_lock=0;
struct pike_timer_head  *timer;


static cmd_export_t cmds[]={
	{"pike_check_req",  pike_check_req,  0,  0, REQUEST_ROUTE},
	{0,0,0,0,0}
};

static param_export_t params[]={
	{"sampling_time_unit",    INT_PARAM,  &time_unit},
	{"reqs_density_per_unit", INT_PARAM,  &max_reqs},
	{"remove_latency",        INT_PARAM,  &timeout},
	{0,0,0}
};


struct module_exports exports= {
	"pike",
	cmds,
	params,
	
	pike_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) pike_exit,   /* module exit function */
	0,
	0  /* per-child init function */
};




static int pike_init(void)
{
	printf("pike - initializing\n");
	/* alloc the locks */
	timer_lock=lock_alloc();
	tree_lock=lock_alloc();
	if ((timer_lock==0)||(tree_lock==0)) {
		LOG(L_ERR,"ERROR:pike_init: alloc locks failed!\n");
		goto error1;
	}
	/* init the locks */
	if (lock_init(timer_lock)==0){
		LOG(L_ERR, "ERROR:pike_init: init lock failed\n");
		goto error1;
	}
	if (lock_init(tree_lock)==0){
		LOG(L_ERR, "ERROR:pike_init: init lock failed\n");
		lock_destroy(timer_lock);
		goto error1;
	}
	
	/* init the IP tree */
	tree = init_ip_tree(max_reqs);
	if (!tree) {
		LOG(L_ERR,"ERROR:pike_init: ip_tree creation failed!\n");
		goto error2;
	}
	/* setting up timers */
	timer = (struct pike_timer_head*)
		shm_malloc(sizeof(struct pike_timer_head));
	if (!timer) {
		LOG(L_ERR,"ERROR:pike_init: no free shm mem\n");
		goto error3;
	}
	memset(timer,0,sizeof(struct pike_timer_head));
	/* registering timeing functions  */
	register_timer( clean_routine , 0, 1 );
	register_timer( swap_routine , 0, time_unit );


	return 0;
error3:
	destroy_ip_tree(tree);
error2:
	lock_destroy(timer_lock);
	lock_destroy(tree_lock);
error1:
	if (timer_lock) lock_dealloc(timer_lock);
	if (tree_lock)  lock_dealloc(tree_lock);
	return -1;
}



static int pike_exit(void)
{
	/* lock the timer list */
	lock_get(timer_lock);
	/* free the timer list head */
	shm_free(timer);
	/* destroy the IP tree */
	lock_get(tree_lock);
	destroy_ip_tree(tree);
	/* destroy semaphore */
	lock_release(timer_lock);
	lock_release(tree_lock);
	lock_destroy(timer_lock);
	lock_destroy(tree_lock);
	lock_dealloc(timer_lock);
	lock_dealloc(tree_lock);
	return 0;
}


