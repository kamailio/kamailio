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
 *
 *
 * History:
 * --------
 * 2003-03-06  voicemail changes accepted
 *
 */


#ifndef _TM_BIND_H
#define _TM_BIND_H

#include "defs.h"


#include "../../sr_module.h"
#include "t_hooks.h"
#include "uac.h"
#include "t_fwd.h"
#include "t_reply.h"
#include "t_lookup.h"
#include "dlg.h"

/* export not usable from scripts */
#define NO_SCRIPT	-1

#define T_RELAY_TO           "t_relay_to"
#define T_RELAY_TO_UDP       "t_relay_to_udp"
#define T_RELAY_TO_TCP       "t_relay_to_tcp"
#define T_RELAY_TO_TLS       "t_relay_to_tls"
#define T_RELAY              "t_relay"
#define T_REPLY              "t_reply"
#define T_REPLY_WB           "t_reply_with_body"
#define T_ADDBLIND           "t_add_blind"
#define T_REPLY_UNSAFE       "t_reply_unsafe"
#define T_FORWARD_NONACK     "t_forward_nonack"
#define T_FORWARD_NONACK_URI "t_forward_nonack_uri"
#define T_FORWARD_NONACK_UDP "t_forward_nonack_udp"
#define T_FORWARD_NONACK_TCP "t_forward_nonack_tcp"
#define T_FORWARD_NONACK_TLS "t_forward_nonack_tls"
#define T_GET_TI             "t_get_trans_ident"
#define T_LOOKUP_IDENT       "t_lookup_ident"
#define T_IS_LOCAL           "t_is_local"
#define T_REQUEST_WITHIN     "request_within"
#define T_REQUEST_OUTSIDE    "request_outside"
#define T_GETT				 "t_gett"



struct tm_binds {
	register_tmcb_f register_tmcb;
	cmd_function    t_relay_to_udp;
	cmd_function    t_relay_to_tcp;
	cmd_function    t_relay;
	treply_f        t_reply;
	treply_wb_f     t_reply_with_body;
	tislocal_f      t_is_local;
	tget_ti_f       t_get_trans_ident;
	tlookup_ident_f t_lookup_ident;
	taddblind_f     t_addblind;
	treply_f        t_reply_unsafe;
	tfwd_f          t_forward_nonack;
	reqwith_t       t_request_within;
	reqout_t        t_request_outside;
	req_t           t_request;
	
	new_dlg_uac_f      new_dlg_uac;
	dlg_response_uac_f dlg_response_uac;
	new_dlg_uas_f      new_dlg_uas;
	dlg_request_uas_f  dlg_request_uas;
	free_dlg_f         free_dlg;
	print_dlg_f        print_dlg;

	tgett_f			t_gett;
};


typedef int(*load_tm_f)( struct tm_binds *tmb );
int load_tm( struct tm_binds *tmb);


#endif
