/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */
/* History:
 * --------
 *
 *  2003-04-12  FORCE_RPORT_T added (andrei)
 *  2003-04-22  strip_tail added (jiri)
 *  2003-10-10  >,<,>=,<=, != and MSGLEN_O added (andrei)
 *  2003-10-28  FORCE_TCP_ALIAS added (andrei)
 *  2004-02-24  added LOAD_AVP_T and AVP_TO_URI_T (bogdan)
 */


#ifndef route_struct_h
#define route_struct_h

#define EXPR_DROP -127  /* used only by the expression and if evaluator */
/*
 * Other important values (no macros for them yet):
 * expr true = 1
 * expr false = 0 (used only inside the expression and if evaluator)
 * 
 * action continue  or if used in condition true = 1
 * action drop/quit/stop script processing = 0
 * action error or if used in condition false = -1 (<0 and !=EXPR_DROP)
 * 
 */


enum { EXP_T=1, ELEM_T };
enum { AND_OP=1, OR_OP, NOT_OP };
enum { EQUAL_OP=10, MATCH_OP, GT_OP, LT_OP, GTE_OP, LTE_OP, DIFF_OP, NO_OP };
enum { METHOD_O=1, URI_O, SRCIP_O, SRCPORT_O,
	   DSTIP_O, DSTPORT_O, PROTO_O, AF_O, MSGLEN_O, DEFAULT_O, ACTION_O,
	   NUMBER_O};

enum { FORWARD_T=1, SEND_T, DROP_T, LOG_T, ERROR_T, ROUTE_T, EXEC_T,
		SET_HOST_T, SET_HOSTPORT_T, SET_USER_T, SET_USERPASS_T, 
		SET_PORT_T, SET_URI_T, IF_T, MODULE_T,
		SETFLAG_T, RESETFLAG_T, ISFLAGSET_T ,
		LEN_GT_T, PREFIX_T, STRIP_T,STRIP_TAIL_T,
		APPEND_BRANCH_T,
		REVERT_URI_T,
		FORWARD_TCP_T,
		FORWARD_UDP_T,
		FORWARD_TLS_T,
		SEND_TCP_T,
		FORCE_RPORT_T,
		SET_ADV_ADDR_T,
		SET_ADV_PORT_T,
		FORCE_TCP_ALIAS_T,
		LOAD_AVP_T,
		AVP_TO_URI_T
};
enum { NOSUBTYPE=0, STRING_ST, NET_ST, NUMBER_ST, IP_ST, RE_ST, PROXY_ST,
		EXPR_ST, ACTIONS_ST, CMDF_ST, MODFIXUP_ST, URIHOST_ST, URIPORT_ST,
		MYSELF_ST, STR_ST };

	
struct expr{
	int type; /* exp, exp_elem */
	int op; /* and, or, not | ==,  =~ */
	int  subtype;
	union {
		struct expr* expr;
		int operand;
	}l;
	union {
		struct expr* expr;
		void* param;
		int   intval;
	}r;
};


struct action{
	int type;  /* forward, drop, log, send ...*/
	int p1_type;
	int p2_type;
	int p3_type;
	union {
		long number;
		char* string;
		void* data;
	}p1, p2, p3;
	struct action* next;
};



struct expr* mk_exp(int op, struct expr* left, struct expr* right);
struct expr* mk_elem(int op, int subtype, int operand, void* param);
struct action* mk_action(int type, int p1_type, int p2_type,
							void* p1, void* p2);
struct action* mk_action3(int type, int p1_type, int p2_type, int p3_type, 
							void* p1, void* p2, void* p3);
struct action* append_action(struct action* a, struct action* b);


void print_action(struct action* a);
void print_expr(struct expr* exp);





#endif

