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
 *
 */

#ifndef _UAC_H
#define _UAC_H

#include <stdio.h>
#include "../../str.h"
#include "dlg.h"
#include "t_hooks.h"
#include "h_table.h"

#define DEFAULT_CSEQ 10 /* Default CSeq number */

/* structure for UAC interface
 *
 * You can free the memory allocated
 * for the callback parameter if necessary:
 *  - when the function is unable to create the UAC
 *    and returns failure
 *  - when TMCB_DESTROY callback is called -- you must
 *    register it explicitly!
 */
typedef struct uac_req {
	str	*method;
	str	*headers;
	str	*body;
	str *ssock;
	dlg_t	*dialog;
	int	cb_flags;
	transaction_cb	*cb;
	void	*cbp;
	str	*callid;
} uac_req_t;

/* macro for setting the values of uac_req_t struct */
#define set_uac_req(_req, \
		_m, _h, _b, _dlg, _cb_flags, _cb, _cbp) \
	do { \
		memset((_req), 0, sizeof(uac_req_t)); \
		(_req)->method = (_m); \
		(_req)->headers = (_h); \
		(_req)->body = (_b); \
		(_req)->dialog = (_dlg); \
		(_req)->cb_flags = (_cb_flags); \
		(_req)->cb = (_cb); \
		(_req)->cbp = (_cbp); \
	} while (0)


#ifdef WITH_EVENT_LOCAL_REQUEST
/* where to go for the local request route ("tm:local-request") */
extern int goto_on_local_req;
#endif /* WITH_EVEN_LOCAL_REQuEST */

/*
 * Function prototypes
 */
typedef int (*reqwith_t)(uac_req_t *uac_r);
typedef int (*reqout_t)(uac_req_t *uac_r, str* ruri, str* to, str* from, str *next_hop);
typedef int (*req_t)(uac_req_t *uac_r, str* ruri, str* to, str* from, str *next_hop);
typedef int (*t_uac_t)(uac_req_t *uac_r);
typedef int (*t_uac_with_ids_t)(uac_req_t *uac_r,
		unsigned int *ret_index, unsigned int *ret_label);
#ifdef WITH_AS_SUPPORT
typedef int (*ack_local_uac_f)(struct cell *trans, str *hdrs, str *body);
#endif
typedef int (*prepare_request_within_f)(uac_req_t *uac_r,
		struct retr_buf **dst_req);
typedef void (*send_prepared_request_f)(struct retr_buf *request_dst);
typedef void (*generate_fromtag_f)(str*, str*);

/*
 * Generate a fromtag based on given Call-ID
 */
void generate_fromtag(str* tag, str* callid);


/*
 * Initialization function
 */
int uac_init(void);


/*
 * Send a request
 */
int t_uac(uac_req_t *uac_r);

/*
 * Send a request
 * ret_index and ret_label will identify the new cell
 */
int t_uac_with_ids(uac_req_t *uac_r,
	unsigned int *ret_index, unsigned int *ret_label);
/*
 * Send a message within a dialog
 */
int req_within(uac_req_t *uac_r);


/*
 * Send an initial request that will start a dialog
 */
int req_outside(uac_req_t *uac_r, str* ruri, str* to, str* from, str* next_hop);


#ifdef WITH_AS_SUPPORT
struct retr_buf *local_ack_rb(sip_msg_t *rpl_2xx, struct cell *trans,
					unsigned int branch, str *hdrs, str *body);
void free_local_ack(struct retr_buf *lack);
void free_local_ack_unsafe(struct retr_buf *lack);

/**
 * ACK an existing local INVITE transaction...
 */
int ack_local_uac(struct cell *trans, str *hdrs, str *body);
#endif

/*
 * Send a transactional request, no dialogs involved
 */
int request(uac_req_t *uac_r, str* ruri, str* to, str* from, str *next_hop);

int prepare_req_within(uac_req_t *uac_r,
		struct retr_buf **dst_req);

void send_prepared_request(struct retr_buf *request);


#endif
