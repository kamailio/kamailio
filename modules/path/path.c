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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#define PATH_LR_PARAM		";lr"
#define PATH_LR_PARAM_LEN	(sizeof(PATH_LR_PARAM)-1)

#define PATH_RC_PARAM		";received="
#define PATH_RC_PARAM_LEN	(sizeof(PATH_RC_PARAM)-1)

#define PATH_OB_PARAM		";ob"
#define PATH_OB_PARAM_LEN	(sizeof(PATH_OB_PARAM)-1)

#define	PATH_CRLF		">\r\n"
#define PATH_CRLF_LEN		(sizeof(PATH_CRLF)-1)

#define ALLOC_AND_COPY_PATH_HDR() \
	if ((suffix = pkg_malloc(suffix_len)) == NULL) { \
		LM_ERR("no pkg memory left for suffix\n"); \
		goto out1; \
	} \
	memcpy(suffix, PATH_LR_PARAM, PATH_LR_PARAM_LEN);

static int prepend_path(struct sip_msg* _m, str *user, path_param_t param)
{
	struct lump *l;
	char *prefix, *suffix, *crlf;
	int prefix_len, suffix_len;
	struct hdr_field *hf;
	str rcv_addr = {0, 0};
	char *src_ip;
		
	prefix = suffix = crlf = 0;

	prefix_len = PATH_PREFIX_LEN + (user->len ? (user->len+1) : 0);
	prefix = pkg_malloc(prefix_len);
	if (!prefix) {
		LM_ERR("no pkg memory left for prefix\n");
		goto out1;
	}
	memcpy(prefix, PATH_PREFIX, PATH_PREFIX_LEN);
	if (user->len) {
		memcpy(prefix + PATH_PREFIX_LEN, user->s, user->len);
		memcpy(prefix + prefix_len - 1, "@", 1);
	}

	switch(param) {
	default:
		suffix_len = PATH_LR_PARAM_LEN;
		ALLOC_AND_COPY_PATH_HDR();
		break;
	case PATH_PARAM_RECEIVED:
		suffix_len = PATH_LR_PARAM_LEN + PATH_RC_PARAM_LEN;
		ALLOC_AND_COPY_PATH_HDR();
		memcpy(suffix + PATH_LR_PARAM_LEN, PATH_RC_PARAM,	
			PATH_RC_PARAM_LEN);
		break;
	case PATH_PARAM_OB:
		suffix_len = PATH_LR_PARAM_LEN + PATH_OB_PARAM_LEN;
		ALLOC_AND_COPY_PATH_HDR();
		memcpy(suffix + PATH_LR_PARAM_LEN, PATH_OB_PARAM,
			PATH_OB_PARAM_LEN);
		break;
	}

	crlf = pkg_malloc(PATH_CRLF_LEN);
	if (!crlf) {
		LM_ERR("no pkg memory left for crlf\n");
		goto out1;
	}
	memcpy(crlf, PATH_CRLF, PATH_CRLF_LEN);

	if (parse_headers(_m, HDR_PATH_F, 0) < 0) {
		LM_ERR("failed to parse message for Path header\n");
		goto out1;
	}
	for (hf = _m->headers; hf; hf = hf->next) {
		if (hf->type == HDR_PATH_T) {
			break;
		} 
	}
	if (hf)
		/* path found, add ours in front of that */
		l = anchor_lump(_m, hf->name.s - _m->buf, 0, 0);
	else
		/* no path, append to message */
		l = anchor_lump(_m, _m->unparsed - _m->buf, 0, 0);
	if (!l) {
		LM_ERR("failed to get anchor\n");
		goto out1;
	}

	l = insert_new_lump_before(l, prefix, prefix_len, 0);
	if (!l) goto out1;
	l = insert_subst_lump_before(l, SUBST_SND_ALL, 0);
	if (!l) goto out2;
	l = insert_new_lump_before(l, suffix, suffix_len, 0);
	if (!l) goto out2;
	if (param == PATH_PARAM_RECEIVED) {
		/* TODO: agranig: optimize this one! */
		src_ip = ip_addr2a(&_m->rcv.src_ip);
		rcv_addr.s = pkg_malloc(6 + IP_ADDR_MAX_STR_SIZE + 24); /* sip:<ip>:<port>%3Btransport%3Dsctp\0 */
		if(!rcv_addr.s) {
			LM_ERR("no pkg memory left for receive-address\n");
			goto out3;
		}
		switch (_m->rcv.proto) {
			case PROTO_UDP:
				rcv_addr.len = snprintf(rcv_addr.s, 6 + IP_ADDR_MAX_STR_SIZE + 4, "sip:%s:%u", src_ip, _m->rcv.src_port);
				break;
			case PROTO_TCP:
				rcv_addr.len = snprintf(rcv_addr.s, 6 + IP_ADDR_MAX_STR_SIZE + 22, "sip:%s:%u%%3Btransport%%3Dtcp", src_ip, _m->rcv.src_port);
				break;
			case PROTO_TLS:
				rcv_addr.len = snprintf(rcv_addr.s, 6 + IP_ADDR_MAX_STR_SIZE + 22, "sip:%s:%u%%3Btransport%%3Dtls", src_ip, _m->rcv.src_port);
				break;
			case PROTO_SCTP:
				rcv_addr.len = snprintf(rcv_addr.s, 6 + IP_ADDR_MAX_STR_SIZE + 23, "sip:%s:%u%%3Btransport%%3Dsctp", src_ip, _m->rcv.src_port);
				break;
			case PROTO_WS:
			case PROTO_WSS:
				rcv_addr.len = snprintf(rcv_addr.s, 6 + IP_ADDR_MAX_STR_SIZE + 21, "sip:%s:%u%%3Btransport%%3Dws", src_ip, _m->rcv.src_port);
				break;
	    }

		l = insert_new_lump_before(l, rcv_addr.s, rcv_addr.len, 0);
		if (!l) goto out3;
	}
	l = insert_new_lump_before(l, crlf, CRLF_LEN+1, 0);
	if (!l) goto out4;
	
	return 1;
	
out1:
	if (prefix) pkg_free(prefix);
out2:
	if (suffix) pkg_free(suffix);
out3:
	if (rcv_addr.s) pkg_free(rcv_addr.s);
out4:
	if (crlf) pkg_free(crlf);

	LM_ERR("failed to insert prefix lump\n");

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

	ret = prepend_path(_msg, &user, param);

	if (user.s != NULL)
		pkg_free(user.s);

	return ret;
}

/*! \brief
 * Prepend own uri to Path header and take care of given
 * user.
 */
int add_path_usr(struct sip_msg* _msg, char* _usr, char* _b)
{
	return prepend_path(_msg, (str*)_usr, PATH_PARAM_NONE);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri.
 */
int add_path_received(struct sip_msg* _msg, char* _a, char* _b)
{
	str user = {0,0};
	return prepend_path(_msg, &user, PATH_PARAM_RECEIVED);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int add_path_received_usr(struct sip_msg* _msg, char* _usr, char* _b)
{
	return prepend_path(_msg, (str*)_usr, PATH_PARAM_RECEIVED);
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
