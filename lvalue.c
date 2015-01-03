/* 
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
 * @brief Kamailio core :: lvalues (assignment)
 * \ingroup core
 * Module: \ref core
 */

#include "lvalue.h"
#include "dprint.h"
#include "route.h"

/* callback to log assign actions */
static log_assign_action_f _log_assign_action = NULL;

/**
 * @brief set callback function log assign actions
 */
void set_log_assign_action_cb(log_assign_action_f f)
{
	_log_assign_action = f;
}

/**
 * @brief eval rve and assign the result to an avp
 * 
 * eval rve and assign the result to an avp, lv->lv.avp=eval(rve)
 * based on do_action() ASSIGN_T.
 * @param h  - script context
 * @param msg - sip msg
 * @param lv - lvalue
 * @param rv - rvalue expression
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
	int ret, v, destroy_pval;
	int avp_add;

#if 0
	#define AVP_ASSIGN_NOVAL() \
		/* unknown value => reset the avp in function of its type */ \
		flags=avp->type; \
		if (flags & AVP_VAL_STR){ \
			value.s.s=""; \
			value.s.len=0; \
		}else{ \
			value.n=0; \
		}
#endif
	#define AVP_ASSIGN_NOVAL() \
		/* no value => delete avp */ \
		avp_add=0
	
	destroy_pval=0;
	flags = 0;
	avp=&lv->lv.avps;
	ret=0;
	avp_add=1;
	
	switch(rv->type){
		case RV_NONE:
			BUG("non-intialized rval / rval expr \n");
			/* unknown value => reset the avp in function of its type */
			flags=avp->type;
			AVP_ASSIGN_NOVAL();
			ret=-1;
			break;
		case RV_INT:
			value.n=rv->v.l;
			flags=avp->type & ~AVP_VAL_STR;
			ret=!(!value.n);
			break;
		case RV_STR:
			value.s=rv->v.s;
			flags=avp->type | AVP_VAL_STR;
			ret=(value.s.len>0);
			break;
		case RV_ACTION_ST:
			flags=avp->type & ~AVP_VAL_STR;
			if (rv->v.action) {
				value.n=run_actions_safe(h, rv->v.action, msg);
				h->run_flags &= ~(RETURN_R_F|BREAK_R_F); /* catch return &
														    break in expr*/
			} else
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
			flags=avp->type & ~AVP_VAL_STR;
			ret=value.n;
			break;
		case RV_SEL:
			flags=avp->type|AVP_VAL_STR;
			v=run_select(&value.s, &rv->v.sel, msg);
			if (unlikely(v!=0)){
				value.s.s="";
				value.s.len=0;
				if (v<0){
					ret=-1;
					break;
				} /* v>0 */
			}
			ret=(value.s.len>0);
			break;
		case RV_AVP:
			avp_mark=0;
			if (unlikely((rv->v.avps.type & AVP_INDEX_ALL) == AVP_INDEX_ALL)){
				/* special case: add the value to the avp */
				r_avp = search_first_avp(rv->v.avps.type, rv->v.avps.name,
											&value, &st);
				while(r_avp){
					/* We take only the val type  from the source avp
					 * and reset the class, track flags and name type  */
					flags=(avp->type & ~(AVP_INDEX_ALL|AVP_VAL_STR)) | 
							(r_avp->flags & ~(AVP_CLASS_ALL|AVP_TRACK_ALL|
												AVP_NAME_STR|AVP_NAME_RE));
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
				/* normal case, value is replaced */
				r_avp = search_avp_by_index(rv->v.avps.type, rv->v.avps.name,
											&value, rv->v.avps.index);
				if (likely(r_avp)){
					/* take only the val type from the source avp
					 * and reset the class, track flags and name type  */
					flags=(avp->type & ~AVP_VAL_STR) | (r_avp->flags & 
								~(AVP_CLASS_ALL|AVP_TRACK_ALL|AVP_NAME_STR|
									AVP_NAME_RE));
					ret=1;
				}else{
					/* on error, keep the type of the assigned avp, but
					   reset it to an empty value */
					AVP_ASSIGN_NOVAL();
					ret=0;
					break;
				}
			}
			break;
		case RV_PVAR:
			memset(&pval, 0, sizeof(pval));
			if (likely(pv_get_spec_value(msg, &rv->v.pvs, &pval)==0)){
				destroy_pval=1;
				if (pval.flags & PV_TYPE_INT){
					value.n=pval.ri;
					ret=value.n;
					flags=avp->type & ~AVP_VAL_STR;
				}else if (pval.flags & PV_VAL_STR){
					value.s=pval.rs;
					ret=(value.s.len>0);
					flags=avp->type | AVP_VAL_STR;
				}else if (pval.flags==PV_VAL_NONE ||
							(pval.flags & (PV_VAL_NULL|PV_VAL_EMPTY))){
					AVP_ASSIGN_NOVAL();
					ret=0;
				}
			}else{
				/* non existing pvar */
				/* on error, keep the type of the assigned avp, but
				   reset it to an empty value */
				AVP_ASSIGN_NOVAL();
				ret=0;
			}
			break;
	}
	/* If the left attr was specified without indexing brackets delete
	 * existing AVPs before adding the new value */
	delete_avp(avp->type, avp->name);
	if (avp_add && (add_avp(flags & ~AVP_INDEX_ALL, avp->name, value) < 0)) {
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



/**
 * @brief eval rve and assign the result to a pvar
 *
 * eval rve and assign the result to a pvar, lv->lv.pvar=eval(rve)
 * based on do_action() ASSIGN_T.
 * @param h  - script context
 * @param msg - sip msg
 * @param lv - lvalue
 * @param rv - rvalue expression
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
	
	#define PVAR_ASSIGN_NOVAL() \
		/* no value found => "undefine" */ \
		pv_get_null(msg, 0, &pval)
	
	destroy_pval=0;
	pvar=lv->lv.pvs;
	if (unlikely(!pv_is_w(pvar))){
		ERR("read only pvar\n");
		goto error;
	}
	memset(&pval, 0, sizeof(pval));
	ret=0;
	switch(rv->type){
		case RV_NONE:
			BUG("non-intialized rval / rval expr \n");
			PVAR_ASSIGN_NOVAL();
			ret=-1;
			break;
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
			if (rv->v.action) {
				pval.ri=run_actions_safe(h, rv->v.action, msg);
				h->run_flags &= ~(RETURN_R_F|BREAK_R_F); /* catch return &
														    break in expr*/
			} else
				pval.ri=0;
			ret=!(!pval.ri);
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
			ret=!(!pval.ri);
			break;
		case RV_SEL:
			pval.flags=PV_VAL_STR;
			v=run_select(&pval.rs, &rv->v.sel, msg);
			if (unlikely(v!=0)){
				pval.flags|=PV_VAL_EMPTY;
				pval.rs.s="";
				pval.rs.len=0;
				if (v<0){
					ret=-1;
					break;
				}
			}
			ret=(pval.rs.len>0);
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
					PVAR_ASSIGN_NOVAL();
					ret=0; /* avp not defined (valid case) */
					break;
				}
			break;
		case RV_PVAR:
			if (likely(pv_get_spec_value(msg, &rv->v.pvs, &pval)==0)){
				destroy_pval=1;
				if (pval.flags & PV_TYPE_INT){
					ret=!(!pval.ri);
				}else if (pval.flags & PV_VAL_STR){
					ret=(pval.rs.len>0);
				}else{
					/* no value / not defined (e.g. avp) -> keep the flags */
					ret=0;
				}
			}else{
				ERR("non existing right pvar\n");
				PVAR_ASSIGN_NOVAL();
				ret=-1;
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
		ERR("rval expression evaluation failed (%d,%d-%d,%d)\n",
				rve->fpos.s_line, rve->fpos.s_col,
				rve->fpos.e_line, rve->fpos.e_col);
		goto error;
	}
	switch(lv->type){
		case LV_NONE:
			BUG("uninitialized/invalid lvalue (%d) (cfg line: %d)\n",
					lv->type, rve->fpos.s_line);
			goto error;
		case LV_AVP:
			ret=lval_avp_assign(h, msg, lv, rv);
			break;
		case LV_PVAR:
			ret=lval_pvar_assign(h, msg, lv, rv);
			break;
	}
	if (unlikely(ret<0)){
		ERR("assignment failed at pos: (%d,%d-%d,%d)\n",
			rve->fpos.s_line, rve->fpos.s_col,
			rve->fpos.e_line, rve->fpos.e_col);
	}
	else
	{
		if(unlikely(_log_assign_action!=NULL))
			_log_assign_action(msg, lv);
	}
	rval_destroy(rv);
	return ret;
error:
	if (rv) rval_destroy(rv);
	return -1;
}
