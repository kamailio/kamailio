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
