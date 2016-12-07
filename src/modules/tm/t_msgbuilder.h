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


#ifndef _MSGBUILDER_H
#define _MSGBUILDER_H

#include "../../ip_addr.h"
#include "defs.h"
#include "dlg.h"
#include "h_table.h"
#include "t_reply.h"


#define CSEQ "CSeq: "
#define CSEQ_LEN (sizeof(CSEQ)-1)
#define TO "To: "
#define TO_LEN (sizeof(TO)-1)
#define CALLID "Call-ID: "
#define CALLID_LEN (sizeof(CALLID)-1)
#define FROM "From: "
#define FROM_LEN (sizeof(FROM)-1)
#define FROMTAG ";tag="
#define FROMTAG_LEN (sizeof(FROMTAG)-1)
#define TOTAG ";tag="
#define TOTAG_LEN (sizeof(TOTAG)-1)
#define MAXFWD_VALUE "70"
#define MAXFWD_HEADER "Max-Forwards: " MAXFWD_VALUE CRLF
#define MAXFWD_HEADER_LEN (sizeof(MAXFWD_HEADER) - 1)


char *build_local(struct cell *Trans, unsigned int branch,
	unsigned int *len, char *method, int method_len, str *to
#ifdef CANCEL_REASON_SUPPORT
	, struct cancel_reason* reason
#endif /* CANCEL_REASON_SUPPORT */
	);

char *build_local_reparse(struct cell *Trans, unsigned int branch,
	unsigned int *len, char *method, int method_len, str *to
#ifdef CANCEL_REASON_SUPPORT
	, struct cancel_reason* reason
#endif /* CANCEL_REASON_SUPPORT */
	);

char *build_uac_request(  str msg_type, str dst, str from,
	str fromtag, int cseq, str callid, str headers, 
	str body, int branch,
	struct cell *t, unsigned int *len);

/*
 * The function creates an UAC CANCEL
 */
char *build_uac_cancel(str *headers,str *body,struct cell *cancelledT,
		unsigned int branch, unsigned int *len, struct dest_info* dst);

/*
 * The function creates an ACK to 200 OK. Route set will be created
 * and parsed and the dst parameter will contain the destination to which the
 * request should be send. The function is used by tm when it generates
 * local ACK to 200 OK (on behalf of applications using uac
 */
char *build_dlg_ack(struct sip_msg* rpl, struct cell *Trans, 
					unsigned int branch, str *hdrs, str *body,
					unsigned int *len, struct dest_info* dst);


/*
 * Create a request
 */
char* build_uac_req(str* method, str* headers, str* body, dlg_t* dialog, int branch, 
		    struct cell *t, int* len, struct dest_info* dst);


int t_calc_branch(struct cell *t,
	int b, char *branch, int *branch_len);

/* exported minimum functions for use in t_cancel */
char* print_callid_mini(char* target, str callid);
char* print_cseq_mini(char* target, str* cseq, str* method);

#endif
