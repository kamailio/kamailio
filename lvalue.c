/* 
 * $Id$
 * 
 * Copyright (C) 2008 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @file 
 * @brief lvalues (assignment)
 */
/* 
 * History:
 * --------
 *  2008-11-30  initial version (andrei)
 */

#include "lvalue.h"
#include "dprint.h"
#include "route.h"



/** eval rve and assign the result to an avp
 * lv->lv.avp=eval(rve)
 *
 * based on do_action() ASSIGN_T
 *
 * @param h  - script context
 * @param msg - sip msg
 * @param lv - lvalue
 * @param rve - rvalue expression
 * @return >= 0 on success (expr. bool value), -1 on error
 */
inline static int lval_avp_assign(struct run_act_ctx* h, struct sip_msg* msg,
									struct lvalue* lv, struct rvalue* rv)
{
	avp_spec_t* avp;
	avp_t* r_avp;
	avp_t* avp_mark;
	pv_value_t pval;
	int_str value;
	unsigned short flags;
	struct search_state st;
	int ret;
	int v;
	int destroy_pval;
	
	destroy_pval=0;
	avp=&lv->lv.avps;
	ret=0;
	/* If the left attr was specified without indexing brackets delete
	 * existing AVPs before adding new ones */
	if ((avp->type & AVP_INDEX_ALL) != AVP_INDEX_ALL)
		delete_avp(avp->type, avp->name);
	switch(rv->type){
		case RV_NONE:
			BUG("non-intialized rval / rval expr \n");
			goto error;
		case RV_INT:
			value.n=rv->v.l;
			flags=avp->type;
			ret=!(!value.n);
			break;
		case RV_STR:
			value.s=rv->v.s;
			flags=avp->type | AVP_VAL_STR;
			ret=(value.s.len>0);
			break;
		case RV_ACTION_ST:
			flags=avp->type;
			if (rv->v.action)
				value.n=run_actions(h, rv->v.action, msg);
			else
				value.n=-1;
			ret=value.n;
			break;
		case RV_BEXPR: /* logic/boolean expr. */
			value.n=eval_expr(h, rv->v.bexpr, msg);
			if (unlikely(value.n<0)){
				if (value.n==EXPR_DROP) /* hack to quit on drop */
					goto drop;
				WARN("error in expression\n");
				value.n=0; /* expr. is treated as false */
			}
			flags=avp->type;
			ret=value.n;
			break;
		case RV_SEL:
			v=run_select(&value.s, &rv->v.sel, msg);
			if (unlikely(v!=0)){
				if (v<0){
					ret=-1;
					goto error;
				}else { /* v>0 */
					value.s.s="";
					value.s.len=0;
				}
			}
			flags=avp->type;
			ret=(value.s.len>0);
			break;
		case RV_AVP:
			avp_mark=0;
			if (unlikely((rv->v.avps.type & AVP_INDEX_ALL) == AVP_INDEX_ALL)){
				r_avp = search_first_avp(rv->v.avps.type, rv->v.avps.name,
											&value, &st);
				while(r_avp){
					/* We take only the type and name from the source avp
					 * and reset the class and track flags */
					flags=(avp->type & ~AVP_INDEX_ALL) | 
							(r_avp->flags & ~(AVP_CLASS_ALL|AVP_TRACK_ALL));
					if (add_avp_before(avp_mark, flags, avp->name, value)<0){
						ERR("failed to assign avp\n");
						ret=-1;
						goto error;
					}
					/* move the mark, so the next found AVP will come before
					   the one currently added so they will have the same 
					   order as in the source list */
					if (avp_mark) avp_mark=avp_mark->next;
					else
						avp_mark=search_first_avp(flags, avp->name, 0, 0);
					r_avp=search_next_avp(&st, &value);
				}
				ret=1;
				goto end;
			}else{
				r_avp = search_avp_by_index(rv->v.avps.type, rv->v.avps.name,
											&value, rv->v.avps.index);
				if (likely(r_avp)){
					flags=avp->type | (r_avp->flags & 
								~(AVP_CLASS_ALL|AVP_TRACK_ALL));
					ret=1;
				}else{
					ret=-1;
					goto error;
				}
			}
			break;
		case RV_PVAR:
			flags=avp->type;
			memset(&pval, 0, sizeof(pval));
			if (likely(pv_get_spec_value(msg, &rv->v.pvs, &pval)==0)){
				destroy_pval=1;
				if (pval.flags & PV_VAL_STR){
					value.s=pval.rs;
					ret=(value.s.len>0);
				}else if (pval.flags & PV_TYPE_INT){
					value.n=pval.ri;
					ret=value.n;
				}else if (pval.flags==PV_VAL_NONE ||
							(pval.flags & (PV_VAL_NULL|PV_VAL_EMPTY))){
					value.s.s="";
					value.s.len=0;
					ret=0;
				}
			}else{
				/* non existing pvar */
				value.s.s="";
				value.s.len=0;
				ret=0;
			}
			break;
	}
	if (add_avp(flags & ~AVP_INDEX_ALL, avp->name, value) < 0) {
		ERR("failed to assign value to avp\n");
		goto error;
	}
end:
	if (destroy_pval) pv_value_destroy(&pval);
	return ret;
error:
	if (destroy_pval) pv_value_destroy(&pval);
	return -1;
drop:
	if (destroy_pval) pv_value_destroy(&pval);
	return EXPR_DROP;
}



/** eval rve and assign the result to a pvar
 * lv->lv.pvar=eval(rve)
 *
 * based on do_action() ASSIGN_T
 *
 * @param h  - script context
 * @param msg - sip msg
 * @param lv - lvalue
 * @param rve - rvalue expression
 * @return >= 0 on success (expr. bool value), -1 on error
 */
inline static int lval_pvar_assign(struct run_act_ctx* h, struct sip_msg* msg,
									struct lvalue* lv, struct rvalue* rv)
{
	pv_spec_t* pvar;
	pv_value_t pval;
	avp_t* r_avp;
	int_str avp_val;
	int ret;
	int v;
	int destroy_pval;
	
	destroy_pval=0;
	pvar=&lv->lv.pvs;
	if (unlikely(!pv_is_w(pvar))){
		ERR("read only pvar\n");
		goto error;
	}
	memset(&pval, 0, sizeof(pval));
	ret=0;
	switch(rv->type){
		case RV_NONE:
			BUG("non-intialized rval / rval expr \n");
			goto error;
		case RV_INT:
			pval.flags=PV_TYPE_INT|PV_VAL_INT;
			pval.ri=rv->v.l;
			ret=!(!pval.ri);
			break;
		case RV_STR:
			pval.flags=PV_VAL_STR;
			pval.rs=rv->v.s;
			ret=(pval.rs.len>0);
			break;
		case RV_ACTION_ST:
			pval.flags=PV_TYPE_INT|PV_VAL_INT;
			if (rv->v.action)
				pval.ri=run_actions(h, rv->v.action, msg);
			else
				pval.ri=0;
			ret=pval.ri;
			break;
		case RV_BEXPR: /* logic/boolean expr. */
			pval.flags=PV_TYPE_INT|PV_VAL_INT;
			pval.ri=eval_expr(h, rv->v.bexpr, msg);
			if (unlikely(pval.ri<0)){
				if (pval.ri==EXPR_DROP) /* hack to quit on drop */
					goto drop;
				WARN("error in expression\n");
				pval.ri=0; /* expr. is treated as false */
			}
			ret=pval.ri;
			break;
		case RV_SEL:
			pval.flags=PV_VAL_STR;
			v=run_select(&pval.rs, &rv->v.sel, msg);
			if (unlikely(v!=0)){
				if (v<0){
					ret=-1;
					goto error;
				}else { /* v>0 */
					pval.rs.s="";
					pval.rs.len=0;
				}
			}
			ret=(pval.rs.len)>0;
			break;
		case RV_AVP:
				r_avp = search_avp_by_index(rv->v.avps.type, rv->v.avps.name,
											&avp_val, rv->v.avps.index);
				if (likely(r_avp)){
					if (r_avp->flags & AVP_VAL_STR){
						pval.flags=PV_VAL_STR;
						pval.rs=avp_val.s;
						ret=(pval.rs.len>0);
					}else{
						pval.flags=PV_TYPE_INT|PV_VAL_INT;
						pval.ri=avp_val.n;
						ret=!(!pval.ri);
					}
				}else{
					ret=-1;
					goto error;
				}
			break;
		case RV_PVAR:
			memset(&pval, 0, sizeof(pval));
			if (likely(pv_get_spec_value(msg, &rv->v.pvs, &pval)==0)){
				destroy_pval=1;
				if (pval.flags & PV_VAL_STR){
					ret=(pval.rs.len>0);
				}else if (pval.flags & PV_TYPE_INT){
					ret=!(!pval.ri);
				}else{
					ERR("no value in pvar assignment rval\n");
					ret=-1;
					goto error;
				}
			}else{
				ERR("non existing right pvar\n");
				ret=-1;
				goto error;
			}
			break;
	}
	if (unlikely(pvar->setf(msg, &pvar->pvp, EQ_T, &pval)<0)){
		ERR("setting pvar failed\n");
		goto error;
	}
	if (destroy_pval) pv_value_destroy(&pval);
	return ret;
error:
	if (destroy_pval) pv_value_destroy(&pval);
	return -1;
drop:
	if (destroy_pval) pv_value_destroy(&pval);
	return EXPR_DROP;
}



/** eval rve and assign the result to lv
 * lv=eval(rve)
 *
 * @param h  - script context
 * @param msg - sip msg
 * @param lv - lvalue
 * @param rve - rvalue expression
 * @return >= 0 on success (expr. bool value), -1 on error
 */
int lval_assign(struct run_act_ctx* h, struct sip_msg* msg, 
				struct lvalue* lv, struct rval_expr* rve)
{
	struct rvalue* rv;
	int ret;
	
	ret=0;
	rv=rval_expr_eval(h, msg, rve);
	if (unlikely(rv==0)){
		ERR("rval expression evaluation failed\n");
		goto error;
	}
	switch(lv->type){
		case LV_NONE:
			BUG("uninitialized/invalid lvalue (%d)\n", lv->type);
			goto error;
		case LV_AVP:
			ret=lval_avp_assign(h, msg, lv, rv);
			break;
		case LV_PVAR:
			ret=lval_pvar_assign(h, msg, lv, rv);
			break;
	}
	rval_destroy(rv);
	return ret;
error:
	if (rv) rval_destroy(rv);
	return -1;
}
