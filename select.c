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
	select_f f, pf;
	int param_idx = 0;
	int table_idx = 0;
	select_table_t* t = NULL;;
	int accept = 0;
	
	f = pf = NULL;
	while (param_idx<s->n) {
		accept = 0;
		for (t=select_list; t; t=t->next) {
			table_idx = 0;	
			if (!t->table) continue;
			while (t->table[table_idx].curr_f || t->table[table_idx].new_f) {
				if (t->table[table_idx].curr_f == f) {
					if (t->table[table_idx].type == s->params[param_idx].type) {
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
					if ((t->table[table_idx].flags & IS_ALIAS)&&(!pf)) {
						accept = 1;
					}
				}
				if (accept) goto accepted;
				table_idx++;
			}
		}
		goto not_found;

		accepted:
		if (t->table[table_idx].flags & CONSUME_NEXT_STR) {
			if ((param_idx<s->n-1) && (s->params[param_idx+1].type == SEL_PARAM_STR)) {
				param_idx++;
			} else if (!(t->table[table_idx].flags & OPTIONAL)) {
				goto not_found;
			}
		}
		if (t->table[table_idx].flags & CONSUME_NEXT_INT) {
			if ((param_idx<s->n-1) && (s->params[param_idx+1].type == SEL_PARAM_INT)) {
				param_idx++;
			} else if (!(t->table[table_idx].flags & OPTIONAL)) {
				goto not_found;
			}
		}
		if (t->table[table_idx].flags & IS_ALIAS) {
			pf = f;
		} else {
			param_idx++;
		}
		f = t->table[table_idx].new_f;
	}

	if (t->table[table_idx].flags & SEL_PARAM_EXPECTED) goto not_found;
	s->f = f;
	s->parent_f = pf;
	return 0;
	
not_found:
	return -1;
}

int run_select(str* res, select_t* s, struct sip_msg* msg)
{
	if (res == NULL) {
		BUG("Select unprepared result space\n");
		return -1;
	}
	if (s == 0) {
		BUG("Select structure is NULL\n");
		return -1;
	}
	if (s->f == 0) {
		BUG("Select structure has not been resolved\n");
		return -1;
	}
DBG("Calling SELECT %p \n", s->f);
	return s->f(res, s, msg);
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
