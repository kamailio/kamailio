/*
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

#include "../../core/mem/mem.h"
#include "../../core/data_lump.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/strutils.h"
#include "../../core/dset.h"

#include "path.h"
#include "path_mod.h"

typedef enum {
	PATH_PARAM_NONE = 0, PATH_PARAM_RECEIVED = 1, PATH_PARAM_OB = 2
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

static char *path_strzdup(char *src, int len)
{
	char *res;

	if (!src)
		return NULL;
	if(len<0) {
		len = strlen(src);
	}
	if (!(res = (char *) pkg_malloc(len + 1)))
		return NULL;
	strncpy(res, src, len);
	res[len] = 0;

	return res;
}

static int handleOutbound(sip_msg_t* _m, str *user, path_param_t *param)
{
	if (path_obb.use_outbound != NULL && path_obb.use_outbound(_m)) {
		struct via_body *via;

		if (path_obb.encode_flow_token(user, _m->rcv) != 0) {
			LM_ERR("encoding outbound flow-token\n");
			return -1;
		}

		/* Only include ;ob parameter if this is the first-hop
		 * (that means only one Via:) */
		if (parse_via_header(_m, 2, &via) < 0) {
			*param |= PATH_PARAM_OB;
		}
	}

	return 1;
}

static int prepend_path(sip_msg_t* _m, str *user, path_param_t param,
		str *add_params)
{
	struct lump *l;
	char *prefix, *suffix, *cp, *dp;
	const char *proto_str;
	int prefix_len, suffix_len;
	struct hdr_field *hf;

	/* maximum possible length of suffix */
	suffix_len = sizeof(";lr;r2=on;received=sip::12345%3Btransport%3Dsctp;ob;>\r\n")
			+ IP_ADDR_MAX_STR_SIZE + 2 + (add_params ? add_params->len : 0) + 1;

	cp = suffix = pkg_malloc(suffix_len);
	if (!suffix) {
		LM_ERR("no pkg memory left for suffix\n");
		goto out1;
	}

	cp += sprintf(cp, ";lr");

	if (param & PATH_PARAM_RECEIVED) {
		if(path_received_format==0) {
			if (_m->rcv.proto
						< (sizeof(proto_strings) / sizeof(*proto_strings))) {
				proto_str = proto_strings[(unsigned int) _m->rcv.proto];
			} else {
				proto_str = NULL;
			}
			if(_m->rcv.src_ip.af==AF_INET6) {
				cp += sprintf(cp, ";received=sip:[%s]:%hu%s",
						ip_addr2a(&_m->rcv.src_ip),
						_m->rcv.src_port, proto_str ? : "");
			} else {
				cp += sprintf(cp, ";received=sip:%s:%hu%s"
						, ip_addr2a(&_m->rcv.src_ip),
						_m->rcv.src_port, proto_str ? : "");
			}
		} else {
			if(_m->rcv.src_ip.af==AF_INET6) {
				cp += sprintf(cp, ";received=[%s]~%hu~%d",
						ip_addr2a(&_m->rcv.src_ip),
						_m->rcv.src_port, (int)_m->rcv.proto);
			} else {
				cp += sprintf(cp, ";received=%s~%hu~%d",
						ip_addr2a(&_m->rcv.src_ip),
						_m->rcv.src_port, (int)_m->rcv.proto);
			}
		}
	}

	if (param & PATH_PARAM_OB) {
		cp += sprintf(cp, ";ob");
	}

	if (add_params && add_params->len) {
		cp += sprintf(cp, ";%.*s", add_params->len, add_params->s);
	}

	if(path_enable_r2==0) {
		cp += sprintf(cp, ">\r\n");
	} else {
		cp += sprintf(cp, ";r2=on>\r\n");
	}

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

	if(path_enable_r2!=0) {
		dp = path_strzdup(prefix, prefix_len);
		if(dp==NULL) goto out1;
		l = insert_new_lump_before(l, dp, prefix_len, 0);
		if (!l) goto out1;
		l = insert_subst_lump_before(l, SUBST_RCV_ALL, 0);
		if (!l) goto out1;
		dp = path_strzdup(suffix, cp - suffix);
		if(dp==NULL) goto out1;
		l = insert_new_lump_before(l, dp, cp - suffix, 0);
		if (!l) goto out1;
	}

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
int ki_add_path(struct sip_msg* _msg)
{
	str user = {0,0};
	int ret;
	path_param_t param = PATH_PARAM_NONE;

	ret = handleOutbound(_msg, &user, &param);

	if (ret > 0) {
		ret = prepend_path(_msg, &user, param, NULL);
	}

	if (user.s != NULL) {
		pkg_free(user.s);
	}

	return ret;
}

int add_path(sip_msg_t* _msg, char* _a, char* _b)
{
	return ki_add_path(_msg);
}

/*! \brief
 * Prepend own uri to Path header and take care of given
 * user.
 */
int add_path_usr(struct sip_msg* _msg, char* _usr, char* _parms)
{
	str user = {0,0};
	str parms = {0,0};

	if (_usr) {
		if(get_str_fparam(&user, _msg, (fparam_t *) _usr)<0) {
			LM_ERR("failed to get user value\n");
			return -1;
		}
	}
	if (_parms) {
		if(get_str_fparam(&parms, _msg, (fparam_t *) _parms)<0) {
			LM_ERR("failed to get params value\n");
			return -1;
		}
	}

	return prepend_path(_msg, &user, PATH_PARAM_NONE, &parms);
}

/*! \brief
 * Prepend own uri to Path header and take care of given
 * user.
 */
int ki_add_path_user(sip_msg_t* _msg, str* _user)
{
	str parms = {0,0};
	return prepend_path(_msg, _user, PATH_PARAM_NONE, &parms);
}

/*! \brief
 * Prepend own uri to Path header and take care of given
 * user.
 */
int ki_add_path_user_params(sip_msg_t* _msg, str* _user, str* _params)
{
	return prepend_path(_msg, _user, PATH_PARAM_NONE, _params);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri.
 */
int ki_add_path_received(sip_msg_t* _msg)
{
	str user = {0,0};
	int ret;
	path_param_t param = PATH_PARAM_RECEIVED;

	ret = handleOutbound(_msg, &user, &param);

	if (ret > 0) {
		ret = prepend_path(_msg, &user, param, NULL);
	}

	if (user.s != NULL) {
		pkg_free(user.s);
	}

	return ret;
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri.
 */
int add_path_received(struct sip_msg* _msg, char* _a, char* _b)
{
	return ki_add_path_received(_msg);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int add_path_received_usr(struct sip_msg* _msg, char* _usr, char* _parms)
{
	str user = {0,0};
	str parms = {0,0};

	if (_usr) {
		if(get_str_fparam(&user, _msg, (fparam_t *) _usr)<0) {
			LM_ERR("failed to get user value\n");
			return -1;
		}
	}
	if (_parms) {
		if(get_str_fparam(&parms, _msg, (fparam_t *) _parms)<0) {
			LM_ERR("failed to get params value\n");
			return -1;
		}
	}

	return prepend_path(_msg, &user, PATH_PARAM_RECEIVED, &parms);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int ki_add_path_received_user(sip_msg_t* _msg, str* _user)
{
	str parms = {0,0};
	return prepend_path(_msg, _user, PATH_PARAM_RECEIVED, &parms);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int ki_add_path_received_user_params(sip_msg_t* _msg, str* _user, str* _params)
{
	return prepend_path(_msg, _user, PATH_PARAM_RECEIVED, _params);
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
	char *p;
	int n;
	int nproto;
	str sproto;

	if (parse_params(r_param, CLASS_CONTACT, &hooks, &params) != 0) {
		LM_ERR("failed to parse route parameters\n");
		return;
	}

	if (hooks.contact.received==NULL
			|| hooks.contact.received->body.len<=0) {
		LM_DBG("no received parameter in route header\n");
		free_params(params);
		return;
	}

	/* 24 => sip:...;transport=sctp */
	if(hooks.contact.received->body.len + 24 >= MAX_URI_SIZE) {
		LM_ERR("received uri is too long\n");
		goto done;
	}
	dst_uri.s = dst_uri_buf;
	dst_uri.len = MAX_URI_SIZE;
	if(path_received_format==0) {
		/* received=sip:...;transport... */
		if (unescape_user(&(hooks.contact.received->body), &dst_uri) < 0) {
			LM_ERR("unescaping received failed\n");
			free_params(params);
			return;
		}
	} else {
		/* received=ip~port~proto */
		strncpy(dst_uri_buf, "sip:", 4);
		strncpy(dst_uri_buf+4, hooks.contact.received->body.s,
					hooks.contact.received->body.len);
		dst_uri_buf[4+hooks.contact.received->body.len] = '\0';
		p = dst_uri_buf + 4;
		n = 0;
		while(*p!='\0') {
			if(*p=='~') {
				n++;
				if(n==1) {
					/* port */
					*p = ':';
				} else if(n==2) {
					/* proto */
					*p = ';';
					p++;
					if(*p=='\0') {
						LM_ERR("invalid received format\n");
						goto done;
					}
					nproto = *p - '0';
					if (nproto != PROTO_UDP) {
						proto_type_to_str(nproto, &sproto);
						if (sproto.len == 0) {
							LM_ERR("unknown proto in received param\n");
							goto done;
						}
						strncpy(p, "transport=", 10);
						p += 10;
						memcpy(p, sproto.s, sproto.len);
						p += sproto.len;
						*p = '\0';
						dst_uri.len = p - dst_uri_buf;
						break;
					}
				} else {
					LM_ERR("invalid number of spearators (%d)\n", n);
					goto done;
				}
			}
			p++;
		}
	}

	LM_DBG("setting dst uri: %.*s\n", dst_uri.len, dst_uri.s);
	if (set_dst_uri(_m, &dst_uri) != 0) {
		LM_ERR("failed to set dst-uri\n");
		free_params(params);
		return;
	}
	/* dst_uri changed, so it makes sense to re-use the current uri
	 * for forking */
	ruri_mark_new(); /* re-use uri for serial forking */

done:
	free_params(params);
}
