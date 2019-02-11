/* 
 * Copyright (C) 2009 iptelorg GmbH
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

#ifndef __switch_h
#define __switch_h

#include <stddef.h>
#include <regex.h>

#include "route_struct.h"


struct case_stms{
	struct rval_expr* ct_rve;
	struct action* actions;
	struct case_stms* next;
	struct case_stms** append;
	int type;     /**< type: MATCH_UNKOWN, MATCH_INT, MATCH_STR, MATCH_RE */
	int re_flags; /**< used only for REs */
	int is_default;
	union {
		int match_int;
		str match_str;
		regex_t* match_re;
	} label;  /**< fixed case argument */
};


struct switch_cond_table{
	int n;                  /**< size */
	int* cond;              /**< int labels array */
	struct action** jump;   /**< jump points array */
	struct action* def;     /**< default jump  */
};


struct switch_jmp_table{
	int first;              /**< first int label in the jump table */
	int last;               /**< last int label in the jump table */
	struct action** tbl;    /**< jmp table [v-first] iff first<=v<=last */
	struct switch_cond_table rest; /**< normal cond. table for the rest */
};


enum match_str_type { MATCH_UNKNOWN, MATCH_INT, MATCH_STR, MATCH_RE };

struct match_str{
	enum match_str_type type;/**< string or RE */
	int flags;               /**< flags for re */
	union{
		str s;              /* string */
		regex_t* regex;     /**< compiled regex */
	}l;
};

struct match_cond_table{
	int n;                   /**< size */
	struct match_str* match; /**< match array */
	struct action** jump;    /**< jump points array */
	struct action* def;      /**< default jmp */
};

int fix_switch(struct action* t);

#endif /*__switch_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
