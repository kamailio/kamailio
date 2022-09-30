/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Serge Zaitsev
 * Copyright (c) 2022 Daniel-Constantin Mierla
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _SNEXPR_H_
#define _SNEXPR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h> /* for isspace */
#include <limits.h>
#include <math.h> /* for pow */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SNEXPR_TOP (1 << 0)
#define SNEXPR_TOPEN (1 << 1)
#define SNEXPR_TCLOSE (1 << 2)
#define SNEXPR_TNUMBER (1 << 3)
#define SNEXPR_TSTRING (1 << 4)
#define SNEXPR_TWORD (1 << 5)
#define SNEXPR_TDEFAULT (SNEXPR_TOPEN | SNEXPR_TNUMBER | SNEXPR_TSTRING | SNEXPR_TWORD)

#define SNEXPR_UNARY (1 << 16)
#define SNEXPR_COMMA (1 << 17)
#define SNEXPR_EXPALLOC (1 << 18)
#define SNEXPR_VALALLOC (1 << 19)
#define SNEXPR_VALASSIGN (1 << 20)


/*
 * Simple expandable vector implementation
 */
static int sne_vec_expand(char **buf, int *length, int *cap, int memsz)
{
	if(*length + 1 > *cap) {
		void *ptr;
		int n = (*cap == 0) ? 1 : *cap << 1;
		ptr = realloc(*buf, n * memsz);
		if(ptr == NULL) {
			return -1; /* allocation failed */
		}
		*buf = (char *)ptr;
		*cap = n;
	}
	return 0;
}
#define sne_vec(T)   \
	struct       \
	{            \
		T *buf;  \
		int len; \
		int cap; \
	}
#define sne_vec_init() \
	{              \
		NULL, 0, 0 \
	}
#define sne_vec_len(v) ((v)->len)
#define sne_vec_unpack(v) \
	(char **)&(v)->buf, &(v)->len, &(v)->cap, sizeof(*(v)->buf)
#define sne_vec_push(v, val) \
	sne_vec_expand(sne_vec_unpack(v)) ? -1 : ((v)->buf[(v)->len++] = (val), 0)
#define sne_vec_nth(v, i) (v)->buf[i]
#define sne_vec_peek(v) (v)->buf[(v)->len - 1]
#define sne_vec_pop(v) (v)->buf[--(v)->len]
#define sne_vec_free(v) (free((v)->buf), (v)->buf = NULL, (v)->len = (v)->cap = 0)
#define sne_vec_foreach(v, var, iter)                                             \
	if((v)->len > 0)                                                          \
		for((iter) = 0; (iter) < (v)->len && (((var) = (v)->buf[(iter)]), 1); \
				++(iter))

/*
 * Expression data types
 */
struct snexpr;
struct snexpr_func;
struct snexpr_var;

enum snexpr_type
{
	SNE_OP_UNKNOWN,
	SNE_OP_UNARY_MINUS,
	SNE_OP_UNARY_LOGICAL_NOT,
	SNE_OP_UNARY_BITWISE_NOT,

	SNE_OP_POWER,
	SNE_OP_DIVIDE,
	SNE_OP_MULTIPLY,
	SNE_OP_REMAINDER,

	SNE_OP_PLUS,
	SNE_OP_MINUS,

	SNE_OP_SHL,
	SNE_OP_SHR,

	SNE_OP_LT,
	SNE_OP_LE,
	SNE_OP_GT,
	SNE_OP_GE,
	SNE_OP_EQ,
	SNE_OP_NE,

	SNE_OP_BITWISE_AND,
	SNE_OP_BITWISE_OR,
	SNE_OP_BITWISE_XOR,

	SNE_OP_LOGICAL_AND,
	SNE_OP_LOGICAL_OR,

	SNE_OP_ASSIGN,
	SNE_OP_COMMA,

	SNE_OP_CONSTNUM,
	SNE_OP_CONSTSTZ,
	SNE_OP_VAR,
	SNE_OP_FUNC,
};

static int prec[] = {0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 6, 7,
		8, 9, 10, 11, 12, 0, 0, 0, 0};

typedef sne_vec(struct snexpr) sne_vec_expr_t;
typedef void (*snexprfn_cleanup_t)(struct snexpr_func *f, void *context);
typedef struct snexpr* (*snexprfn_t)(struct snexpr_func *f, sne_vec_expr_t *args, void *context);

typedef struct snexpr* (*snexternval_cbf_t)(char *vname);

static snexternval_cbf_t _snexternval_cbf = NULL;

struct snexpr
{
	enum snexpr_type type;
	unsigned int eflags;
	union
	{
		struct
		{
			float nval;
		} num;
		struct
		{
			char *sval;
		} stz;
		struct
		{
			struct snexpr_var *vref;
		} var;
		struct
		{
			sne_vec_expr_t args;
		} op;
		struct
		{
			struct snexpr_func *f;
			sne_vec_expr_t args;
			void *context;
		} func;
	} param;
};

#define snexpr_init()                \
	{                                \
		.type = (enum snexpr_type)0, \
		.eflags = 0u                 \
	}

struct snexpr_string
{
	const char *s;
	int n;
};
struct snexpr_arg
{
	int oslen;
	int eslen;
	sne_vec_expr_t args;
};

typedef sne_vec(struct snexpr_string) sne_vec_str_t;
typedef sne_vec(struct snexpr_arg) sne_vec_arg_t;

static int snexpr_is_unary(enum snexpr_type op)
{
	return op == SNE_OP_UNARY_MINUS || op == SNE_OP_UNARY_LOGICAL_NOT
		   || op == SNE_OP_UNARY_BITWISE_NOT;
}

static int snexpr_is_binary(enum snexpr_type op)
{
	return !snexpr_is_unary(op) && op != SNE_OP_CONSTNUM && op != SNE_OP_CONSTSTZ
		 && op != SNE_OP_VAR && op != SNE_OP_FUNC && op != SNE_OP_UNKNOWN;
}

static int snexpr_prec(enum snexpr_type a, enum snexpr_type b)
{
	int left = snexpr_is_binary(a) && a != SNE_OP_ASSIGN && a != SNE_OP_POWER
			   && a != SNE_OP_COMMA;
	return (left && prec[a] >= prec[b]) || (prec[a] > prec[b]);
}

#define isfirstvarchr(c) \
	(((unsigned char)c >= '@' && c != '^' && c != '|') || c == '$')
#define isvarchr(c)                                                            \
	(((unsigned char)c >= '@' && c != '^' && c != '|') || c == '$' || c == '#' \
			|| (c >= '0' && c <= '9'))

static struct
{
	const char *s;
	const enum snexpr_type op;
} OPS[] = {
		{"-u", SNE_OP_UNARY_MINUS},
		{"!u", SNE_OP_UNARY_LOGICAL_NOT},
		{"^u", SNE_OP_UNARY_BITWISE_NOT},
		{"**", SNE_OP_POWER},
		{"*", SNE_OP_MULTIPLY},
		{"/", SNE_OP_DIVIDE},
		{"%", SNE_OP_REMAINDER},
		{"+", SNE_OP_PLUS},
		{"-", SNE_OP_MINUS},
		{"<<", SNE_OP_SHL},
		{">>", SNE_OP_SHR},
		{"<", SNE_OP_LT},
		{"<=", SNE_OP_LE},
		{">", SNE_OP_GT},
		{">=", SNE_OP_GE},
		{"==", SNE_OP_EQ},
		{"!=", SNE_OP_NE},
		{"&", SNE_OP_BITWISE_AND},
		{"|", SNE_OP_BITWISE_OR},
		{"^", SNE_OP_BITWISE_XOR},
		{"&&", SNE_OP_LOGICAL_AND},
		{"||", SNE_OP_LOGICAL_OR},
		{"=", SNE_OP_ASSIGN},
		{",", SNE_OP_COMMA},

		/* These are used by lexer and must be ignored by parser, so we put
       them at the end */
		{"-", SNE_OP_UNARY_MINUS},
		{"!", SNE_OP_UNARY_LOGICAL_NOT},
		{"^", SNE_OP_UNARY_BITWISE_NOT},
};

static enum snexpr_type snexpr_op(const char *s, size_t len, int unary)
{
	unsigned int i;
	for(i = 0; i < sizeof(OPS) / sizeof(OPS[0]); i++) {
		if(strlen(OPS[i].s) == len && strncmp(OPS[i].s, s, len) == 0
				&& (unary == -1 || snexpr_is_unary(OPS[i].op) == unary)) {
			return OPS[i].op;
		}
	}
	return SNE_OP_UNKNOWN;
}

static float snexpr_parse_number(const char *s, size_t len)
{
	float num = 0;
	unsigned int frac = 0;
	unsigned int digits = 0;
	unsigned int i;
	for(i = 0; i < len; i++) {
		if(s[i] == '.' && frac == 0) {
			frac++;
			continue;
		}
		if(isdigit(s[i])) {
			digits++;
			if(frac > 0) {
				frac++;
			}
			num = num * 10 + (s[i] - '0');
		} else {
			return NAN;
		}
	}
	while(frac > 1) {
		num = num / 10;
		frac--;
	}
	return (digits > 0 ? num : NAN);
}

/*
 * Functions
 */
struct snexpr_func
{
	const char *name;
	snexprfn_t f;
	snexprfn_cleanup_t cleanup;
	size_t ctxsz;
};

static struct snexpr_func *snexpr_func_find(
		struct snexpr_func *funcs, const char *s, size_t len)
{
	struct snexpr_func *f;
	for(f = funcs; f->name; f++) {
		if(strlen(f->name) == len && strncmp(f->name, s, len) == 0) {
			return f;
		}
	}
	return NULL;
}

/*
 * Variables
 */
struct snexpr_var
{
	unsigned int evflags;
	char *name;
	union
	{
		float nval;
		char *sval;
	} v;
	struct snexpr_var *next;
};

struct snexpr_var_list
{
	struct snexpr_var *head;
};

static struct snexpr_var *snexpr_var_find(
		struct snexpr_var_list *vars, const char *s, size_t len)
{
	struct snexpr_var *v = NULL;
	if(len == 0 || !isfirstvarchr(*s)) {
		return NULL;
	}
	for(v = vars->head; v; v = v->next) {
		if(strlen(v->name) == len && strncmp(v->name, s, len) == 0) {
			return v;
		}
	}
	v = (struct snexpr_var *)calloc(1, sizeof(struct snexpr_var) + len + 1);
	if(v == NULL) {
		return NULL; /* allocation failed */
	}
	memset(v, 0, sizeof(struct snexpr_var) + len + 1);
	v->next = vars->head;
	v->name = (char *)v + sizeof(struct snexpr_var);
	strncpy(v->name, s, len);
	v->name[len] = '\0';
	vars->head = v;
	return v;
}

static int to_int(float x)
{
	if(isnan(x)) {
		return 0;
	} else if(isinf(x) != 0) {
		return INT_MAX * isinf(x);
	} else {
		return (int)x;
	}
}


static int snexpr_format_num(char **out, float value)
{
	int ret = 0;
	*out = (char*)malloc(24*sizeof(char));
	if(*out==NULL) {
		return -1;
	}
	if(value - (long)value != 0) {
#ifdef SNEXPR_FLOAT_FULLPREC
		ret = snprintf(*out, 24, "%g", value);
#else
		ret = snprintf(*out, 24, "%.4g", value);
#endif
	} else {
		ret = snprintf(*out, 24, "%lld", (long long)value);
	}
	if((ret < 0) || (ret >= 24)) {
		free(*out);
		*out = NULL;
		return -2;
	}
	return 0;
}

static struct snexpr *snexpr_convert_num(float value, unsigned int ctype)
{
	struct snexpr *e = (struct snexpr *)malloc(sizeof(struct snexpr));
	if(e == NULL) {
		return NULL;
	}
	memset(e, 0, sizeof(struct snexpr));

	if(ctype == SNE_OP_CONSTSTZ) {
		e->eflags |= SNEXPR_EXPALLOC | SNEXPR_VALALLOC;
		e->type = SNE_OP_CONSTSTZ;
		snexpr_format_num(&e->param.stz.sval, value);
		return e;
	}

	e->eflags |= SNEXPR_EXPALLOC;
	e->type = SNE_OP_CONSTNUM;
	e->param.num.nval = value;
	return e;
}

static struct snexpr *snexpr_convert_stzl(char *value, size_t len, unsigned int ctype)
{
	struct snexpr *e = NULL;
	if(value==NULL) {
		return NULL;
	}
	e = (struct snexpr *)malloc(sizeof(struct snexpr));
	if(e == NULL) {
		return NULL;
	}
	memset(e, 0, sizeof(struct snexpr));

	if(ctype == SNE_OP_CONSTNUM) {
		e->eflags |= SNEXPR_EXPALLOC;
		e->type = SNE_OP_CONSTNUM;
		e->param.num.nval = snexpr_parse_number(value, len);
		return e;
	}

	e->param.stz.sval = (char *)malloc(len + 1);
	if(e->param.stz.sval == NULL) {
		free(e);
		return NULL;
	}
	e->eflags |= SNEXPR_EXPALLOC | SNEXPR_VALALLOC;
	e->type = SNE_OP_CONSTSTZ;
	memcpy(e->param.stz.sval, value, len);
	e->param.stz.sval[len] = '\0';
	return e;
}

static struct snexpr *snexpr_convert_stz(char *value, unsigned int ctype)
{
	if(value==NULL) {
		return NULL;
	}

	return snexpr_convert_stzl(value, strlen(value), ctype);
}

static struct snexpr *snexpr_concat_strz(char *value0, char *value1)
{
	struct snexpr *e = (struct snexpr *)malloc(sizeof(struct snexpr));
	if(e == NULL) {
		return NULL;
	}
	memset(e, 0, sizeof(struct snexpr));

	e->param.stz.sval = (char *)malloc(strlen(value0) + strlen(value1) + 1);
	if(e->param.stz.sval == NULL) {
		free(e);
		return NULL;
	}
	e->eflags |= SNEXPR_EXPALLOC | SNEXPR_VALALLOC;
	e->type = SNE_OP_CONSTSTZ;
	strcpy(e->param.stz.sval, value0);
	strcat(e->param.stz.sval, value1);
	return e;
}

static void snexpr_result_free(struct snexpr *e)
{
	if(e == NULL) {
		return;
	}
	if((e->eflags & SNEXPR_VALALLOC) && (e->type == SNE_OP_CONSTSTZ)
			&& (e->param.stz.sval != NULL)) {
		free(e->param.stz.sval);
	}
	if(!(e->eflags & SNEXPR_EXPALLOC)) {
		return;
	}
	free(e);
}

#define snexpr_eval_check_val(val, vtype) do { \
		if(val==NULL || val->type != vtype) { \
			goto error; \
		} \
	} while(0)

#define snexpr_eval_check_null(val, vtype) do { \
		if(val==NULL) { \
			goto error; \
		} \
	} while(0)

#define snexpr_eval_cmp(_CMPOP_) do { \
			rv0 = snexpr_eval(&e->param.op.args.buf[0]); \
			snexpr_eval_check_null(rv0, SNE_OP_CONSTNUM); \
			rv1 = snexpr_eval(&e->param.op.args.buf[1]); \
			snexpr_eval_check_null(rv1, SNE_OP_CONSTNUM); \
			if(rv0->type == SNE_OP_CONSTSTZ) { \
				/* string comparison */ \
				if(rv1->type == SNE_OP_CONSTNUM) { \
					tv = snexpr_convert_num(rv1->param.num.nval, SNE_OP_CONSTSTZ); \
					snexpr_result_free(rv1); \
					rv1 = tv; \
					snexpr_eval_check_val(rv1, SNE_OP_CONSTSTZ); \
				} \
				if(strcmp(rv0->param.stz.sval, rv1->param.stz.sval) _CMPOP_ 0) { \
					lv = snexpr_convert_num(1, SNE_OP_CONSTNUM); \
				} else { \
					lv = snexpr_convert_num(0, SNE_OP_CONSTNUM); \
				} \
			} else { \
				/* number comparison */ \
				if(rv1->type == SNE_OP_CONSTSTZ) { \
					tv = snexpr_convert_stz(rv1->param.stz.sval, SNE_OP_CONSTNUM); \
					snexpr_result_free(rv1); \
					rv1 = tv; \
					snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM); \
				} \
				lv = snexpr_convert_num( \
						rv0->param.num.nval _CMPOP_ rv1->param.num.nval, SNE_OP_CONSTNUM); \
			} \
	} while(0)

static struct snexpr *snexpr_eval(struct snexpr *e)
{
	float n;
	struct snexpr *lv = NULL;
	struct snexpr *rv0 = NULL;
	struct snexpr *rv1 = NULL;
	struct snexpr *tv = NULL;

	switch(e->type) {
		case SNE_OP_UNARY_MINUS:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(-(rv0->param.num.nval), SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_UNARY_LOGICAL_NOT:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(!(rv0->param.num.nval), SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_UNARY_BITWISE_NOT:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(~(to_int(rv0->param.num.nval)), SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_POWER:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					powf(rv0->param.num.nval, rv1->param.num.nval),
					SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_MULTIPLY:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					rv0->param.num.nval * rv1->param.num.nval, SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_DIVIDE:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			if(rv1->param.num.nval == 0) {
				goto error;
			}
			lv = snexpr_convert_num(
					rv0->param.num.nval / rv1->param.num.nval, SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_REMAINDER:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					fmodf(rv0->param.num.nval, rv1->param.num.nval),
					SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_PLUS:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_null(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_null(rv1, SNE_OP_CONSTNUM);
			if(rv0->type == SNE_OP_CONSTSTZ) {
				/* string concatenation */
				if(rv1->type == SNE_OP_CONSTNUM) {
					tv = snexpr_convert_num(rv1->param.num.nval, SNE_OP_CONSTSTZ);
					snexpr_result_free(rv1);
					rv1 = tv;
					snexpr_eval_check_val(rv1, SNE_OP_CONSTSTZ);
				}
				lv = snexpr_concat_strz(rv0->param.stz.sval, rv1->param.stz.sval);
			} else {
				/* add */
				if(rv1->type == SNE_OP_CONSTSTZ) {
					tv = snexpr_convert_stz(rv1->param.stz.sval, SNE_OP_CONSTNUM);
					snexpr_result_free(rv1);
					rv1 = tv;
					snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
				}
				lv = snexpr_convert_num(
						rv0->param.num.nval + rv1->param.num.nval, SNE_OP_CONSTNUM);
			}
			goto done;
		case SNE_OP_MINUS:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					rv0->param.num.nval - rv1->param.num.nval, SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_SHL:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					to_int(rv0->param.num.nval) << to_int(rv1->param.num.nval),
					SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_SHR:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					to_int(rv0->param.num.nval) >> to_int(rv1->param.num.nval),
					SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_LT:
			snexpr_eval_cmp(<);
			goto done;
		case SNE_OP_LE:
			snexpr_eval_cmp(<=);
			goto done;
		case SNE_OP_GT:
			snexpr_eval_cmp(>);
			goto done;
		case SNE_OP_GE:
			snexpr_eval_cmp(>=);
			goto done;
		case SNE_OP_EQ:
			snexpr_eval_cmp(==);
			goto done;
		case SNE_OP_NE:
			snexpr_eval_cmp(!=);
			goto done;
		case SNE_OP_BITWISE_AND:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					to_int(rv0->param.num.nval) & to_int(rv1->param.num.nval),
					SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_BITWISE_OR:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					to_int(rv0->param.num.nval) | to_int(rv1->param.num.nval),
					SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_BITWISE_XOR:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			snexpr_eval_check_val(rv0, SNE_OP_CONSTNUM);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_val(rv1, SNE_OP_CONSTNUM);
			lv = snexpr_convert_num(
					to_int(rv0->param.num.nval) ^ to_int(rv1->param.num.nval),
					SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_LOGICAL_AND:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			n = rv0->param.num.nval;
			if(n != 0) {
				rv1 = snexpr_eval(&e->param.op.args.buf[1]);
				n = rv1->param.num.nval;
				if(n != 0) {
					lv = snexpr_convert_num(n, SNE_OP_CONSTNUM);
					goto done;
				}
			}
			lv = snexpr_convert_num(0, SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_LOGICAL_OR:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			n = rv0->param.num.nval;
			if(n != 0 && !isnan(n)) {
				lv = snexpr_convert_num(n, SNE_OP_CONSTNUM);
				goto done;
			} else {
				rv1 = snexpr_eval(&e->param.op.args.buf[1]);
				n = rv1->param.num.nval;
				if(n != 0) {
					lv = snexpr_convert_num(n, SNE_OP_CONSTNUM);
					goto done;
				}
			}
			lv = snexpr_convert_num(0, SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_ASSIGN:
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			snexpr_eval_check_null(rv1, SNE_OP_CONSTNUM);
			if(sne_vec_nth(&e->param.op.args, 0).type == SNE_OP_VAR) {
				if(e->param.op.args.buf[0].param.var.vref->evflags & SNEXPR_VALALLOC) {
					if(e->param.op.args.buf[0].param.var.vref->v.sval!=NULL) {
						free(e->param.op.args.buf[0].param.var.vref->v.sval);
						e->param.op.args.buf[0].param.var.vref->v.sval = NULL;
					}
					e->param.op.args.buf[0].param.var.vref->evflags &= ~(SNEXPR_TSTRING|SNEXPR_VALALLOC);
				}
				if(rv1->type == SNE_OP_CONSTSTZ) {
					e->param.op.args.buf[0].param.var.vref->v.sval = strdup(rv1->param.stz.sval);
					e->param.op.args.buf[0].param.var.vref->evflags |= SNEXPR_VALASSIGN|SNEXPR_TSTRING|SNEXPR_VALALLOC;
					lv = snexpr_convert_stz(rv1->param.stz.sval, SNE_OP_CONSTSTZ);
				} else {
					n = rv1->param.num.nval;
					e->param.op.args.buf[0].param.var.vref->v.nval = n;
					e->param.op.args.buf[0].param.var.vref->evflags |= SNEXPR_VALASSIGN;
					lv = snexpr_convert_num(n, SNE_OP_CONSTNUM);
				}
			}
			goto done;
		case SNE_OP_COMMA:
			rv0 = snexpr_eval(&e->param.op.args.buf[0]);
			rv1 = snexpr_eval(&e->param.op.args.buf[1]);
			if(rv1->type == SNE_OP_CONSTSTZ) {
				lv = snexpr_convert_stz(rv1->param.stz.sval, SNE_OP_CONSTSTZ);
			} else {
				lv = snexpr_convert_num(rv1->param.num.nval, SNE_OP_CONSTNUM);
			}
			goto done;
		case SNE_OP_CONSTNUM:
			lv = snexpr_convert_num(e->param.num.nval, SNE_OP_CONSTNUM);
			goto done;
		case SNE_OP_CONSTSTZ:
			lv = snexpr_convert_stz(e->param.stz.sval, SNE_OP_CONSTSTZ);
			goto done;
		case SNE_OP_VAR:
			if((_snexternval_cbf == NULL)
					|| (e->param.var.vref->evflags & SNEXPR_VALASSIGN)) {
				if(e->param.var.vref->evflags & SNEXPR_TSTRING) {
					lv = snexpr_convert_stz(e->param.var.vref->v.sval, SNE_OP_CONSTSTZ);
				} else {
					snexpr_convert_num(e->param.var.vref->v.nval, SNE_OP_CONSTNUM);
				}
			} else {
				lv = _snexternval_cbf(e->param.var.vref->name);
			}
			goto done;
		case SNE_OP_FUNC:
			rv0 = e->param.func.f->f(e->param.func.f, &e->param.func.args,
							e->param.func.context);
			if(rv0->type == SNE_OP_CONSTSTZ) {
				lv = snexpr_convert_stz(rv0->param.stz.sval, SNE_OP_CONSTSTZ);
			} else {
				lv = snexpr_convert_num(rv0->param.num.nval, SNE_OP_CONSTNUM);
			}
			goto done;
		default:
			lv = snexpr_convert_num(NAN, SNE_OP_CONSTNUM);
			goto done;
	}

done:
	if(rv0 != NULL) {
		snexpr_result_free(rv0);
	}
	if(rv1 != NULL) {
		snexpr_result_free(rv1);
	}
	return lv;

error:
	if(rv0 != NULL) {
		snexpr_result_free(rv0);
	}
	if(rv1 != NULL) {
		snexpr_result_free(rv1);
	}
	return NULL;

}

static int snexpr_next_token(const char *s, size_t len, int *flags)
{
	unsigned int i = 0;
	char b;
	int bsf = 0;
	if(len == 0) {
		return 0;
	}
	char c = s[0];
	if(c == '#') {
		for(; i < len && s[i] != '\n'; i++)
			;
		return i;
	} else if(c == '\n') {
		for(; i < len && isspace(s[i]); i++)
			;
		if(*flags & SNEXPR_TOP) {
			if(i == len || s[i] == ')') {
				*flags = *flags & (~SNEXPR_COMMA);
			} else {
				*flags = SNEXPR_TNUMBER | SNEXPR_TSTRING | SNEXPR_TWORD | SNEXPR_TOPEN
						 | SNEXPR_COMMA;
			}
		}
		return i;
	} else if(isspace(c)) {
		while(i < len && isspace(s[i]) && s[i] != '\n') {
			i++;
		}
		return i;
	} else if(isdigit(c)) {
		if((*flags & SNEXPR_TNUMBER) == 0) {
			return -1; // unexpected number
		}
		*flags = SNEXPR_TOP | SNEXPR_TCLOSE;
		while((c == '.' || isdigit(c)) && i < len) {
			i++;
			c = s[i];
		}
		return i;
	} else if(c == '"' || c == '\'') {
		if((*flags & SNEXPR_TSTRING) == 0) {
			return -1; // unexpected string
		}
		if(i == len - 1) {
			return -1; // invalud start of string
		}
		*flags = SNEXPR_TOP | SNEXPR_TCLOSE;
		b = c;
		i++;
		c = s[i];
		while(i < len && (bsf==1 || c != b)) {
			if(bsf == 0 && c == '\\') {
				bsf = 1;
			} else {
				bsf = 0;
			}
			i++;
			c = s[i];
		}
		return i + 1;
	} else if(isfirstvarchr(c)) {
		if((*flags & SNEXPR_TWORD) == 0) {
			return -2; // unexpected word
		}
		*flags = SNEXPR_TOP | SNEXPR_TOPEN | SNEXPR_TCLOSE;
		while((isvarchr(c)) && i < len) {
			i++;
			c = s[i];
		}
		return i;
	} else if(c == '(' || c == ')') {
		if(c == '(' && (*flags & SNEXPR_TOPEN) != 0) {
			*flags = SNEXPR_TNUMBER | SNEXPR_TSTRING | SNEXPR_TWORD | SNEXPR_TOPEN
					 | SNEXPR_TCLOSE;
		} else if(c == ')' && (*flags & SNEXPR_TCLOSE) != 0) {
			*flags = SNEXPR_TOP | SNEXPR_TCLOSE;
		} else {
			return -3; // unexpected parenthesis
		}
		return 1;
	} else {
		if((*flags & SNEXPR_TOP) == 0) {
			if(snexpr_op(&c, 1, 1) == SNE_OP_UNKNOWN) {
				return -4; // missing expected operand
			}
			*flags = SNEXPR_TNUMBER | SNEXPR_TSTRING | SNEXPR_TWORD | SNEXPR_TOPEN
					 | SNEXPR_UNARY;
			return 1;
		} else {
			int found = 0;
			while(!isvarchr(c) && !isspace(c) && c != '(' && c != ')'
					&& i < len) {
				if(snexpr_op(s, i + 1, 0) != SNE_OP_UNKNOWN) {
					found = 1;
				} else if(found) {
					break;
				}
				i++;
				c = s[i];
			}
			if(!found) {
				return -5; // unknown operator
			}
			*flags = SNEXPR_TNUMBER | SNEXPR_TSTRING | SNEXPR_TWORD | SNEXPR_TOPEN;
			return i;
		}
	}
}

#define SNEXPR_PAREN_ALLOWED 0
#define SNEXPR_PAREN_EXPECTED 1
#define SNEXPR_PAREN_FORBIDDEN 2

static int snexpr_bind(const char *s, size_t len, sne_vec_expr_t *es)
{
	enum snexpr_type op = snexpr_op(s, len, -1);
	if(op == SNE_OP_UNKNOWN) {
		return -1;
	}

	if(snexpr_is_unary(op)) {
		if(sne_vec_len(es) < 1) {
			return -1;
		}
		struct snexpr arg = sne_vec_pop(es);
		struct snexpr unary = snexpr_init();
		unary.type = op;
		sne_vec_push(&unary.param.op.args, arg);
		sne_vec_push(es, unary);
	} else {
		if(sne_vec_len(es) < 2) {
			return -1;
		}
		struct snexpr b = sne_vec_pop(es);
		struct snexpr a = sne_vec_pop(es);
		struct snexpr binary = snexpr_init();
		binary.type = op;
		if(op == SNE_OP_ASSIGN && a.type != SNE_OP_VAR) {
			return -1; /* Bad assignment */
		}
		sne_vec_push(&binary.param.op.args, a);
		sne_vec_push(&binary.param.op.args, b);
		sne_vec_push(es, binary);
	}
	return 0;
}

static struct snexpr snexpr_constnum(float value)
{
	struct snexpr e = snexpr_init();
	e.type = SNE_OP_CONSTNUM;
	e.param.num.nval = value;
	return e;
}

static struct snexpr snexpr_varref(struct snexpr_var *v)
{
	struct snexpr e = snexpr_init();
	e.type = SNE_OP_VAR;
	e.param.var.vref = v;
	return e;
}

static struct snexpr snexpr_conststr(const char *value, int len)
{
	struct snexpr e = snexpr_init();
	char *p;
	int i;
	int bsf = 0;
	if(len < 2) {
		len = 0;
	} else {
		/* skip the quotes */
		len -= 2;
	}
	e.type = SNE_OP_CONSTSTZ;
	e.param.stz.sval = malloc(len + 1);
	if(e.param.stz.sval) {
		if(len > 0) {
			/* do not copy the quotes - start from value[1] */
			p = e.param.stz.sval;
			for(i=0; i<len; i++) {
				if(bsf==0 && value[i+1]=='\\') {
					bsf = 1;
				} else if(bsf==1) {
					bsf = 0;
					switch(value[i+1]) {
						case 'n':
							*p = '\n';
						break;
						case 'r':
							*p = '\r';
						break;
						case 't':
							*p = '\t';
						break;
						default:
							*p = value[i+1];
					}
					p++;
				} else {
					bsf = 0;
					*p = value[i+1];
					p++;
				}
			}
			*p = '\0';
		} else {
			e.param.stz.sval[0] = '\0';
		}
	}
	return e;
}

static struct snexpr snexpr_binary(
		enum snexpr_type type, struct snexpr a, struct snexpr b)
{
	struct snexpr e = snexpr_init();
	e.type = type;
	sne_vec_push(&e.param.op.args, a);
	sne_vec_push(&e.param.op.args, b);
	return e;
}

static inline void snexpr_copy(struct snexpr *dst, struct snexpr *src)
{
	int i;
	struct snexpr arg;
	dst->type = src->type;
	if(src->type == SNE_OP_FUNC) {
		dst->param.func.f = src->param.func.f;
		sne_vec_foreach(&src->param.func.args, arg, i)
		{
			struct snexpr tmp = snexpr_init();
			snexpr_copy(&tmp, &arg);
			sne_vec_push(&dst->param.func.args, tmp);
		}
		if(src->param.func.f->ctxsz > 0) {
			dst->param.func.context = calloc(1, src->param.func.f->ctxsz);
		}
	} else if(src->type == SNE_OP_CONSTNUM) {
		dst->param.num.nval = src->param.num.nval;
	} else if(src->type == SNE_OP_VAR) {
		dst->param.var.vref = src->param.var.vref;
	} else {
		sne_vec_foreach(&src->param.op.args, arg, i)
		{
			struct snexpr tmp = snexpr_init();
			snexpr_copy(&tmp, &arg);
			sne_vec_push(&dst->param.op.args, tmp);
		}
	}
}

static void snexpr_destroy_args(struct snexpr *e);

static struct snexpr *snexpr_create(const char *s, size_t len,
		struct snexpr_var_list *vars, struct snexpr_func *funcs,
		snexternval_cbf_t evcbf)
{
	float num;
	struct snexpr_var *v;
	const char *id = NULL;
	size_t idn = 0;

	struct snexpr *result = NULL;

	_snexternval_cbf = evcbf;

	sne_vec_expr_t es = sne_vec_init();
	sne_vec_str_t os = sne_vec_init();
	sne_vec_arg_t as = sne_vec_init();

	struct macro
	{
		char *name;
		sne_vec_expr_t body;
	};
	sne_vec(struct macro) macros = sne_vec_init();

	int flags = SNEXPR_TDEFAULT;
	int paren = SNEXPR_PAREN_ALLOWED;
	for(;;) {
		int n = snexpr_next_token(s, len, &flags);
		if(n == 0) {
			break;
		} else if(n < 0) {
			goto cleanup;
		}
		const char *tok = s;
		s = s + n;
		len = len - n;
		if(*tok == '#') {
			continue;
		}
		if(flags & SNEXPR_UNARY) {
			if(n == 1) {
				switch(*tok) {
					case '-':
						tok = "-u";
						break;
					case '^':
						tok = "^u";
						break;
					case '!':
						tok = "!u";
						break;
					default:
						goto cleanup;
				}
				n = 2;
			}
		}
		if(*tok == '\n' && (flags & SNEXPR_COMMA)) {
			flags = flags & (~SNEXPR_COMMA);
			n = 1;
			tok = ",";
		}
		if(isspace(*tok)) {
			continue;
		}
		int paren_next = SNEXPR_PAREN_ALLOWED;

		if(idn > 0) {
			if(n == 1 && *tok == '(') {
				int i;
				int has_macro = 0;
				struct macro m;
				sne_vec_foreach(&macros, m, i)
				{
					if(strlen(m.name) == idn && strncmp(m.name, id, idn) == 0) {
						has_macro = 1;
						break;
					}
				}
				if((idn == 1 && id[0] == '$') || has_macro
						|| snexpr_func_find(funcs, id, idn) != NULL) {
					struct snexpr_string str = {id, (int)idn};
					sne_vec_push(&os, str);
					paren = SNEXPR_PAREN_EXPECTED;
				} else {
					goto cleanup; /* invalid function name */
				}
			} else if((v = snexpr_var_find(vars, id, idn)) != NULL) {
				sne_vec_push(&es, snexpr_varref(v));
				paren = SNEXPR_PAREN_FORBIDDEN;
			}
			id = NULL;
			idn = 0;
		}

		if(n == 1 && *tok == '(') {
			if(paren == SNEXPR_PAREN_EXPECTED) {
				struct snexpr_string str = {"{", 1};
				sne_vec_push(&os, str);
				struct snexpr_arg arg = {sne_vec_len(&os), sne_vec_len(&es), sne_vec_init()};
				sne_vec_push(&as, arg);
			} else if(paren == SNEXPR_PAREN_ALLOWED) {
				struct snexpr_string str = {"(", 1};
				sne_vec_push(&os, str);
			} else {
				goto cleanup; // Bad call
			}
		} else if(paren == SNEXPR_PAREN_EXPECTED) {
			goto cleanup; // Bad call
		} else if(n == 1 && *tok == ')') {
			int minlen = (sne_vec_len(&as) > 0 ? sne_vec_peek(&as).oslen : 0);
			while(sne_vec_len(&os) > minlen && *sne_vec_peek(&os).s != '('
					&& *sne_vec_peek(&os).s != '{') {
				struct snexpr_string str = sne_vec_pop(&os);
				if(snexpr_bind(str.s, str.n, &es) == -1) {
					goto cleanup;
				}
			}
			if(sne_vec_len(&os) == 0) {
				goto cleanup; // Bad parens
			}
			struct snexpr_string str = sne_vec_pop(&os);
			if(str.n == 1 && *str.s == '{') {
				str = sne_vec_pop(&os);
				struct snexpr_arg arg = sne_vec_pop(&as);
				if(sne_vec_len(&es) > arg.eslen) {
					sne_vec_push(&arg.args, sne_vec_pop(&es));
				}
				if(str.n == 1 && str.s[0] == '$') {
					if(sne_vec_len(&arg.args) < 1) {
						sne_vec_free(&arg.args);
						goto cleanup; /* too few arguments for $() function */
					}
					struct snexpr *u = &sne_vec_nth(&arg.args, 0);
					if(u->type != SNE_OP_VAR) {
						sne_vec_free(&arg.args);
						goto cleanup; /* first argument is not a variable */
					}
					struct snexpr_var *v;
					for(v = vars->head; v; v = v->next) {
						if(v == u->param.var.vref) {
							struct macro m = {v->name, arg.args};
							sne_vec_push(&macros, m);
							break;
						}
					}
					sne_vec_push(&es, snexpr_constnum(0));
				} else {
					int i = 0;
					int found = -1;
					struct macro m;
					sne_vec_foreach(&macros, m, i)
					{
						if(strlen(m.name) == (size_t)str.n
								&& strncmp(m.name, str.s, str.n) == 0) {
							found = i;
						}
					}
					if(found != -1) {
						m = sne_vec_nth(&macros, found);
						struct snexpr root = snexpr_constnum(0);
						struct snexpr *p = &root;
						int j;
						/* Assign macro parameters */
						for(j = 0; j < sne_vec_len(&arg.args); j++) {
							char varname[4];
							snprintf(varname, sizeof(varname) - 1, "$%d",
									(j + 1));
							struct snexpr_var *v =
									snexpr_var_find(vars, varname, strlen(varname));
							struct snexpr ev = snexpr_varref(v);
							struct snexpr assign = snexpr_binary(
									SNE_OP_ASSIGN, ev, sne_vec_nth(&arg.args, j));
							*p = snexpr_binary(
									SNE_OP_COMMA, assign, snexpr_constnum(0));
							p = &sne_vec_nth(&p->param.op.args, 1);
						}
						/* Expand macro body */
						for(j = 1; j < sne_vec_len(&m.body); j++) {
							if(j < sne_vec_len(&m.body) - 1) {
								*p = snexpr_binary(SNE_OP_COMMA, snexpr_constnum(0),
										snexpr_constnum(0));
								snexpr_copy(&sne_vec_nth(&p->param.op.args, 0),
										&sne_vec_nth(&m.body, j));
							} else {
								snexpr_copy(p, &sne_vec_nth(&m.body, j));
							}
							p = &sne_vec_nth(&p->param.op.args, 1);
						}
						sne_vec_push(&es, root);
						sne_vec_free(&arg.args);
					} else {
						struct snexpr_func *f = snexpr_func_find(funcs, str.s, str.n);
						struct snexpr bound_func = snexpr_init();
						bound_func.type = SNE_OP_FUNC;
						bound_func.param.func.f = f;
						bound_func.param.func.args = arg.args;
						if(f->ctxsz > 0) {
							void *p = calloc(1, f->ctxsz);
							if(p == NULL) {
								goto cleanup; /* allocation failed */
							}
							bound_func.param.func.context = p;
						}
						sne_vec_push(&es, bound_func);
					}
				}
			}
			paren_next = SNEXPR_PAREN_FORBIDDEN;
		} else if(!isnan(num = snexpr_parse_number(tok, n))) {
			sne_vec_push(&es, snexpr_constnum(num));
			paren_next = SNEXPR_PAREN_FORBIDDEN;
		} else if(*tok == '"' || *tok == '\'') {
			sne_vec_push(&es, snexpr_conststr(tok, n));
			paren_next = SNEXPR_PAREN_FORBIDDEN;
		} else if(snexpr_op(tok, n, -1) != SNE_OP_UNKNOWN) {
			enum snexpr_type op = snexpr_op(tok, n, -1);
			struct snexpr_string o2 = {NULL, 0};
			if(sne_vec_len(&os) > 0) {
				o2 = sne_vec_peek(&os);
			}
			for(;;) {
				if(n == 1 && *tok == ',' && sne_vec_len(&os) > 0) {
					struct snexpr_string str = sne_vec_peek(&os);
					if(str.n == 1 && *str.s == '{') {
						struct snexpr e = sne_vec_pop(&es);
						sne_vec_push(&sne_vec_peek(&as).args, e);
						break;
					}
				}
				enum snexpr_type type2 = snexpr_op(o2.s, o2.n, -1);
				if(!(type2 != SNE_OP_UNKNOWN && snexpr_prec(op, type2))) {
					struct snexpr_string str = {tok, n};
					sne_vec_push(&os, str);
					break;
				}

				if(snexpr_bind(o2.s, o2.n, &es) == -1) {
					goto cleanup;
				}
				(void)sne_vec_pop(&os);
				if(sne_vec_len(&os) > 0) {
					o2 = sne_vec_peek(&os);
				} else {
					o2.n = 0;
				}
			}
		} else {
			if(n > 0 && !isdigit(*tok)) {
				/* Valid identifier, a variable or a function */
				id = tok;
				idn = n;
			} else {
				goto cleanup; // Bad variable name, e.g. '2.3.4' or '4ever'
			}
		}
		paren = paren_next;
	}

	if(idn > 0) {
		sne_vec_push(&es, snexpr_varref(snexpr_var_find(vars, id, idn)));
	}

	while(sne_vec_len(&os) > 0) {
		struct snexpr_string rest = sne_vec_pop(&os);
		if(rest.n == 1 && (*rest.s == '(' || *rest.s == ')')) {
			goto cleanup; // Bad paren
		}
		if(snexpr_bind(rest.s, rest.n, &es) == -1) {
			goto cleanup;
		}
	}

	result = (struct snexpr *)calloc(1, sizeof(struct snexpr));
	if(result != NULL) {
		if(sne_vec_len(&es) == 0) {
			result->type = SNE_OP_CONSTNUM;
		} else {
			*result = sne_vec_pop(&es);
		}
	}

	int i, j;
	struct macro m;
	struct snexpr e;
	struct snexpr_arg a;
cleanup:
	sne_vec_foreach(&macros, m, i)
	{
		struct snexpr e;
		sne_vec_foreach(&m.body, e, j)
		{
			snexpr_destroy_args(&e);
		}
		sne_vec_free(&m.body);
	}
	sne_vec_free(&macros);

	sne_vec_foreach(&es, e, i)
	{
		snexpr_destroy_args(&e);
	}
	sne_vec_free(&es);

	sne_vec_foreach(&as, a, i)
	{
		sne_vec_foreach(&a.args, e, j)
		{
			snexpr_destroy_args(&e);
		}
		sne_vec_free(&a.args);
	}
	sne_vec_free(&as);

	/*sne_vec_foreach(&os, o, i) {sne_vec_free(&m.body);}*/
	sne_vec_free(&os);
	if(result==NULL) {
		_snexternval_cbf = NULL;
	}

	return result;
}

static void snexpr_destroy_args(struct snexpr *e)
{
	int i;
	struct snexpr arg;
	if(e->type == SNE_OP_FUNC) {
		sne_vec_foreach(&e->param.func.args, arg, i)
		{
			snexpr_destroy_args(&arg);
		}
		sne_vec_free(&e->param.func.args);
		if(e->param.func.context != NULL) {
			if(e->param.func.f->cleanup != NULL) {
				e->param.func.f->cleanup(
						e->param.func.f, e->param.func.context);
			}
			free(e->param.func.context);
		}
	} else if(e->type != SNE_OP_CONSTNUM && e->type != SNE_OP_CONSTSTZ
			&& e->type != SNE_OP_VAR) {
		sne_vec_foreach(&e->param.op.args, arg, i)
		{
			snexpr_destroy_args(&arg);
		}
		sne_vec_free(&e->param.op.args);
	}
}

static void snexpr_destroy(struct snexpr *e, struct snexpr_var_list *vars)
{
	struct snexpr_var *v;
	_snexternval_cbf = NULL;

	if(e != NULL) {
		snexpr_destroy_args(e);
		free(e);
	}
	if(vars != NULL) {
		for(v = vars->head; v;) {
			struct snexpr_var *next = v->next;
			if(v->evflags & SNEXPR_VALALLOC) {
				free(v->v.sval);
			}
			free(v);
			v = next;
		}
	}
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SNEXPR_H_ */
