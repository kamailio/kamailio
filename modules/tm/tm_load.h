/*
 * $Id$
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
