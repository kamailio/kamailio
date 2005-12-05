/*
 * $Id$
 *
 * SIP routing engine
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
 *  2004-10-19  added from_uri & to_uri (andrei)
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
#include "socket_info.h"
#include "parser/parse_uri.h"
#include "parser/parse_from.h"
#include "parser/parse_to.h"
#include "mem/mem.h"


/* main routing script table  */
struct action* rlist[RT_NO];
/* reply routing table */
struct action* onreply_rlist[ONREPLY_RT_NO];
struct action* failure_rlist[FAILURE_RT_NO];
struct action* branch_rlist[BRANCH_RT_NO];

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
			case LOGAND_OP:
			case LOGOR_OP:
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
				     /* right side either has to be string, in which case
				      * we turn it into regular expression, or it is regular
				      * expression already. In that case we do nothing
				      */
				if (exp->r_type==STRING_ST){
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
					exp->r.re=re;
					exp->r_type=RE_ST;
				}else if (exp->r_type!=RE_ST && exp->r_type != AVP_ST){
					LOG(L_CRIT, "BUG: fix_expr : invalid type for match\n");
					return E_BUG;
				}
			}
			if (exp->l_type==ACTION_O){
				ret=fix_actions((struct action*)exp->r.param);
				if (ret!=0){
					LOG(L_CRIT, "ERROR: fix_expr : fix_actions error\n");
					return ret;
				}
			}
			     /* Calculate lengths of strings */
			if (exp->l_type==STRING_ST) {
				int len;
				if (exp->l.string) len = strlen(exp->l.string);
				else len = 0;
				exp->l.str.s = exp->l.string;
				exp->l.str.len = len;
			}
			if (exp->r_type==STRING_ST) {
				int len;
				if (exp->r.string) len = strlen(exp->r.string);
				else len = 0;
				exp->r.str.s = exp->r.string;
				exp->r.str.len = len;
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
	struct hostent* he;
	struct ip_addr ip;
	struct socket_info* si;
	
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

		        case ASSIGN_T:
		        case ADD_T:
				if (t->p1_type != AVP_ST) {
					LOG(L_CRIT, "BUG: fix_actions: Invalid left side of assignment\n");
					return E_BUG;
				}
				if (t->p1.attr->type & AVP_CLASS_DOMAIN) {
					LOG(L_ERR, "ERROR: You cannot change domain attributes from the script, they are read-only\n");
					return E_BUG;
				} else if (t->p1.attr->type & AVP_CLASS_GLOBAL) {
					LOG(L_ERR, "ERROR: You cannot change global attributes from the script, they are read-only\n");
					return E_BUG;
				}

				if (t->p2_type == ACTION_ST && t->p2.data) {
					if ((ret = fix_actions((struct action*)t->p2.data)) < 0) {
						return ret;
					}
				} else if (t->p2_type == EXPR_ST && t->p2.data) {
					if ((ret = fix_expr((struct expr*)t->p2.data)) < 0) {
						return ret;
					}
				} else if (t->p2_type == STRING_ST) {
					int len;
					len = strlen(t->p2.data);
					t->p2.str.s = t->p2.data;
					t->p2.str.len = len;
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
				break;
			case FORCE_SEND_SOCKET_T:
				if (t->p1_type!=SOCKID_ST){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for force_send_socket\n",
								t->p1_type);
					return E_BUG;
				}
				he=resolvehost(((struct socket_id*)t->p1.data)->name);
				if (he==0){
					LOG(L_ERR, "ERROR: fix_actions: force_send_socket:"
								" could not resolve %s\n",
							((struct socket_id*)t->p1.data)->name);
					return E_BAD_ADDRESS;
				}
				hostent2ip_addr(&ip, he, 0);
				si=find_si(&ip, ((struct socket_id*)t->p1.data)->port,
								((struct socket_id*)t->p1.data)->proto);
				if (si==0){
					LOG(L_ERR, "ERROR: fix_actions: bad force_send_socket"
							" argument: %s:%d (ser doesn't listen on it)\n",
							((struct socket_id*)t->p1.data)->name,
							((struct socket_id*)t->p1.data)->port);
					return E_BAD_ADDRESS;
				}
				t->p1.data=si;
				t->p1_type=SOCKETINFO_ST;
				break;
		}
	}
	return 0;
}


/* Compare parameters as ordinary numbers
 *
 * Left and right operands can be either numbers or
 * attributes. If either of the attributes if of string type then the length of
 * its value will be used.
 */
inline static int comp_num(int op, long left, int rtype, union exp_op* r)
{
	int_str val;
	avp_t* avp;
	long right;
	
	if (rtype == AVP_ST) {
		avp = search_first_avp(r->attr->type, r->attr->name, &val, 0);
		if (avp && !(avp->flags & AVP_VAL_STR)) right = val.n;
		else return 0; /* Always fail */
	} else if (rtype == NUMBER_ST) {
		right = r->intval;
	} else {
		LOG(L_CRIT, "BUG: comp_num: Invalid right operand (%d)\n", rtype);
		return E_BUG;
	}

	switch (op){
	case EQUAL_OP: return (long)left == (long)right;
	case DIFF_OP:  return (long)left != (long)right;
	case GT_OP:    return (long)left >  (long)right;
	case LT_OP:    return (long)left <  (long)right;
	case GTE_OP:   return (long)left >= (long)right;
	case LTE_OP:   return (long)left <= (long)right;
	default:
		LOG(L_CRIT, "BUG: comp_num: unknown operator: %d\n", op);
		return E_BUG;
	}
}

/*
 * Compare given string "left" with right side of expression
 */
inline static int comp_str(int op, str* left, int rtype, union exp_op* r)
{
	str* right;
	int_str val;
	avp_t* avp;
	int ret;
	char backup;
	regex_t* re;
	
	right=0; /* warning fix */
	
	if (rtype == AVP_ST) {
		avp = search_first_avp(r->attr->type, r->attr->name, &val, 0);
		if (avp && (avp->flags & AVP_VAL_STR)) right = &val.s;
		else return 0;
	} else if ((op == MATCH_OP && rtype == RE_ST)) {
	} else if (op != MATCH_OP && rtype == STRING_ST) {
		right = &r->str;
	} else {
		LOG(L_CRIT, "BUG: comp_str: Bad type %d, "
		    "string or RE expected\n", rtype);
		goto error;
	}

	ret=-1;
	switch(op){
		case EQUAL_OP:
			if (left->len != right->len) return 0;
			ret=(strncasecmp(left->s, right->s, left->len)==0);
			break;
		case DIFF_OP:
			if (left->len != right->len) return 1;
			ret = (strncasecmp(left->s, right->s, left->len)!=0);
			break;
		case MATCH_OP:
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
			      *
			      * janakj: AVPs are zero terminated too so this is not problem either
			      */
			backup=left->s[left->len];
			left->s[left->len]='\0';
			if (rtype == AVP_ST) {
				     /* For AVPs we need to compile the RE on the fly */
				re=(regex_t*)pkg_malloc(sizeof(regex_t));
				if (re==0){
					LOG(L_CRIT, "ERROR: comp_strstr: memory allocation"
					    " failure\n");
					left->s[left->len] = backup;
					goto error;
				}
				if (regcomp(re, right->s, REG_EXTENDED|REG_NOSUB|REG_ICASE)) {
					pkg_free(re);
					left->s[left->len] = backup;
					goto error;
				}				
				ret=(regexec(re, left->s, 0, 0, 0)==0);
				regfree(re);
				pkg_free(re);
			} else {
				ret=(regexec(r->re, left->s, 0, 0, 0)==0);
			}
			left->s[left->len] = backup;
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
inline static int comp_string(int op, char* left, int rtype, union exp_op* r)
{
	int ret;
	
	ret=-1;
	switch(op){
		case EQUAL_OP:
			if (rtype!=STRING_ST){
				LOG(L_CRIT, "BUG: comp_string: bad type %d, "
						"string expected\n", rtype);
				goto error;
			}
			ret=(strcasecmp(left, r->str.s)==0);
			break;
		case DIFF_OP:
			if (rtype!=STRING_ST){
				LOG(L_CRIT, "BUG: comp_string: bad type %d, "
						"string expected\n", rtype);
				goto error;
			}
			ret=(strcasecmp(left, r->str.s)!=0);
			break;
		case MATCH_OP:
			if (rtype!=RE_ST){
				LOG(L_CRIT, "BUG: comp_string: bad type %d, "
						" RE expected\n", rtype);
				goto error;
			}
			ret=(regexec(r->re, left, 0, 0, 0)==0);
			break;
		default:
			LOG(L_CRIT, "BUG: comp_string: unknown op %d\n", op);
			goto error;
	}
	return ret;
	
error:
	return -1;
}


inline static int comp_avp(int op, avp_spec_t* spec, int rtype, union exp_op* r)
{
	avp_t* avp;
	int_str val;

	avp = search_first_avp(spec->type, spec->name, &val, 0);
	if (!avp) return 0;

	switch(op) {
	case NO_OP:
		if (avp->flags & AVP_VAL_STR) {
			return val.s.len;
		} else {
			return val.n != 0;
		}
		break;

	case BINOR_OP:
		return val.n | r->intval;
		break;

	case BINAND_OP:
		return val.n & r->intval;
		break;
	}

	if (avp->flags & AVP_VAL_STR) {
		return comp_str(op, &val.s, rtype, r);
	} else {
		return comp_num(op, val.n, rtype, r);
	}
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
inline static int comp_ip(int op, struct ip_addr* ip, int rtype, union exp_op* r)
{
	struct hostent* he;
	char ** h;
	int ret;
	str tmp;

	ret=-1;
	switch(rtype){
		case NET_ST:
			switch(op){
				case EQUAL_OP:
					ret=(matchnet(ip, r->net)==1);
					break;
				case DIFF_OP:
					ret=(matchnet(ip, r->net)!=1);
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
					ret=comp_string(op, ip_addr2a(ip), rtype, r);
					if (ret==1) break;
					/* 2: resolve (name) & compare w/ all the ips */
					if (rtype==STRING_ST){
						he=resolvehost(r->str.s);
						if (he==0){
							DBG("comp_ip: could not resolve %s\n",
							    r->str.s);
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
						ret=comp_string(op, he->h_name, rtype, r);
						/* compare with all the aliases */
						for(h=he->h_aliases; (ret!=1) && (*h); h++){
							ret=comp_string(op, *h, rtype, r);
						}
					}
					break;
				case DIFF_OP:
					ret=comp_ip(EQUAL_OP, ip, rtype, r);
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
						" src_ip or dst_ip (%d)\n", rtype);
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
	struct sip_uri uri;
	int ret;
	ret=E_BUG;
	
	if (e->type!=ELEM_T){
		LOG(L_CRIT," BUG: eval_elem: invalid type\n");
		goto error;
	}
	switch(e->l_type){
	case METHOD_O:
		ret=comp_str(e->op, &msg->first_line.u.request.method, 
			     e->r_type, &e->r);
		break;
	case URI_O:
		if(msg->new_uri.s) {
			if (e->r_type==MYSELF_ST){
				if (parse_sip_msg_uri(msg)<0) ret=-1;
				else ret=check_self_op(e->op, &msg->parsed_uri.host,
						       msg->parsed_uri.port_no?
						       msg->parsed_uri.port_no:SIP_PORT);
			}else{
				ret=comp_str(e->op, &msg->new_uri, 
					     e->r_type, &e->r);
			}
		}else{
			if (e->r_type==MYSELF_ST){
				if (parse_sip_msg_uri(msg)<0) ret=-1;
				else ret=check_self_op(e->op, &msg->parsed_uri.host,
						       msg->parsed_uri.port_no?
						       msg->parsed_uri.port_no:SIP_PORT);
			}else{
				ret=comp_str(e->op, &msg->first_line.u.request.uri,
					     e->r_type, &e->r);
			}
		}
		break;
		
	case FROM_URI_O:
		if (parse_from_header(msg)!=0){
			LOG(L_ERR, "ERROR: eval_elem: bad or missing"
			    " From: header\n");
			goto error;
		}
		if (e->r_type==MYSELF_ST){
			if (parse_uri(get_from(msg)->uri.s, get_from(msg)->uri.len,
				      &uri) < 0){
				LOG(L_ERR, "ERROR: eval_elem: bad uri in From:\n");
				goto error;
			}
			ret=check_self_op(e->op, &uri.host,
					  uri.port_no?uri.port_no:SIP_PORT);
		}else{
			ret=comp_str(e->op, &get_from(msg)->uri,
				     e->r_type, &e->r);
		}
		break;

	case TO_URI_O:
		if ((msg->to==0) && ((parse_headers(msg, HDR_TO_F, 0)==-1) ||
				     (msg->to==0))){
			LOG(L_ERR, "ERROR: eval_elem: bad or missing"
			    " To: header\n");
			goto error;
		}
		     /* to content is parsed automatically */
		if (e->r_type==MYSELF_ST){
			if (parse_uri(get_to(msg)->uri.s, get_to(msg)->uri.len,
				      &uri) < 0){
				LOG(L_ERR, "ERROR: eval_elem: bad uri in To:\n");
				goto error;
			}
			ret=check_self_op(e->op, &uri.host,
					  uri.port_no?uri.port_no:SIP_PORT);
		}else{
			ret=comp_str(e->op, &get_to(msg)->uri,
				     e->r_type, &e->r);
		}
		break;
		
	case SRCIP_O:
		ret=comp_ip(e->op, &msg->rcv.src_ip, e->r_type, &e->r);
		break;
		
	case DSTIP_O:
		ret=comp_ip(e->op, &msg->rcv.dst_ip, e->r_type, &e->r);
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
		ret=comp_num(e->op, (int)msg->rcv.src_port, 
			     e->r_type, &e->r);
		break;
		
	case DSTPORT_O:
		ret=comp_num(e->op, (int)msg->rcv.dst_port, 
			     e->r_type, &e->r);
		break;
		
	case PROTO_O:
		ret=comp_num(e->op, msg->rcv.proto, 
			     e->r_type, &e->r);
		break;
		
	case AF_O:
		ret=comp_num(e->op, (int)msg->rcv.src_ip.af, 
			     e->r_type, &e->r);
		break;

	case MSGLEN_O:
		ret=comp_num(e->op, (int)msg->len, 
				e->r_type, &e->r);
		break;

	case AVP_ST:
		ret = comp_avp(e->op, e->l.attr, e->r_type, &e->r);
		break;
		
	default:
		LOG(L_CRIT, "BUG: eval_elem: invalid operand %d\n",
		    e->l_type);
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
			case LOGAND_OP:
				ret=eval_expr(e->l.expr, msg);
				/* if error or false stop evaluating the rest */
				if (ret!=1) break;
				ret=eval_expr(e->r.expr, msg); /*ret1 is 1*/
				break;
			case LOGOR_OP:
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
	for(i=0;i<BRANCH_RT_NO;i++){
		if(branch_rlist[i]){
			if ((ret=fix_actions(branch_rlist[i]))!=0){
				return ret;
			}
		}
	}
	return 0;
}


/* debug function, prints main routing table */
void print_rl()
{
	int j;

	for(j=0; j<RT_NO; j++){
		if (rlist[j]==0){
			if (j==0) DBG("WARNING: the main routing table is empty\n");
			continue;
		}
		DBG("routing table %d:\n",j);
		print_actions(rlist[j]);
		DBG("\n");
	}
	for(j=0; j<ONREPLY_RT_NO; j++){
		if (onreply_rlist[j]==0){
			continue;
		}
		DBG("onreply routing table %d:\n",j);
		print_actions(onreply_rlist[j]);
		DBG("\n");
	}
	for(j=0; j<FAILURE_RT_NO; j++){
		if (failure_rlist[j]==0){
			continue;
		}
		DBG("failure routing table %d:\n",j);
		print_actions(failure_rlist[j]);
		DBG("\n");
	}
	for(j=0; j<BRANCH_RT_NO; j++){
		if (branch_rlist[j]==0){
			continue;
		}
		DBG("branch routing table %d:\n",j);
		print_actions(branch_rlist[j]);
		DBG("\n");
	}
}
