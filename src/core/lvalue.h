/* 
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
 * @brief Kamailio core :: lvalues (assignment)
 * @author andrei
 */

#ifndef __lvalue_h_
#define __lvalue_h_

#include "rvalue.h"
#include "usr_avp.h"
#include "pvar.h"
#include "parser/msg_parser.h"
#include "action.h"

union lval_u{
	pv_spec_t *pvs;
	avp_spec_t avps;
};

enum lval_type{
	LV_NONE, LV_AVP, LV_PVAR
};

struct lvalue{
	enum lval_type type;
	union lval_u lv;
};

/* lval operators */
#define EQ_T 254 /* k compatibility */

typedef int (*log_assign_action_f)(struct sip_msg* msg, struct lvalue *lv);
void set_log_assign_action_cb(log_assign_action_f f);

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
				struct lvalue* lv, struct rval_expr* rve);
#endif /* __lvalue_h_*/
