/*
 * Copyright (C) 2005-2006 iptelorg GmbH
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

/*!
* \file
* \brief Kamailio core :: Select framework, basic core functions
* \author mma
* \ingroup core
* Module: \ref core
*/

 
#ifndef _SELECT_CORE_H
#define _SELECT_CORE_H

#include "str.h"
#include "parser/msg_parser.h"
#include "select.h"

enum {
	SEL_PARAM_TAG, 
	SEL_PARAM_Q, SEL_PARAM_EXPIRES, SEL_PARAM_METHODS, SEL_PARAM_RECEIVED, SEL_PARAM_INSTANCE, 
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

enum {
	SEL_SRC  = 1<<0,
	SEL_DST  = 1<<1,
	SEL_RCV  = 1<<2,
	SEL_PROTO= 1<<5,
	SEL_IP   = 1<<6,
	SEL_PORT = 1<<7,
	SEL_IP_PORT = SEL_IP | SEL_PORT,
};

enum {
	SEL_NOW_GMT = 1,
	SEL_NOW_LOCAL = 2
};

enum {
	SEL_BRANCH_URI = 1<<0,
	SEL_BRANCH_Q = 1<<1,
	SEL_BRANCH_DST_URI = 1<<2
};

SELECT_F(select_ruri)
SELECT_F(select_dst_uri)
SELECT_F(select_next_hop)
SELECT_F(select_next_hop_src_ip)
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
SELECT_F(select_refer_to)
SELECT_F(select_refer_to_uri)
SELECT_F(select_refer_to_tag)
SELECT_F(select_refer_to_name)
SELECT_F(select_refer_to_params)
SELECT_F(select_rpid)
SELECT_F(select_rpid_uri)
SELECT_F(select_rpid_tag)
SELECT_F(select_rpid_name)
SELECT_F(select_rpid_params)
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

SELECT_F(select_msg)
SELECT_F(select_msg_first_line)
SELECT_F(select_msg_flags)
SELECT_F(select_msg_type)
SELECT_F(select_msg_len)
SELECT_F(select_msg_id)
SELECT_F(select_msg_id_hex)
SELECT_F(select_msg_body)
SELECT_F(select_msg_body_sdp)
SELECT_F(select_sdp_line)
SELECT_F(select_msg_header)
SELECT_F(select_anyheader)
SELECT_F(select_anyheader_params)
SELECT_F(select_msg_request)
SELECT_F(select_msg_request_method)
SELECT_F(select_msg_request_uri)
SELECT_F(select_msg_request_version)
SELECT_F(select_msg_response)
SELECT_F(select_msg_response_version)
SELECT_F(select_msg_response_status)
SELECT_F(select_msg_response_reason)
SELECT_F(select_version)

SELECT_F(select_any_nameaddr)
SELECT_F(select_nameaddr_name)
SELECT_F(select_nameaddr_uri)
SELECT_F(select_nameaddr_params)
SELECT_F(select_any_params)
	
SELECT_F(select_any_uri)
SELECT_F(select_uri_type)
SELECT_F(select_uri_user)
SELECT_F(select_uri_rn_user)
SELECT_F(select_uri_pwd)
SELECT_F(select_uri_host)
SELECT_F(select_uri_port)
SELECT_F(select_uri_hostport)
SELECT_F(select_uri_params)
SELECT_F(select_uri_proto)

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

SELECT_F(select_src)
SELECT_F(select_dst)
SELECT_F(select_rcv)
SELECT_F(select_ip_port)

SELECT_F(select_call_id)
SELECT_F(select_expires)
SELECT_F(select_max_forwards)
SELECT_F(select_content_type)
SELECT_F(select_content_length)
SELECT_F(select_subject)
SELECT_F(select_organization)
SELECT_F(select_priority)
SELECT_F(select_session_expires)
SELECT_F(select_min_se)
SELECT_F(select_user_agent)
SELECT_F(select_sip_if_match)

SELECT_F(select_sys)
SELECT_F(select_sys_pid)
SELECT_F(select_sys_server_id)
SELECT_F(select_sys_unique)
SELECT_F(select_sys_now)
SELECT_F(select_sys_now_fmt)

SELECT_F(select_branch)
SELECT_F(select_branch_count)
SELECT_F(select_branch_uri)
SELECT_F(select_branch_dst_uri)
SELECT_F(select_branch_uriq)
SELECT_F(select_branch_q)

SELECT_F(select_date)
SELECT_F(select_identity)
SELECT_F(select_identity_info)

SELECT_F(select_cfg_var)
SELECT_F(select_cfg_var1)
SELECT_F(cfg_selected_inst)

static select_row_t select_core[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("ruri"), select_ruri, 0}, /* not the same as request.uri because it is involved by new_uri */
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("request_uri"), select_ruri, 0},
	{ select_ruri, SEL_PARAM_STR, STR_NULL, select_any_uri, NESTED},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("dst_uri"), select_dst_uri, 0},
	{ select_dst_uri, SEL_PARAM_STR, STR_NULL, select_any_uri, NESTED},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("next_hop"), select_next_hop, 0},
	{ select_next_hop, SEL_PARAM_STR, STR_STATIC_INIT("src_ip"), select_next_hop_src_ip, 0},
	{ select_next_hop, SEL_PARAM_STR, STR_NULL, select_any_uri, NESTED},
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
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("refer_to"), select_refer_to, 0},
	{ select_refer_to, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_refer_to_uri, 0},
	{ select_refer_to, SEL_PARAM_STR, STR_STATIC_INIT("tag"), select_refer_to_tag, 0},
	{ select_refer_to, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_refer_to_name, 0},
	{ select_refer_to, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_refer_to_params, CONSUME_NEXT_STR},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("remote_party_id"), select_rpid, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("rpid"), select_rpid, 0},
	{ select_rpid, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_rpid_uri, 0},
	{ select_rpid, SEL_PARAM_STR, STR_STATIC_INIT("tag"), select_rpid_tag, 0},
	{ select_rpid, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_rpid_name, 0},
	{ select_rpid, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_rpid_params, CONSUME_NEXT_STR},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("contact"), select_contact, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("m"), select_contact, 0},
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_contact_uri, 0},
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("name"), select_contact_name, 0}, 
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("q"), select_contact_params_spec, DIVERSION | SEL_PARAM_Q}, 
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("expires"), select_contact_params_spec, DIVERSION | SEL_PARAM_EXPIRES}, 
	{ select_contact, SEL_PARAM_STR, STR_STATIC_INIT("methods"), select_contact_params_spec, DIVERSION | SEL_PARAM_METHODS}, 
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
	{ select_refer_to_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ select_rpid_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ select_contact_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ select_rr_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("type"), select_uri_type, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("user"), select_uri_user, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("rn_user"), select_uri_rn_user, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("pwd"), select_uri_pwd, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("host"), select_uri_host, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("port"), select_uri_port, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_uri_params, CONSUME_NEXT_STR | OPTIONAL | FIXUP_CALL },
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("hostport"), select_uri_hostport, 0},
	{ select_any_uri, SEL_PARAM_STR, STR_STATIC_INIT("transport"), select_uri_proto, 0},

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
	{ select_any_nameaddr, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_nameaddr_params, OPTIONAL | CONSUME_NEXT_STR},
	{ select_nameaddr_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("msg"), select_msg, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("message"), select_msg, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("first_line"), select_msg_first_line, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("flags"), select_msg_flags, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("len"), select_msg_len, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("id"), select_msg_id, 0},
	{ select_msg_id, SEL_PARAM_STR, STR_STATIC_INIT("hex"), select_msg_id_hex, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("type"), select_msg_type, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("header"), select_msg_header, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("h"), select_msg_header, 0},
	{ select_msg_header, SEL_PARAM_STR, STR_NULL, select_anyheader, OPTIONAL | CONSUME_NEXT_INT | FIXUP_CALL},
	{ select_anyheader, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ select_anyheader, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_anyheader_params, NESTED},
	{ select_anyheader_params, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_any_params, CONSUME_NEXT_STR},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("body"), select_msg_body, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("content"), select_msg_body, 0},
	{ select_msg_body, SEL_PARAM_STR, STR_STATIC_INIT("sdp"), select_msg_body_sdp, 0},
	{ select_msg_body_sdp, SEL_PARAM_STR, STR_STATIC_INIT("line"), select_sdp_line, CONSUME_NEXT_STR | FIXUP_CALL},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("request"), select_msg_request, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("req"), select_msg_request, 0},
	{ select_msg_request, SEL_PARAM_STR, STR_STATIC_INIT("method"), select_msg_request_method, 0},
	{ select_msg_request, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_msg_request_uri, 0},
	{ select_msg_request_uri, SEL_PARAM_STR, STR_NULL, select_any_uri, NESTED},
	{ select_msg_request, SEL_PARAM_STR, STR_STATIC_INIT("version"), select_msg_request_version, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("response"), select_msg_response, 0},
	{ select_msg, SEL_PARAM_STR, STR_STATIC_INIT("res"), select_msg_response, 0},
	{ select_msg_response, SEL_PARAM_STR, STR_STATIC_INIT("version"), select_msg_response_version, 0},
	{ select_msg_response, SEL_PARAM_STR, STR_STATIC_INIT("status"), select_msg_response_status, 0},
	{ select_msg_response, SEL_PARAM_STR, STR_STATIC_INIT("code"), select_msg_response_status, 0},
	{ select_msg_response, SEL_PARAM_STR, STR_STATIC_INIT("reason"), select_msg_response_reason, 0},
	/*short aliases*/
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("method"), select_msg_request_method, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("version"), select_version, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("status"), select_msg_response_status, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("code"), select_msg_response_status, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("reason"), select_msg_response_reason, 0},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("proxy_authorization"), select_auth, CONSUME_NEXT_STR | DIVERSION | SEL_AUTH_PROXY | FIXUP_CALL},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("authorization"), select_auth, CONSUME_NEXT_STR | DIVERSION | SEL_AUTH_WWW | FIXUP_CALL}, 
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

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("src"), select_src, SEL_PARAM_EXPECTED},
	{ select_src, SEL_PARAM_STR, STR_STATIC_INIT("ip"), select_ip_port, DIVERSION | SEL_SRC | SEL_IP},
	{ select_src, SEL_PARAM_STR, STR_STATIC_INIT("port"), select_ip_port, DIVERSION | SEL_SRC | SEL_PORT},
	{ select_src, SEL_PARAM_STR, STR_STATIC_INIT("ip_port"), select_ip_port, DIVERSION | SEL_SRC | SEL_IP_PORT},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("dst"), select_dst, SEL_PARAM_EXPECTED},
	{ select_dst, SEL_PARAM_STR, STR_STATIC_INIT("ip"), select_ip_port, DIVERSION | SEL_DST | SEL_IP},
	{ select_dst, SEL_PARAM_STR, STR_STATIC_INIT("port"), select_ip_port, DIVERSION | SEL_DST | SEL_PORT},
	{ select_dst, SEL_PARAM_STR, STR_STATIC_INIT("ip_port"), select_ip_port, DIVERSION | SEL_DST | SEL_IP_PORT},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("received"), select_rcv, SEL_PARAM_EXPECTED},
	{ select_rcv, SEL_PARAM_STR, STR_STATIC_INIT("proto"), select_ip_port, DIVERSION | SEL_RCV | SEL_PROTO},
	{ select_rcv, SEL_PARAM_STR, STR_STATIC_INIT("ip"), select_ip_port, DIVERSION | SEL_RCV | SEL_IP},
	{ select_rcv, SEL_PARAM_STR, STR_STATIC_INIT("port"), select_ip_port, DIVERSION | SEL_RCV | SEL_PORT},
	{ select_rcv, SEL_PARAM_STR, STR_STATIC_INIT("ip_port"), select_ip_port, DIVERSION | SEL_RCV | SEL_IP_PORT},
	{ select_rcv, SEL_PARAM_STR, STR_STATIC_INIT("proto_ip_port"), select_ip_port, DIVERSION | SEL_RCV | SEL_PROTO | SEL_IP_PORT},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("call_id"), select_call_id, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("expires"), select_expires, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("max_forwards"), select_max_forwards, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("content_type"), select_content_type, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("content_length"), select_content_length, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("subject"), select_subject, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("organization"), select_organization, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("priority"), select_priority, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("session_expires"), select_session_expires, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("min_se"), select_min_se, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("user_agent"), select_user_agent, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("sip_if_match"), select_sip_if_match, 0},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("sys"), select_sys, SEL_PARAM_EXPECTED},
	{ select_sys, SEL_PARAM_STR, STR_STATIC_INIT("pid"), select_sys_pid, 0},
	{ select_sys, SEL_PARAM_STR, STR_STATIC_INIT("unique"), select_sys_unique, 0},
	{ select_sys, SEL_PARAM_STR, STR_STATIC_INIT("now"), select_sys_now, 0},
	{ select_sys_now, SEL_PARAM_STR, STR_STATIC_INIT("local"), select_sys_now_fmt, OPTIONAL | CONSUME_NEXT_STR | DIVERSION | SEL_NOW_LOCAL},
	{ select_sys_now, SEL_PARAM_STR, STR_STATIC_INIT("gmt"), select_sys_now_fmt, OPTIONAL | CONSUME_NEXT_STR | DIVERSION | SEL_NOW_GMT},
	{ select_sys_now, SEL_PARAM_STR, STR_STATIC_INIT("utc"), select_sys_now_fmt, OPTIONAL | CONSUME_NEXT_STR | DIVERSION | SEL_NOW_GMT},
	{ select_sys, SEL_PARAM_STR, STR_STATIC_INIT("server_id"), select_sys_server_id, 0},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("branch"), select_branch, SEL_PARAM_EXPECTED},
	{ select_branch, SEL_PARAM_STR, STR_STATIC_INIT("count"), select_branch_count, 0},
	{ select_branch, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_branch_uri, OPTIONAL | CONSUME_NEXT_INT | DIVERSION | SEL_BRANCH_URI },
	{ select_branch, SEL_PARAM_STR, STR_STATIC_INIT("dst_uri"), select_branch_dst_uri, OPTIONAL | CONSUME_NEXT_INT | DIVERSION | SEL_BRANCH_DST_URI},
	{ select_branch_uri, SEL_PARAM_STR, STR_NULL, select_any_uri, NESTED},
	{ select_branch_dst_uri, SEL_PARAM_STR, STR_NULL, select_any_uri, NESTED},
	{ select_branch, SEL_PARAM_STR, STR_STATIC_INIT("uriq"), select_branch_uriq, OPTIONAL | CONSUME_NEXT_INT | DIVERSION | SEL_BRANCH_URI | SEL_BRANCH_Q},
	{ select_branch_uriq, SEL_PARAM_STR, STR_NULL, select_any_nameaddr, NESTED},
	{ select_branch, SEL_PARAM_STR, STR_STATIC_INIT("q"), select_branch_q, OPTIONAL | CONSUME_NEXT_INT | DIVERSION | SEL_BRANCH_Q},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("date"), select_date, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("identity"), select_identity, 0},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("identity_info"), select_identity_info, 0},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("cfg_get"), select_cfg_var1, SEL_PARAM_EXPECTED | CONSUME_NEXT_STR},
	{ select_cfg_var1, SEL_PARAM_STR, STR_NULL, select_cfg_var, FIXUP_CALL },
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("cfg_selected"), cfg_selected_inst, CONSUME_NEXT_STR | FIXUP_CALL },

	{ select_cfg_var, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ select_cfg_var, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ select_cfg_var, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_anyheader_params, NESTED},

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

static select_table_t select_core_table = {select_core, NULL};

#endif // _SELECT_CORE_H
