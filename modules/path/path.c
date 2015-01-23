/*
 * $Id$
 *
 * Path handling for intermediate proxies.
 *
 * Copyright (C) 2006 Inode GmbH (Andreas Granig <andreas.granig@inode.info>)
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

/*! \file
 * \brief Path :: Utilities
 *
 * \ingroup path
 * - Module: path
 */

#include <string.h>
#include <stdio.h>

#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../parser/parse_param.h"
#include "../../lib/kcore/strcommon.h"
#include "../../dset.h"

#include "path.h"
#include "path_mod.h"

typedef enum {
	PATH_PARAM_NONE, PATH_PARAM_RECEIVED, PATH_PARAM_OB
} path_param_t;

#define PATH_PREFIX		"Path: <sip:"
#define PATH_PREFIX_LEN		(sizeof(PATH_PREFIX)-1)

const static char *proto_strings[] = {
	[PROTO_TCP] = "%3Btransport%3Dtcp",
	[PROTO_TLS] = "%3Btransport%3Dtls",
	[PROTO_SCTP] = "%3Btransport%3Dsctp",
	[PROTO_WS] = "%3Btransport%3Dws",
	[PROTO_WSS] = "%3Btransport%3Dws",
};

static int prepend_path(struct sip_msg* _m, str *user, path_param_t param, str *add_params)
{
	struct lump *l;
	char *prefix, *suffix, *cp;
	const char *proto_str;
	int prefix_len, suffix_len;
	struct hdr_field *hf;

	/* maximum possible length of suffix */
	suffix_len = strlen(";lr;received=sip::12345%3Btransport%3Dsctp;ob;>\r\n")
			+ IP_ADDR_MAX_STR_SIZE + 2 + (add_params ? add_params->len : 0) + 1;

	cp = suffix = pkg_malloc(suffix_len);
	if (!suffix) {
		LM_ERR("no pkg memory left for suffix\n");
		goto out1;
	}

	cp += sprintf(cp, ";lr");

	switch(param) {
	default:
		break;
	case PATH_PARAM_RECEIVED:
		if (_m->rcv.proto < (sizeof(proto_strings) / sizeof(*proto_strings)))
			proto_str = proto_strings[(unsigned int) _m->rcv.proto];
		else
			proto_str = NULL;

		if(_m->rcv.src_ip.af==AF_INET6) {
			cp += sprintf(cp, ";received=sip:[%s]:%hu%s", ip_addr2a(&_m->rcv.src_ip),
					_m->rcv.src_port, proto_str ? : "");
		} else {
			cp += sprintf(cp, ";received=sip:%s:%hu%s", ip_addr2a(&_m->rcv.src_ip),
					_m->rcv.src_port, proto_str ? : "");
		}
		break;
	case PATH_PARAM_OB:
		cp += sprintf(cp, ";ob");
		break;
	}

	if (add_params && add_params->len)
		cp += sprintf(cp, ";%.*s", add_params->len, add_params->s);

	cp += sprintf(cp, ">\r\n");

	prefix_len = PATH_PREFIX_LEN + (user ? user->len : 0) + 2;
	prefix = pkg_malloc(prefix_len);
	if (!prefix) {
		LM_ERR("no pkg memory left for prefix\n");
		goto out2;
	}
	if (user && user->len)
		prefix_len = sprintf(prefix, PATH_PREFIX "%.*s@", user->len, user->s);
	else
		prefix_len = sprintf(prefix, PATH_PREFIX);

	if (parse_headers(_m, HDR_PATH_F, 0) < 0) {
		LM_ERR("failed to parse message for Path header\n");
		goto out3;
	}
	hf = get_hdr(_m, HDR_PATH_T);
	if (hf)
		/* path found, add ours in front of that */
		l = anchor_lump(_m, hf->name.s - _m->buf, 0, 0);
	else
		/* no path, append to message */
		l = anchor_lump(_m, _m->unparsed - _m->buf, 0, 0);
	if (!l) {
		LM_ERR("failed to get anchor\n");
		goto out3;
	}

	l = insert_new_lump_before(l, prefix, prefix_len, 0);
	if (!l) goto out3;
	l = insert_subst_lump_before(l, SUBST_SND_ALL, 0);
	if (!l) goto out2;
	l = insert_new_lump_before(l, suffix, cp - suffix, 0);
	if (!l) goto out2;

	return 1;

out3:
	pkg_free(prefix);
out2:
	pkg_free(suffix);
out1:
	LM_ERR("failed to insert Path header\n");

	return -1;
}

/*! \brief
 * Prepend own uri to Path header
 */
int add_path(struct sip_msg* _msg, char* _a, char* _b)
{
	str user = {0,0};
	int ret;
	path_param_t param = PATH_PARAM_NONE;
	struct via_body *via;

	if (path_obb.use_outbound != NULL
		&& path_obb.use_outbound(_msg)) {
		if (path_obb.encode_flow_token(&user, _msg->rcv) != 0) {
			LM_ERR("encoding outbound flow-token\n");
			return -1;	
		}

		/* Only include ;ob parameter if this is the first-hop (that
		   means only one Via:) */
		if (parse_via_header(_msg, 2, &via) < 0)
			param = PATH_PARAM_OB;
	}

	ret = prepend_path(_msg, &user, param, NULL);

	if (user.s != NULL)
		pkg_free(user.s);

	return ret;
}

/*! \brief
 * Prepend own uri to Path header and take care of given
 * user.
 */
int add_path_usr(struct sip_msg* _msg, char* _usr, char* _parms)
{
	str user = {0,0};
	str parms = {0,0};

	if (_usr)
		get_str_fparam(&user, _msg, (fparam_t *) _usr);
	if (_parms)
		get_str_fparam(&parms, _msg, (fparam_t *) _parms);

	return prepend_path(_msg, &user, PATH_PARAM_NONE, &parms);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri.
 */
int add_path_received(struct sip_msg* _msg, char* _a, char* _b)
{
	return prepend_path(_msg, NULL, PATH_PARAM_RECEIVED, NULL);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int add_path_received_usr(struct sip_msg* _msg, char* _usr, char* _parms)
{
	str user = {0,0};
	str parms = {0,0};

	if (_usr)
		get_str_fparam(&user, _msg, (fparam_t *) _usr);
	if (_parms)
		get_str_fparam(&parms, _msg, (fparam_t *) _parms);

	return prepend_path(_msg, &user, PATH_PARAM_RECEIVED, &parms);
}

/*! \brief
 * rr callback
 */
void path_rr_callback(struct sip_msg *_m, str *r_param, void *cb_param)
{
	param_hooks_t hooks;
	param_t *params;
	static char dst_uri_buf[MAX_URI_SIZE];
	static str dst_uri;
			
	if (parse_params(r_param, CLASS_CONTACT, &hooks, &params) != 0) {
		LM_ERR("failed to parse route parameters\n");
		return;
	}

	if (hooks.contact.received) {
	        dst_uri.s = dst_uri_buf;
		dst_uri.len = MAX_URI_SIZE;
		if (unescape_user(&(hooks.contact.received->body), &dst_uri) < 0) {
		        LM_ERR("unescaping received failed\n");
			free_params(params);
			return;
		}	    
		if (set_dst_uri(_m, &dst_uri) != 0) {
			LM_ERR("failed to set dst-uri\n");
			free_params(params);
			return;
		}
		/* dst_uri changed, so it makes sense to re-use the current uri for
			forking */
		ruri_mark_new(); /* re-use uri for serial forking */
	}
	free_params(params);
}
