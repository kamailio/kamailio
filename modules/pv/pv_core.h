/*
 * Copyright (C) 2001-2005 FhG Fokus
 * Copyright (C) 2008 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _PV_CORE_H_
#define _PV_CORE_H_

#include "../../pvar.h"
#include "../../parser/parse_to.h"

int pv_get_msgid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_udp(struct sip_msg *msg, pv_param_t *param, 
		pv_value_t *res);

int pv_get_5060(struct sip_msg *msg, pv_param_t *param, 
		pv_value_t *res);

int pv_get_return_code(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_pid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_method(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_methodid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_msgtype(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_status(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_reason(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_ruri(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_ouri(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_xuri_attr(struct sip_msg *msg, struct sip_uri *parsed_uri,
		pv_param_t *param, pv_value_t *res);

int pv_get_ruri_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_ouri_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_errinfo_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_contact(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_xto_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, struct to_body *xto, int type);

int pv_get_to_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_from_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_cseq(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_msg_buf(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_msg_len(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_flags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_hexflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_flag(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_bflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_hexbflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_bflag(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_sflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_hexsflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_sflag(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_callid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_srcip(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_srcport(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_srcaddr_uri(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_srcaddr_uri_full(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_rcvip(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_rcvport(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_force_sock(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_useragent(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_refer_to(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_diversion(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_rpid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_ppi_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_pai(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_proto(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_dset(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_dsturi(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_dsturi_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_content_type(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_content_length(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_msg_body(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_body_size(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_authattr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_acc_username(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_branch(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_branches(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_avp(struct sip_msg *msg,  pv_param_t *param, pv_value_t *res);

int pv_get_hdr(struct sip_msg *msg,  pv_param_t *param, pv_value_t *res);

int pv_get_scriptvar(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);

int pv_get_true(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_version(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_server_id(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_cnt(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_ruid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_location_ua(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_tcpconn_id(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

/********* end PV get functions *********/

/********* start PV set functions *********/
int pv_set_avp(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_scriptvar(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_dsturi(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_ruri(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_ruri_user(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_ruri_host(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_ruri_port(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_branch(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_force_sock(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_mflags(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_mflag(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_sflags(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_sflag(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_bflags(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_bflag(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_to_uri(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_to_username(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_to_domain(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_to_display(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_from_uri(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_from_username(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_from_domain(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_set_from_display(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

/********* end PV set functions *********/

int pv_parse_scriptvar_name(pv_spec_p sp, str *in);
int pv_parse_scriptvarnull_name(pv_spec_p sp, str *in);

int pv_parse_hdr_name(pv_spec_p sp, str *in);

int pv_parse_cnt_name(pv_spec_p sp, str *in);

int pv_parse_af_name(pv_spec_p sp, str *in);

int pv_get_af(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);

int pv_parse_K_name(pv_spec_p sp, str *in);

int pv_get_K(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);

int pv_parse_flag_param(pv_spec_p sp, str *in);

int pv_parse__s_name(pv_spec_p sp, str *in);

int pv_get__s(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);

int pv_parse_expires_name(pv_spec_p sp, str *in);

int pv_get_expires(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);

#endif

