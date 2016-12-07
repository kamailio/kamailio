/*
 * route structures helping functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file
 * \brief Kamailio core :: route structures helping functions
 * \ingroup core
 * Module: \ref core
 */



#include  "route_struct.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "dprint.h"
#include "ip_addr.h"
#include "mem/mem.h"
#include "usr_avp.h"
#include "ut.h" /* ZSW() */



/** joins to cfg file positions into a new one. */
void cfg_pos_join(struct cfg_pos* res,
							struct cfg_pos* pos1, struct cfg_pos* pos2)
{
	struct cfg_pos ret;
	ret=*pos1;
	if ((ret.s_line == 0) || (ret.s_line > pos2->s_line)){
		ret.s_line=pos2->s_line;
		ret.s_col=pos2->s_col;
	}else if ((ret.s_line == pos2->s_line) && (ret.s_col > pos2->s_col)){
		ret.s_col=pos2->s_col;
	}
	if ((ret.e_line == 0) || (ret.e_line < pos2->e_line)){
		ret.e_line=pos2->e_line;
		ret.e_col=pos2->e_col;
	}else if ((ret.e_line == pos2->e_line) && (ret.e_col < pos2->e_col)){
		ret.e_col=pos2->e_col;
	}
	*res=ret;
}



struct expr* mk_exp(int op, struct expr* left, struct expr* right)
{
	struct expr * e;
	e=(struct expr*)pkg_malloc(sizeof (struct expr));
	if (e==0) goto error;
	e->type=EXP_T;
	e->op=op;
	e->l.expr=left;
	e->r.expr=right;
	return e;
error:
	LM_CRIT("memory allocation failure\n");
	return 0;
}


struct expr* mk_elem(int op, expr_l_type ltype, void* lparam,
							 expr_r_type rtype, void* rparam)
{
	struct expr * e;
	e=(struct expr*)pkg_malloc(sizeof (struct expr));
	if (e==0) goto error;
	e->type=ELEM_T;
	e->op=op;
	e->l_type=ltype;
	e->l.param=lparam;
	e->r_type = rtype;
	e->r.param=rparam;
	return e;
error:
	LM_CRIT("memory allocation failure\n");
	return 0;
}


/** create an action structure (parser use).
 * @param type - type of the action
 * @param count - count of couples {param_type,val}
 * @param ... -   count {param_type, val} pairs, where param_type is
 *                action_param_type.
 * @return  new action structure on success (pkg_malloc'ed) or 0 on error.
 */
struct action* mk_action(enum action_type type, int count, ...)
{
	va_list args;
	int i;
	struct action* a;

	a = (struct action*)pkg_malloc(sizeof(struct action));
	if (a==0) goto  error;
	memset(a, 0, sizeof(struct action));
	a->type=type;
	a->count = (count > MAX_ACTIONS)?MAX_ACTIONS:count;

	va_start(args, count);
	for (i=0; i<a->count; i++) {
		a->val[i].type = va_arg(args, int);
		a->val[i].u.data = va_arg(args, void *);

		DBG("ACTION_#%d #%d/%d: %d(%x)/ %p\n", a->type, i, a->count, a->val[i].type, a->val[i].type, a->val[i].u.data);
	}
	va_end(args);

	a->next=0;
	return a;

error:
	LM_CRIT("memory allocation failure\n");
	return 0;
}


struct action* append_action(struct action* a, struct action* b)
{
	struct action *t;
	if (b==0) return a;
	if (a==0) return b;

	for(t=a; t->next; t=t->next);
	t->next=b;
	return a;
}



void print_expr(struct expr* exp)
{
	if (exp==0){
		LM_CRIT("null expression!\n");
		return;
	}
	if (exp->type==ELEM_T){
		switch(exp->l_type){
			case METHOD_O:
				DBG("method");
				break;
			case URI_O:
				DBG("uri");
				break;
			case FROM_URI_O:
				DBG("from_uri");
				break;
			case TO_URI_O:
				DBG("to_uri");
				break;
			case SRCIP_O:
				DBG("srcip");
				break;
			case SRCPORT_O:
				DBG("srcport");
				break;
			case DSTIP_O:
				DBG("dstip");
				break;
			case DSTPORT_O:
				DBG("dstport");
				break;
			case PROTO_O:
				DBG("proto");
				break;
			case AF_O:
				DBG("af");
				break;
			case MSGLEN_O:
				DBG("msglen");
				break;
			case ACTION_O:
				break;
			case NUMBER_O:
				break;
			case AVP_O:
				DBG("avp");
				break;
			case SNDIP_O:
				DBG("sndip");
				break;
			case SNDPORT_O:
				DBG("sndport");
				break;
			case TOIP_O:
				DBG("toip");
				break;
			case TOPORT_O:
				DBG("toport");
				break;
			case SNDPROTO_O:
				DBG("sndproto");
				break;
			case SNDAF_O:
				DBG("sndaf");
				break;
			case RETCODE_O:
				DBG("retcode");
				break;
			case SELECT_O:
				DBG("select");
				break;
			case RVEXP_O:
				DBG("rval");
				break;

			default:
				DBG("UNKNOWN");
		}
		switch(exp->op){
			case EQUAL_OP:
				DBG("==");
				break;
			case MATCH_OP:
				DBG("=~");
				break;
			case NO_OP:
				break;
			case GT_OP:
				DBG(">");
				break;
			case GTE_OP:
				DBG(">=");
				break;
			case LT_OP:
				DBG("<");
				break;
			case LTE_OP:
				DBG("<=");
				break;
			case DIFF_OP:
				DBG("!=");
				break;
			default:
				DBG("<UNKNOWN>");
		}
		switch(exp->r_type){
			case NOSUBTYPE:
					DBG("N/A");
					break;
			case STRING_ST:
					DBG("\"%s\"", ZSW((char*)exp->r.param));
					break;
			case NET_ST:
					print_net((struct net*)exp->r.param);
					break;
			case IP_ST:
					print_ip("", (struct ip_addr*)exp->r.param, "");
					break;
			case ACTIONS_ST:
					print_actions((struct action*)exp->r.param);
					break;
			case NUMBER_ST:
					DBG("%ld",exp->r.numval);
					break;
			case MYSELF_ST:
					DBG("_myself_");
					break;
		        case AVP_ST:
				        DBG("attr");
			 	        break;
		        case SELECT_ST:
				        DBG("select");
				        break;
			default:
					DBG("type<%d>", exp->r_type);
		}
	}else if (exp->type==EXP_T){
		switch(exp->op){
			case LOGAND_OP:
					DBG("AND( ");
					print_expr(exp->l.expr);
					DBG(", ");
					print_expr(exp->r.expr);
					DBG(" )");
					break;
			case LOGOR_OP:
					DBG("OR( ");
					print_expr(exp->l.expr);
					DBG(", ");
					print_expr(exp->r.expr);
					DBG(" )");
					break;
			case NOT_OP:
					DBG("NOT( ");
					print_expr(exp->l.expr);
					DBG(" )");
					break;
			default:
					DBG("UNKNOWN_EXP ");
		}

	}else{
		DBG("ERROR:print_expr: unknown type\n");
	}
}




void print_action(struct action* t)
{
	switch(t->type){
		case FORWARD_T:
			DBG("forward(");
			break;
		case FORWARD_TCP_T:
			DBG("forward_tcp(");
			break;
		case FORWARD_UDP_T:
			DBG("forward_udp(");
			break;
		case DROP_T:
			DBG("drop(");
			break;
		case LOG_T:
			DBG("log(");
			break;
		case ERROR_T:
			DBG("error(");
			break;
		case ROUTE_T:
			DBG("route(");
			break;
		case EXEC_T:
			DBG("exec(");
			break;
		case REVERT_URI_T:
			DBG("revert_uri(");
			break;
		case STRIP_T:
			DBG("strip(");
			break;
		case APPEND_BRANCH_T:
			DBG("append_branch(");
			break;
		case PREFIX_T:
			DBG("prefix(");
			break;
		case LEN_GT_T:
			DBG("len_gt(");
			break;
		case SETFLAG_T:
			DBG("setflag(");
			break;
		case RESETFLAG_T:
			DBG("resetflag(");
			break;
		case ISFLAGSET_T:
			DBG("isflagset(");
			break;
		case AVPFLAG_OPER_T:
			DBG("avpflagoper(");
			break;
		case SET_HOST_T:
			DBG("sethost(");
			break;
		case SET_HOSTPORT_T:
			DBG("sethostport(");
			break;
		case SET_HOSTPORTTRANS_T:
			DBG("sethostporttrans(");
			break;
		case SET_USER_T:
			DBG("setuser(");
			break;
		case SET_USERPASS_T:
			DBG("setuserpass(");
			break;
		case SET_PORT_T:
			DBG("setport(");
			break;
		case SET_URI_T:
			DBG("seturi(");
			break;
		case IF_T:
			DBG("if (");
			break;
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
			DBG(" external_module_call(");
			break;
		case FORCE_RPORT_T:
			DBG("force_rport(");
			break;
		case SET_ADV_ADDR_T:
			DBG("set_advertised_address(");
			break;
		case SET_ADV_PORT_T:
			DBG("set_advertised_port(");
			break;
		case FORCE_TCP_ALIAS_T:
			DBG("force_tcp_alias(");
			break;
		case LOAD_AVP_T:
			DBG("load_avp(");
			break;
		case AVP_TO_URI_T:
			DBG("avp_to_attr");
			break;
		case FORCE_SEND_SOCKET_T:
			DBG("force_send_socket");
			break;
		case ASSIGN_T
	:		DBG("assign(");
			break;
		case ADD_T:
			DBG("assign_add(");
			break;
		default:
			DBG("UNKNOWN(");
	}
	switch(t->val[0].type){
		case STRING_ST:
			DBG("\"%s\"", ZSW(t->val[0].u.string));
			break;
		case NUMBER_ST:
			DBG("%lu",t->val[0].u.number);
			break;
		case IP_ST:
			print_ip("", (struct ip_addr*)t->val[0].u.data, "");
			break;
		case EXPR_ST:
			print_expr((struct expr*)t->val[0].u.data);
			break;
		case ACTIONS_ST:
			print_actions((struct action*)t->val[0].u.data);
			break;
		case MODEXP_ST:
			DBG("f_ptr<%p>",t->val[0].u.data);
			break;
		case SOCKID_ST:
			DBG("%d:%s:%d",
			((struct socket_id*)t->val[0].u.data)->proto,
			ZSW(((struct socket_id*)t->val[0].u.data)->addr_lst->name),
			((struct socket_id*)t->val[0].u.data)->port
			);
			break;
		case AVP_ST:
			DBG("avp(%u,%.*s)", t->val[0].u.attr->type, t->val[0].u.attr->name.s.len, ZSW(t->val[0].u.attr->name.s.s));
			break;
		case SELECT_ST:
			DBG("select");
			break;
		default:
			DBG("type<%d>", t->val[0].type);
	}
	if (t->type==IF_T) DBG(") {");
	switch(t->val[1].type){
		case NOSUBTYPE:
			break;
		case STRING_ST:
			DBG(", \"%s\"", ZSW(t->val[1].u.string));
			break;
		case NUMBER_ST:
			DBG(", %lu",t->val[1].u.number);
			break;
		case EXPR_ST:
			print_expr((struct expr*)t->val[1].u.data);
			break;
		case ACTION_ST:
		case ACTIONS_ST:
			print_actions((struct action*)t->val[1].u.data);
			break;

		case SOCKID_ST:
			DBG("%d:%s:%d",
			((struct socket_id*)t->val[0].u.data)->proto,
			ZSW(((struct socket_id*)t->val[0].u.data)->addr_lst->name),
			((struct socket_id*)t->val[0].u.data)->port
			);
			break;
		case AVP_ST:
			DBG(", avp(%u,%.*s)", t->val[1].u.attr->type, t->val[1].u.attr->name.s.len, ZSW(t->val[1].u.attr->name.s.s));
			break;
		case SELECT_ST:
			DBG("select");
			break;
		default:
			DBG(", type<%d>", t->val[1].type);
	}
	if (t->type==IF_T) DBG("} else {");
	switch(t->val[2].type){
		case NOSUBTYPE:
			break;
		case STRING_ST:
			DBG(", \"%s\"", ZSW(t->val[2].u.string));
			break;
		case NUMBER_ST:
			DBG(", %lu",t->val[2].u.number);
			break;
		case EXPR_ST:
			print_expr((struct expr*)t->val[2].u.data);
			break;
		case ACTIONS_ST:
			print_actions((struct action*)t->val[2].u.data);
			break;
		case SOCKID_ST:
			DBG("%d:%s:%d",
			((struct socket_id*)t->val[0].u.data)->proto,
			ZSW(((struct socket_id*)t->val[0].u.data)->addr_lst->name),
			((struct socket_id*)t->val[0].u.data)->port
			);
			break;
		default:
			DBG(", type<%d>", t->val[2].type);
	}
	if (t->type==IF_T) DBG("}; ");
		else	DBG("); ");
}

void print_actions(struct action* a)
{
	while(a) {
		print_action(a);
		a = a->next;
	}
}

/**
 * get the pointer to action structure from parameter
 */
struct action *get_action_from_param(void **param, int param_no)
{
	struct action *ac, ac2;
	action_u_t *au, au2;
	/* param points to au->u.string, get pointer to au */
	au = (void*) ((char *)param - ((char *)&au2.u.string-(char *)&au2));
	au = au - 1 - param_no;
	ac = (void*) ((char *)au - ((char *)&ac2.val-(char *)&ac2));
	return ac;
}
