/*
 * Route & Record-Route module
 *
 * $Id$
 */

#include "rr.h"
#include "../../dprint.h"
#include "utils.h"
#include "../../route_struct.h"
#include <string.h>
#include "../../mem/mem.h"

#define RR_PREFIX "Record-Route: <"
#define RR_PREFIX_LEN 15

/*
 * Returns TRUE if there is a Route header
 * field in the message, FALSE otherwise
 */
int findRouteHF(struct sip_msg* _m)
{
	struct hdr_field* rh;
	if (parse_headers(_m, HDR_ROUTE) == -1) {
		LOG(L_ERR, "DEBUG : findRouteHF(): Error while parsing headers\n");
		return FALSE;
	} else {
		if (_m->route) {
			rh = _m->route;
			if (rh->type != HDR_ROUTE) {
				DBG("DEBUG : findRouteHF(): No Route header fields found\n");
				return FALSE;
			}
			return TRUE;
		} else {
			LOG(L_ERR, "ERROR : findRouteHF(): msg->route = NULL\n");
			return FALSE;
		}
	}
}


/*
 * Gets the first URI from the first Route
 * header field in a message
 * Returns pointer to next URI in next
 */
int parseRouteHF(struct sip_msg* _m, char** _s, char** _next)
{
	char* uri, *uri_end;
	struct hdr_field* r;
#ifdef PARANOID
	if (!_m) {
		LOG(L_ERR, "parseRouteHF(): Invalid parameter _m");
		return FALSE;
	}
	if (!_s) {
		LOG(L_ERR, "parseRouteHF(): Invalid parameter _s");
		return FALSE;
	}
#endif
	r = remove_crlf(_m->route);

	uri = find_not_quoted(r->body.s, '<') + 1; /* We will skip < character */
	if (!uri) {
		LOG(L_ERR, "parseRouteHF(): Malformed Route HF (no begining found)\n");
		return FALSE;
	}
	uri_end = find_not_quoted(uri, '>');
	if (!uri_end) {
		LOG(L_ERR, "parseRouteHF(): Malformed Route HF (no end found)\n");
		return FALSE;
	}
	*uri_end = '\0';  /* Replace > with 0 */
	*_next = ++uri_end;
	*_s = uri;

	return TRUE;
}



/*
 * Rewrites Request URI from Route HF
 */
int rewriteReqURI(struct sip_msg* _m, char* _s)
{
	struct action act;
			
#ifdef PARANOID
	if (!_m) {
		LOG(L_ERR, "rewriteReqURI(): Invalid parameter _m\n");
		return FALSE;
	}
#endif
	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = _s;
	act.next = NULL;

	if (do_action(&act, _m) < 0) {
		LOG(L_ERR, "rewriteReqUIR(): Error in do_action\n");
		return FALSE;
	}

	return TRUE;
}


/*
 * Removes the first URI from the first Route header
 * field, if there is only one URI in the Route header
 * field, remove the whole header field
 */
int remFirstRoute(struct sip_msg* _m, char* _next)
{
	struct hdr_field* r;
	struct lump* dl;
	int offset, len;
#ifdef PARANOID
	if (!_m) {
		LOG(L_ERR, "remFirstRoute(): Invalid parameter _m");
		return FALSE;
	}
#endif
	_next = find_not_quoted(_next, '<');
	if (_next) {
		DBG("remFirstRoute(): next URI found: %s\n", _next);
		offset = _m->route->body.s - _m->buf + 1; /* + 1 - keep the first white char */
		len = _next - _m->route->body.s - 1;
	} else {
		DBG("remFirstRoute(): No next URI in Route found\n");
		offset = _m->route->name.s - _m->buf;
		len = _m->route->name.len + _m->route->body.len + 2;
	}

	if (del_lump(&_m->add_rm, offset, len, 0) == 0) {
		LOG(L_ERR, "remFirstRoute(): Can't remove Route HF\n");
		return FALSE;
	}
	return TRUE;
}




/*
 * Builds Record-Route line
 */
int buildRRLine(struct sip_msg* _m, char* _l)
{
	int len;
#ifdef PARANOID
	if (!_m) {
		LOG(L_ERR, "buildRRLine(): Invalid parameter value\n");
		return FALSE;
	}
	if (!_l) {
		LOG(L_ERR, "buildRRLine(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	len = RR_PREFIX_LEN;
	memcpy(_l, RR_PREFIX, len);
	memcpy(_l + len, _m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len);
	len += _m->first_line.u.request.uri.len;
               /* bogdan :replaced \n with CRLF*/
	memcpy(_l + len, ";branch=0>" CRLF,  10+CRLF_LEN);
	len += 10+CRLF_LEN;
	_l[len] = '\0';

	DBG("buildRRLine: %s", _l);

	return TRUE;
}



/*
 * Add a new Record-Route line in SIP message
 */
int addRRLine(struct sip_msg* _m, char* _l)
{
	struct lump* anchor;
#ifdef PARANOID
	if ((!_m) || (!_l)) {
		LOG(L_ERR, "addRRLine(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	anchor = anchor_lump(&_m->add_rm, _m->via1->hdr.s - _m->buf, 0 , 0);
	if (anchor == NULL) {
		LOG(L_ERR, "addRRLine(): Error, can't get anchor\n");
		return FALSE;
	}

	if (insert_new_lump_before(anchor, _l, strlen(_l), 0) == 0) {
		LOG(L_ERR, "addRRLine(): Error, can't insert Record-Route\n");
		return FALSE;
	}
	return TRUE;
}

