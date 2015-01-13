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
 * @brief Kamailio core :: rvalue expressions
 * @ingroup core
 * Module: \ref core
 */

/** special defines:
 *
 *  UNDEF_EQ_* - how to behave when undef is on the right side of a generic
 *               compare operator
 *  UNDEF_EQ_ALWAYS_FALSE:  undef  == something  is always false
 *  UNDEF_EQ_UNDEF_TRUE  :  undef == something false except for undef==undef
 *                          which is true
 *  no UNDEF_EQ* define  :  undef == expr => convert undef to typeof(expr)
 *                          and perform normal ==. undef == undef will be
 *                          converted to string and it will be true
 *                          ("" == "")
 * NOTE: expr == undef, with defined(expr) is always evaluated this way:
         expr == (type_of(expr))undef
 *  RV_STR2INT_VERBOSE_ERR - if a string conversion to int fails, log (L_WARN)
 *                           the string that caused it (only the string, not
 *                           the expression position).
 *  RV_STR2INT_ERR         - if a string conversion to int fails, don't ignore
 *                           the error (return error).
 *  RVAL_GET_INT_ERR_WARN  - if a conversion to int fails, log a warning with
 *                           the expression position.
 *                           Depends on RV_STR2INT_ERR.
 *  RVAL_GET_INT_ERR_IGN   - if a conversion to int fails, ignore the error
 *                           (the result will be 0). Can be combined with
 *                           RVAL_GET_INT_ERR_WARN.
 *                           Depends on RV_STR2INT_ERR.
 */


#include "rvalue.h"

#include <stdlib.h> /* abort() */

/* if defined warn when str2int conversions fail */
#define RV_STR2INT_VERBOSE_ERR

/* if defined rval_get_int will fail if str2int conversion fail
   (else convert to 0) */
#define RV_STR2INT_ERR

/* if a rval_get_int fails (conversion to int), warn
   Depends on RV_STR2INT_ERR.
 */
#define RVAL_GET_INT_ERR_WARN

/* if a rval_get_int fails, ignore it (expression evaluation will not fail,
   the int conversion will result in 0).
   Can be combined with RVAL_GET_INT_ERR_WARN.
   Depends on RV_STR2INT_ERR.
 */
#define RVAL_GET_INT_ERR_IGN

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
	if (rv->flags & RV_RE_ALLOCED_F){
		if (rv->v.re.regex){
			if (unlikely(rv->type!=RV_STR || !(rv->flags & RV_RE_F))){
				BUG("RV_RE_ALLOCED_F not supported for type %d or "
						"bad flags %x\n", rv->type, rv->flags);
			}
			regfree(rv->v.re.regex);
			pkg_free(rv->v.re.regex);
			rv->v.re.regex=0;
		}
		rv->flags&=~(RV_RE_ALLOCED_F|RV_RE_F);
	}
}



/** frees a rval returned by rval_new(), rval_convert() or rval_expr_eval().
 *   Note: it will be freed only when refcnt reaches 0
 */
void rval_destroy(struct rvalue* rv)
{
	if (rv && rv_unref(rv)){
		rval_force_clean(rv);
		/* still an un-regfreed RE ? */
		if ((rv->flags & RV_RE_F) && rv->v.re.regex){
			if (unlikely(rv->type!=RV_STR))
				BUG("RV_RE_F not supported for type %d\n", rv->type);
			regfree(rv->v.re.regex);
		}
		if (rv->flags & RV_RV_ALLOCED_F){
			pkg_free(rv);
		}
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
					BUG("rval expr rval with invalid refcnt: %d (%d,%d-%d,%d)"
							"\n", rve->left.rval.refcnt,
							rve->fpos.s_line, rve->fpos.s_col,
							rve->fpos.e_line, rve->fpos.e_col);
			}
			if (rve->right.rval.refcnt){
				if (rve->right.rval.refcnt==1)
					rval_destroy(&rve->right.rval);
				else
					BUG("rval expr rval with invalid refcnt: %d (%d,%d-%d,%d)"
							"\n", rve->right.rval.refcnt,
							rve->fpos.s_line, rve->fpos.s_col,
							rve->fpos.e_line, rve->fpos.e_col);
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
	if ((rvc->cache_type==RV_CACHE_PVAR) && (rvc->val_type!=RV_NONE)){
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



/** create a new pk_malloc'ed RE rv from a str re.
  * It acts as rval_new_str, but also compiles a RE from the str
  * and sets v->re.regex.
  * @param s - pointer to str, must be non-null, zero-term'ed and a valid RE.
  * @return new rv or 0 on error
  */
struct rvalue* rval_new_re(str* s)
{
	struct rvalue* rv;
	long offs;
	
	offs=(long)&((struct rvalue*)0)->buf[0]; /* offset of the buf. member */
	/* make sure we reserve enough space so that we can satisfy any regex_t
	   alignment requirement (pointer) */
	rv=rval_new_empty(ROUND_POINTER(offs)-offs+sizeof(*rv->v.re.regex)+
						s->len+1/* 0 */);
	if (likely(rv)){
		rv->type=RV_STR;
		/* make sure regex points to a properly aligned address
		   (use max./pointer alignment to be sure ) */
		rv->v.re.regex=(regex_t*)((char*)&rv->buf[0]+ROUND_POINTER(offs)-offs);
		rv->v.s.s=(char*)rv->v.re.regex+sizeof(*rv->v.re.regex);
		rv->v.s.len=s->len;
		memcpy(rv->v.s.s, s->s, s->len);
		rv->v.s.s[s->len]=0;
		/* compile the regex */
		/* same flags as for expr. =~ (fix_expr()) */
		if (unlikely(regcomp(rv->v.re.regex, s->s,
								REG_EXTENDED|REG_NOSUB|REG_ICASE))){
			/* error */
			pkg_free(rv);
			rv=0;
		}else /* success */
			rv->flags|=RV_RE_F;
	}
	return rv;
}



/** get string name for a type.
  *
  * @return - null terminated name of the type
  */
char* rval_type_name(enum rval_type type)
{
	switch(type){
		case RV_NONE:
			return "none";
		case RV_INT:
			return "int";
		case RV_STR:
			return "str";
		case RV_BEXPR:
			return "bexpr_t";
		case RV_ACTION_ST:
			return "action_t";
		case RV_PVAR:
			return "pvar";
		case RV_AVP:
			return "avp";
			break;
		case RV_SEL:
			return "select";
	}
	return "error_unknown_type";
}



/**
 * @brief create a new pk_malloc'ed rvalue from a rval_val union
 * @param t rvalue type
 * @param v rvalue value
 * @param extra_size extra space to allocate
 * (so that future string operation can reuse the space)
 * @return new rv or 0 on error
 */
struct rvalue* rval_new(enum rval_type t, union rval_val* v, int extra_size)
{
	struct rvalue* rv;
	
	if (t==RV_STR && v && v->s.s)
		return rval_new_str(&v->s, extra_size);
	rv=rval_new_empty(extra_size);
	if (likely(rv)){
		rv->type=t;
		if (likely(v && t!=RV_STR)){
			rv->v=*v;
		}else if (t==RV_STR){
			rv->v.s.s=&rv->buf[0];
			rv->v.s.len=0;
			if (likely(extra_size)) rv->v.s.s[0]=0;
		}else
			memset (&rv->v, 0, sizeof(rv->v));
	}
	return rv;
}



/**
 * @brief get rvalue basic type (RV_INT or RV_STR)
 *
 * Given a rvalue it tries to determinte its basic type.
 * Fills val_cache if non-null and empty (can be used in other rval*
 * function calls, to avoid re-resolving avps or pvars). It must be
 * rval_cache_clean()'en when no longer needed.
 *
 * @param h run action context
 * @param msg SIP message
 * @param rv target rvalue
 * @param val_cache write-only value cache, might be filled if non-null,
 * it _must_ be rval_cache_clean()'en when done.
 * @return basic type or RV_NONE on error
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
				val_cache->cache_type=RV_CACHE_PVAR;
			}else{
				val_cache=0;
				pv=&tmp_pval;
			}
			memset(pv, 0, sizeof(tmp_pval));
			if (likely(pv_get_spec_value(msg, &rv->v.pvs, pv)==0)){
				if (pv->flags & PV_TYPE_INT){
					if (likely(val_cache!=0))
						val_cache->val_type=RV_INT;
					else
						pv_value_destroy(pv);
					return RV_INT;
				}else if (pv->flags & PV_VAL_STR){
					if (likely(val_cache!=0))
						val_cache->val_type=RV_STR;
					else
						pv_value_destroy(pv);
					return RV_STR;
				}else{
					pv_value_destroy(pv);
					if (likely(val_cache!=0))
						val_cache->val_type=RV_NONE; /* undefined */
					goto error;
				}
			}else{
				if (likely(val_cache!=0))
					val_cache->val_type=RV_NONE; /* undefined */
				goto error;
			}
			break;
		case RV_AVP:
			if (likely(val_cache && val_cache->cache_type==RV_CACHE_EMPTY)){
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
		case RVE_BNOT_OP:
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
		case RVE_IPLUS_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
			return RV_INT;
		case RVE_PLUS_OP:
			/* '+' evaluates to the type of the left operand */
			return rve_guess_type(rve->left.rve);
		case RVE_CONCAT_OP:
		case RVE_STR_OP:
			return RV_STR;
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
		case RVE_BNOT_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
		case RVE_STR_OP:
			return rve_is_constant(rve->left.rve);
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
		case RVE_PLUS_OP:
		case RVE_IPLUS_OP:
		case RVE_CONCAT_OP:
			return rve_is_constant(rve->left.rve) &&
					rve_is_constant(rve->right.rve);
		case RVE_NONE_OP:
			break;
	}
	return 0;
}



/** returns true if the expression has side-effects.
  * @return  1 for possible side-effects, 0 for no side-effects
  * TODO: add better checks
  */
int rve_has_side_effects(struct rval_expr* rve)
{
	return !rve_is_constant(rve);
}



/** returns true if operator is unary (takes only 1 arg).
  * @return 0 or 1 on
  */
static int rve_op_unary(enum rval_expr_op op)
{
	switch(op){
		case RVE_RVAL_OP: /* not realy an operator */
			return -1;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_BNOT_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
		case RVE_STR_OP:
			return 1;
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
		case RVE_PLUS_OP:
		case RVE_IPLUS_OP:
		case RVE_CONCAT_OP:
			return 0;
		case RVE_NONE_OP:
			return -1;
			break;
	}
	return 0;
}



/**
 * @brief Returns 1 if expression is valid (type-wise)
 * @param type filled with the type of the expression (RV_INT, RV_STR or
 *                RV_NONE if it's dynamic)
 * @param rve  checked expression
 * @param bad_rve set on failure to the subexpression for which the 
 * type check failed
 * @param bad_t set on failure to the type of the bad subexpression
 * @param exp_t set on failure to the expected type for the bad
 * subexpression
 * @return 0 or 1 and sets *type to the resulting type
 * (RV_INT, RV_STR or RV_NONE if it can be found only at runtime)
 */
int rve_check_type(enum rval_type* type, struct rval_expr* rve,
					struct rval_expr** bad_rve, 
					enum rval_type* bad_t,
					enum rval_type* exp_t)
{
	enum rval_type type1, type2;
	
	switch(rve->op){
		case RVE_RVAL_OP:
			*type=rve_guess_type(rve);
			return 1;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_BNOT_OP:
			*type=RV_INT;
			if (rve_check_type(&type1, rve->left.rve, bad_rve, bad_t, exp_t)){
				if (type1==RV_STR){
					if (bad_rve) *bad_rve=rve->left.rve;
					if (bad_t) *bad_t=type1;
					if (exp_t) *exp_t=RV_INT;
					return 0;
				}
				return 1;
			}
			return 0;
			break;
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_IPLUS_OP:
			*type=RV_INT;
			if (rve_check_type(&type1, rve->left.rve, bad_rve, bad_t, exp_t)){
				if (type1==RV_STR){
					if (bad_rve) *bad_rve=rve->left.rve;
					if (bad_t) *bad_t=type1;
					if (exp_t) *exp_t=RV_INT;
					return 0;
				}
				if (rve_check_type(&type2, rve->right.rve, bad_rve,
									bad_t, exp_t)){
					if (type2==RV_STR){
						if (bad_rve) *bad_rve=rve->right.rve;
						if (bad_t) *bad_t=type2;
						if (exp_t) *exp_t=RV_INT;
						return 0;
					}
					return 1;
				}
			}
			return 0;
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			*type=RV_INT;
			if (rve_check_type(&type1, rve->left.rve, bad_rve, bad_t, exp_t)){
				if (rve_check_type(&type2, rve->right.rve, bad_rve, bad_t,
										exp_t)){
					if ((type2!=type1) && (type1!=RV_NONE) &&
							(type2!=RV_NONE) && 
							!(type1==RV_STR && type2==RV_INT)){
						if (bad_rve) *bad_rve=rve->right.rve;
						if (bad_t) *bad_t=type2;
						if (exp_t) *exp_t=type1;
						return 0;
					}
					return 1;
				}
			}
			return 0;
		case RVE_PLUS_OP:
			*type=RV_NONE;
			if (rve_check_type(&type1, rve->left.rve, bad_rve, bad_t, exp_t)){
				if (rve_check_type(&type2, rve->right.rve, bad_rve, bad_t,
									exp_t)){
					if ((type2!=type1) && (type1!=RV_NONE) &&
							(type2!=RV_NONE) && 
							!(type1==RV_STR && type2==RV_INT)){
						if (bad_rve) *bad_rve=rve->right.rve;
						if (bad_t) *bad_t=type2;
						if (exp_t) *exp_t=type1;
						return 0;
					}
					*type=type1;
					return 1;
				}
			}
			break;
		case RVE_CONCAT_OP:
			*type=RV_STR;
			if (rve_check_type(&type1, rve->left.rve, bad_rve, bad_t, exp_t)){
				if (rve_check_type(&type2, rve->right.rve, bad_rve, bad_t,
									exp_t)){
					if ((type2!=type1) && (type1!=RV_NONE) &&
							(type2!=RV_NONE) && 
							!(type1==RV_STR && type2==RV_INT)){
						if (bad_rve) *bad_rve=rve->right.rve;
						if (bad_t) *bad_t=type2;
						if (exp_t) *exp_t=type1;
						return 0;
					}
					if (type1==RV_INT){
						if (bad_rve) *bad_rve=rve->left.rve;
						if (bad_t) *bad_t=type1;
						if (exp_t) *exp_t=RV_STR;
						return 0;
					}
					return 1;
				}
			}
			break;
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
			*type=RV_INT;
			if (rve_check_type(&type1, rve->left.rve, bad_rve, bad_t, exp_t)){
				if (rve_check_type(&type2, rve->right.rve, bad_rve, bad_t,
									exp_t)){
					if ((type2!=type1) && (type1!=RV_NONE) &&
							(type2!=RV_NONE) &&
							!(type1==RV_STR && type2==RV_INT)){
						if (bad_rve) *bad_rve=rve->right.rve;
						if (bad_t) *bad_t=type2;
						if (exp_t) *exp_t=type1;
						return 0;
					}
					if (type1==RV_INT){
						if (bad_rve) *bad_rve=rve->left.rve;
						if (bad_t) *bad_t=type1;
						if (exp_t) *exp_t=RV_STR;
						return 0;
					}
					return 1;
				}
			}
			break;
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
			*type=RV_INT;
			if (rve_check_type(&type1, rve->left.rve, bad_rve, bad_t, exp_t)){
				if (type1==RV_INT){
					if (bad_rve) *bad_rve=rve->left.rve;
					if (bad_t) *bad_t=type1;
					if (exp_t) *exp_t=RV_STR;
					return 0;
				}
				return 1;
			}
			break;
		case RVE_INT_OP:
			*type=RV_INT;
			return 1;
			break;
		case RVE_STR_OP:
			*type=RV_STR;
			return 1;
			break;
		case RVE_NONE_OP:
		default:
			BUG("unexpected rve op %d (%d,%d-%d,%d)\n", rve->op,
					rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col);
			if (bad_rve) *bad_rve=rve;
			if (bad_t) *bad_t=RV_NONE;
			if (exp_t) *exp_t=RV_STR;
			break;
	}
	return 0;
}



/** get the integer value of an rvalue.
  * *i=(int)rv
  * if rv == undefined select, avp or pvar, return 0.
  * if an error occurs while evaluating a select, avp or pvar, behave as
  * for the undefined case (and return success).
  * @param h - script context handle
  * @param msg - sip msg
  * @param i   - pointer to int, where the conversion result will be stored
  * @param rv   - rvalue to be converted
  * @param cache - cached rv value (read-only), can be 0
  *
  * @return 0 on success, \<0 on error and EXPR_DROP on drop
 */
int rval_get_int(struct run_act_ctx* h, struct sip_msg* msg,
								int* i, struct rvalue* rv,
								struct rval_cache* cache)
{
	avp_t* r_avp;
	int_str avp_val;
	pv_value_t pval;
	str tmp;
	str* s;
	int r, ret;
	int destroy_pval;
	
	destroy_pval=0;
	s=0;
	ret=0;
	switch(rv->type){
		case RV_INT:
			*i=rv->v.l;
			break;
		case RV_STR:
			s=&rv->v.s;
			goto rv_str;
		case RV_BEXPR:
			*i=eval_expr(h, rv->v.bexpr, msg);
			if (*i==EXPR_DROP){
				*i=0; /* false */
				return EXPR_DROP;
			}
			break;
		case RV_ACTION_ST:
			if (rv->v.action) {
				*i=(run_actions_safe(h, rv->v.action, msg)>0);
				h->run_flags &= ~(RETURN_R_F|BREAK_R_F); /* catch return &
														    break in expr*/
			} else
				*i=0;
			break;
		case RV_SEL:
			r=run_select(&tmp, &rv->v.sel, msg);
			if (unlikely(r!=0)){
				if (r<0)
					goto eval_error;
				else /* i>0  => undefined */
					goto undef;
			}
			s=&tmp;
			goto rv_str;
		case RV_AVP:
			if (unlikely(cache && cache->cache_type==RV_CACHE_AVP)){
				if (likely(cache->val_type==RV_INT)){
					*i=cache->c.avp_val.n;
				}else if (cache->val_type==RV_STR){
					s=&cache->c.avp_val.s;
					goto rv_str;
				}else if (cache->val_type==RV_NONE)
					goto undef;
				else goto error_cache;
			}else{
				r_avp = search_avp_by_index(rv->v.avps.type, rv->v.avps.name,
											&avp_val, rv->v.avps.index);
				if (likely(r_avp)){
					if (unlikely(r_avp->flags & AVP_VAL_STR)){
						s=&avp_val.s;
						goto rv_str;
					}else{
						*i=avp_val.n;
					}
				}else{
					goto undef;
				}
			}
			break;
		case RV_PVAR:
			if (unlikely(cache && cache->cache_type==RV_CACHE_PVAR)){
				if (likely((cache->val_type==RV_INT) ||
								(cache->c.pval.flags & PV_VAL_INT))){
					*i=cache->c.pval.ri;
				}else if (cache->val_type==RV_STR){
					s=&cache->c.pval.rs;
					goto rv_str;
				}else if (cache->val_type==RV_NONE)
					goto undef;
				else goto error_cache;
			}else{
				memset(&pval, 0, sizeof(pval));
				if (likely(pv_get_spec_value(msg, &rv->v.pvs, &pval)==0)){
					if (likely(pval.flags & PV_VAL_INT)){
						*i=pval.ri;
						pv_value_destroy(&pval);
					}else if (likely(pval.flags & PV_VAL_STR)){
						destroy_pval=1; /* we must pv_value_destroy() later*/
						s=&pval.rs;
						goto rv_str;
					}else{
						/* no PV_VAL_STR and no PV_VAL_INT => undef
						   (PV_VAL_NULL) */
						pv_value_destroy(&pval);
						goto undef;
					}
				}else{
					goto eval_error;
				}
			}
			break;
		default:
			BUG("rv type %d not handled\n", rv->type);
			goto error;
	}
	return ret;
undef:
eval_error: /* same as undefined */
	/* handle undefined => result 0, return success */
	*i=0;
	return 0;
rv_str:
	/* rv is of string type => try to convert it to int */
	/* if "" => 0 (most likely case) */
	if (likely(s->len==0)) *i=0;
	else if (unlikely(str2sint(s, i)!=0)){
		/* error converting to int => non numeric => 0 */
		*i=0;
#ifdef RV_STR2INT_VERBOSE_ERR
		WARN("automatic string to int conversion for \"%.*s\" failed\n",
				s->len, ZSW(s->s));
		/* return an error code */
#endif
#ifdef RV_STR2INT_ERR
		ret=-1;
#endif
	}
	if (destroy_pval)
		pv_value_destroy(&pval);
	return ret;
error_cache:
	BUG("invalid cached value:cache type %d, value type %d\n",
			cache?cache->cache_type:0, cache?cache->val_type:0);
error:
	if (destroy_pval)
		pv_value_destroy(&pval);
	*i=0;
	return -1;
}



/** log a message, appending rve position and a '\n'.*/
#define RVE_LOG(lev, rve, txt) \
	LOG((lev), txt " (%d,%d-%d,%d)\n", \
			(rve)->fpos.s_line, rve->fpos.s_col, \
			(rve)->fpos.e_line, rve->fpos.e_col )


/** macro for checking and handling rval_get_int() retcode.
 * check if the return code is an rval_get_int error and if so
 * handle the error (e.g. print a log message, ignore the error by
 * setting ret to 0 a.s.o.)
 * @param ret - retcode as returned by rval_get_int() (might be changed)
 * @param txt - warning message txt (no pointer allowed)
 * @param rve - rval_expr, used to access the config. pos
 */
#if defined RVAL_GET_INT_ERR_WARN && defined RVAL_GET_INT_ERR_IGN
#define rval_get_int_handle_ret(ret, txt, rve) \
	do { \
		if (unlikely((ret)<0)) { \
			RVE_LOG(L_WARN, rve, txt); \
			(ret)=0; \
		} \
	}while(0)
#elif defined RVAL_GET_INT_ERR_WARN
#define rval_get_int_handle_ret(ret, txt, rve) \
	do { \
		if (unlikely((ret)<0)) \
			RVE_LOG(L_WARN, rve, txt); \
	}while(0)
#elif defined RVAL_GET_INT_ERR_IGN
#define rval_get_int_handle_ret(ret, txt, rve) \
	do { \
		if (unlikely((ret)<0)) \
				(ret)=0; \
	} while(0)
#else
#define rval_get_int_handle_ret(ret, txt, rve) /* do nothing */
#endif






/** get the string value of an rv in a tmp variable
  * *s=(str)rv
  * if rv == undefined select, avp or pvar, return "".
  * if an error occurs while evaluating a select, avp or pvar, behave as
  * for the undefined case (and return success).
  * The result points either inside the passed rv, inside
  * new_cache or inside an avp. new_cache must be non zero,
  * initialized previously and it _must_ be rval_cache_clean(...)'ed when
  * done.
  * WARNING: it's not intended for general use. It might return a pointer
  * inside rv so the result _must_ be treated as read-only. rv and new_cache
  * must not be released/freed until the result is no longer needed.
  * For general use see  rval_get_str().
  * @param h - script context handle
  * @param msg - sip msg
  * @param tmpv - str return value (pointer to a str struct that will be
  *               be filled with the conversion result)
  * @param rv   - rvalue to be converted
  * @param cache - cached rv value (read-only), can be 0
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
	int i;
	
	switch(rv->type){
		case RV_INT:
			tmpv->s=sint2strbuf(rv->v.l, tmp_cache->i2s,
								sizeof(tmp_cache->i2s), &tmpv->len);
			tmp_cache->cache_type = RV_CACHE_INT2STR;
			break;
		case RV_STR:
			*tmpv=rv->v.s;
			break;
		case RV_ACTION_ST:
			if (rv->v.action) {
				i=(run_actions_safe(h, rv->v.action, msg)>0);
				h->run_flags &= ~(RETURN_R_F|BREAK_R_F); /* catch return &
														    break in expr*/
			} else
				i=0;
			tmpv->s=sint2strbuf(i, tmp_cache->i2s,
								sizeof(tmp_cache->i2s), &tmpv->len);
			tmp_cache->cache_type = RV_CACHE_INT2STR;
			break;
		case RV_BEXPR:
			i=eval_expr(h, rv->v.bexpr, msg);
			if (i==EXPR_DROP){
				i=0; /* false */
				tmpv->s=sint2strbuf(i, tmp_cache->i2s,
						sizeof(tmp_cache->i2s), &tmpv->len);
				tmp_cache->cache_type = RV_CACHE_INT2STR;
				return EXPR_DROP;
			}
			tmpv->s=sint2strbuf(i, tmp_cache->i2s, sizeof(tmp_cache->i2s),
								&tmpv->len);
			tmp_cache->cache_type = RV_CACHE_INT2STR;
			break;
		case RV_SEL:
			i=run_select(tmpv, &rv->v.sel, msg);
			if (unlikely(i!=0)){
				if (i<0){
					goto eval_error;
				}else { /* i>0  => undefined */
					goto undef;
				}
			}
			break;
		case RV_AVP:
			if (likely(cache && cache->cache_type==RV_CACHE_AVP)){
				if (likely(cache->val_type==RV_STR)){
					*tmpv=cache->c.avp_val.s;
				}else if (cache->val_type==RV_INT){
					i=cache->c.avp_val.n;
					tmpv->s=sint2strbuf(i, tmp_cache->i2s,
										sizeof(tmp_cache->i2s), &tmpv->len);
					tmp_cache->cache_type = RV_CACHE_INT2STR;
				}else if (cache->val_type==RV_NONE){
					goto undef;
				}else goto error_cache;
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
						tmpv->s=sint2strbuf(i, tmp_cache->i2s,
										sizeof(tmp_cache->i2s), &tmpv->len);
						tmp_cache->cache_type = RV_CACHE_INT2STR;
					}
				}else goto undef;
			}
			break;
		case RV_PVAR:
			if (likely(cache && cache->cache_type==RV_CACHE_PVAR)){
				if (likely(cache->val_type==RV_STR)){
					*tmpv=cache->c.pval.rs;
				}else if (cache->val_type==RV_INT){
					i=cache->c.pval.ri;
					tmpv->s=sint2strbuf(i, tmp_cache->i2s,
										sizeof(tmp_cache->i2s), &tmpv->len);
					tmp_cache->cache_type = RV_CACHE_INT2STR;
				}else if (cache->val_type==RV_NONE){
					goto undef;
				}else goto error_cache;
			}else{
				memset(&tmp_cache->c.pval, 0, sizeof(tmp_cache->c.pval));
				if (likely(pv_get_spec_value(msg, &rv->v.pvs,
												&tmp_cache->c.pval)==0)){
					if (likely(tmp_cache->c.pval.flags & PV_VAL_STR)){
						/*  the value is not destroyed, but saved instead
							in tmp_cache so that it can be destroyed later
							when no longer needed */
						tmp_cache->cache_type=RV_CACHE_PVAR;
						tmp_cache->val_type=RV_STR;
						*tmpv=tmp_cache->c.pval.rs;
					}else if (likely(tmp_cache->c.pval.flags & PV_VAL_INT)){
						i=tmp_cache->c.pval.ri;
						pv_value_destroy(&tmp_cache->c.pval);
						tmpv->s=sint2strbuf(i, tmp_cache->i2s,
										sizeof(tmp_cache->i2s), &tmpv->len);
						tmp_cache->cache_type = RV_CACHE_INT2STR;
					}else{
						/* no PV_VAL_STR and no PV_VAL_INT => undef
						   (PV_VAL_NULL) */
						pv_value_destroy(&tmp_cache->c.pval);
						goto undef;
					}
				}else{
					goto eval_error;
				}
			}
			break;
		default:
			BUG("rv type %d not handled\n", rv->type);
			goto error;
	}
	return 0;
undef:
eval_error: /* same as undefined */
	/* handle undefined => result "", return success */
	tmpv->s="";
	tmpv->len=0;
	return 0;
error_cache:
	BUG("invalid cached value:cache type %d, value type %d\n",
			cache?cache->cache_type:0, cache?cache->val_type:0);
error:
	tmpv->s="";
	tmpv->len=0;
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



/**
 * @brief Convert a rvalue to another rvalue, of a specific type
 *
 * Convert a rvalue to another rvalue, of a specific type.
 * The result is read-only in most cases (can be a reference
 * to another rvalue, can be checked by using rv_chg_in_place()) and
 * _must_ be rval_destroy()'ed.
 *
 * @param h run action context
 * @param msg SIP mesasge
 * @param type - type to convert to
 * @param v - rvalue to convert
 * @param c - rval_cache (cached v value if known/filled by another
 *            function), can be 0 (unknown/not needed)
 * @return pointer to a rvalue (reference to an existing one or a new
 * one, @see rv_chg_in_place() and the above comment), or 0 on error.
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
		case RVE_BNOT_OP:
			*res=~v;
			break;
		default:
			BUG("rv unsupported intop1 %d\n", op);
			return -1;
	}
	return 0;
}



/** integer operation: *res= v1 op v2
  * @return 0 on succes, \<0 on error
  */
inline static int int_intop2(int* res, enum rval_expr_op op, int v1, int v2)
{
	switch(op){
		case RVE_PLUS_OP:
		case RVE_IPLUS_OP:
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
		case RVE_MOD_OP:
			if (unlikely(v2==0)){
				ERR("rv mod by 0\n");
				return -1;
			}
			*res=v1%v2;
			break;
		case RVE_BOR_OP:
			*res=v1|v2;
			break;
		case RVE_BAND_OP:
			*res=v1&v2;
			break;
		case RVE_BXOR_OP:
			*res=v1^v2;
			break;
		case RVE_BLSHIFT_OP:
			*res=v1<<v2;
			break;
		case RVE_BRSHIFT_OP:
			*res=v1>>v2;
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
		case RVE_IEQ_OP:
			*res=v1 == v2;
			break;
		case RVE_DIFF_OP:
		case RVE_IDIFF_OP:
			*res=v1 != v2;
			break;
		case RVE_CONCAT_OP:
			*res=0;
			/* invalid operand for int */
			return -1;
		default:
			BUG("rv unsupported intop %d\n", op);
			return -1;
	}
	return 0;
}



/** internal helper: compare 2 RV_STR RVs.
  * Warning: rv1 & rv2 must be RV_STR
  * @return 0 on success, -1 on error
  */
inline static int bool_rvstrop2( enum rval_expr_op op, int* res,
								struct rvalue* rv1, struct rvalue* rv2)
{
	str* s1;
	str* s2;
	regex_t tmp_re;
	
	s1=&rv1->v.s;
	s2=&rv2->v.s;
	switch(op){
		case RVE_EQ_OP:
		case RVE_STREQ_OP:
			*res= (s1->len==s2->len) && (memcmp(s1->s, s2->s, s1->len)==0);
			break;
		case RVE_DIFF_OP:
		case RVE_STRDIFF_OP:
			*res= (s1->len!=s2->len) || (memcmp(s1->s, s2->s, s1->len)!=0);
			break;
		case RVE_MATCH_OP:
			if (likely(rv2->flags & RV_RE_F)){
				*res=(regexec(rv2->v.re.regex, rv1->v.s.s, 0, 0, 0)==0);
			}else{
				/* we need to compile the RE on the fly */
				if (unlikely(regcomp(&tmp_re, s2->s,
										REG_EXTENDED|REG_NOSUB|REG_ICASE))){
					/* error */
					ERR("Bad regular expression \"%s\"\n", s2->s);
					goto error;
				}
				*res=(regexec(&tmp_re, s1->s, 0, 0, 0)==0);
				regfree(&tmp_re);
			}
			break;
		default:
			BUG("rv unsupported intop %d\n", op);
			goto error;
	}
	return 0;
error:
	*res=0; /* false */
	return -1;
}



/** integer returning operation on string: *res= op str (returns integer)
  * @return 0 on succes, \<0 on error
  */
inline static int int_strop1(int* res, enum rval_expr_op op, str* s1)
{
	switch(op){
		case RVE_STRLEN_OP:
			*res=s1->len;
			break;
		case RVE_STREMPTY_OP:
			*res=(s1->len==0);
			break;
		default:
			BUG("rv unsupported int_strop1 %d\n", op);
			*res=0;
			return -1;
	}
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
	memmove(ret->buf+rv1->v.s.len, s2->s, s2->len);
	if (s1){
		memcpy(ret->buf, s1->s, s1->len);
	}
	ret->v.s.len=rv1->v.s.len+s2->len;
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
	ret=bool_rvstrop2(op, res, rv1, rv2);
	rval_destroy(rv1); 
	rval_destroy(rv2); 
	return ret;
error:
	rval_destroy(rv1); 
	rval_destroy(rv2); 
	return 0;
}



/**
 * @brief Integer operation on rval evaluated as string
 * 
 * Integer operation on rval evaluated as string, can use cached
 * rvalues (c1 & c2).
 * @param h run action context
 * @param msg SIP message
 * @param res will be set to the result
 * @param op rvalue expression operation
 * @param l rvalue
 * @param c1 rvalue cache
 * @return 0 success, -1 on error
 */
inline static int rval_int_strop1(struct run_act_ctx* h,
						 struct sip_msg* msg,
						 int* res,
						 enum rval_expr_op op,
						 struct rvalue* l,
						 struct rval_cache* c1)
{
	struct rvalue* rv1;
	int ret;
	
	rv1=0;
	ret=0;
	if ((rv1=rval_convert(h, msg, RV_STR, l, c1))==0)
		goto error;
	ret=int_strop1(res, op, &rv1->v.s);
	rval_destroy(rv1); 
	return ret;
error:
	*res=0;
	rval_destroy(rv1); 
	return -1;
}



/**
 * @brief Checks if rv is defined
 * @param h run action context
 * @param msg SIP message
 * @param res set to the result 1 is defined, 0 not defined
 * @param rv rvalue
 * @param cache rvalue cache
 * @return 0 on success, -1 on error
 * @note Can use cached rvalues (cache). A rv can be undefined if it's
 * an undefined avp or pvar or select or if it's NONE
 * @note An error in the avp, pvar or select search is equivalent to
 * undefined (and it's not reported)
 */
inline static int rv_defined(struct run_act_ctx* h,
						 struct sip_msg* msg, int* res,
						 struct rvalue* rv, struct rval_cache* cache)
{
	avp_t* r_avp;
	int_str avp_val;
	pv_value_t pval;
	str tmp;
	
	*res=1;
	switch(rv->type){
		case RV_SEL:
			if (unlikely(cache && cache->cache_type==RV_CACHE_SELECT)){
				*res=(cache->val_type!=RV_NONE);
			}else
				/* run select returns 0 on success, -1 on error and >0 on 
				   undefined. error is considered undefined */
				*res=(run_select(&tmp, &rv->v.sel, msg)==0);
			break;
		case RV_AVP:
			if (unlikely(cache && cache->cache_type==RV_CACHE_AVP)){
				*res=(cache->val_type!=RV_NONE);
			}else{
				r_avp = search_avp_by_index(rv->v.avps.type, rv->v.avps.name,
											&avp_val, rv->v.avps.index);
				if (unlikely(r_avp==0)){
					*res=0;
				}
			}
			break;
		case RV_PVAR:
			/* PV_VAL_NULL or pv_get_spec_value error => undef */
			if (unlikely(cache && cache->cache_type==RV_CACHE_PVAR)){
				*res=(cache->val_type!=RV_NONE);
			}else{
				memset(&pval, 0, sizeof(pval));
				if (likely(pv_get_spec_value(msg, &rv->v.pvs, &pval)==0)){
					if ((pval.flags & PV_VAL_NULL) &&
							! (pval.flags & (PV_VAL_INT|PV_VAL_STR))){
						*res=0;
					}
					pv_value_destroy(&pval);
				}else{
					*res=0; /* in case of error, consider it undef */
				}
			}
			break;
		case RV_NONE:
			*res=0;
			break;
		default:
			break;
	}
	return 0;
}


/**
 * @brief Defined (integer) operation on rve
 * @param h run action context
 * @param msg SIP message
 * @param res - set to  1 defined, 0 not defined
 * @param rve rvalue expression
 * @return 0 on success, -1 on error
 */
inline static int int_rve_defined(struct run_act_ctx* h,
						 struct sip_msg* msg, int* res,
						 struct rval_expr* rve)
{
	/* only a rval can be undefined, any expression consisting on more
	   then one rval => defined */
	if (likely(rve->op==RVE_RVAL_OP))
		return rv_defined(h, msg, res, &rve->left.rval, 0);
	*res=1;
	return 0;
}



/** evals an integer expr  to an int.
 * 
 *  *res=(int)eval(rve)
 *  @return 0 on success, \<0 on error
 */
int rval_expr_eval_int( struct run_act_ctx* h, struct sip_msg* msg,
						int* res, struct rval_expr* rve)
{
	int i1, i2, ret;
	struct rval_cache c1, c2;
	struct rvalue* rv1;
	struct rvalue* rv2;
	
	ret=-1;
	switch(rve->op){
		case RVE_RVAL_OP:
			ret=rval_get_int(h, msg, res,  &rve->left.rval, 0);
			rval_get_int_handle_ret(ret, "rval expression conversion to int"
										" failed", rve);
			break;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_BNOT_OP:
			if (unlikely(
					(ret=rval_expr_eval_int(h, msg, &i1, rve->left.rve)) <0) )
				break;
			ret=int_intop1(res, rve->op, i1);
			break;
		case RVE_INT_OP:
			ret=rval_expr_eval_int(h, msg, res, rve->left.rve);
			break;
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_MINUS_OP:
		case RVE_PLUS_OP:
		case RVE_IPLUS_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
			if (unlikely(
					(ret=rval_expr_eval_int(h, msg, &i1, rve->left.rve)) <0) )
				break;
			if (unlikely(
					(ret=rval_expr_eval_int(h, msg, &i2, rve->right.rve)) <0) )
				break;
			ret=int_intop2(res, rve->op, i1, i2);
			break;
		case RVE_LAND_OP:
			if (unlikely(
					(ret=rval_expr_eval_int(h, msg, &i1, rve->left.rve)) <0) )
				break;
			if (i1==0){
				*res=0;
			}else{
				if (unlikely( (ret=rval_expr_eval_int(h, msg, &i2,
										rve->right.rve)) <0) )
					break;
				*res=i1 && i2;
			}
			ret=0;
			break;
		case RVE_LOR_OP:
			if (unlikely(
					(ret=rval_expr_eval_int(h, msg, &i1, rve->left.rve)) <0) )
				break;
			if (i1){
				*res=1;
			}else{
				if (unlikely( (ret=rval_expr_eval_int(h, msg, &i2,
										rve->right.rve)) <0) )
					break;
				*res=i1 || i2;
			}
			ret=0;
			break;
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
			/* if left is string, eval left & right as string and
			 *   use string diff.
			 * if left is int eval as int using int diff
			 * if left is undef, look at right and convert to right type
			 */
			rval_cache_init(&c1);
			if (unlikely( (ret=rval_expr_eval_rvint(h, msg, &rv1, &i1,
													rve->left.rve, &c1))<0)){
				/* error */
				rval_cache_clean(&c1);
				break;
			}
			if (likely(rv1==0)){
				/* int */
				rval_cache_clean(&c1);
				if (unlikely( (ret=rval_expr_eval_int(h, msg, &i2,
														rve->right.rve)) <0) )
					break;  /* error */
				ret=int_intop2(res, rve->op, i1, i2);
			}else{
				/* not int => str or undef */
				/* check for undefined left operand */
				if (unlikely( c1.cache_type!=RV_CACHE_EMPTY &&
								c1.val_type==RV_NONE)){
#ifdef UNDEF_EQ_ALWAYS_FALSE
					/* undef == something  always false
					   undef != something  always true*/
					ret=(rve->op==RVE_DIFF_OP);
#elif defined UNDEF_EQ_UNDEF_TRUE
					/* undef == something defined always false
					   undef == undef true */
					if (int_rve_defined(h, msg, &i2, rve->right.rve)<0){
						/* error */
						rval_cache_clean(&c1);
						rval_destroy(rv1);
						break;
					}
					ret=(!i2) ^ (rve->op==RVE_DIFF_OP);
#else  /* ! UNDEF_EQ_* */
					/*  undef == val
					 *  => convert to (type_of(val)) (undef) == val */
					rval_cache_init(&c2);
					if (unlikely( (ret=rval_expr_eval_rvint(h, msg, &rv2, &i2,
													rve->right.rve, &c2))<0)){
						/* error */
						rval_cache_clean(&c1);
						rval_cache_clean(&c2);
						rval_destroy(rv1);
						break;
					}
					if (rv2==0){
						/* int */
						ret=int_intop2(res, rve->op, 0 /* undef */, i2);
					}else{
						/* str or undef */
						ret=rval_str_lop2(h, msg, res, rve->op, rv1, &c1,
											rv2, &c2);
						rval_cache_clean(&c2);
						rval_destroy(rv2);
					}
#endif /* UNDEF_EQ_* */
					rval_cache_clean(&c1);
					rval_destroy(rv1);
				}else{
					/* left value == defined and != int => str
					 * => lval == (str) val */
					if (unlikely((rv2=rval_expr_eval(h, msg,
														rve->right.rve))==0)){
						/* error */
						rval_destroy(rv1);
						rval_cache_clean(&c1);
						break;
					}
					ret=rval_str_lop2(h, msg, res, rve->op, rv1, &c1, rv2, 0);
					rval_cache_clean(&c1);
					rval_destroy(rv1);
					rval_destroy(rv2);
				}
			}
			break;
		case RVE_CONCAT_OP:
			/* eval expression => string */
			if (unlikely((rv1=rval_expr_eval(h, msg, rve))==0)){
				ret=-1;
				break;
			}
			/* convert to int */
			ret=rval_get_int(h, msg, res, rv1, 0); /* convert to int */
			rval_get_int_handle_ret(ret, "rval expression conversion to int"
										" failed", rve);
			rval_destroy(rv1);
			break;
		case RVE_STR_OP:
			/* (str)expr => eval expression */
			rval_cache_init(&c1);
			if (unlikely((ret=rval_expr_eval_rvint(h, msg, &rv1, res,
													rve->left.rve, &c1))<0)){
				/* error */
				rval_cache_clean(&c1);
				break;
			}
			if (unlikely(rv1)){
				/* expr evaluated to string => (int)(str)v == (int)v */
				ret=rval_get_int(h, msg, res, rv1, &c1); /* convert to int */
				rval_get_int_handle_ret(ret, "rval expression conversion"
												" to int failed", rve);
				rval_destroy(rv1);
				rval_cache_clean(&c1);
			} /* else (rv1==0)
				 => expr evaluated to int => 
				 return (int)(str)v == (int)v => do nothing */
			break;

#if 0
			/* same thing as above, but in a not optimized, easier to
			   understand way */
			/* 1. (str) expr => eval expr */
			if (unlikely((rv1=rval_expr_eval(h, msg, rve->left.rve))==0)){
				ret=-1;
				break;
			}
			/* 2. convert to str and then convert to int
			   but since (int)(str)v == (int)v skip over (str)v */
			ret=rval_get_int(h, msg, res, rv1, 0); /* convert to int */
			rval_destroy(rv1);
			break;
#endif
		case RVE_DEFINED_OP:
			ret=int_rve_defined(h, msg, res, rve->left.rve);
			break;
		case RVE_NOTDEFINED_OP:
			ret=int_rve_defined(h, msg, res, rve->left.rve);
			*res = !(*res);
			break;
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
			if (unlikely((rv1=rval_expr_eval(h, msg, rve->left.rve))==0)){
				ret=-1;
				break;
			}
			if (unlikely((rv2=rval_expr_eval(h, msg, rve->right.rve))==0)){
				rval_destroy(rv1);
				ret=-1;
				break;
			}
			ret=rval_str_lop2(h, msg, res, rve->op, rv1, 0, rv2, 0);
			rval_destroy(rv1);
			rval_destroy(rv2);
			break;
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
			if (unlikely((rv1=rval_expr_eval(h, msg, rve->left.rve))==0)){
					ret=-1;
					break;
			}
			ret=rval_int_strop1(h, msg, res, rve->op, rv1, 0);
			rval_destroy(rv1);
			break;
		case RVE_NONE_OP:
		/*default:*/
			BUG("invalid rval int expression operation %d (%d,%d-%d,%d)\n",
					rve->op, rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col);
			ret=-1;
	};
	return ret;
}



/**
 * @brief Evals a rval expression into an int or another rv(str)
 * @warning rv result (rv_res) must be rval_destroy()'ed if non-null
 * (it might be a reference to another rval). The result can be
 * modified only if rv_chg_in_place() returns true.
 * @param h run action context
 * @param msg SIP message
 * @param res_rv pointer to rvalue result, if non-null it means the 
 * expression evaluated to a non-int (str), which will be stored here.
 * @param res_i pointer to int result, if res_rv==0 and the function
 * returns success => the result is an int which will be stored here.
 * @param rve expression that will be evaluated.
 * @param cache write-only value cache, it might be filled if non-null and
 * empty (rval_cache_init()). If non-null, it _must_ be rval_cache_clean()'ed
 * when done. 
 * @return 0 on success, -1 on error, sets *res_rv or *res_i.
 */
int rval_expr_eval_rvint(			   struct run_act_ctx* h,
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
	ret=-1;
	switch(rve->op){
		case RVE_RVAL_OP:
			rv1=&rve->left.rval;
			rv_ref(rv1);
			type=rval_get_btype(h, msg, rv1, cache);
			if (type==RV_INT){
					r=rval_get_int(h, msg, res_i, rv1, cache);
					rval_get_int_handle_ret(r, "rval expression conversion"
												" to int failed", rve);
					*res_rv=0;
					ret=r; /* equiv. to if (r<0) goto error */
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
		case RVE_BNOT_OP:
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_IPLUS_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
			/* operator forces integer type */
			ret=rval_expr_eval_int(h, msg, res_i, rve);
			*res_rv=0;
			break;
		case RVE_PLUS_OP:
			rval_cache_init(&c1);
			r=rval_expr_eval_rvint(h, msg, &rv1, &i, rve->left.rve, &c1);
			if (unlikely(r<0)){
				ERR("rval expression evaluation failed (%d,%d-%d,%d)\n",
						rve->left.rve->fpos.s_line, rve->left.rve->fpos.s_col,
						rve->left.rve->fpos.e_line, rve->left.rve->fpos.e_col
					);
				rval_cache_clean(&c1);
				goto error;
			}
			if (rv1==0){
				if (unlikely((r=rval_expr_eval_int(h, msg, &j,
														rve->right.rve))<0)){
						ERR("rval expression evaluation failed (%d,%d-%d,%d)"
								"\n", rve->right.rve->fpos.s_line,
								rve->right.rve->fpos.s_col,
								rve->right.rve->fpos.e_line,
								rve->right.rve->fpos.e_col);
						rval_cache_clean(&c1);
						goto error;
				}
				ret=int_intop2(res_i, rve->op, i, j);
				*res_rv=0;
			}else{
				rv2=rval_expr_eval(h, msg, rve->right.rve);
				if (unlikely(rv2==0)){
					ERR("rval expression evaluation failed (%d,%d-%d,%d)\n",
								rve->right.rve->fpos.s_line,
								rve->right.rve->fpos.s_col,
								rve->right.rve->fpos.e_line,
								rve->right.rve->fpos.e_col);
					rval_cache_clean(&c1);
					goto error;
				}
				*res_rv=rval_str_add2(h, msg, rv1, &c1, rv2, 0);
				ret=-(*res_rv==0);
			}
			rval_cache_clean(&c1);
			break;
		case RVE_CONCAT_OP:
		case RVE_STR_OP:
			*res_rv=rval_expr_eval(h, msg, rve);
			ret=-(*res_rv==0);
			break;
		case RVE_NONE_OP:
		/*default:*/
			BUG("invalid rval expression operation %d (%d,%d-%d,%d)\n",
					rve->op, rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col);
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



/**
 * @brief Evals a rval expression
 * @warning result must be rval_destroy()'ed if non-null (it might be
 * a reference to another rval). The result can be modified only
 * if rv_chg_in_place() returns true.
 * @param h run action context
 * @param msg SIP message
 * @param rve rvalue expression
 * @return rvalue on success, 0 on error
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
	ret=0;
	switch(rve->op){
		case RVE_RVAL_OP:
			rv_ref(&rve->left.rval);
			return &rve->left.rval;
			break;
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_BNOT_OP:
		case RVE_MINUS_OP:
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_IPLUS_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
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
				ERR("rval expression evaluation failed (%d,%d-%d,%d)\n",
						rve->fpos.s_line, rve->fpos.s_col,
						rve->fpos.e_line, rve->fpos.e_col);
				goto error;
			}
			break;
		case RVE_PLUS_OP:
			rv1=rval_expr_eval(h, msg, rve->left.rve);
			if (unlikely(rv1==0)){
				ERR("rval expression evaluation failed (%d,%d-%d,%d)\n",
						rve->left.rve->fpos.s_line, rve->left.rve->fpos.s_col,
						rve->left.rve->fpos.e_line, rve->left.rve->fpos.e_col);
				goto error;
			}
			rval_cache_init(&c1);
			type=rval_get_btype(h, msg, rv1, &c1);
			switch(type){
				case RV_INT:
					r=rval_get_int(h, msg, &i, rv1, &c1);
					rval_get_int_handle_ret(r, "rval expression left side "
												"conversion to int failed",
											rve);
					if (unlikely(r<0)){
						rval_cache_clean(&c1);
						goto error;
					}
					if (unlikely((r=rval_expr_eval_int(h, msg, &j,
														rve->right.rve))<0)){
						rval_cache_clean(&c1);
						ERR("rval expression evaluation failed (%d,%d-%d,%d):"
								" could not evaluate right side to int\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col);
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
				case RV_NONE:
					rv2=rval_expr_eval(h, msg, rve->right.rve);
					if (unlikely(rv2==0)){
						ERR("rval expression evaluation failed (%d,%d-%d,%d)"
								"\n", rve->right.rve->fpos.s_line,
								rve->right.rve->fpos.s_col,
								rve->right.rve->fpos.e_line,
								rve->right.rve->fpos.e_col);
						rval_cache_clean(&c1);
						goto error;
					}
					ret=rval_str_add2(h, msg, rv1, &c1, rv2, 0);
					break;
				default:
					BUG("rv unsupported basic type %d (%d,%d-%d,%d)\n", type,
							rve->fpos.s_line, rve->fpos.s_col,
							rve->fpos.e_line, rve->fpos.e_col);
			}
			rval_cache_clean(&c1);
			break;
		case RVE_CONCAT_OP:
			rv1=rval_expr_eval(h, msg, rve->left.rve);
			if (unlikely(rv1==0)){
				ERR("rval expression evaluation failed (%d,%d-%d,%d)\n",
						rve->left.rve->fpos.s_line, rve->left.rve->fpos.s_col,
						rve->left.rve->fpos.e_line, rve->left.rve->fpos.e_col);
				goto error;
			}
			rv2=rval_expr_eval(h, msg, rve->right.rve);
			if (unlikely(rv2==0)){
				ERR("rval expression evaluation failed (%d,%d-%d,%d)\n",
						rve->right.rve->fpos.s_line,
						rve->right.rve->fpos.s_col,
						rve->right.rve->fpos.e_line,
						rve->right.rve->fpos.e_col);
				goto error;
			}
			ret=rval_str_add2(h, msg, rv1, 0, rv2, 0);
			break;
		case RVE_STR_OP:
			rv1=rval_expr_eval(h, msg, rve->left.rve);
			if (unlikely(rv1==0)){
				ERR("rval expression evaluation failed (%d,%d-%d,%d)\n",
						rve->left.rve->fpos.s_line, rve->left.rve->fpos.s_col,
						rve->left.rve->fpos.e_line, rve->left.rve->fpos.e_col);
				goto error;
			}
			ret=rval_convert(h, msg, RV_STR, rv1, 0);
			break;
		case RVE_NONE_OP:
		/*default:*/
			BUG("invalid rval expression operation %d (%d,%d-%d,%d)\n",
					rve->op, rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col);
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



/** evals a rval expr and always returns a new rval.
 * like rval_expr_eval, but always returns a new rvalue (never a reference
 * to an exisiting one).
 * WARNING: result must be rval_destroy()'ed if non-null (it might be
 * a reference to another rval). The result can be modified only
 * if rv_chg_in_place() returns true.
 * @result rvalue on success, 0 on error
 */
struct rvalue* rval_expr_eval_new(struct run_act_ctx* h, struct sip_msg* msg,
								struct rval_expr* rve)
{
	struct rvalue* ret;
	struct rvalue* rv;
	
	ret=rval_expr_eval(h, msg, rve);
	if (ret && !rv_chg_in_place(ret)){
		rv=ret;
		/* create a new rv */
		ret=rval_new(rv->type, &rv->v, 0);
		rval_destroy(rv);
	}
	return ret;
}



/** create a RVE_RVAL_OP rval_expr, containing a single rval of the given type.
 *
 * @param rv_type - rval type
 * @param val     - rval value
 * @param pos     - config position
 * @return new pkg_malloc'ed rval_expr or 0 on error.
 */
struct rval_expr* mk_rval_expr_v(enum rval_type rv_type, void* val, 
									struct cfg_pos* pos)
{
	struct rval_expr* rve;
	union rval_val v;
	str* s;
	int flags;
	
	rve=pkg_malloc(sizeof(*rve));
	if (rve==0) 
		return 0;
	memset(rve, 0, sizeof(*rve));
	flags=0;
	switch(rv_type){
		case RV_INT:
			v.l=(long)val;
			break;
		case RV_STR:
			s=(str*)val;
			v.s.s=pkg_malloc(s->len+1 /*0*/);
			if (v.s.s==0){
				pkg_free(rve);
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
	if (pos) rve->fpos=*pos;
	return rve;
}



/**
 * @brief Create a unary op. rval_expr
 * ret= op rve1
 * @param op   - rval expr. unary operator
 * @param rve1 - rval expr. on which the operator will act.
 * @param pos configuration position
 * @return new pkg_malloc'ed rval_expr or 0 on error.
 */
struct rval_expr* mk_rval_expr1(enum rval_expr_op op, struct rval_expr* rve1,
								struct cfg_pos* pos)
{
	struct rval_expr* ret;
	
	switch(op){
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_BNOT_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
		case RVE_STR_OP:
			break;
		default:
			BUG("unsupported unary operator %d\n", op);
			return 0;
	}
	ret=pkg_malloc(sizeof(*ret));
	if (ret==0) 
		return 0;
	memset(ret, 0, sizeof(*ret));
	ret->op=op;
	ret->left.rve=rve1;
	if (pos) ret->fpos=*pos;
	return ret;
}



/**
 * @brief Create a rval_expr. from 2 other rval exprs, using op
 * ret = rve1 op rve2
 * @param op   - rval expr. operator
 * @param rve1 - rval expr. on which the operator will act.
 * @param rve2 - rval expr. on which the operator will act.
 * @param pos configuration position
 * @return new pkg_malloc'ed rval_expr or 0 on error.
 */
struct rval_expr* mk_rval_expr2(enum rval_expr_op op, struct rval_expr* rve1,
													  struct rval_expr* rve2,
													  struct cfg_pos* pos)
{
	struct rval_expr* ret;
	
	switch(op){
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_MINUS_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_PLUS_OP:
		case RVE_IPLUS_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
		case RVE_CONCAT_OP:
			break;
		default:
			BUG("unsupported operator %d\n", op);
			return 0;
	}
	ret=pkg_malloc(sizeof(*ret));
	if (ret==0) 
		return 0;
	memset(ret, 0, sizeof(*ret));
	ret->op=op;
	ret->left.rve=rve1;
	ret->right.rve=rve2;
	if (pos) ret->fpos=*pos;
	return ret;
}



/** returns true if the operator is associative. */
static int rve_op_is_assoc(enum rval_expr_op op)
{
	switch(op){
		case RVE_NONE_OP:
		case RVE_RVAL_OP:
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_BNOT_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
		case RVE_STR_OP:
			/* one operand expression => cannot be assoc. */
			return 0;
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_MINUS_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
			return 0;
		case RVE_PLUS_OP:
			/* the generic plus is not assoc, e.g.
			   "a" + 1 + "2" => "a12" in one case and "a3" in the other */
			return 0;
		case RVE_IPLUS_OP:
		case RVE_CONCAT_OP:
		case RVE_MUL_OP:
		case RVE_BAND_OP:
		case RVE_BOR_OP:
		case RVE_BXOR_OP:
			return 1;
		case RVE_LAND_OP:
		case RVE_LOR_OP:
			return 1;
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_MATCH_OP:
			return 0;
	}
	return 0;
}



/** returns true if the operator is commutative. */
static int rve_op_is_commutative(enum rval_expr_op op)
{
	switch(op){
		case RVE_NONE_OP:
		case RVE_RVAL_OP:
		case RVE_UMINUS_OP:
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_BNOT_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
		case RVE_STR_OP:
			/* one operand expression => cannot be commut. */
			return 0;
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_MINUS_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
			return 0;
		case RVE_PLUS_OP:
			/* non commut. when diff. type 
			   (e.g 1 + "2" != "2" + 1 ) => non commut. in general
			   (specific same type versions are covered by IPLUS & CONCAT) */
			return 0;
		case RVE_IPLUS_OP:
		case RVE_MUL_OP:
		case RVE_BAND_OP:
		case RVE_BOR_OP:
		case RVE_BXOR_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
			return 1;
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_CONCAT_OP:
		case RVE_MATCH_OP:
			return 0;
		case RVE_DIFF_OP:
		case RVE_EQ_OP:
			/* non. commut. in general, only for same type e.g.:
			   "" == 0  diff. 0 == "" ( "" == "0" and 0 == 0)
			   same type versions are covered by IEQ, IDIFF, STREQ, STRDIFF
			 */
			return 0 /* asymmetrical undef handling */;
	}
	return 0;
}


#if 0
/** returns true if the rval expr can be optimized to an int.
 *  (if left & right are leafs (RVE_RVAL_OP) and both of them are
 *   ints return true, else false)
 *  @return 0 or 1
 */
static int rve_can_optimize_int(struct rval_expr* rve)
{
	if (scr_opt_lev<1)
		return 0;
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
	LM_DBG("left %d, right %d\n", 
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
	if (scr_opt_lev<1)
		return 0;
	if (rve->op == RVE_RVAL_OP)
		return 0;
	LM_DBG("left %d, right %d\n", 
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
#endif



static int fix_rval(struct rvalue* rv)
{
	LM_DBG("RV fixing type %d\n", rv->type);
	switch(rv->type){
		case RV_INT:
			/*nothing to do*/
			LM_DBG("RV is int: %d\n", (int)rv->v.l);
			return 0;
		case RV_STR:
			/*nothing to do*/
			LM_DBG("RV is str: \"%s\"\n", rv->v.s.s);
			return 0;
		case RV_BEXPR:
			return fix_expr(rv->v.bexpr);
		case RV_ACTION_ST:
			return fix_actions(rv->v.action);
		case RV_SEL:
			if (resolve_select(&rv->v.sel)<0){
				ERR("Unable to resolve select\n");
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



/**
 * @brief Helper function: replace a rve (in-place) with a constant rval_val
 * @warning since it replaces in-place, one should make sure that if
 * rve is in fact a rval (rve->op==RVE_RVAL_OP), no reference is kept
 * to the rval!
 * @param rve expression to be replaced (in-place)
 * @param type rvalue type
 * @param v pointer to a rval_val union containing the replacement value.
 * @param flags value flags (how it was alloc'ed, e.g.: RV_CNT_ALLOCED_F)
 * @return 0 on success, -1 on error
 */
static int rve_replace_with_val(struct rval_expr* rve, enum rval_type type,
								union rval_val* v, int flags)
{
	int refcnt;
	
	refcnt=1; /* replaced-in-place rval refcnt */
	if (rve->op!=RVE_RVAL_OP){
		rve_destroy(rve->left.rve);
		if (rve_op_unary(rve->op)==0)
			rve_destroy(rve->right.rve);
	}else{
		if (rve->left.rval.refcnt!=1){
			BUG("trying to replace a referenced rval! (refcnt=%d)\n",
					rve->left.rval.refcnt);
			/* try to recover */
			refcnt=rve->left.rval.refcnt;
			abort(); /* find bugs quicker -- andrei */
		}
		rval_destroy(&rve->left.rval);
	}
	rval_init(&rve->left.rval, type, v, flags);
	rve->left.rval.refcnt=refcnt;
	rval_init(&rve->right.rval, RV_NONE, 0, 0);
	rve->op=RVE_RVAL_OP;
	return 0;
}



/** helper function: replace a rve (in-place) with a constant rvalue.
 * @param rve - expression to be replaced (in-place)
 * @param rv   - pointer to the replacement _constant_ rvalue structure
 * @return 0 on success, -1 on error */
static int rve_replace_with_ct_rv(struct rval_expr* rve, struct rvalue* rv)
{
	enum rval_type type;
	int flags;
	int i;
	union rval_val v;
	
	type=rv->type;
	flags=0;
	if (rv->type==RV_INT){
		if (rval_get_int(0, 0, &i, rv, 0)!=0){
			BUG("unexpected int evaluation failure (%d,%d-%d,%d)\n",
					rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col);
			return -1;
		}
		v.l=i;
	}else if(rv->type==RV_STR){
		if (rval_get_str(0, 0, &v.s, rv, 0)<0){
			BUG("unexpected str evaluation failure(%d,%d-%d,%d)\n",
					rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col);
			return -1;
		}
		flags|=RV_CNT_ALLOCED_F;
	}else{
		BUG("unknown constant expression type %d (%d,%d-%d,%d)\n", rv->type,
				rve->fpos.s_line, rve->fpos.s_col,
				rve->fpos.e_line, rve->fpos.e_col);
		return -1;
	}
	return rve_replace_with_val(rve, type, &v, flags);
}



/** try to replace the right side of the rve with a compiled regex.
  * @return 0 on success and -1 on error.
 */
static int fix_match_rve(struct rval_expr* rve)
{
	struct rvalue* rv;
	regex_t* re;
	union rval_val v;
	int flags;
	int ret;

	rv=0;
	v.s.s=0;
	v.re.regex=0;
	/* normal fix-up for the  left side */
	ret=fix_rval_expr((void*)rve->left.rve);
	if (ret<0) return ret;
	
	/* fixup the right side (RE) */
	if (rve_is_constant(rve->right.rve)){
		if ((rve_guess_type(rve->right.rve)!=RV_STR)){
			ERR("fixup failure(%d,%d-%d,%d): left side of  =~ is not string"
					" (%d,%d)\n",   rve->fpos.s_line, rve->fpos.s_col,
									rve->fpos.e_line, rve->fpos.e_col,
									rve->right.rve->fpos.s_line,
									rve->right.rve->fpos.s_col);
			goto error;
		}
		if ((rv=rval_expr_eval(0, 0, rve->right.rve))==0){
			ERR("fixup failure(%d,%d-%d,%d):  bad RE expression\n",
					rve->right.rve->fpos.s_line, rve->right.rve->fpos.s_col,
					rve->right.rve->fpos.e_line, rve->right.rve->fpos.e_col);
			goto error;
		}
		if (rval_get_str(0, 0, &v.s, rv, 0)<0){
			BUG("fixup unexpected failure (%d,%d-%d,%d)\n",
					rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col);
			goto error;
		}
		/* we have the str, we don't need the rv anymore */
		rval_destroy(rv);
		rv=0;
		re=pkg_malloc(sizeof(*re));
		if (re==0){
			ERR("out of memory\n");
			goto error;
		}
		/* same flags as for expr. =~ (fix_expr()) */
		if (regcomp(re, v.s.s, REG_EXTENDED|REG_NOSUB|REG_ICASE)){
			pkg_free(re);
			ERR("Bad regular expression \"%s\"(%d,%d-%d,%d)\n", v.s.s,
					rve->right.rve->fpos.s_line, rve->right.rve->fpos.s_col,
					rve->right.rve->fpos.e_line, rve->right.rve->fpos.e_col);
			goto error;
		}
		v.re.regex=re;
		flags=RV_RE_F|RV_RE_ALLOCED_F|RV_CNT_ALLOCED_F;
		if (rve_replace_with_val(rve->right.rve, RV_STR, &v, flags)<0)
			goto error;
	}else{
		/* right side is not constant => normal fixup */
		return fix_rval_expr((void*)rve->right.rve);
	}
	return 0;
error:
	if (rv) rval_destroy(rv);
	if (v.s.s) pkg_free(v.s.s);
	if (v.re.regex){
		regfree(v.re.regex);
		pkg_free(v.re.regex);
	}
	return -1;
}



/** optimize op($v, 0) or op($v, 1).
 * Note: internal use only from rve_optimize
 * It should be called after ct optimization, for non-contant
 *  expressions (the left or right side is not constant).
 * @return 1 on success (rve was changed), 0 on failure and -1 on error
 */
static int rve_opt_01(struct rval_expr* rve, enum rval_type rve_type)
{
	struct rvalue* rv;
	struct rval_expr* ct_rve;
	struct rval_expr* v_rve;
	int i;
	int ret;
	enum rval_expr_op op;
	struct cfg_pos pos;
	int right; /* debugging msg */
	int dbg; /* debugging msg on/off */

/* helper macro: replace in-place a <ctype> type rve with v (another rve).
 * if type_of(v)== <ctype> => rve:=*v (copy v contents into rve and free v)
 * else if type_of(v)!=<ctype> => rve:= (ctype) v (casts v to <ctype>)
 * Uses pos.
 * ctype can be INT or STR
 * WARNING: - v might be pkg_free()'d
 *          - rve members _are_ _not_ freed or destroyed
 */
#define replace_rve_type_cast(e, v, ctype) \
	do{\
		if ( rve_guess_type((v)) == RV_##ctype ){\
			/* if type_of($v)==int we don't need to add an \
			   int cast operator => replace with v */\
			pos=(e)->fpos; \
			*(e)=*(v); /* replace e with v (in-place) */ \
			(e)->fpos=pos; \
			pkg_free((v)); /* rve_destroy(v_rve) would free everything*/ \
		}else{\
			/* unknown type or str => (int) $v */ \
			(e)->op=RVE_##ctype##_OP; \
			(e)->left.rve=(v); \
			(e)->right.rve=0; \
		}\
	}while(0)
	
/* helper macro: replace in-place an int type rve with v (another rve).*/
#define replace_int_rve(e, v) replace_rve_type_cast(e, v, INT)
/* helper macro: replace in-place a str type rve with v (another rve).*/
#define replace_str_rve(e, v) replace_rve_type_cast(e, v, STR)
	
	
	rv=0;
	ret=0;
	right=0;
	dbg=1;
	
	if (rve_is_constant(rve->right.rve)){
		ct_rve=rve->right.rve;
		v_rve=rve->left.rve;
		right=1;
	}else if (rve_is_constant(rve->left.rve)){
		ct_rve=rve->left.rve;
		v_rve=rve->right.rve;
		right=0;
	}else
		return 0; /* op($v, $w) */
	
	/* rval_expr_eval_new() instead of rval_expr_eval() to avoid
	   referencing a ct_rve->left.rval if ct_rve is a rval, which
	   would prevent rve_destroy(ct_rve) from working */
	if ((rv=rval_expr_eval_new(0, 0, ct_rve))==0){
		ERR("optimization failure, bad expression (%d,%d-%d,%d)\n",
				ct_rve->fpos.s_line, ct_rve->fpos.s_col,
				ct_rve->fpos.e_line, ct_rve->fpos.e_col);
		goto error;
	}
	op=rve->op;
	if (rv->type==RV_INT){
		i=rv->v.l;
		switch(op){
			case RVE_MUL_OP:
				if (i==0){
					/* $v *  0 -> 0
					 *  0 * $v -> 0 */
					if (rve_replace_with_ct_rv(rve, rv)<0)
						goto error;
					ret=1;
				}else if (i==1){
					/* $v *  1 -> (int)$v
					 *  1 * $v -> (int)$v */
					rve_destroy(ct_rve);
					replace_int_rve(rve, v_rve);
					ret=1;
				}
				break;
			case RVE_DIV_OP:
				if (i==0){
					if (ct_rve==rve->left.rve){
						/* 0 / $v -> 0 */
						if (rve_replace_with_ct_rv(rve, rv)<0)
							goto error;
						ret=1;
					}else{
						/* $v / 0 */
						ERR("RVE divide by 0 at %d,%d\n",
								ct_rve->fpos.s_line, ct_rve->fpos.s_col);
					}
				}else if (i==1){
					if (ct_rve==rve->right.rve){
						/* $v / 1 -> (int)$v */
						rve_destroy(ct_rve);
						replace_int_rve(rve, v_rve);
						ret=1;
					}
				}
				break;
			case RVE_MOD_OP:
				if (i==0){
					if (ct_rve==rve->left.rve){
						/* 0 % $v -> 0 */
						if (rve_replace_with_ct_rv(rve, rv)<0)
							goto error;
						ret=1;
					}else{
						/* $v % 0 */
						ERR("RVE modulo by 0 at %d,%d\n",
								ct_rve->fpos.s_line, ct_rve->fpos.s_col);
					}
				}
				/* $v % 1 -> 0 ? */
				break;
			case RVE_MINUS_OP:
				if (i==0){
					if (ct_rve==rve->right.rve){
						/* $v - 0 -> $v */
						rve_destroy(ct_rve);
						replace_int_rve(rve, v_rve);
						ret=1;
					}
					/* ? 0 - $v -> -($v)  ? */
				}
				break;
			case RVE_BAND_OP:
				if (i==0){
					/* $v &  0 -> 0
					 *  0 & $v -> 0 */
					if (rve_replace_with_ct_rv(rve, rv)<0)
						goto error;
					ret=1;
				}
				/* no 0xffffff optimization for now (haven't decided on
				   the number of bits ) */
				break;
			case RVE_BOR_OP:
				if (i==0){
					/* $v |  0 -> (int)$v
					 *  0 | $v -> (int)$v */
					rve_destroy(ct_rve);
					replace_int_rve(rve, v_rve);
					ret=1;
				}
				break;
			case RVE_LAND_OP:
				if (i==0){
					/* $v &&  0 -> 0
					 *  0 && $v -> 0 */
					if (rve_replace_with_ct_rv(rve, rv)<0)
						goto error;
					ret=1;
				}else if (i==1){
					/* $v &&  1 -> (int)$v
					 *  1 && $v -> (int)$v */
					rve_destroy(ct_rve);
					replace_int_rve(rve, v_rve);
					ret=1;
				}
				break;
			case RVE_LOR_OP:
				if (i==1){
					/* $v ||  1 -> 1
					 *  1 || $v -> 1 */
					if (rve_replace_with_ct_rv(rve, rv)<0)
						goto error;
					ret=1;
				}else if (i==0){
					/* $v ||  0 -> (int)$v
					 *  0 && $v -> (int)$v */
					rve_destroy(ct_rve);
					replace_int_rve(rve, v_rve);
					ret=1;
				}
				break;
			case RVE_PLUS_OP:
			case RVE_IPLUS_OP:
				/* we must make sure that this is an int PLUS
				   (because "foo"+0 is valid => "foo0") =>
				   check if it's an IPLUS or the result is an integer
				   (which generally means unoptimized <int> + <something>).
				 */
				if ((i==0) && ((op==RVE_IPLUS_OP) || (rve_type==RV_INT))){
					/* $v +  0 -> (int)$v
					 *  0 + $v -> (int)$v */
					rve_destroy(ct_rve);
					replace_int_rve(rve, v_rve);
					ret=1;
				}
				break;
			default:
				/* do nothing */
				break;
		}
		/* debugging messages */
		if (ret==1 && dbg){
			if (right){
				if (rve->op==RVE_RVAL_OP){
					if (rve->left.rval.type==RV_INT)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, %d) -> %d\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, i, (int)rve->left.rval.v.l);
					else
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, %d) -> $v (rval)\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, i);
				}else if (rve->op==RVE_INT_OP){
					if (rve->left.rve->op==RVE_RVAL_OP &&
							rve->left.rve->left.rval.type==RV_INT)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, %d) -> (int)%d\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, i, (int)rve->left.rve->left.rval.v.l);
					else
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, %d) -> (int)$v\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, i);
				}else{
					LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
							" op%d($v, %d) -> $v\n",
							rve->fpos.s_line, rve->fpos.s_col,
							rve->fpos.e_line, rve->fpos.e_col,
							op, i);
				}
			}else{
				if (rve->op==RVE_RVAL_OP){
					if (rve->left.rval.type==RV_INT)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d(%d, $v) -> %d\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, i, (int)rve->left.rval.v.l);
					else
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d(%d, $v) -> $v (rval)\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, i);
				}else if (rve->op==RVE_INT_OP){
					if (rve->left.rve->op==RVE_RVAL_OP &&
							rve->left.rve->left.rval.type==RV_INT)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d(%d, $v) -> (int)%d\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, i, (int)rve->left.rve->left.rval.v.l);
					else
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d(%d, $v) -> (int)$v\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, i);
				}else{
					LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
							" op%d(%d, $v) -> $v\n",
							rve->fpos.s_line, rve->fpos.s_col,
							rve->fpos.e_line, rve->fpos.e_col,
							op, i);
				}
			}
		}
	}else if (rv->type==RV_STR){
		switch(op){
			case RVE_CONCAT_OP:
				if (rv->v.s.len==0){
					/* $v . "" -> (str)$v
					   "" . $v -> (str)$v */
					rve_destroy(ct_rve);
					replace_str_rve(rve, v_rve);
					ret=1;
				}
				break;
			case RVE_EQ_OP:
			case RVE_STREQ_OP:
				if (rv->v.s.len==0){
					/* $v == "" -> strempty($v) 
					   "" == $v -> strempty ($v) */
					rve_destroy(ct_rve);
					/* replace current expr. with strempty(rve) */
					rve->op=RVE_STREMPTY_OP;
					rve->left.rve=v_rve;
					rve->right.rve=0;
					ret=1;
					if (dbg)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, \"\") -> strempty($v)\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op);
					dbg=0;
				}
				break;
			default:
				break;
		}
	/* no optimization for generic RVE_PLUS_OP for now, only for RVE_CONCAT_OP
	   (RVE_PLUS_OP should be converted to RVE_CONCAT_OP if it's supposed
	    to work on strings. If it's not converted/optimized it means it's type
	    can be determined only at runtime => we cannot optimize */
		
		/* debugging messages */
		if (ret==1 && dbg){
			if (right){
				if (rve->op==RVE_RVAL_OP){
					if (rve->left.rval.type==RV_STR)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, <string>) -> \"%s\"\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, rve->left.rval.v.s.s);
					else
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, <string>) -> $v (rval)\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col, op);
				}else if (rve->op==RVE_STR_OP){
					if (rve->left.rve->op==RVE_RVAL_OP &&
							rve->left.rve->left.rval.type==RV_STR)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, <string>) -> (str)\"%s\"\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, rve->left.rve->left.rval.v.s.s);
					else
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d($v, <string>) -> (str)$v\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col, op);
				}else{
					LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
							" op%d($v, <string>) -> $v\n",
							rve->fpos.s_line, rve->fpos.s_col,
							rve->fpos.e_line, rve->fpos.e_col, op);
				}
			}else{
				if (rve->op==RVE_RVAL_OP){
					if (rve->left.rval.type==RV_STR)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d(<string>, $v) -> \"%s\"\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, rve->left.rval.v.s.s);
					else
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d(<string>, $v) -> $v (rval)\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col, op);
				}else if (rve->op==RVE_STR_OP){
					if (rve->left.rve->op==RVE_RVAL_OP &&
							rve->left.rve->left.rval.type==RV_STR)
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d(<string>, $v) -> (str)\"%s\"\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								op, rve->left.rve->left.rval.v.s.s);
					else
						LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
								" op%d(<string>, $v) -> (str)$v\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col, op);
				}else{
					LM_DBG("FIXUP RVE: (%d,%d-%d,%d) optimized"
							" op%d(<string>, $v) -> $v\n",
							rve->fpos.s_line, rve->fpos.s_col,
							rve->fpos.e_line, rve->fpos.e_col, op);
				}
			}
		}
	}
	if (rv) rval_destroy(rv);
	return ret;
error:
	if (rv) rval_destroy(rv);
	return -1;
}



/** tries to optimize a rval_expr. */
static int rve_optimize(struct rval_expr* rve)
{
	int ret;
	struct rvalue* rv;
	struct rvalue* trv; /* used only for DBG() */
	enum rval_expr_op op;
	struct rval_expr tmp_rve;
	enum rval_type type, l_type;
	struct rval_expr* bad_rve;
	enum rval_type bad_type, exp_type;
	
	ret=0;
	rv=0;
	if (scr_opt_lev<1)
		return 0;
	if (rve->op == RVE_RVAL_OP) /* if rval, nothing to do */
		return 0;
	if (rve_is_constant(rve)){
		if ((rv=rval_expr_eval_new(0, 0, rve))==0){
			ERR("optimization failure, bad expression (%d,%d-%d,%d)\n",
					rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col);
			goto error;
		}
		op=rve->op;
		if (rve_replace_with_ct_rv(rve, rv)<0)
			goto error;
		rval_destroy(rv);
		rv=0;
		trv=&rve->left.rval;
		if (trv->type==RV_INT)
			LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized constant int rve "
					"(old op %d) to %d\n",
					rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col,
					op, (int)trv->v.l);
		else if (trv->type==RV_STR)
			LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized constant str rve "
					"(old op %d) to \"%.*s\"\n",
					rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col,
					op, trv->v.s.len, trv->v.s.s);
		ret=1;
	}else{
		/* expression is not constant */
		/* if unary => nothing to do */
		if (rve_op_unary(rve->op))
			return rve_optimize(rve->left.rve);
		rve_optimize(rve->left.rve);
		rve_optimize(rve->right.rve);
		if (!rve_check_type(&type, rve, &bad_rve, &bad_type, &exp_type)){
			ERR("optimization failure while optimizing %d,%d-%d,%d:"
					" type mismatch in expression (%d,%d-%d,%d), "
					"type %s, but expected %s\n",
					rve->fpos.s_line, rve->fpos.s_col,
					rve->fpos.e_line, rve->fpos.e_col,
					bad_rve->fpos.s_line, bad_rve->fpos.s_col,
					bad_rve->fpos.e_line, bad_rve->fpos.e_col,
					rval_type_name(bad_type), rval_type_name(exp_type));
			return 0;
		}
		/* $v - a => $v + (-a)  (easier to optimize)*/
		if ((rve->op==RVE_MINUS_OP) && (rve_is_constant(rve->right.rve))){
			if ((rv=rval_expr_eval_new(0, 0, rve->right.rve))==0){
				ERR("optimization failure, bad expression (%d,%d-%d,%d)\n",
								rve->right.rve->fpos.s_line,
								rve->right.rve->fpos.s_col,
								rve->right.rve->fpos.e_line,
								rve->right.rve->fpos.e_col);
				goto error;
			}
			if (rv->type==RV_INT){
				rv->v.l=-rv->v.l;
				if (rve_replace_with_ct_rv(rve->right.rve, rv)<0)
					goto error;
				rve->op=RVE_IPLUS_OP;
				LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized $v - a into "
						"$v + (%d)\n",
						rve->fpos.s_line, rve->fpos.s_col,
						rve->fpos.e_line, rve->fpos.e_col,
						(int)rve->right.rve->left.rval.v.l);
			}
			rval_destroy(rv);
			rv=0;
		}
		
		/* e1 PLUS_OP e2 -> change op if we know e1 basic type */
		if (rve->op==RVE_PLUS_OP){
			l_type=rve_guess_type(rve->left.rve);
			if (l_type==RV_INT){
				rve->op=RVE_IPLUS_OP;
				LM_DBG("FIXUP RVE (%d,%d-%d,%d): changed + into integer plus\n",
						rve->fpos.s_line, rve->fpos.s_col,
						rve->fpos.e_line, rve->fpos.e_col);
			}else if (l_type==RV_STR){
				rve->op=RVE_CONCAT_OP;
				LM_DBG("FIXUP RVE (%d,%d-%d,%d): changed + into string concat\n",
						rve->fpos.s_line, rve->fpos.s_col,
						rve->fpos.e_line, rve->fpos.e_col);
			}
		}
		/* e1 EQ_OP e2 -> change op if we know e1 basic type
		   e1 DIFF_OP e2 -> change op if we know e2 basic type */
		if (rve->op==RVE_EQ_OP || rve->op==RVE_DIFF_OP){
			l_type=rve_guess_type(rve->left.rve);
			if (l_type==RV_INT){
				rve->op=(rve->op==RVE_EQ_OP)?RVE_IEQ_OP:RVE_IDIFF_OP;
				LM_DBG("FIXUP RVE (%d,%d-%d,%d): changed ==/!= into integer"
						" ==/!=\n",
						rve->fpos.s_line, rve->fpos.s_col,
						rve->fpos.e_line, rve->fpos.e_col);
			}else if (l_type==RV_STR){
				rve->op=(rve->op==RVE_EQ_OP)?RVE_STREQ_OP:RVE_STRDIFF_OP;
				LM_DBG("FIXUP RVE (%d,%d-%d,%d): changed ==/!= into string"
						" ==/!=\n",
						rve->fpos.s_line, rve->fpos.s_col,
						rve->fpos.e_line, rve->fpos.e_col);
			}
		}
		
		/* $v * 0 => 0; $v * 1 => $v (for *, /, &, |, &&, ||, +, -) */
		if (rve_opt_01(rve, type)==1){
			/* success, rve was changed => return now
			  (since this is recursively invoked the "new" rve
			   is already optimized) */
			ret=1;
			goto end;
		}
		
		/* op(op($v, a), b) => op($v, op(a,b)) */
		if (rve_is_constant(rve->right.rve)){
			/* op1(op2(...), b) */
			if ((rve->op==rve->left.rve->op) && rve_op_is_assoc(rve->op)){
				/* op(op(...), b) */
				if (rve_is_constant(rve->left.rve->right.rve)){
					/* op(op($v, a), b) => op($v, op(a, b)) */
					/* rv= op(a, b) */
					tmp_rve.op=rve->op;
					tmp_rve.left.rve=rve->left.rve->right.rve;
					tmp_rve.right.rve=rve->right.rve;
					/* hack for RVE_PLUS_OP which can work on string, ints
					   or a combination of them */
					if ((rve->op==RVE_PLUS_OP) &&
						(rve_guess_type(tmp_rve.left.rve)!=RV_STR)){
						LM_DBG("RVE optimization failed (%d,%d-%d,%d): cannot "
							"optimize +(+($v, a), b) when typeof(a)==INT\n",
							rve->fpos.s_line, rve->fpos.s_col,
							rve->fpos.e_line, rve->fpos.e_col);
						return 0;
					}
					if ((rv=rval_expr_eval_new(0, 0, &tmp_rve))==0){
						ERR("optimization failure, bad expression\n");
						goto error;
					}
					/* op($v, rv) */
					if (rve_replace_with_ct_rv(rve->right.rve, rv)<0)
						goto error;
					rval_destroy(rv);
					rv=0;
					rve_destroy(tmp_rve.left.rve);
					rve->left.rve=rve->left.rve->left.rve;
					trv=&rve->right.rve->left.rval;
					if (trv->type==RV_INT)
						LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized int rve: "
								"op(op($v, a), b) with op($v, %d); op=%d\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								(int)trv->v.l, rve->op);
					else if (trv->type==RV_STR)
						LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized str rve "
								"op(op($v, a), b) with op($v, \"%.*s\");"
								" op=%d\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								trv->v.s.len, trv->v.s.s, rve->op);
					ret=1;
				}else if (rve_is_constant(rve->left.rve->left.rve) &&
							rve_op_is_commutative(rve->op)){
					/* op(op(a, $v), b) => op(op(a, b), $v) */
					/* rv= op(a, b) */
					tmp_rve.op=rve->op;
					tmp_rve.left.rve=rve->left.rve->left.rve;
					tmp_rve.right.rve=rve->right.rve;
					/* no need for the RVE_PLUS_OP hack, all the bad
					   cases are caught by rve_op_is_commutative()
					   (in this case type will be typeof(a)) => ok only if
					   typeof(a) is int) */
					if ((rv=rval_expr_eval_new(0, 0, &tmp_rve))==0){
						ERR("optimization failure, bad expression\n");
						goto error;
					}
					/* op(rv, $v) */
					rve_destroy(rve->right.rve);
					rve->right.rve=rve->left.rve->right.rve;
					rve->left.rve->right.rve=0;
					if (rve_replace_with_ct_rv(rve->left.rve, rv)<0)
						goto error;
					rval_destroy(rv);
					rv=0;
					trv=&rve->left.rve->left.rval;
					if (trv->type==RV_INT)
						LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized int rve: "
								"op(op(a, $v), b) with op(%d, $v); op=%d\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								(int)trv->v.l, rve->op);
					else if (trv->type==RV_STR)
						LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized str rve "
								"op(op(a, $v), b) with op(\"%.*s\", $v);"
								" op=%d\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								trv->v.s.len, trv->v.s.s, rve->op);
					ret=1;
				}
				/* op(op($v, $w),b) => can't optimize */
			}
			/* op1(op2(...), b) and op1!=op2 or op is non assoc.
			   => can't optimize */
		}else if (rve_is_constant(rve->left.rve)){
			/* op1(a, op2(...)) */
			if ((rve->op==rve->right.rve->op) && rve_op_is_assoc(rve->op)){
				/* op(a, op(...)) */
				if (rve_is_constant(rve->right.rve->right.rve) &&
						rve_op_is_commutative(rve->op)){
					/* op(a, op($v, b)) => op(op(a, b), $v) */
					/* rv= op(a, b) */
					tmp_rve.op=rve->op;
					tmp_rve.left.rve=rve->left.rve;
					tmp_rve.right.rve=rve->right.rve->right.rve;
					/* no need for the RVE_PLUS_OP hack, all the bad
					   cases are caught by rve_op_is_commutative()
					   (in this case type will be typeof(a)) => ok only if
					   typeof(a) is int) */
					if ((rv=rval_expr_eval_new(0, 0, &tmp_rve))==0){
						ERR("optimization failure, bad expression\n");
						goto error;
					}
					/* op(rv, $v) */
					if (rve_replace_with_ct_rv(rve->left.rve, rv)<0)
						goto error;
					rval_destroy(rv);
					rv=0;
					rve_destroy(tmp_rve.right.rve);
					rve->right.rve=rve->right.rve->left.rve;
					trv=&rve->left.rve->left.rval;
					if (trv->type==RV_INT)
						LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized int rve: "
								"op(a, op($v, b)) with op(%d, $v); op=%d\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								(int)trv->v.l, rve->op);
					else if (trv->type==RV_STR)
						LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized str rve "
								"op(a, op($v, b)) with op(\"%.*s\", $v);"
								" op=%d\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								trv->v.s.len, trv->v.s.s, rve->op);
					ret=1;
				}else if (rve_is_constant(rve->right.rve->left.rve)){
					/* op(a, op(b, $v)) => op(op(a, b), $v) */
					/* rv= op(a, b) */
					tmp_rve.op=rve->op;
					tmp_rve.left.rve=rve->left.rve;
					tmp_rve.right.rve=rve->right.rve->left.rve;
					/* hack for RVE_PLUS_OP which can work on string, ints
					   or a combination of them */
					if ((rve->op==RVE_PLUS_OP) &&
						(rve_guess_type(tmp_rve.left.rve) != 
						 	rve_guess_type(tmp_rve.right.rve))){
						LM_DBG("RVE optimization failed (%d,%d-%d,%d): cannot "
								"optimize +(a, +(b, $v)) when "
								"typeof(a)!=typeof(b)\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col);
						return 0;
					}
					if ((rv=rval_expr_eval_new(0, 0, &tmp_rve))==0){
						ERR("optimization failure, bad expression\n");
						goto error;
					}
					/* op(rv, $v) */
					if (rve_replace_with_ct_rv(rve->left.rve, rv)<0)
						goto error;
					rval_destroy(rv);
					rv=0;
					rve_destroy(tmp_rve.right.rve);
					rve->right.rve=rve->right.rve->right.rve;
					trv=&rve->left.rve->left.rval;
					if (trv->type==RV_INT)
						LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized int rve: "
								"op(a, op(b, $v)) with op(%d, $v); op=%d\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								(int)trv->v.l, rve->op);
					else if (trv->type==RV_STR)
						LM_DBG("FIXUP RVE (%d,%d-%d,%d): optimized str rve "
								"op(a, op(b, $v)) with op(\"%.*s\", $v);"
								" op=%d\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col,
								trv->v.s.len, trv->v.s.s, rve->op);
					ret=1;
				}
				/* op(a, op($v, $w)) => can't optimize */
			}
			/* op1(a, op2(...)) and op1!=op2 or op is non assoc.
			   => can't optimize */
		}
		/* op(op($v,a), op($w,b)) => no optimizations for now (TODO) */
	}
end:
	return ret;
error:
	if (rv) rval_destroy(rv);
	return -1;
}



/** fix a rval_expr.
 * fixes action, bexprs, resolves selects, pvars and
 * optimizes simple sub expressions (e.g. 1+2).
 *
 * @param p - pointer to a rval_expr
 * @return 0 on success, <0 on error (modifies also *(struct rval_expr*)p)
 */
int fix_rval_expr(void* p)
{
	struct rval_expr* rve;
	int ret;
	
	rve=(struct rval_expr*)p;
	
	switch(rve->op){
		case RVE_NONE_OP:
			BUG("empty rval expr\n");
			break;
		case RVE_RVAL_OP:
			return fix_rval(&rve->left.rval);
		case RVE_UMINUS_OP: /* unary operators */
		case RVE_BOOL_OP:
		case RVE_LNOT_OP:
		case RVE_BNOT_OP:
		case RVE_STRLEN_OP:
		case RVE_STREMPTY_OP:
		case RVE_DEFINED_OP:
		case RVE_NOTDEFINED_OP:
		case RVE_INT_OP:
		case RVE_STR_OP:
			ret=fix_rval_expr((void*)rve->left.rve);
			if (ret<0) return ret;
			break;
		case RVE_MUL_OP:
		case RVE_DIV_OP:
		case RVE_MOD_OP:
		case RVE_MINUS_OP:
		case RVE_BOR_OP:
		case RVE_BAND_OP:
		case RVE_BXOR_OP:
		case RVE_BLSHIFT_OP:
		case RVE_BRSHIFT_OP:
		case RVE_LAND_OP:
		case RVE_LOR_OP:
		case RVE_GT_OP:
		case RVE_GTE_OP:
		case RVE_LT_OP:
		case RVE_LTE_OP:
		case RVE_PLUS_OP:
		case RVE_IPLUS_OP:
		case RVE_EQ_OP:
		case RVE_DIFF_OP:
		case RVE_IEQ_OP:
		case RVE_IDIFF_OP:
		case RVE_STREQ_OP:
		case RVE_STRDIFF_OP:
		case RVE_CONCAT_OP:
			ret=fix_rval_expr((void*)rve->left.rve);
			if (ret<0) return ret;
			ret=fix_rval_expr((void*)rve->right.rve);
			if (ret<0) return ret;
			break;
		case RVE_MATCH_OP:
			ret=fix_match_rve(rve);
			if (ret<0) return ret;
			break;
		default:
			BUG("unsupported op type %d\n", rve->op);
	}
	/* try to optimize */
	rve_optimize(rve);
	return 0;
}
