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
 * @brief rvalue expressions
 */
/* 
 * History:
 * --------
 *  2008-12-01  initial version (andrei)
 */

#include "rvalue.h"

/* minimum size alloc'ed for STR RVs (to accomodate
 * strops without reallocs) */
#define RV_STR_EXTRA 80

#define rv_ref(rv) ((rv)->refcnt++)

/** unref rv and returns true if 0 */
#define rv_unref(rv) ((--(rv)->refcnt)==0)


inline static void rval_force_clean(struct rvalue* rv)
{
	if (rv->flags & RV_CNT_ALLOCED_F){
		switch(rv->type){
			case RV_STR:
				pkg_free(rv->v.s.s);
				rv->v.s.s=0;
				rv->v.s.len=0;
				break;
			default:
				BUG("RV_CNT_ALLOCED_F not supported for type %d\n", rv->type);
		}
		rv->flags&=~RV_CNT_ALLOCED_F;
	}
}



/** frees a rval returned by rval_new(), rval_convert() or rval_expr_eval().
 *   Note: ir will be freed only when refcnt reaches 0
 */
void rval_destroy(struct rvalue* rv)
{
	if (rv && rv_unref(rv)){
		rval_force_clean(rv);
		if (rv->flags & RV_RV_ALLOCED_F)
			pkg_free(rv);
	}
}



void rval_clean(struct rvalue* rv)
{
	if (rv_unref(rv))
		rval_force_clean(rv);
}



void rve_destroy(struct rval_expr* rve)
{
	if (rve){
		if (rve->op==RVE_RVAL_OP){
			if (rve->left.rval.refcnt){
				if (rve->left.rval.refcnt==1)
					rval_destroy(&rve->left.rval);
				else
					BUG("rval expr rval with invalid refcnt: %d\n", 
							rve->left.rval.refcnt);
			}
			if (rve->right.rval.refcnt){
				if (rve->right.rval.refcnt==1)
					rval_destroy(&rve->right.rval);
				else
					BUG("rval expr rval with invalid refcnt: %d\n", 
							rve->right.rval.refcnt);
			}
		}else{
			if (rve->left.rve)
				rve_destroy(rve->left.rve);
			if (rve->right.rve)
				rve_destroy(rve->right.rve);
		}
		pkg_free(rve);
	}
}



void rval_cache_clean(struct rval_cache* rvc)
{
	if (rvc->cache_type==RV_CACHE_PVAR){
		pv_value_destroy(&rvc->c.pval);
	}
	rvc->cache_type=RV_CACHE_EMPTY;
	rvc->val_type=RV_NONE;
}


#define rv_chg_in_place(rv)  ((rv)->refcnt==1) 



/** init a rvalue structure.
 * Note: not needed if the structure is allocate with one of the 
 * rval_new* functions
 */
void rval_init(struct rvalue* rv, enum rval_type t, union rval_val* v, 
				int flags)
{
	rv->flags=flags;
	rv->refcnt=1;
	rv->type=t;
	if (v){
		rv->v=*v;
	}else{
		memset (&rv->v, 0, sizeof(rv->v));
	}
}



/** create a new pk_malloc'ed empty rvalue.
  *
  * @param extra_size - extra space to allocate
  *                    (e.g.: so that future string operation can reuse
  *                     the space)
  * @return new rv or 0 on error
  */
struct rvalue* rval_new_empty(int extra_size)
{
	struct rvalue* rv;
	int size; /* extra size at the end */
	
	size=ROUND_LONG(sizeof(*rv)-sizeof(rv->buf)+extra_size); /* round up */
	rv=pkg_malloc(size);
	if (likely(rv)){
		rv->bsize=size-sizeof(*rv)-sizeof(rv->buf); /* remaining size->buffer*/
		rv->flags=RV_RV_ALLOCED_F;
		rv->refcnt=1;
		rv->type=RV_NONE;
	}
	return rv;
}



/** create a new pk_malloc'ed rv from a str.
  *
  * @param s - pointer to str, must be non-null
  * @param extra_size - extra space to allocate
  *                    (so that future string operation can reuse
  *                     the space)
  * @return new rv or 0 on error
  */
struct rvalue* rval_new_str(str* s, int extra_size)
{
	struct rvalue* rv;
	
	rv=rval_new_empty(extra_size+s->len+1/* 0 term */);
	if (likely(rv)){
		rv->type=RV_STR;
		rv->v.s.s=&rv->buf[0];
		rv->v.s.len=s->len;
		memcpy(rv->v.s.s, s->s, s->len);
		rv->v.s.s[s->len]=0;
	}
	return rv;
}



/** create a new pk_malloc'ed rvalue from a rval_val union.
  *
  * @param s - pointer to str, must be non-null
  * @param extra_size - extra space to allocate
  *                    (so that future string operation can reuse
  *                     the space)
  * @return new rv or 0 on error
  */
struct rvalue* rval_new(enum rval_type t, union rval_val* v, int extra_size)
{
	struct rvalue* rv;
	
	if (t==RV_STR && v && v->s.len)
		return rval_new_str(&v->s, extra_size);
	rv=rval_new_empty(extra_size);
	if (likely(rv)){
		rv->type=t;
		if (likely(v)){
			rv->v=*v;
		}else
			memset (&rv->v, 0, sizeof(rv->v));
	}
	return rv;
}



/** get rvalue basic type (RV_INT or RV_STR).
  *
  * Given a rvalue it tries to determinte its basic type.
  * Fills val_cache if non-null and empty (can be used in other rval*
  * function calls, to avoid re-resolving avps or pvars). It must be
  * rval_cache_clean()'en when no longer needed.
  *
  * @param rv - target rvalue
  * @param val_cache - value cache, might be filled if non-null, 
  *                    it _must_ be rval_cache_clean()'en when done.
  * @return - basic type or RV_NONE on error
  */
inline static enum rval_type rval_get_btype(struct run_act_ctx* h,
											struct sip_msg* msg,
											struct rvalue* rv,
											struct rval_cache* val_cache)
{
	avp_t* r_avp;
	int_str tmp_avp_val;
	int_str* avpv;
	pv_value_t tmp_pval;
	pv_value_t* pv;
	enum rval_type tmp;
	enum rval_type* ptype;

	switch(rv->type){
		case RV_INT:
		case RV_STR:
			return rv->type;
		case RV_BEXPR:
		case RV_ACTION_ST:
			return RV_INT;
		case RV_PVAR:
			if (likely(val_cache && val_cache->cache_type==RV_CACHE_EMPTY)){
				pv=&val_cache->c.pval;
			}else{
				val_cache=0;
				pv=&tmp_pval;
			}
			memset(pv, 0, sizeof(tmp_pval));
			if (likely(pv_get_spec_value(msg, &rv->v.pvs, pv)==0)){
				if (pv->flags & PV_VAL_STR){
					if (unlikely(val_cache==0)) pv_value_destroy(pv);
					else{
						val_cache->cache_type=RV_CACHE_PVAR;
						val_cache->val_type=RV_STR;
					}
					return RV_STR;
				}else if (pv->flags & PV_TYPE_INT){
					if (unlikely(val_cache==0)) pv_value_destroy(pv);
					else{
						val_cache->cache_type=RV_CACHE_PVAR;
						val_cache->val_type=RV_INT;
					}
					return RV_INT;
				}else{
					pv_value_destroy(pv);
					goto error;
				}
			}else{
				goto error;
			}
			break;
		case RV_AVP:
			if (likely(val_cache && val_cache==RV_CACHE_EMPTY)){
				ptype=&val_cache->val_type;
				avpv=&val_cache->c.avp_val;
				val_cache->cache_type=RV_CACHE_AVP;
			}else{
				ptype=&tmp;
				avpv=&tmp_avp_val;
			}
			r_avp = search_avp_by_index(rv->v.avps.type, rv->v.avps.name,
											avpv, rv->v.avps.index);
			if (likely(r_avp)){
				if (r_avp->flags & AVP_VAL_STR){
					*ptype=RV_STR;
					return RV_STR;
				}else{
					*ptype=RV_INT;
					return RV_INT;
				}
			}else{
				*ptype=RV_NONE;
				if (val_cache) val_cache->cache_type=RV_CACHE_EMPTY;
				goto error;
			}
			break;
		case RV_SEL:
			return RV_STR;
		default:
			BUG("rv type %d not handled\n", rv->type);
	}
error:
	return RV_NONE;
}



/** guess the type of an expression.
  * @return RV_INT, RV_STR or RV_NONE (when type could not be found,
  * e.g. avp or pvar)
  */
enum rval_type rve_guess_type( struct rval_expr* rve)
{
	switch(rve->op){
		case RVE_RVAL_OP:
			switch(rve->left.rval.type){
				case RV_STR:
				case RV_SEL:
					return RV_STR;
				case RV_INT:
				case RV_BEXPR:
				case RV_ACTION_ST:
					return RV_INT;
				case RV_PVAR:
				case RV_AVP:
				case RV_NONE:
					return RV_NONE;
			}
			break;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			return RV_INT;
		case RVE_PLUS_OP:
			/* '+' evaluates to the type of the left operand */
			return rve_guess_type(rve->left.rve);
		case RVE_NONE_OP:
			break;
	}
	return RV_NONE;
}



/** returns true if expression is constant.
  * @return 0 or 1 on
  *  non constant type
  */
int rve_is_constant(struct rval_expr* rve)
{
	switch(rve->op){
		case RVE_RVAL_OP:
			switch(rve->left.rval.type){
				case RV_STR:
					return 1;
				case RV_INT:
					return 1;
				case RV_SEL:
				case RV_BEXPR:
				case RV_ACTION_ST:
				case RV_PVAR:
				case RV_AVP:
				case RV_NONE:
					return 0;
			}
			break;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
			return rve_is_constant(rve->left.rve);
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_PLUS_OP:
			return rve_is_constant(rve->left.rve) &&
					rve_is_constant(rve->right.rve);
		case RVE_NONE_OP:
			break;
	}
	return 0;
}



/** returns 1 if expression is valid (type-wise).
  * @return 0 or 1  and sets *type to the resulting type
  * (RV_INT, RV_STR or RV_NONE if it can be found only at runtime)
  */
int rve_check_type(enum rval_type* type, struct rval_expr* rve)
{
	enum rval_type type1, type2;
	
	switch(rve->op){
		case RVE_RVAL_OP:
			*type=rve_guess_type(rve);
			return 1;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
			*type=RV_INT;
			if (rve_check_type(&type1, rve->left.rve)){
				if (type1==RV_STR)
					return 0;
				return 1;
			}
			return 0;
			break;
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
			*type=RV_INT;
			if (rve_check_type(&type1, rve->left.rve)){
				if (type1==RV_STR)
					return 0;
				if (rve_check_type(&type2, rve->right.rve)){
					if (type2==RV_STR)
						return 0;
					return 1;
				}
			}
			return 0;
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			*type=RV_INT;
			if (rve_check_type(&type1, rve->left.rve)){
				if (rve_check_type(&type2, rve->right.rve)){
					if ((type2!=type1) && (type1!=RV_NONE) &&
							(type2!=RV_NONE) && 
							!(type1==RV_STR && type2==RV_INT))
						return 0;
					return 1;
				}
			}
			return 0;
		case RVE_PLUS_OP:
			if (rve_check_type(&type1, rve->left.rve)){
				if (rve_check_type(&type2, rve->right.rve)){
					if ((type2!=type1) && (type1!=RV_NONE) &&
							(type2!=RV_NONE) && 
							!(type1==RV_STR && type2==RV_INT))
						return 0;
					*type=type1;
					return 1;
				}
			}
		case RVE_NONE_OP:
			break;
	}
	return 0;
}



/** get the integer value of an rvalue.
  * *i=(int)rv
  * @return 0 on success, \<0 on error and EXPR_DROP on drop
 */
int rval_get_int(struct run_act_ctx* h, struct sip_msg* msg,
								int* i, struct rvalue* rv,
								struct rval_cache* cache)
{
	avp_t* r_avp;
	int_str avp_val;
	pv_value_t pval;
	
	switch(rv->type){
		case RV_INT:
			*i=rv->v.l;
			break;
		case RV_STR:
			goto rv_str;
		case RV_BEXPR:
			*i=eval_expr(h, rv->v.bexpr, msg);
			if (*i==EXPR_DROP){
				*i=0; /* false */
				return EXPR_DROP;
			}
			break;
		case RV_ACTION_ST:
			if (rv->v.action)
				*i=run_actions(h, rv->v.action, msg);
			else 
				*i=0;
			break;
		case RV_SEL:
			goto rv_str;
		case RV_AVP:
			if (unlikely(cache && cache->cache_type==RV_CACHE_AVP)){
				if (likely(cache->val_type==RV_INT)){
					*i=cache->c.avp_val.n;
				}else if (cache->val_type==RV_STR)
					goto rv_str;
				else goto error;
			}else{
				r_avp = search_avp_by_index(rv->v.avps.type, rv->v.avps.name,
											&avp_val, rv->v.avps.index);
				if (likely(r_avp)){
					if (unlikely(r_avp->flags & AVP_VAL_STR)){
						goto rv_str;
					}else{
						*i=avp_val.n;
					}
				}else{
					goto error;
				}
			}
			break;
		case RV_PVAR:
			if (unlikely(cache && cache->cache_type==RV_CACHE_PVAR)){
				if (likely((cache->val_type==RV_INT) || 
								(cache->c.pval.flags & PV_VAL_INT))){
					*i=cache->c.pval.ri;
				}else if (cache->val_type==RV_STR)
					goto rv_str;
				else goto error;
			}else{
				memset(&pval, 0, sizeof(pval));
				if (likely(pv_get_spec_value(msg, &rv->v.pvs, &pval)==0)){
					if (likely(pval.flags & PV_VAL_INT)){
						pv_value_destroy(&pval);
						*i=pval.ri;
					}else if (likely(pval.flags & PV_VAL_STR)){
						pv_value_destroy(&pval);
						goto rv_str;
					}else{
						pv_value_destroy(&pval);
						goto error;
					}
				}else{
					goto error;
				}
			}
			break;
		default:
			BUG("rv type %d not handled\n", rv->type);
			goto error;
	}
	return 0;
rv_str:
	/* rv is of string type => error */
	/* ERR("string in int expression\n"); */
error:
	return -1;
}



/** get the string value of an rv in a tmp variable
  * *s=(str)rv
  * The result points either to a temporary string or inside
  * new_cache. new_cache must be non zero, initialized previously,
  * and it _must_ be rval_cache_clean(...)'ed when done.
  * WARNING: it's not intended for general use, it might return a pointer
  * to a static buffer (int2str) so use the result a.s.a.p, make a copy.
  * or use rval_get_str() instead.
  * @param h - script context handle
  * @param msg - sip msg
  * @param tmpv - str return value (pointer to a str struct that will be
  *               be filled.
  * @param rv   - rvalue to be converted
  * @param cache - cached rv value (read-only)
  * @param tmp_cache - used for temporary storage (so that tmpv will not
  *                 point to possible freed data), it must be non-null,
  *                 initialized and cleaned afterwards.
  * @return 0 on success, <0 on error and EXPR_DROP on drop
 */
int rval_get_tmp_str(struct run_act_ctx* h, struct sip_msg* msg,
								str* tmpv, struct rvalue* rv,
								struct rval_cache* cache,
								struct rval_cache* tmp_cache)
{
	avp_t* r_avp;
	pv_value_t pval;
	int i;
	
	switch(rv->type){
		case RV_INT:
			tmpv->s=int2str(rv->v.l, &tmpv->len);
			break;
		case RV_STR:
			*tmpv=rv->v.s;
			break;
		case RV_ACTION_ST:
			if (rv->v.action)
				i=run_actions(h, rv->v.action, msg);
			else 
				i=0;
			tmpv->s=int2str(i, &tmpv->len);
			break;
		case RV_BEXPR:
			i=eval_expr(h, rv->v.bexpr, msg);
			if (i==EXPR_DROP){
				i=0; /* false */
				tmpv->s=int2str(i, &tmpv->len);
				return EXPR_DROP;
			}
			tmpv->s=int2str(i, &tmpv->len);
			break;
		case RV_SEL:
			i=run_select(tmpv, &rv->v.sel, msg);
			if (unlikely(i!=0)){
				if (i<0){
					goto error;
				}else { /* i>0 */
					tmpv->s="";
					tmpv->len=0;
				}
			}
			break;
		case RV_AVP:
			if (likely(cache && cache->cache_type==RV_CACHE_AVP)){
				if (likely(cache->val_type==RV_STR)){
					*tmpv=cache->c.avp_val.s;
				}else if (cache->val_type==RV_INT){
					i=cache->c.avp_val.n;
					tmpv->s=int2str(i, &tmpv->len);
				}else goto error;
			}else{
				r_avp = search_avp_by_index(rv->v.avps.type, rv->v.avps.name,
											&tmp_cache->c.avp_val,
											rv->v.avps.index);
				if (likely(r_avp)){
					if (likely(r_avp->flags & AVP_VAL_STR)){
						tmp_cache->cache_type=RV_CACHE_AVP;
						tmp_cache->val_type=RV_STR;
						*tmpv=tmp_cache->c.avp_val.s;
					}else{
						i=tmp_cache->c.avp_val.n;
						tmpv->s=int2str(i, &tmpv->len);
					}
				}else{
					goto error;
				}
			}
			break;
		case RV_PVAR:
			if (likely(cache && cache->cache_type==RV_CACHE_PVAR)){
				if (likely(cache->val_type==RV_STR)){
					*tmpv=cache->c.pval.rs;
				}else if (cache->val_type==RV_INT){
					i=cache->c.pval.ri;
					tmpv->s=int2str(i, &tmpv->len);
				}else goto error;
			}else{
				memset(&pval, 0, sizeof(pval));
				if (likely(pv_get_spec_value(msg, &rv->v.pvs,
												&tmp_cache->c.pval)==0)){
					if (likely(pval.flags & PV_VAL_STR)){
						/*  the value is not destroyed, but saved instead
							in tmp_cache so that it can be destroyed later
							when no longer needed */
						tmp_cache->cache_type=RV_CACHE_PVAR;
						tmp_cache->val_type=RV_STR;
						*tmpv=tmp_cache->c.pval.rs;
					}else if (likely(pval.flags & PV_VAL_INT)){
						i=pval.ri;
						pv_value_destroy(&tmp_cache->c.pval);
						tmpv->s=int2str(i, &tmpv->len);
					}else{
						pv_value_destroy(&tmp_cache->c.pval);
						goto error;
					}
				}else{
					goto error;
				}
			}
			break;
		default:
			BUG("rv type %d not handled\n", rv->type);
			goto error;
	}
	return 0;
error:
	return -1;
}



/** get the string value of an rv.
  * *s=(str)rv
  * The result is pkg malloc'ed (so it should be pkg_free()'ed when finished.
  * @return 0 on success, <0 on error and EXPR_DROP on drop
 */
int rval_get_str(struct run_act_ctx* h, struct sip_msg* msg,
								str* s, struct rvalue* rv,
								struct rval_cache* cache)
{
	str tmp;
	struct rval_cache tmp_cache;
	
	rval_cache_init(&tmp_cache);
	if (unlikely(rval_get_tmp_str(h, msg, &tmp, rv, cache, &tmp_cache)<0))
		goto error;
	s->s=pkg_malloc(tmp.len+1/* 0 term */);
	if (unlikely(s->s==0)){
		ERR("memory allocation error\n");
		goto error;
	}
	s->len=tmp.len;
	memcpy(s->s, tmp.s, tmp.len);
	s->s[tmp.len]=0; /* 0 term */
	rval_cache_clean(&tmp_cache);
	return 0;
error:
	rval_cache_clean(&tmp_cache);
	return -1;
}



/** convert a rvalue to another rvalue, of a specific type.
 *
 * The result is read-only in most cases (can be a reference
 * to another rvalue, can be checked by using rv_chg_in_place()) and
 * _must_ be rval_destroy()'ed.
 *
 * @param type - type to convert to
 * @param v - rvalue to convert
 * @param c - rval_cache (cached v value if known/filled by another
 *            function), can be 0 (unknown/not needed)
 * @return pointer to a rvalue (reference to an existing one or a new
 *   one, @see rv_chg_in_place() and the above comment) or 0 on error.
 */
struct rvalue* rval_convert(struct run_act_ctx* h, struct sip_msg* msg,
							enum rval_type type, struct rvalue* v,
							struct rval_cache* c)
{
	int i;
	struct rval_cache tmp_cache;
	str tmp;
	struct rvalue* ret;
	union rval_val val;
	
	if (v->type==type){
		rv_ref(v);
		return v;
	}
	switch(type){
		case RV_INT:
			if (unlikely(rval_get_int(h, msg, &i, v, c) < 0))
				return 0;
			val.l=i;
			return rval_new(RV_INT, &val, 0);
		case RV_STR:
			rval_cache_init(&tmp_cache);
			if (unlikely(rval_get_tmp_str(h, msg, &tmp, v, c, &tmp_cache) < 0))
			{
				rval_cache_clean(&tmp_cache);
				return 0;
			}
			ret=rval_new_str(&tmp, RV_STR_EXTRA);
			rval_cache_clean(&tmp_cache);
			return ret;
		case RV_NONE:
		default:
			BUG("unsupported conversion to type %d\n", type);
			return 0;
	}
	return 0;
}



/** integer operation: *res= op v.
  * @return 0 on succes, \<0 on error
  */
inline static int int_intop1(int* res, enum rval_expr_op op, int v)
{
	switch(op){
		case RVE_UMINUS_OP:
			*res=-v;
			break;
		case RVE_BOOL_OP:
			*res=!!v;
			break;
		case RVE_LNOT_OP:
			*res=!v;
			break;
		default:
			BUG("rv unsupported intop1 %d\n", op);
			return -1;
	}
	return 0;
}



/** integer operation: *res= op v.
  * @return 0 on succes, \<0 on error
  */
inline static int int_intop2(int* res, enum rval_expr_op op, int v1, int v2)
{
	switch(op){
		case RVE_PLUS_OP:
			*res=v1+v2;
			break;
		case RVE_MINUS_OP:
			*res=v1-v2;
			break;
		case RVE_MUL_OP:
			*res=v1*v2;
			break;
		case RVE_DIV_OP:
			if (unlikely(v2==0)){
				ERR("rv div by 0\n");
				return -1;
			}
			*res=v1/v2;
			break;
		case RVE_BOR_OP:
			*res=v1|v2;
			break;
		case RVE_BAND_OP:
			*res=v1&v2;
			break;
		case RVE_LAND_OP:
			*res=v1 && v2;
			break;
		case RVE_LOR_OP:
			*res=v1 || v2;
			break;
		case RVE_GT_OP:
			*res=v1 > v2;
			break;
		case RVE_GTE_OP:
			*res=v1 >= v2;
			break;
		case RVE_LT_OP:
			*res=v1 < v2;
			break;
		case RVE_LTE_OP:
			*res=v1 <= v2;
			break;
		case RVE_EQ_OP:
			*res=v1 == v2;
			break;
		case RVE_DIFF_OP:
			*res=v1 != v2;
			break;
		default:
			BUG("rv unsupported intop %d\n", op);
			return -1;
	}
	return 0;
}



inline static int bool_strop2( enum rval_expr_op op, int* res,
								str* s1, str* s2)
{
	if (s1->len!=s2->len)
		*res= op==RVE_DIFF_OP;
	else if (memcmp(s1->s, s2->s, s1->len)==0)
		*res= op==RVE_EQ_OP;
	else
		*res= op==RVE_DIFF_OP;
	return 0;
}



/** integer operation: ret= op v (returns a rvalue).
 * @return rvalue on success, 0 on error
 */
inline static struct rvalue* rval_intop1(struct run_act_ctx* h,
											struct sip_msg* msg,
											enum rval_expr_op op,
											struct rvalue* v)
{
	struct rvalue* rv2;
	struct rvalue* ret;
	int i;
	
	i=0;
	rv2=rval_convert(h, msg, RV_INT, v, 0);
	if (unlikely(rv2==0)){
		ERR("rval int conversion failed\n");
		goto error;
	}
	if (unlikely(int_intop1(&i, op, rv2->v.l)<0))
		goto error;
	if (rv_chg_in_place(rv2)){
		ret=rv2;
		rv_ref(ret);
	}else if (rv_chg_in_place(v)){
		ret=v;
		rv_ref(ret);
	}else{
		ret=rval_new(RV_INT, &rv2->v, 0);
		if (unlikely(ret==0)){
			ERR("eval out of memory\n");
			goto error;
		}
	}
	rval_destroy(rv2);
	ret->v.l=i;
	return ret;
error:
	rval_destroy(rv2);
	return 0;
}



/** integer operation: ret= l op r (returns a rvalue).
 * @return rvalue on success, 0 on error
 */
inline static struct rvalue* rval_intop2(struct run_act_ctx* h,
											struct sip_msg* msg,
											enum rval_expr_op op,
											struct rvalue* l,
											struct rvalue* r)
{
	struct rvalue* rv1;
	struct rvalue* rv2;
	struct rvalue* ret;
	int i;

	rv2=rv1=0;
	ret=0;
	if ((rv1=rval_convert(h, msg, RV_INT, l, 0))==0)
		goto error;
	if ((rv2=rval_convert(h, msg, RV_INT, r, 0))==0)
		goto error;
	if (unlikely(int_intop2(&i, op, rv1->v.l, rv2->v.l)<0))
		goto error;
	if (rv_chg_in_place(rv1)){
		/* try reusing rv1 */
		ret=rv1;
		rv_ref(ret);
	}else if (rv_chg_in_place(rv2)){
		/* try reusing rv2 */
		ret=rv2;
		rv_ref(ret);
	}else if ((l->type==RV_INT) && (rv_chg_in_place(l))){
		ret=l;
		rv_ref(ret);
	} else if ((r->type==RV_INT) && (rv_chg_in_place(r))){
		ret=r;
		rv_ref(ret);
	}else{
		ret=rval_new(RV_INT, &rv1->v, 0);
		if (unlikely(ret==0)){
			ERR("rv eval out of memory\n");
			goto error;
		}
	}
	rval_destroy(rv1); 
	rval_destroy(rv2); 
	ret->v.l=i;
	return ret;
error:
	rval_destroy(rv1); 
	rval_destroy(rv2); 
	return 0;
}



/** string add operation: ret= l . r (returns a rvalue).
 * Can use cached rvalues (c1 & c2).
 * @return rvalue on success, 0 on error
 */
inline static struct rvalue* rval_str_add2(struct run_act_ctx* h,
											struct sip_msg* msg,
											struct rvalue* l,
											struct rval_cache* c1,
											struct rvalue* r,
											struct rval_cache* c2
											)
{
	struct rvalue* rv1;
	struct rvalue* rv2;
	struct rvalue* ret;
	str* s1;
	str* s2;
	str tmp;
	short flags;
	int len;
	
	rv2=rv1=0;
	ret=0;
	flags=0;
	s1=0;
	s2=0;
	if ((rv1=rval_convert(h, msg, RV_STR, l, c1))==0)
		goto error;
	if ((rv2=rval_convert(h, msg, RV_STR, r, c2))==0)
		goto error;
	
	len=rv1->v.s.len + rv2->v.s.len + 1 /* 0 */;
	
	if (rv_chg_in_place(rv1) && (rv1->bsize>=len)){
		/* try reusing rv1 */
		ret=rv1;
		rv_ref(ret);
		s2=&rv2->v.s;
		if (ret->v.s.s == &ret->buf[0]) s1=0;
		else{
			tmp=ret->v.s;
			flags=ret->flags;
			ret->flags &= ~RV_CNT_ALLOCED_F;
			ret->v.s.s=&ret->buf[0];
			ret->v.s.len=0;
			s1=&tmp;
		}
	}else if (rv_chg_in_place(rv2) && (rv2->bsize>=len)){
		/* try reusing rv2 */
		ret=rv2;
		rv_ref(ret);
		s1=&rv1->v.s;
		if (ret->v.s.s == &ret->buf[0]) 
			s2=&ret->v.s;
		else{
			tmp=ret->v.s;
			flags=ret->flags;
			ret->flags &= ~RV_CNT_ALLOCED_F;
			ret->v.s.s=&ret->buf[0];
			ret->v.s.len=0;
			s2=&tmp;
		}
	}else if ((l->type==RV_STR) && (rv_chg_in_place(l)) && (l->bsize>=len)){
		ret=l;
		rv_ref(ret);
		s2=&rv2->v.s;
		if (ret->v.s.s == &ret->buf[0]) s1=0;
		else{
			tmp=ret->v.s;
			flags=ret->flags;
			ret->flags &= ~RV_CNT_ALLOCED_F;
			ret->v.s.s=&ret->buf[0];
			ret->v.s.len=0;
			s1=&tmp;
		}
	} else if ((r->type==RV_STR) && (rv_chg_in_place(r) && (r->bsize>=len))){
		ret=r;
		rv_ref(ret);
		s1=&rv1->v.s;
		if (ret->v.s.s == &ret->buf[0]) 
			s2=&ret->v.s;
		else{
			tmp=ret->v.s;
			flags=ret->flags;
			ret->flags &= ~RV_CNT_ALLOCED_F;
			ret->v.s.s=&ret->buf[0];
			ret->v.s.len=0;
			s2=&tmp;
		}
	}else{
		ret=rval_new(RV_STR, &rv1->v, len + RV_STR_EXTRA);
		if (unlikely(ret==0)){
			ERR("rv eval out of memory\n");
			goto error;
		}
		s1=0;
		s2=&rv2->v.s;
	}
	/* do the actual copy */
	ret->v.s.len=rv1->v.s.len;
	memmove(ret->buf+ret->v.s.len, s2->s, s2->len);
	if (s1){
		memcpy(ret->buf, s1->s, s1->len);
	}
	ret->v.s.len+=s2->len;
	ret->v.s.s[ret->v.s.len]=0;
	/* cleanup if needed */
	if (flags & RV_CNT_ALLOCED_F)
		pkg_free(tmp.s);
	rval_destroy(rv1); 
	rval_destroy(rv2); 
	return ret;
error:
	rval_destroy(rv1); 
	rval_destroy(rv2); 
	return 0;
}



/** bool operation on rval evaluated as strings.
 * Can use cached rvalues (c1 & c2).
 * @return 0 success, -1 on error
 */
inline static int rval_str_lop2(struct run_act_ctx* h,
						 struct sip_msg* msg,
						 int* res,
						 enum rval_expr_op op,
						 struct rvalue* l,
						 struct rval_cache* c1,
						 struct rvalue* r,
						 struct rval_cache* c2)
{
	struct rvalue* rv1;
	struct rvalue* rv2;
	int ret;
	
	rv2=rv1=0;
	ret=0;
	if ((rv1=rval_convert(h, msg, RV_STR, l, c1))==0)
		goto error;
	if ((rv2=rval_convert(h, msg, RV_STR, r, c2))==0)
		goto error;
	ret=bool_strop2(op, res, &rv1->v.s, &rv2->v.s);
	rval_destroy(rv1); 
	rval_destroy(rv2); 
	return ret;
error:
	rval_destroy(rv1); 
	rval_destroy(rv2); 
	return 0;
}



/* forward decl. */
inline static int rval_expr_eval_rvint(struct run_act_ctx* h,
									   struct sip_msg* msg,
									   struct rvalue** res_rv,
									   int* res_i,
									   struct rval_expr* rve,
									   struct rval_cache* cache
									   );



/** evals an integer expr  to an int.
 * 
 *  *res=(int)eval(rve)
 *  @return 0 on success, \<0 on error
 */
int rval_expr_eval_int( struct run_act_ctx* h, struct sip_msg* msg,
						int* res, struct rval_expr* rve)
{
	int i1, i2, ret;
	struct rval_cache c1;
	struct rvalue* rv1;
	struct rvalue* rv2;
	
	ret=-1;
	switch(rve->op){
		case RVE_RVAL_OP:
			ret=rval_get_int(h, msg, res,  &rve->left.rval, 0);
			break;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
			if (unlikely(
					(ret=rval_expr_eval_int(h, msg, &i1, rve->left.rve)) <0) )
				break;
			ret=int_intop1(res, rve->op, i1);
			break;
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MINUS_OP:
		case RVE_PLUS_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
			if (unlikely(
					(ret=rval_expr_eval_int(h, msg, &i1, rve->left.rve)) <0) )
				break;
			if (unlikely(
					(ret=rval_expr_eval_int(h, msg, &i2, rve->right.rve)) <0) )
				break;
			ret=int_intop2(res, rve->op, i1, i2);
			break;
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			/* if left is string, eval left & right as string and
			   use string diff, else eval as int */
			rval_cache_init(&c1);
			if (unlikely( (ret=rval_expr_eval_rvint(h, msg, &rv1, &i1,
													rve->left.rve, &c1))<0)){
				rval_cache_clean(&c1);
				break;
			}
			if (likely(rv1==0)){
				/* int */
				rval_cache_clean(&c1);
				if (unlikely( (ret=rval_expr_eval_int(h, msg, &i2,
														rve->right.rve)) <0) )
					break;
				ret=int_intop2(res, rve->op, i1, i2);
			}else{
				if (unlikely((rv2=rval_expr_eval(h, msg,
													rve->right.rve))==0)){
					rval_destroy(rv1);
					rval_cache_clean(&c1);
					ret=-1;
					break;
				}
				ret=rval_str_lop2(h, msg, res, rve->op, rv1, &c1, rv2, 0);
				rval_cache_clean(&c1);
				rval_destroy(rv1);
				rval_destroy(rv2);
			}
			break;
#if 0
		case RVE_MATCH_OP:
				if (unlikely((rv1=rval_expr_eval(h, msg, rve->left.rve))==0)){
					ret=-1;
					break;
				}
				if (unlikely((rv2=rval_expr_eval(h, msg,
													rve->right.rve))==0)){
					rval_destroy(rv1);
					ret=-1;
					break;
				}
				ret=rval_str_lop2(res, rve->op, rv1, 0, rv2, 0);
				rval_destroy(rv1);
				rval_destroy(rv2);
			break;
#endif
		case RVE_NONE_OP:
		/*default:*/
			BUG("invalid rval int expression operation %d\n", rve->op);
			ret=-1;
	};
	return ret;
}



/** evals a rval expr. into an int or another rv(str).
 * WARNING: rv result (rv_res) must be rval_destroy()'ed if non-null
 * (it might be a reference to another rval). The result can be 
 * modified only if rv_chg_in_place() returns true.
 * @result  0 on success, -1 on error,  sets *res_rv or *res_i.
 */
inline static int rval_expr_eval_rvint(struct run_act_ctx* h,
									   struct sip_msg* msg,
									   struct rvalue** res_rv,
									   int* res_i,
									   struct rval_expr* rve,
									   struct rval_cache* cache
									   )
{
	struct rvalue* rv1;
	struct rvalue* rv2;
	struct rval_cache c1; /* local cache */
	int ret;
	int r, i, j;
	enum rval_type type;
	
	rv1=0;
	rv2=0;
	switch(rve->op){
		case RVE_RVAL_OP:
			rv1=&rve->left.rval;
			rv_ref(rv1);
			type=rval_get_btype(h, msg, rv1, cache);
			if (type==RV_INT){
					r=rval_get_int(h, msg, res_i, rv1, cache);
					if (unlikely(r<0)){
						ERR("rval expression evaluation failed\n");
						goto error;
					}
					*res_rv=0;
					ret=0;
			}else{
				/* RV_STR, RV_PVAR, RV_AVP a.s.o => return rv1 and the 
				   cached resolved value in cache*/
					*res_rv=rv1;
					rv_ref(rv1);
					ret=0;
			}
			break;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			/* operator forces integer type */
			ret=rval_expr_eval_int(h, msg, res_i, rve);
			*res_rv=0;
			break;
		case RVE_PLUS_OP:
			rval_cache_init(&c1);
			r=rval_expr_eval_rvint(h, msg, &rv1, &i, rve->left.rve, &c1);
			if (unlikely(r<0)){
				ERR("rval expression evaluation failed\n");
				rval_cache_clean(&c1);
				goto error;
			}
			if (rv1==0){
				if (unlikely((r=rval_expr_eval_int(h, msg, &j,
														rve->right.rve))<0)){
						ERR("rval expression evaluation failed\n");
						rval_cache_clean(&c1);
						goto error;
				}
				int_intop2(res_i, rve->op, i, j);
				*res_rv=0;
			}else{
				rv2=rval_expr_eval(h, msg, rve->right.rve);
				if (unlikely(rv2==0)){
					ERR("rval expression evaluation failed\n");
					rval_cache_clean(&c1);
					goto error;
				}
				*res_rv=rval_str_add2(h, msg, rv1, &c1, rv2, 0);
				ret=-(*res_rv==0);
			}
			rval_cache_clean(&c1);
			break;
		case RVE_NONE_OP:
		/*default:*/
			BUG("invalid rval expression operation %d\n", rve->op);
			goto error;
	};
	rval_destroy(rv1);
	rval_destroy(rv2);
	return ret;
error:
	rval_destroy(rv1);
	rval_destroy(rv2);
	return -1;
}



/** evals a rval expr..
 * WARNING: result must be rval_destroy()'ed if non-null (it might be
 * a reference to another rval). The result can be modified only
 * if rv_chg_in_place() returns true.
 * @result rvalue on success, 0 on error
 */
struct rvalue* rval_expr_eval(struct run_act_ctx* h, struct sip_msg* msg,
								struct rval_expr* rve)
{
	struct rvalue* rv1;
	struct rvalue* rv2;
	struct rvalue* ret;
	struct rval_cache c1;
	union rval_val v;
	int r, i, j;
	enum rval_type type;
	
	rv1=0;
	rv2=0;
	switch(rve->op){
		case RVE_RVAL_OP:
			rv_ref(&rve->left.rval);
			return &rve->left.rval;
			break;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			/* operator forces integer type */
			r=rval_expr_eval_int(h, msg, &i, rve);
			if (likely(r==0)){
				v.l=i;
				ret=rval_new(RV_INT, &v, 0);
				if (unlikely(ret==0)){
					ERR("rv eval int expression: out of memory\n");
					goto error;
				}
				return ret;
			}else{
				ERR("rval expression evaluation failed\n");
				goto error;
			}
			break;
		case RVE_PLUS_OP:
			rv1=rval_expr_eval(h, msg, rve->left.rve);
			if (unlikely(rv1==0)){
				ERR("rval expression evaluation failed\n");
				goto error;
			}
			rval_cache_init(&c1);
			type=rval_get_btype(h, msg, rv1, &c1);
			switch(type){
				case RV_INT:
					if (unlikely((r=rval_get_int(h, msg, &i, rv1, &c1))<0)){
						rval_cache_clean(&c1);
						ERR("rval expression evaluation failed\n");
						goto error;
					}
					if (unlikely((r=rval_expr_eval_int(h, msg, &j,
														rve->right.rve))<0)){
						rval_cache_clean(&c1);
						ERR("rval expression evaluation failed\n");
						goto error;
					}
					int_intop2(&r, rve->op, i, j);
					if (rv_chg_in_place(rv1)){
						rv1->v.l=r;
						ret=rv1;
						rv_ref(ret);
					}else{
						v.l=r;
						ret=rval_new(RV_INT, &v, 0);
						if (unlikely(ret==0)){
							rval_cache_clean(&c1);
							ERR("rv eval int expression: out of memory\n");
							goto error;
						}
					}
					break;
				case RV_STR:
					rv2=rval_expr_eval(h, msg, rve->right.rve);
					if (unlikely(rv2==0)){
						ERR("rval expression evaluation failed\n");
						rval_cache_clean(&c1);
						goto error;
					}
					ret=rval_str_add2(h, msg, rv1, &c1, rv2, 0);
					break;
				default:
					BUG("rv unsupported basic type %d\n", type);
				case RV_NONE:
					rval_cache_clean(&c1);
					goto error;
			}
			rval_cache_clean(&c1);
			break;
		case RVE_NONE_OP:
		/*default:*/
			BUG("invalid rval expression operation %d\n", rve->op);
			goto error;
	};
	rval_destroy(rv1);
	rval_destroy(rv2);
	return ret;
error:
	rval_destroy(rv1);
	rval_destroy(rv2);
	return 0;
}



/** create a RVE_RVAL_OP rval_expr, containing a single rval of the given type.
 *
 * @param rv_type - rval type
 * @param val     - rval value
 * @return new pkg_malloc'ed rval_expr or 0 on error.
 */
struct rval_expr* mk_rval_expr_v(enum rval_type rv_type, void* val)
{
	struct rval_expr* rve;
	union rval_val v;
	str* s;
	int flags;
	
	rve=pkg_malloc(sizeof(*rve));
	if (rve==0) 
		return 0;
	memset(rve, sizeof(*rve), 0);
	flags=0;
	switch(rv_type){
		case RV_INT:
			v.l=(long)val;
			break;
		case RV_STR:
			s=(str*)val;
			v.s.s=pkg_malloc(s->len+1 /*0*/);
			if (v.s.s==0){
				ERR("memory allocation failure\n");
				return 0;
			}
			v.s.len=s->len;
			memcpy(v.s.s, s->s, s->len);
			v.s.s[s->len]=0;
			flags=RV_CNT_ALLOCED_F;
			break;
		case RV_AVP:
			v.avps=*(avp_spec_t*)val;
			break;
		case RV_PVAR:
			v.pvs=*(pv_spec_t*)val;
			break;
		case RV_SEL:
			v.sel=*(select_t*)val;
			break;
		case RV_BEXPR:
			v.bexpr=(struct expr*)val;
			break;
		case RV_ACTION_ST:
			v.action=(struct action*)val;
			break;
		default:
			BUG("unsupported rv type %d\n", rv_type);
			pkg_free(rve);
			return 0;
	}
	rval_init(&rve->left.rval, rv_type, &v, flags);
	rve->op=RVE_RVAL_OP;
	return rve;
}



/** create a unary op. rval_expr..
 * ret= op rve1
 * @param op   - rval expr. unary operator
 * @param rve1 - rval expr. on which the operator will act.
 * @return new pkg_malloc'ed rval_expr or 0 on error.
 */
struct rval_expr* mk_rval_expr1(enum rval_expr_op op, struct rval_expr* rve1)
{
	struct rval_expr* ret;
	
	switch(op){
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
			break;
		default:
			BUG("unsupported unary operator %d\n", op);
			return 0;
	}
	ret=pkg_malloc(sizeof(*ret));
	if (ret==0) 
		return 0;
	memset(ret, sizeof(*ret), 0);
	ret->op=op;
	ret->left.rve=rve1;
	return ret;
}



/** create a rval_expr. from 2 other rval exprs, using op.
 * ret = rve1 op rve2
 * @param op   - rval expr. operator
 * @param rve1 - rval expr. on which the operator will act.
 * @param rve2 - rval expr. on which the operator will act.
 * @return new pkg_malloc'ed rval_expr or 0 on error.
 */
struct rval_expr* mk_rval_expr2(enum rval_expr_op op, struct rval_expr* rve1,
													  struct rval_expr* rve2)
{
	struct rval_expr* ret;
	
	switch(op){
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MINUS_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_PLUS_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			break;
		default:
			BUG("unsupported operator %d\n", op);
			return 0;
	}
	ret=pkg_malloc(sizeof(*ret));
	if (ret==0) 
		return 0;
	memset(ret, sizeof(*ret), 0);
	ret->op=op;
	ret->left.rve=rve1;
	ret->right.rve=rve2;
	return ret;
}



/** returns true if the rval expr can be optimized to an int.
 *  (if left & right are leafs (RVE_RVAL_OP) and both of them are
 *   ints return true, else false)
 *  @return 0 or 1
 */
static int rve_can_optimize_int(struct rval_expr* rve)
{
	if (rve->op == RVE_RVAL_OP)
		return 0;
	if (rve->left.rve->op != RVE_RVAL_OP)
		return 0;
	if (rve->left.rve->left.rval.type!=RV_INT)
		return 0;
	if (rve->right.rve){
		if  (rve->right.rve->op != RVE_RVAL_OP)
			return 0;
		if (rve->right.rve->left.rval.type!=RV_INT)
			return 0;
	}
	DBG("rve_can_optimize_int: left %d, right %d\n", 
			rve->left.rve->op, rve->right.rve?rve->right.rve->op:0);
	return 1;
}



/** returns true if the rval expr can be optimized to a str.
 *  (if left & right are leafs (RVE_RVAL_OP) and both of them are
 *   str or left is str and right is int return true, else false)
 *  @return 0 or 1
 */
static int rve_can_optimize_str(struct rval_expr* rve)
{
	if (rve->op == RVE_RVAL_OP)
		return 0;
	DBG("rve_can_optimize_str: left %d, right %d\n", 
			rve->left.rve->op, rve->right.rve?rve->right.rve->op:0);
	if (rve->left.rve->op != RVE_RVAL_OP)
		return 0;
	if (rve->left.rve->left.rval.type!=RV_STR)
		return 0;
	if (rve->right.rve){
		if  (rve->right.rve->op != RVE_RVAL_OP)
			return 0;
		if ((rve->right.rve->left.rval.type!=RV_STR) && 
				(rve->right.rve->left.rval.type!=RV_INT))
			return 0;
	}
	return 1;
}



static int fix_rval(struct rvalue* rv)
{
	DBG("RV fixing type %d\n", rv->type);
	switch(rv->type){
		case RV_INT:
			/*nothing to do*/
			DBG("RV is int: %d\n", (int)rv->v.l);
			return 0;
		case RV_STR:
			/*nothing to do*/
			DBG("RV is str: \"%s\"\n", rv->v.s.s);
			return 0;
		case RV_BEXPR:
			return fix_expr(rv->v.bexpr);
		case RV_ACTION_ST:
			return fix_actions(rv->v.action);
		case RV_SEL:
			if (resolve_select(&rv->v.sel)<0){
				BUG("Unable to resolve select\n");
				print_select(&rv->v.sel);
			}
			return 0;
		case RV_AVP:
			/* nothing to do, resolved at runtime */
			return 0;
		case RV_PVAR:
			/* nothing to do, resolved at parsing time */
			return 0;
		case RV_NONE:
			BUG("uninitialized rvalue\n");
			return -1;
	}
	BUG("unknown rvalue type %d\n", rv->type);
	return -1;
}



/** fix a rval_expr.
 * fixes action, bexprs, resolves selects, pvars and
 * optimizes simple sub expressions (e.g. 1+2).
 * It might modify *p.
 *
 * @param p - double pointer to a rval_expr (might be changed to a new one)
 * @return 0 on success, <0 on error (modifies also *p)
 */
int fix_rval_expr(void** p)
{
	struct rval_expr** prve;
	struct rval_expr* rve;
	union rval_val v;
	struct rvalue* rv;
	int ret;
	int i;
	
	prve=(struct rval_expr**)p;
	rve=*prve;
	
	switch(rve->op){
		case RVE_NONE_OP:
			BUG("empty rval expr\n");
			break;
		case RVE_RVAL_OP:
			return fix_rval(&rve->left.rval);
		case RVE_UMINUS_OP: /* unary operators */
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
			ret=fix_rval_expr((void**)&rve->left.rve);
			if (ret<0) return ret;
			break;
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MINUS_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_PLUS_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			ret=fix_rval_expr((void**)&rve->left.rve);
			if (ret<0) return ret;
			ret=fix_rval_expr((void**)&rve->right.rve);
			if (ret<0) return ret;
			break;
		default:
			BUG("unsupported op type %d\n", rve->op);
	}
	/* try to optimize */
	if (rve_can_optimize_int(rve)){
		if (rval_expr_eval_int(0, 0, &i, rve)<0){
			BUG("unexpected failure\n");
			return -1;
		}
		rve_destroy(rve->left.rve);
		rve_destroy(rve->right.rve);
		v.l=i;
		rval_init(&rve->left.rval, RV_INT, &v, 0);
		rval_init(&rve->right.rval, RV_NONE, 0, 0);
		DBG("FIXUP RVE: optimized int rve/rves (op %d) to %d\n", rve->op, i);
		rve->op=RVE_RVAL_OP;
	}else if (rve_can_optimize_str(rve)){
		if ((rv=rval_expr_eval(0, 0, rve))==0){
			BUG("unexpected failure\n");
			return -1;
		}
		if (rval_get_str(0, 0, &v.s, rv, 0)<0){
			BUG("unexpected failure\n");
			return -1;
		}
		rval_destroy(rv);
		rve_destroy(rve->left.rve);
		rve_destroy(rve->right.rve);
		rval_init(&rve->left.rval, RV_STR, &v, RV_CNT_ALLOCED_F);
		rval_init(&rve->right.rval, RV_NONE, 0, 0);
		DBG("FIXUP RVE: optimized str rves (op %d) to %.*s\n",
				rve->op, v.s.len, v.s.s);
		rve->op=RVE_RVAL_OP;
	}
	return 0;
}
