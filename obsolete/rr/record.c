/*
 * $Id$
 *
 * Route & Record-Route module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2003-04-04 Extracted from common.[ch] (janakj)
 */

#include <string.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../str.h"
#include "../../data_lump.h"
#include "record.h"
#include "rr_mod.h"
#include "avp_cookie.h"
#include "fix_lumps.h"


#define RR_PREFIX "Record-Route: <sip:"
#define RR_PREFIX_LEN (sizeof(RR_PREFIX)-1)

#define RR_LR_TERM ";lr>"
#define RR_LR_TERM_LEN (sizeof(RR_LR_TERM)-1)

#define RR_LR_FULL_TERM ";lr=on>"
#define RR_LR_FULL_TERM_LEN (sizeof(RR_LR_FULL_TERM)-1)

#define RR_SR_TERM ">"
#define RR_SR_TERM_LEN (sizeof(RR_SR_TERM)-1)

#define RR_FROMTAG ";ftag="
#define RR_FROMTAG_LEN (sizeof(RR_FROMTAG)-1)

#define RR_AVPCOOKIETAG ";avp="
#define RR_AVPCOOKIETAG_LEN (sizeof(RR_AVPCOOKIETAG)-1)

#define RR_R2 ";r2=on"
#define RR_R2_LEN (sizeof(RR_R2)-1)

#define INBOUND  1  /* Insert inbound Record-Route */
#define OUTBOUND 0  /* Insert outbound Record-Route */


/*
 * Extract username from the Request URI
 * First try to look at the original Request URI and if there
 * is no username use the new Request URI
 */
static inline int get_username(struct sip_msg* _m, str* _user)
{
	struct sip_uri puri;

	     /* first try to look at r-uri for a username */
	if (parse_uri(_m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len, &puri) < 0) {
		LOG(L_ERR, "get_username(): Error while parsing R-URI\n");
		return -1;
	}

	/* no username in original uri -- hmm; maybe it is a uri
	 * with just host address and username is in a preloaded route,
	 * which is now no rewritten r-uri (assumed rewriteFromRoute
	 * was called somewhere in script's beginning)
	 */
	if (!puri.user.len && _m->new_uri.s) {
		if (parse_uri(_m->new_uri.s, _m->new_uri.len, &puri) < 0) {
			LOG(L_ERR, "get_username(): Error while parsing new_uri\n");
			return -2;
	        }
	}

	_user->s = puri.user.s;
	_user->len = puri.user.len;
	return 0;
}


/*
 * build a Record-Route header field
 */
static inline int build_rr(struct lump* _l, struct lump* _l2, int _lr, str* user, str* tag, str* avp_cookie, int _inbound)
{
	char* prefix, *suffix, *crlf, *r2;
	int suffix_len, prefix_len, suffix_pos;

	prefix_len = RR_PREFIX_LEN + (user->len ? (user->len + 1) : 0);
	prefix = pkg_malloc(prefix_len);
	if (enable_full_lr) {
		suffix_len = (_lr ? RR_LR_FULL_TERM_LEN : RR_SR_TERM_LEN);
	} else {
		suffix_len = (_lr ? RR_LR_TERM_LEN : RR_SR_TERM_LEN);
	}
	suffix_len += ((tag && tag->len) ? (RR_FROMTAG_LEN + tag->len) : 0);
	suffix_len += ((avp_cookie && avp_cookie->len) ? (RR_AVPCOOKIETAG_LEN + avp_cookie->len) : 0);

	suffix = pkg_malloc(suffix_len);

	crlf = pkg_malloc(2);

	r2 = pkg_malloc(RR_R2_LEN);

	if (!prefix || !suffix || !crlf || !r2) {
		LOG(L_ERR, "build_rr(): No memory left\n");
		if (suffix) pkg_free(suffix);
		if (prefix) pkg_free(prefix);
		if (crlf) pkg_free(crlf);
		if (r2) pkg_free(r2);
		return -3;
	}

	memcpy(prefix, RR_PREFIX, RR_PREFIX_LEN);
	if (user->len) {
		memcpy(prefix + RR_PREFIX_LEN, user->s, user->len);
		prefix[RR_PREFIX_LEN + user->len] = '@';
	}

	suffix_pos = 0;
	if (tag && tag->len) {
		memcpy(suffix + suffix_pos, RR_FROMTAG, RR_FROMTAG_LEN);
		memcpy(suffix + suffix_pos + RR_FROMTAG_LEN, tag->s, tag->len);
		suffix_pos += RR_FROMTAG_LEN + tag->len;
	}

	if (avp_cookie && avp_cookie->len) {
		memcpy(suffix + suffix_pos, RR_AVPCOOKIETAG, RR_AVPCOOKIETAG_LEN);
		memcpy(suffix + suffix_pos + RR_AVPCOOKIETAG_LEN, avp_cookie->s, avp_cookie->len);
		suffix_pos += RR_AVPCOOKIETAG_LEN + avp_cookie->len;
	}

	if (enable_full_lr) {
		memcpy(suffix + suffix_pos, _lr ? RR_LR_FULL_TERM : RR_SR_TERM, _lr ? RR_LR_FULL_TERM_LEN : RR_SR_TERM_LEN);
	} else {
		memcpy(suffix + suffix_pos, _lr ? RR_LR_TERM : RR_SR_TERM, _lr ? RR_LR_TERM_LEN : RR_SR_TERM_LEN);
	}

	memcpy(crlf, CRLF, 2);
	memcpy(r2, RR_R2, RR_R2_LEN);

	if (!(_l = insert_new_lump_after(_l, prefix, prefix_len, HDR_RECORDROUTE_T))) goto lump_err;
	prefix = 0;
	if (!(_l = insert_subst_lump_after(_l, _inbound ? SUBST_RCV_ALL : SUBST_SND_ALL, HDR_RECORDROUTE_T))) goto lump_err;
	if (enable_double_rr) {
		if (!(_l = insert_cond_lump_after(_l, COND_IF_DIFF_REALMS, HDR_RECORDROUTE_T))) goto lump_err;
		if (!(_l = insert_new_lump_after(_l, r2, RR_R2_LEN, HDR_RECORDROUTE_T))) goto lump_err;
		r2 = 0;
	} else {
		pkg_free(r2);
		r2 = 0;
	}
	if (!(_l2 = insert_new_lump_before(_l2, suffix, suffix_len, HDR_RECORDROUTE_T))) goto lump_err;
	suffix = 0;
	if (!(_l2 = insert_new_lump_before(_l2, crlf, 2, HDR_RECORDROUTE_T))) goto lump_err;
	crlf = 0;
	return 0;

 lump_err:
	LOG(L_ERR, "build_rr(): Error while inserting lumps\n");
	if (prefix) pkg_free(prefix);
	if (suffix) pkg_free(suffix);
	if (r2) pkg_free(r2);
	if (crlf) pkg_free(crlf);
	return -4;
}


/*
 * Insert a new Record-Route header field
 * And also 2nd one if it is enabled and realm changed so
 * the 2nd record-route header will be necessary
 */
static inline int insert_RR(struct sip_msg* _m, int _lr)
{
	struct lump* l, *l2;
	str user;
	struct to_body* from;
	str* tag;
	str* avp_cookie;
	int res = 0;

	from = 0; /* Makes gcc happy */
	user.len = 0;

 	if (fparam_username) {
 		if (get_str_fparam(&user, _m, fparam_username) < 0) {
 			ERR("insert_RR(): Error while getting username (fparam '%s')\n", fparam_username->orig);
		}
	}

	if (append_fromtag) {
		if (parse_from_header(_m) < 0) {
			LOG(L_ERR, "insert_RR(): From parsing failed\n");
			return -2;
		}
		from = (struct to_body*)_m->from->parsed;
		tag = &from->tag_value;
	} else {
		tag = 0;
	}

	avp_cookie = rr_get_avp_cookies();
	if (enable_double_rr) {
		l = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, HDR_RECORDROUTE_T);
		l2 = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, HDR_RECORDROUTE_T);
		if (!l || !l2) {
			LOG(L_ERR, "insert_RR(): Error while creating an anchor\n");
			res = -5;
			goto exit;
		}
		l = insert_cond_lump_after(l, COND_IF_DIFF_REALMS, HDR_RECORDROUTE_T);
		l2 = insert_cond_lump_before(l2, COND_IF_DIFF_REALMS, HDR_RECORDROUTE_T);
		if (!l || !l2) {
			LOG(L_ERR, "insert_RR(): Error while inserting conditional lump\n");
			res = -6;
			goto exit;
		}
		if (build_rr(l, l2, _lr, &user, tag, avp_cookie, OUTBOUND) < 0) {
			LOG(L_ERR, "insert_RR(): Error while inserting outbound Record-Route\n");
			res = -7;
			goto exit;
		}
	}

	l = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, HDR_RECORDROUTE_T);
	l2 = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, HDR_RECORDROUTE_T);
	if (!l || !l2) {
		LOG(L_ERR, "insert_RR(): Error while creating an anchor\n");
		res = -3;
		goto exit;
	}

	if (build_rr(l, l2, _lr, &user, tag, avp_cookie, INBOUND) < 0) {
		LOG(L_ERR, "insert_RR(): Error while inserting inbound Record-Route\n");
		res = -4;
	}
exit:
        if (avp_cookie)
		pkg_free(avp_cookie);
	return res;
}


/*
 * Make sure that the record-route function is not called
 * twice and if not, execute it
 */
static inline int do_RR(struct sip_msg* _m, int _lr)
{
	static unsigned int last_rr_msg;

	if (_m->id == last_rr_msg) {
			LOG(L_ERR, "record_route(): Double attempt to record-route\n");
			return -1;
	}

	if (insert_RR(_m, _lr) < 0) {
		LOG(L_ERR, "record_route(): Error while inserting Record-Route line\n");
		return -3;
	}

	last_rr_msg=_m->id;
	return 1;
}


/*
 * Insert a new Record-Route header field with lr parameter
 */
int record_route(struct sip_msg* _m, char* _s1, char* _s2)
{
	return do_RR(_m, 1);
}


/*
 * Insert manually created Record-Route header, no checks, no restrictions,
 * always adds lr parameter, only fromtag is added automatically when requested
 */
int record_route_preset(struct sip_msg* _m, char* _data, char* _s2)
{
	str user;
	str data;
	struct to_body* from;
	struct lump* l;
	char* hdr, *p;
	int hdr_len;

	from = 0;
	user.len = 0;

	if (fparam_username) {
		if (get_str_fparam(&user, _m, fparam_username) < 0) {
			ERR("record_route_preset(): Error while getting username (fparam '%s')\n", fparam_username->orig);
			return -1;
		}
	}

	if (get_str_fparam(&data, _m, (fparam_t*)_data) <0) {
		ERR("record_route_preset(): Error while getting header value (fparam '%s')\n", ((fparam_t*)_data)->orig);
		return -1;
	}

	if (append_fromtag) {
		if (parse_from_header(_m) < 0) {
			LOG(L_ERR, "record_route_preset(): From parsing failed\n");
			return -2;
		}
		from = (struct to_body*)_m->from->parsed;
	}

	l = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, HDR_RECORDROUTE_T);
	if (!l) {
		LOG(L_ERR, "record_route_preset(): Error while creating an anchor\n");
		return -3;
	}

	hdr_len = RR_PREFIX_LEN;
	hdr_len += user.len;
	if (user.len) hdr_len += 1; /* @ */
	hdr_len += data.len;

	if (append_fromtag && from->tag_value.len) {
		hdr_len += RR_FROMTAG_LEN + from->tag_value.len;
	}

	if (enable_full_lr) {
		hdr_len += RR_LR_FULL_TERM_LEN;
	} else {
		hdr_len += RR_LR_TERM_LEN;
	}

	hdr_len += 2; /* CRLF */

	hdr = pkg_malloc(hdr_len);
	if (!hdr) {
		LOG(L_ERR, "record_route_preset(): No memory left\n");
		return -4;
	}

	p = hdr;
	memcpy(p, RR_PREFIX, RR_PREFIX_LEN);
	p += RR_PREFIX_LEN;

	if (user.len) {
		memcpy(p, user.s, user.len);
		p += user.len;
		*p = '@';
		p++;
	}

	memcpy(p, data.s, data.len);
	p += data.len;

	if (append_fromtag && from->tag_value.len) {
		memcpy(p, RR_FROMTAG, RR_FROMTAG_LEN);
		p += RR_FROMTAG_LEN;
		memcpy(p, from->tag_value.s, from->tag_value.len);
		p += from->tag_value.len;
	}

	if (enable_full_lr) {
		memcpy(p, RR_LR_FULL_TERM, RR_LR_FULL_TERM_LEN);
		p += RR_LR_FULL_TERM_LEN;
	} else {
		memcpy(p, RR_LR_TERM, RR_LR_TERM_LEN);
		p += RR_LR_TERM_LEN;
	}

	memcpy(p, CRLF, 2);

	if (!insert_new_lump_after(l, hdr, hdr_len, HDR_RECORDROUTE_T)) {
		LOG(L_ERR, "record_route_preset(): Error while inserting new lump\n");
		pkg_free(hdr);
		return -5;
	}
	return 1;
}


/*
 * Insert a new Record-Route header field without lr parameter
 */
int record_route_strict(struct sip_msg* _m, char* _s1, char* _s2)
{
	return do_RR(_m, 0);
}

/*
 * Remove Record-Route header from message lumps
 */
int remove_record_route(struct sip_msg* _m, char* _s1, char* _s2)
{
	free_rr_lump(&(_m->add_rm));
	return 1;
}
