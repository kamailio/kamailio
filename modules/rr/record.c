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
 */

/*!
 * \file
 * \brief Route & Record-Route module, loose routing support
 * \ingroup rr
 */

/*!
 * \defgroup rr RR :: Route & Record-Route Module
 * This module contains record routing logic, as described in RFC 3261
 * (see chapter 16.12 and 12.1.1 - UAS behavior).
 */

#include <string.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../str.h"
#include "../../data_lump.h"
#include "record.h"
#include "rr_mod.h"


#define RR_PREFIX_SIP "Record-Route: <sip:"
#define RR_PREFIX_SIP_LEN (sizeof(RR_PREFIX_SIP)-1)

#define RR_PREFIX_SIPS "Record-Route: <sips:"
#define RR_PREFIX_SIPS_LEN (sizeof(RR_PREFIX_SIPS)-1)

#define RR_LR ";lr"
#define RR_LR_LEN (sizeof(RR_LR)-1)

#define RR_LR_FULL ";lr=on"
#define RR_LR_FULL_LEN (sizeof(RR_LR_FULL)-1)

#define RR_FROMTAG ";ftag="
#define RR_FROMTAG_LEN (sizeof(RR_FROMTAG)-1)

#define RR_R2 ";r2=on"
#define RR_R2_LEN (sizeof(RR_R2)-1)

#define RR_TERM ">"CRLF
#define RR_TERM_LEN (sizeof(RR_TERM)-1)

#define INBOUND  1	/*!< Insert inbound Record-Route */
#define OUTBOUND 0	/*!< Insert outbound Record-Route */

#define RR_PARAM_BUF_SIZE 512 /*!< buffer for RR parameter */


/*!
 * \brief RR param buffer 
 * \note used for storing RR param which are added before RR insertion
 */
static char rr_param_buf_ptr[RR_PARAM_BUF_SIZE];
static str rr_param_buf = {rr_param_buf_ptr,0};
static unsigned int rr_param_msg;

static pv_spec_t *custom_user_avp;		/*!< AVP for custom_user setting */


inline static int rr_is_sips(sip_msg_t *_m)
{
	if(parse_sip_msg_uri(_m)<0)
		return 0;
	if(_m->parsed_uri.type==SIPS_URI_T)
		return 1;
	return 0;
}

void init_custom_user(pv_spec_t *custom_user_avp_p)
{
    custom_user_avp = custom_user_avp_p;
}

/*!
 * \brief Return the custom_user for a record route
 * \param req SIP message
 * \param custom_user to be returned
 * \return <0 for failure
 */
inline static int get_custom_user(struct sip_msg *req, str *custom_user) {
	pv_value_t pv_val;

	if (custom_user_avp) {
		if ((pv_get_spec_value(req, custom_user_avp, &pv_val) == 0)
				&& (pv_val.flags & PV_VAL_STR) && (pv_val.rs.len > 0)) {
			custom_user->s = pv_val.rs.s;
			custom_user->len = pv_val.rs.len;
			return 0;
		}
		LM_DBG("invalid AVP value, using default user from RURI\n");
	}

	return -1;
}

/*!
 * \brief Extract username from the Request URI
 *
 * Extract username from the Request URI. First try to look at the original
 * Request URI and if there is no username use the new Request URI.
 * \param _m SIP message
 * \param _user username
 * \return 0 on success, negative on errors
 */
static inline int get_username(struct sip_msg* _m, str* _user)
{
	struct sip_uri puri;

	/* first try to look at r-uri for a username */
	if (parse_uri(_m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len, &puri) < 0) {
		LM_ERR("failed to parse R-URI\n");
		return -1;
	}

	/* no username in original uri -- hmm; maybe it is a uri
	 * with just host address and username is in a preloaded route,
	 * which is now no rewritten r-uri (assumed rewriteFromRoute
	 * was called somewhere in script's beginning) 
	 */
	if (!puri.user.len && _m->new_uri.s) {
		if (parse_uri(_m->new_uri.s, _m->new_uri.len, &puri) < 0) {
			LM_ERR("failed to parse new_uri\n");
			return -2;
		}
	}

	_user->s = puri.user.s;
	_user->len = puri.user.len;
	return 0;
}


/*!
 * \brief Insert RR parameter lump in new allocated private memory
 * \param before lump list
 * \param s parameter string
 * \param l parameter string length
 * \return pointer to new lump on success, NULL on failure
 */
static inline struct lump *insert_rr_param_lump(struct lump *before,
						char *s, int l)
{
	struct lump *rrp_l;
	char *s1;

	/* duplicate data in pkg mem */
	s1 = (char*)pkg_malloc(l);
	if (s1==0) {
		LM_ERR("no more pkg mem (%d)\n",l);
		return 0;
	}
	memcpy( s1, s, l);

	/* add lump */
	rrp_l = insert_new_lump_before( before, s1, l, HDR_RECORDROUTE_T);
	if (rrp_l==0) {
		LM_ERR("failed to add before lump\n");
		pkg_free(s1);
		return 0;
	}
	return rrp_l;
}


/*!
 * \brief Build a Record-Route header field
 *
* Build a Record-Route header field, allocates new private memory for this.
 * \param _l first lump
 * \param _l2 second lump
 * \param user user parameter
 * \param tag tag parameter
 * \param params parameter
 * \param _inbound inbound request
 * \return 0 on success, negative on failure
 */
static inline int build_rr(struct lump* _l, struct lump* _l2, str* user,
				str *tag, str *params, int _inbound, int _sips)
{
	char* prefix, *suffix, *term, *r2;
	int suffix_len, prefix_len;
	char *p;
	char *rr_prefix;
	int rr_prefix_len;

	if(_sips==0) {
		rr_prefix = RR_PREFIX_SIP;
		rr_prefix_len = RR_PREFIX_SIP_LEN;
	} else {
		rr_prefix = RR_PREFIX_SIPS;
		rr_prefix_len = RR_PREFIX_SIPS_LEN;
	}

	prefix_len = rr_prefix_len + (user->len ? (user->len + 1) : 0);
	if (enable_full_lr) {
		suffix_len = RR_LR_FULL_LEN + (params?params->len:0) +
				((tag && tag->len) ? (RR_FROMTAG_LEN + tag->len) : 0);
	} else {
		suffix_len = RR_LR_LEN + (params?params->len:0) +
				((tag && tag->len) ? (RR_FROMTAG_LEN + tag->len) : 0);
	}

	prefix = pkg_malloc(prefix_len);
	suffix = pkg_malloc(suffix_len);
	term = pkg_malloc(RR_TERM_LEN);
	r2 = pkg_malloc(RR_R2_LEN);

	if (!prefix || !suffix || !term || !r2) {
		LM_ERR("No more pkg memory\n");
		if (suffix) pkg_free(suffix);
		if (prefix) pkg_free(prefix);
		if (term) pkg_free(term);
		if (r2) pkg_free(r2);
		return -3;
	}
	
	memcpy(prefix, rr_prefix, rr_prefix_len);
	if (user->len) {
		memcpy(prefix + rr_prefix_len, user->s, user->len);
#ifdef ENABLE_USER_CHECK
		/* don't add the ignored user into a RR */
		if(i_user.len && i_user.len == user->len && 
				!strncmp(i_user.s, user->s, i_user.len))
		{
			if(prefix[rr_prefix_len]=='x')
				prefix[rr_prefix_len]='y';
			else
				prefix[rr_prefix_len]='x';
		}
#endif
		prefix[rr_prefix_len + user->len] = '@';
	}

	p = suffix;
	if (enable_full_lr) {
		memcpy( p, RR_LR_FULL, RR_LR_FULL_LEN);
		p += RR_LR_FULL_LEN;
	} else {
		memcpy( p, RR_LR, RR_LR_LEN);
		p += RR_LR_LEN;
	}
	if (tag && tag->len) {
		memcpy(p, RR_FROMTAG, RR_FROMTAG_LEN);
		p += RR_FROMTAG_LEN;
		memcpy(p, tag->s, tag->len);
		p += tag->len;
	}
	if (params && params->len) {
		memcpy(p, params->s, params->len);
		p += params->len;
	}
	
	memcpy(term, RR_TERM, RR_TERM_LEN);
	memcpy(r2, RR_R2, RR_R2_LEN);

	if (!(_l = insert_new_lump_after(_l, prefix, prefix_len, 0))) 
		goto lump_err;
	prefix = 0;
	_l = insert_subst_lump_after(_l, _inbound?SUBST_RCV_ALL:SUBST_SND_ALL, 0);
	if (_l ==0 )
		goto lump_err;
	if (enable_double_rr) {
		if (!(_l = insert_cond_lump_after(_l, COND_IF_DIFF_REALMS, 0)))
			goto lump_err;
		if (!(_l = insert_new_lump_after(_l, r2, RR_R2_LEN, 0)))
			goto lump_err;
		r2 = 0;
	} else {
		pkg_free(r2);
		r2 = 0;
	}
	_l2 = insert_new_lump_before(_l2, suffix, suffix_len, HDR_RECORDROUTE_T);
	if (_l2 == 0)
		goto lump_err;
	if (rr_param_buf.len) {
		_l2 = insert_rr_param_lump(_l2, rr_param_buf.s, rr_param_buf.len);
		if (_l2 == 0)
			goto lump_err;
	}
	suffix = 0;
	if (!(_l2 = insert_new_lump_before(_l2, term, RR_TERM_LEN, 0)))
		goto lump_err;
	term = 0;
	return 0;
	
lump_err:
	LM_ERR("failed to insert lumps\n");
	if (prefix) pkg_free(prefix);
	if (suffix) pkg_free(suffix);
	if (r2) pkg_free(r2);
	if (term) pkg_free(term);
	return -4;
}

/*!
 * \brief Copy flow-token from top-Route: to a string
 *
 * Copy the user part of the top-Route: to a string (allocating private memory
 * for this).
 * \param token where the user-part of the top-Route: will be copied to
 * \param _m the SIP message to extract the top-Route: from
 * \return 0 on success, negative on failure
 */
static int copy_flow_token(str *token, struct sip_msg *_m)
{
	rr_t *rt;
	struct sip_uri puri;

	if (_m->route
	    || (parse_headers(_m, HDR_ROUTE_F, 0) != -1 && _m->route)) {
		if (parse_rr(_m->route) < 0) {
			LM_ERR("parsing Route: header body\n");
			return -1;
		}
		rt = (rr_t *) _m->route->parsed;
		if (!rt) {
			LM_ERR("empty Route:\n");
			return -1;
		}
		if (parse_uri(rt->nameaddr.uri.s, rt->nameaddr.uri.len,
				&puri) < 0) {
			LM_ERR("parsing Route-URI\n");
			return -1;
		}

		token->s = pkg_malloc(puri.user.len * sizeof(char));
		if (token->s == NULL) {
			LM_ERR("allocating memory\n");
			return -1;
		}
		memcpy(token->s, puri.user.s, puri.user.len);
		token->len = puri.user.len;
		return 0;
	}

	LM_ERR("no Route: headers found\n");
	return -1;
}


/*!
 * \brief Insert a new Record-Route header field with lr parameter
 *
 * Insert a new Record-Route header field and also 2nd one if it is enabled
 * and the realm changed so the 2nd record-route header will be necessary.
 * \param _m SIP message
 * \param params RR parameter
 * \return 0 on success, negative on failure
 */
int record_route(struct sip_msg* _m, str *params)
{
	struct lump* l, *l2;
	str user = {NULL, 0};
	struct to_body* from = NULL;
	str* tag;
	int use_ob = rr_obb.use_outbound ? rr_obb.use_outbound(_m) : 0;
	int sips;
	int ret = 0;
	
	user.len = 0;
	
	if (add_username) {
		/* check if there is a custom user set */
		if (get_custom_user(_m, &user) < 0) {
			if (get_username(_m, &user) < 0) {
				LM_ERR("failed to extract username\n");
				return -1;
			}
		}
	} else if (use_ob == 1) {
		if (rr_obb.encode_flow_token(&user, _m->rcv) != 0) {
			LM_ERR("encoding outbound flow-token\n");
			return -1;
		}
	} else if (use_ob == 2) {
		if (copy_flow_token(&user, _m) != 0) {
			LM_ERR("copying outbound flow-token\n");
			return -1;
		}
	}

	if (append_fromtag) {
		if (parse_from_header(_m) < 0) {
			LM_ERR("From parsing failed\n");
			ret = -2;
			goto error;
		}
		from = (struct to_body*)_m->from->parsed;
		tag = &from->tag_value;
	} else {
		tag = 0;
	}

	if (rr_param_buf.len && rr_param_msg!=_m->id) {
		/* rr_params were set for a different message -> reset buffer */
		rr_param_buf.len = 0;
	}

	sips = rr_is_sips(_m);

	if (enable_double_rr) {
		l = anchor_lump(_m, _m->headers->name.s - _m->buf,0,HDR_RECORDROUTE_T);
		l2 = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, 0);
		if (!l || !l2) {
			LM_ERR("failed to create an anchor\n");
			ret = -5;
			goto error;
		}
		l = insert_cond_lump_after(l, COND_IF_DIFF_REALMS, 0);
		l2 = insert_cond_lump_before(l2, COND_IF_DIFF_REALMS, 0);
		if (!l || !l2) {
			LM_ERR("failed to insert conditional lump\n");
			ret = -6;
			goto error;
		}
		if (build_rr(l, l2, &user, tag, params, OUTBOUND, sips) < 0) {
			LM_ERR("failed to insert outbound Record-Route\n");
			ret = -7;
			goto error;
		}
	}
	
	l = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, HDR_RECORDROUTE_T);
	l2 = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, 0);
	if (!l || !l2) {
		LM_ERR("failed to create an anchor\n");
		ret = -3;
		goto error;
	}
	
	if (build_rr(l, l2, &user, tag, params, INBOUND, sips) < 0) {
		LM_ERR("failed to insert inbound Record-Route\n");
		ret = -4;
		goto error;
	}

	/* reset the rr_param buffer */
	rr_param_buf.len = 0;
	ret = 0;
error:
	if ((use_ob == 1) || (use_ob == 2))
		if (user.s != NULL)
			pkg_free(user.s);
	return ret;
}


/*!
 * \brief Insert manually created Record-Route header
 *
 * Insert manually created Record-Route header, no checks, no restrictions,
 * always adds lr parameter, only fromtag is added automatically when requested.
 * Allocates new private memory for this.
 * \param _m SIP message
 * \param _data manually created RR header
 * \return 1 on success, negative on failure
 */
int record_route_preset(struct sip_msg* _m, str* _data)
{
	str user = {NULL, 0};
	struct to_body* from;
	struct lump* l;
	char* hdr, *p;
	int hdr_len;
	int use_ob = rr_obb.use_outbound ? rr_obb.use_outbound(_m) : 0;
	char *rr_prefix;
	int rr_prefix_len;
	int sips;
	int ret = 0;

	sips = rr_is_sips(_m);
	if(sips==0) {
		rr_prefix = RR_PREFIX_SIP;
		rr_prefix_len = RR_PREFIX_SIP_LEN;
	} else {
		rr_prefix = RR_PREFIX_SIPS;
		rr_prefix_len = RR_PREFIX_SIPS_LEN;
	}

	from = 0;
	user.len = 0;
	user.s = 0;

	if (add_username) {
		if (get_username(_m, &user) < 0) {
			LM_ERR("failed to extract username\n");
			return -1;
		}
	} else if (use_ob == 1) {
		if (rr_obb.encode_flow_token(&user, _m->rcv) != 0) {
			LM_ERR("encoding outbound flow-token\n");
			return -1;
		}
	} else if (use_ob == 2) {
		if (copy_flow_token(&user, _m) != 0) {
			LM_ERR("copying outbound flow-token\n");
			return -1;
		}
	}

	if (append_fromtag) {
		if (parse_from_header(_m) < 0) {
			LM_ERR("From parsing failed\n");
			ret = -2;
			goto error;
		}
		from = (struct to_body*)_m->from->parsed;
	}
	
	l = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, HDR_RECORDROUTE_T);
	if (!l) {
		LM_ERR("failed to create lump anchor\n");
		ret = -3;
		goto error;
	}

	hdr_len = rr_prefix_len;
	if (user.len)
		hdr_len += user.len + 1; /* @ */
	hdr_len += _data->len;

	if (append_fromtag && from->tag_value.len) {
		hdr_len += RR_FROMTAG_LEN + from->tag_value.len;
	}
	
	if (enable_full_lr) {
		hdr_len += RR_LR_FULL_LEN;
	} else {
		hdr_len += RR_LR_LEN;
	}

	hdr_len += RR_TERM_LEN;

	hdr = pkg_malloc(hdr_len);
	if (!hdr) {
		LM_ERR("no pkg memory left\n");
		ret = -4;
		goto error;
	}

	p = hdr;
	memcpy(p, rr_prefix, rr_prefix_len);
	p += rr_prefix_len;

	if (user.len) {
		memcpy(p, user.s, user.len);
		p += user.len;
		*p = '@';
		p++;
	}

	memcpy(p, _data->s, _data->len);
	p += _data->len;
	
	if (append_fromtag && from->tag_value.len) {
		memcpy(p, RR_FROMTAG, RR_FROMTAG_LEN);
		p += RR_FROMTAG_LEN;
		memcpy(p, from->tag_value.s, from->tag_value.len);
		p += from->tag_value.len;
	}

	if (enable_full_lr) {
		memcpy(p, RR_LR_FULL, RR_LR_FULL_LEN);
		p += RR_LR_FULL_LEN;
	} else {
		memcpy(p, RR_LR, RR_LR_LEN);
		p += RR_LR_LEN;
	}

	memcpy(p, RR_TERM, RR_TERM_LEN);

	if (!insert_new_lump_after(l, hdr, hdr_len, 0)) {
		LM_ERR("failed to insert new lump\n");
		pkg_free(hdr);
		ret = -5;
		goto error;
	}
	ret = 1;
error:
	if ((use_ob == 1) || (use_ob == 2))
		if (user.s != NULL)
			pkg_free(user.s);
	return ret;
}

/*!
 * \brief Insert manually created Record-Route header
 *
 * Insert manually created Record-Route header, no checks, no restrictions,
 * always adds lr parameter, only fromtag is added automatically when requested.
 * Allocates new private memory for this.
 * \param _m SIP message
 * \param _data manually created RR header
 * \return 1 on success, negative on failure
 */

#define RR_TRANS_LEN 11
#define RR_TRANS ";transport="
static inline int build_advertised_rr(struct lump* _l, struct lump* _l2, str *_data,
				str* user, str *tag, int _inbound, int _sips)
{
	char *p;
	char *hdr, *trans, *r2, *suffix, *term;
	int hdr_len, suffix_len;
	char *rr_prefix;
	int rr_prefix_len;

	if(_sips==0) {
		rr_prefix = RR_PREFIX_SIP;
		rr_prefix_len = RR_PREFIX_SIP_LEN;
	} else {
		rr_prefix = RR_PREFIX_SIPS;
		rr_prefix_len = RR_PREFIX_SIPS_LEN;
	}

	hdr_len = rr_prefix_len;
	if (user && user->len)
		hdr_len += user->len + 1; /* @ */
	hdr_len += _data->len;

	suffix_len = 0;
	if (tag && tag->len) {
		suffix_len += RR_FROMTAG_LEN + tag->len;
	}
	
	if (enable_full_lr) {
		suffix_len += RR_LR_FULL_LEN;
	} else {
		suffix_len += RR_LR_LEN;
	}

	hdr = pkg_malloc(hdr_len);
	trans = pkg_malloc(RR_TRANS_LEN);
	suffix = pkg_malloc(suffix_len);
	r2 = pkg_malloc(RR_R2_LEN);
	term = pkg_malloc(RR_TERM_LEN);
	if (!hdr || !trans || !suffix || !term || !r2) {
		LM_ERR("no pkg memory left\n");
		if (hdr) pkg_free(hdr);
		if (trans) pkg_free(trans);
		if (suffix) pkg_free(suffix);
		if (r2) pkg_free(r2);
		if (term) pkg_free(term);
		return -1;
	}

	p = hdr;
	memcpy(p, rr_prefix, rr_prefix_len);
	p += rr_prefix_len;

	if (user && user->len) {
		memcpy(p, user->s, user->len);
		p += user->len;
		*p = '@';
		p++;
	}

	memcpy(p, _data->s, _data->len);
	p += _data->len;
	
	p = suffix;
	if (tag && tag->len) {
		memcpy(p, RR_FROMTAG, RR_FROMTAG_LEN);
		p += RR_FROMTAG_LEN;
		memcpy(p, tag->s, tag->len);
		p += tag->len;
	}

	if (enable_full_lr) {
		memcpy(p, RR_LR_FULL, RR_LR_FULL_LEN);
		p += RR_LR_FULL_LEN;
	} else {
		memcpy(p, RR_LR, RR_LR_LEN);
		p += RR_LR_LEN;
	}

	memcpy(trans, RR_TRANS, RR_TRANS_LEN);
	memcpy(term, RR_TERM, RR_TERM_LEN);
	memcpy(r2, RR_R2, RR_R2_LEN);

	if (!(_l = insert_new_lump_after(_l, hdr, hdr_len, 0))) {
		LM_ERR("failed to insert new lump\n");
		goto lump_err;
	}
	hdr = NULL;
	if (!(_l = insert_cond_lump_after(_l, COND_IF_DIFF_PROTO, 0)))
		goto lump_err;
	if (!(_l = insert_new_lump_after(_l, trans, RR_TRANS_LEN, 0)))
		goto lump_err;
	if (!(_l = insert_subst_lump_after(_l, _inbound?SUBST_RCV_PROTO:SUBST_SND_PROTO, 0)))
		goto lump_err;
	if (enable_double_rr) {
		if (!(_l = insert_cond_lump_after(_l, COND_IF_DIFF_REALMS, 0)))
			goto lump_err;
		if (!(_l = insert_new_lump_after(_l, r2, RR_R2_LEN, 0)))
			goto lump_err;
		r2 = 0;
	} else {
		pkg_free(r2);
		r2 = 0;
	}
	if (!(_l2 = insert_new_lump_before(_l2, suffix, suffix_len, HDR_RECORDROUTE_T)))
		goto lump_err;
	suffix = NULL;
	if (rr_param_buf.len) {
		if (!(_l2 = insert_rr_param_lump(_l2, rr_param_buf.s, rr_param_buf.len)))
			goto lump_err;
	}
	if (!(_l2 = insert_new_lump_before(_l2, term, RR_TERM_LEN, 0)))
		goto lump_err;
	return 1;
lump_err:
	if (hdr) pkg_free(hdr);
	if (trans) pkg_free(trans);
	if (suffix) pkg_free(suffix);
	if (term) pkg_free(term);
	if (r2) pkg_free(r2);
	return -7;
}

int record_route_advertised_address(struct sip_msg* _m, str* _data)
{
	str user = {NULL, 0};
	str *tag = NULL;
	struct lump* l;
	struct lump* l2;
	int use_ob = rr_obb.use_outbound ? rr_obb.use_outbound(_m) : 0;
	int sips;
	int ret = 0;
	
	user.len = 0;
	user.s = 0;

	if (add_username) {
		if (get_username(_m, &user) < 0) {
			LM_ERR("failed to extract username\n");
			return -1;
		}
	} else if (use_ob == 1) {
		if (rr_obb.encode_flow_token(&user, _m->rcv) != 0) {
			LM_ERR("encoding outbound flow-token\n");
			return -1;
		}
	} else if (use_ob == 2) {
		if (copy_flow_token(&user, _m) != 0) {
			LM_ERR("copying outbound flow-token\n");
			return -1;
		}
	}

	if (append_fromtag) {
		if (parse_from_header(_m) < 0) {
			LM_ERR("From parsing failed\n");
			ret = -2;
			goto error;
		}
		tag = &((struct to_body*)_m->from->parsed)->tag_value;
	}

	sips = rr_is_sips(_m);

	if (enable_double_rr) {
		l = anchor_lump(_m, _m->headers->name.s - _m->buf,0,HDR_RECORDROUTE_T);
		l2 = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, 0);
		if (!l || !l2) {
			LM_ERR("failed to create an anchor\n");
			ret = -3;
			goto error;
		}
		l = insert_cond_lump_after(l, COND_IF_DIFF_PROTO, 0);
		l2 = insert_cond_lump_before(l2, COND_IF_DIFF_PROTO, 0);
		if (!l || !l2) {
			LM_ERR("failed to insert conditional lump\n");
			ret = -4;
			goto error;
		}
		if (build_advertised_rr(l, l2, _data, &user, tag, OUTBOUND,
					sips) < 0) {
			LM_ERR("failed to insert outbound Record-Route\n");
			ret = -5;
			goto error;
		}
	}
	
	l = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, HDR_RECORDROUTE_T);
	l2 = anchor_lump(_m, _m->headers->name.s - _m->buf, 0, 0);
	if (!l || !l2) {
		LM_ERR("failed to create an anchor\n");
		ret = -6;
		goto error;
	}
	
	if (build_advertised_rr(l, l2, _data, &user, tag, INBOUND, sips) < 0) {
		LM_ERR("failed to insert outbound Record-Route\n");
		ret = -7;
		goto error;
	}
	ret = 1;
error:
	if ((use_ob == 1) || (use_ob == 2))
		if (user.s != NULL)
			pkg_free(user.s);
	return ret;
}


/*!
 * \brief Get the RR parameter lump
 * \param root root of the lump list
 * \return pointer to the RR parameter lump, or NULL if not found
 */
static struct lump *get_rr_param_lump( struct lump** root)
{
	struct lump *r, *crt, *last;
	/* look on the "before" branch for the last added lump */

	last = 0;
	for( crt=*root ; crt && !last ; crt=crt->next,(*root)=crt ) {
		/* check on before list */
		for( r=crt->before ; r ; r=r->before ) {
			/* we are looking for the lump that adds the 
			 * suffix of the RR header */
			if ( r->type==HDR_RECORDROUTE_T && r->op==LUMP_ADD)
				last = r;
		}
	}
	return last;
}


/*!
 * \brief Appends a new Record-Route parameter
 * \param msg SIP message
 * \param rr_param RR parameter
 * \return 0 on success, -1 on failure
 */
int add_rr_param(struct sip_msg* msg, str* rr_param)
{
	struct lump *last_param;
	struct lump *root;

	root = msg->add_rm;
	last_param = get_rr_param_lump( &root );
	if (last_param) {
		/* RR was already done -> have to add a new lump before this one */
		if (insert_rr_param_lump( last_param, rr_param->s, rr_param->len)==0) {
			LM_ERR("failed to add lump\n");
			goto error;
		}
		/* double routing enabled? */
		if (enable_double_rr) {
			if (root==0 || (last_param=get_rr_param_lump(&root))==0) {
				LM_CRIT("failed to locate double RR lump\n");
				goto error;
			}
			if (insert_rr_param_lump(last_param,rr_param->s,rr_param->len)==0){
				LM_ERR("failed to add 2nd lump\n");
				goto error;
			}
		}
	} else {
		/* RR not done yet -> store the param in the static buffer */
		if (rr_param_msg!=msg->id) {
			/* it's about a different message -> reset buffer */
			rr_param_buf.len = 0;
			rr_param_msg = msg->id;
		}
		if (rr_param_buf.len+rr_param->len>RR_PARAM_BUF_SIZE) {
			LM_ERR("maximum size of rr_param_buf exceeded\n");
			goto error;
		}
		memcpy( rr_param_buf.s+rr_param_buf.len, rr_param->s, rr_param->len);
		rr_param_buf.len += rr_param->len;
		LM_DBG("rr_param_buf=<%.*s>\n",rr_param_buf.len, rr_param_buf.s);
	}
	return 0;

error:
	return -1;
}
