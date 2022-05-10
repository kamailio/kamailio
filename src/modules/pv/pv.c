/*
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../core/sr_module.h"
#include "../../core/pvar.h"
#include "../../core/pvapi.h"
#include "../../core/lvalue.h"
#include "../../core/mod_fix.h"
#include "../../core/xavp.h"
#include "../../core/kemi.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/strutils.h"


#include "pv_branch.h"
#include "pv_core.h"
#include "pv_stats.h"
#include "pv_shv.h"
#include "pv_time.h"
#include "pv_trans.h"
#include "pv_select.h"
#include "pv_xavp.h"
#include "pv_api.h"

MODULE_VERSION

static tr_export_t mod_trans[] = {
	{ {"s", sizeof("s")-1}, /* string class */
		tr_parse_string },
	{ {"nameaddr", sizeof("nameaddr")-1}, /* nameaddr class */
		tr_parse_nameaddr },
	{ {"uri", sizeof("uri")-1}, /* uri class */
		tr_parse_uri },
	{ {"param", sizeof("param")-1}, /* param class */
		tr_parse_paramlist },
	{ {"tobody", sizeof("tobody")-1}, /* param class */
		tr_parse_tobody },
	{ {"line", sizeof("line")-1}, /* line class */
		tr_parse_line },
	{ {"urialias", sizeof("urialias")-1}, /* uri alias class */
		tr_parse_urialias },

	{ { 0, 0 }, 0 }
};

static pv_export_t mod_pvs[] = {
	{ {"_s", (sizeof("_s")-1)}, PVT_OTHER, pv_get__s, 0,
		pv_parse__s_name, 0, 0, 0 },
	{ {"af", (sizeof("af")-1)}, PVT_OTHER, pv_get_af, 0,
		pv_parse_af_name, 0, 0, 0 },
	{ {"branch", sizeof("branch")-1}, /* branch attributes */
		PVT_CONTEXT, pv_get_branchx, pv_set_branchx,
		pv_parse_branchx_name, pv_parse_index, 0, 0 },
	{ {"sbranch", sizeof("sbranch")-1}, /* static branch attributes */
		PVT_CONTEXT, pv_get_sbranch, pv_set_sbranch,
		pv_parse_branchx_name, 0, 0, 0 },
	{ {"mi", (sizeof("mi")-1)}, /* message id */
		PVT_OTHER, pv_get_msgid, 0,
		0, 0, 0, 0},
	{ {"stat", sizeof("stat")-1}, /* statistics */
		PVT_OTHER, pv_get_stat, 0,
		pv_parse_stat_name, 0, 0, 0 },
	{ {"sel", sizeof("sel")-1}, /* select */
		PVT_OTHER, pv_get_select, 0,
		pv_parse_select_name, 0, 0, 0 },
	{ {"snd", (sizeof("snd")-1)}, PVT_OTHER, pv_get_sndto, 0,
		pv_parse_snd_name, 0, 0, 0 },
	{ {"sndto", (sizeof("sndto")-1)}, PVT_OTHER, pv_get_sndto, 0,
		pv_parse_snd_name, 0, 0, 0 },
	{ {"sndfrom", (sizeof("sndfrom")-1)}, PVT_OTHER, pv_get_sndfrom, 0,
		pv_parse_snd_name, 0, 0, 0 },
	{ {"rcv", (sizeof("rcv")-1)}, PVT_OTHER, pv_get_rcv, pv_set_rcv,
		pv_parse_rcv_name, 0, 0, 0 },
	{ {"xavp", sizeof("xavp")-1}, /* xavp */
		PVT_XAVP, pv_get_xavp, pv_set_xavp,
		pv_parse_xavp_name, 0, 0, 0 },
	{ {"xavu", sizeof("xavu")-1}, /* xavu */
		PVT_XAVU, pv_get_xavu, pv_set_xavu,
		pv_parse_xavu_name, 0, 0, 0 },
	{ {"xavi", sizeof("xavi")-1}, /* xavi */
		PVT_XAVI, pv_get_xavi, pv_set_xavi,
		pv_parse_xavi_name, 0, 0, 0 },
	{{"avp", (sizeof("avp")-1)}, PVT_AVP, pv_get_avp, pv_set_avp,
		pv_parse_avp_name, pv_parse_index, 0, 0},
	{{"hdr", (sizeof("hdr")-1)}, PVT_HDR, pv_get_hdr, 0, pv_parse_hdr_name,
		pv_parse_index, 0, 0},
	{{"hdrc", (sizeof("hdrc")-1)}, PVT_HDRC, pv_get_hdrc, 0, pv_parse_hdr_name,
		0, 0, 0},
	{{"hfl", (sizeof("hfl")-1)}, PVT_HDR, pv_get_hfl, 0, pv_parse_hfl_name,
		pv_parse_index, 0, 0},
	{{"hflc", (sizeof("hflc")-1)}, PVT_HDRC, pv_get_hflc, 0, pv_parse_hfl_name,
		0, 0, 0},
	{{"var", (sizeof("var")-1)}, PVT_SCRIPTVAR, pv_get_scriptvar,
		pv_set_scriptvar, pv_parse_scriptvar_name, 0, 0, 0},
	{{"vz", (sizeof("vz")-1)}, PVT_SCRIPTVAR, pv_get_scriptvar,
		pv_set_scriptvar, pv_parse_scriptvar_name, 0, 0, 0},
	{{"vn", (sizeof("vn")-1)}, PVT_SCRIPTVAR, pv_get_scriptvar,
		pv_set_scriptvar, pv_parse_scriptvarnull_name, 0, 0, 0},
	{{"ai", (sizeof("ai")-1)}, /* */
		PVT_OTHER, pv_get_pai, 0,
		0, pv_parse_index, 0, 0},
	{{"adu", (sizeof("adu")-1)}, /* auth digest uri */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 3},
	{{"ar", (sizeof("ar")-1)}, /* auth realm */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 2},
	{{"au", (sizeof("au")-1)}, /* */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 1},
	{{"ad", (sizeof("ad")-1)}, /* */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 4},
	{{"aU", (sizeof("aU")-1)}, /* */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 5},
	{{"aa", (sizeof("aa")-1)}, /* auth algorithm */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 6},
	{{"adn", (sizeof("adn")-1)}, /* auth nonce */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 7},
	{{"adc", (sizeof("adc")-1)}, /* auth cnonce */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 8},
	{{"adr", (sizeof("adr")-1)}, /* auth response */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 9},
	{{"ado", (sizeof("ado")-1)}, /* auth opaque */
		PVT_OTHER, pv_get_authattr, 0,
		0, 0, pv_init_iname, 10},
	{{"Au", (sizeof("Au")-1)}, /* */
		PVT_OTHER, pv_get_acc_username, 0,
		0, 0, pv_init_iname, 1},
	{{"AU", (sizeof("AU")-1)}, /* */
		PVT_OTHER, pv_get_acc_user, 0,
		0, 0, pv_init_iname, 1},
	{{"bf", (sizeof("bf")-1)}, /* */
		PVT_CONTEXT, pv_get_bflags, pv_set_bflags,
		0, 0, 0, 0},
	{{"bF", (sizeof("bF")-1)}, /* */
		PVT_CONTEXT, pv_get_hexbflags, pv_set_bflags,
		0, 0, 0, 0},
	{{"Bf", (sizeof("Bf")-1)}, /* */
		PVT_CONTEXT, pv_get_bflag, pv_set_bflag,
		pv_parse_flag_param, 0, 0, 0},
	{{"br", (sizeof("br")-1)}, /* */
		PVT_BRANCH, pv_get_branch, pv_set_branch,
		0, 0, 0, 0},
	{{"bR", (sizeof("bR")-1)}, /* */
		PVT_CONTEXT, pv_get_branches, 0,
		0, 0, 0, 0},
	{{"bs", (sizeof("bs")-1)}, /* */
		PVT_OTHER, pv_get_body_size, 0,
		0, 0, 0, 0},
	{{"ci", (sizeof("ci")-1)}, /* */
		PVT_OTHER, pv_get_callid, 0,
		0, 0, 0, 0},
	{{"cl", (sizeof("cl")-1)}, /* */
		PVT_OTHER, pv_get_content_length, 0,
		0, 0, 0, 0},
	{{"cnt", sizeof("cnt")-1},
		PVT_OTHER, pv_get_cnt, 0,
		pv_parse_cnt_name, 0, 0, 0 },
	{{"conid", (sizeof("conid")-1)}, /* */
		PVT_OTHER, pv_get_tcpconn_id, 0,
		0, 0, 0, 0},
	{{"cs", (sizeof("cs")-1)}, /* */
		PVT_OTHER, pv_get_cseq, 0,
		0, 0, 0, 0},
	{{"csb", (sizeof("csb")-1)}, /* */
		PVT_OTHER, pv_get_cseq_body, 0,
		0, 0, 0, 0},
	{{"ct", (sizeof("ct")-1)}, /* */
		PVT_OTHER, pv_get_contact, 0,
		0, 0, 0, 0},
	{{"cT", (sizeof("cT")-1)}, /* */
		PVT_OTHER, pv_get_content_type, 0,
		0, 0, 0, 0},
	{{"dd", (sizeof("dd")-1)}, /* */
		PVT_OTHER, pv_get_dsturi_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"di", (sizeof("di")-1)}, /* */
		PVT_OTHER, pv_get_diversion, 0,
		0, 0, pv_init_iname, 1},
	{{"dir", (sizeof("dir")-1)}, /* */
		PVT_OTHER, pv_get_diversion, 0,
		0, 0, pv_init_iname, 2},
	{{"dip", (sizeof("dis")-1)}, /* */
		PVT_OTHER, pv_get_diversion, 0,
		0, 0, pv_init_iname, 3},
	{{"dic", (sizeof("dic")-1)}, /* */
		PVT_OTHER, pv_get_diversion, 0,
		0, 0, pv_init_iname, 4},
	{{"dp", (sizeof("dp")-1)}, /* */
		PVT_OTHER, pv_get_dsturi_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"dP", (sizeof("dP")-1)}, /* */
		PVT_OTHER, pv_get_dsturi_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"ds", (sizeof("ds")-1)}, /* */
		PVT_CONTEXT, pv_get_dset, 0,
		0, 0, 0, 0},
	{{"du", (sizeof("du")-1)}, /* */
		PVT_DSTURI, pv_get_dsturi, pv_set_dsturi,
		0, 0, 0, 0},
	{{"duri", (sizeof("duri")-1)}, /* */
		PVT_DSTURI, pv_get_dsturi, pv_set_dsturi,
		0, 0, 0, 0},
	{{"err.class", (sizeof("err.class")-1)}, /* */
		PVT_OTHER, pv_get_errinfo_attr, 0,
		0, 0, 0, 0},
	{{"err.level", (sizeof("err.level")-1)}, /* */
		PVT_OTHER, pv_get_errinfo_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"err.info", (sizeof("err.info")-1)}, /* */
		PVT_OTHER, pv_get_errinfo_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"err.rcode", (sizeof("err.rcode")-1)}, /* */
		PVT_OTHER, pv_get_errinfo_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"err.rreason", (sizeof("err.rreason")-1)}, /* */
		PVT_OTHER, pv_get_errinfo_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"fd", (sizeof("fd")-1)}, /* */
		PVT_OTHER, pv_get_from_attr, pv_set_from_domain,
		0, 0, pv_init_iname, 3},
	{{"from.domain", (sizeof("from.domain")-1)}, /* */
		PVT_OTHER, pv_get_from_attr, pv_set_from_domain,
		0, 0, pv_init_iname, 3},
	{{"fn", (sizeof("fn")-1)}, /* */
		PVT_OTHER, pv_get_from_attr, pv_set_from_display,
		0, 0, pv_init_iname, 5},
	{{"fs", (sizeof("fs")-1)}, /* */
		PVT_OTHER, pv_get_force_sock, pv_set_force_sock,
		0, 0, 0, 0},
	{{"fsn", (sizeof("fsn")-1)}, /* */
		PVT_OTHER, pv_get_force_sock_name, pv_set_force_sock_name,
		0, 0, 0, 0},
	{{"fsp", (sizeof("fsp")-1)}, /* */
		PVT_OTHER, pv_get_force_sock_port, 0,
		0, 0, 0, 0},
	{{"ft", (sizeof("ft")-1)}, /* */
		PVT_OTHER, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"fu", (sizeof("fu")-1)}, /* */
		PVT_FROM, pv_get_from_attr, pv_set_from_uri,
		0, 0, pv_init_iname, 1},
	{{"from", (sizeof("from")-1)}, /* */
		PVT_FROM, pv_get_from_attr, pv_set_from_uri,
		0, 0, pv_init_iname, 1},
	{{"fU", (sizeof("fU")-1)}, /* */
		PVT_OTHER, pv_get_from_attr, pv_set_from_username,
		0, 0, pv_init_iname, 2},
	{{"from.user", (sizeof("from.user")-1)}, /* */
		PVT_OTHER, pv_get_from_attr, pv_set_from_username,
		0, 0, pv_init_iname, 2},
	{{"fUl", (sizeof("fUl")-1)}, /* */
		PVT_OTHER, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 6},
	{{"mb", (sizeof("mb")-1)}, /* */
		PVT_OTHER, pv_get_msg_buf, 0,
		0, 0, 0, 0},
	{{"mbu", (sizeof("mbu")-1)}, /* */
		PVT_OTHER, pv_get_msg_buf_updated, 0,
		0, 0, 0, 0},
	{{"mf", (sizeof("mf")-1)}, /* */
		PVT_OTHER, pv_get_flags, pv_set_mflags,
		0, 0, 0, 0},
	{{"mF", (sizeof("mF")-1)}, /* */
		PVT_OTHER, pv_get_hexflags, pv_set_mflags,
		0, 0, 0, 0},
	{{"Mf", (sizeof("mf")-1)}, /* */
		PVT_OTHER, pv_get_flag, pv_set_mflag,
		pv_parse_flag_param, 0, 0, 0},
	{{"ml", (sizeof("ml")-1)}, /* */
		PVT_OTHER, pv_get_msg_len, 0,
		0, 0, 0, 0},
	{{"mt", (sizeof("mt")-1)}, /* */
		PVT_OTHER, pv_get_msgtype, 0,
		0, 0, 0, 0},
	{{"mts", (sizeof("mts")-1)}, /* */
		PVT_OTHER, pv_get_msgtypes, 0,
		0, 0, 0, 0},
	{{"od", (sizeof("od")-1)}, /* */
		PVT_OTHER, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"op", (sizeof("op")-1)}, /* */
		PVT_OTHER, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"oP", (sizeof("oP")-1)}, /* */
		PVT_OTHER, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"ou", (sizeof("ou")-1)}, /* */
		PVT_OURI, pv_get_ouri, 0,
		0, 0, 0, 0},
	{{"ouri", (sizeof("ouri")-1)}, /* */
		PVT_OURI, pv_get_ouri, 0,
		0, 0, 0, 0},
	{{"oU", (sizeof("oU")-1)}, /* */
		PVT_OTHER, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"oUl", (sizeof("oUl")-1)}, /* */
		PVT_OTHER, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 6},
	{{"pd", (sizeof("pd")-1)}, /* */
		PVT_OTHER, pv_get_ppi_attr, 0,
		0, pv_parse_index, pv_init_iname, 3},
	{{"pn", (sizeof("pn")-1)}, /* */
		PVT_OTHER, pv_get_ppi_attr, 0,
		0, pv_parse_index, pv_init_iname, 4},
	{{"pp", (sizeof("pp")-1)}, /* */
		PVT_OTHER, pv_get_pid, 0,
		0, 0, 0, 0},
	{{"pr", (sizeof("pr")-1)}, /* */
		PVT_OTHER, pv_get_proto, 0,
		0, 0, 0, 0},
	{{"prid", (sizeof("prid")-1)}, /* */
		PVT_OTHER, pv_get_protoid, 0,
		0, 0, 0, 0},
	{{"proto", (sizeof("proto")-1)}, /* */
		PVT_OTHER, pv_get_proto, 0,
		0, 0, 0, 0},
	{{"pu", (sizeof("pu")-1)}, /* */
		PVT_OTHER, pv_get_ppi_attr, 0,
		0, pv_parse_index, pv_init_iname, 1},
	{{"pU", (sizeof("pU")-1)}, /* */
		PVT_OTHER, pv_get_ppi_attr, 0,
		0, pv_parse_index, pv_init_iname, 2},
	{{"rb", (sizeof("rb")-1)}, /* */
		PVT_MSG_BODY, pv_get_msg_body, 0,
		0, 0, 0, 0},
	{{"rd", (sizeof("rd")-1)}, /* */
		PVT_RURI_DOMAIN, pv_get_ruri_attr, pv_set_ruri_host,
		0, 0, pv_init_iname, 2},
	{{"ruri.domain", (sizeof("ruri.domain")-1)}, /* */
		PVT_RURI_DOMAIN, pv_get_ruri_attr, pv_set_ruri_host,
		0, 0, pv_init_iname, 2},
	{{"re", (sizeof("re")-1)}, /* */
		PVT_OTHER, pv_get_rpid, 0,
		0, 0, 0, 0},
	{{"rm", (sizeof("rm")-1)}, /* */
		PVT_OTHER, pv_get_method, 0,
		0, 0, 0, 0},
	{{"rmid", (sizeof("rmid")-1)}, /* */
		PVT_OTHER, pv_get_methodid, 0,
		0, 0, 0, 0},
	{{"rp", (sizeof("rp")-1)}, /* */
		PVT_OTHER, pv_get_ruri_attr, pv_set_ruri_port,
		0, 0, pv_init_iname, 3},
	{{"rP", (sizeof("rP")-1)}, /* */
		PVT_OTHER, pv_get_ruri_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"rr", (sizeof("rr")-1)}, /* */
		PVT_OTHER, pv_get_reason, 0,
		0, 0, 0, 0},
	{{"rs", (sizeof("rs")-1)}, /* */
		PVT_OTHER, pv_get_status, 0,
		0, 0, 0, 0},
	{{"rsi", (sizeof("rsi")-1)}, /* */
		PVT_OTHER, pv_get_statusi, 0,
		0, 0, 0, 0},
	{{"rt", (sizeof("rt")-1)}, /* */
		PVT_OTHER, pv_get_refer_to, 0,
		0, 0, 0, 0},
	{{"ru", (sizeof("ru")-1)}, /* */
		PVT_RURI, pv_get_ruri, pv_set_ruri,
		0, 0, 0, 0},
	{{"ruri", (sizeof("ruri")-1)}, /* */
		PVT_RURI, pv_get_ruri, pv_set_ruri,
		0, 0, 0, 0},
	{{"rU", (sizeof("rU")-1)}, /* */
		PVT_RURI_USERNAME, pv_get_ruri_attr, pv_set_ruri_user,
		0, 0, pv_init_iname, 1},
	{{"ruri.user", (sizeof("ruri.user")-1)}, /* */
		PVT_RURI_USERNAME, pv_get_ruri_attr, pv_set_ruri_user,
		0, 0, pv_init_iname, 1},
	{{"rUl", (sizeof("rUl")-1)}, /* */
		PVT_RURI_USERNAME, pv_get_ruri_attr, 0,
		0, 0, pv_init_iname, 6},
	{{"rv", (sizeof("rv")-1)}, /* */
		PVT_OTHER, pv_get_version, 0,
		0, 0, 0, 0},
	{{"rz", (sizeof("rz")-1)}, /* */
		PVT_OTHER, pv_get_ruri_attr, 0,
		0, 0, pv_init_iname, 5},
	{{"Ras", (sizeof("Ras")-1)}, /* */
		PVT_OTHER, pv_get_rcvaddr_socket, 0,
		0, 0, 0, 0},
	{{"Ri", (sizeof("Ri")-1)}, /* */
		PVT_OTHER, pv_get_rcvip, 0,
		0, 0, 0, 0},
	{{"Rp", (sizeof("Rp")-1)}, /* */
		PVT_OTHER, pv_get_rcvport, 0,
		0, 0, 0, 0},
	{{"Ru", (sizeof("Ru")-1)}, /* */
		PVT_OTHER, pv_get_rcvaddr_uri, 0,
		0, 0, 0, 0},
	{{"Rut", (sizeof("Rut")-1)}, /* */
		PVT_OTHER, pv_get_rcvaddr_uri_full, 0,
		0, 0, 0, 0},
	{{"Rn", (sizeof("Rn")-1)}, /* */
		PVT_OTHER, pv_get_rcvsname, 0,
		0, 0, 0, 0},
	{{"RAi", (sizeof("RAi")-1)}, /* */
		PVT_OTHER, pv_get_rcv_advertised_ip, 0,
		0, 0, 0, 0},
	{{"RAp", (sizeof("RAp")-1)}, /* */
		PVT_OTHER, pv_get_rcv_advertised_port, 0,
		0, 0, 0, 0},
	{{"RAu", (sizeof("RAu")-1)}, /* */
		PVT_OTHER, pv_get_rcvadv_uri, 0,
		0, 0, 0, 0},
	{{"RAut", (sizeof("RAut")-1)}, /* */
		PVT_OTHER, pv_get_rcvadv_uri_full, 0,
		0, 0, 0, 0},
	{{"sas", (sizeof("sas")-1)}, /* */
		PVT_OTHER, pv_get_srcaddr_socket, 0,
		0, 0, 0, 0},
	{{"sf", (sizeof("sf")-1)}, /* */
		PVT_OTHER, pv_get_sflags, pv_set_sflags,
		0, 0, 0, 0},
	{{"sF", (sizeof("sF")-1)}, /* */
		PVT_OTHER, pv_get_hexsflags, pv_set_sflags,
		0, 0, 0, 0},
	{{"Sf", (sizeof("sf")-1)}, /* */
		PVT_OTHER, pv_get_sflag, pv_set_sflag,
		pv_parse_flag_param, 0, 0, 0},
	{{"src_ip", (sizeof("src_ip")-1)}, /* */
		PVT_OTHER, pv_get_srcip, 0,
		0, 0, 0, 0},
	{{"si", (sizeof("si")-1)}, /* */
		PVT_OTHER, pv_get_srcip, 0,
		0, 0, 0, 0},
	{{"siz", (sizeof("siz")-1)}, /* */
		PVT_OTHER, pv_get_srcipz, 0,
		0, 0, 0, 0},
	{ {"sid", (sizeof("sid")-1)}, /* server id */
		PVT_OTHER, pv_get_server_id, 0,
		0, 0, 0, 0},
	{{"sp", (sizeof("sp")-1)}, /* */
		PVT_OTHER, pv_get_srcport, 0,
		0, 0, 0, 0},
	{{"su", (sizeof("su")-1)}, /* */
		PVT_OTHER, pv_get_srcaddr_uri, 0,
		0, 0, 0, 0},
	{{"sut", (sizeof("sut")-1)}, /* */
		PVT_OTHER, pv_get_srcaddr_uri_full, 0,
		0, 0, 0, 0},
	{{"td", (sizeof("td")-1)}, /* */
		PVT_OTHER, pv_get_to_attr, pv_set_to_domain,
		0, 0, pv_init_iname, 3},
	{{"to.domain", (sizeof("to.domain")-1)}, /* */
		PVT_OTHER, pv_get_to_attr, pv_set_to_domain,
		0, 0, pv_init_iname, 3},
	{{"tn", (sizeof("tn")-1)}, /* */
		PVT_OTHER, pv_get_to_attr, pv_set_to_display,
		0, 0, pv_init_iname, 5},
	{{"tt", (sizeof("tt")-1)}, /* */
		PVT_OTHER, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"tu", (sizeof("tu")-1)}, /* */
		PVT_TO, pv_get_to_attr, pv_set_to_uri,
		0, 0, pv_init_iname, 1},
	{{"to", (sizeof("to")-1)}, /* */
		PVT_TO, pv_get_to_attr, pv_set_to_uri,
		0, 0, pv_init_iname, 1},
	{{"tU", (sizeof("tU")-1)}, /* */
		PVT_OTHER, pv_get_to_attr, pv_set_to_username,
		0, 0, pv_init_iname, 2},
	{{"to.user", (sizeof("to.user")-1)}, /* */
		PVT_OTHER, pv_get_to_attr, pv_set_to_username,
		0, 0, pv_init_iname, 2},
	{{"tUl", (sizeof("tUl")-1)}, /* */
		PVT_OTHER, pv_get_to_attr, pv_set_to_username,
		0, 0, pv_init_iname, 6},
	{{"true", (sizeof("true")-1)}, /* */
		PVT_OTHER, pv_get_true, 0,
		0, 0, 0, 0},
	{{"Tb", (sizeof("Tb")-1)}, /* */
		PVT_OTHER, pv_get_timeb, 0,
		0, 0, 0, 0},
	{{"Tf", (sizeof("Tf")-1)}, /* */
		PVT_CONTEXT, pv_get_timef, 0,
		0, 0, 0, 0},
	{{"TF", (sizeof("TF")-1)}, /* */
		PVT_OTHER, pv_get_timenowf, 0,
		0, 0, 0, 0},
	{{"Ts", (sizeof("Ts")-1)}, /* */
		PVT_CONTEXT, pv_get_times, 0,
		0, 0, 0, 0},
	{{"TS", (sizeof("TS")-1)}, /* */
		PVT_OTHER, pv_get_timenows, 0,
		0, 0, 0, 0},
	{{"ua", (sizeof("ua")-1)}, /* */
		PVT_OTHER, pv_get_useragent, 0,
		0, 0, 0, 0},
	{{"ruid", (sizeof("ruid")-1)}, /* */
		PVT_OTHER, pv_get_ruid, 0,
		0, 0, 0, 0},
	{{"location_ua", (sizeof("location_ua")-1)}, /* */
		PVT_OTHER, pv_get_location_ua, 0,
		0, 0, 0, 0},

	{ {"shv", (sizeof("shv")-1)}, PVT_OTHER, pv_get_shvar,
		pv_set_shvar, pv_parse_shvar_name, 0, 0, 0},
	{ {"time", (sizeof("time")-1)}, PVT_CONTEXT, pv_get_local_time,
		0, pv_parse_time_name, 0, 0, 0},
	{ {"timef", (sizeof("timef")-1)}, PVT_CONTEXT, pv_get_local_strftime,
		0, pv_parse_strftime_name, 0, 0, 0},
	{ {"utime", (sizeof("utime")-1)}, PVT_CONTEXT, pv_get_utc_time,
		0, pv_parse_time_name, 0, 0, 0},
	{ {"utimef", (sizeof("utimef")-1)}, PVT_CONTEXT, pv_get_utc_strftime,
		0, pv_parse_strftime_name, 0, 0, 0},
	{ {"TV", (sizeof("TV")-1)}, PVT_OTHER, pv_get_timeval,
		0, pv_parse_timeval_name, 0, 0, 0},
	{ {"nh", (sizeof("nh")-1)}, PVT_OTHER, pv_get_nh,
		0, pv_parse_nh_name, 0, 0, 0},
	{ {"version", (sizeof("version")-1)}, PVT_OTHER, pv_get_sr_version,
		0, pv_parse_sr_version_name, 0, 0, 0},
	{ {"K", (sizeof("K")-1)}, PVT_OTHER, pv_get_K, 0,
		pv_parse_K_name, 0, 0, 0 },
	{ {"expires", (sizeof("expires")-1)}, PVT_OTHER, pv_get_expires, 0,
		pv_parse_expires_name, 0, 0, 0 },
	{ {"msg", (sizeof("msg")-1)}, PVT_OTHER, pv_get_msg_attrs, 0,
		pv_parse_msg_attrs_name, 0, 0, 0 },
	{ {"ksr", (sizeof("ksr")-1)}, PVT_OTHER, pv_get_ksr_attrs, 0,
		pv_parse_ksr_attrs_name, 0, 0, 0 },
	{{"rpl", (sizeof("rpl")-1)}, PVT_OTHER, pv_get_rpl_attrs, 0,
		pv_parse_rpl_attrs_name, 0, 0, 0},
	{{"ccp", (sizeof("ccp")-1)}, PVT_OTHER, pv_get_ccp_attrs, pv_set_ccp_attrs,
		pv_parse_ccp_attrs_name, 0, 0, 0},
	{{"via0", (sizeof("via0")-1)}, PVT_OTHER, pv_get_via0, 0,
		pv_parse_via_name, 0, 0, 0},
	{{"via1", (sizeof("via1")-1)}, PVT_OTHER, pv_get_via1, 0,
		pv_parse_via_name, 0, 0, 0},
	{{"viaZ", (sizeof("viaZ")-1)}, PVT_OTHER, pv_get_viaZ, 0,
		pv_parse_via_name, 0, 0, 0},
	{{"msgbuf", (sizeof("msgbuf")-1)}, PVT_OTHER, pv_get_msgbuf, pv_set_msgbuf,
		pv_parse_msgbuf_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static int add_avp_aliases(modparam_t type, void* val);

static param_export_t params[]={
	{"shvset",              PARAM_STRING|USE_FUNC_PARAM, (void*)param_set_shvar },
	{"varset",              PARAM_STRING|USE_FUNC_PARAM, (void*)param_set_var },
	{"avp_aliases",         PARAM_STRING|USE_FUNC_PARAM, (void*)add_avp_aliases },
	{0,0,0}
};

static int mod_init(void);
static void mod_destroy(void);
static int pv_isset(struct sip_msg* msg, char* pvid, char *foo);
static int pv_unset(struct sip_msg* msg, char* pvid, char *foo);
static int is_int(struct sip_msg* msg, char* pvar, char* s2);
static int pv_typeof(sip_msg_t *msg, char *pv, char *t);
static int pv_not_empty(sip_msg_t *msg, char *pv, char *s2);
static int w_xavp_copy(sip_msg_t *msg, char *src_name, char *src_idx, char *dst_name);
static int w_xavp_copy_dst(sip_msg_t *msg, char *src_name, char *src_idx,
		char *dst_name, char *dst_idx);
static int w_xavp_params_explode(sip_msg_t *msg, char *pparams, char *pxname);
static int w_xavp_params_implode(sip_msg_t *msg, char *pxname, char *pvname);
static int w_xavu_params_explode(sip_msg_t *msg, char *pparams, char *pxname);
static int w_xavu_params_implode(sip_msg_t *msg, char *pxname, char *pvname);
static int w_xavp_slist_explode(sip_msg_t *msg, char *pslist, char *psep,
		char *pmode, char *pxname);
static int w_xavp_child_seti(sip_msg_t *msg, char *prname, char *pcname,
		char *pval);
static int w_xavp_child_sets(sip_msg_t *msg, char *prname, char *pcname,
		char *pval);
static int w_xavp_rm(sip_msg_t *msg, char *prname, char *p2);
static int w_xavp_child_rm(sip_msg_t *msg, char *prname, char *pcname);
static int w_sbranch_set_ruri(sip_msg_t *msg, char p1, char *p2);
static int w_sbranch_append(sip_msg_t *msg, char p1, char *p2);
static int w_sbranch_reset(sip_msg_t *msg, char p1, char *p2);
static int w_var_to_xavp(sip_msg_t *msg, char *p1, char *p2);
static int w_xavp_to_var(sip_msg_t *msg, char *p1);

static int w_xavi_child_seti(sip_msg_t *msg, char *prname, char *pcname,
		char *pval);
static int w_xavi_child_sets(sip_msg_t *msg, char *prname, char *pcname,
		char *pval);
static int w_xavi_rm(sip_msg_t *msg, char *prname, char *p2);
static int w_xavi_child_rm(sip_msg_t *msg, char *prname, char *pcname);

static int w_xavp_lshift(sip_msg_t *msg, char *pxname, char *pidx);

int pv_xavp_copy_fixup(void** param, int param_no);
int pv_evalx_fixup(void** param, int param_no);
int w_pv_evalx(struct sip_msg *msg, char *dst, str *fmt);

static int fixup_xavp_child_seti(void** param, int param_no);
static int fixup_free_xavp_child_seti(void** param, int param_no);

static int pv_init_rpc(void);
int pv_register_api(pv_api_t*);

static cmd_export_t cmds[]={
	{"pv_isset",  (cmd_function)pv_isset,  1, fixup_pvar_null, 0,
		ANY_ROUTE },
	{"pv_unset",  (cmd_function)pv_unset,  1, fixup_pvar_null, 0,
		ANY_ROUTE },
	{"pv_xavp_print",  (cmd_function)pv_xavp_print,  0, 0, 0,
		ANY_ROUTE },
	{"pv_xavu_print",  (cmd_function)pv_xavu_print,  0, 0, 0,
		ANY_ROUTE },
	{"pv_xavi_print",  (cmd_function)pv_xavi_print,  0, 0, 0,
		ANY_ROUTE },
	{"pv_var_to_xavp",  (cmd_function)w_var_to_xavp, 2, fixup_spve_spve,
		fixup_free_spve_spve, ANY_ROUTE },
	{"pv_xavp_to_var",  (cmd_function)w_xavp_to_var, 1, fixup_spve_null,
		fixup_free_spve_null, ANY_ROUTE },
	{"is_int", (cmd_function)is_int, 1, fixup_pvar_null, fixup_free_pvar_null,
		ANY_ROUTE},
	{"typeof", (cmd_function)pv_typeof,       2, fixup_pvar_none,
		fixup_free_pvar_none,
		ANY_ROUTE},
	{"not_empty", (cmd_function)pv_not_empty, 1, fixup_pvar_null,
		fixup_free_pvar_null,
		ANY_ROUTE},
	{"xavp_copy", (cmd_function)w_xavp_copy, 3, pv_xavp_copy_fixup, 0,
		ANY_ROUTE},
	{"xavp_copy", (cmd_function)w_xavp_copy_dst, 4, pv_xavp_copy_fixup, 0,
		ANY_ROUTE},
	{"xavp_slist_explode", (cmd_function)w_xavp_slist_explode,
		4, fixup_spve_all, fixup_free_spve_all,
		ANY_ROUTE},
	{"xavp_params_explode", (cmd_function)w_xavp_params_explode,
		2, fixup_spve_spve, fixup_free_spve_spve,
		ANY_ROUTE},
	{"xavp_params_implode", (cmd_function)w_xavp_params_implode,
		2, fixup_spve_str, fixup_free_spve_str,
		ANY_ROUTE},
	{"xavu_params_explode", (cmd_function)w_xavu_params_explode,
		2, fixup_spve_spve, fixup_free_spve_spve,
		ANY_ROUTE},
	{"xavu_params_implode", (cmd_function)w_xavu_params_implode,
		2, fixup_spve_str, fixup_free_spve_str,
		ANY_ROUTE},
	{"xavp_child_seti", (cmd_function)w_xavp_child_seti,
		3, fixup_xavp_child_seti, fixup_free_xavp_child_seti,
		ANY_ROUTE},
	{"xavp_child_sets", (cmd_function)w_xavp_child_sets,
		3, fixup_spve_all, fixup_free_spve_all,
		ANY_ROUTE},
	{"xavp_rm", (cmd_function)w_xavp_rm,
		1, fixup_spve_null, fixup_free_spve_null,
		ANY_ROUTE},
	{"xavp_child_rm", (cmd_function)w_xavp_child_rm,
		2, fixup_spve_spve, fixup_free_spve_spve,
		ANY_ROUTE},
	{"xavi_child_seti", (cmd_function)w_xavi_child_seti,
		3, fixup_xavp_child_seti, fixup_free_xavp_child_seti,
		ANY_ROUTE},
	{"xavi_child_sets", (cmd_function)w_xavi_child_sets,
		3, fixup_spve_all, fixup_free_spve_all,
		ANY_ROUTE},
	{"xavi_rm", (cmd_function)w_xavi_rm,
		1, fixup_spve_null, fixup_free_spve_null,
		ANY_ROUTE},
	{"xavi_child_rm", (cmd_function)w_xavi_child_rm,
		2, fixup_spve_spve, fixup_free_spve_spve,
		ANY_ROUTE},
	{"xavp_lshift", (cmd_function)w_xavp_lshift,
		2, fixup_spve_igp, fixup_free_spve_igp,
		ANY_ROUTE},
	{"sbranch_set_ruri",  (cmd_function)w_sbranch_set_ruri,  0, 0, 0,
		ANY_ROUTE },
	{"sbranch_append",    (cmd_function)w_sbranch_append,    0, 0, 0,
		ANY_ROUTE },
	{"sbranch_reset",     (cmd_function)w_sbranch_reset,     0, 0, 0,
		ANY_ROUTE },
	{"pv_evalx",          (cmd_function)w_pv_evalx,    2, pv_evalx_fixup,
		0, ANY_ROUTE },
	/* API exports */
	{"pv_register_api",   (cmd_function)pv_register_api,     NO_SCRIPT, 0, 0},
	{0,0,0,0,0,0}
};



/** module exports */
struct module_exports exports= {
	"pv",            /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pv exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	0,               /* per-child init function */
	mod_destroy      /* module destroy function */
};

static int mod_init(void)
{
	if(pv_init_rpc()!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	if(pv_ccp_ctx_init()!=0) {
		LM_ERR("failed to initialize var ccp context\n");
		return -1;
	}
	pv_init_sbranch();

	return 0;
}

static void mod_destroy(void)
{
	shvar_destroy_locks();
	destroy_shvars();
}

static int pv_isset(struct sip_msg* msg, char* pvid, char *foo)
{
	pv_spec_t *sp;
	pv_value_t value;
	int ret;

	sp = (pv_spec_t*)pvid;
	if(pv_get_spec_value(msg, sp, &value)!=0)
		return -1;
	ret =1;
	if(value.flags & (PV_VAL_EMPTY|PV_VAL_NULL))
		ret = -1;
	pv_value_destroy(&value);
	return ret;
}

static int pv_unset(struct sip_msg* msg, char* pvid, char *foo)
{
	pv_spec_t *sp;

	sp = (pv_spec_t*)pvid;
	if(pv_set_spec_value(msg, sp, 0, NULL)<0) {
		LM_ERR("faile to unset variable\n");
		return -1;
	}

	return 1;
}

static int add_avp_aliases(modparam_t type, void* val)
{
	if (val!=0 && ((char*)val)[0]!=0)
	{
		if ( add_avp_galias_str((char*)val)!=0 )
			return -1;
	}

	return 0;
}

/**
 * match the type of the variable value
 */
static int pv_typeof(sip_msg_t *msg, char *pv, char *t)
{
	pv_value_t val;

	if (pv==NULL || t==NULL)
		return -1;
	if(pv_get_spec_value(msg, (pv_spec_t*)pv, &val) != 0)
		return -1;

	switch(t[0]) {
		case 'i':
		case 'I':
			if(val.flags & PV_TYPE_INT)
				return 1;
			return -1;
		case 'n':
		case 'N':
			if(val.flags & PV_VAL_NULL)
				return 1;
			return -1;
		case 's':
		case 'S':
			if(!(val.flags & PV_VAL_STR))
				return -1;
			if(val.flags & PV_TYPE_INT)
				return -1;
			return 1;
		default:
			return -1;
	}
}

/**
 * return true if the type is string and value not empty
 */
static int pv_not_empty(sip_msg_t *msg, char *pv, char *s2)
{
	pv_value_t val;

	if (pv==NULL)
		return -1;

	if(pv_get_spec_value(msg, (pv_spec_t*)pv, &val) != 0)
		return -1;

	if(!(val.flags & PV_VAL_STR))
		return -1;
	if(val.flags & PV_TYPE_INT)
		return -1;

	if(val.rs.len>0)
		return 1;

	return -1;
}

/**
 * Copyright (C) 2011 Juha Heinanen
 *
 * Checks if pvar argument contains int value
 */
static int is_int(struct sip_msg* msg, char* pvar, char* s2)
{
	pv_spec_t *pvar_sp;
	pv_value_t pv_val;

	pvar_sp = (pv_spec_t *)pvar;

	if (pvar_sp && (pv_get_spec_value(msg, pvar_sp, &pv_val) == 0)) {
		return (pv_val.flags & PV_VAL_INT)?1:-1;
	}

	return -1;
}

/**
 * script variable to xavp
 */
static int w_var_to_xavp(sip_msg_t *msg, char *s1, char *s2)
{
	str xname = STR_NULL;
	str varname = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)s1, &varname)<0) {
		LM_ERR("failed to get the var name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)s2, &xname)<0) {
		LM_ERR("failed to get the xavp name\n");
		return -1;
	}

	return pv_var_to_xavp(&varname, &xname);
}

static int ki_var_to_xavp(sip_msg_t *msg, str *varname, str *xname)
{
	return pv_var_to_xavp(varname, xname);
}

/**
 * xavp to script variable
 */
static int w_xavp_to_var(sip_msg_t *msg, char *s1)
{
	str xname = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)s1, &xname)<0) {
		LM_ERR("failed to get the xavp name\n");
		return -1;
	}

	return pv_xavp_to_var(&xname);
}

static int ki_xavp_to_var(sip_msg_t *msg, str *xname)
{
	return pv_xavp_to_var(xname);
}

static int ki_xavp_print(sip_msg_t* msg)
{
	xavp_print_list(NULL);
	return 1;
}

/**
 *
 */
static int ki_xavp_lshift(sip_msg_t *msg, str *xname, int idx)
{
	int ret;

	ret = xavp_lshift(xname, NULL, idx);

	return (ret==0)?1:ret;
}

/**
 *
 */
static int w_xavp_lshift(sip_msg_t *msg, char *pxname, char *pidx)
{
	str xname = STR_NULL;
	int idx = 0;

	if(fixup_get_svalue(msg, (gparam_t*)pxname, &xname)<0) {
		LM_ERR("failed to get the xavp name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pidx, &idx)<0) {
		LM_ERR("failed to get the xavp index\n");
		return -1;
	}

	return ki_xavp_lshift(msg, &xname, idx);
}

static int ki_xavu_print(sip_msg_t* msg)
{
	xavu_print_list(NULL);
	return 1;
}

static int ki_xavi_print(sip_msg_t* msg)
{
	xavi_print_list(NULL);
	return 1;
}

/**
 *
 */
static int ki_xavp_copy_dst_mode(str *src_name, int src_idx, str *dst_name,
		int dst_idx, int dimode)
{
	sr_xavp_t *src_xavp = NULL;
	sr_xavp_t *dst_xavp = NULL;
	sr_xavp_t *new_xavp = NULL;
	sr_xavp_t *prev_xavp = NULL;

	src_xavp = xavp_get_by_index(src_name, src_idx, NULL);
	if(!src_xavp) {
		LM_ERR("missing can not find source xavp [%.*s]\n",
				src_name->len, src_name->s);
		return -1;
	}

	LM_DBG("dst_name xavp [%.*s]\n", dst_name->len, dst_name->s);
	new_xavp = xavp_clone_level_nodata_with_new_name(src_xavp, dst_name);
	if (!new_xavp) {
		LM_ERR("error cloning xavp\n");
		return -1;
	}

	if (dimode) {
		dst_xavp = xavp_get_by_index(dst_name, dst_idx, NULL);
		if(!dst_xavp) {
			LM_ERR("xavp_copy: missing can not find destination xavp [%.*s]\n",
					dst_name->len, dst_name->s);
			xavp_destroy_list(&new_xavp);
			return -1;
		}

		LM_DBG("xavp_copy(replace): $xavp(%.*s[%d]) >> $xavp(%.*s[%d])\n",
				src_name->len, src_name->s, src_idx,
				dst_name->len, dst_name->s, dst_idx);
		if(dst_idx == 0) {
			if(xavp_add(new_xavp, NULL)<0) {
				LM_ERR("error adding new xavp\n");
				xavp_destroy_list(&new_xavp);
				return -1;
			}
		} else {
			prev_xavp = xavp_get_by_index(dst_name, dst_idx-1, NULL);
			if(!prev_xavp) {
				LM_ERR("error inserting xavp, parent not found $xavp(%.*s[%d])\n",
						dst_name->len, dst_name->s, dst_idx);
				xavp_destroy_list(&new_xavp);
				return -1;
			}
			xavp_add_after(new_xavp, prev_xavp);
		}
		if(xavp_rm(dst_xavp, NULL)<0) {
			LM_ERR("can not remove the exiting index $xavp(%.*s[%d])\n",
					dst_name->len, dst_name->s, dst_idx);
			return -1;
		}
	} else {
		/* check if destination exists,
		 * if it does we will append, similar to XAVP assigment */
		dst_xavp = xavp_get(dst_name, NULL);
		if (!dst_xavp) {
			LM_DBG("xavp_copy(new): $xavp(%.*s[%d]) >> $xavp(%.*s)\n",
					src_name->len, src_name->s, src_idx, dst_name->len,
					dst_name->s);
			if(xavp_add(new_xavp, NULL)<0) {
				LM_ERR("error adding new xavp\n");
				xavp_destroy_list(&dst_xavp);
				return -1;
			}
		} else {
			LM_DBG("xavp_copy(append): $xavp(%.*s[%d]) >> $xavp(%.*s)\n",
					src_name->len, src_name->s, src_idx,
					dst_name->len, dst_name->s);
			if(xavp_add_last(new_xavp, &dst_xavp)<0) {
				LM_ERR("error appending new xavp\n");
				xavp_destroy_list(&dst_xavp);
				return -1;
			}
		}
	}
	return 1;
}

/**
 *
 */
static int ki_xavp_copy(sip_msg_t *msg, str *src_name, int src_idx, str *dst_name)
{
	return ki_xavp_copy_dst_mode(src_name, src_idx, dst_name, 0, 0);
}

/**
 *
 */
static int ki_xavp_copy_dst(sip_msg_t *msg, str *src_name, int src_idx,
		str *dst_name, int dst_idx)
{
	return ki_xavp_copy_dst_mode(src_name, src_idx, dst_name, dst_idx, 0);
}

/**
 *
 */
static int w_xavp_copy(sip_msg_t *msg, char *_src_name, char *_src_idx, char *_dst_name)
{
	return w_xavp_copy_dst(msg, _src_name, _src_idx, _dst_name, NULL);
}

/**
 *
 */
static int w_xavp_copy_dst(sip_msg_t *msg, char *_src_name, char *_src_idx,
		char *_dst_name, char *_dst_idx)
{
	str src_name;
	int src_idx;
	str dst_name;
	int dst_idx;
	int dimode;

	if(get_str_fparam(&src_name, msg, (gparam_p)_src_name) != 0) {
		LM_ERR("xavp_copy: missing source\n");
		return -1;
	}
	if(get_str_fparam(&dst_name, msg, (gparam_p)_dst_name) != 0) {
		LM_ERR("xavp_copy: missing destination\n");
		return -1;
	}
	if(get_int_fparam(&src_idx, msg, (gparam_t*)_src_idx)<0) {
		LM_ERR("failed to get the src_idx value\n");
		return -1;
	}
	dst_idx = 0;
	if (_dst_idx) {
		if(get_int_fparam(&dst_idx, msg, (gparam_t*)_dst_idx)<0) {
			LM_ERR("failed to get the dst_idx value\n");
			return -1;
		}
		dimode = 1;
	} else {
		dimode = 0;
	}
	return ki_xavp_copy_dst_mode(&src_name, src_idx, &dst_name, dst_idx, dimode);
}

/**
 *
 */
static int w_xavp_params_explode(sip_msg_t *msg, char *pparams, char *pxname)
{
	str sparams;
	str sxname;

	if(fixup_get_svalue(msg, (gparam_t*)pparams, &sparams)!=0) {
		LM_ERR("cannot get the params\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pxname, &sxname)!=0) {
		LM_ERR("cannot get the xavp name\n");
		return -1;
	}

	if(xavp_params_explode(&sparams, &sxname)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_xavp_params_explode(sip_msg_t *msg, str *sparams, str *sxname)
{
	if(xavp_params_explode(sparams, sxname)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int w_xavu_params_explode(sip_msg_t *msg, char *pparams, char *pxname)
{
	str sparams;
	str sxname;

	if(fixup_get_svalue(msg, (gparam_t*)pparams, &sparams)!=0) {
		LM_ERR("cannot get the params\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pxname, &sxname)!=0) {
		LM_ERR("cannot get the xavp name\n");
		return -1;
	}

	if(xavu_params_explode(&sparams, &sxname)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_xavu_params_explode(sip_msg_t *msg, str *sparams, str *sxname)
{
	if(xavu_params_explode(sparams, sxname)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_xavp_params_implode(sip_msg_t *msg, str *sxname, str *svname)
{
	pv_spec_t *vspec=NULL;
	pv_value_t val;

	if(sxname==NULL || sxname->s==NULL || sxname->len<=0) {
		LM_ERR("invalid xavp name\n");
		return -1;
	}
	if(svname==NULL || svname->s==NULL || svname->len<=0) {
		LM_ERR("invalid output var name\n");
		return -1;
	}

	vspec = pv_cache_get(svname);
	if(vspec==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", svname->len, svname->s);
		return -1;
	}
	if(vspec->setf==NULL) {
		LM_ERR("read only output variable [%.*s]\n", svname->len, svname->s);
		return -1;
	}

	val.rs.s = pv_get_buffer();
	val.rs.len = xavp_serialize_fields(sxname, val.rs.s, pv_get_buffer_size());
	if(val.rs.len<=0) {
		return -1;
	}

	val.flags = PV_VAL_STR;
	if(vspec->setf(msg, &vspec->pvp, EQ_T, &val)<0) {
		LM_ERR("setting PV failed [%.*s]\n", svname->len, svname->s);
		return -1;
	}

	return 1;
}

/**
 *
 */
static int w_xavp_params_implode(sip_msg_t *msg, char *pxname, char *pvname)
{
	str sxname;

	if(fixup_get_svalue(msg, (gparam_t*)pxname, &sxname)!=0) {
		LM_ERR("cannot get the xavp name\n");
		return -1;
	}

	return ki_xavp_params_implode(msg, &sxname, (str*)pvname);
}

/**
 *
 */
static int ki_xavu_params_implode(sip_msg_t *msg, str *sxname, str *svname)
{
	pv_spec_t *vspec=NULL;
	pv_value_t val;

	if(sxname==NULL || sxname->s==NULL || sxname->len<=0) {
		LM_ERR("invalid xavp name\n");
		return -1;
	}
	if(svname==NULL || svname->s==NULL || svname->len<=0) {
		LM_ERR("invalid output var name\n");
		return -1;
	}

	vspec = pv_cache_get(svname);
	if(vspec==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", svname->len, svname->s);
		return -1;
	}
	if(vspec->setf==NULL) {
		LM_ERR("read only output variable [%.*s]\n", svname->len, svname->s);
		return -1;
	}

	val.rs.s = pv_get_buffer();
	val.rs.len = xavu_serialize_fields(sxname, val.rs.s, pv_get_buffer_size());
	if(val.rs.len<=0) {
		return -1;
	}

	val.flags = PV_VAL_STR;
	if(vspec->setf(msg, &vspec->pvp, EQ_T, &val)<0) {
		LM_ERR("setting PV failed [%.*s]\n", svname->len, svname->s);
		return -1;
	}

	return 1;
}

/**
 *
 */
static int w_xavu_params_implode(sip_msg_t *msg, char *pxname, char *pvname)
{
	str sxname;

	if(fixup_get_svalue(msg, (gparam_t*)pxname, &sxname)!=0) {
		LM_ERR("cannot get the xavp name\n");
		return -1;
	}

	return ki_xavu_params_implode(msg, &sxname, (str*)pvname);
}

/**
 *
 */
static int w_xavp_slist_explode(sip_msg_t *msg, char *pslist, char *psep,
		char *pmode, char *pxname)
{
	str slist;
	str sep;
	str smode;
	str sxname;

	if(fixup_get_svalue(msg, (gparam_t*)pslist, &slist)!=0) {
		LM_ERR("cannot get the params\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)psep, &sep)!=0) {
		LM_ERR("cannot get the params\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pmode, &smode)!=0) {
		LM_ERR("cannot get the params\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pxname, &sxname)!=0) {
		LM_ERR("cannot get the xavp name\n");
		return -1;
	}

	if(xavp_slist_explode(&slist, &sep, &smode, &sxname)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_xavp_slist_explode(sip_msg_t *msg, str *slist, str *sep, str *mode,
		str *sxname)
{
	if(xavp_slist_explode(slist, sep, mode, sxname)<0)
		return -1;

	return 1;
}


/**
 *
 */
static int ki_xav_seti(sip_msg_t *msg, str *rname, int ival, int _case)
{
	sr_xavp_t *xavp = NULL;
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_INT;
	xval.v.i = ival;

	if(_case) {
		xavp = xavi_add_value(rname, &xval, NULL);
	} else {
		xavp = xavp_add_value(rname, &xval, NULL);
	}
	return (xavp!=NULL)?1:-1;
}

static int ki_xavp_seti(sip_msg_t *msg, str *rname, int ival)
{
	return ki_xav_seti(msg, rname, ival, 0);
}

static int ki_xavi_seti(sip_msg_t *msg, str *rname, int ival)
{
	return ki_xav_seti(msg, rname, ival, 1);
}

/**
 *
 */
static int ki_xav_sets(sip_msg_t *msg, str *rname, str *sval, int _case)
{
	sr_xavp_t *xavp = NULL;
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_STR;
	xval.v.s = *sval;

	if(_case) {
		xavp = xavi_add_value(rname, &xval, NULL);
	} else {
		xavp = xavp_add_value(rname, &xval, NULL);
	}
	return (xavp!=NULL)?1:-1;
}

static int ki_xavp_sets(sip_msg_t *msg, str *rname, str *sval)
{
	return ki_xav_sets(msg, rname, sval, 0);
}

static int ki_xavi_sets(sip_msg_t *msg, str *rname, str *sval)
{
	return ki_xav_sets(msg, rname, sval, 1);
}

/**
 *
 */
static int ki_xav_child_seti(sip_msg_t *msg, str *rname, str *cname,
		int ival, int _case)
{
	int ret;
	if(_case) {
		ret = xavi_set_child_ival(rname, cname, ival);
	} else {
		ret = xavp_set_child_ival(rname, cname, ival);
	}
	return (ret<0)?ret:1;
}

static int ki_xavp_child_seti(sip_msg_t *msg, str *rname, str *cname,
		int ival)
{
	return ki_xav_child_seti(msg, rname, cname, ival, 0);
}

static int ki_xavi_child_seti(sip_msg_t *msg, str *rname, str *cname,
		int ival)
{
	return ki_xav_child_seti(msg, rname, cname, ival, 1);
}

/**
 *
 */
static int w_xav_child_seti(sip_msg_t *msg, char *prname, char *pcname,
		char *pval, int _case)
{
	str rname = STR_NULL;
	str cname = STR_NULL;
	int ival = 0;

	if(fixup_get_svalue(msg, (gparam_t*)prname, &rname)<0) {
		LM_ERR("failed to get root xavp name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pcname, &cname)<0) {
		LM_ERR("failed to get child xavp name\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t*)pval, &ival)<0) {
		LM_ERR("failed to get the value\n");
		return -1;
	}

	return ki_xav_child_seti(msg, &rname, &cname, ival, _case);
}

static int w_xavp_child_seti(sip_msg_t *msg, char *prname, char *pcname,
		char *pval)
{
	return w_xav_child_seti(msg, prname, pcname, pval, 0);
}

static int w_xavi_child_seti(sip_msg_t *msg, char *prname, char *pcname,
		char *pval)
{
	return w_xav_child_seti(msg, prname, pcname, pval, 1);
}

/**
 *
 */
static int ki_xav_child_sets(sip_msg_t *msg, str *rname, str *cname,
		str *sval, int _case)
{
	int ret;
	if(_case) {
		ret = xavi_set_child_sval(rname, cname, sval);
	} else {
		ret = xavp_set_child_sval(rname, cname, sval);
	}
	return (ret<0)?ret:1;
}

static int ki_xavp_child_sets(sip_msg_t *msg, str *rname, str *cname,
		str *sval)
{
	return ki_xav_child_sets(msg, rname, cname, sval, 0);
}

static int ki_xavi_child_sets(sip_msg_t *msg, str *rname, str *cname,
		str *sval)
{
	return ki_xav_child_sets(msg, rname, cname, sval, 1);
}

/**
 *
 */
static int w_xav_child_sets(sip_msg_t *msg, char *prname, char *pcname,
		char *pval, int _case)
{
	str rname;
	str cname;
	str sval;

	if(fixup_get_svalue(msg, (gparam_t*)prname, &rname)<0) {
		LM_ERR("failed to get root xavp name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pcname, &cname)<0) {
		LM_ERR("failed to get child xavp name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pval, &sval)<0) {
		LM_ERR("failed to get the value\n");
		return -1;
	}

	return ki_xav_child_sets(msg, &rname, &cname, &sval, _case);
}

static int w_xavp_child_sets(sip_msg_t *msg, char *prname, char *pcname,
		char *pval) {
	return w_xav_child_sets(msg, prname, pcname, pval, 0);
}

static int w_xavi_child_sets(sip_msg_t *msg, char *prname, char *pcname,
		char *pval) {
	return w_xav_child_sets(msg, prname, pcname, pval, 1);
}

/**
 *
 */
static int fixup_xavp_child_seti(void** param, int param_no)
{
	if(param_no==1 || param_no==2)
		return fixup_spve_all(param, param_no);
	if(param_no==3)
		return fixup_igp_all(param, param_no);
	return 0;
}

/**
 *
 */
static int fixup_free_xavp_child_seti(void** param, int param_no)
{
	if(param_no==1 || param_no==2)
		return fixup_free_spve_all(param, param_no);
	if(param_no==3)
		return fixup_free_igp_all(param, param_no);

	return 0;
}

/**
 *
 */
static int ki_xav_rm(sip_msg_t *msg, str *rname, int _case)
{
	int ret;
	if(_case) {
		ret = xavi_rm_by_index(rname, 0, NULL);
	} else {
		ret = xavp_rm_by_index(rname, 0, NULL);
	}

	return (ret==0)?1:ret;
}

static int ki_xavp_rm(sip_msg_t *msg, str *rname)
{
	return ki_xav_rm(msg, rname, 0);
}

static int ki_xavi_rm(sip_msg_t *msg, str *rname)
{
	return ki_xav_rm(msg, rname, 1);
}

/**
 *
 */
static int w_xav_rm(sip_msg_t *msg, char *prname, char *p2, int _case)
{
	str rname;

	if(fixup_get_svalue(msg, (gparam_t*)prname, &rname)<0) {
		LM_ERR("failed to get root xavp name\n");
		return -1;
	}

	return ki_xav_rm(msg, &rname, _case);
}

static int w_xavp_rm(sip_msg_t *msg, char *prname, char *p2) {
	return w_xav_rm(msg, prname, p2, 0);
}

static int w_xavi_rm(sip_msg_t *msg, char *prname, char *p2) {
	return w_xav_rm(msg, prname, p2, 1);
}

/**
 *
 */
static int ki_xav_child_rm(sip_msg_t *msg, str *rname, str *cname, int _case)
{
	int ret;
	if(_case) {
		ret = xavi_rm_child_by_index(rname, cname, 0);
	} else {
		ret = xavp_rm_child_by_index(rname, cname, 0);
	}
	return (ret==0)?1:ret;
}

static int ki_xavp_child_rm(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_rm(msg, rname, cname, 0);
}

static int ki_xavi_child_rm(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_rm(msg, rname, cname, 1);
}

/**
 *
 */
static int w_xav_child_rm(sip_msg_t *msg, char *prname, char *pcname, int _case)
{
	str rname;
	str cname;

	if(fixup_get_svalue(msg, (gparam_t*)prname, &rname)<0) {
		LM_ERR("failed to get root xavp name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pcname, &cname)<0) {
		LM_ERR("failed to get child xavp name\n");
		return -1;
	}

	return ki_xav_child_rm(msg, &rname, &cname, _case);
}

static int w_xavp_child_rm(sip_msg_t *msg, char *prname, char *pcname) {
	return w_xav_child_rm(msg, prname, pcname, 0);
}

static int w_xavi_child_rm(sip_msg_t *msg, char *prname, char *pcname) {
	return w_xav_child_rm(msg, prname, pcname, 1);
}

/**
 *
 */
static int ki_xav_is_null(sip_msg_t *msg, str *rname, int _case)
{
	sr_xavp_t *xavp=NULL;
	if(_case) {
		xavp = xavi_get_by_index(rname, 0, NULL);
	} else {
		xavp = xavp_get_by_index(rname, 0, NULL);
	}
	if(xavp==NULL) {
		return 1;
	}
	if(xavp->val.type == SR_XTYPE_NULL) {
		return 1;
	}
	return -1;
}

static int ki_xavp_is_null(sip_msg_t *msg, str *rname) {
	return ki_xav_is_null(msg, rname, 0);
}

static int ki_xavi_is_null(sip_msg_t *msg, str *rname) {
	return ki_xav_is_null(msg, rname, 1);
}
/**
 *
 */
static sr_kemi_xval_t _sr_kemi_pv_xval = {0};

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_get_xval(sr_xavp_t *xavp, int rmode)
{
	static char _pv_ki_xavp_buf[128];

	switch(xavp->val.type) {
		case SR_XTYPE_NULL:
			sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
			return &_sr_kemi_pv_xval;
		break;
		case SR_XTYPE_INT:
			_sr_kemi_pv_xval.vtype = SR_KEMIP_INT;
			_sr_kemi_pv_xval.v.n = xavp->val.v.i;
			return &_sr_kemi_pv_xval;
		break;
		case SR_XTYPE_STR:
			_sr_kemi_pv_xval.vtype = SR_KEMIP_STR;
			_sr_kemi_pv_xval.v.s = xavp->val.v.s;
			return &_sr_kemi_pv_xval;
		break;
		case SR_XTYPE_TIME:
			if(snprintf(_pv_ki_xavp_buf, 128, "%lu", (long unsigned)xavp->val.v.t)<0) {
				sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
				return &_sr_kemi_pv_xval;
			}
		break;
		case SR_XTYPE_LONG:
			if(snprintf(_pv_ki_xavp_buf, 128, "%ld", (long unsigned)xavp->val.v.l)<0) {
				sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
				return &_sr_kemi_pv_xval;
			}
		break;
		case SR_XTYPE_LLONG:
			if(snprintf(_pv_ki_xavp_buf, 128, "%lld", xavp->val.v.ll)<0) {
				sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
				return &_sr_kemi_pv_xval;
			}
		break;
		case SR_XTYPE_XAVP:
			if(snprintf(_pv_ki_xavp_buf, 128, "<<xavp:%p>>", xavp->val.v.xavp)<0) {
				sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
				return &_sr_kemi_pv_xval;
			}
		break;
		case SR_XTYPE_DATA:
			if(snprintf(_pv_ki_xavp_buf, 128, "<<data:%p>>", xavp->val.v.data)<0) {
				sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
				return &_sr_kemi_pv_xval;
			}
		break;
		default:
			sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
			return &_sr_kemi_pv_xval;
	}

	_sr_kemi_pv_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_pv_xval.v.s.s = _pv_ki_xavp_buf;
	_sr_kemi_pv_xval.v.s.len = strlen(_pv_ki_xavp_buf);
	return &_sr_kemi_pv_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_xav_get_mode(sip_msg_t *msg, str *rname, int rmode,
		int _case)
{
	sr_xavp_t *xavp=NULL;

	memset(&_sr_kemi_pv_xval, 0, sizeof(sr_kemi_xval_t));
	if(_case) {
		xavp = xavi_get_by_index(rname, 0, NULL);
	} else {
		xavp = xavp_get_by_index(rname, 0, NULL);
	}
	if(xavp==NULL) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}

	return ki_xavp_get_xval(xavp, rmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_get(sip_msg_t *msg, str *rname)
{
	return ki_xav_get_mode(msg, rname, SR_KEMI_XVAL_NULL_NONE, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_get(sip_msg_t *msg, str *rname)
{
	return ki_xav_get_mode(msg, rname, SR_KEMI_XVAL_NULL_NONE, 1);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_gete(sip_msg_t *msg, str *rname)
{
	return ki_xav_get_mode(msg, rname, SR_KEMI_XVAL_NULL_EMPTY, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_gete(sip_msg_t *msg, str *rname)
{
	return ki_xav_get_mode(msg, rname, SR_KEMI_XVAL_NULL_EMPTY, 1);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_getw(sip_msg_t *msg, str *rname)
{
	return ki_xav_get_mode(msg, rname, SR_KEMI_XVAL_NULL_PRINT, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_getw(sip_msg_t *msg, str *rname)
{
	return ki_xav_get_mode(msg, rname, SR_KEMI_XVAL_NULL_PRINT, 1);
}

/**
 *
 */
sr_kemi_dict_item_t* ki_xav_dict(sr_xavp_t *xavp, int _case);

/**
 * SR_KEMIP_ARRAY with values of xavp=>name
 */
sr_kemi_dict_item_t* ki_xav_dict_name(sr_xavp_t *xavp, str *name, int _case)
{
	sr_kemi_dict_item_t *ini = NULL;
	sr_kemi_dict_item_t *val;
	sr_kemi_dict_item_t *last = NULL;
	sr_xavp_t *avp = xavp;

	ini = (sr_kemi_dict_item_t*)pkg_malloc(sizeof(sr_kemi_dict_item_t));
	if(ini==NULL) {
		PKG_MEM_ERROR;
		return NULL;
	}
	memset(ini, 0, sizeof(sr_kemi_xval_t));
	ini->vtype = SR_KEMIP_ARRAY;
	if(_case) {
		while(avp!=NULL&&!cmpi_str(&avp->name, name))
		{
			avp = avp->next;
		}
	} else {
		while(avp!=NULL&&!STR_EQ(avp->name,*name))
		{
			avp = avp->next;
		}
	}
	while(avp!=NULL){
		switch(avp->val.type) {
			case SR_XTYPE_XAVP:
			break;
			default:
				val = (sr_kemi_dict_item_t*)pkg_malloc(sizeof(sr_kemi_dict_item_t));
				if(val==NULL) {
					PKG_MEM_ERROR;
					goto error;
				}
				memset(val, 0, sizeof(sr_kemi_xval_t));
			break;
		}
		switch(avp->val.type) {
			case SR_XTYPE_NULL:
				val->vtype = SR_KEMIP_NULL;
			break;
			case SR_XTYPE_INT:
				val->vtype = SR_KEMIP_INT;
				val->v.n = avp->val.v.i;
			break;
			case SR_XTYPE_STR:
				val->vtype = SR_KEMIP_STR;
				val->v.s.s = avp->val.v.s.s;
				val->v.s.len = avp->val.v.s.len;
			break;
			case SR_XTYPE_TIME:
			case SR_XTYPE_LONG:
			case SR_XTYPE_LLONG:
			case SR_XTYPE_DATA:
				val->vtype = SR_KEMIP_NULL;
				LM_WARN("XAVP type:%d value not supported\n", avp->val.type);
			break;
			case SR_XTYPE_XAVP:
				val = ki_xav_dict(avp->val.v.xavp, _case);
			break;
			default:
				val->vtype = SR_KEMIP_NULL;
				LM_ERR("xavp:%.*s unknown type: %d\n",
					avp->name.len, avp->name.s, avp->val.type);
			break;
		}
		if(last) {
			last->next = val;
		} else {
			ini->v.dict = val;
		}
		last = val;
		if(_case) {
			avp = xavi_get_next(avp);
		} else {
			avp = xavp_get_next(avp);
		}
	}
	return ini;
error:
	while(ini) {
		last = ini;
		ini = ini->next;
		pkg_free(last);
	}
	return NULL;
}

/**
 * SR_KEMIP_DICT of xavp
 */
sr_kemi_dict_item_t* ki_xav_dict(sr_xavp_t *xavp, int _case)
{
	sr_xavp_t *avp = NULL;
	struct str_list *keys;
	struct str_list *k;
	sr_kemi_dict_item_t *val;
	sr_kemi_dict_item_t *ini = NULL;
	sr_kemi_dict_item_t *last = NULL;

	if(xavp->val.type!=SR_XTYPE_XAVP) {
		LM_ERR("%s not xavp?\n", xavp->name.s);
		return NULL;
	}
	avp = xavp->val.v.xavp;
	if(_case) {
		keys = xavi_get_list_key_names(xavp);
	} else {
		keys = xavp_get_list_key_names(xavp);
	}
	if( keys != NULL) {
		do {
			val = (sr_kemi_dict_item_t*)pkg_malloc(sizeof(sr_kemi_dict_item_t));
			if(val==NULL) {
				PKG_MEM_ERROR;
				goto error;
			}
			memset(val, 0, sizeof(sr_kemi_xval_t));
			val->vtype = SR_KEMIP_DICT;
			val->name.s = keys->s.s;
			val->name.len = keys->s.len;
			val->v.dict = ki_xav_dict_name(avp, &keys->s, _case);
			if(last) {
				last->next = val;
			} else {
				ini = val;
			}
			last = val;
			k = keys;
			keys = keys->next;
			pkg_free(k);
		} while(keys!=NULL);
	}
	return ini;
error:
	while(keys!=NULL) {
		k = keys;
		keys = keys->next;
		pkg_free(k);
	}
	while(ini) {
		val = ini;
		ini = ini->next;
		pkg_free(val);
	}
	return NULL;
}

/**
 *
 */
static sr_kemi_xval_t* ki_xav_getd_helper(sip_msg_t *msg, str *rname,
		int *_indx, int _case)
{
	sr_xavp_t *xavp=NULL;
	int xavp_size = 0;
	int indx = 0;
	sr_kemi_dict_item_t *val;
	sr_kemi_dict_item_t *last = NULL;

	memset(&_sr_kemi_pv_xval, 0, sizeof(sr_kemi_xval_t));
	if(_indx) {
		indx = *_indx;
		/* we're going to retrive just one */
		_sr_kemi_pv_xval.vtype = SR_KEMIP_DICT;
	} else {
		/* we're going to retrive all */
		_sr_kemi_pv_xval.vtype = SR_KEMIP_ARRAY;
	}
	if(_case) {
		xavp_size = xavi_count(rname, NULL);
	} else {
		xavp_size = xavp_count(rname, NULL);
	}
	if(indx<0)
	{
		if((indx*-1)>xavp_size)
		{
			sr_kemi_xval_null(&_sr_kemi_pv_xval, SR_KEMI_XVAL_NULL_NONE);
			return &_sr_kemi_pv_xval;
		}
		indx = xavp_size + indx;
	}

	if(_case) {
		xavp = xavi_get_by_index(rname, indx, NULL);
	} else {
		xavp = xavp_get_by_index(rname, indx, NULL);
	}
	if(xavp==NULL) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_pv_xval;
	}
	do {
		val = ki_xav_dict(xavp, _case);
		if(last) {
			last->next = val;
		} else {
			_sr_kemi_pv_xval.v.dict = val;
		}
		if(val) last = val;
		if(_indx) {
			xavp = NULL;
		} else {
			indx = indx + 1;
			if(_case) {
				xavp = xavi_get_by_index(rname, indx, NULL);
			} else {
				xavp = xavp_get_by_index(rname, indx, NULL);
			}
		}
	} while(xavp!=NULL);
	return &_sr_kemi_pv_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_getd(sip_msg_t *msg, str *rname)
{
	return ki_xav_getd_helper(msg, rname, NULL, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_getd(sip_msg_t *msg, str *rname)
{
	return ki_xav_getd_helper(msg, rname, NULL, 1);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_getd_p1(sip_msg_t *msg, str *rname, int indx)
{
	return ki_xav_getd_helper(msg, rname, &indx, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_getd_p1(sip_msg_t *msg, str *rname, int indx)
{
	return ki_xav_getd_helper(msg, rname, &indx, 1);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xav_get_keys(sip_msg_t *msg, str *rname, int indx, int _case)
{
	sr_xavp_t *xavp=NULL;
	struct str_list *keys, *k;
	sr_kemi_dict_item_t *val;
	sr_kemi_dict_item_t *last = NULL;

	memset(&_sr_kemi_pv_xval, 0, sizeof(sr_kemi_xval_t));

	if(_case) {
		xavp = xavi_get_by_index(rname, indx, NULL);
	} else {
		xavp = xavp_get_by_index(rname, indx, NULL);
	}
	if(xavp==NULL) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, SR_KEMI_XVAL_NULL_NONE);
		return &_sr_kemi_pv_xval;
	}
	if(_case) {
		keys = xavi_get_list_key_names(xavp);
	} else {
		keys = xavp_get_list_key_names(xavp);
	}
	_sr_kemi_pv_xval.vtype = SR_KEMIP_ARRAY;
	while(keys!=NULL){
		k = keys;
		val = (sr_kemi_dict_item_t*)pkg_malloc(sizeof(sr_kemi_dict_item_t));
		if(val==NULL) {
			PKG_MEM_ERROR;
			goto error;
		}
		memset(val, 0, sizeof(sr_kemi_xval_t));
		val->vtype = SR_KEMIP_STR;
		val->v.s.len = k->s.len;
		val->v.s.s = k->s.s;
		keys = k->next;
		pkg_free(k);
		if(last) {
			last->next = val;
		} else {
			_sr_kemi_pv_xval.v.dict = val;
		}
		last = val;
	}
	return &_sr_kemi_pv_xval;
error:
	while(keys!=NULL) {
		k = keys;
		keys = keys->next;
		pkg_free(k);
	}
	last = _sr_kemi_pv_xval.v.dict;
	while(last) {
		val = last;
		last = last->next;
		pkg_free(val);
	}
	sr_kemi_xval_null(&_sr_kemi_pv_xval, SR_KEMI_XVAL_NULL_NONE);
	return &_sr_kemi_pv_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_get_keys(sip_msg_t *msg, str *rname, int indx)
{
	return ki_xav_get_keys(msg, rname, indx, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_get_keys(sip_msg_t *msg, str *rname, int indx)
{
	return ki_xav_get_keys(msg, rname, indx, 1);
}

/**
 *
 */
static int ki_xav_child_is_null(sip_msg_t *msg, str *rname, str *cname, int _case)
{
	sr_xavp_t *xavp=NULL;
	if(_case) {
		xavp = xavi_get_by_index(rname, 0, NULL);
	} else {
		xavp = xavp_get_by_index(rname, 0, NULL);
	}
	if(xavp==NULL) {
		return 1;
	}
	if(xavp->val.type != SR_XTYPE_XAVP) {
		return 1;
	}
	if(_case) {
		xavp = xavi_get_by_index(cname, 0, &xavp->val.v.xavp);
	} else {
		xavp = xavp_get_by_index(cname, 0, &xavp->val.v.xavp);
	}
	if(xavp==NULL) {
		return 1;
	}
	if(xavp->val.type == SR_XTYPE_NULL) {
		return 1;
	}
	return -1;
}

/**
 *
 */
static int ki_xavp_child_is_null(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_is_null(msg, rname, cname, 0);
}

/**
 *
 */
static int ki_xavi_child_is_null(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_is_null(msg, rname, cname, 1);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xav_child_get_mode(sip_msg_t *msg, str *rname,
		str *cname, int rmode, int _case)
{
	sr_xavp_t *xavp=NULL;

	memset(&_sr_kemi_pv_xval, 0, sizeof(sr_kemi_xval_t));

	if(_case) {
		xavp = xavi_get_by_index(rname, 0, NULL);
	} else {
		xavp = xavp_get_by_index(rname, 0, NULL);
	}
	if(xavp==NULL) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}

	if(xavp->val.type != SR_XTYPE_XAVP) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}

	if(_case) {
		xavp = xavi_get_by_index(cname, 0, &xavp->val.v.xavp);
	} else {
		xavp = xavp_get_by_index(cname, 0, &xavp->val.v.xavp);
	}
	if(xavp==NULL) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}

	return ki_xavp_get_xval(xavp, rmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_child_get(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_NONE, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_child_get(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_NONE, 1);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_child_gete(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_EMPTY, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_child_gete(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_EMPTY, 1);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavp_child_getw(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_PRINT, 0);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavi_child_getw(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xav_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_PRINT, 1);
}

/**
 *
 */
static int ki_xavu_is_null(sip_msg_t *msg, str *rname)
{
	sr_xavp_t *xavu=NULL;

	xavu = xavu_lookup(rname, NULL);
	if(xavu==NULL) {
		return 1;
	}
	if(xavu->val.type == SR_XTYPE_NULL) {
		return 1;
	}
	return -1;
}

/**
 *
 */
static int ki_xavu_rm(sip_msg_t *msg, str *rname)
{
	int ret;

	ret = xavu_rm_by_name(rname, NULL);

	return (ret==0)?1:ret;
}

/**
 *
 */
static int ki_xavu_child_rm(sip_msg_t *msg, str *rname, str *cname)
{
	int ret;

	ret = xavu_rm_child_by_name(rname, cname);

	return (ret==0)?1:ret;
}

/**
 *
 */
static int ki_xavu_seti(sip_msg_t *msg, str *rname, int ival)
{
	sr_xavp_t *xavp = NULL;

	xavp = xavu_set_ival(rname, ival);

	return (xavp!=NULL)?1:-1;
}

/**
 *
 */
static int ki_xavu_sets(sip_msg_t *msg, str *rname, str *sval)
{
	sr_xavp_t *xavp = NULL;

	xavp = xavu_set_sval(rname, sval);

	return (xavp!=NULL)?1:-1;
}

/**
 *
 */
static int ki_xavu_child_seti(sip_msg_t *msg, str *rname, str *cname,
		int ival)
{
	sr_xavp_t *xavu = NULL;

	xavu = xavu_set_child_ival(rname, cname, ival);

	return (xavu!=NULL)?1:-1;
}

/**
 *
 */
static int ki_xavu_child_sets(sip_msg_t *msg, str *rname, str *cname,
		str *sval)
{
	sr_xavp_t *xavu = NULL;

	xavu = xavu_set_child_sval(rname, cname, sval);

	return (xavu!=NULL)?1:-1;
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavu_get_mode(sip_msg_t *msg, str *rname, int rmode)
{
	sr_xavp_t *xavu=NULL;

	memset(&_sr_kemi_pv_xval, 0, sizeof(sr_kemi_xval_t));

	xavu = xavu_lookup(rname, NULL);
	if(xavu==NULL) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}

	return ki_xavp_get_xval(xavu, rmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavu_get(sip_msg_t *msg, str *rname)
{
	return ki_xavu_get_mode(msg, rname, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavu_gete(sip_msg_t *msg, str *rname)
{
	return ki_xavu_get_mode(msg, rname, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavu_getw(sip_msg_t *msg, str *rname)
{
	return ki_xavu_get_mode(msg, rname, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static int ki_xavu_child_is_null(sip_msg_t *msg, str *rname, str *cname)
{
	sr_xavp_t *xavu=NULL;

	xavu = xavp_get_by_index(rname, 0, NULL);
	if(xavu==NULL) {
		return 1;
	}
	if(xavu->val.type != SR_XTYPE_XAVP) {
		return 1;
	}
	xavu = xavp_get_by_index(cname, 0, &xavu->val.v.xavp);
	if(xavu==NULL) {
		return 1;
	}
	if(xavu->val.type == SR_XTYPE_NULL) {
		return 1;
	}
	return -1;
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavu_child_get_mode(sip_msg_t *msg, str *rname,
		str *cname, int rmode)
{
	sr_xavp_t *xavu=NULL;

	memset(&_sr_kemi_pv_xval, 0, sizeof(sr_kemi_xval_t));

	xavu = xavu_lookup(rname, NULL);
	if(xavu==NULL) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}

	if(xavu->val.type != SR_XTYPE_XAVP) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}

	xavu = xavp_get_by_index(cname, 0, &xavu->val.v.xavp);
	if(xavu==NULL) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}

	return ki_xavp_get_xval(xavu, rmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_xavu_child_get(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xavu_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_NONE);
}


/**
 *
 */
static sr_kemi_xval_t* ki_xavu_child_gete(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xavu_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_EMPTY);
}


/**
 *
 */
static sr_kemi_xval_t* ki_xavu_child_getw(sip_msg_t *msg, str *rname, str *cname)
{
	return ki_xavu_child_get_mode(msg, rname, cname, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static int w_sbranch_set_ruri(sip_msg_t *msg, char p1, char *p2)
{
	if(sbranch_set_ruri(msg)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_sbranch_append(sip_msg_t *msg, char p1, char *p2)
{
	if(sbranch_append(msg)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_sbranch_reset(sip_msg_t *msg, char p1, char *p2)
{
	if(sbranch_reset()<0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_sbranch_set_ruri(sip_msg_t *msg)
{
	if(sbranch_set_ruri(msg)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_sbranch_append(sip_msg_t *msg)
{
	if(sbranch_append(msg)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_sbranch_reset(sip_msg_t *msg)
{
	if(sbranch_reset()<0)
		return -1;
	return 1;
}

int pv_xavp_copy_fixup(void **param, int param_no)
{
	if(param_no == 1 || param_no == 3)
		return fixup_var_str_12(param, param_no);
	if (param_no == 2 || param_no == 4)
		return fixup_var_int_12(param, param_no);
	LM_ERR("invalid parameter count [%d]\n", param_no);
	return -1;
}

int pv_evalx_fixup(void** param, int param_no)
{
	pv_spec_t *spec=NULL;
	pv_elem_t *pvmodel=NULL;
	str tstr;

	if(param_no==1) {
		spec = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
		if(spec==NULL) {
			LM_ERR("out of pkg\n");
			return -1;
		}
		memset(spec, 0, sizeof(pv_spec_t));
		tstr.s = (char*)(*param);
		tstr.len = strlen(tstr.s);
		if(pv_parse_spec(&tstr, spec)==NULL) {
			LM_ERR("unknown script variable in first parameter\n");
			pkg_free(spec);
			return -1;
		}
		if(!pv_is_w(spec)) {
			LM_ERR("read-only script variable in first parameter\n");
			pkg_free(spec);
			return -1;
		}
		*param = spec;
	} else if(param_no==2) {
		pvmodel = 0;
		tstr.s = (char*)(*param);
		tstr.len = strlen(tstr.s);
		if(pv_parse_format(&tstr, &pvmodel)<0) {
			LM_ERR("error in second parameter\n");
			return -1;
		}
		*param = pvmodel;
	}
	return 0;
}

/**
 *
 */
int w_pv_evalx(struct sip_msg *msg, char *dst, str *fmt)
{
	pv_spec_t *ispec=NULL;
	pv_elem_t *imodel=NULL;
	str tstr = {0, 0};
	pv_value_t val;

	ispec = (pv_spec_t*)dst;

	imodel = (pv_elem_t*)fmt;

	memset(&val, 0, sizeof(pv_value_t));

	if(pv_printf_s(msg, imodel, &tstr)!=0) {
		LM_ERR("cannot eval second parameter\n");
		goto error;
	}

	LM_DBG("preparing to evaluate: [%.*s]\n", tstr.len, tstr.s);
	if(pv_eval_str(msg, &val.rs, &tstr)<0){
		LM_ERR("cannot eval reparsed value of second parameter\n");
		return -1;
	}

	val.flags = PV_VAL_STR;
	if(ispec->setf(msg, &ispec->pvp, EQ_T, &val)<0) {
		LM_ERR("setting PV failed\n");
		goto error;
	}

	return 1;
error:
	return -1;
}

/**
 *
 */
int ki_pv_evalx(sip_msg_t *msg, str *dst, str *fmt)
{
	pv_value_t val;
	pv_spec_t *ispec=NULL;

	if(dst==NULL || dst->s==NULL || dst->len<=0) {
		LM_ERR("invalid destination var name\n");
		return -1;
	}
	ispec = pv_cache_get(dst);
	if(ispec==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", dst->len, dst->s);
		return -1;
	}

	memset(&val, 0, sizeof(pv_value_t));
	if(pv_eval_str(msg, &val.rs, fmt)<0) {
		LM_ERR("cannot eval reparsed value of second parameter\n");
		return -1;
	}

	val.flags = PV_VAL_STR;
	if(ispec->setf(msg, &ispec->pvp, EQ_T, &val)<0) {
		LM_ERR("setting PV failed\n");
		goto error;
	}

	return 1;
error:
	return -1;
}

/**
 *
 */
static int ki_avp_seti(sip_msg_t *msg, str *xname, int vn)
{
	unsigned short atype;
	int_str aname;
	int_str avalue;

	memset(&aname, 0, sizeof(int_str));

	atype = AVP_NAME_STR;
	aname.s = *xname;

	avalue.n = vn;

	if (add_avp(atype, aname, avalue)<0) {
		LM_ERR("error - cannot add AVP\n");
		return -1;
	}

	return 1;
}

/**
 *
 */
static int ki_avp_sets(sip_msg_t *msg, str *xname, str *vs)
{
	unsigned short atype;
	int_str aname;
	int_str avalue;

	memset(&aname, 0, sizeof(int_str));

	atype = AVP_NAME_STR;
	aname.s = *xname;

	avalue.s = *vs;
	atype |= AVP_VAL_STR;

	if (add_avp(atype, aname, avalue)<0) {
		LM_ERR("error - cannot add AVP\n");
		return -1;
	}

	return 1;
}

/**
 *
 */
static int ki_avp_rm(sip_msg_t *msg, str *xname)
{
	unsigned short atype;
	int_str aname;

	memset(&aname, 0, sizeof(int_str));

	atype = AVP_NAME_STR;
	aname.s = *xname;

	destroy_avps(atype, aname, 0);

	return 1;
}

/**
 *
 */
static int ki_avp_is_null(sip_msg_t *msg, str *xname)
{
	unsigned short atype;
	int_str aname;
	int_str avalue;
	avp_search_state_t astate;

	memset(&astate, 0, sizeof(avp_search_state_t));
	memset(&aname, 0, sizeof(int_str));

	atype = AVP_NAME_STR;
	aname.s = *xname;

	destroy_avps(atype, aname, 0);

	if (search_first_avp(atype, aname, &avalue, &astate)==0) {
		return 1;
	}

	return -1;
}

/**
 *
 */
static sr_kemi_xval_t* ki_avp_get_mode(sip_msg_t *msg, str *xname, int rmode)
{
	avp_t *avp = NULL;
	avp_search_state_t astate;
	unsigned short atype;
	int_str aname;
	int_str avalue;

	memset(&_sr_kemi_pv_xval, 0, sizeof(sr_kemi_xval_t));
	memset(&astate, 0, sizeof(avp_search_state_t));
	memset(&aname, 0, sizeof(int_str));

	atype = AVP_NAME_STR;
	aname.s = *xname;

	if ((avp=search_first_avp(atype, aname, &avalue, &astate))==0) {
		sr_kemi_xval_null(&_sr_kemi_pv_xval, rmode);
		return &_sr_kemi_pv_xval;
	}
	if(avp->flags & AVP_VAL_STR) {
		_sr_kemi_pv_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_pv_xval.v.s = avalue.s;
		return &_sr_kemi_pv_xval;
	} else {
		_sr_kemi_pv_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_pv_xval.v.n = avalue.n;
		return &_sr_kemi_pv_xval;
	}
}

/**
 *
 */
static sr_kemi_xval_t* ki_avp_get(sip_msg_t *msg, str *xname)
{
	return ki_avp_get_mode(msg, xname, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_avp_gete(sip_msg_t *msg, str *xname)
{
	return ki_avp_get_mode(msg, xname, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* ki_avp_getw(sip_msg_t *msg, str *xname)
{
	return ki_avp_get_mode(msg, xname, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_pvx_exports[] = {
	{ str_init("pvx"), str_init("sbranch_set_ruri"),
		SR_KEMIP_INT, ki_sbranch_set_ruri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("sbranch_append"),
		SR_KEMIP_INT, ki_sbranch_append,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("sbranch_reset"),
		SR_KEMIP_INT, ki_sbranch_reset,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("var_seti"),
		SR_KEMIP_INT, ki_var_seti,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("var_sets"),
		SR_KEMIP_INT, ki_var_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("var_get"),
		SR_KEMIP_XVAL, ki_var_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("shv_seti"),
		SR_KEMIP_INT, ki_shv_seti,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("shv_sets"),
		SR_KEMIP_INT, ki_shv_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("shv_get"),
		SR_KEMIP_XVAL, ki_shv_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("pv_var_to_xavp"),
		SR_KEMIP_INT, ki_var_to_xavp,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("pv_xavp_to_var"),
		SR_KEMIP_INT, ki_xavp_to_var,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("pv_xavp_print"),
		SR_KEMIP_INT, ki_xavp_print,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("pv_xavu_print"),
		SR_KEMIP_INT, ki_xavu_print,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("pv_xavi_print"),
		SR_KEMIP_INT, ki_xavi_print,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_params_explode"),
		SR_KEMIP_INT, ki_xavp_params_explode,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_params_implode"),
		SR_KEMIP_INT, ki_xavp_params_implode,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_params_explode"),
		SR_KEMIP_INT, ki_xavu_params_explode,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_params_implode"),
		SR_KEMIP_INT, ki_xavu_params_implode,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_slist_explode"),
		SR_KEMIP_INT, ki_xavp_slist_explode,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_seti"),
		SR_KEMIP_INT, ki_xavp_seti,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_sets"),
		SR_KEMIP_INT, ki_xavp_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_get"),
		SR_KEMIP_XVAL, ki_xavp_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_gete"),
		SR_KEMIP_XVAL, ki_xavp_gete,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_getw"),
		SR_KEMIP_XVAL, ki_xavp_getw,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_getd"),
		SR_KEMIP_XVAL, ki_xavp_getd,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_getd_p1"),
		SR_KEMIP_XVAL, ki_xavp_getd_p1,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_get_keys"),
		SR_KEMIP_XVAL, ki_xavp_get_keys,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_rm"),
		SR_KEMIP_INT, ki_xavp_rm,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_is_null"),
		SR_KEMIP_INT, ki_xavp_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_child_seti"),
		SR_KEMIP_INT, ki_xavp_child_seti,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_child_sets"),
		SR_KEMIP_INT, ki_xavp_child_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_child_rm"),
		SR_KEMIP_INT, ki_xavp_child_rm,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_child_is_null"),
		SR_KEMIP_INT, ki_xavp_child_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_child_get"),
		SR_KEMIP_XVAL, ki_xavp_child_get,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_child_gete"),
		SR_KEMIP_XVAL, ki_xavp_child_gete,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_child_getw"),
		SR_KEMIP_XVAL, ki_xavp_child_getw,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_seti"),
		SR_KEMIP_INT, ki_xavu_seti,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_sets"),
		SR_KEMIP_INT, ki_xavu_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_get"),
		SR_KEMIP_XVAL, ki_xavu_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_gete"),
		SR_KEMIP_XVAL, ki_xavu_gete,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_getw"),
		SR_KEMIP_XVAL, ki_xavu_getw,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_rm"),
		SR_KEMIP_INT, ki_xavu_rm,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_is_null"),
		SR_KEMIP_INT, ki_xavu_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_child_seti"),
		SR_KEMIP_INT, ki_xavu_child_seti,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_child_sets"),
		SR_KEMIP_INT, ki_xavu_child_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_child_rm"),
		SR_KEMIP_INT, ki_xavu_child_rm,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_child_is_null"),
		SR_KEMIP_INT, ki_xavu_child_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_child_get"),
		SR_KEMIP_XVAL, ki_xavu_child_get,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_child_gete"),
		SR_KEMIP_XVAL, ki_xavu_child_gete,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavu_child_getw"),
		SR_KEMIP_XVAL, ki_xavu_child_getw,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_seti"),
		SR_KEMIP_INT, ki_xavi_seti,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_sets"),
		SR_KEMIP_INT, ki_xavi_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_get"),
		SR_KEMIP_XVAL, ki_xavi_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_gete"),
		SR_KEMIP_XVAL, ki_xavi_gete,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_getw"),
		SR_KEMIP_XVAL, ki_xavi_getw,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_getd"),
		SR_KEMIP_XVAL, ki_xavi_getd,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_getd_p1"),
		SR_KEMIP_XVAL, ki_xavi_getd_p1,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_get_keys"),
		SR_KEMIP_XVAL, ki_xavi_get_keys,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_rm"),
		SR_KEMIP_INT, ki_xavi_rm,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_is_null"),
		SR_KEMIP_INT, ki_xavi_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_child_seti"),
		SR_KEMIP_INT, ki_xavi_child_seti,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_child_sets"),
		SR_KEMIP_INT, ki_xavi_child_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_child_rm"),
		SR_KEMIP_INT, ki_xavi_child_rm,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_child_is_null"),
		SR_KEMIP_INT, ki_xavi_child_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_child_get"),
		SR_KEMIP_XVAL, ki_xavi_child_get,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_child_gete"),
		SR_KEMIP_XVAL, ki_xavi_child_gete,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavi_child_getw"),
		SR_KEMIP_XVAL, ki_xavi_child_getw,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("evalx"),
		SR_KEMIP_INT, ki_pv_evalx,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("avp_seti"),
		SR_KEMIP_INT, ki_avp_seti,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("avp_sets"),
		SR_KEMIP_INT, ki_avp_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("avp_get"),
		SR_KEMIP_XVAL, ki_avp_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("avp_gete"),
		SR_KEMIP_XVAL, ki_avp_gete,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("avp_getw"),
		SR_KEMIP_XVAL, ki_avp_getw,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("avp_rm"),
		SR_KEMIP_INT, ki_avp_rm,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("avp_is_null"),
		SR_KEMIP_INT, ki_avp_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_copy"),
		SR_KEMIP_INT, ki_xavp_copy,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_copy_dst"),
		SR_KEMIP_INT, ki_xavp_copy_dst,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pvx"), str_init("xavp_lshift"),
		SR_KEMIP_INT, ki_xavp_lshift,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
static const char* rpc_shv_set_doc[2] = {
	"Set a shared variable (args: name type value)",
	0
};

static const char* rpc_shv_get_doc[2] = {
	"Get the value of a shared variable. If no argument, dumps all",
	0
};

rpc_export_t pv_rpc[] = {
	{"pv.shvSet", rpc_shv_set, rpc_shv_set_doc, 0},
	{"pv.shvGet", rpc_shv_get, rpc_shv_get_doc, 0},
	{0, 0, 0, 0}
};

static int pv_init_rpc(void)
{
	if (rpc_register_array(pv_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_pvx_exports);
	if(tr_init_buffers()<0)
	{
		LM_ERR("failed to initialize transformations buffers\n");
		return -1;
	}
	return register_trans_mod(path, mod_trans);
}
