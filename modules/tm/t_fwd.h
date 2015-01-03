/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
void set_branch_route(unsigned int on_branch);
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

/* prepare_new_uac flags */
#define UAC_DNS_FAILOVER_F 1 /**< new branch due to dns failover */
#define UAC_SKIP_BR_DST_F  2 /**< don't set next hop as dst_uri for
							   branch_route */
int add_uac( struct cell *t, struct sip_msg *request, str *uri, str* next_hop,
			str* path, struct proxy_l *proxy, struct socket_info* fsocket,
			snd_flags_t snd_flags, int proto, int flags, str *instance, str *ruid,
			str *location_ua);
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


