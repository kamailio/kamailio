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
#include "tree234.h"
#include "pike_funcs.h"



static int pike_init(void);
static int pike_exit(void);



/* parameters */
int time_unit = 60*10;
int max_value = 500;
int timeout   = 60*60;

/* global variables */
tree234     *ipv4_bt;
tree234     *ipv6_bt;
ser_lock_t  *pike_locks;



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
	/* init the B trees - ipv4 and ipv6 */
	ipv4_bt = newtree234(cmp_ipv4);
	ipv6_bt = newtree234(cmp_ipv6);
	/* init semaphore */
	if ((pike_locks = create_semaphores(PIKE_NR_LOCKS))==0) {
		LOG(L_ERR,"ERROR:pike_init: create sem failed!\n");
		goto error;
	}
	/* registering function timer */


	return 0;
error:
	return -1;

}




static int pike_exit(void)
{
	/* empty the timer list*/
	lock(PTL_lock);
	/* destroy the B trees - ipv4 and ipv6 */
	lock(BT4_lock);
	lock(BT6_lock);
	freetree234(ipv4_bt,free_elem);
	freetree234(ipv6_bt,free_elem);
	/* detroy semaphore */
	destroy_semaphores(pike_locks);
	return 0;
}


