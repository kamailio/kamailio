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

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

/* main routing script table  */
struct action* rlist[RT_NO];
/* reply routing table */
struct action* reply_rlist[REPLY_RT_NO];


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
					re=(regex_t*)malloc(sizeof(regex_t));
					if (re==0){
						LOG(L_CRIT, "ERROR: fix_expr: memory allocation"
								" failure\n");
						return E_OUT_OF_MEM;
					}
					if (regcomp(re, (char*) exp->r.param,
								REG_EXTENDED|REG_NOSUB|REG_ICASE) ){
						LOG(L_CRIT, "ERROR: fix_expr : bad re \"%s\"\n",
									(char*) exp->r.param);
						free(re);
						return E_BAD_RE;
					}
					/* replace the string with the re */
					free(exp->r.param);
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
	int ret,r;
	struct sr_module* mod;
	
	if (a==0){
		LOG(L_CRIT,"BUG: fix_actions: null pointer\n");
		return E_BUG;
	}
	for(t=a; t!=0; t=t->next){
		switch(t->type){
			case FORWARD_T:
			case SEND_T:
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
							p=add_proxy(t->p1.string, t->p2.number);
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
				if ((mod=find_module(t->p1.data, &r))!=0){
					DBG("fixing %s %s\n", mod->path,
							mod->exports->cmd_names[r]);
					if (mod->exports->fixup_pointers[r]){
						if (mod->exports->param_no[r]>0){
							ret=mod->exports->fixup_pointers[r](&t->p2.data,
																1);
							t->p2_type=MODFIXUP_ST;
							if (ret<0) return ret;
						}
						if (mod->exports->param_no[r]>1){
							ret=mod->exports->fixup_pointers[r](&t->p3.data,
																2);
							t->p3_type=MODFIXUP_ST;
							if (ret<0) return ret;
						}
					}
				}
			
		}
	}
	return 0;
}



/* eval_elem helping function, returns str op param */
static int comp_str(char* str, void* param, int op, int subtype)
{
	int ret;
	
	ret=-1;
	if (op==EQUAL_OP){
		if (subtype!=STRING_ST){
			LOG(L_CRIT, "BUG: comp_str: bad type %d, "
					"string expected\n", subtype);
			goto error;
		}
		ret=(strcasecmp(str, (char*)param)==0);
	}else if (op==MATCH_OP){
		if (subtype!=RE_ST){
			LOG(L_CRIT, "BUG: comp_str: bad type %d, "
					" RE expected\n", subtype);
			goto error;
		}
		ret=(regexec((regex_t*)param, str, 0, 0, 0)==0);
	}else{
		LOG(L_CRIT, "BUG: comp_str: unknown op %d\n", op);
		goto error;
	}
	return ret;
	
error:
	return -1;
}



/* eval_elem helping function, returns an op param */
static int comp_ip(struct ip_addr* ip, void* param, int op, int subtype)
{
	struct hostent* he;
	char ** h;
	int ret;
	str tmp;

	ret=-1;
	switch(subtype){
		case NET_ST:
			ret=matchnet(ip, (struct net*) param);
			/*ret=(a&((struct net*)param)->mask)==((struct net*)param)->ip;*/
			break;
		case STRING_ST:
		case RE_ST:
			/* 1: compare with ip2str*/
		/* !!!??? review reminder ( resolve(name) & compare w/ all ips? */
#if 0
			ret=comp_str(inet_ntoa(*(struct in_addr*)&a), param, op,
						subtype);
			if (ret==1) break;
#endif
			/* 2: (slow) rev dns the address
			 * and compare with all the aliases */
			he=rev_resolvehost(ip);
			if (he==0){
				DBG( "comp_ip: could not rev_resolve ip address: ");
				print_ip(ip);
				DBG("\n");
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
		case MYSELF_ST: /* check if it's one of our addresses*/
			tmp.s=ip_addr2a(ip);
			tmp.len=strlen(tmp.s);
			ret=check_self(&tmp, 0);
			break;
		default:
			LOG(L_CRIT, "BUG: comp_ip: invalid type for "
						" src_ip or dst_ip (%d)\n", subtype);
			ret=-1;
	}
	return ret;
	
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
				ret=comp_str(msg->first_line.u.request.method.s, e->r.param,
								e->op, e->subtype);
				break;
		case URI_O:
				if(msg->new_uri.s){
					if (e->subtype==MYSELF_ST){
						if (parse_sip_msg_uri(msg)<0) ret=-1;
						else	ret=check_self(&msg->parsed_uri.host,
									msg->parsed_uri.port_no?
									msg->parsed_uri.port_no:SIP_PORT);
					}else{
						ret=comp_str(msg->new_uri.s, e->r.param,
										e->op, e->subtype);
					}
				}else{
					if (e->subtype==MYSELF_ST){
						if (parse_sip_msg_uri(msg)<0) ret=-1;
						else	ret=check_self(&msg->parsed_uri.host,
									msg->parsed_uri.port_no?
									msg->parsed_uri.port_no:SIP_PORT);
					}else{
						ret=comp_str(msg->first_line.u.request.uri.s,
										 e->r.param, e->op, e->subtype);
					}
				}
				break;
		case SRCIP_O:
				ret=comp_ip(&msg->src_ip, e->r.param, e->op, e->subtype);
				break;
		case DSTIP_O:
				ret=comp_ip(&msg->dst_ip, e->r.param, e->op, e->subtype);
				break;
		case NUMBER_O:
				ret=!(!e->r.intval); /* !! to transform it in {0,1} */
				break;
		case ACTION_O:
				ret=run_actions( (struct action*)e->r.param, msg);
				if (ret<=0) ret=(ret==0)?EXPR_DROP:0;
				else ret=1;
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
	for(i=0;i<REPLY_RT_NO;i++){
		if(reply_rlist[i]){
			if ((ret=fix_actions(reply_rlist[i]))!=0){
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
	for(j=0; j<REPLY_RT_NO; j++){
		if (reply_rlist[j]==0){
			if (j==0) DBG("WARNING: the main reply routing table is empty\n");
			continue;
		}
		DBG("routing table %d:\n",j);
		for (t=reply_rlist[j],i=0; t; i++, t=t->next){
			print_action(t);
		}
		DBG("\n");
	}
}


