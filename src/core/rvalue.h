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
 * @author andrei
 */
 
#ifndef _rvalue_h_
#define _rvalue_h_

#include "str.h"
#include "ut.h"
#include "usr_avp.h"
#include "select.h"
#include "pvar.h"
#include "route.h"
#include "parser/msg_parser.h"
#include "action.h"

enum rval_type{
	RV_NONE, RV_INT, RV_STR, /* basic types */
	RV_BEXPR, RV_ACTION_ST,  /* special values */
	RV_SEL, RV_AVP, RV_PVAR
};

enum rval_expr_op{
	RVE_NONE_OP,  /**< uninit / empty */
	RVE_RVAL_OP,  /**< special op, means that the expr. is in fact a rval */
	RVE_UMINUS_OP, /**< one member expression, returns -(val) */
	RVE_BOOL_OP,  /**< one member evaluate as bool. : (val!=0)*/
	RVE_LNOT_OP,  /**< one member evaluate as bool. : (!val)*/
	RVE_BNOT_OP,  /**< one member evaluate as binary : (~ val)*/
	RVE_MUL_OP,   /**< 2 members, returns left * right */
	RVE_DIV_OP,   /**< 2 members, returns left / right */
	RVE_MOD_OP,   /**< 2 members, returns left % right */
	RVE_MINUS_OP, /**< 2 members, returns left - right */
	RVE_BAND_OP,  /**< 2 members, returns left | right */
	RVE_BOR_OP,   /**< 2 members, returns left & right */
	RVE_BXOR_OP,   /**< 2 members, returns left XOR right */
	RVE_BLSHIFT_OP, /**< 2 members, returns left << right */
	RVE_BRSHIFT_OP, /**< 2 members, returns left >> right */
	RVE_LAND_OP,  /**< 2 members, returns left && right */
	RVE_LOR_OP,   /**< 2 members, returns left || right */
	RVE_GT_OP,    /**<  2 members, returns left > right */
	RVE_GTE_OP,   /**<  2 members, returns left >= right */
	RVE_LT_OP,    /**<  2 members, returns left  < right */
	RVE_LTE_OP,   /**<  2 members, returns left <= right */
	RVE_IEQ_OP,   /**<  2 members, int == version, returns left == right */
	RVE_IDIFF_OP, /**< 2 members, int != version, returns left != right */
	RVE_IPLUS_OP, /**< 2 members, integer +, returns int(a)+int(b) */
	/* common int & str */
	RVE_PLUS_OP,  /**< generic plus (int or str) returns left + right */
	RVE_EQ_OP,    /**<  2 members, returns left == right  (int)*/
	RVE_DIFF_OP,  /**<  2 members, returns left != right  (int)*/
	/* str only */
	RVE_CONCAT_OP, /**< 2 members, string concat, returns left . right (str)*/
	RVE_STRLEN_OP, /**< one member, string length:, returns strlen(val) (int)*/
	RVE_STREMPTY_OP, /**< one member, returns val=="" (bool) */
	RVE_STREQ_OP,  /**< 2 members, string == , returns left == right (bool)*/
	RVE_STRDIFF_OP,/**< 2 members, string != , returns left != right (bool)*/
	RVE_MATCH_OP,  /**< 2 members, string ~),  returns left matches re(right) */
	/* avp, pvars a.s.o */
	RVE_DEFINED_OP, /**< one member, returns is_defined(val) (bool) */
	RVE_NOTDEFINED_OP, /**< one member, returns is_not_defined(val) (bool) */
	RVE_INT_OP,   /**< one member, returns (int)val  (int) */
	RVE_STR_OP    /**< one member, returns (str)val  (str) */
};


struct str_re{
	str s;
	regex_t* regex;
};

union rval_val{
	void* p;
	long  l;
	str s;
	avp_spec_t avps;
	select_t sel;
	pv_spec_t pvs;
	struct action* action;
	struct expr* bexpr;
	struct str_re re;
};


struct rvalue{
	enum rval_type type;
	int refcnt; /**< refcnt, on 0 the structure is destroyed */
	union rval_val v;
	int bsize; /**< extra data size */
	short flags;
	char buf[1]; /**< extra data, like string contents can be stored here */
};


/* rvalue flags */
#define RV_CNT_ALLOCED_F  1  /**< free contents  (pkg mem allocated) */
#define RV_RV_ALLOCED_F   2  /**< free rv itself (pkg_free(rv)) */
#define RV_ALL_ALLOCED_F  (RV_CNT_ALLOCED|RV_RV_ALLOCED)
#define RV_RE_F  4 /**< string is a RE with a valid v->re member */
#define RV_RE_ALLOCED_F 8 /**< v->re.regex must be freed */

struct rval_expr{
	enum rval_expr_op op;
	union{
		struct rval_expr* rve;
		struct rvalue rval;
	}left;
	union{
		struct rval_expr* rve;
		struct rvalue rval;
	}right;
	struct cfg_pos fpos;
};


enum rval_cache_type{
	RV_CACHE_EMPTY,
	RV_CACHE_PVAR,
	RV_CACHE_AVP,
	RV_CACHE_SELECT,
	RV_CACHE_INT2STR
};

/** value cache for a rvalue struct.
  * Used to optimize functions that would need to 
  * get the value repeatedly (e.g. rval_get_btype() and then rval_get_int())
  */
struct rval_cache{
	enum rval_cache_type cache_type;
	enum rval_type val_type;
	union{
		int_str avp_val; /**< avp value */
		pv_value_t pval; /**< pvar value */
	}c;
	char i2s[INT2STR_MAX_LEN]; /**< space for converting an int to string*/
};



/** allocates a new rval (should be freed by rval_destroy()). */
struct rvalue* rval_new_empty(int extra_size);
struct rvalue* rval_new_str(str* s, int extra_size);

/**
 * @brief create a new pk_malloc'ed rvalue from a rval_val union
 * @param t rvalue type
 * @param v rvalue value
 * @param extra_size extra space to allocate
 * (so that future string operation can reuse the space)
 * @return new rv or 0 on error
 */
struct rvalue* rval_new(enum rval_type t, union rval_val* v, int extra_size);

/** inits a rvalue structure- */
void rval_init(struct rvalue* rv, enum rval_type t, union rval_val* v,
					int flags);
/** frees a rval_new(), rval_convert() or rval_expr_eval() returned rval. */
void rval_destroy(struct rvalue* rv);

/** frees a rval contents */
void rval_clean(struct rvalue* rv);

/** init a rval_cache struct */
#define rval_cache_init(rvc) \
	do{ (rvc)->cache_type=RV_CACHE_EMPTY; (rvc)->val_type=RV_NONE; }while(0)

/** destroy a rval_cache struct contents */
void rval_cache_clean(struct rval_cache* rvc);


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
							struct rval_cache* c);

/** get the integer value of an rvalue. */
int rval_get_int(struct run_act_ctx* h, struct sip_msg* msg, int* i, 
				struct rvalue* rv, struct rval_cache* cache);
/** get the string value of an rv. */
int rval_get_str(struct run_act_ctx* h, struct sip_msg* msg,
								str* s, struct rvalue* rv,
								struct rval_cache* cache);
/** get the string value of an rv in a tmp variable */
int rval_get_tmp_str(struct run_act_ctx* h, struct sip_msg* msg,
								str* tmpv, struct rvalue* rv,
								struct rval_cache* cache,
								struct rval_cache* tmp_cache);

/** evals an integer expr  to an int. */
int rval_expr_eval_int( struct run_act_ctx* h, struct sip_msg* msg,
						int* res, struct rval_expr* rve);

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
								struct rval_expr* rve);

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
int rval_expr_eval_rvint( struct run_act_ctx* h, struct sip_msg* msg,
						 struct rvalue** rv_res, int* i_res,
						 struct rval_expr* rve, struct rval_cache* cache);


/** guess the type of an expression.  */
enum rval_type rve_guess_type(struct rval_expr* rve);
/** returns true if expression is constant. */
int rve_is_constant(struct rval_expr* rve);
/** returns true if the expression can have side-effect */
int rve_has_side_effects(struct rval_expr* rve);

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
					struct rval_expr** bad_rve, enum rval_type* bad_type,
					enum rval_type* exp_type);
/** returns a string name for type (debugging).*/
char* rval_type_name(enum rval_type type);

/** create a RVE_RVAL_OP rval_expr, containing a single rval of the given type
  */
struct rval_expr* mk_rval_expr_v(enum rval_type rv_type, void* val,
									struct cfg_pos* pos);

/**
 * @brief Create a unary op. rval_expr
 * ret= op rve1
 * @param op   - rval expr. unary operator
 * @param rve1 - rval expr. on which the operator will act.
 * @param pos configuration position
 * @return new pkg_malloc'ed rval_expr or 0 on error.
 */
struct rval_expr* mk_rval_expr1(enum rval_expr_op op, struct rval_expr* rve1,
									struct cfg_pos* pos);

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
													  struct cfg_pos* pos);
/** destroys a pkg_malloc'ed rve. */
void rve_destroy(struct rval_expr* rve);

/** fix a rval_expr. */
int fix_rval_expr(void* p);
#endif /* _rvalue_h */
