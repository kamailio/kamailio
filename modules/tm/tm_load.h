/*
 * $Id$
 */

#ifndef _TM_BIND_H
#define _TM_BIND_H

#include "../../sr_module.h"
#include "t_hooks.h"
#include "uac.h"
#include "t_fwd.h"
#include "t_reply.h"

/* export not usable from scripts */
#define NO_SCRIPT	-1

#define T_RELAY_TO "t_relay_to"
#define T_RELAY "t_relay"
#define T_UAC "t_uac"
#define T_REPLY "t_reply"
#define T_REPLY_UNSAFE "t_reply_unsafe"
#define T_FORWARD_NONACK "t_forward_nonack"



struct tm_binds {
	register_tmcb_f	register_tmcb;
	cmd_function	t_relay_to;
	cmd_function 	t_relay;
	tuac_f			t_uac;
	treply_f		t_reply;
	treply_f		t_reply_unsafe;
	tfwd_f			t_forward_nonack;
};


typedef int(*load_tm_f)( struct tm_binds *tmb );
int load_tm( struct tm_binds *tmb);


#endif
