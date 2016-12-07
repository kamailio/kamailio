/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../sr_module.h"

#include "debugger_act.h"

typedef struct _dbg_action {
	int type;
	str name;
} dbg_action_t;

static str _dbg_action_special[] = {
	str_init("unknown"),   /* 0 */
	str_init("exit"),      /* 1 */
	str_init("drop"),      /* 2 */
	str_init("return"),    /* 3 */
	{0, 0}
};

static dbg_action_t _dbg_action_list[] = {
	{ FORWARD_T, str_init("forward") },
	{ LOG_T, str_init("log") },
	{ ERROR_T, str_init("error") },
	{ ROUTE_T, str_init("route") },
	{ EXEC_T, str_init("exec") },
	{ SET_HOST_T, str_init("sethost") },
	{ SET_HOSTPORT_T, str_init("sethostport") },
	{ SET_USER_T, str_init("setuser") },
	{ SET_USERPASS_T, str_init("setuserpass") },
	{ SET_PORT_T, str_init("setport") },
	{ SET_URI_T, str_init("seturi") },
	{ SET_HOSTPORTTRANS_T, str_init("sethostporttrans") },
	{ SET_HOSTALL_T, str_init("sethostall") },
	{ SET_USERPHONE_T, str_init("setuserphone") },
	{ IF_T, str_init("if") },
	{ SWITCH_T, str_init("switch") },
	{ BLOCK_T, str_init("block") },
	{ EVAL_T, str_init("eval") },
	{ SWITCH_JT_T, str_init("switch") },
	{ SWITCH_COND_T, str_init("switch") },
	{ MATCH_COND_T, str_init("case") },
	{ WHILE_T, str_init("while") },
	{ SETFLAG_T, str_init("setflag") },
	{ RESETFLAG_T, str_init("resetflag") },
	{ ISFLAGSET_T, str_init("isflagset") },
	{ AVPFLAG_OPER_T, str_init("avpflag") },
	{ LEN_GT_T, str_init("lengt") },
	{ PREFIX_T, str_init("prefix") },
	{ STRIP_T, str_init("strip") },
	{ STRIP_TAIL_T, str_init("striptail") },
	{ APPEND_BRANCH_T, str_init("append_branch") },
	{ REVERT_URI_T, str_init("reverturi") },
	{ FORWARD_TCP_T, str_init("forward_tcp") },
	{ FORWARD_UDP_T, str_init("forward_udp") },
	{ FORWARD_TLS_T, str_init("forward_tls") },
	{ FORWARD_SCTP_T, str_init("forward_sctp") },
	{ FORCE_RPORT_T, str_init("force_rport") },
	{ ADD_LOCAL_RPORT_T, str_init("add_local_rport") },
	{ SET_ADV_ADDR_T, str_init("set_adv_addr") },
	{ SET_ADV_PORT_T, str_init("set_adv_port") },
	{ FORCE_TCP_ALIAS_T, str_init("force_tcp_alias") },
	{ LOAD_AVP_T, str_init("load_avp") },
	{ AVP_TO_URI_T, str_init("avp_to_uri") },
	{ FORCE_SEND_SOCKET_T, str_init("force_send_socket") },
	{ ASSIGN_T, str_init("assign") },
	{ ADD_T, str_init("add") },
	{ UDP_MTU_TRY_PROTO_T, str_init("udp_mtu_try_proto") },
	{ SET_FWD_NO_CONNECT_T, str_init("set_fwd_no_connect") },
	{ SET_RPL_NO_CONNECT_T, str_init("set_rpl_no_connect") },
	{ SET_FWD_CLOSE_T, str_init("set_fwd_close") },
	{ SET_RPL_CLOSE_T, str_init("set_rpl_close") },
	{ 0, {0, 0} }
};

str* dbg_get_action_name(struct action *a)
{
	int i;
	static str aname;
	cmd_export_common_t *cmd;

	if(a==NULL)
		return &_dbg_action_special[0];

	switch(a->type) {
		case DROP_T:
			if(a->val[1].u.number&DROP_R_F)
				return &_dbg_action_special[2];
			if(a->val[1].u.number&RETURN_R_F)
				return &_dbg_action_special[3];
			return &_dbg_action_special[1];
		case MODULE0_T:
		case MODULE1_T:
		case MODULE2_T:
		case MODULE3_T:
		case MODULE4_T:
		case MODULE5_T:
		case MODULE6_T:
		case MODULEX_T:
		case MODULE1_RVE_T:
		case MODULE2_RVE_T:
		case MODULE3_RVE_T:
		case MODULE4_RVE_T:
		case MODULE5_RVE_T:
		case MODULE6_RVE_T:
		case MODULEX_RVE_T:
			cmd = (cmd_export_common_t*)(a->val[0].u.data);
			aname.s = cmd->name;
			aname.len = strlen(aname.s);
			return &aname;
		default:
			for(i=0; _dbg_action_list[i].type!=0; i++)
			{
				if(_dbg_action_list[i].type==a->type)
					return &_dbg_action_list[i].name;
			}
	}

	return &_dbg_action_special[0];
}
