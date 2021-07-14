/**
 * Copyright (C) 2019 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/pvapi.h"
#include "../../core/kemi.h"
#include "../../core/ppcfg.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/parser/contact/parse_contact.h"

#include "../../core/tcp_conn.h"

MODULE_VERSION

/* clang-format off */
struct module_exports exports = {
	"kemix",         /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,               /* cmd (cfg function) exports */
	0,               /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	0,               /* module init function */
	0,               /* per-child init function */
	0                /* module destroy function */
};
/* clang-format on */


/**
 *
 */
static sr_kemi_xval_t _sr_kemi_kx_xval = {0};

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ruri(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	if(msg->first_line.type == SIP_REPLY) {
		/* REPLY doesnt have a ruri */
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0) {
		LM_ERR("failed to parse the R-URI\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	if (msg->new_uri.s!=NULL) {
		_sr_kemi_kx_xval.v.s = msg->new_uri;
	} else {
		_sr_kemi_kx_xval.v.s = msg->first_line.u.request.uri;
	}
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ouri(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	if(msg->first_line.type == SIP_REPLY) {
		/* REPLY doesnt have a ruri */
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0) {
		LM_ERR("failed to parse the R-URI\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->first_line.u.request.uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_furi(sip_msg_t *msg)
{
	to_body_t *xto = NULL;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}
	if(parse_from_header(msg)<0) {
		LM_ERR("cannot parse From header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}
	if(msg->from==NULL || get_from(msg)==NULL) {
		LM_DBG("no From header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	xto = get_from(msg);
	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = xto->uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_turi(sip_msg_t *msg)
{
	to_body_t *xto = NULL;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}
	if(parse_to_header(msg)<0) {
		LM_ERR("cannot parse To header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}
	if(msg->to==NULL || get_to(msg)==NULL) {
		LM_DBG("no To header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	xto = get_to(msg);
	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = xto->uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_xuri_attr(sip_msg_t *msg, sip_uri_t *puri,
		int iattr, int xmode)
{
	if(iattr==1) {
		/* username */
		if(puri->user.s==NULL || puri->user.len<=0) {
			sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
			return &_sr_kemi_kx_xval;
		}
		_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_kx_xval.v.s = puri->user;
		return &_sr_kemi_kx_xval;
	} else if(iattr==2) {
		/* domain */
		if(puri->host.s==NULL || puri->host.len<=0) {
			sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
			return &_sr_kemi_kx_xval;
		}
		_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_kx_xval.v.s = puri->host;
		return &_sr_kemi_kx_xval;
	}
	LM_ERR("unknown attribute id: %d\n", iattr);
	sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ruri_attr(sip_msg_t *msg, int iattr, int xmode)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL || msg->first_line.type == SIP_REPLY) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0) {
		LM_ERR("failed to parse the R-URI\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}
	return ki_kx_get_xuri_attr(msg, &(msg->parsed_uri), iattr, xmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ruser(sip_msg_t *msg)
{
	return ki_kx_get_ruri_attr(msg, 1, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_ruser(sip_msg_t *msg)
{
	return ki_kx_get_ruri_attr(msg, 1, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_ruser(sip_msg_t *msg)
{
	return ki_kx_get_ruri_attr(msg, 1, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_rhost(sip_msg_t *msg)
{
	return ki_kx_get_ruri_attr(msg, 2, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_rhost(sip_msg_t *msg)
{
	return ki_kx_get_ruri_attr(msg, 2, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_rhost(sip_msg_t *msg)
{
	return ki_kx_get_ruri_attr(msg, 2, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_furi_attr(sip_msg_t *msg, int iattr, int xmode)
{
	sip_uri_t *uri;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	uri=parse_from_uri(msg);
	if(uri==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	return ki_kx_get_xuri_attr(msg, uri, iattr, xmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_fuser(sip_msg_t *msg)
{
	return ki_kx_get_furi_attr(msg, 1, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_fuser(sip_msg_t *msg)
{
	return ki_kx_get_furi_attr(msg, 1, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_fuser(sip_msg_t *msg)
{
	return ki_kx_get_furi_attr(msg, 1, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_fhost(sip_msg_t *msg)
{
	return ki_kx_get_furi_attr(msg, 2, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_fhost(sip_msg_t *msg)
{
	return ki_kx_get_furi_attr(msg, 2, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_fhost(sip_msg_t *msg)
{
	return ki_kx_get_furi_attr(msg, 2, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_turi_attr(sip_msg_t *msg, int iattr, int xmode)
{
	sip_uri_t *uri;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	uri=parse_to_uri(msg);
	if(uri==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	return ki_kx_get_xuri_attr(msg, uri, iattr, xmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_tuser(sip_msg_t *msg)
{
	return ki_kx_get_turi_attr(msg, 1, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_tuser(sip_msg_t *msg)
{
	return ki_kx_get_turi_attr(msg, 1, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_tuser(sip_msg_t *msg)
{
	return ki_kx_get_turi_attr(msg, 1, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_thost(sip_msg_t *msg)
{
	return ki_kx_get_turi_attr(msg, 2, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_thost(sip_msg_t *msg)
{
	return ki_kx_get_turi_attr(msg, 2, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_thost(sip_msg_t *msg)
{
	return ki_kx_get_turi_attr(msg, 2, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ua_mode(sip_msg_t *msg, int rmode)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, rmode);
		return &_sr_kemi_kx_xval;
	}
	if(msg->user_agent==NULL && ((parse_headers(msg, HDR_USERAGENT_F, 0)==-1)
			|| (msg->user_agent==NULL))) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, rmode);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->user_agent->body;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ua(sip_msg_t *msg)
{
	return ki_kx_get_ua_mode(msg, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_ua(sip_msg_t *msg)
{
	return ki_kx_get_ua_mode(msg, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_ua(sip_msg_t *msg)
{
	return ki_kx_get_ua_mode(msg, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_srcip(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s.s = ip_addr2a(&msg->rcv.src_ip);
	_sr_kemi_kx_xval.v.s.len = strlen(_sr_kemi_kx_xval.v.s.s);
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_srcport(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		_sr_kemi_kx_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_kx_xval.v.n = 0;
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_INT;
	_sr_kemi_kx_xval.v.n = (int)msg->rcv.src_port;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_send_sock(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if (msg==NULL || msg->force_send_socket==0) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->force_send_socket->sock_str;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_send_sock_name(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if (msg==NULL || msg->force_send_socket==0
			|| msg->force_send_socket->sockname.s==0) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->force_send_socket->sockname;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static int ki_kx_get_send_sock_port(sip_msg_t *msg)
{
	if (msg==NULL || msg->force_send_socket==0) {
		return 0;
	}
	return (int)msg->force_send_socket->port_no;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_rcvip(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL || msg->rcv.bind_address==NULL
			|| msg->rcv.bind_address->address_str.s==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->rcv.bind_address->address_str;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_rcvport(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL || msg->rcv.bind_address==NULL) {
		_sr_kemi_kx_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_kx_xval.v.n = 0;
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_INT;
	_sr_kemi_kx_xval.v.n = (int)msg->rcv.bind_address->port_no;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_rcv_sock_name(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL || msg->rcv.bind_address==NULL
			|| msg->rcv.bind_address->sockname.s==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->rcv.bind_address->sockname;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_rcvadvip(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg->rcv.bind_address!=NULL
			&& msg->rcv.bind_address->useinfo.address_str.len > 0) {
		_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_kx_xval.v.s = msg->rcv.bind_address->useinfo.address_str;
		return &_sr_kemi_kx_xval;
	}

	return ki_kx_get_rcvip(msg);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_rcvadvport(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg->rcv.bind_address!=NULL
			&& msg->rcv.bind_address->useinfo.port_no_str.len > 0) {
		_sr_kemi_kx_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_kx_xval.v.n = (int)msg->rcv.bind_address->useinfo.port_no;
		return &_sr_kemi_kx_xval;
	}

	return ki_kx_get_rcvport(msg);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_proto(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	if(get_valid_proto_string(msg->rcv.proto, 0, 0, &_sr_kemi_kx_xval.v.s)<0) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
	}

	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static int ki_kx_get_protoid(sip_msg_t *msg)
{
	if(msg==NULL) {
		return -1;
	}
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	_sr_kemi_kx_xval.vtype = SR_KEMIP_INT;
	return (int)msg->rcv.proto;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_auth_attr(sip_msg_t *msg, int iattr, int xmode)
{
	hdr_field_t *hdr;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	if ((msg->REQ_METHOD == METHOD_ACK) ||
			(msg->REQ_METHOD == METHOD_CANCEL)) {
		LM_DBG("no [Proxy-]Authorization header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	if ((parse_headers(msg, HDR_PROXYAUTH_F|HDR_AUTHORIZATION_F, 0)==-1)
			|| (msg->proxy_auth==0 && msg->authorization==0)) {
		LM_DBG("no [Proxy-]Authorization header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	hdr = (msg->proxy_auth==0)?msg->authorization:msg->proxy_auth;

	if(parse_credentials(hdr)!=0) {
		LM_ERR("failed to parse credentials\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}
	switch(iattr) {
		case 1:
			_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
			_sr_kemi_kx_xval.v.s = ((auth_body_t*)(hdr->parsed))->digest.username.user;
			return &_sr_kemi_kx_xval;

		break;
		default:
			_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
			_sr_kemi_kx_xval.v.s = ((auth_body_t*)(hdr->parsed))->digest.username.whole;
			return &_sr_kemi_kx_xval;
	}
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_au(sip_msg_t *msg)
{
	return ki_kx_get_auth_attr(msg, 1, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_au(sip_msg_t *msg)
{
	return ki_kx_get_auth_attr(msg, 1, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_au(sip_msg_t *msg)
{
	return ki_kx_get_auth_attr(msg, 1, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_method(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	if(msg->first_line.type == SIP_REQUEST) {
		_sr_kemi_kx_xval.v.s = msg->first_line.u.request.method;
		return &_sr_kemi_kx_xval;
	}

	if(msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ_F, 0)==-1) ||
				(msg->cseq==NULL))) {
		LM_ERR("no CSEQ header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.v.s = get_cseq(msg)->method;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static int ki_kx_get_status(sip_msg_t *msg)
{
	if(msg==NULL) {
		return -1;
	}
	if(msg->first_line.type != SIP_REPLY) {
		return -1;
	}
	return (int)msg->first_line.u.reply.statuscode;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gets_status(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}
	if(msg->first_line.type != SIP_REPLY) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_kx_xval;
	}
	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->first_line.u.reply.status;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_body_mode(sip_msg_t *msg, int rmode)
{
	str s;
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, rmode);
		return &_sr_kemi_kx_xval;
	}

	s.s = get_body(msg);

	if(s.s == NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, rmode);
		return &_sr_kemi_kx_xval;
	}
	s.len = msg->buf + msg->len - s.s;
	if(s.len <=0) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, rmode);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = s;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_body(sip_msg_t *msg)
{
	return ki_kx_get_body_mode(msg, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_body(sip_msg_t *msg)
{
	return ki_kx_get_body_mode(msg, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_body(sip_msg_t *msg)
{
	return ki_kx_get_body_mode(msg, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static int ki_kx_get_bodylen(sip_msg_t *msg)
{
	str s;
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		return 0;
	}

	s.s = get_body(msg);

	if(s.s == NULL) {
		return 0;
	}
	s.len = msg->buf + msg->len - s.s;
	if(s.len <=0) {
		return 0;
	}

	return s.len;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_duri_mode(sip_msg_t *msg, int xmode)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg->dst_uri.s==NULL || msg->dst_uri.len<=0) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->dst_uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_duri(sip_msg_t *msg)
{
	return ki_kx_get_duri_mode(msg, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_duri(sip_msg_t *msg)
{
	return ki_kx_get_duri_mode(msg, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_duri(sip_msg_t *msg)
{
	return ki_kx_get_duri_mode(msg, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_nhuri(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg->dst_uri.s==NULL || msg->dst_uri.len<=0) {
		return ki_kx_get_ruri(msg);
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->dst_uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static int ki_kx_get_timestamp(sip_msg_t *msg)
{
	return (int)time(NULL);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_callid(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}
	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID_F, 0)==-1)
			|| (msg->callid==NULL))) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->callid->body;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static int ki_kx_get_conid(sip_msg_t *msg)
{
	tcp_connection_t *con;
	int conid;

	if (msg == NULL)
		return -1;

	if ((con = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0, 0)) == NULL)
		return -1;

	conid = con->id;
	tcpconn_put(con);

	return conid;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_msgbuf(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s.s = msg->buf;
	_sr_kemi_kx_xval.v.s.len = msg->len;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static int ki_kx_get_msglen(sip_msg_t *msg)
{
	if(msg==NULL) {
		return -1;
	}

	return msg->len;
}

/**
 *
 */
static int ki_kx_get_msgtype(sip_msg_t *msg)
{
	if(msg==NULL) {
		return -1;
	}

	if(msg->first_line.type == SIP_REQUEST) {
		return 1;
	} else if(msg->first_line.type == SIP_REPLY) {
		return 2;
	}

	return 0;
}


/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_cturi_mode(sip_msg_t *msg, int xmode)
{
	contact_t *c;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if((parse_contact_headers(msg) < 0) || (msg->contact == NULL)
			|| (msg->contact->parsed == NULL)) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	if (((contact_body_t*)msg->contact->parsed)->star == 1) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	c = (((contact_body_t*)msg->contact->parsed)->contacts);
	if(c==NULL || c->uri.s==NULL || c->uri.len<=0) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = c->uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_cturi(sip_msg_t *msg)
{
	return ki_kx_get_cturi_mode(msg, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_getw_cturi(sip_msg_t *msg)
{
	return ki_kx_get_cturi_mode(msg, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_gete_cturi(sip_msg_t *msg)
{
	return ki_kx_get_cturi_mode(msg, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_srcuri(sip_msg_t *msg)
{
	str ssock;

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;

	}

	if(get_src_uri(msg, 0, &ssock)<0) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}

	if (ssock.len + 1 >= pv_get_buffer_size()) {
		LM_ERR("local buffer size exceeded\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.v.s.s = pv_get_buffer();
	strncpy(_sr_kemi_kx_xval.v.s.s, ssock.s, ssock.len);
	_sr_kemi_kx_xval.v.s.len = ssock.len;
	_sr_kemi_kx_xval.v.s.s[_sr_kemi_kx_xval.v.s.len] = '\0';

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_def(sip_msg_t *msg, str *dname)
{
	str *val;

	val = pp_define_get(dname->len, dname->s);

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if (val==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s.s = val->s;
	_sr_kemi_kx_xval.v.s.len = val->len;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static int ki_kx_get_defn(sip_msg_t *msg, str *dname)
{
	str *val;
	int n = 0;

	val = pp_define_get(dname->len, dname->s);

	if (val != NULL) {
		str2sint(val, &n);
	}

	return n;
}

/**
 *
 */
static int ki_kx_ifdef(sip_msg_t *msg, str *dname)
{
	str *val;

	val = pp_define_get(dname->len, dname->s);

	if (val!=NULL) {
		return SR_KEMI_TRUE;
	}
	return SR_KEMI_FALSE;
}

/**
 *
 */
static int ki_kx_ifndef(sip_msg_t *msg, str *dname)
{
	str *val;

	val = pp_define_get(dname->len, dname->s);

	if (val==NULL) {
		return SR_KEMI_TRUE;
	}
	return SR_KEMI_FALSE;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_env(sip_msg_t *msg, str *envname)
{
	char *val;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));
	if(envname==NULL || envname->s==NULL || envname->len<=0) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}

	val = getenv(envname->s);
	if (val == NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s.s = val;
	_sr_kemi_kx_xval.v.s.len = strlen(val);
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static int ki_kx_get_envn(sip_msg_t *msg, str *envname)
{
	str val;
	int r = 0;

	if(envname==NULL || envname->s==NULL || envname->len<=0) {
		return 0;
	}

	val.s = getenv(envname->s);
	if (val.s) {
		val.len = strlen(val.s);
		str2sint(&val, &r);
		return r;
	}

	return r;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_kx_exports[] = {
	{ str_init("kx"), str_init("get_ruri"),
		SR_KEMIP_XVAL, ki_kx_get_ruri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_ruser"),
		SR_KEMIP_XVAL, ki_kx_get_ruser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_ruser"),
		SR_KEMIP_XVAL, ki_kx_getw_ruser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_ruser"),
		SR_KEMIP_XVAL, ki_kx_gete_ruser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_rhost"),
		SR_KEMIP_XVAL, ki_kx_get_rhost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_rhost"),
		SR_KEMIP_XVAL, ki_kx_getw_rhost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_rhost"),
		SR_KEMIP_XVAL, ki_kx_gete_rhost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_ouri"),
		SR_KEMIP_XVAL, ki_kx_get_ouri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_furi"),
		SR_KEMIP_XVAL, ki_kx_get_furi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_fuser"),
		SR_KEMIP_XVAL, ki_kx_get_fuser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_fuser"),
		SR_KEMIP_XVAL, ki_kx_getw_fuser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_fuser"),
		SR_KEMIP_XVAL, ki_kx_gete_fuser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_fhost"),
		SR_KEMIP_XVAL, ki_kx_get_fhost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_fhost"),
		SR_KEMIP_XVAL, ki_kx_getw_fhost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_fhost"),
		SR_KEMIP_XVAL, ki_kx_gete_fhost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_turi"),
		SR_KEMIP_XVAL, ki_kx_get_turi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_tuser"),
		SR_KEMIP_XVAL, ki_kx_get_tuser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_tuser"),
		SR_KEMIP_XVAL, ki_kx_gete_tuser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_tuser"),
		SR_KEMIP_XVAL, ki_kx_getw_tuser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_thost"),
		SR_KEMIP_XVAL, ki_kx_get_thost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_thost"),
		SR_KEMIP_XVAL, ki_kx_gete_thost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_thost"),
		SR_KEMIP_XVAL, ki_kx_getw_thost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_duri"),
		SR_KEMIP_XVAL, ki_kx_get_duri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_duri"),
		SR_KEMIP_XVAL, ki_kx_gete_duri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_duri"),
		SR_KEMIP_XVAL, ki_kx_getw_duri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_cturi"),
		SR_KEMIP_XVAL, ki_kx_get_cturi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_cturi"),
		SR_KEMIP_XVAL, ki_kx_gete_cturi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_cturi"),
		SR_KEMIP_XVAL, ki_kx_getw_cturi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_nhuri"),
		SR_KEMIP_XVAL, ki_kx_get_nhuri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_srcuri"),
		SR_KEMIP_XVAL, ki_kx_get_srcuri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_ua"),
		SR_KEMIP_XVAL, ki_kx_get_ua,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_ua"),
		SR_KEMIP_XVAL, ki_kx_gete_ua,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_ua"),
		SR_KEMIP_XVAL, ki_kx_getw_ua,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_srcip"),
		SR_KEMIP_XVAL, ki_kx_get_srcip,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_srcport"),
		SR_KEMIP_XVAL, ki_kx_get_srcport,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_send_sock"),
		SR_KEMIP_XVAL, ki_kx_get_send_sock,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_send_sock_name"),
		SR_KEMIP_XVAL, ki_kx_get_send_sock_name,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_send_sock_port"),
		SR_KEMIP_INT, ki_kx_get_send_sock_port,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_rcvip"),
		SR_KEMIP_XVAL, ki_kx_get_rcvip,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_rcv_sock_name"),
		SR_KEMIP_XVAL, ki_kx_get_rcv_sock_name,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_rcvport"),
		SR_KEMIP_XVAL, ki_kx_get_rcvport,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_rcvadvip"),
		SR_KEMIP_XVAL, ki_kx_get_rcvadvip,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_rcvadvport"),
		SR_KEMIP_XVAL, ki_kx_get_rcvadvport,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_proto"),
		SR_KEMIP_XVAL, ki_kx_get_proto,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_protoid"),
		SR_KEMIP_INT, ki_kx_get_protoid,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_au"),
		SR_KEMIP_XVAL, ki_kx_get_au,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_au"),
		SR_KEMIP_XVAL, ki_kx_getw_au,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_au"),
		SR_KEMIP_XVAL, ki_kx_gete_au,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_method"),
		SR_KEMIP_XVAL, ki_kx_get_method,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_status"),
		SR_KEMIP_INT, ki_kx_get_status,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gets_status"),
		SR_KEMIP_XVAL, ki_kx_gets_status,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_body"),
		SR_KEMIP_XVAL, ki_kx_get_body,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("getw_body"),
		SR_KEMIP_XVAL, ki_kx_getw_body,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("gete_body"),
		SR_KEMIP_XVAL, ki_kx_gete_body,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_bodylen"),
		SR_KEMIP_INT, ki_kx_get_bodylen,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_timestamp"),
		SR_KEMIP_INT, ki_kx_get_timestamp,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_callid"),
		SR_KEMIP_XVAL, ki_kx_get_callid,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_conid"),
		SR_KEMIP_INT, ki_kx_get_conid,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_msgbuf"),
		SR_KEMIP_XVAL, ki_kx_get_msgbuf,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_msglen"),
		SR_KEMIP_INT, ki_kx_get_msglen,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_msgtype"),
		SR_KEMIP_INT, ki_kx_get_msgtype,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_def"),
		SR_KEMIP_XVAL, ki_kx_get_def,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_defn"),
		SR_KEMIP_INT, ki_kx_get_defn,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("ifdef"),
		SR_KEMIP_BOOL, ki_kx_ifdef,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("ifndef"),
		SR_KEMIP_BOOL, ki_kx_ifndef,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_env"),
		SR_KEMIP_XVAL, ki_kx_get_env,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_envn"),
		SR_KEMIP_INT, ki_kx_get_envn,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},


	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_kx_exports);
	return 0;
}
