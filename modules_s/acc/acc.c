/*
 *
 * $Id$
 */

#include <stdio.h>

#include "../../dprint.h"
#include "../../msg_parser.h"
#include "../tm/t_funcs.h"
#include "acc_mod.h"

void acc_report(  struct cell* t , struct sip_msg *msg )
{

	struct sip_msg *rq;

	rq =  t->uas.request;	

	if (parse_headers(rq, HDR_CALLID)==-1 && ! rq->callid) {
		LOG(L_INFO, "ERROR: attempt to account on a request with invalid Call-ID\n");
		return;
	}
	LOG( log_level, "ACC: method=%.*s, i-uri=%.*s, o-uri=%.*s, call_id=%.*s, code=%d\n",
		rq->first_line.u.request.method.len, rq->first_line.u.request.method.s, 
		rq->first_line.u.request.uri.len, rq->first_line.u.request.uri.s,
		rq->new_uri.len, rq->new_uri.s,
		rq->callid->body.len, rq->callid->body.s,
		msg->REPLY_STATUS );
}

