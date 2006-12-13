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
 *  2005-12-12  added retcode support (anrei)
 *  2005-12-19  select framework (mma)
 *  2006-01-30  removed rec. protection from eval_expr (andrei)
 *  2006-02-06  added named route tables (andrei)
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
#include "select.h"
#include "onsend.h"
#include "str_hash.h"
#include "ut.h"

#define RT_HASH_SIZE	8 /* route names hash */

/* main routing script table  */
struct route_list main_rt;
struct route_list onreply_rt;
struct route_list failure_rt;
struct route_list branch_rt;
struct route_list onsend_rt;



inline static void destroy_rlist(struct route_list* rt)
{
	struct str_hash_entry* e;
	struct str_hash_entry* tmp;

	if (rt->rlist){
		pkg_free(rt->rlist);
		rt->rlist=0;
		rt->entries=0;
	}
	if (rt->names.table){
		clist_foreach_safe(rt->names.table, e, tmp, next){
			pkg_free(e);
		}
		pkg_free(rt->names.table);
		rt->names.table=0;
		rt->names.size=0;
	}
}



void destroy_routes()
{
	destroy_rlist(&main_rt);
	destroy_rlist(&onreply_rt);
	destroy_rlist(&failure_rt);
	destroy_rlist(&branch_rt);
}



/* adds route name -> i mapping
 * WARNING: it doesn't check for pre-existing routes 
 * return -1 on error, route index on success
 */
static int route_add(struct route_list* rt, char* name, int i)
{
	struct str_hash_entry* e;
	
	e=pkg_malloc(sizeof(struct str_hash_entry));
	if (e==0){
		LOG(L_CRIT, "ERROR: route_add: out of memory\n");
		goto error;
	}
	e->key.s=name;
	e->key.len=strlen(name);
	e->flags=0;
	e->u.n=i;
	str_hash_add(&rt->names, e);
	return 0;
error:
	return -1;
}



/* returns -1 on error, 0 on success */
inline  static int init_rlist(char* r_name, struct route_list* rt,
								int n_entries, int hash_size)
{
		rt->rlist=pkg_malloc(sizeof(struct action*)*n_entries);
		if (rt->rlist==0){ 
			LOG(L_CRIT, "ERROR: failed to allocate \"%s\" route tables: " 
					"out of memory\n", r_name); 
			goto error; 
		}
		memset(rt->rlist, 0 , sizeof(struct action*)*n_entries);
		rt->idx=1; /* idx=0 == default == reserved */
		rt->entries=n_entries;
		if (str_hash_alloc(&rt->names, hash_size)<0){
			LOG(L_CRIT, "ERROR: \"%s\" route table: failed to alloc hash\n",
					r_name);
			goto error;
		}
		str_hash_init(&rt->names);
		route_add(rt, "0", 0);  /* default route */
		
		return 0;
error:
		return -1;
}



/* init route tables */
int init_routes()
{
	if (init_rlist("main", &main_rt, RT_NO, RT_HASH_SIZE)<0)
		goto error;
	if (init_rlist("on_reply", &onreply_rt, ONREPLY_RT_NO, RT_HASH_SIZE)<0)
		goto error;
	if (init_rlist("failure", &failure_rt, FAILURE_RT_NO, RT_HASH_SIZE)<0)
		goto error;
	if (init_rlist("branch", &branch_rt, BRANCH_RT_NO, RT_HASH_SIZE)<0)
		goto error;
	if (init_rlist("on_send", &onsend_rt, ONSEND_RT_NO, RT_HASH_SIZE)<0)
		goto error;
	return 0;
error:
	destroy_routes();
	return -1;
}



static inline int route_new_list(struct route_list* rt)
{
	int ret;
	struct action** tmp;
	
	ret=-1;
	if (rt->idx >= rt->entries){
		tmp=pkg_realloc(rt->rlist, 2*rt->entries*sizeof(struct action*));
		if (tmp==0){
			LOG(L_CRIT, "ERROR: route_new_list: out of memory\n");
			goto end;
		}
		/* init the newly allocated memory chunk */
		memset(&tmp[rt->entries], 0, rt->entries*sizeof(struct action*));
		rt->rlist=tmp;
		rt->entries*=2;
	}
	if (rt->idx<rt->entries){
		ret=rt->idx;
		rt->idx++;
	}
end:
	return ret;
}




/* 
 * if the "name" route already exists, return its index, else
 * create a new empty route
 * return route index in rt->rlist or -1 on error
 */
int route_get(struct route_list* rt, char* name)
{
	int len;
	struct str_hash_entry* e;
	int i;
	
	len=strlen(name);
	/* check if exists an non empty*/
	e=str_hash_get(&rt->names, name, len);
	if (e){
		i=e->u.n;
	}else{
		i=route_new_list(rt);
		if (i==-1) goto error;
		if (route_add(rt, name, i)<0){
			goto error;
		}
	}
	return i;
error:
	return -1;
}



/* 
 * if the "name" route already exists, return its index, else
 * return error
 * return route index in rt->rlist or -1 on error
 */
int route_lookup(struct route_list* rt, char* name)
{
	int len;
	struct str_hash_entry* e;
	
	len=strlen(name);
	/* check if exists an non empty*/
	e=str_hash_get(&rt->names, name, len);
	if (e){
		return e->u.n;
	}else{
		return -1;
	}
}



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
				}else if (exp->r_type!=RE_ST && exp->r_type != AVP_ST && exp->r_type != SELECT_ST){
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
			if (exp->l_type==SELECT_O) {
				if ((ret=resolve_select(exp->l.select)) < 0) {
					BUG("Unable to resolve select\n");
					print_select(exp->l.select);
					return ret;
				}
			}
			if ((exp->r_type==SELECT_O)||(exp->r_type==SELECT_ST)) {
				if ((ret=resolve_select(exp->r.select)) < 0) {
					BUG("Unable to resolve select\n");
					print_select(exp->l.select);
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
					switch(t->val[0].type){
						case IP_ST:
							tmp=strdup(ip_addr2a(
										(struct ip_addr*)t->val[0].u.data));
							if (tmp==0){
								LOG(L_CRIT, "ERROR: fix_actions:"
										"memory allocation failure\n");
								return E_OUT_OF_MEM;
							}
							t->val[0].type=STRING_ST;
							t->val[0].u.string=tmp;
							/* no break */
						case STRING_ST:
							s.s = t->val[0].u.string;
							s.len = strlen(s.s);
							p=add_proxy(&s, t->val[1].u.number, 0); /* FIXME proto*/
							if (p==0) return E_BAD_ADDRESS;
							t->val[0].u.data=p;
							t->val[0].type=PROXY_ST;
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
				if (t->val[0].type!=EXPR_ST){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if (should be expr)\n",
								t->val[0].type);
					return E_BUG;
				}else if( (t->val[1].type!=ACTIONS_ST)&&(t->val[1].type!=NOSUBTYPE) ){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if() {...} (should be action)\n",
								t->val[1].type);
					return E_BUG;
				}else if( (t->val[2].type!=ACTIONS_ST)&&(t->val[2].type!=NOSUBTYPE) ){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if() {} else{...}(should be action)\n",
								t->val[2].type);
					return E_BUG;
				}
				if (t->val[0].u.data){
					if ((ret=fix_expr((struct expr*)t->val[0].u.data))<0)
						return ret;
				}
				if ( (t->val[1].type==ACTIONS_ST)&&(t->val[1].u.data) ){
					if ((ret=fix_actions((struct action*)t->val[1].u.data))<0)
						return ret;
				}
				if ( (t->val[2].type==ACTIONS_ST)&&(t->val[2].u.data) ){
						if ((ret=fix_actions((struct action*)t->val[2].u.data))<0)
						return ret;
				}
				break;

		        case ASSIGN_T:
		        case ADD_T:
				if (t->val[0].type != AVP_ST) {
					LOG(L_CRIT, "BUG: fix_actions: Invalid left side of assignment\n");
					return E_BUG;
				}
				if (t->val[0].u.attr->type & AVP_CLASS_DOMAIN) {
					LOG(L_ERR, "ERROR: You cannot change domain attributes from the script, they are read-only\n");
					return E_BUG;
				} else if (t->val[0].u.attr->type & AVP_CLASS_GLOBAL) {
					LOG(L_ERR, "ERROR: You cannot change global attributes from the script, they are read-only\n");
					return E_BUG;
				}

				if (t->val[1].type == ACTION_ST && t->val[1].u.data) {
					if ((ret = fix_actions((struct action*)t->val[1].u.data)) < 0) {
						return ret;
					}
				} else if (t->val[1].type == EXPR_ST && t->val[1].u.data) {
					if ((ret = fix_expr((struct expr*)t->val[1].u.data)) < 0) {
						return ret;
					}
				} else if (t->val[1].type == STRING_ST) {
					int len;
					len = strlen(t->val[1].u.data);
					t->val[1].u.str.s = t->val[1].u.data;
					t->val[1].u.str.len = len;
				} else if (t->val[1].type == SELECT_ST) {
					if ((ret=resolve_select(t->val[1].u.select)) < 0) {
						BUG("Unable to resolve select\n");
						print_select(t->val[1].u.select);
						return ret;
					}
				}
				break;

			case MODULE_T:
				cmd = t->val[0].u.data;
				if (cmd && cmd->fixup) {
					int i;
					DBG("fixing %s()\n", cmd->name);
					/* type cast NUMBER to STRING, old modules may expect all STRING params during fixup */
					for (i=0; i<t->val[1].u.number; i++) {
						if (t->val[i+2].type == NUMBER_ST) {
							char buf[30];
							snprintf(buf, sizeof(buf)-1, "%ld", t->val[i+2].u.number);
							/* fixup currently requires string pkg_malloc-aed */
							t->val[i+2].u.string = pkg_malloc(strlen(buf)+1);
							if (!t->val[i+2].u.string) {
								LOG(L_CRIT, "ERROR: cannot translate NUMBER to STRING\n");
								return E_OUT_OF_MEM;
							}
							strcpy(t->val[i+2].u.string, buf);
							t->val[i+2].type = STRING_ST;
						}
					}
					for (i=0; i<t->val[1].u.number; i++) {
						void *p;
						p = t->val[i+2].u.data;
						ret = cmd->fixup(&t->val[i+2].u.data, i+1);
						if (t->val[i+2].u.data != p)
							t->val[i+2].type = MODFIXUP_ST;
						if (ret < 0)
							return ret;
					}
				}
				break;
			case FORCE_SEND_SOCKET_T:
				if (t->val[0].type!=SOCKID_ST){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for force_send_socket\n",
								t->val[0].type);
					return E_BUG;
				}
				he=resolvehost(((struct socket_id*)t->val[0].u.data)->name);
				if (he==0){
					LOG(L_ERR, "ERROR: fix_actions: force_send_socket:"
								" could not resolve %s\n",
							((struct socket_id*)t->val[0].u.data)->name);
					return E_BAD_ADDRESS;
				}
				hostent2ip_addr(&ip, he, 0);
				si=find_si(&ip, ((struct socket_id*)t->val[0].u.data)->port,
								((struct socket_id*)t->val[0].u.data)->proto);
				if (si==0){
					LOG(L_ERR, "ERROR: fix_actions: bad force_send_socket"
							" argument: %s:%d (ser doesn't listen on it)\n",
							((struct socket_id*)t->val[0].u.data)->name,
							((struct socket_id*)t->val[0].u.data)->port);
					return E_BAD_ADDRESS;
				}
				t->val[0].u.data=si;
				t->val[0].type=SOCKETINFO_ST;
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
		avp = search_avp_by_index(r->attr->type, r->attr->name, &val, r->attr->index);
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
inline static int comp_str(int op, str* left, int rtype, union exp_op* r, struct sip_msg* msg)
{
	str* right;
	int_str val;
	str v;
	avp_t* avp;
	int ret;
	char backup;
	regex_t* re;
	unsigned int l;
	
	right=0; /* warning fix */

	if (rtype == AVP_ST) {
		avp = search_avp_by_index(r->attr->type, r->attr->name, &val, r->attr->index);
		if (avp && (avp->flags & AVP_VAL_STR)) right = &val.s;
		else return 0;
	} else if (rtype == SELECT_ST) {
		ret = run_select(&v, r->select, msg);
		if (ret > 0) return 0;       /* Not found */
		else if (ret < 0) goto error; /* Error */
		right = &v;
	} else if ((op == MATCH_OP && rtype == RE_ST)) {
	} else if (op != MATCH_OP && rtype == STRING_ST) {
		right = &r->str;
	} else if (rtype == NUMBER_ST) {
			/* "123" > 100 is not allowed by cfg.y rules
			 * but can happen as @select or $avp evaluation
			 * $test > 10
			 * the right operator MUST be number to do the conversion
			 */
		str2int(left,&l);
		return comp_num(op, l, rtype, r);
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
			if (rtype == AVP_ST || rtype == SELECT_ST) {
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


inline static int comp_avp(int op, avp_spec_t* spec, int rtype, union exp_op* r, struct sip_msg* msg)
{
	avp_t* avp;
	int_str val;
	union exp_op num_val;
	str tmp;

	if (spec->type & AVP_INDEX_ALL) {
		avp = search_first_avp(spec->type & ~AVP_INDEX_ALL, spec->name, NULL, NULL);
		return (avp!=0);
	}
	avp = search_avp_by_index(spec->type, spec->name, &val, spec->index);
	if (!avp) return 0;

	switch(op) {
	case NO_OP:
		if (avp->flags & AVP_VAL_STR) {
			return val.s.len!=0;
		} else {
			return val.n != 0;
		}
		break;

	case BINOR_OP:
		return (val.n | r->intval)!=0;
		break;

	case BINAND_OP:
		return (val.n & r->intval)!=0;
		break;
	}

	if (avp->flags & AVP_VAL_STR) {
		return comp_str(op, &val.s, rtype, r, msg);
	} else {
		switch(rtype){
			case NUMBER_ST:
				return comp_num(op, val.n, rtype, r);
			case STRING_ST:
				tmp.s=r->string;
				tmp.len=strlen(r->string);
				if (str2int(&tmp, (unsigned int*)&num_val.intval)<0){
					LOG(L_ERR, "ERROR: comp_avp: cannot convert string value"
								" to int (%s)\n", ZSW(r->string));
					return -1;
				}
				return comp_num(op, val.n, NUMBER_ST, &num_val);
			case STR_ST:
				if (str2int(&r->str, (unsigned int*)&num_val.intval)<0){
					LOG(L_ERR, "ERROR: comp_avp: cannot convert str value"
								" to int (%.*s)\n", r->str.len, ZSW(r->str.s));
					return -1;
				}
				return comp_num(op, val.n, NUMBER_ST, &num_val);
			default:
				LOG(L_CRIT, "BUG: comp_avp: invalid type for numeric avp "
							"comparison (%d)\n", rtype);
				return -1;
		}
	}
}

/*
 * Left side of expression was select
 */
inline static int comp_select(int op, select_t* sel, int rtype, union exp_op* r, struct sip_msg* msg)
{
	int ret;
	str val;

	ret = run_select(&val, sel, msg);
	if (ret < 0) return -1;
	if (ret > 0) return 0;

	switch(op) {
	case NO_OP: return (val.len>0);
	case BINOR_OP:
	case BINAND_OP:
		ERR("Binary operators cannot be used with string selects\n");
		return -1;
	}
	return comp_str(op, &val, rtype, r, msg);
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


/* returns: 0/1 (false/true) or -1 on error */
inline static int eval_elem(struct expr* e, struct sip_msg* msg)
{
	struct sip_uri uri;
	int ret;
	struct onsend_info* snd_inf;
	struct ip_addr ip;
	ret=E_BUG;

	if (e->type!=ELEM_T){
		LOG(L_CRIT," BUG: eval_elem: invalid type\n");
		goto error;
	}
	switch(e->l_type){
	case METHOD_O:
		ret=comp_str(e->op, &msg->first_line.u.request.method,
			     e->r_type, &e->r, msg);
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
					     e->r_type, &e->r, msg);
			}
		}else{
			if (e->r_type==MYSELF_ST){
				if (parse_sip_msg_uri(msg)<0) ret=-1;
				else ret=check_self_op(e->op, &msg->parsed_uri.host,
						       msg->parsed_uri.port_no?
						       msg->parsed_uri.port_no:SIP_PORT);
			}else{
				ret=comp_str(e->op, &msg->first_line.u.request.uri,
					     e->r_type, &e->r, msg);
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
				     e->r_type, &e->r, msg);
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
				     e->r_type, &e->r, msg);
		}
		break;

	case SRCIP_O:
		ret=comp_ip(e->op, &msg->rcv.src_ip, e->r_type, &e->r);
		break;

	case DSTIP_O:
		ret=comp_ip(e->op, &msg->rcv.dst_ip, e->r_type, &e->r);
		break;

	case SNDIP_O:
		snd_inf=get_onsend_info();
		if (snd_inf && snd_inf->send_sock){
			ret=comp_ip(e->op, &snd_inf->send_sock->address, e->r_type, &e->r);
		}else{
			BUG("eval_elem: snd_ip unknown (not in a onsend_route?)\n");
		}
		break;

	case TOIP_O:
		snd_inf=get_onsend_info();
		if (snd_inf && snd_inf->to){
			su2ip_addr(&ip, snd_inf->to);
			ret=comp_ip(e->op, &ip, e->r_type, &e->r);
		}else{
			BUG("eval_elem: to_ip unknown (not in a onsend_route?)\n");
		}
		break;

	case NUMBER_O:
		ret=!(!e->r.intval); /* !! to transform it in {0,1} */
		break;

	case ACTION_O:
		ret=run_actions( (struct action*)e->r.param, msg);
		if (ret<=0) ret=0;
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

	case SNDPORT_O:
		snd_inf=get_onsend_info();
		if (snd_inf && snd_inf->send_sock){
			ret=comp_num(e->op, (int)snd_inf->send_sock->port_no,
				     e->r_type, &e->r);
		}else{
			BUG("eval_elem: snd_port unknown (not in a onsend_route?)\n");
		}
		break;

	case TOPORT_O:
		snd_inf=get_onsend_info();
		if (snd_inf && snd_inf->to){
			ret=comp_num(e->op, (int)su_getport(snd_inf->to),
				     e->r_type, &e->r);
		}else{
			BUG("eval_elem: to_port unknown (not in a onsend_route?)\n");
		}
		break;

	case PROTO_O:
		ret=comp_num(e->op, msg->rcv.proto,
			     e->r_type, &e->r);
		break;

	case SNDPROTO_O:
		snd_inf=get_onsend_info();
		if (snd_inf && snd_inf->send_sock){
			ret=comp_num(e->op, snd_inf->send_sock->proto,
				     e->r_type, &e->r);
		}else{
			BUG("eval_elem: snd_proto unknown (not in a onsend_route?)\n");
		}
		break;

	case AF_O:
		ret=comp_num(e->op, (int)msg->rcv.src_ip.af,
			     e->r_type, &e->r);
		break;

	case SNDAF_O:
		snd_inf=get_onsend_info();
		if (snd_inf && snd_inf->send_sock){
			ret=comp_num(e->op, snd_inf->send_sock->address.af,
							e->r_type, &e->r);
		}else{
			BUG("eval_elem: snd_af unknown (not in a onsend_route?)\n");
		}
		break;

	case MSGLEN_O:
		if ((snd_inf=get_onsend_info())!=0){
			ret=comp_num(e->op, (int)snd_inf->len,
					e->r_type, &e->r);
		}else{
			ret=comp_num(e->op, (int)msg->len,
					e->r_type, &e->r);
		}
		break;

	case RETCODE_O:
		ret=comp_num(e->op, last_retcode, e->r_type, &e->r);
		break;

	case AVP_O:
		ret = comp_avp(e->op, e->l.attr, e->r_type, &e->r, msg);
		break;

	case SELECT_O:
		ret = comp_select(e->op, e->l.select, e->r_type, &e->r, msg);
		break;

	default:
		LOG(L_CRIT, "BUG: eval_elem: invalid operand %d\n",
		    e->l_type);
	}
	return ret;
error:
	return -1;
}



/* ret= 0/1 (true/false) ,  -1 on error */
int eval_expr(struct expr* e, struct sip_msg* msg)
{
	int ret;

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



static int fix_rl(struct route_list* rt)
{
	int i;
	int ret;
	
	for(i=0;i<rt->idx; i++){
		if(rt->rlist[i]){
			if ((ret=fix_actions(rt->rlist[i]))!=0){
				return ret;
			}
		}
	}
	return 0;
}



/* fixes all action tables */
/* returns 0 if ok , <0 on error */
int fix_rls()
{
	int ret;
	
	if ((ret=fix_rl(&main_rt))!=0)
		return ret;
	if ((ret=fix_rl(&onreply_rt))!=0)
		return ret;
	if ((ret=fix_rl(&failure_rt))!=0)
		return ret;
	if ((ret=fix_rl(&branch_rt))!=0)
		return ret;
	if ((ret=fix_rl(&onsend_rt))!=0)
		return ret;

	return 0;
}



static void print_rl(struct route_list* rt, char* name)
{
	int j;
	
	for(j=0; j<rt->entries; j++){
		if (rt->rlist[j]==0){
			if ((j==0) && (rt==&main_rt))
				DBG("WARNING: the main routing table is empty\n");
			continue;
		}
		DBG("%s routing table %d:\n", name, j);
		print_actions(rt->rlist[j]);
		DBG("\n");
	}
}


/* debug function, prints routing tables */
void print_rls()
{
	print_rl(&main_rt, "");
	print_rl(&onreply_rt, "onreply");
	print_rl(&failure_rt, "failure");
	print_rl(&branch_rt, "branch");
	print_rl(&onsend_rt, "onsend");
}
