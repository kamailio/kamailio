/*
 *
 * $Id$
 */

#include <stdio.h>

#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../tm/t_funcs.h"
#include "acc_mod.h"

void acc_reply_report(  struct cell* t , struct sip_msg *msg )
{

	struct sip_msg *rq;

	rq =  t->uas.request;	

	/* take call-if from TM -- it is parsed for requests and we do not
	   want to parse it from reply unnecessarily */
	if (parse_headers(rq, HDR_CALLID)==-1 && ! rq->callid) {
		LOG(L_INFO, "ERROR: attempt to account on a reply to request "
			"with an invalid Call-ID\n");
		return;
	}
	LOG( log_level, 
		"ACC: transaction answered: "
		"method=%.*s, i-uri=%.*s, o-uri=%.*s, call_id=%.*s, code=%d\n",
		rq->first_line.u.request.method.len, rq->first_line.u.request.method.s, 
		rq->first_line.u.request.uri.len, rq->first_line.u.request.uri.s,
		rq->new_uri.len, rq->new_uri.s,
		rq->callid->body.len, rq->callid->body.s,
		/* take a reply from message -- that's safe and we don't need to be
		   worried about TM reply status being changed concurrently */
		msg->REPLY_STATUS );
}

void acc_ack_report(  struct cell* t , struct sip_msg *msg )
{

	struct sip_msg *rq;

	rq =  t->uas.request;	

	if (parse_headers(rq, HDR_CALLID)==-1 && ! rq->callid) {
		LOG(L_INFO, "ERROR: attempt to account on a request with invalid Call-ID\n");
		return;
	}
	LOG( log_level, "ACC: transaction acknowledged: "
		"method=%.*s, i-uri=%.*s, o-uri=%.*s, call_id=%.*s, code=%d\n",
		/* take all stuff directly from message */

		/* CAUTION -- need to re-think here !!! The issue is with magic cookie
	       transaction matching all this stuff does not have to be parsed; so I
		   may want to try parsing here; which raises all the annoying issues
		   about in which memory space the parsed structures will live
		*/
		msg->first_line.u.request.method.len, msg->first_line.u.request.method.s, 
		msg->first_line.u.request.uri.len, msg->first_line.u.request.uri.s,
		msg->new_uri.len, msg->new_uri.s,
		msg->callid->body.len, msg->callid->body.s,
		rq->REPLY_STATUS );
}
