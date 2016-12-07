/*
 * SIP routing engine
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
 *
 */


/** Kamailio core :: expression evaluation, route fixups and routing lists.
 * @file route.c
 * @ingroup core
 * Module: @ref core
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
#include "lvalue.h"
#include "rvalue.h"
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
#include "rvalue.h"
#include "switch.h"
#include "cfg/cfg_struct.h"

#define RT_HASH_SIZE	8 /* route names hash */

/* main routing script table  */
struct route_list main_rt;
struct route_list onreply_rt;
struct route_list failure_rt;
struct route_list branch_rt;
struct route_list onsend_rt;
struct route_list event_rt;

int route_type = REQUEST_ROUTE;

/** script optimization level, useful for debugging.
 *  0 - no optimization
 *  1 - optimize rval expressions
 *  2 - optimize expr elems
 */
int scr_opt_lev=9;

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
	destroy_rlist(&event_rt);
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
		LM_CRIT("out of memory\n");
		goto error;
	}
	LM_DBG("mapping routing block (%p)[%s] to %d\n", rt, name, i);
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
			LM_CRIT("failed to allocate \"%s\" route tables: " 
					"out of memory\n", r_name); 
			goto error; 
		}
		memset(rt->rlist, 0 , sizeof(struct action*)*n_entries);
		rt->idx=1; /* idx=0 == default == reserved */
		rt->entries=n_entries;
		if (str_hash_alloc(&rt->names, hash_size)<0){
			LM_CRIT("\"%s\" route table: failed to alloc hash\n",
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
	if (init_rlist("event", &event_rt, EVENT_RT_NO, RT_HASH_SIZE)<0)
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
			LM_CRIT("out of memory\n");
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



int fix_actions(struct action* a); /*fwd declaration*/


/** optimize the left side of a struct expr.
 *  @return 1 if optimized, 0 if not and -1 on error
 */
static int exp_optimize_left(struct expr* exp)
{
	struct rval_expr* rve;
	struct rvalue* rval;
	int old_ltype, old_rtype, old_op;
	int ret;
	
	ret=0;
	if (exp->type!=ELEM_T)
		return 0;
	old_ltype=exp->l_type;
	old_rtype=exp->r_type;
	old_op=exp->op;
	if (exp->l_type==RVEXP_O){
		rve=exp->l.param;
		/* rve should be previously fixed/optimized */
		/* optimize exp (rval(val)) -> exp(val) */
		if (rve->op==RVE_RVAL_OP){
			rval=&rve->left.rval;
			switch(rval->type){
				case RV_INT:
					if (exp->op==NO_OP){
						exp->l_type=NUMBER_O;
						exp->l.param=0;
						exp->r_type=NUMBER_ST;
						exp->r.numval=rval->v.l;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}
					break;
				case RV_STR:
					/* string evaluated in expression context - not
					   supported */
					break;
				case RV_BEXPR:
					if (exp->op==NO_OP){
						/* replace the current expr. */
						*exp=*(rval->v.bexpr);
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					};
					break;
				case RV_ACTION_ST:
					if (exp->op==NO_OP){
						exp->l_type=ACTION_O;
						exp->l.param=0;
						exp->r_type=ACTION_ST;
						exp->r.param=rval->v.action;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}
					break;
				case RV_SEL:
					exp->l.select=pkg_malloc(sizeof(*exp->l.select));
					if (exp->l.select){
						exp->l_type=SELECT_O;
						*exp->l.select=rval->v.sel;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}else
						ret=-1;
					break;
				case RV_AVP:
					exp->l.attr=pkg_malloc(sizeof(*exp->l.attr));
					if (exp->l.attr){
						exp->l_type=AVP_O;
						*exp->l.attr=rval->v.avps;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}else
						ret=-1;
					break;
				case RV_PVAR:
					exp->l.param=pkg_malloc(sizeof(pv_spec_t));
					if (exp->l.param){
						exp->l_type=PVAR_O;
						*((pv_spec_t*)exp->l.param)=rval->v.pvs;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}else
						ret=-1;
					break;
				case RV_NONE:
					break;
			}
		}
	}
	if (ret>0)
		LM_DBG("op%d(_O%d_, ST%d) => op%d(_O%d_, ST%d)\n",
			old_op, old_ltype, old_rtype, exp->op, exp->l_type, exp->r_type);
	return ret;
}



/** optimize the left side of a struct expr.
 *  @return 1 if optimized, 0 if not and -1 on error
 */
static int exp_optimize_right(struct expr* exp)
{
	struct rval_expr* rve;
	struct rvalue* rval;
	int old_ltype, old_rtype, old_op;
	int ret;
	
	ret=0;
	if ((exp->type!=ELEM_T) ||(exp->op==NO_OP))
		return 0;
	old_ltype=exp->l_type;
	old_rtype=exp->r_type;
	old_op=exp->op;
	if (exp->r_type==RVE_ST){
		rve=exp->r.param;
		/* rve should be previously fixed/optimized */
		/* optimize exp (rval(val)) -> exp(val) */
		if (rve->op==RVE_RVAL_OP){
			rval=&rve->left.rval;
			switch(rval->type){
				case RV_INT:
					exp->r_type=NUMBER_ST;
					exp->r.numval=rval->v.l;
					rval_destroy(rval);
					pkg_free(rve);
					ret=1;
					break;
				case RV_STR:
					exp->r.str.s=pkg_malloc(rval->v.s.len+1);
					if (exp->r.str.s){
						exp->r.str.len=rval->v.s.len;
						memcpy(exp->r.str.s, rval->v.s.s, rval->v.s.len);
						exp->r.str.s[exp->r.str.len]=0;
						exp->r_type=STRING_ST;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}else
						ret=-1;
					break;
				case RV_BEXPR:
					/* cannot be optimized further, is an exp_elem
					   which is not constant */
					break;
				case RV_ACTION_ST:
					/* cannot be optimized further, is not constant and
					  eval_elem() does not support ACTION_ST for op!=NO_OP*/
					break;
				case RV_SEL:
					exp->r.select=pkg_malloc(sizeof(*exp->l.select));
					if (exp->r.select){
						exp->r_type=SELECT_ST;
						*exp->r.select=rval->v.sel;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}else
						ret=-1;
					break;
				case RV_AVP:
					exp->r.attr=pkg_malloc(sizeof(*exp->l.attr));
					if (exp->r.attr){
						exp->r_type=AVP_ST;
						*exp->r.attr=rval->v.avps;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}else
						ret=-1;
					break;
				case RV_PVAR:
					exp->r.param=pkg_malloc(sizeof(pv_spec_t));
					if (exp->r.param){
						exp->r_type=PVAR_ST;
						*((pv_spec_t*)exp->r.param)=rval->v.pvs;
						rval_destroy(rval);
						pkg_free(rve);
						ret=1;
					}else
						ret=-1;
					break;
				case RV_NONE:
					ret=-1;
					break;
			}
		}
	}
	if (ret>0)
		LM_DBG("op%d(O%d, _ST%d_) => op%d(O%d, _ST%d_)\n",
			old_op, old_ltype, old_rtype, exp->op, exp->l_type, exp->r_type);
	return ret;
}



/* traverses an expr tree and compiles the REs where necessary)
 * returns: 0 for ok, <0 if errors */
int fix_expr(struct expr* exp)
{
	regex_t* re;
	int ret;
	int len;

	ret=E_BUG;
	if (exp==0){
		LM_CRIT("null pointer\n");
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
						LM_CRIT("unknown op %d\n", exp->op);
		}
	}else if (exp->type==ELEM_T){
			/* first calculate lengths of strings  (only right side, since 
			  left side can never be a string) */
			if (exp->r_type==STRING_ST) {
				if (exp->r.string) len = strlen(exp->r.string);
				else len = 0;
				exp->r.str.s = exp->r.string;
				exp->r.str.len = len;
			}
			/* then fix & optimize rve/rvals (they might be optimized
			   to non-rvals, e.g. string, avp a.s.o and needs to be done
			   before MATCH_OP and other fixups) */
			if (exp->l_type==RVEXP_O){
				if ((ret=fix_rval_expr(exp->l.param))<0){
					LM_ERR("Unable to fix left rval expression\n");
					return ret;
				}
				if (scr_opt_lev>=2)
					exp_optimize_left(exp);
			}
			if (exp->r_type==RVE_ST){
				if ((ret=fix_rval_expr(exp->r.param))<0){
					LM_ERR("Unable to fix right rval expression\n");
					return ret;
				}
				if (scr_opt_lev>=2)
					exp_optimize_right(exp);
			}
			
			
			if (exp->op==MATCH_OP){
				     /* right side either has to be string, in which case
				      * we turn it into regular expression, or it is regular
				      * expression already. In that case we do nothing
				      */
				if (exp->r_type==STRING_ST){
					re=(regex_t*)pkg_malloc(sizeof(regex_t));
					if (re==0){
						LM_CRIT("memory allocation failure\n");
						return E_OUT_OF_MEM;
					}
					if (regcomp(re, (char*) exp->r.param,
								REG_EXTENDED|REG_NOSUB|REG_ICASE) ){
						LM_CRIT("bad re \"%s\"\n", (char*) exp->r.param);
						pkg_free(re);
						return E_BAD_RE;
					}
					/* replace the string with the re */
					pkg_free(exp->r.param);
					exp->r.re=re;
					exp->r_type=RE_ST;
				}else if (exp->r_type!=RE_ST && exp->r_type != AVP_ST
						&& exp->r_type != SELECT_ST &&
						exp->r_type != SELECT_UNFIXED_ST &&
						exp->r_type!= RVE_ST
						&& exp->r_type != PVAR_ST){
					LM_CRIT("invalid type for match\n");
					return E_BUG;
				}
			}
			if (exp->l_type==ACTION_O){
				ret=fix_actions((struct action*)exp->r.param);
				if (ret!=0){
					LM_CRIT("fix_actions error\n");
					return ret;
				}
			}
			if (exp->l_type==SELECT_UNFIXED_O) {
				if ((ret=resolve_select(exp->l.select)) < 0) {
					LM_ERR("Unable to resolve select\n");
					print_select(exp->l.select);
					return ret;
				}
				exp->l_type=SELECT_O;
			}
			if (exp->r_type==SELECT_UNFIXED_ST) {
				if ((ret=resolve_select(exp->r.select)) < 0) {
					LM_ERR("Unable to resolve select\n");
					print_select(exp->r.select);
					return ret;
				}
				exp->r_type=SELECT_ST;
			}
			/* PVAR don't need fixing */
			ret=0;
	}
	return ret;
}



/* adds the proxies in the proxy list & resolves the hostnames */
/* returns 0 if ok, <0 on error */
int fix_actions(struct action* a)
{
	struct action *t;
	struct proxy_l* p;
	char *tmp;
	void *tmp_p;
	int ret;
	int i;
	sr31_cmd_export_t* cmd;
	str s;
	struct hostent* he;
	struct ip_addr ip;
	struct socket_info* si;
	struct lvalue* lval;
	struct rval_expr* rve;
	struct rval_expr* err_rve;
	enum rval_type rve_type, err_type, expected_type;
	struct rvalue* rv;
	int rve_param_no;

	if (a==0){
		LM_CRIT("null pointer\n");
		return E_BUG;
	}
	for(t=a; t!=0; t=t->next){
		switch(t->type){
			case FORWARD_T:
			case FORWARD_TLS_T:
			case FORWARD_TCP_T:
			case FORWARD_SCTP_T:
			case FORWARD_UDP_T:
					switch(t->val[0].type){
						case IP_ST:
							tmp=strdup(ip_addr2a(
										(struct ip_addr*)t->val[0].u.data));
							if (tmp==0){
								LM_CRIT("memory allocation failure\n");
								ret = E_OUT_OF_MEM;
								goto error;
							}
							t->val[0].type=STRING_ST;
							t->val[0].u.string=tmp;
							/* no break */
						case STRING_ST:
							s.s = t->val[0].u.string;
							s.len = strlen(s.s);
							p=add_proxy(&s, t->val[1].u.number, 0); /* FIXME proto*/
							if (p==0) { ret =E_BAD_ADDRESS; goto error; }
							t->val[0].u.data=p;
							t->val[0].type=PROXY_ST;
							break;
						case URIHOST_ST:
							break;
						default:
							LM_CRIT("invalid type %d (should be string or number)\n",
										t->type);
							ret = E_BUG;
							goto error;
					}
					break;
			case IF_T:
				if (t->val[0].type!=RVE_ST){
					LM_CRIT("invalid subtype %d for if (should be rval expr)\n",
								t->val[0].type);
					ret = E_BUG;
					goto error;
				}else if( (t->val[1].type!=ACTIONS_ST) &&
							(t->val[1].type!=NOSUBTYPE) ){
					LM_CRIT("invalid subtype %d for if() {...} (should be action)\n",
								t->val[1].type);
					ret = E_BUG;
					goto error;
				}else if( (t->val[2].type!=ACTIONS_ST) &&
							(t->val[2].type!=NOSUBTYPE) ){
					LM_CRIT("invalid subtype %d for if() {} else{...}(should be action)\n",
								t->val[2].type);
					ret = E_BUG;
					goto error;
				}
				rve=(struct rval_expr*)t->val[0].u.data;
				if (rve){
					err_rve=0;
					if (!rve_check_type(&rve_type, rve, &err_rve,
											&err_type, &expected_type)){
						if (err_rve)
							LM_ERR("invalid expression "
									"(%d,%d): subexpression (%d,%d) has type"
									" %s,  but %s is expected\n",
									rve->fpos.s_line, rve->fpos.s_col,
									err_rve->fpos.s_line, err_rve->fpos.s_col,
									rval_type_name(err_type),
									rval_type_name(expected_type) );
						else
							LM_ERR("invalid expression  (%d,%d): type mismatch?",
									rve->fpos.s_line, rve->fpos.s_col);
						ret = E_SCRIPT;
						goto error;
					}
					/* it's not an error anymore to have non-int in an if,
					   only a script warning (to allow backward compat. stuff
					   like if (@ruri) 
					if (rve_type!=RV_INT && rve_type!=RV_NONE){
						LM_ERR("fix_actions: invalid expression (%d,%d):"
								" bad type, integer expected\n",
								rve->fpos.s_line, rve->fpos.s_col);
						return E_UNSPEC;
					}
					*/
					if ((ret=fix_rval_expr(t->val[0].u.data))<0)
						goto error;
				}
				if ( (t->val[1].type==ACTIONS_ST)&&(t->val[1].u.data) ){
					if ((ret=fix_actions((struct action*)t->val[1].u.data))<0)
						goto error;
				}
				if ( (t->val[2].type==ACTIONS_ST)&&(t->val[2].u.data) ){
						if ((ret=fix_actions((struct action*)t->val[2].u.data))
								<0)
						goto error;
				}
				break;
			case SWITCH_T:
				if (t->val[0].type!=RVE_ST){
					LM_CRIT("invalid subtype %d for switch() (should be expr)\n",
								t->val[0].type);
					ret = E_BUG;
					goto error;
				}else if (t->val[1].type!=CASE_ST){
					LM_CRIT("invalid subtype %d for switch(...){...}(should be case)\n",
								t->val[1].type);
					ret = E_BUG;
					goto error;
				}
				if (t->val[0].u.data){
					if ((ret=fix_rval_expr(t->val[0].u.data))<0)
						goto error;
				}else{
					LM_CRIT("null switch() expression\n");
					ret = E_BUG;
					goto error;
				}
				if ((ret=fix_switch(t))<0)
					goto error;
				break;
			case WHILE_T:
				if (t->val[0].type!=RVE_ST){
					LM_CRIT("invalid subtype %d for while() (should be expr)\n",
								t->val[0].type);
					ret = E_BUG;
					goto error;
				}else if (t->val[1].type!=ACTIONS_ST){
					LM_CRIT("invalid subtype %d for while(...){...}(should be action)\n",
								t->val[1].type);
					ret = E_BUG;
					goto error;
				}
				rve=(struct rval_expr*)t->val[0].u.data;
				if (rve){
					err_rve=0;
					if (!rve_check_type(&rve_type, rve, &err_rve,
											&err_type, &expected_type)){
						if (err_rve)
							LM_ERR("invalid expression "
									"(%d,%d): subexpression (%d,%d) has type"
									" %s,  but %s is expected\n",
									rve->fpos.s_line, rve->fpos.s_col,
									err_rve->fpos.s_line, err_rve->fpos.s_col,
									rval_type_name(err_type),
									rval_type_name(expected_type) );
						else
							LM_ERR("invalid expression (%d,%d): type mismatch?",
									rve->fpos.s_line, rve->fpos.s_col);
						ret = E_SCRIPT;
						goto error;
					}
					if (rve_type!=RV_INT && rve_type!=RV_NONE){
						LM_ERR("invalid expression (%d,%d): bad type, integer expected\n",
								rve->fpos.s_line, rve->fpos.s_col);
						ret = E_SCRIPT;
						goto error;
					}
					if ((ret=fix_rval_expr(t->val[0].u.data))<0)
						goto error;
				}else{
					LM_CRIT("null while() expression\n");
					ret = E_BUG;
					goto error;
				}
				if ( t->val[1].u.data && 
					((ret= fix_actions((struct action*)t->val[1].u.data))<0)){
					goto error;
				}
				break;
			case DROP_T:
				/* only RVEs need fixing for drop/return/break */
				if (t->val[0].type!=RVE_ST)
					break;
				rve=(struct rval_expr*)t->val[0].u.data;
				if (rve){
					err_rve=0;
					if (!rve_check_type(&rve_type, rve, &err_rve,
											&err_type, &expected_type)){
						if (err_rve)
							LM_ERR("invalid expression "
									"(%d,%d): subexpression (%d,%d) has type"
									" %s,  but %s is expected\n",
									rve->fpos.s_line, rve->fpos.s_col,
									err_rve->fpos.s_line, err_rve->fpos.s_col,
									rval_type_name(err_type),
									rval_type_name(expected_type) );
						else
							LM_ERR("invalid expression (%d,%d): type mismatch?",
									rve->fpos.s_line, rve->fpos.s_col);
						ret = E_SCRIPT;
						goto error;
					}
					if (rve_type!=RV_INT && rve_type!=RV_NONE){
						LM_ERR("invalid expression (%d,%d): bad type, integer expected\n",
								rve->fpos.s_line, rve->fpos.s_col);
						ret = E_SCRIPT;
						goto error;
					}
					if ((ret=fix_rval_expr(t->val[0].u.data))<0)
						goto error;
				}else{
					LM_CRIT("null drop/return expression\n");
					ret = E_BUG;
					goto error;
				}
				break;
			case ASSIGN_T:
			case ADD_T:
				if (t->val[0].type !=LVAL_ST) {
					LM_CRIT("Invalid left side of assignment\n");
					ret = E_BUG;
					goto error;
				}
				if (t->val[1].type !=RVE_ST) {
					LM_CRIT("Invalid right side of assignment (%d)\n",
								t->val[1].type);
					ret = E_BUG;
					goto error;
				}
				lval=t->val[0].u.data;
				if (lval->type==LV_AVP){
					if (lval->lv.avps.type & AVP_CLASS_DOMAIN) {
						LM_ERR("You cannot change domain"
									" attributes from the script, they are"
									" read-only\n");
						ret = E_BUG;
						goto error;
					} else if (lval->lv.avps.type & AVP_CLASS_GLOBAL) {
						LM_ERR("You cannot change global"
								   " attributes from the script, they are"
								   "read-only\n");
						ret = E_BUG;
						goto error;
					}
				}
				if ((ret=fix_rval_expr(t->val[1].u.data))<0)
					goto error;
				break;

			case MODULE0_T:
			case MODULE1_T:
			case MODULE2_T:
			case MODULE3_T:
			case MODULE4_T:
			case MODULE5_T:
			case MODULE6_T:
			case MODULEX_T:
				cmd = t->val[0].u.data;
				rve_param_no = 0;
				if (cmd) {
					LM_DBG("fixing %s()\n", cmd->name);
					if (t->val[1].u.number==0) {
						ret = call_fixup(cmd->fixup, 0, 0);
						if (ret < 0)
							goto error;
					}
					for (i=0; i < t->val[1].u.number; i++) {
						if (t->val[i+2].type == RVE_ST) {
							rve = t->val[i+2].u.data;
							if (rve_is_constant(rve)) {
								/* if expression is constant => evaluate it
								   as string and replace it with the corresp.
								   string */
								rv = rval_expr_eval(0, 0, rve);
								if (rv == 0 ||
										rval_get_str( 0, 0, &s, rv, 0) < 0 ) {
									ERR("failed to fix constant rve");
									if (rv) rval_destroy(rv);
									ret = E_BUG;
									goto error;
								}
								rval_destroy(rv);
								rve_destroy(rve);
								t->val[i+2].type = STRING_ST;/*asciiz string*/
								t->val[i+2].u.string = s.s;
								/* len is not used for now */
								t->val[i+2].u.str.len = s.len;
								tmp_p = t->val[i+2].u.data;
								ret = call_fixup(cmd->fixup,
												&t->val[i+2].u.data, i+1);
								if (t->val[i+2].u.data != tmp_p)
									t->val[i+2].type = MODFIXUP_ST;
								if (ret < 0)
									goto error;
							} else {
								/* expression is not constant => fixup &
								   optimize it */
								rve_param_no++;
								if ((ret=fix_rval_expr(t->val[i+2].u.data))
										< 0) {
									ERR("rve fixup failed\n");
									ret = E_BUG;
									goto error;
								}
							}
						} else  if (t->val[i+2].type == STRING_ST) {
							tmp_p = t->val[i+2].u.data;
							ret = call_fixup(cmd->fixup,
											&t->val[i+2].u.data, i+1);
							if (t->val[i+2].u.data != tmp_p)
								t->val[i+2].type = MODFIXUP_ST;
							if (ret < 0)
								goto error;
						} else {
							BUG("invalid module function param type %d\n",
									t->val[i+2].type);
							ret = E_BUG;
							goto error;
						}
					} /* for */
					/* here all the params are either STRING_ST
					   (constant RVEs), MODFIXUP_ST (fixed up)
					   or RVE_ST (non-ct RVEs) */
					if (rve_param_no) { /* we have to fix the type */
						if (cmd->fixup &&
							!(cmd->fixup_flags & FIXUP_F_FPARAM_RVE) &&
							cmd->free_fixup == 0) {
							BUG("non-ct RVEs (%d) in module function call"
									"that does not support them (%s)\n",
									rve_param_no, cmd->name);
							ret = E_BUG;
							goto error;
						}
						switch(t->type) {
							case MODULE1_T:
								t->type = MODULE1_RVE_T;
								break;
							case MODULE2_T:
								t->type = MODULE2_RVE_T;
								break;
							case MODULE3_T:
								t->type = MODULE3_RVE_T;
								break;
							case MODULE4_T:
								t->type = MODULE4_RVE_T;
								break;
							case MODULE5_T:
								t->type = MODULE5_RVE_T;
								break;
							case MODULE6_T:
								t->type = MODULE6_RVE_T;
								break;
							case MODULEX_T:
								t->type = MODULEX_RVE_T;
								break;
							default:
								BUG("unsupported module function type %d\n",
										t->type);
								ret = E_BUG;
								goto error;
						}
					} /* if rve_param_no */
				}
				break;
			case FORCE_SEND_SOCKET_T:
				if (t->val[0].type!=SOCKID_ST){
					LM_CRIT("invalid subtype %d for force_send_socket\n",
								t->val[0].type);
					ret = E_BUG;
					goto error;
				}
				he=resolvehost(
						((struct socket_id*)t->val[0].u.data)->addr_lst->name
						);
				if (he==0){
					LM_ERR("force_send_socket: could not resolve %s\n",
						((struct socket_id*)t->val[0].u.data)->addr_lst->name);
					ret = E_BAD_ADDRESS;
					goto error;
				}
				hostent2ip_addr(&ip, he, 0);
				si=find_si(&ip, ((struct socket_id*)t->val[0].u.data)->port,
								((struct socket_id*)t->val[0].u.data)->proto);
				if (si==0){
					LM_ERR("bad force_send_socket argument: %s:%d (ser doesn't listen on it)\n",
						((struct socket_id*)t->val[0].u.data)->addr_lst->name,
							((struct socket_id*)t->val[0].u.data)->port);
					ret = E_BAD_ADDRESS;
					goto error;
				}
				t->val[0].u.data=si;
				t->val[0].type=SOCKETINFO_ST;
				break;
			case UDP_MTU_TRY_PROTO_T:
				if (t->val[0].type!=NUMBER_ST){
					LM_CRIT("invalid subtype %d for udp_mtu_try_proto\n",
								t->val[0].type);
					ret = E_BUG;
					goto error;
				}
				switch(t->val[0].u.number){
					case PROTO_UDP:
						t->val[0].u.number=0;
						break;
					case PROTO_TCP:
						t->val[0].u.number=FL_MTU_TCP_FB;
						break;
					case PROTO_TLS:
						t->val[0].u.number=FL_MTU_TLS_FB;
						break;
					case PROTO_SCTP:
						t->val[0].u.number=FL_MTU_SCTP_FB;
						break;
					default:
						LM_CRIT("invalid argument for udp_mtu_try_proto (%d)\n", 
									(unsigned int)t->val[0].u.number);
				}
				break;
			case APPEND_BRANCH_T:
				if (t->val[0].type!=STRING_ST){
					BUG("invalid subtype%d for append_branch_t\n",
								t->val[0].type);
					ret = E_BUG;
					goto error;
				}
				s.s=t->val[0].u.string;
				s.len=(s.s)?strlen(s.s):0;
				t->val[0].u.str=s;
				t->val[0].type=STR_ST;
				break;
			case ROUTE_T:
				if (t->val[0].type == RVE_ST) {
					rve=(struct rval_expr*)t->val[0].u.data;
					if (!rve_is_constant(rve)) {
						if ((ret=fix_rval_expr(t->val[0].u.data)) < 0){
							LM_ERR("route() failed to fix rve at %s:%d\n",
								(t->cfile)?t->cfile:"line", t->cline);
							ret = E_BUG;
							goto error;
						}
					} else {
						/* rve is constant => replace it with a string */
						if ((rv = rval_expr_eval(0, 0, rve)) == 0 ||
								rval_get_str(0, 0, &s, rv, 0) < 0) {
							/* out of mem. or bug ? */
							rval_destroy(rv);
							LM_ERR("route() failed to fix ct. rve at %s:%d\n",
								(t->cfile)?t->cfile:"line", t->cline);
							ret = E_BUG;
							goto error;
						}
						rval_destroy(rv);
						rve_destroy(rve);
						t->val[0].type = STRING_ST;
						t->val[0].u.string = s.s;
						t->val[0].u.str.len = s.len; /* not used */
						/* fall-through the STRING_ST if */
					}
				}
				if (t->val[0].type == STRING_ST) {
					i=route_lookup(&main_rt, t->val[0].u.string);
					if (i < 0) {
						LM_ERR("route \"%s\" not found at %s:%d\n",
								t->val[0].u.string,
								(t->cfile)?t->cfile:"line", t->cline);
						ret = E_SCRIPT;
						goto error;
					}
					t->val[0].type = NUMBER_ST;
					pkg_free(t->val[0].u.string);
					t->val[0].u.number = i;
				} else if (t->val[0].type != NUMBER_ST &&
							t->val[0].type != RVE_ST) {
					BUG("invalid subtype %d for route()\n",
								t->val[0].type);
					ret = E_BUG;
					goto error;
				}
				break;
			case CFG_SELECT_T:
				if (t->val[1].type == RVE_ST) {
					rve = t->val[1].u.data;
					if (rve_is_constant(rve)) {
						/* if expression is constant => evaluate it
						   as integer and replace it with the corresp.
						   int */
						rv = rval_expr_eval(0, 0, rve);
						if (rv == 0 ||
								rval_get_int( 0, 0, &i, rv, 0) < 0 ) {
							LM_ERR("failed to fix constant rve");
							if (rv) rval_destroy(rv);
							ret = E_BUG;
							goto error;
						}
						rval_destroy(rv);
						rve_destroy(rve);
						t->val[1].type = NUMBER_ST;
						t->val[1].u.number = i;
					} else {
						/* expression is not constant => fixup &
						   optimize it */
						if ((ret=fix_rval_expr(rve))
								< 0) {
							LM_ERR("rve fixup failed\n");
							ret = E_BUG;
							goto error;
						}
					}
				} else if (t->val[1].type != NUMBER_ST) {
					BUG("invalid subtype %d for cfg_select()\n",
								t->val[1].type);
					ret = E_BUG;
					goto error;
				}

			case CFG_RESET_T:
				if (t->val[0].type != STRING_ST) {
					BUG("invalid subtype %d for cfg_select() or cfg_reset()\n",
								t->val[0].type);
					ret = E_BUG;
					goto error;
				}
				tmp_p = (void *)cfg_lookup_group(t->val[0].u.string, strlen(t->val[0].u.string));
				if (!tmp_p) {
					LM_ERR("configuration group \"%s\" not found\n",
						t->val[0].u.string);
					ret = E_SCRIPT;
					goto error;
				}
				pkg_free(t->val[0].u.string);
				t->val[0].u.data = tmp_p;
				t->val[0].type = CFG_GROUP_ST;
				break;
			default:
				/* no fixup required for the rest */
				break;
		}
	}
	return 0;

error:
	LM_ERR("fixing failed (code=%d) at cfg:%s:%d\n", ret,
			(t->cfile)?t->cfile:"", t->cline);
	return ret;
}


/* Compare parameters as ordinary numbers
 *
 * Left and right operands can be either numbers or
 * attributes. If either of the attributes if of string type then the length of
 * its value will be used.
 */
inline static int comp_num(int op, long left, int rtype, union exp_op* r,
							struct sip_msg* msg, struct run_act_ctx* h)
{
	int_str val;
	pv_value_t pval;
	avp_t* avp;
	int right;

	if (unlikely(op==NO_OP)) return !(!left);
	switch(rtype){
		case AVP_ST:
			avp = search_avp_by_index(r->attr->type, r->attr->name,
										&val, r->attr->index);
			if (avp && !(avp->flags & AVP_VAL_STR)) right = val.n;
			else return (op == DIFF_OP);
			break;
		case NUMBER_ST:
			right = r->numval;
			break;
		case RVE_ST:
			if (unlikely(rval_expr_eval_int(h, msg, &right, r->param)<0))
				return (op == DIFF_OP); /* not found/invalid */
			break;
		case PVAR_ST:
			memset(&pval, 0, sizeof(pv_value_t));
			if (unlikely(pv_get_spec_value(msg, r->param, &pval)!=0)){
				return (op == DIFF_OP); /* error, not found => false */
			}
			if (likely(pval.flags & (PV_TYPE_INT|PV_VAL_INT))){
				right=pval.ri;
				pv_value_destroy(&pval);
			}else{
				pv_value_destroy(&pval);
				return (op == DIFF_OP); /* not found or invalid type */
			}
			break;
		default:
			LM_CRIT("Invalid right operand (%d)\n", rtype);
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
			LM_CRIT("unknown operator: %d\n", op);
			return E_BUG;
	}
	return E_BUG;
}

/*
 * Compare given string "left" with right side of expression
 */
inline static int comp_str(int op, str* left, int rtype, 
							union exp_op* r, struct sip_msg* msg,
							struct run_act_ctx* h)
{
	str* right;
	int_str val;
	str v;
	avp_t* avp;
	int ret;
	char backup;
	regex_t* re;
	unsigned int l;
	struct rvalue* rv;
	struct rval_cache rv_cache;
	pv_value_t pval;
	int destroy_pval;
	
	right=0; /* warning fix */
	rv=0;
	destroy_pval=0;
	if (unlikely(op==NO_OP)) return (left->s!=0);
	switch(rtype){
		case AVP_ST:
			avp = search_avp_by_index(r->attr->type, r->attr->name,
										&val, r->attr->index);
			if (likely(avp && (avp->flags & AVP_VAL_STR))) right = &val.s;
			else return (op == DIFF_OP);
			break;
		case SELECT_ST:
			ret = run_select(&v, r->select, msg);
			if (unlikely(ret != 0)) 
				return (op == DIFF_OP); /* Not found or error */
			right = &v;
			break;
		case RVE_ST:
			rval_cache_init(&rv_cache);
			rv=rval_expr_eval(h, msg, r->param);
			if (unlikely (rv==0)) 
				return (op==DIFF_OP); /* not found or error*/
			if (unlikely(rval_get_tmp_str(h, msg, &v, rv, 0, &rv_cache)<0)){
				goto error;
			}
			right = &v;
			break;
		case PVAR_ST:
			memset(&pval, 0, sizeof(pv_value_t));
			if (unlikely(pv_get_spec_value(msg, r->param, &pval)!=0)){
				return (op == DIFF_OP); /* error, not found => false */
			}
			destroy_pval=1;
			if (likely(pval.flags & PV_VAL_STR)){
				right=&pval.rs;
			}else{
				pv_value_destroy(&pval);
				return (op == DIFF_OP); /* not found or invalid type */
			}
			break;
		case RE_ST:
			if (unlikely(op != MATCH_OP)){
				LM_CRIT("Bad operator %d, ~= expected\n", op);
				goto error;
			}
			break;
		case STRING_ST: /* strings are stored as {asciiz, len } */
		case STR_ST:
			right=&r->str;
			break;
		case NUMBER_ST:
			/* "123" > 100 is not allowed by cfg.y rules
			 * but can happen as @select or $avp evaluation
			 * $test > 10
			 * the right operator MUST be number to do the conversion
			 */
			if (str2int(left,&l) < 0)
				goto error;
			return comp_num(op, l, rtype, r, msg, h);
		default:
			LM_CRIT("Bad type %d, string or RE expected\n", rtype);
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
			 * with calls from route{}; the same goes for fline in 
			 * reply_route{};
			 *
			 * also, the received function should always give us an extra
			 * character, into which we can put the 0-terminator now;
			 * an alternative would be allocating a new piece of memory,
			 * which might be too slow
			 * -jiri
			 *
			 * janakj: AVPs are zero terminated too so this is not problem 
			 * either
			 */
			backup=left->s[left->len];
			left->s[left->len]='\0';
			switch(rtype){
				case AVP_ST:
				case SELECT_ST:
				case RVE_ST:
				case PVAR_ST:
				case STRING_ST:
				case STR_ST:
					/* we need to compile the RE on the fly */
					re=(regex_t*)pkg_malloc(sizeof(regex_t));
					if (re==0){
						LM_CRIT("memory allocation failure\n");
						left->s[left->len] = backup;
						goto error;
					}
					if (regcomp(re, right->s,
								REG_EXTENDED|REG_NOSUB|REG_ICASE)) {
						pkg_free(re);
						left->s[left->len] = backup;
						goto error;
					}
					ret=(regexec(re, left->s, 0, 0, 0)==0);
					regfree(re);
					pkg_free(re);
					break;
				case RE_ST:
					ret=(regexec(r->re, left->s, 0, 0, 0)==0);
					break;
				default:
					LM_CRIT("Bad operator type %d, for ~= \n", rtype);
					goto error;
			}
			left->s[left->len] = backup;
			break;
		default:
			LM_CRIT("unknown op %d\n", op);
			goto error;
	}
	if (rv){
		rval_cache_clean(&rv_cache);
		rval_destroy(rv);
	}
	if (destroy_pval)
		pv_value_destroy(&pval);
	return ret;

error:
	if (rv){
		rval_cache_clean(&rv_cache);
		rval_destroy(rv);
	}
	if (destroy_pval)
		pv_value_destroy(&pval);
	return (op == DIFF_OP) ? 1 : -1;
}


/* eval_elem helping function, returns str op param */
inline static int comp_string(int op, char* left, int rtype, union exp_op* r,
								struct sip_msg* msg, struct run_act_ctx* h)
{
	str s;
	
	s.s=left;
	s.len=strlen(left);
	return comp_str(op, &s, rtype, r, msg, h);
}


inline static int comp_avp(int op, avp_spec_t* spec, int rtype,
							union exp_op* r, struct sip_msg* msg,
							struct run_act_ctx* h)
{
	avp_t* avp;
	int_str val;
	union exp_op num_val;
	str tmp;
	unsigned int uval;

	if (spec->type & AVP_INDEX_ALL) {
		avp = search_first_avp(spec->type & ~AVP_INDEX_ALL, spec->name,
								NULL, NULL);
		return (avp!=0);
	}
	avp = search_avp_by_index(spec->type, spec->name, &val, spec->index);
	if (!avp) return (op == DIFF_OP);

	if (op==NO_OP){
		if (avp->flags & AVP_VAL_STR) {
			return val.s.len!=0;
		} else {
			return val.n != 0;
		}
	}
	if (avp->flags & AVP_VAL_STR) {
		return comp_str(op, &val.s, rtype, r, msg, h);
	} else {
		switch(rtype){
			case NUMBER_ST:
			case AVP_ST:
			case RVE_ST:
			case PVAR_ST:
				return comp_num(op, val.n, rtype, r, msg, h);
				break;
			case STRING_ST:
				tmp.s=r->string;
				tmp.len=strlen(r->string);
				if (str2int(&tmp, &uval)<0){
					LM_WARN("cannot convert string value to int (%s)\n",
								ZSW(r->string));
					goto error;
				}
				num_val.numval=uval;
				return comp_num(op, val.n, NUMBER_ST, &num_val, msg, h);
			case STR_ST:
				if (str2int(&r->str, &uval)<0){
					LM_WARN("cannot convert str value to int (%.*s)\n",
								r->str.len, ZSW(r->str.s));
					goto error;
				}
				num_val.numval=uval;
				return comp_num(op, val.n, NUMBER_ST, &num_val, msg, h);
			default:
				LM_CRIT("invalid type for numeric avp comparison (%d)\n",
								rtype);
				goto error;
		}
	}
error:
	return (op == DIFF_OP) ? 1 : -1;
}

/*
 * Left side of expression was select
 */
inline static int comp_select(int op, select_t* sel, int rtype,
								union exp_op* r, struct sip_msg* msg,
								struct run_act_ctx* h)
{
	int ret;
	str val;
	char empty_str=0;

	ret = run_select(&val, sel, msg);
	if (ret != 0) return (op == DIFF_OP);

	if (op==NO_OP) return (val.len>0);
	if (unlikely(val.len==0)) {
		/* make sure the string pointer uses accessible memory range
		 * the comp_str function might dereference it
		 */
		val.s=&empty_str;
	}
	return comp_str(op, &val, rtype, r, msg, h);
}


inline static int comp_rve(int op, struct rval_expr* rve, int rtype,
							union exp_op* r, struct sip_msg* msg,
							struct run_act_ctx* h)
{
	int i;
	struct rvalue* rv;
	struct rvalue* rv1;
	struct rval_cache c1;
	
	rval_cache_init(&c1);
	if (unlikely(rval_expr_eval_rvint(h,  msg, &rv, &i, rve, &c1)<0)){
		LM_ERR("failure evaluating expression: bad type\n");
		i=0; /* false */
		goto int_expr;
	}
	if (unlikely(rv)){
		/* no int => str */
		rv1=rval_convert(h, msg, RV_STR, rv, &c1);
		i=comp_str(op, &rv1->v.s, rtype, r, msg, h);
		rval_destroy(rv1);
		rval_destroy(rv);
		rval_cache_clean(&c1);
		return i;
	}
	/* expr evaluated to int */
int_expr:
	rval_cache_clean(&c1);
	if (op==NO_OP)
		return !(!i); /* transform it into { 0, 1 } */
	return comp_num(op, i, rtype, r, msg, h);
}



inline static int comp_pvar(int op, pv_spec_t* pvs, int rtype,
							union exp_op* r, struct sip_msg* msg,
							struct run_act_ctx* h)
{
	pv_value_t pval;
	int ret;
	
	ret=0;
	memset(&pval, 0, sizeof(pv_value_t));
	if (unlikely(pv_get_spec_value(msg, r->param, &pval)!=0)){
		return 0; /* error, not found => false */
	}
	if (likely(pval.flags & PV_TYPE_INT)){
		if (op==NO_OP)
			ret=!(!pval.ri);
		else
			ret=comp_num(op, pval.ri, rtype, r, msg, h);
	}else if ((pval.flags==PV_VAL_NONE) ||
			(pval.flags & (PV_VAL_NULL|PV_VAL_EMPTY))){
		if (op==NO_OP)
			ret=0;
		else
			ret=comp_num(op, 0, rtype, r, msg, h);
	}else{
		ret=pval.rs.len!=0;
		if (op!=NO_OP)
			ret=comp_num(op, ret, rtype, r, msg, h);
	}
	pv_value_destroy(&pval);
	return ret;
}



/* check_self wrapper -- it checks also for the op */
inline static int check_self_op(int op, str* s, unsigned short p)
{
	int ret;

	ret=check_self(s, p, 0);
	switch(op){
		case EQUAL_OP:
		case MATCH_OP:
			break;
		case DIFF_OP:
			ret=(ret > 0) ? 0 : 1;
			break;
		default:
			LM_CRIT("invalid operator %d\n", op);
			ret=-1;
	}
	return ret;
}


/* eval_elem helping function, returns an op param */
inline static int comp_ip(int op, struct ip_addr* ip, int rtype,
							union exp_op* r, struct sip_msg* msg,
							struct run_act_ctx *ctx )
{
	struct hostent* he;
	char ** h;
	int ret;
	str tmp;
	str* right;
	struct net net;
	union exp_op r_expop;
	struct rvalue* rv;
	struct rval_cache rv_cache;
	avp_t* avp;
	int_str val;
	pv_value_t pval;
	int destroy_pval;

	right=NULL; /* warning fix */
	rv=NULL;
	destroy_pval=0;
	ret=-1;
	he=NULL; /* warning fix */
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
			return ret; /* exit directly */
		case MYSELF_ST: /* check if it's one of our addresses*/
			tmp.s=ip_addr2a(ip);
			tmp.len=strlen(tmp.s);
			ret=check_self_op(op, &tmp, 0);
			return ret;
		case STRING_ST:
		case STR_ST:
			right=&r->str;
			break;
		case RVE_ST:
			rval_cache_init(&rv_cache);
			rv=rval_expr_eval(ctx, msg, r->param);
			if (unlikely (rv==0))
				return (op==DIFF_OP); /* not found or error*/
			if (unlikely(rval_get_tmp_str(ctx, msg, &tmp, rv, 0, &rv_cache)
							< 0)){
				goto error;
			}
			right = &tmp;
			break;
		case AVP_ST:
			/* we can still have AVP_ST due to the RVE optimisations
			   (if a RVE == $avp => rve wrapper removed => pure avp) */
			avp = search_avp_by_index(r->attr->type, r->attr->name,
										&val, r->attr->index);
			if (likely(avp && (avp->flags & AVP_VAL_STR))) right = &val.s;
			else return (op == DIFF_OP);
			break;
		case SELECT_ST:
			/* see AVP_ST comment and s/AVP_ST/SELECT_ST/ */
			ret = run_select(&tmp, r->select, msg);
			if (unlikely(ret != 0))
				return (op == DIFF_OP); /* Not found or error */
			right = &tmp;
			break;
		case PVAR_ST:
			/* see AVP_ST comment and s/AVP_ST/PVAR_ST/ */
			memset(&pval, 0, sizeof(pv_value_t));
			if (unlikely(pv_get_spec_value(msg, r->param, &pval)!=0)){
				return (op == DIFF_OP); /* error, not found => false */
			}
			destroy_pval=1;
			if (likely(pval.flags & PV_VAL_STR)){
				right=&pval.rs;
			}else{
				pv_value_destroy(&pval);
				return (op == DIFF_OP); /* not found or invalid type */
			}
			break;
		case RE_ST:
			if (unlikely(op != MATCH_OP))
				goto error_op;
			/* 1: compare with ip2str*/
			ret=comp_string(op, ip_addr2a(ip), rtype, r, msg, ctx);
			if (likely(ret==1))
				return ret;
			/* 3: (slow) rev dns the address
			* and compare with all the aliases
			* !!??!! review: remove this? */
			if (unlikely((received_dns & DO_REV_DNS) &&
				((he=rev_resolvehost(ip))!=0) )){
				/*  compare with primary host name */
				ret=comp_string(op, he->h_name, rtype, r, msg, ctx);
				/* compare with all the aliases */
				for(h=he->h_aliases; (ret!=1) && (*h); h++){
					ret=comp_string(op, *h, rtype, r, msg, ctx);
				}
			}else{
				ret=0;
			}
			return ret;
		default:
			LM_CRIT("invalid type for src_ip or dst_ip (%d)\n", rtype);
			return -1;
	}
	/* here "right" is set to the str we compare with */
	r_expop.str=*right;
	switch(op){
		case EQUAL_OP:
			/* 0: try if ip or network (ip/mask) */
			if (mk_net_str(&net, right) == 0) {
				ret=(matchnet(ip, &net)==1);
				break;
			}
			/* 2: resolve (name) & compare w/ all the ips */
			he=resolvehost(right->s);
			if (he==0){
				LM_DBG("could not resolve %s\n", r->str.s);
			}else if (he->h_addrtype==ip->af){
				for(h=he->h_addr_list;(ret!=1)&& (*h); h++){
					ret=(memcmp(ip->u.addr, *h, ip->len)==0);
				}
				if (ret==1) break;
			}
			/* 3: (slow) rev dns the address
			 * and compare with all the aliases
			 * !!??!! review: remove this? */
			if (unlikely((received_dns & DO_REV_DNS) &&
							((he=rev_resolvehost(ip))!=0) )){
				/*  compare with primary host name */
				ret=comp_string(op, he->h_name, STR_ST, &r_expop, msg, ctx);
				/* compare with all the aliases */
				for(h=he->h_aliases; (ret!=1) && (*h); h++){
					ret=comp_string(op, *h, STR_ST, &r_expop, msg, ctx);
				}
			}else{
				ret=0;
			}
			break;
		case MATCH_OP:
			/* 0: try if ip or network (ip/mask)
			  (one should not use MATCH for that, but try to be nice)*/
			if (mk_net_str(&net, right) == 0) {
				ret=(matchnet(ip, &net)==1);
				break;
			}
			/* 1: compare with ip2str (but only for =~)*/
			ret=comp_string(op, ip_addr2a(ip), STR_ST, &r_expop, msg, ctx);
			if (likely(ret==1)) break;
			/* 2: resolve (name) & compare w/ all the ips */
			he=resolvehost(right->s);
			if (he==0){
				LM_DBG("could not resolve %s\n", r->str.s);
			}else if (he->h_addrtype==ip->af){
				for(h=he->h_addr_list;(ret!=1)&& (*h); h++){
					ret=(memcmp(ip->u.addr, *h, ip->len)==0);
				}
				if (ret==1) break;
			}
			/* 3: (slow) rev dns the address
			 * and compare with all the aliases
			 * !!??!! review: remove this? */
			if (unlikely((received_dns & DO_REV_DNS) &&
							((he=rev_resolvehost(ip))!=0) )){
				/*  compare with primary host name */
				ret=comp_string(op, he->h_name, STR_ST, &r_expop, msg, ctx);
				/* compare with all the aliases */
				for(h=he->h_aliases; (ret!=1) && (*h); h++){
					ret=comp_string(op, *h, STR_ST, &r_expop, msg, ctx);
				}
			}else{
				ret=0;
			}
			break;
		case DIFF_OP:
			ret=(comp_ip(EQUAL_OP, ip, STR_ST, &r_expop, msg, ctx) > 0)?0:1;
		break;
		default:
			goto error_op;
	}
	if (rv){
		rval_cache_clean(&rv_cache);
		rval_destroy(rv);
	}
	if (destroy_pval)
		pv_value_destroy(&pval);
	return ret;
error_op:
	LM_CRIT("invalid operator %d for type %d\n", op, rtype);
error:
	if (unlikely(rv)){
		rval_cache_clean(&rv_cache);
		rval_destroy(rv);
	}
	if (destroy_pval)
		pv_value_destroy(&pval);
	return -1;
}



/* returns: 0/1 (false/true) or -1 on error */
inline static int eval_elem(struct run_act_ctx* h, struct expr* e, 
								struct sip_msg* msg)
{
	struct sip_uri uri;
	int ret;
	struct onsend_info* snd_inf;
	struct ip_addr ip;
	ret=E_BUG;

	if (e->type!=ELEM_T){
		LM_CRIT("invalid type\n");
		goto error;
	}
	switch(e->l_type){
	case METHOD_O:
		if(msg->first_line.type==SIP_REQUEST)
		{
			ret=comp_str(e->op, &msg->first_line.u.request.method,
			 			e->r_type, &e->r, msg, h);
		} else {
			if(parse_headers(msg, HDR_CSEQ_F, 0)!=0 || msg->cseq==NULL)
			{
				LM_ERR("cannot parse cseq header\n");
				goto error;
			}
			ret=comp_str(e->op, &get_cseq(msg)->method,
						e->r_type, &e->r, msg, h);
		}
		break;
	case URI_O:
		if(msg->new_uri.s) {
			if (e->r_type==MYSELF_ST){
				if (parse_sip_msg_uri(msg)<0) ret=-1;
				else ret=check_self_op(e->op, &msg->parsed_uri.host,
								GET_URI_PORT(&msg->parsed_uri));
			}else{
				ret=comp_str(e->op, &msg->new_uri,
								e->r_type, &e->r, msg, h);
			}
		}else{
			if (e->r_type==MYSELF_ST){
				if (parse_sip_msg_uri(msg)<0) ret=-1;
				else ret=check_self_op(e->op, &msg->parsed_uri.host,
								GET_URI_PORT(&msg->parsed_uri));
			}else{
				ret=comp_str(e->op, &msg->first_line.u.request.uri,
								e->r_type, &e->r, msg, h);
			}
		}
		break;

	case FROM_URI_O:
		if (parse_from_header(msg)!=0){
			LM_ERR("bad or missing From: header\n");
			goto error;
		}
		if (e->r_type==MYSELF_ST){
			if (parse_uri(get_from(msg)->uri.s, get_from(msg)->uri.len,
				      &uri) < 0){
				LM_ERR("bad uri in From:\n");
				goto error;
			}
			ret=check_self_op(e->op, &uri.host, GET_URI_PORT(&uri));
		}else{
			ret=comp_str(e->op, &get_from(msg)->uri,
							e->r_type, &e->r, msg, h);
		}
		break;

	case TO_URI_O:
		if ((msg->to==0) && ((parse_headers(msg, HDR_TO_F, 0)==-1) ||
				     (msg->to==0))){
			LM_ERR("bad or missing To: header\n");
			goto error;
		}
		     /* to content is parsed automatically */
		if (e->r_type==MYSELF_ST){
			if (parse_uri(get_to(msg)->uri.s, get_to(msg)->uri.len,
				      &uri) < 0){
				LM_ERR("bad uri in To:\n");
				goto error;
			}
			ret=check_self_op(e->op, &uri.host, GET_URI_PORT(&uri));
		}else{
			ret=comp_str(e->op, &get_to(msg)->uri,
							e->r_type, &e->r, msg, h);
		}
		break;

	case SRCIP_O:
		ret=comp_ip(e->op, &msg->rcv.src_ip, e->r_type, &e->r, msg, h);
		break;

	case DSTIP_O:
		ret=comp_ip(e->op, &msg->rcv.dst_ip, e->r_type, &e->r, msg, h);
		break;

	case SNDIP_O:
		snd_inf=get_onsend_info();
		if (likely(snd_inf && snd_inf->send_sock)){
			ret=comp_ip(e->op, &snd_inf->send_sock->address,
						e->r_type, &e->r, msg, h);
		}else{
			BUG("eval_elem: snd_ip unknown (not in a onsend_route?)\n");
		}
		break;

	case TOIP_O:
		snd_inf=get_onsend_info();
		if (likely(snd_inf && snd_inf->to)){
			su2ip_addr(&ip, snd_inf->to);
			ret=comp_ip(e->op, &ip, e->r_type, &e->r, msg, h);
		}else{
			BUG("eval_elem: to_ip unknown (not in a onsend_route?)\n");
		}
		break;

	case NUMBER_O:
		ret=!(!e->r.numval); /* !! to transform it in {0,1} */
		break;

	case ACTION_O:
		ret=run_actions(h, (struct action*)e->r.param, msg);
		if (ret<=0) ret=0;
		else ret=1;
		break;

	case SRCPORT_O:
		ret=comp_num(e->op, (int)msg->rcv.src_port, e->r_type, &e->r, msg, h);
		break;

	case DSTPORT_O:
		ret=comp_num(e->op, (int)msg->rcv.dst_port, e->r_type, &e->r, msg, h);
		break;

	case SNDPORT_O:
		snd_inf=get_onsend_info();
		if (likely(snd_inf && snd_inf->send_sock)){
			ret=comp_num(e->op, (int)snd_inf->send_sock->port_no,
							e->r_type, &e->r, msg, h);
		}else{
			BUG("eval_elem: snd_port unknown (not in a onsend_route?)\n");
		}
		break;

	case TOPORT_O:
		snd_inf=get_onsend_info();
		if (likely(snd_inf && snd_inf->to)){
			ret=comp_num(e->op, (int)su_getport(snd_inf->to),
								e->r_type, &e->r, msg, h);
		}else{
			BUG("eval_elem: to_port unknown (not in a onsend_route?)\n");
		}
		break;

	case PROTO_O:
		ret=comp_num(e->op, msg->rcv.proto, e->r_type, &e->r, msg, h);
		break;

	case SNDPROTO_O:
		snd_inf=get_onsend_info();
		if (likely(snd_inf && snd_inf->send_sock)){
			ret=comp_num(e->op, snd_inf->send_sock->proto,
							e->r_type, &e->r, msg, h);
		}else{
			BUG("eval_elem: snd_proto unknown (not in a onsend_route?)\n");
		}
		break;

	case AF_O:
		ret=comp_num(e->op, (int)msg->rcv.src_ip.af, e->r_type, &e->r, msg, h);
		break;

	case SNDAF_O:
		snd_inf=get_onsend_info();
		if (likely(snd_inf && snd_inf->send_sock)){
			ret=comp_num(e->op, snd_inf->send_sock->address.af,
							e->r_type, &e->r, msg, h);
		}else{
			BUG("eval_elem: snd_af unknown (not in a onsend_route?)\n");
		}
		break;

	case MSGLEN_O:
		if ((snd_inf=get_onsend_info())!=0){
			ret=comp_num(e->op, (int)snd_inf->len, e->r_type, &e->r, msg, h);
		}else{
			ret=comp_num(e->op, (int)msg->len, e->r_type, &e->r, msg, h);
		}
		break;

	case RETCODE_O:
		ret=comp_num(e->op, h->last_retcode, e->r_type, &e->r, msg, h);
		break;

	case AVP_O:
		ret = comp_avp(e->op, e->l.attr, e->r_type, &e->r, msg, h);
		break;

	case SELECT_O:
		ret = comp_select(e->op, e->l.select, e->r_type, &e->r, msg, h);
		break;

	case RVEXP_O:
		ret = comp_rve(e->op, e->l.param, e->r_type, &e->r, msg, h);
		break;

	case PVAR_O:
		ret=comp_pvar(e->op, e->l.param, e->r_type, &e->r, msg, h);
		break;

	case SELECT_UNFIXED_O:
		BUG("unexpected unfixed select operand %d\n", e->l_type);
		break;
/*
	default:
		LM_CRIT("invalid operand %d\n", e->l_type);
*/
	}
	return ret;
error:
	return (e->op == DIFF_OP) ? 1 : -1;
}



/* ret= 1/0 (true/false) ,  -1 on error (evaluates as false)*/
int eval_expr(struct run_act_ctx* h, struct expr* e, struct sip_msg* msg)
{
	int ret;

	if (e->type==ELEM_T){
		ret=eval_elem(h, e, msg);
	}else if (e->type==EXP_T){
		switch(e->op){
			case LOGAND_OP:
				ret=eval_expr(h, e->l.expr, msg);
				/* if error or false stop evaluating the rest */
				if (ret <= 0) break;
				ret=eval_expr(h, e->r.expr, msg); /*ret1 is 1*/
				break;
			case LOGOR_OP:
				ret=eval_expr(h, e->l.expr, msg);
				/* if true stop evaluating the rest */
				if (ret > 0) break;
				ret=eval_expr(h, e->r.expr, msg); /* ret1 is 0 */
				break;
			case NOT_OP:
				ret=eval_expr(h, e->l.expr, msg);
				ret=(ret > 0) ? 0 : 1;
				break;
			default:
				LM_CRIT("unknown op %d\n", e->op);
				ret=-1;
		}
	}else{
		LM_CRIT("unknown type %d\n", e->type);
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

	LM_DBG("fixing actions...\n");
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
	if ((ret=fix_rl(&event_rt))!=0)
		return ret;

	return 0;
}



static void print_rl(struct route_list* rt, char* name)
{
	int j;
	
	for(j=0; j<rt->entries; j++){
		if (rt->rlist[j]==0){
			if ((j==0) && (rt==&main_rt))
				LM_DBG("WARNING: the main routing table is empty\n");
			continue;
		}
		LM_DBG("%s routing table %d:\n", name, j);
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
	print_rl(&event_rt, "event");
}
