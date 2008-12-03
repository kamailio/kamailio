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
 *  2008-11-30  initial version (andrei)
 */

#ifndef _rvalue_h_
#define _rvalue_h_

#include "str.h"
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
	RVE_NONE_OP,  /* uninit / empty */
	RVE_RVAL_OP,  /* special op, means that the expr. is in fact a rval */
	RVE_UMINUS_OP, /* one member expression, returns -(val) */
	RVE_BOOL_OP,  /* one member evaluate as bool. : (val!=0)*/
	RVE_LNOT_OP,  /* one member evaluate as bool. : (!val)*/
	RVE_MUL_OP,   /* 2 members, returns left * right */
	RVE_DIV_OP,   /* 2 memebers, returns left / right */
	RVE_MINUS_OP, /* 2 memebers, returns left - right */
	/* common int & str */
	RVE_PLUS_OP  /* 2 members, returns left + right */
	/* str only */
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
#define RV_CNT_ALLOCED_F  1  /* free contents  (pkg mem allocated) */
#define RV_RV_ALLOCED_F   2  /* free rv itself (pkg_free(rv)) */
#define RV_ALL_ALLOCED_F  (RV_CNT_ALLOCED|RV_RV_ALLOCED)

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
};


enum rval_cache_type{
	RV_CACHE_EMPTY,
	RV_CACHE_PVAR,
	RV_CACHE_AVP
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
};



/** allocates a new rval (should be freed by rval_destroy()). */
struct rvalue* rval_new_empty(int extra_size);
struct rvalue* rval_new_str(str* s, int extra_size);
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


/** convert a rvalue to another type.  */
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

/** evals an integer expr  to an int. */
int rval_expr_eval_int( struct run_act_ctx* h, struct sip_msg* msg,
						int* res, struct rval_expr* rve);
/** evals a rval expr.. */
struct rvalue* rval_expr_eval(struct run_act_ctx* h, struct sip_msg* msg,
								struct rval_expr* rve);


/** create a RVE_RVAL_OP rval_expr, containing a single rval of the given type.
  */
struct rval_expr* mk_rval_expr_v(enum rval_type rv_type, void* val);
/** create a unary op. rval_expr.. */
struct rval_expr* mk_rval_expr1(enum rval_expr_op op, struct rval_expr* rve1);
/** create a rval_expr. from 2 other rval exprs, using op. */
struct rval_expr* mk_rval_expr2(enum rval_expr_op op, struct rval_expr* rve1,
													  struct rval_expr* rve2);

/** fix a rval_expr. */
int fix_rval_expr(void** p);
#endif /* _rvalue_h */
