/*
 * $Id$
 */

#ifndef _TM_BIND_H
#define _TM_BIND_H

#include "../../sr_module.h"
#include "t_hooks.h"

struct tm_binds {
	register_tmcb_f	register_tmcb;

/*
	cmd_function	t_isflagset;
	cmd_function	t_setflag;
	cmd_function	t_resetflag;
*/

	cmd_function	t_relay_to;
	cmd_function 	t_relay;
	cmd_function	t_fork_to_uri;
	cmd_function	t_fork_on_no_response;
};


typedef int(*load_tm_f)( struct tm_binds *tmb );
int load_tm( struct tm_binds *tmb);


#endif
