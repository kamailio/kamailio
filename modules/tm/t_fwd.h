/*
 * $Id$
 *
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
/*
 * History:
 * --------
 *  2003-02-18  added proto to various function prototypes (andrei)
 */


#ifndef _T_FWD_H
#define _T_FWD_H

#include "defs.h"

#include "../../proxy.h"

typedef int (*tfwd_f)(struct sip_msg* p_msg , struct proxy_l * proxy );

int t_replicate(struct sip_msg *p_msg, struct proxy_l * proxy, int proto);
char *print_uac_request( struct cell *t, struct sip_msg *i_req,
    int branch, str *uri, unsigned int *len, struct socket_info *send_sock );
void e2e_cancel( struct sip_msg *cancel_msg, struct cell *t_cancel, struct cell *t_invite );
int e2e_cancel_branch( struct sip_msg *cancel_msg, struct cell *t_cancel, struct cell *t_invite, int branch );
int add_uac(	struct cell *t, struct sip_msg *request, str *uri,
				struct proxy_l *proxy, int proto );
int t_forward_nonack( struct cell *t, struct sip_msg* p_msg,
						struct proxy_l * p, int proto);
int t_forward_ack( struct sip_msg* p_msg );


#endif


