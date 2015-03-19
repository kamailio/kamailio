/*
 * Copyright (C)  2007-2008 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file
 * \brief Kamailio dialplan :: Module interface
 * \ingroup dialplan
 * Module: \ref dialplan
 */


#ifndef _DP_DIALPLAN_H
#define _DP_DIALPLAN_H

#include <pcre.h>
#include "../../pvar.h"
#include "../../parser/msg_parser.h"

#define DP_EQUAL_OP		0
#define DP_REGEX_OP		1
#define DP_FNMATCH_OP	2

#define MAX_REPLACE_WITH	10

#define DP_TFLAGS_PV_MATCH		(1 << 0)
#define DP_TFLAGS_PV_SUBST		(1 << 1)

typedef struct dpl_node {
	int dpid;         /* dialplan id */
	int pr;           /* priority */
	int matchop;      /* matching operator */
	int matchlen;     /* matching value length */
	str match_exp;    /* match-first string */
	str subst_exp;    /* match string with subtitution groupping */
	str repl_exp;     /* replacement expression string */
	pcre *match_comp; /* compiled matching expression */
	pcre *subst_comp; /* compiled substitution expression */
	struct subst_expr *repl_comp; /* compiled replacement */
	str attrs;        /* attributes string */
	unsigned int tflags; /* flags for type of values for matching */

	struct dpl_node * next; /* next rule */
} dpl_node_t, *dpl_node_p;

/*For every distinct length of a matching string*/
typedef struct dpl_index{
	int len;
	dpl_node_t * first_rule;
	dpl_node_t * last_rule;

	struct dpl_index * next; 
}dpl_index_t, *dpl_index_p;

/*For every DPID*/
typedef struct dpl_id{
	int dp_id;
	dpl_index_t* first_index;/*fast access :rules with specific length*/
	struct dpl_id * next;
}dpl_id_t,*dpl_id_p;


#define DP_VAL_INT		0
#define DP_VAL_SPEC		1

typedef struct dp_param{
	int type;
	union {
		int id;
		pv_spec_t* sp[2];
	} v;
}dp_param_t, *dp_param_p;

int init_data();
void destroy_data();
int dp_load_db();

dpl_id_p select_dpid(int id);

struct subst_expr* repl_exp_parse(str subst);
void repl_expr_free(struct subst_expr *se);
int translate(struct sip_msg *msg, str user_name, str* repl_user, dpl_id_p idp, str *);
int rule_translate(struct sip_msg *msg, str , dpl_node_t * rule, pcre *subst_comp,  str *);

pcre *reg_ex_comp(const char *pattern, int *cap_cnt, int mtype);
#endif
