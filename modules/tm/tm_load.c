/*
 * $Id$
 */

#include "tm_load.h"
#include "uac.h"

#define LOAD_ERROR "ERROR: tm_bind: TM module function "

int load_tm( struct tm_binds *tmb)
{
	if (!( tmb->register_tmcb=(register_tmcb_f) 
		find_export("register_tmcb", NO_SCRIPT)) ) {
		LOG(L_ERR, LOAD_ERROR "'register_tmcb' not found\n");
		return -1;
	}

	if (!( tmb->t_relay_to=find_export(T_RELAY_TO, 2)) ) {
		LOG(L_ERR, LOAD_ERROR "'t_relay_to' not found\n");
		return -1;
	}
	if (!( tmb->t_relay=find_export(T_RELAY, 0)) ) {
		LOG(L_ERR, LOAD_ERROR "'t_relay' not found\n");
		return -1;
	}
	if (!(tmb->t_uac=(tuac_f)find_export(T_UAC, NO_SCRIPT)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_uac' not found\n");
		return -1;
	}
	if (!(tmb->t_reply=(treply_f)find_export(T_REPLY, 2)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_reply' not found\n");
		return -1;
	}
	if (!(tmb->t_reply_unsafe=(treply_f)find_export(T_REPLY_UNSAFE, 2)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_reply_unsafe' not found\n");
		return -1;
	}
	if (!(tmb->t_forward_nonack=(tfwd_f)find_export(T_FORWARD_NONACK , 2)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_forward_nonack' not found\n");
		return -1;
	}

	return 1;

}
