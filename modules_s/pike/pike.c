/*
 * $Id$
 *
 * PIKE module
 *
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
	lock( timer->sem );
	/* free the tmer list head */
	shm_free(timer);
	/* destroy the IP tree */
	lock( &locks[TREE_LOCK] );
	destroy_ip_tree(tree);
	/* destroy semaphore */
	destroy_semaphores(locks);
	return 0;
}


