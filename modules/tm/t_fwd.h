/*
 * $Id$
 *
 */

#ifndef _T_FWD_H
#define _T_FWD_H

#include "../../proxy.h"

typedef int (*tfwd_f)(struct sip_msg* p_msg , struct proxy_l * proxy );

int t_replicate(struct sip_msg *p_msg, struct proxy_l * proxy );
char *print_uac_request( struct cell *t, struct sip_msg *i_req,
    int branch, str *uri, int *len, struct socket_info *send_sock );
void e2e_cancel( struct sip_msg *cancel_msg, struct cell *t_cancel, struct cell *t_invite );
int e2e_cancel_branch( struct sip_msg *cancel_msg, struct cell *t_cancel, struct cell *t_invite, int branch );
int add_uac( struct cell *t, struct sip_msg *request, str *uri, struct proxy_l *proxy );
int t_forward_nonack( struct cell *t, struct sip_msg* p_msg, struct proxy_l * p );
int t_forward_ack( struct sip_msg* p_msg );


#endif


