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
#include "../../mem/mem.h"
#include "tree234.h"
#include "pike_funcs.h"



static int pike_init(void);
static int pike_exit(void);



/* parameters */
int time_unit = 60*10;
int max_value = 500;
int timeout   = 60*60;

/* global variables */
tree234                 *btrees[IP_TYPES];
pike_lock               *locks;
struct pike_timer_head  timers[IP_TYPES];




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
		"time_unit",
		"max_value",
		"timeout"
	},
	(modparam_t[]) {   /* Module parameter types */
		INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&time_unit,
		&max_value,
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
		goto error;
	}
		/* init the B trees - ipv4 and ipv6 */
	btrees[IPv4] = newtree234(cmp_ipv4);
	btrees[IPv6] = newtree234(cmp_ipv6);
	/* setting up timers */
	memset(&timers,0,2*sizeof(struct pike_timer_head));
	timers[IPv4].sem = locks+2;
	timers[IPv6].sem = locks+3;
	/* registering timeing functions  */
	register_timer( clean_routine , 0, 1 );
	register_timer( swap_routine , 0, time_unit );

	return 0;
error:
	return -1;

}




static int pike_exit(void)
{
	/* empty the timer list*/
	lock( timers[IPv4].sem );
	lock( timers[IPv6].sem );
	/* destroy the B trees - ipv4 and ipv6 */
	lock( &locks[IPv4] );
	lock( &locks[IPv6] );
	freetree234(btrees[IPv4],free_elem);
	freetree234(btrees[IPv6],free_elem);
	/* destroy semaphore */
	destroy_semaphores(locks);
	return 0;
}


