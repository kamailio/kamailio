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
int time_unit = 2;
int max_reqs  = 30;
int timeout   = 120;

/* global variables */
struct ip_node          *tree;
pike_lock               *locks;
struct pike_timer_head  *timer;




struct module_exports exports= {
	"pike",
	(char*[]){
				"pike_check_req"
			},
	(cmd_function[]){
				pike_check_req
			},
	(int[]){
				0
			},
	(fixup_function[]){
				0
		},
	1,

	(char*[]) {   /* Module parameter names */
		"sampling_time_unit",
		"reqs_density_per_unit",
		"removel_latency"
	},
	(modparam_t[]) {   /* Module parameter types */
		INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&time_unit,
		&max_reqs,
		&timeout
	},
	3,      /* Number of module paramers */

	pike_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) pike_exit,   /* module exit function */
	0,
	0  /* per-child init function */
};




static int pike_init(void)
{
	printf("pike - initializing\n");
	/* init semaphore */
	if ((locks = create_semaphores(PIKE_NR_LOCKS))==0) {
		LOG(L_ERR,"ERROR:pike_init: create sem failed!\n");
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
	timer->sem = &(locks[TIMER_LOCK]);
	/* registering timeing functions  */
	register_timer( clean_routine , 0, 1 );
	register_timer( swap_routine , 0, time_unit );


	return 0;
error3:
	destroy_ip_tree(tree);
error2:
	destroy_semaphores(locks);
error1:
	return -1;

}




static int pike_exit(void)
{
	/* lock the timer list */
	lock( &locks[TIMER_LOCK] );
	/* free the tmer list head */
	shm_free(timer);
	/* destroy the IP tree */
	lock( &locks[TREE_LOCK] );
	destroy_ip_tree(tree);
	/* destroy semaphore */
	destroy_semaphores(locks);
	return 0;
}


