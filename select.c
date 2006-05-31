/*
 * $Id$
 *
 * Copyright (C) 2005-2006 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2005-12-19  select framework (mma)
 *  2006-01-19  multiple nested calls, IS_ALIAS -> NESTED flag renamed (mma)
 *              DIVERSION flag checked
 *  2006-02-26  don't free str when changing type STR -> DIVERSION (mma)
 *				it can't be freeable sometimes (e.g. xlog's select)
 *
 */


#include "select.h"
#include "dprint.h"
#include "select_core.h"
#include "mem/mem.h"

/*
 * The main parser table list placeholder
 * at startup use core table, modules can
 * add their own via register_select_table call
 */
static select_table_t *select_list = &select_core_table;

int resolve_select(select_t* s)
{
	select_f f;
	int nested;
	int param_idx = 0;
	int table_idx = 0;
	select_table_t* t = NULL;;
	int accept = 0;

	f = NULL;
	nested = 0;
	memset (s->f, 0, sizeof(s->f));
	while (param_idx<s->n) {
		accept = 0;
		switch (s->params[param_idx].type) {
		case SEL_PARAM_STR:
			DBG("resolve_select: '%.*s'\n", s->params[param_idx].v.s.len, s->params[param_idx].v.s.s);
			break;
		case SEL_PARAM_INT:
			DBG("resolve_select: [%d]\n", s->params[param_idx].v.i);
			break;
		default:
			/* just to avoid the warning */
			break;
		}
		for (t=select_list; t; t=t->next) {
			table_idx = 0;
			if (!t->table) continue;
			while (t->table[table_idx].curr_f || t->table[table_idx].new_f) {
				if (t->table[table_idx].curr_f == f) {
					if (t->table[table_idx].flags & NESTED) {
						accept = 1;
					} else if (t->table[table_idx].type == s->params[param_idx].type) {
						switch (t->table[table_idx].type) {
						case SEL_PARAM_INT:
							accept = 1;
							break;
						case SEL_PARAM_STR:
							accept = (((t->table[table_idx].name.len == s->params[param_idx].v.s.len) || !t->table[table_idx].name.len)
								   && (!t->table[table_idx].name.s || !strncasecmp(t->table[table_idx].name.s, s->params[param_idx].v.s.s, s->params[param_idx].v.s.len)));
							break;
						default:
							break;
						}
					};
				}
				if (accept) goto accepted;
				table_idx++;
			}
		}
		switch (s->params[param_idx].type) {
			case SEL_PARAM_STR:
				LOG(L_ERR, "Unable to resolve select '%.*s' at level %d\n", s->params[param_idx].v.s.len, s->params[param_idx].v.s.s, param_idx);
				break;
			case SEL_PARAM_INT:
				LOG(L_ERR, "Unable to resolve select [%d] at level %d\n", s->params[param_idx].v.i, param_idx);
				break;
			default:
				BUG ("Unable to resolve select at level %d\n", param_idx);
				break;
			break;
		}
		goto not_found;

		accepted:
		if (t->table[table_idx].flags & DIVERSION) {
			/* if (s->params[param_idx].type == SEL_PARAM_STR) pkg_free(s->params[param_idx].v.s.s); */
			/* don't free it (the mem can leak only once at startup)
			 * the parsed string can live inside larger string block
			 * e.g. when xlog's select is parsed
			 */
			s->params[param_idx].type = SEL_PARAM_DIV;
			s->params[param_idx].v.i = t->table[table_idx].flags & DIVERSION_MASK;

		}
		if (t->table[table_idx].flags & CONSUME_NEXT_STR) {
			if ((param_idx<s->n-1) && (s->params[param_idx+1].type == SEL_PARAM_STR)) {
				param_idx++;
			} else if (!(t->table[table_idx].flags & OPTIONAL)) {
				BUG ("Mandatory STR parameter not found\n");
				goto not_found;
			}
		}
		if (t->table[table_idx].flags & CONSUME_NEXT_INT) {
			if ((param_idx<s->n-1) && (s->params[param_idx+1].type == SEL_PARAM_INT)) {
				param_idx++;
			} else if (!(t->table[table_idx].flags & OPTIONAL)) {
				BUG ("Mandatory INT parameter not found\n");
				goto not_found;
			}
		}
		if (t->table[table_idx].flags & FIXUP_CALL) {
			if (t->table[table_idx].new_f(NULL, s, NULL)<0) goto not_found;
		}

		if (t->table[table_idx].flags & NESTED) {
			if (nested < MAX_NESTED_CALLS-1) { /* need space for final function */
				s->f[nested++] = f;
			} else {
				BUG("MAX_NESTED_CALLS too small to resolve select\n");
				goto not_found;
			}
		} else {
			param_idx++;
		}
		f = t->table[table_idx].new_f;
	}

	if (t->table[table_idx].flags & SEL_PARAM_EXPECTED) {
		BUG ("final node has SEL_PARAM_EXPECTED set (no more parameters available)\n");
		goto not_found;
	}
	if (nested >= MAX_NESTED_CALLS) {
		BUG("MAX_NESTED_CALLS too small, no space for finally resolved function\n");
		goto not_found;
	}
	if ((nested>0) && (s->f[nested-1] == f)) {
		BUG("Topmost nested function equals to final function, won't call it twice\n");
	} else {
		s->f[nested] = f;
	}

	return 0;

not_found:
	return -1;
}

int run_select(str* res, select_t* s, struct sip_msg* msg)
{
	int ret, i;

	if (res == NULL) {
		BUG("Select unprepared result space\n");
		return -1;
	}
	if (s == 0) {
		BUG("Select structure is NULL\n");
		return -1;
	}
	if (s->f[0] == 0) {
		BUG("Select structure has not been resolved\n");
		return -1;
	}
	DBG("Calling SELECT %p \n", s->f);

	ret = 0;
	for (i=0; (ret == 0) && (s->f[i] !=0 ) && (i<MAX_NESTED_CALLS); i++)	{
		ret = s->f[i](res, s, msg);
	}
	return ret;
}

void print_select(select_t* s)
{
	int i;
	DBG("select(");
	for(i = 0; i < s->n; i++) {
		if (s->params[i].type == SEL_PARAM_INT) {
			DBG("%d,", s->params[i].v.i);
		} else {
			DBG("%.*s,", s->params[i].v.s.len, s->params[i].v.s.s);
		}
	}
	DBG(")\n");
}

int register_select_table(select_row_t* mod_tab)
{
	select_table_t* t;
	t=(select_table_t*)pkg_malloc(sizeof(select_table_t));
	if (!t) {
		ERR("No memory for new select_table structure\n");
		return -1;
	}

	t->table=mod_tab;
	t->next=select_list;
	select_list=t;
	return 0;
}
