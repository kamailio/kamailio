/*
 * $Id$
 *
 * Copyright (C) 2005-2006 iptelorg GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2005-12-19  select framework, basic core functions (mma)
 *  2006-01-19  multiple nested calls, IS_ALIAS -> NESTED flag renamed (mma)
 */

 
#ifndef _SELECT_CORE_H
#define _SELECT_CORE_H

#include "str.h"
#include "parser/msg_parser.h"
#include "select.h"

enum {
	SEL_PARAM_TAG, 
	SEL_PARAM_Q, SEL_PARAM_EXPIRES, SEL_PARAM_METHOD, SEL_PARAM_RECEIVED, SEL_PARAM_INSTANCE, 
	SEL_PARAM_BRANCH, SEL_PARAM_RPORT, SEL_PARAM_I, SEL_PARAM_ALIAS
       };

enum {
	SEL_AUTH_PROXY,
	SEL_AUTH_WWW,
	SEL_AUTH_USERNAME,
	SEL_AUTH_USER,
	SEL_AUTH_DOMAIN,
	SEL_AUTH_REALM,
	SEL_AUTH_NONCE,
	SEL_AUTH_URI,
	SEL_AUTH_CNONCE,
	SEL_AUTH_NC,
	SEL_AUTH_RESPONSE,
	SEL_AUTH_OPAQUE,
	SEL_AUTH_ALG,
	SEL_AUTH_QOP
};

SELECT_F(select_ruri)
SELECT_F(select_from)
SELECT_F(select_from_uri)
SELECT_F(select_from_tag)
SELECT_F(select_from_name)
SELECT_F(select_from_params)
SELECT_F(select_to)
SELECT_F(select_to_uri)
SELECT_F(select_to_tag)
SELECT_F(select_to_name)
SELECT_F(select_to_params)
SELECT_F(select_contact)
SELECT_F(select_contact_uri)
SELECT_F(select_contact_name)
SELECT_F(select_contact_params)
SELECT_F(select_contact_params_spec)
SELECT_F(select_via)
SELECT_F(select_via_name)
SELECT_F(select_via_version)
SELECT_F(select_via_transport)
SELECT_F(select_via_host)
SELECT_F(select_via_port)
SELECT_F(select_via_comment)
SELECT_F(select_via_params)
SELECT_F(select_via_params_spec)

SELECT_F(select_msgheader)
SELECT_F(select_anyheader)

SELECT_F(select_any_nameaddr)
SELECT_F(select_nameaddr_name)
SELECT_F(select_nameaddr_uri)
	
SELECT_F(select_any_uri)
SELECT_F(select_uri_type)
SELECT_F(select_uri_user)
SELECT_F(select_uri_pwd)
SELECT_F(select_uri_host)
SELECT_F(select_uri_port)
SELECT_F(select_uri_hostport)
SELECT_F(select_uri_params)

SELECT_F(select_event)

SELECT_F(select_rr)
SELECT_F(select_rr_uri)
SELECT_F(select_rr_name)
SELECT_F(select_rr_params)

SELECT_F(select_cseq)
SELECT_F(select_cseq_method)
SELECT_F(select_cseq_num)

SELECT_F(select_auth)
SELECT_F(select_auth_param)
SELECT_F(select_auth_username)
SELECT_F(select_auth_username_comp)

static select_row_t select_core[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("ruri"), select_ruri, 0},
	{ select_ruri, SEL_PARAM_STR, STR_NULL, select_any_uri, NESTED},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("from"), select_from, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("f"), select_from, 0},
	{ select_from, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_from_uri, 0},
	{ select_from, SEL_PARAM_STR, STR_STATIC_INIT("tag"), select_from_tag, 0},
	{ select_from, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_from_name, 0},
	{ select_from, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_from_params, CONSUME_NEXT_STR},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("to"), select_to, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("t"), select_to, 0},
	{ select_to, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_to_uri, 0},
	{ select_to, SEL_PARAM_STR, STR_STATIC_INIT("tag"), select_to_tag, 0},
	{ select_to, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_to_name, 0},
	{ select_to, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_to_params, CONSUME_NEXT_STR},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("contact"), select_contact, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("m"), select_contact, 0},
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_contact_uri, 0},
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_contact_name, 0}, 
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("q"), select_contact_params_spec, DIVERSION | SEL_PARAM_Q}, 
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("expires"), select_contact_params_spec, DIVERSION | SEL_PARAM_EXPIRES}, 
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("method"), select_contact_params_spec, DIVERSION | SEL_PARAM_METHOD}, 
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("received"), select_contact_params_spec, DIVERSION | SEL_PARAM_RECEIVED}, 
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("instance"), select_contact_params_spec, DIVERSION | SEL_PARAM_INSTANCE}, 	
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_contact_params, CONSUME_NEXT_STR},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("via"), select_via, OPTIONAL | CONSUME_NEXT_INT},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("v"), select_via, OPTIONAL | CONSUME_NEXT_INT},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_via_name, 0},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("version"), select_via_version, 0},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("transport"), select_via_transport, 0},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("host"), select_via_host, 0},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("port"), select_via_port, 0},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("comment"), select_via_comment, 0},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("branch"), select_via_params_spec, DIVERSION | SEL_PARAM_BRANCH},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("received"), select_via_params_spec, DIVERSION | SEL_PARAM_RECEIVED},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("rport"), select_via_params_spec, DIVERSION | SEL_PARAM_RPORT},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("i"), select_via_params_spec, DIVERSION | SEL_PARAM_I},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("alias"), select_via_params_spec, DIVERSION | SEL_PARAM_ALIAS},
	{ select_via, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_via_params, CONSUME_NEXT_STR},
	
	{ select_from_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ select_to_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ select_contact_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ select_rr_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("type"), select_uri_type, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("user"), select_uri_user, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("pwd"), select_uri_pwd, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("host"), select_uri_host, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("port"), select_uri_port, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_uri_params, CONSUME_NEXT_STR | OPTIONAL},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("hostport"), select_uri_hostport, 0},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("event"), select_event, 0},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("record_route"), select_rr, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("rr"), select_rr, 0},
	{ select_rr, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_rr_uri, 0},
	{ select_rr, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_rr_name, 0}, 
	{ select_rr, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_rr_params, CONSUME_NEXT_STR},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("cseq"), select_cseq, 0},
	{ select_cseq, SEL_PARAM_STR, STR_STATIC_INIT("num"), select_cseq_num, 0},
	{ select_cseq, SEL_PARAM_STR, STR_STATIC_INIT("method"), select_cseq_method, 0},

	{ select_any_nameaddr, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_nameaddr_name, 0},
	{ select_any_nameaddr, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_nameaddr_uri, 0},
	{ select_nameaddr_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("msg"), select_msgheader, SEL_PARAM_EXPECTED},
	{ select_msgheader, SEL_PARAM_STR, STR_NULL, select_anyheader, OPTIONAL | CONSUME_NEXT_INT | FIXUP_CALL},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("proxy_authorization"), select_auth, CONSUME_NEXT_STR | DIVERSION | SEL_AUTH_PROXY},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("authorization"), select_auth, CONSUME_NEXT_STR | DIVERSION | SEL_AUTH_WWW}, 
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("username"), select_auth_username, DIVERSION | SEL_AUTH_USERNAME},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("realm"), select_auth_param, DIVERSION | SEL_AUTH_REALM},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("nonce"), select_auth_param, DIVERSION | SEL_AUTH_NONCE},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_auth_param, DIVERSION | SEL_AUTH_URI},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("cnonce"), select_auth_param, DIVERSION | SEL_AUTH_CNONCE},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("nc"), select_auth_param, DIVERSION | SEL_AUTH_NC},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("response"), select_auth_param, DIVERSION | SEL_AUTH_RESPONSE},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("opaque"), select_auth_param, DIVERSION | SEL_AUTH_OPAQUE},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("algorithm"), select_auth_param, DIVERSION | SEL_AUTH_ALG},
	{ select_auth, SEL_PARAM_STR, STR_STATIC_INIT("qop"), select_auth_param, DIVERSION | SEL_AUTH_QOP},
	{ select_auth_username, SEL_PARAM_STR, STR_STATIC_INIT("user"), select_auth_username_comp, DIVERSION | SEL_AUTH_USER},
	{ select_auth_username, SEL_PARAM_STR, STR_STATIC_INIT("domain"), select_auth_username_comp, DIVERSION | SEL_AUTH_DOMAIN},

	{ select_anyheader, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

static select_table_t select_core_table = {select_core, NULL};

#endif // _SELECT_CORE_H
