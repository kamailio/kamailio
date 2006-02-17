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
#include "../../parser/parse_content.h"
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
	struct hdr_field *acc;

	if ( (parse_headers(_m, HDR_EOH_T, 0) == -1) ||  /* we need all Accept headers... */
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
	
	acc = _m->accept;
	while (acc) { /* parse all accept headers */
		if (acc->type == HDR_ACCEPT_T) {
			/* DEBUG_LOG("parsing accept header: %.*s\n", FMT_STR(acc->body)); */
			if (parse_accept_body(acc) < 0) {
				LOG(L_ERR, "parse_rls_headers(): Error while parsing Accept header field\n");
				return -1;
			}
		}
		acc = acc->next;
	}

	return 0;
}

static int get_event(struct sip_msg *_m)
{
	int et = 0;
	event_t *event = NULL;
	
	if (_m->event) {
		event = (event_t*)(_m->event->parsed);
		et = event->parsed;
	} else {
		LOG(L_ERR, "no event package for RLS - using EVENT_PRESENCE\n");
		et = EVENT_PRESENCE;
	}
	return et;
}

/* returns 1 if package supported by RLS */
static int verify_event_package(struct sip_msg *m)
{
	int et = get_event(m);
	switch (et) {
		case EVENT_PRESENCE: return 0;
		default: return -1;
	}
	return -1;
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

struct accepted_types {
	char *mime_txt;
	int mime;
	int needed;
	int found;
};

/* marks mime_type as found */
void mark_accepted_type(struct accepted_types *types, int mime_type)
{
	int i;
	for (i = 0; types[i].mime_txt; i++) 
		if (mime_type == types[i].mime) types[i].found = 1;
}

static int check_message(struct sip_msg *_m, int send_err)
{
	int *accepts_mimes = NULL;
	int i;
	struct hdr_field *acc;
	struct accepted_types accepts[] = {
		{ "multipart/related", MIMETYPE(MULTIPART,RELATED), 1, 0 },
		{ "application/rlmi+xml", MIMETYPE(APPLICATION,RLMIXML), 1, 0 },
		{ "application/pidf+xml", MIMETYPE(APPLICATION, PIDFXML), 1, 0 },
		{ NULL, 0, 0, 0 } 
	};
	
	if (verify_event_package(_m) != 0) { 
		/* allow only selected packages independently on rls document */
		if (send_err) {
			ERR("unsupported events\n");
			add_response_header(_m, "Allow-Events: presence\r\n");
			send_reply(_m, 489, "Bad Event");
		}
		return -1;
	}

	/* verify Accept: multipart/related, application/rlmi+xml, application/pidf+xml */
	acc = _m->accept;
	while (acc) { /* go through all Accept headers */
		if (acc->type == HDR_ACCEPT_T) {
			/* it MUST be parsed from parse_hdr !!! */
			accepts_mimes = acc->parsed;

			/* go through all in accept mimes and test our */
			for (i = 0; accepts_mimes[i]; i++) 
				mark_accepted_type(accepts, accepts_mimes[i]);
		}
		acc = acc->next;
	}
	for (i = 0; accepts[i].mime_txt; i++) 
		if ((!accepts[i].found) && (accepts[i].needed)) {
			if (send_err) 
				ERR("required type %s not in Accept headers\n", 
					accepts[i].mime_txt);
			return -1;
		}

	/* verify Supported: eventlist */
	
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

	DEBUG_LOG("handle_new_subscription(rls)\n");
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
				DEBUG_LOG("subscription rejected\n");
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
	DEBUG_LOG("RLS subscription successfully handled\n");
	
	/* create NOTIFY message 
	 * FIXME - this may be a nonsense for polling, because the notifier might not
	 * catch up sent notification */
	rls_generate_notify(s, 1);	

	/* free subscription if only polling */
	if (sm_subscription_terminated(&s->external) == 0) {
		rls_remove(s);
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
	
	DEBUG_LOG("handle_renew_subscription(rls)\n");
	
	rls_lock();
	
	res = rls_find_subscription(from_tag, to_tag, call_id, &s);
	if ((res != RES_OK) || (!s)) {
		DEBUG_LOG("handle_renew_subscription(): can't refresh unknown subscription\n");
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
	if (sm_subscription_terminated(&s->external) == 0) {
		rls_remove(s);
	}
	
	rls_unlock();
	return 0;
}

int handle_rls_subscription(struct sip_msg* _m, /*const char *xcap_server,*/ char *send_bad_resp)
{
	int res;
	int send_err = 1;
	char *xcap_server = rls_xcap_root;

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
	if (check_message(_m, send_err) != 0) {
		DBG("check message failed\n");
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

