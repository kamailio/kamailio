#include "rls_mod.h"
#include "rls_handler.h"
#include "rl_subscription.h"
#include <cds/dstring.h>
#include <cds/logger.h>
#include "result_codes.h"

#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_event.h"
#include "../../parser/parse_expires.h"
#include "../../data_lump_rpl.h"


static int send_reply(struct sip_msg* _m, int code, char *msg) 
{
	if (tmb.t_reply(_m, code, msg) == -1) {
		LOG(L_ERR, "send_reply(): Error while sending %d %s\n", code, msg);
		return -1;
	} 
	else return 0;	
}

static int parse_rls_headers(struct sip_msg* _m)
{
	if ( (parse_headers(_m, HDR_FROM_T | HDR_TO_T | HDR_EXPIRES_T | 
					HDR_CALLID_T | HDR_EVENT_T | HDR_ACCEPT_T, 0) == -1) || 
			(_m->from==0)||(_m->to==0)||(_m->event==0) ) {
		LOG(L_ERR, "parse_rls_headers(): Error while parsing headers\n");
		return -1;
	}
	
/*	there is no parse_to_header function (only parse_to)
	if (parse_to_header(_m) < 0) {
		LOG(L_ERR, "parse_rls_headers(): To malformed or missing\n");
		return -1;
	}*/

	if (parse_from_header(_m) < 0) {
		LOG(L_ERR, "parse_rls_headers(): From malformed or missing\n");
		return -1;
	}

	if (_m->expires) {
		if (parse_expires(_m->expires) < 0) {
			LOG(L_ERR, "parse_rls_headers(): Error parsing Expires header\n");
			return -1;
		}
	}

	if (_m->event) {
		if (parse_event(_m->event) < 0) {
			LOG(L_ERR, "parse_rls_headers(): Error while parsing Event header field\n");
			return -1;
		}
		
	}
	
	/*if (_m->accept) {
		if (parse_accept(_m->accept, &acc) < 0) {
			LOG(L_ERR, "parse_rls_headers(): Error while parsing Accept header field\n");
			return -1;
		}
	}*/

	return 0;
}

static int add_response_header(struct sip_msg *_m, char *hdr)
{
	if (!add_lump_rpl(_m, hdr, strlen(hdr), LUMP_RPL_HDR)) return -1;
	return 0;
}

static int add_response_min_expires_header(struct sip_msg *_m)
{
	char tmp[64];
	sprintf(tmp, "Min-Expires: %d\r\n", rls_min_expiration);
	if (!add_lump_rpl(_m, tmp, strlen(tmp), LUMP_RPL_HDR)) return -1;
	return 0;
}

/**
 * Verifies presence of the To-tag in message. Returns 1 if
 * the tag is present, 0 if not, -1 on error.
 */
static int has_to_tag(struct sip_msg *_m)
{
	struct to_body *to = (struct to_body*)_m->to->parsed;
	if (!to) return 0;
	if (to->tag_value.len > 0) return 1;
	return 0;
}

static int handle_new_subscription(struct sip_msg *m, const char *xcap_server, int send_error_responses)
{
	rl_subscription_t *s;
	int res = 0;
	
	rls_lock();

	TRACE_LOG("handle_new_subscription(rls)\n");
	/* create a new subscription structure */
	res = rls_create_subscription(m, &s, xcap_server);
	if (res != RES_OK) {
		rls_unlock();
			
		switch (res) {
			case RES_PARSE_HEADERS_ERR:
				if (!send_error_responses) return -1; /* "unprocessed" */
				add_response_header(m, "Reason-Phrase: Bad or missing headers\r\n");
				send_reply(m, 400, "Bad Request");
				break;
			case RES_SUBSCRIPTION_REJECTED:
				/* if (!send_error_responses) return -1; */
				/* FIXME: authorization is done before XCAP query, so though it is NOT 
				 * resource-list subscription it may be marked as rejected !!! */
				TRACE_LOG("subscription rejected\n");
				add_response_header(m, "Reason-Phrase: Subscription rejected\r\n");
				send_reply(m, 403, "Forbidden");
				break;
			case RES_EXPIRATION_INTERVAL_TOO_SHORT:
				if (!send_error_responses) return -1; /* "unprocessed" */
				add_response_min_expires_header(m);
				send_reply(m, 423, "Interval too small");
				break;
			case RES_BAD_EVENT_PACKAGE_ERR:
				if (!send_error_responses) return -1; /* "unprocessed" */
				/* TODO: add_response_header(_m, "Allow-Events: \r\n"); */
				send_reply(m, 489, "Bad Event");
				break;
			case RES_BAD_GATEWAY_ERR:
				if (!send_error_responses) return -1; /* "unprocessed" */
				send_reply(m, 502, "Bad Gateway");
				break;
			case RES_XCAP_QUERY_ERR:
				if (!send_error_responses) return -1; /* "unprocessed" */
				add_response_header(m, "Reason-Phrase: XCAP query error\r\n");
				send_reply(m, 502, "Bad Gateway");
				/*send_reply(m, 500, "Internal error"); */
				break;
			case RES_XCAP_PARSE_ERR:
				if (!send_error_responses) return -1; /* "unprocessed" */
				add_response_header(m, "Reason-Phrase: XCAP result parsing error\r\n");
				send_reply(m, 500, "Internal error");
				break;
					
			default: 
				if (!send_error_responses) return -1; /* "unprocessed" */
				send_reply(m, 500, "Internal error");
		}
		return 0;	/* processed */
	}

	/* send a response */
	rls_prepare_subscription_response(s, m);
	send_reply(m, 200, "OK");
	
	/* create NOTIFY message 
	 * FIXME - this may be a nonsense for polling, because the notifier might not
	 * catch up sending notification */
	rls_generate_notify(s, 1);	

	/* free subscription if only polling */
	if (sm_subscription_terminated(&s->subscription) == 0) {
		rls_free(s);
	}
	
	rls_unlock();

	return 0;
}

static int handle_renew_subscription(struct sip_msg *m, int send_error_responses)
{
	str *from_tag;
	str *to_tag;
	str *call_id;
	rl_subscription_t *s = NULL;
	int res;

	to_tag = &((struct to_body*)m->to->parsed)->tag_value;
	from_tag = &((struct to_body*)m->from->parsed)->tag_value;
	call_id = NULL;
	if (m->callid) call_id = &m->callid->body;
	
	TRACE_LOG("handle_renew_subscription(rls)\n");
	
	rls_lock();
	
	res = rls_find_subscription(from_tag, to_tag, call_id, &s);
	if ((res != RES_OK) || (!s)) {
		rls_unlock();
		if (send_error_responses)
			send_reply(m, 481, "Call/Transaction Does Not Exist");
		return -1; /* "unprocessed" */
	}
		
	res = rls_refresh_subscription(m, s);
	if (res != RES_OK) {
		rls_unlock();
		switch (res) {
			case RES_PARSE_HEADERS_ERR:
				if (!send_error_responses) return -1; /* "unprocessed" */
				add_response_header(m, "Reason-Phrase: Bad or missing headers\r\n");
				send_reply(m, 400, "Bad Request");
				break;
			case RES_EXPIRATION_INTERVAL_TOO_SHORT:
				if (!send_error_responses) return -1; /* "unprocessed" */
				add_response_min_expires_header(m);
				send_reply(m, 423, "Interval too small");
				break;
			case RES_SUBSCRIPTION_TERMINATED:
				send_reply(m, 481, "Subscription terminated");
				break;

			default: 
				if (!send_error_responses) return -1; /* "unprocessed" */
				send_reply(m, 500, "Internal error");
		}
		return 0; /* processed */
	}
	
	/* send a response */
	rls_prepare_subscription_response(s, m);
	send_reply(m, 200, "OK");
	
	/* create NOTIFY message */
	rls_generate_notify(s, 1);	

	/* free subscription if only polling */
	if (sm_subscription_terminated(&s->subscription) == 0) {
		rls_free(s);
	}
	
	rls_unlock();
	return 0;
}

int handle_rls_subscription(struct sip_msg* _m, const char *xcap_server, char *send_bad_resp)
{
	int res;
	int send_err = 1;

	if (!xcap_server) {
		LOG(L_ERR, "handle_rls_subscription(): No XCAP server given!\n");
		return -1;
	}
	
	send_err = (int)send_bad_resp;
	DEBUG_LOG("handle_rls_subscription(): XCAP server = \'%s\' send errors=%d\n", 
			xcap_server, send_err);
	
	res = parse_rls_headers(_m);
	if (res == -1) {
		LOG(L_INFO, "handle_rls_subscription(): problems parsing headers.\n");
		if (send_err) {
			add_response_header(_m, "Reason-Phrase: Bad or missing headers\r\n");
			send_reply(_m, 400, "Bad Request");
		}
		return -1;
	}
	if (has_to_tag(_m)) {
		/* handle SUBSCRIBE for an existing subscription */
		res = handle_renew_subscription(_m, send_err);
	}
	else {
		/* handle SUBSCRIBE for a new subscription */
		res = handle_new_subscription(_m, xcap_server, send_err);
	}

	if (res == 0) return 1;
	else return -1;
}

