/*
 *
 * $Id$
 */

#include <stdio.h>

#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../tm/t_funcs.h"
#include "../../error.h"
#include "acc_mod.h"
#include "acc.h"


int acc_request( struct sip_msg *rq, char * comment, char  *foo)
{
	int comment_len;

	comment_len=strlen(comment);

	/* we can parse here even if we do not knwo whether the
	   request is an incmoing pkg_mem message or stoed  shmem_msg;
	   thats because acc_missed_report operating over shmem never
	   enters acc_missed if the header fields are not parsed
	   and entering the function from script gives pkg_mem,
	   which can be parsed
	*/
	   
	if ( parse_headers(rq, HDR_FROM | HDR_CALLID, 0)==-1 
				|| !(rq->from && rq->callid) ) {
		LOG(L_ERR, "ERROR: acc_missed: From not dounf\n");
		return -1;
	}
	LOG( log_level,
		"ACC: call missed: "
		"i-uri=%.*s, o-uri=%.*s, call_id=%.*s, "
		"from=%.*s, reason=%.*s\n",
        rq->first_line.u.request.uri.len,
        rq->first_line.u.request.uri.s,
		rq->new_uri.len, rq->new_uri.s,
		rq->callid->body.len, rq->callid->body.s,
		rq->from->body.len, rq->from->body.s,
        comment_len, comment);
	return 1;
}

void acc_missed_report( struct cell* t, struct sip_msg *reply,
	unsigned int code )
{
	struct sip_msg *rq;
	str acc_text;

	rq =  t->uas.request;
	/* it is coming from TM -- it must be already parsed ! */
	if (! rq->from ) {
		LOG(L_ERR, "ERROR: TM request for accounting not parsed\n");
		return;
	}

	get_reply_status(&acc_text, reply, code);
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR: acc_missed_report: get_reply_status failed\n" );
		return;
	}

	acc_request(rq, acc_text.s , 0 /* foo */);
	pkg_free(acc_text.s);

	/* zdravime vsechny cechy -- jestli jste se dostali se ctenim 
	   kodu az sem a rozumite mu, jste na tom lip, nez autor!
	   prijmete uprimne blahoprani
	*/
}

void acc_reply_report(  struct cell* t , struct sip_msg *reply, 
	unsigned int code )
{

	struct sip_msg *rq;

	/* note that we don't worry about *reply -- it may be actually
	   a pointer FAKED_REPLY and we need only code anyway, anything else is
	   stored in transaction context
	*/
	rq =  t->uas.request;	

	/* take call-if from TM -- it is parsed for requests and we do not
	   want to parse it from reply unnecessarily */
	/* don't try to parse any more -- the request is conserved in shmem */
	if ( /*parse_headers(rq, HDR_CALLID)==-1 || */ 
		!  (rq->callid && rq->from )) {
		LOG(L_INFO, "ERROR: attempt to account on a reply to request "
			"with an invalid Call-ID or From\n");
		return;
	}
	LOG( log_level, 
		"ACC: transaction answered: "
		"method=%.*s, i-uri=%.*s, o-uri=%.*s, call_id=%.*s, "
		"from=%.*s, code=%d\n",
		rq->first_line.u.request.method.len, 
		rq->first_line.u.request.method.s, 
		rq->first_line.u.request.uri.len, 
		rq->first_line.u.request.uri.s,
		rq->new_uri.len, rq->new_uri.s,
		rq->callid->body.len, rq->callid->body.s,
		rq->from->body.len, rq->from->body.s,
		/* take a reply from message -- that's safe and we don't need to be
		   worried about TM reply status being changed concurrently */
		code );
}

void acc_ack_report(  struct cell* t , struct sip_msg *ack )
{

	struct sip_msg *rq;

	rq =  t->uas.request;	

	if (/* parse_headers(rq, HDR_CALLID, 0)==-1 || */ 
			!( rq->callid && rq->from )) {
		LOG(L_INFO, "ERROR: attempt to account on a request with invalid Call-ID\n");
		return;
	}
	LOG( log_level, "ACC: transaction acknowledged: "
		"method=%.*s, i-uri=%.*s, o-uri=%.*s, call_id=%.*s,"
		"from=%.*s, code=%d\n",
		/* take all stuff directly from message */

		/* CAUTION -- need to re-think here !!! The issue is with magic cookie
	       transaction matching all this stuff does not have to be parsed; so I
		   may want to try parsing here; which raises all the annoying issues
		   about in which memory space the parsed structures will live
		*/
		ack->first_line.u.request.method.len, ack->first_line.u.request.method.s, 
		ack->first_line.u.request.uri.len, ack->first_line.u.request.uri.s,
		ack->new_uri.len, ack->new_uri.s,
		ack->callid->body.len, ack->callid->body.s,
		ack->from->body.len, ack->from->body.s,
		rq->REPLY_STATUS );
}
