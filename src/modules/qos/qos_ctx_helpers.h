/*
 * Copyright (C) 2007 SOMA Networks, INC.
 * Written By Ovidiu Sas
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 *
 */

#ifndef _QOS_CTX_HELPERS_H_
#define _QOS_CTX_HELPERS_H_

#include "../../parser/sdp/sdp.h"
#include "qos_cb.h"

#define QOS_CALLER  0
#define QOS_CALLEE  1

typedef struct qos_sdp_st {
	struct qos_sdp_st *prev;
	struct qos_sdp_st *next;
	unsigned int method_dir;     /* the transaction initiator: CALLER/CALLEE */
	int          method_id;      /* the method id that is carrying the sdp */
	str          method;         /* the method that is carrying the sdp */
	str          cseq;           /* the cseq of the method */
	unsigned int negotiation;    /* the negotiation type */
	sdp_session_cell_t *sdp_session[2]; /* CALLER's and CALLEE's sdp */
} qos_sdp_t;

/**
 * The QoS context.
 */
typedef struct qos_ctx_st {
	qos_sdp_t           *negotiated_sdp;
	qos_sdp_t           *pending_sdp;
	gen_lock_t lock;
	struct qos_head_cbl  cbs;
} qos_ctx_t;

/*
 
** AFTER INVITE/183 **

qos_ctx:
 +----------------+
 | *negotiated_sdp|
 +----------------+    qos_sdp (pending)
 | *pending_sdp------->+----------------+
 +----------------+    | *prev          |
                       +----------------+
                       | *next          |
                       +----------------+  
		       | method_dir     |      sdp_session (caller)
		       | method_id      |  +-->+----------+
		       | method         |  |   |          |
		       | cseq           |  |   |          |
		       | negotiation    |  |   +----------+
		       +----------------+  |
		       | sdp_session[0]----+
		       +----------------+      sdp_session (callee)
		       | sdp_session[1]------->+----------+
		       +----------------+      |          |
		                               |          |
					       +----------+

** AFTER INVITE/200ok **

qos_ctx:
 +----------------+    qos_sdp (negotiated)
 | *negotiated_sdp---->+----------------+
 +----------------+    | *prev          |
 | *pending_sdp   |    +----------------+
 +----------------+    | *next          |
                       +----------------+  
		       | method_dir     |      sdp_session (caller)
		       | method_id      |  +-->+----------+
		       | method         |  |   |          |
		       | cseq           |  |   |          |
		       | negotiation    |  |   +----------+
		       +----------------+  |
		       | sdp_session[0]----+
		       +----------------+      sdp_session (callee)
		       | sdp_session[1]------->+----------+
		       +----------------+      |          |
		                               |          |
					       +----------+


 */

qos_ctx_t* build_new_qos_ctx();
void destroy_qos_ctx(qos_ctx_t *ctx);

void add_sdp(qos_ctx_t *qos_ctx, unsigned int dir, struct sip_msg *_m, unsigned int role, unsigned int other_role);
void remove_sdp(qos_ctx_t *qos_ctx, unsigned int dir, struct sip_msg *_m, unsigned int role, unsigned int other_role);

#endif /* _QOS_CTX_HELPERS_H_ */
