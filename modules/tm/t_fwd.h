/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * History:
 * --------
 *  2003-02-18  added proto to various function prototypes (andrei)
 *  2007-05-02  added unmatched_cancel & t_forward_cancel (andrei)
 */


#ifndef _T_FWD_H
#define _T_FWD_H

#include "defs.h"

#include "../../proxy.h"
#include "h_table.h"

/* cancel hop by hop */
#define E2E_CANCEL_HOP_BY_HOP

enum unmatched_cancel_t { UM_CANCEL_STATEFULL=0, UM_CANCEL_STATELESS,
							UM_CANCEL_DROP };

typedef int (*tfwd_f)(struct sip_msg* p_msg , struct proxy_l * proxy );
typedef int (*taddblind_f)( /*struct cell *t */ void);
typedef int (*treplicate_uri_f)(struct sip_msg* p_msg , str *suri );

void t_on_branch(unsigned int go_to);
unsigned int get_on_branch(void);
int t_replicate_uri(struct sip_msg *p_msg, str *suri);
int t_replicate(struct sip_msg *p_msg, struct proxy_l * proxy, int proto);
/*  -- not use outside t_fwd.c for noe
char *print_uac_request( struct cell *t, struct sip_msg *i_req,
    int branch, str *uri, unsigned int *len, struct dest_info *dst);
*/
void e2e_cancel( struct sip_msg *cancel_msg, struct cell *t_cancel, struct cell *t_invite );
int e2e_cancel_branch( struct sip_msg *cancel_msg, struct cell *t_cancel, struct cell *t_invite, int branch );
/*
int add_uac(struct cell *t, struct sip_msg *request, str *uri, str* next_hop,
				str* path, struct proxy_l *proxy, int proto );
*/
#ifdef USE_DNS_FAILOVER
int add_uac_dns_fallback( struct cell *t, struct sip_msg* msg, 
									struct ua_client* old_uac,
									int lock_replies);
#endif
int add_blind_uac(/* struct cell *t */ void);
int t_forward_nonack( struct cell *t, struct sip_msg* p_msg,
						struct proxy_l * p, int proto);
int t_forward_cancel(struct sip_msg* p_msg , struct proxy_l * proxy,
						int proto, struct cell** tran);
int t_forward_ack( struct sip_msg* p_msg );
int t_send_branch( struct cell *t, int branch, struct sip_msg* p_msg ,
					struct proxy_l * proxy, int lock_replies);
int t_relay_cancel(struct sip_msg* p_msg);

int reparse_on_dns_failover_fixup(void *handle, str *gname, str *name, void **val);

#endif


