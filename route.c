/*
 * $Id$
 *
 * SIP routing engine
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *  2003-01-28  scratchpad removed, src_port introduced (jiri)
 *  2003-02-28  scratchpad compatibility abandoned (jiri)
 *  2003-03-10  updated to the new module exports format (andrei)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-01  added dst_port, proto, af; renamed comp_port to comp_no,
 *               inlined all the comp_* functions (andrei)
 *  2003-04-05  s/reply_route/failure_route, onreply_route introduced (jiri)
 *  2003-05-23  comp_ip fixed, now it will resolve its operand and compare
 *              the ip with all the addresses (andrei)
 *  2003-10-10  added more operators support to comp_* (<,>,<=,>=,!=) (andrei)
 */

 
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "route.h"
#include "forward.h"
#include "dprint.h"
#include "proxy.h"
#include "action.h"
#include "sr_module.h"
#include "ip_addr.h"
#include "resolve.h"
#include "parser/parse_uri.h"
#include "mem/mem.h"


/* main routing script table  */
struct action* rlist[RT_NO];
/* reply routing table */
struct action* onreply_rlist[ONREPLY_RT_NO];
struct action* failure_rlist[FAILURE_RT_NO];


static int fix_actions(struct action* a); /*fwd declaration*/


/* traverses an expr tree and compiles the REs where necessary) 
 * returns: 0 for ok, <0 if errors */
static int fix_expr(struct expr* exp)
{
	regex_t* re;
	int ret;
	
	ret=E_BUG;
	if (exp==0){
		LOG(L_CRIT, "BUG: fix_expr: null pointer\n");
		return E_BUG;
	}
	if (exp->type==EXP_T){
		switch(exp->op){
			case AND_OP:
			case OR_OP:
						if ((ret=fix_expr(exp->l.expr))!=0)
							return ret;
						ret=fix_expr(exp->r.expr);
						break;
			case NOT_OP:
						ret=fix_expr(exp->l.expr);
						break;
			default:
						LOG(L_CRIT, "BUG: fix_expr: unknown op %d\n",
								exp->op);
		}
	}else if (exp->type==ELEM_T){
			if (exp->op==MATCH_OP){
				if (exp->subtype==STRING_ST){
					re=(regex_t*)pkg_malloc(sizeof(regex_t));
					if (re==0){
						LOG(L_CRIT, "ERROR: fix_expr: memory allocation"
								" failure\n");
						return E_OUT_OF_MEM;
					}
					if (regcomp(re, (char*) exp->r.param,
								REG_EXTENDED|REG_NOSUB|REG_ICASE) ){
						LOG(L_CRIT, "ERROR: fix_expr : bad re \"%s\"\n",
									(char*) exp->r.param);
						pkg_free(re);
						return E_BAD_RE;
					}
					/* replace the string with the re */
					pkg_free(exp->r.param);
					exp->r.param=re;
					exp->subtype=RE_ST;
				}else if (exp->subtype!=RE_ST){
					LOG(L_CRIT, "BUG: fix_expr : invalid type for match\n");
					return E_BUG;
				}
			}
			if (exp->l.operand==ACTION_O){
				ret=fix_actions((struct action*)exp->r.param);
				if (ret!=0){
					LOG(L_CRIT, "ERROR: fix_expr : fix_actions error\n");
					return ret;
				}
			}
			ret=0;
	}
	return ret;
}



/* adds the proxies in the proxy list & resolves the hostnames */
/* returns 0 if ok, <0 on error */
static int fix_actions(struct action* a)
{
	struct action *t;
	struct proxy_l* p;
	char *tmp;
	int ret;
	cmd_export_t* cmd;
	struct sr_module* mod;
	str s;
	
	if (a==0){
		LOG(L_CRIT,"BUG: fix_actions: null pointer\n");
		return E_BUG;
	}
	for(t=a; t!=0; t=t->next){
		switch(t->type){
			case FORWARD_T:
			case FORWARD_TLS_T:
			case FORWARD_TCP_T:
			case FORWARD_UDP_T:
			case SEND_T:
			case SEND_TCP_T:
					switch(t->p1_type){
						case IP_ST: 
							tmp=strdup(ip_addr2a(
										(struct ip_addr*)t->p1.data));
							if (tmp==0){
								LOG(L_CRIT, "ERROR: fix_actions:"
										"memory allocation failure\n");
								return E_OUT_OF_MEM;
							}
							t->p1_type=STRING_ST;
							t->p1.string=tmp;
							/* no break */
						case STRING_ST:
							s.s = t->p1.string;
							s.len = strlen(s.s);
							p=add_proxy(&s, t->p2.number, 0); /* FIXME proto*/
							if (p==0) return E_BAD_ADDRESS;
							t->p1.data=p;
							t->p1_type=PROXY_ST;
							break;
						case URIHOST_ST:
							break;
						default:
							LOG(L_CRIT, "BUG: fix_actions: invalid type"
									"%d (should be string or number)\n",
										t->type);
							return E_BUG;
					}
					break;
			case IF_T:
				if (t->p1_type!=EXPR_ST){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if (should be expr)\n",
								t->p1_type);
					return E_BUG;
				}else if( (t->p2_type!=ACTIONS_ST)&&(t->p2_type!=NOSUBTYPE) ){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if() {...} (should be action)\n",
								t->p2_type);
					return E_BUG;
				}else if( (t->p3_type!=ACTIONS_ST)&&(t->p3_type!=NOSUBTYPE) ){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if() {} else{...}(should be action)\n",
								t->p3_type);
					return E_BUG;
				}
				if (t->p1.data){
					if ((ret=fix_expr((struct expr*)t->p1.data))<0)
						return ret;
				}
				if ( (t->p2_type==ACTIONS_ST)&&(t->p2.data) ){
					if ((ret=fix_actions((struct action*)t->p2.data))<0)
						return ret;
				}
				if ( (t->p3_type==ACTIONS_ST)&&(t->p3.data) ){
						if ((ret=fix_actions((struct action*)t->p3.data))<0)
						return ret;
				}
				break;
			case MODULE_T:
				if ((mod=find_module(t->p1.data, &cmd))!=0){
					DBG("fixing %s %s\n", mod->path, cmd->name);
					if (cmd->fixup){
						if (cmd->param_no>0){
							ret=cmd->fixup(&t->p2.data, 1);
							t->p2_type=MODFIXUP_ST;
							if (ret<0) return ret;
						}
						if (cmd->param_no>1){
							ret=cmd->fixup(&t->p3.data, 2);
							t->p3_type=MODFIXUP_ST;
							if (ret<0) return ret;
						}
					}
				}
			
		}
	}
	return 0;
}


inline static int comp_no( int port, void *param, int op, int subtype )
{
	
	if (subtype!=NUMBER_ST) {
		LOG(L_CRIT, "BUG: comp_no: number expected: %d\n", subtype );
		return E_BUG;
	}
	switch (op){
		case EQUAL_OP:
			return port==(long)param;
		case DIFF_OP:
			return port!=(long)param;
		case GT_OP:
			return port>(long)param;
		case LT_OP:
			return port<(long)param;
		case GTE_OP:
			return port>=(long)param;
		case LTE_OP:
			return port<=(long)param;
		default:
		LOG(L_CRIT, "BUG: comp_no: unknown operator: %d\n", op );
		return E_BUG;
	}
}

/* eval_elem helping function, returns str op param */
inline static int comp_strstr(str* str, void* param, int op, int subtype)
{
	int ret;
	char backup;
	
	ret=-1;
	switch(op){
		case EQUAL_OP:
			if (subtype!=STRING_ST){
				LOG(L_CRIT, "BUG: comp_str: bad type %d, "
						"string expected\n", subtype);
				goto error;
			}
			ret=(strncasecmp(str->s, (char*)param, str->len)==0);
			break;
		case DIFF_OP:
			if (subtype!=STRING_ST){
				LOG(L_CRIT, "BUG: comp_str: bad type %d, "
						"string expected\n", subtype);
				goto error;
			}
			ret=(strncasecmp(str->s, (char*)param, str->len)!=0);
			break;
		case MATCH_OP:
			if (subtype!=RE_ST){
				LOG(L_CRIT, "BUG: comp_str: bad type %d, "
						" RE expected\n", subtype);
				goto error;
			}
		/* this is really ugly -- we put a temporary zero-terminating
		 * character in the original string; that's because regexps
         * take 0-terminated strings and our messages are not
         * zero-terminated; it should not hurt as long as this function
		 * is applied to content of pkg mem, which is always the case
		 * with calls from route{}; the same goes for fline in reply_route{};
         *
         * also, the received function should always give us an extra
         * character, into which we can put the 0-terminator now;
         * an alternative would be allocating a new piece of memory,
         * which might be too slow
         * -jiri
         */
			backup=str->s[str->len];str->s[str->len]=0;
			ret=(regexec((regex_t*)param, str->s, 0, 0, 0)==0);
			str->s[str->len]=backup;
			break;
		default:
			LOG(L_CRIT, "BUG: comp_str: unknown op %d\n", op);
			goto error;
	}
	return ret;
	
error:
	return -1;
}

/* eval_elem helping function, returns str op param */
inline static int comp_str(char* str, void* param, int op, int subtype)
{
	int ret;
	
	ret=-1;
	switch(op){
		case EQUAL_OP:
			if (subtype!=STRING_ST){
				LOG(L_CRIT, "BUG: comp_str: bad type %d, "
						"string expected\n", subtype);
				goto error;
			}
			ret=(strcasecmp(str, (char*)param)==0);
			break;
		case DIFF_OP:
			if (subtype!=STRING_ST){
				LOG(L_CRIT, "BUG: comp_str: bad type %d, "
						"string expected\n", subtype);
				goto error;
			}
			ret=(strcasecmp(str, (char*)param)!=0);
			break;
		case MATCH_OP:
			if (subtype!=RE_ST){
				LOG(L_CRIT, "BUG: comp_str: bad type %d, "
						" RE expected\n", subtype);
				goto error;
			}
			ret=(regexec((regex_t*)param, str, 0, 0, 0)==0);
			break;
		default:
			LOG(L_CRIT, "BUG: comp_str: unknown op %d\n", op);
			goto error;
	}
	return ret;
	
error:
	return -1;
}


/* check_self wrapper -- it checks also for the op */
inline static int check_self_op(int op, str* s, unsigned short p)
{
	int ret;
	
	ret=check_self(s, p, 0);
	switch(op){
		case EQUAL_OP:
			break;
		case DIFF_OP:
			if (ret>=0) ret=!ret;
			break;
		default:
			LOG(L_CRIT, "BUG: check_self_op: invalid operator %d\n", op);
			ret=-1;
	}
	return ret;
}


/* eval_elem helping function, returns an op param */
inline static int comp_ip(struct ip_addr* ip, void* param, int op, int subtype)
{
	struct hostent* he;
	char ** h;
	int ret;
	str tmp;

	ret=-1;
	switch(subtype){
		case NET_ST:
			switch(op){
				case EQUAL_OP:
					ret=(matchnet(ip, (struct net*) param)==1);
					break;
				case DIFF_OP:
					ret=(matchnet(ip, (struct net*) param)!=1);
					break;
				default:
					goto error_op;
			}
			break;
		case STRING_ST:
		case RE_ST:
			switch(op){
				case EQUAL_OP:
				case MATCH_OP:
					/* 1: compare with ip2str*/
					ret=comp_str(ip_addr2a(ip), param, op, subtype);
					if (ret==1) break;
					/* 2: resolve (name) & compare w/ all the ips */
					if (subtype==STRING_ST){
						he=resolvehost((char*)param);
						if (he==0){
							DBG("comp_ip: could not resolve %s\n",
									(char*)param);
						}else if (he->h_addrtype==ip->af){
							for(h=he->h_addr_list;(ret!=1)&& (*h); h++){
								ret=(memcmp(ip->u.addr, *h, ip->len)==0);
							}
							if (ret==1) break;
						}
					}
					/* 3: (slow) rev dns the address
					* and compare with all the aliases
					* !!??!! review: remove this? */
					he=rev_resolvehost(ip);
					if (he==0){
						print_ip( "comp_ip: could not rev_resolve ip address:"
									" ", ip, "\n");
					ret=0;
					}else{
						/*  compare with primary host name */
						ret=comp_str(he->h_name, param, op, subtype);
						/* compare with all the aliases */
						for(h=he->h_aliases; (ret!=1) && (*h); h++){
							ret=comp_str(*h, param, op, subtype);
						}
					}
					break;
				case DIFF_OP:
					ret=comp_ip(ip, param, EQUAL_OP, subtype);
					if (ret>=0) ret=!ret;
					break;
				default:
					goto error_op;
			}
			break;
		case MYSELF_ST: /* check if it's one of our addresses*/
			tmp.s=ip_addr2a(ip);
			tmp.len=strlen(tmp.s);
			ret=check_self_op(op, &tmp, 0);
			break;
		default:
			LOG(L_CRIT, "BUG: comp_ip: invalid type for "
						" src_ip or dst_ip (%d)\n", subtype);
			ret=-1;
	}
	return ret;
error_op:
	LOG(L_CRIT, "BUG: comp_ip: invalid operator %d\n", op);
	return -1;
	
}



/* returns: 0/1 (false/true) or -1 on error, -127 EXPR_DROP */
static int eval_elem(struct expr* e, struct sip_msg* msg)
{

	int ret;
	ret=E_BUG;
	
	if (e->type!=ELEM_T){
		LOG(L_CRIT," BUG: eval_elem: invalid type\n");
		goto error;
	}
	switch(e->l.operand){
		case METHOD_O:
				ret=comp_strstr(&msg->first_line.u.request.method, e->r.param,
								e->op, e->subtype);
				break;
		case URI_O:
				if(msg->new_uri.s){
					if (e->subtype==MYSELF_ST){
						if (parse_sip_msg_uri(msg)<0) ret=-1;
						else	ret=check_self_op(e->op, &msg->parsed_uri.host,
									msg->parsed_uri.port_no?
									msg->parsed_uri.port_no:SIP_PORT);
					}else{
						ret=comp_strstr(&msg->new_uri, e->r.param,
										e->op, e->subtype);
					}
				}else{
					if (e->subtype==MYSELF_ST){
						if (parse_sip_msg_uri(msg)<0) ret=-1;
						else	ret=check_self_op(e->op, &msg->parsed_uri.host,
									msg->parsed_uri.port_no?
									msg->parsed_uri.port_no:SIP_PORT);
					}else{
						ret=comp_strstr(&msg->first_line.u.request.uri,
										 e->r.param, e->op, e->subtype);
					}
				}
				break;
		case SRCIP_O:
				ret=comp_ip(&msg->rcv.src_ip, e->r.param, e->op, e->subtype);
				break;
		case DSTIP_O:
				ret=comp_ip(&msg->rcv.dst_ip, e->r.param, e->op, e->subtype);
				break;
		case NUMBER_O:
				ret=!(!e->r.intval); /* !! to transform it in {0,1} */
				break;
		case ACTION_O:
				ret=run_actions( (struct action*)e->r.param, msg);
				if (ret<=0) ret=(ret==0)?EXPR_DROP:0;
				else ret=1;
				break;
		case SRCPORT_O:
				ret=comp_no(msg->rcv.src_port, 
					e->r.param, /* e.g., 5060 */
					e->op, /* e.g. == */
					e->subtype /* 5060 is number */);
				break;
		case DSTPORT_O:
				ret=comp_no(msg->rcv.dst_port, e->r.param, e->op, 
							e->subtype);
				break;
		case PROTO_O:
				ret=comp_no(msg->rcv.proto, e->r.param, e->op, e->subtype);
				break;
		case AF_O:
				ret=comp_no(msg->rcv.src_ip.af, e->r.param, e->op, e->subtype);
				break;
		case MSGLEN_O:
				ret=comp_no(msg->len, e->r.param, e->op, e->subtype);
				break;
		default:
				LOG(L_CRIT, "BUG: eval_elem: invalid operand %d\n",
							e->l.operand);
	}
	return ret;
error:
	return -1;
}



/* ret= 0/1 (true/false) ,  -1 on error or EXPR_DROP (-127)  */
int eval_expr(struct expr* e, struct sip_msg* msg)
{
	static int rec_lev=0;
	int ret;
	
	rec_lev++;
	if (rec_lev>MAX_REC_LEV){
		LOG(L_CRIT, "ERROR: eval_expr: too many expressions (%d)\n",
				rec_lev);
		ret=-1;
		goto skip;
	}
	
	if (e->type==ELEM_T){
		ret=eval_elem(e, msg);
	}else if (e->type==EXP_T){
		switch(e->op){
			case AND_OP:
				ret=eval_expr(e->l.expr, msg);
				/* if error or false stop evaluating the rest */
				if (ret!=1) break;
				ret=eval_expr(e->r.expr, msg); /*ret1 is 1*/
				break;
			case OR_OP:
				ret=eval_expr(e->l.expr, msg);
				/* if true or error stop evaluating the rest */
				if (ret!=0) break;
				ret=eval_expr(e->r.expr, msg); /* ret1 is 0 */
				break;
			case NOT_OP:
				ret=eval_expr(e->l.expr, msg);
				if (ret<0) break;
				ret= ! ret;
				break;
			default:
				LOG(L_CRIT, "BUG: eval_expr: unknown op %d\n", e->op);
				ret=-1;
		}
	}else{
		LOG(L_CRIT, "BUG: eval_expr: unknown type %d\n", e->type);
		ret=-1;
	}

skip:
	rec_lev--;
	return ret;
}


/* adds an action list to head; a must be null terminated (last a->next=0))*/
void push(struct action* a, struct action** head)
{
	struct action *t;
	if (*head==0){
		*head=a;
		return;
	}
	for (t=*head; t->next;t=t->next);
	t->next=a;
}




int add_actions(struct action* a, struct action** head)
{
	int ret;

	LOG(L_DBG, "add_actions: fixing actions...\n");
	if ((ret=fix_actions(a))!=0) goto error;
	push(a,head);
	return 0;
	
error:
	return ret;
}



/* fixes all action tables */
/* returns 0 if ok , <0 on error */
int fix_rls()
{
	int i,ret;
	for(i=0;i<RT_NO;i++){
		if(rlist[i]){
			if ((ret=fix_actions(rlist[i]))!=0){
				return ret;
			}
		}
	}
	for(i=0;i<ONREPLY_RT_NO;i++){
		if(onreply_rlist[i]){
			if ((ret=fix_actions(onreply_rlist[i]))!=0){
				return ret;
			}
		}
	}
	for(i=0;i<FAILURE_RT_NO;i++){
		if(failure_rlist[i]){
			if ((ret=fix_actions(failure_rlist[i]))!=0){
				return ret;
			}
		}
	}
	return 0;
}


/* debug function, prints main routing table */
void print_rl()
{
	struct action* t;
	int i,j;

	for(j=0; j<RT_NO; j++){
		if (rlist[j]==0){
			if (j==0) DBG("WARNING: the main routing table is empty\n");
			continue;
		}
		DBG("routing table %d:\n",j);
		for (t=rlist[j],i=0; t; i++, t=t->next){
			print_action(t);
		}
		DBG("\n");
	}
	for(j=0; j<ONREPLY_RT_NO; j++){
		if (onreply_rlist[j]==0){
			continue;
		}
		DBG("onreply routing table %d:\n",j);
		for (t=onreply_rlist[j],i=0; t; i++, t=t->next){
			print_action(t);
		}
		DBG("\n");
	}
	for(j=0; j<FAILURE_RT_NO; j++){
		if (failure_rlist[j]==0){
			continue;
		}
		DBG("failure routing table %d:\n",j);
		for (t=failure_rlist[j],i=0; t; i++, t=t->next){
			print_action(t);
		}
		DBG("\n");
	}
}


