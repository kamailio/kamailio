/*
 * $Id$
 */

#include "tm_load.h"

int load_tm( struct tm_binds *tmb)
{
	if (!( tmb->register_tmcb=(register_tmcb_f) find_export("register_tmcb", 2)) ) {
		LOG(L_ERR, "ERROR: tm_bind: TM module function 'register_tmcb' not found\n");
		return -1;
	}

	if (!( tmb->t_relay_to=find_export("t_relay_to", 2)) ) {
		LOG(L_ERR, "ERROR: tm_bind: TM module function 't_relay_to' not found\n");
		return -1;
	}
	if (!( tmb->t_relay=find_export("t_relay", 0)) ) {
		LOG(L_ERR, "ERROR: tm_bind: TM module function 't_relay' not found\n");
		return -1;
	}
	if (!( tmb->t_fork_to_uri=find_export("t_fork_to_uri", 1)) ) {
		LOG(L_ERR, "ERROR: tm_bind: TM module function 't_fork_to_uri' not found\n");
		return -1;
	}
	if (!( tmb->t_fork_on_no_response=find_export("t_fork_on_no_response", 1)) ) {
		LOG(L_ERR, "ERROR: tm_bind: TM module function 't_fork_on_no_response' not found\n");
		return -1;
	}

	return 1;

}
