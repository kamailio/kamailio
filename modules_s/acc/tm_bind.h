/*
 * $Id$
 */

#ifndef _TM_BIND_H
#define _TM_BIND_H

#include "../../sr_module.h"
#include "../tm/t_hooks.h"

struct tm_binds {
	register_tmcb_f	register_tmcb;
	cmd_function	t_isflagset;
};

extern struct tm_binds tmb; 

int bind_tm();


#endif
