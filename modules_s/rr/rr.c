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
#include "../../action.h"
#include <stdio.h>

#define RR_PREFIX "Record-Route: <sip:"
#define RR_PREFIX_LEN 19


/*
 * Define this if you want ser become a loose router
 * if not defined, ser will be just old and
 * weak strict router :-)
 */
#define LOOSE_ROUTER


/*
 * This will allow malformed Route headers too,
 * some hard phones need this
 */
#define ALLOW_MALFORMED_ROUTE


/*
 * Returns TRUE if there is a Route header
 * field in the message, FALSE otherwise
 */
int findRouteHF(struct sip_msg* _m)
{
	if (parse_headers(_m, HDR_ROUTE) == -1) {
		LOG(L_ERR, "findRouteHF(): Error while parsing headers\n");
		return FALSE;
	} else {
		if (_m->route) {
			return TRUE;
		} else {
			LOG(L_ERR, "findRouteHF(): msg->route = NULL\n");
			return FALSE;
		}
	}
}


/*
 * Gets the first URI from the first Route
 * header field in a message
 * Returns pointer to next URI in next
 */
int parseRouteHF(struct sip_msg* _m, str* _s, char** _next)
{
	char* uri, *uri_end;
	struct hdr_field* r;
#ifdef PARANOID
	if ((!_m)  || (!_s)) {
		LOG(L_ERR, "parseRouteHF(): Invalid parameter _m");
		return FALSE;
	}
#endif
	r = remove_crlf(_m->route);

#ifdef ALLOW_MALFORMED_ROUTE
	uri = eat_name(r->body.s) + 1;             /* Skip the name-part */
	if (!uri) {
		LOG(L_ERR, "parseRouteHF(): Malformed Route HF\n");
		return FALSE;
	}
	if (*(uri - 1) == '<') {
		uri_end = find_not_quoted(uri, '>');
	} else {
		uri_end = find_not_quoted(uri, ',');
		if (!uri_end) {
			uri_end = r->body.s + r->body.len;
			*_next = uri_end;
			_s->s = uri;
			_s->len = uri_end - uri;
			return TRUE;
		}
	}
#else
	uri = find_not_quoted(r->body.s, '<'); 
	if (uri) {
		uri++; /* We will skip < character */
	} else {
		LOG(L_ERR, "parseRouteHF(): Malformed Route HF (no beginning found)\n");
		return FALSE;
	}
	uri_end = find_not_quoted(uri, '>');
#endif

	if (!uri_end) {
		LOG(L_ERR, "parseRouteHF(): Malformed Route HF (no end found)\n");
		return FALSE;
	}

	_s->s = uri;
	_s->len = uri_end - uri;
	*_next = uri_end + 1;

	return TRUE;
}



/*
 * Rewrites Request URI from Route HF
 */
int rewriteReqURI(struct sip_msg* _m, str* _s)
{
       struct action act;
       char* buffer;
			
#ifdef PARANOID
	if (!_m) {
		LOG(L_ERR, "rewriteReqURI(): Invalid parameter _m\n");
		return FALSE;
	}
#endif
	buffer = (char*)pkg_malloc(_s->len + 1);
	if (!buffer) {
	        LOG(L_ERR, "rewriteReqURI(): No memory left\n");
	        return FALSE;
	}

	memcpy(buffer, _s->s, _s->len);
	buffer[_s->len] = '\0';

	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = buffer;
	act.next = NULL;

	if (do_action(&act, _m) < 0) {
		LOG(L_ERR, "rewriteReqUIR(): Error in do_action\n");
		pkg_free(buffer);
		return FALSE;
	}

	pkg_free(buffer);

	return TRUE;
}


/*
 * Removes the first URI from the first Route header
 * field, if there is only one URI in the Route header
 * field, remove the whole header field
 */
int remFirstRoute(struct sip_msg* _m, char* _next)
{
	int offset, len;
#ifdef PARANOID
	if (!_m) {
		LOG(L_ERR, "remFirstRoute(): Invalid parameter _m");
		return FALSE;
	}
#endif

#ifdef ALLOW_MALFORMED_ROUTE
	while ((*_next == ' ') || (*_next == '\t') || (*_next == ',')) _next++;
	if ((*_next == '\0') || (*_next == '\n') || (*_next == '\r')) _next = NULL;
#else
	_next = find_not_quoted(_next, '<');
#endif
	if (_next) {
		DBG("remFirstRoute(): next URI found: %s\n", _next);
		offset = _m->route->body.s - _m->buf + 1; /* + 1 - keep the first white char */
		len = _next - _m->route->body.s - 1;
	} else {
		DBG("remFirstRoute(): No next URI in Route found\n");
		offset = _m->route->name.s - _m->buf;
		len = _m->route->name.len + _m->route->body.len + 2;
		if (_m->route->body.s[_m->route->body.len] != '\0') len++;
	}

	if (del_lump(&_m->add_rm, offset, len, 0) == 0) {
		LOG(L_ERR, "remFirstRoute(): Can't remove Route HF\n");
		return FALSE;
	}
	return TRUE;
}




static void get_username(str* _s)
{
	char* at, *dcolon, *dc;
	dcolon = find_not_quoted(_s->s, ':');

	if (!dcolon) {
		_s->len = 0;
		return;
	}
	_s->s = dcolon + 1;

	at = strchr(_s->s, '@');
	dc = strchr(_s->s, ':');
	if (at) {
		if ((dc) && (dc < at)) {
			_s->len = dc - dcolon - 1;
			return;
		}
		
		_s->len = at - dcolon - 1;
		/*	_s->s[_s->len] = '\0'; */
	} else {
		_s->len = 0;
	} 
	return;
}



/*
 * Builds Record-Route line
 */
int buildRRLine(struct sip_msg* _m, str* _l)
{
	str user;
#ifdef PARANOID
	if ((!_m) || (!_l)) {
		LOG(L_ERR, "buildRRLine(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	_l->len = RR_PREFIX_LEN;
	memcpy(_l->s, RR_PREFIX, _l->len);

	if (_m->new_uri.s) {
		user.s = _m->new_uri.s;
		user.len = _m->new_uri.len;
	} else {
		user.s = _m->first_line.u.request.uri.s;
		user.len = _m->first_line.u.request.uri.len;
	}

	get_username(&user);
	if (user.len) {
		memcpy(_l->s + _l->len, user.s, user.len);
		_l->len += user.len;
		*(_l->s + _l->len++) = '@';
	}

	_l->len += sprintf(_l->s + _l->len, "%d.%d.%d.%d", 
			   (addresses[0] & 0x000000ff),
			   (addresses[0] & 0x0000ff00) >>  8,
			   (addresses[0] & 0x00ff0000) >> 16,
			   (addresses[0] & 0xff000000) >> 24
			   );

	     /*	memcpy(_l->s + _l->len, _m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len); */
	     /* _l->len += _m->first_line.u.request.uri.len; */

	if (port_no != 5060) {
		_l->len +=  sprintf(_l->s + _l->len, ":%d", port_no);
	}
	memcpy(_l->s + _l->len, ";branch=0>" CRLF,  10 + CRLF_LEN + 1);
	_l->len += 10 + CRLF_LEN;
	
	DBG("buildRRLine(): %s", _l->s);

	return TRUE;
}



/*
 * Add a new Record-Route line in SIP message
 */
int addRRLine(struct sip_msg* _m, str* _l)
{
	struct lump* anchor;
#ifdef PARANOID
	if ((!_m) || (!_l)) {
		LOG(L_ERR, "addRRLine(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	anchor = anchor_lump(&_m->add_rm, _m->headers->name.s - _m->buf, 0 , 0);
	if (anchor == NULL) {
		LOG(L_ERR, "addRRLine(): Error, can't get anchor\n");
		return FALSE;
	}

	if (insert_new_lump_before(anchor, _l->s, _l->len, 0) == 0) {
		LOG(L_ERR, "addRRLine(): Error, can't insert Record-Route\n");
		return FALSE;
	}
	return TRUE;
}
