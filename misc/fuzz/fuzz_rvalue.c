/*
 * Fuzz the script expression evaluator (src/core/rvalue.c, 0/2966 in
 * production coverage): the fuzz bytes drive construction of a small
 * rval_expr tree (constant long/string leaves, unary/binary operators),
 * which is then evaluated through the real evaluator used by every
 * kamailio.cfg assignment/condition, and freed through the project's own
 * rve_destroy(). The evaluator context and message are NULL on purpose:
 * constant trees must never dereference them.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose is freely granted, provided this notice is preserved.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"

#include "../mem/mem.h"
#include "../rvalue.h"

static const enum rval_expr_op kOps[] = {
	/* leaf */
	RVE_RVAL_OP,
	/* unary */
	RVE_UMINUS_OP, RVE_BOOL_OP, RVE_LNOT_OP, RVE_BNOT_OP,
	RVE_STRLEN_OP, RVE_STREMPTY_OP,
	RVE_DEFINED_OP, RVE_NOTDEFINED_OP, RVE_LONG_OP, RVE_STR_OP,
	/* int binary */
	RVE_MUL_OP, RVE_DIV_OP, RVE_MOD_OP, RVE_MINUS_OP,
	RVE_BAND_OP, RVE_BOR_OP, RVE_BXOR_OP,
	RVE_BLSHIFT_OP, RVE_BRSHIFT_OP,
	RVE_LAND_OP, RVE_LOR_OP,
	RVE_GT_OP, RVE_GTE_OP, RVE_LT_OP, RVE_LTE_OP,
	RVE_IEQ_OP, RVE_IDIFF_OP, RVE_IPLUS_OP,
	/* int/str common + string */
	RVE_PLUS_OP, RVE_EQ_OP, RVE_DIFF_OP,
	RVE_CONCAT_OP, RVE_STREQ_OP, RVE_STRDIFF_OP, RVE_MATCH_OP,
};

struct rvbuild {
	const uint8_t *d;
	size_t n;
	size_t i;
	int budget;
};

static uint8_t nxt(struct rvbuild *b)
{
	uint8_t v;

	if(b->i >= b->n) {
		return 0;
	}
	v = b->d[b->i];
	b->i++;
	return v;
}

static struct rval_expr *rve_build(struct rvbuild *b, int depth)
{
	struct rval_expr *rve;
	enum rval_expr_op op;
	char *tmp;
	int len;
	int i;

	if(depth <= 0 || b->budget <= 0) {
		op = RVE_RVAL_OP;
	} else {
		op = kOps[nxt(b) % (sizeof(kOps) / sizeof(kOps[0]))];
	}

	rve = (struct rval_expr *)pkg_malloc(sizeof(struct rval_expr));
	if(rve == NULL) {
		return NULL;
	}
	memset(rve, 0, sizeof(struct rval_expr));
	rve->op = op;
	rve->fpos.s_line = 1;
	rve->fpos.e_line = 1;
	b->budget--;

	if(op == RVE_RVAL_OP) {
		if(nxt(b) & 1) {
			rve->left.rval.type = RV_LONG;
			rve->left.rval.refcnt = 1;
			for(i = 0; i < 8; i++) {
				rve->left.rval.v.l |= (long)nxt(b) << (8 * i);
			}
		} else {
			/* string leaf, built the way the grammar does it: the
			 * content is its own allocation (flagged for free in
			 * rval_force_clean), the rval shell is embedded in the
			 * rve (no RV_RV_ALLOCED_F), and the string is
			 * NUL-terminated as every kamailio cfg string is. */
			len = nxt(b) % 33;
			tmp = (char *)pkg_malloc(len + 1);
			if(tmp == NULL) {
				pkg_free(rve);
				return NULL;
			}
			for(i = 0; i < len; i++) {
				tmp[i] = (char)('a' + (nxt(b) % 26));
			}
			tmp[len] = '\0';
			rve->left.rval.type = RV_STR;
			rve->left.rval.refcnt = 1;
			rve->left.rval.flags = RV_CNT_ALLOCED_F;
			rve->left.rval.v.s.s = tmp;
			rve->left.rval.v.s.len = len;
		}
		return rve;
	}

	rve->left.rve = rve_build(b, depth - 1);
	if(rve->left.rve == NULL) {
		rve_destroy(rve);
		return NULL;
	}

	/* binary operands: every op but the unary list takes a right side */
	if(op != RVE_UMINUS_OP && op != RVE_BOOL_OP && op != RVE_LNOT_OP
			&& op != RVE_BNOT_OP && op != RVE_STRLEN_OP
			&& op != RVE_STREMPTY_OP && op != RVE_DEFINED_OP
			&& op != RVE_NOTDEFINED_OP && op != RVE_LONG_OP
			&& op != RVE_STR_OP) {
		rve->right.rve = rve_build(b, depth - 1);
		if(rve->right.rve == NULL) {
			rve->left.rve = NULL;
			rve_destroy(rve);
			return NULL;
		}
	}

	return rve;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct rvbuild b;
	struct rval_expr *rve;
	struct rvalue *res;
	long lres;
	int ires;

	if(size < 8 || size > 4096) {
		return 0;
	}

	b.d = data;
	b.n = size;
	b.i = 0;
	b.budget = 64;

	rve = rve_build(&b, 7);
	if(rve == NULL) {
		return 0;
	}

	/* generic evaluation */
	res = rval_expr_eval(NULL, NULL, rve);
	if(res != NULL) {
		rval_destroy(res);
	}

	/* typed evaluations */
	ires = rval_expr_eval_long(NULL, NULL, &lres, rve);
	(void)ires;

	rve_destroy(rve);
	return 0;
}
