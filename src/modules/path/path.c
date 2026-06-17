/*
 * Path handling for intermediate proxies.
 *
 * Copyright (C) 2006 Inode GmbH (Andreas Granig <andreas.granig@inode.info>)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "../../core/config.h"
#include "../../core/strutils.h"
#include "../../core/dset.h"

#include "path.h"
#include "path_mod.h"

typedef enum
{
	PATH_PARAM_NONE = 0,
	PATH_PARAM_RECEIVED = 1,
	PATH_PARAM_OB = 2
} path_param_t;

#define PATH_PREFIX "Path: <sip:"
#define PATH_PREFIX_LEN (sizeof(PATH_PREFIX) - 1)

const static char *proto_strings[] = {
		[PROTO_TCP] = "%3Btransport%3Dtcp",
		[PROTO_TLS] = "%3Btransport%3Dtls",
		[PROTO_SCTP] = "%3Btransport%3Dsctp",
		[PROTO_WS] = "%3Btransport%3Dws",
		[PROTO_WSS] = "%3Btransport%3Dws",
};

extern int path_sockname_mode;
extern str path_received_name;

static char *path_strzdup(char *src, int len)
{
	char *res;

	if(!src)
		return NULL;
	if(len < 0) {
		len = strlen(src);
	}
	if(!(res = (char *)pkg_malloc(len + 1))) {
		PKG_MEM_ERROR;
		return NULL;
	}
	strncpy(res, src, len);
	res[len] = 0;

	return res;
}

static int handleOutbound(sip_msg_t *_m, str *user, path_param_t *param)
{
	if(path_obb.use_outbound != NULL && path_obb.use_outbound(_m)) {
		struct via_body *via;

		if(path_obb.encode_flow_token(user, &_m->rcv) != 0) {
			LM_ERR("encoding outbound flow-token\n");
			return -1;
		}

		/* Only include ;ob parameter if this is the first-hop
		 * (that means only one Via:) */
		if(parse_via_header(_m, 2, &via) < 0) {
			*param |= PATH_PARAM_OB;
		}
	}

	return 1;
}

/*! \brief
 * Validate the advertised address value given to the add_path_*_advertised_*
 * functions. The value is expected to be a valid host part, optionally
 * followed by a port (e.g. "proxy.example.com" or "1.2.3.4:5080").
 */
static int path_check_advertised_address(str *addr)
{
	char buf[MAX_URI_SIZE];
	struct sip_uri uri;
	int len;

	if(addr == NULL || addr->len <= 0 || addr->s == NULL) {
		LM_ERR("empty advertised address value\n");
		return -1;
	}

	if(addr->len > MAX_URI_SIZE - 7) {
		LM_ERR("advertised address is too long (%d)\n", addr->len);
		return -1;
	}

	memcpy(buf, "sip:", 4);
	memcpy(buf + 4, addr->s, addr->len);
	memcpy(buf + 4 + addr->len, ";lr", 3);
	len = 4 + addr->len + 3;

	memset(&uri, 0, sizeof(uri));
	if(parse_uri(buf, len, &uri) < 0 || uri.host.len <= 0) {
		LM_ERR("invalid advertised address: %.*s\n", addr->len, addr->s);
		return -1;
	}

	return 0;
}

static int prepend_path(sip_msg_t *_m, str *user, path_param_t param,
		str *add_params, str *adv_addr)
{
	struct lump *l;
	char *prefix, *suffix, *dp;
	str cp = STR_NULL;
	const char *proto_str;
	int prefix_len, suffix_len;
	struct hdr_field *hf;

	/* maximum possible length of suffix */
	suffix_len = sizeof(";lr;r2=on;=sip::12345%3Btransport%3Dsctp;ob;>\r\n")
				 + IP_ADDR_MAX_STR_SIZE + 2 + (add_params ? add_params->len : 0)
				 + path_received_name.len + 1;

	cp.s = suffix = pkg_malloc(suffix_len);
	if(!suffix) {
		PKG_MEM_ERROR_FMT("for suffix\n");
		goto out1;
	}

	cp.len = sprintf(cp.s, ";lr");

	if(param & PATH_PARAM_RECEIVED) {
		if(path_received_format == 0) {
			if(_m->rcv.proto
					< (sizeof(proto_strings) / sizeof(*proto_strings))) {
				proto_str = proto_strings[(unsigned int)_m->rcv.proto];
			} else {
				proto_str = NULL;
			}
			if(_m->rcv.src_ip.af == AF_INET6) {
				cp.len += snprintf(cp.s + cp.len, suffix_len - cp.len,
						";%s=sip:[%s]:%hu%s", path_received_name.s,
						ip_addr2a(&_m->rcv.src_ip), _m->rcv.src_port,
						proto_str ?: "");
			} else {
				cp.len += snprintf(cp.s + cp.len, suffix_len - cp.len,
						";%s=sip:%s:%hu%s", path_received_name.s,
						ip_addr2a(&_m->rcv.src_ip), _m->rcv.src_port,
						proto_str ?: "");
			}
		} else {
			if(_m->rcv.src_ip.af == AF_INET6) {
				cp.len += snprintf(cp.s + cp.len, suffix_len - cp.len,
						";%s=[%s]~%hu~%d", path_received_name.s,
						ip_addr2a(&_m->rcv.src_ip), _m->rcv.src_port,
						(int)_m->rcv.proto);
			} else {
				cp.len += snprintf(cp.s + cp.len, suffix_len - cp.len,
						";%s=%s~%hu~%d", path_received_name.s,
						ip_addr2a(&_m->rcv.src_ip), _m->rcv.src_port,
						(int)_m->rcv.proto);
			}
		}
	}

	if(param & PATH_PARAM_OB) {
		cp.len += sprintf(cp.s + cp.len, ";ob");
	}

	if(add_params && add_params->len) {
		cp.len += snprintf(cp.s + cp.len, suffix_len - cp.len, ";%.*s",
				add_params->len, add_params->s);
	}

	if(path_enable_r2 == 0) {
		cp.len += sprintf(cp.s + cp.len, ">\r\n");
	} else {
		cp.len += sprintf(cp.s + cp.len, ";r2=on>\r\n");
	}

	prefix_len = PATH_PREFIX_LEN + (user ? user->len : 0) + 2;
	prefix = pkg_malloc(prefix_len);
	if(!prefix) {
		PKG_MEM_ERROR_FMT("for prefix\n");
		goto out2;
	}
	if(user && user->len)
		prefix_len = snprintf(
				prefix, prefix_len, PATH_PREFIX "%.*s@", user->len, user->s);
	else
		prefix_len = snprintf(prefix, prefix_len, PATH_PREFIX);

	if(parse_headers(_m, HDR_PATH_F, 0) < 0) {
		LM_ERR("failed to parse message for Path header\n");
		goto out3;
	}
	hf = get_hdr(_m, HDR_PATH_T);
	if(hf)
		/* path found, add ours in front of that */
		l = anchor_lump(_m, hf->name.s - _m->buf, 0, 0);
	else
		/* no path, append to message */
		l = anchor_lump(_m, _m->unparsed - _m->buf, 0, 0);
	if(!l) {
		LM_ERR("failed to get anchor\n");
		goto out3;
	}

	l = insert_new_lump_before(l, prefix, prefix_len, 0);
	if(!l)
		goto out3;
	if(adv_addr && adv_addr->len > 0) {
		dp = path_strzdup(adv_addr->s, adv_addr->len);
		if(dp == NULL)
			goto out2;
		l = insert_new_lump_before(l, dp, adv_addr->len, 0);
		if(!l)
			goto out2;
		/* the advertised address replaces the host part only, so the socket
		 * name still has to be appended at send time if sockname_mode is on */
		if(path_sockname_mode) {
			l = insert_subst_lump_before(l, SUBST_SND_SOCKNAME, 0);
			if(!l)
				goto out2;
		}
	} else {
		l = insert_subst_lump_before(
				l, (path_sockname_mode) ? SUBST_SND_ALL_EX : SUBST_SND_ALL, 0);
		if(!l)
			goto out2;
	}
	l = insert_new_lump_before(l, suffix, cp.len, 0);
	if(!l)
		goto out2;

	if(path_enable_r2 != 0) {
		dp = path_strzdup(prefix, prefix_len);
		if(dp == NULL)
			goto out1;
		l = insert_new_lump_before(l, dp, prefix_len, 0);
		if(!l)
			goto out1;
		if(adv_addr && adv_addr->len > 0) {
			dp = path_strzdup(adv_addr->s, adv_addr->len);
			if(dp == NULL)
				goto out1;
			l = insert_new_lump_before(l, dp, adv_addr->len, 0);
			if(!l)
				goto out1;
			if(path_sockname_mode) {
				l = insert_subst_lump_before(l, SUBST_RCV_SOCKNAME, 0);
				if(!l)
					goto out1;
			}
		} else {
			l = insert_subst_lump_before(l,
					(path_sockname_mode) ? SUBST_RCV_ALL_EX : SUBST_RCV_ALL, 0);
			if(!l)
				goto out1;
		}
		dp = path_strzdup(suffix, cp.len);
		if(dp == NULL)
			goto out1;
		l = insert_new_lump_before(l, dp, cp.len, 0);
		if(!l)
			goto out1;
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
int ki_add_path(struct sip_msg *_msg)
{
	str user = {0, 0};
	int ret;
	path_param_t param = PATH_PARAM_NONE;

	ret = handleOutbound(_msg, &user, &param);

	if(ret > 0) {
		ret = prepend_path(_msg, &user, param, NULL, NULL);
	}

	if(user.s != NULL) {
		pkg_free(user.s);
	}

	return ret;
}

int add_path(sip_msg_t *_msg, char *_a, char *_b)
{
	return ki_add_path(_msg);
}

/*! \brief
 * Prepend own uri to Path header and take care of given
 * user.
 */
int add_path_usr(struct sip_msg *_msg, char *_usr, char *_parms)
{
	str user = {0, 0};
	str parms = {0, 0};

	if(_usr) {
		if(get_str_fparam(&user, _msg, (fparam_t *)_usr) < 0) {
			LM_ERR("failed to get user value\n");
			return -1;
		}
	}
	if(_parms) {
		if(get_str_fparam(&parms, _msg, (fparam_t *)_parms) < 0) {
			LM_ERR("failed to get params value\n");
			return -1;
		}
	}

	return prepend_path(_msg, &user, PATH_PARAM_NONE, &parms, NULL);
}

/*! \brief
 * Prepend own uri to Path header and take care of given
 * user.
 */
int ki_add_path_user(sip_msg_t *_msg, str *_user)
{
	str parms = {0, 0};
	return prepend_path(_msg, _user, PATH_PARAM_NONE, &parms, NULL);
}

/*! \brief
 * Prepend own uri to Path header and take care of given
 * user.
 */
int ki_add_path_user_params(sip_msg_t *_msg, str *_user, str *_params)
{
	return prepend_path(_msg, _user, PATH_PARAM_NONE, _params, NULL);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri.
 */
int ki_add_path_received(sip_msg_t *_msg)
{
	str user = {0, 0};
	int ret;
	path_param_t param = PATH_PARAM_RECEIVED;

	ret = handleOutbound(_msg, &user, &param);

	if(ret > 0) {
		ret = prepend_path(_msg, &user, param, NULL, NULL);
	}

	if(user.s != NULL) {
		pkg_free(user.s);
	}

	return ret;
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri.
 */
int add_path_received(struct sip_msg *_msg, char *_a, char *_b)
{
	return ki_add_path_received(_msg);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int add_path_received_usr(struct sip_msg *_msg, char *_usr, char *_parms)
{
	str user = {0, 0};
	str parms = {0, 0};

	if(_usr) {
		if(get_str_fparam(&user, _msg, (fparam_t *)_usr) < 0) {
			LM_ERR("failed to get user value\n");
			return -1;
		}
	}
	if(_parms) {
		if(get_str_fparam(&parms, _msg, (fparam_t *)_parms) < 0) {
			LM_ERR("failed to get params value\n");
			return -1;
		}
	}

	return prepend_path(_msg, &user, PATH_PARAM_RECEIVED, &parms, NULL);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int ki_add_path_received_user(sip_msg_t *_msg, str *_user)
{
	str parms = {0, 0};
	return prepend_path(_msg, _user, PATH_PARAM_RECEIVED, &parms, NULL);
}

/*! \brief
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int ki_add_path_received_user_params(sip_msg_t *_msg, str *_user, str *_params)
{
	return prepend_path(_msg, _user, PATH_PARAM_RECEIVED, _params, NULL);
}

/*! \brief
 * Prepend own uri to Path header building the host part from the given
 * advertised address instead of the outgoing socket address. Mirrors the
 * behaviour of add_path()/add_path_received() (including outbound handling).
 */
static int prepend_path_advertised_address(
		sip_msg_t *_msg, str *_addr, path_param_t param)
{
	str user = {0, 0};
	int ret;

	if(path_check_advertised_address(_addr) < 0)
		return -1;

	ret = handleOutbound(_msg, &user, &param);

	if(ret > 0) {
		ret = prepend_path(_msg, &user, param, NULL, _addr);
	}

	if(user.s != NULL) {
		pkg_free(user.s);
	}

	return ret;
}

/*! \brief
 * Prepend own uri to Path header building the host part from the given
 * advertised address instead of the outgoing socket address and take care of
 * the given user (and parameters). Mirrors add_path(user[, params]) /
 * add_path_received(user[, params]).
 */
static int prepend_path_advertised_address_usr(sip_msg_t *_msg, str *_addr,
		str *_user, str *_params, path_param_t param)
{
	if(path_check_advertised_address(_addr) < 0)
		return -1;

	return prepend_path(_msg, _user, param, _params, _addr);
}

/*! \brief
 * Resolve the config function parameters and prepend a Path header using the
 * given advertised address. When no user is given the behaviour matches
 * add_path()/add_path_received(), otherwise add_path(user[, params]) /
 * add_path_received(user[, params]).
 */
static int add_path_advertised_address_f(sip_msg_t *_msg, char *_addr,
		char *_usr, char *_parms, path_param_t param)
{
	str addr = {0, 0};
	str user = {0, 0};
	str parms = {0, 0};

	if(_addr == NULL) {
		LM_ERR("advertised address parameter is required\n");
		return -1;
	}
	if(get_str_fparam(&addr, _msg, (fparam_t *)_addr) < 0) {
		LM_ERR("failed to get advertised address value\n");
		return -1;
	}

	if(_usr == NULL) {
		return prepend_path_advertised_address(_msg, &addr, param);
	}

	if(get_str_fparam(&user, _msg, (fparam_t *)_usr) < 0) {
		LM_ERR("failed to get user value\n");
		return -1;
	}
	if(_parms) {
		if(get_str_fparam(&parms, _msg, (fparam_t *)_parms) < 0) {
			LM_ERR("failed to get params value\n");
			return -1;
		}
	}

	return prepend_path_advertised_address_usr(
			_msg, &addr, &user, &parms, param);
}

/*! \brief
 * Prepend own uri to Path header using the given advertised address.
 */
int add_path_advertised_address(
		sip_msg_t *_msg, char *_addr, char *_usr, char *_parms)
{
	return add_path_advertised_address_f(
			_msg, _addr, _usr, _parms, PATH_PARAM_NONE);
}

/*! \brief
 * Prepend own uri to Path header using the given advertised address and append
 * the received address as "received"-param.
 */
int add_path_received_advertised_address(
		sip_msg_t *_msg, char *_addr, char *_usr, char *_parms)
{
	return add_path_advertised_address_f(
			_msg, _addr, _usr, _parms, PATH_PARAM_RECEIVED);
}

/*! \brief
 * KEMI: prepend own uri to Path header using the given advertised address.
 */
int ki_add_path_advertised_address(sip_msg_t *_msg, str *_addr)
{
	return prepend_path_advertised_address(_msg, _addr, PATH_PARAM_NONE);
}

int ki_add_path_advertised_address_user(sip_msg_t *_msg, str *_addr, str *_user)
{
	str parms = {0, 0};
	return prepend_path_advertised_address_usr(
			_msg, _addr, _user, &parms, PATH_PARAM_NONE);
}

int ki_add_path_advertised_address_user_params(
		sip_msg_t *_msg, str *_addr, str *_user, str *_params)
{
	return prepend_path_advertised_address_usr(
			_msg, _addr, _user, _params, PATH_PARAM_NONE);
}

/*! \brief
 * KEMI: prepend own uri to Path header using the given advertised address and
 * append the received address as "received"-param.
 */
int ki_add_path_received_advertised_address(sip_msg_t *_msg, str *_addr)
{
	return prepend_path_advertised_address(_msg, _addr, PATH_PARAM_RECEIVED);
}

int ki_add_path_received_advertised_address_user(
		sip_msg_t *_msg, str *_addr, str *_user)
{
	str parms = {0, 0};
	return prepend_path_advertised_address_usr(
			_msg, _addr, _user, &parms, PATH_PARAM_RECEIVED);
}

int ki_add_path_received_advertised_address_user_params(
		sip_msg_t *_msg, str *_addr, str *_user, str *_params)
{
	return prepend_path_advertised_address_usr(
			_msg, _addr, _user, _params, PATH_PARAM_RECEIVED);
}

/*! \brief
 * rr callback
 */
void path_rr_callback(struct sip_msg *_m, str *r_param, void *cb_param)
{
	param_hooks_t hooks;
	param_t *params;
	param_t *it;
	static char dst_uri_buf[MAX_URI_SIZE];
	static str dst_uri;
	char *p;
	int n;
	int nproto;
	str sproto;
	str rcvuri = STR_NULL;

	if((path_received_name.len == 8)
			&& strncmp(path_received_name.s, "received", 8) == 0) {
		if(parse_params(r_param, CLASS_CONTACT, &hooks, &params) != 0) {
			LM_ERR("failed to parse route parameters\n");
			return;
		}
		if(hooks.contact.received == NULL
				|| hooks.contact.received->body.len <= 0) {
			LM_DBG("no received parameter in route header\n");
			free_params(params);
			return;
		}
		rcvuri = hooks.contact.received->body;
	} else {
		if(parse_params(r_param, CLASS_ANY, &hooks, &params) != 0) {
			LM_ERR("failed to parse route parameters\n");
			return;
		}
		for(it = params; it; it = it->next) {
			if((it->name.len == path_received_name.len)
					&& strncmp(path_received_name.s, it->name.s, it->name.len)
							   == 0) {
				break;
			}
		}
		if(it == NULL || it->body.len <= 0) {
			LM_DBG("no %s parameter in route header\n", path_received_name.s);
			free_params(params);
			return;
		}
		rcvuri = it->body;
	}

	/* 24 => sip:...;transport=sctp */
	if(rcvuri.len + 24 >= MAX_URI_SIZE) {
		LM_ERR("received uri is too long: %d\n", rcvuri.len);
		goto done;
	}
	dst_uri.s = dst_uri_buf;
	dst_uri.len = MAX_URI_SIZE;
	if(path_received_format == 0) {
		/* received=sip:...;transport... */
		if(unescape_user(&rcvuri, &dst_uri) < 0) {
			LM_ERR("unescaping received failed\n");
			free_params(params);
			return;
		}
	} else {
		/* received=ip~port~proto */
		memcpy(dst_uri_buf, "sip:", 4);
		memcpy(dst_uri_buf + 4, rcvuri.s, rcvuri.len);
		dst_uri_buf[4 + rcvuri.len] = '\0';
		p = dst_uri_buf + 4;
		n = 0;
		while(*p != '\0') {
			if(*p == '~') {
				n++;
				if(n == 1) {
					/* port */
					*p = ':';
				} else if(n == 2) {
					/* proto */
					*p = ';';
					p++;
					if(*p == '\0') {
						LM_ERR("invalid received format\n");
						goto done;
					}
					nproto = *p - '0';
					if(nproto != PROTO_UDP) {
						proto_type_to_str(nproto, &sproto);
						if(sproto.len == 0) {
							LM_ERR("unknown proto in received param\n");
							goto done;
						}
						memcpy(p, "transport=", 10);
						p += 10;
						memcpy(p, sproto.s, sproto.len);
						p += sproto.len;
					} else {
						/* go back one byte to overwrite ';' */
						p--;
					}
					*p = '\0';
					dst_uri.len = p - dst_uri_buf;
					break;
				} else {
					LM_ERR("invalid number of separators (%d)\n", n);
					goto done;
				}
			}
			p++;
		}
	}

	LM_DBG("setting dst uri: %.*s\n", dst_uri.len, dst_uri.s);
	if(set_dst_uri(_m, &dst_uri) != 0) {
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
