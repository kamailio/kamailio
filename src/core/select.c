/*
 * Copyright (C) 2005-2006 iptelorg GmbH
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
 * \brief Kamailio core :: The Select framework
 * \ingroup core
 * Module: \ref core
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "select.h"
#include "dprint.h"
#include "select_core.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"

/**
 * The main parser table list placeholder
 * at startup use core table, modules can
 * add their own via register_select_table call
 */
static select_table_t *select_list = &select_core_table;

/** the level of the select call that is beeing evaluated
 * by the child process
 */
int select_level = 0;

/** pointer to the SIP uri beeing processed.
 * Nested function calls can pass information to each
 * other using this pointer. Only for performace reasons.
 * (Miklos)
 */
struct sip_uri	*select_uri_p = NULL;

/** parse a select identifier (internal version)
 * Parse select string into select structure s
 * moves pointer p to the first unused char.
 *
 * The select identifier must be of the form:
\verbatim
 *   [@] <sel_id> [ '.' <sel_id> ...]
 *   
 * Where 
 *       <sel_id> = <id> |
 *                  <id> '[' <idx> ']'
 *       <id> = [a-zA-Z0-9_]+
 *       <idx> = <number> | <string>
 *       <string> = '"' <ascii> '"' | 
 *                  '\"' <ascii> '\"'
 *
 * Examples:
 *     @to.tag
 *     @hf_value["contact"]
 *     @msg.header["SER-Server-ID"]
 *     @eval.pop[-1]
 *     contact.uri.params.maddr
 *     cfg_get.rtp_proxy.enabled 
\endverbatim
 *
 * @return -1 error
 *			  p points to the first unconsumed char
 *          0 success
 *			  p points to the first unconsumed char
 *			  s points to the select structure
 */

int w_parse_select(char**p, select_t* sel)
{
	str name;
	char* select_name;
	
	if (**p=='@') (*p)++;
	select_name=*p;
	sel->n=0;
	while (isalpha((unsigned char)*(*p))) {
		if (sel->n > MAX_SELECT_PARAMS -2) {
			LM_ERR("select depth exceeds max\n");
			goto error;
		}
		name.s=(*p);
		while (isalpha((unsigned char)*(*p)) || 
				isdigit((unsigned char)*(*p)) || (*(*p)=='_')) (*p)++;
		name.len=(*p)-name.s;
		sel->params[sel->n].type=SEL_PARAM_STR;
		sel->params[sel->n].v.s=name;
		LM_DBG("part %d: %.*s\n", sel->n, sel->params[sel->n].v.s.len, sel->params[sel->n].v.s.s);
		sel->n++;
		if (*(*p)=='[') {
			(*p)++; 
			if (*(*p)=='\\') (*p)++;
			if (*(*p)=='"') {
				(*p)++;	
				name.s=(*p);
				while ((*(*p)!='\0') && (*(*p)!='"')) (*p)++;
				if (*(*p)!='"') {
					LM_ERR("end of string is missing\n");
					goto error;
				}
				name.len=(*p)-name.s;
				if (*((*p)-1)=='\\') name.len--;
				(*p)++;
				if (*(*p)!=']') {
					LM_ERR("invalid string index, no closing ]\n");
					goto error;
				};
				(*p)++;
				sel->params[sel->n].type=SEL_PARAM_STR;
				sel->params[sel->n].v.s=name;
				LM_DBG("part %d: [\"%.*s\"]\n", sel->n, sel->params[sel->n].v.s.len, sel->params[sel->n].v.s.s);
			} else {
				name.s=(*p);
				if (*(*p)=='-') (*p)++;
				while (isdigit((unsigned char)*(*p))) (*p)++;
				name.len=(*p)-name.s;
				if (*(*p)!=']') {
					LM_ERR("invalid index, no closing ]\n");
					goto error;
				};
				(*p)++;
				sel->params[sel->n].type=SEL_PARAM_INT;
				sel->params[sel->n].v.i=atoi(name.s);
				LM_DBG("part %d: [%d]\n", sel->n, sel->params[sel->n].v.i);
			}
			sel->n++;
		}
		if (*(*p)!='.') break;
		(*p)++;
	};
	if (sel->n==0) {
		LM_ERR("invalid select '%.*s'\n", (int)(*p - select_name), select_name);
		goto error;
	};
	LM_DBG("end, total elements: %d, calling resolve_select\n", sel->n);
	if (resolve_select(sel)<0) {
		LM_ERR("error while resolve_select '%.*s'\n", (int)(*p - select_name), select_name);
		goto error;
	}
	return 0;
	
error:
	return -1;
}


/** parse a select identifier.
 * Parse select string into select structure s and
 * moves pointer p to the first unused char.
 * 
\verbatim
 * The select identifier must be of the form:
 *   [@] <sel_id> [ '.' <sel_id> ...]
 *   
 * Where 
 *       <sel_id> = <id> |
 *                  <id> '[' <idx> ']'
 *       <id> = [a-zA-Z0-9_]+
 *       <idx> = <number>  | '-' <number> | <string>
 *       <string> = '"' <ascii> '"' | 
 *                  '\"' <ascii> '\"'
 *
 * Examples:
 *     @to.tag
 *     @hf_value["contact"]
 *     @msg.header["SER-Server-ID"]
 *     @eval.pop[-1]
 *     contact.uri.params.maddr
 *     cfg_get.rtp_proxy.enabled 
\endverbatim
  *
  * @param p - double string (asciiz) pointer, *p is moved to the first char
  *            after the select identifier
  * @param s - the result will be stored here
  * @return  < 0 on error, 0 on success
  */
int parse_select (char** p, select_t** s)
{
	select_t* sel;
	
	sel=(select_t*)pkg_malloc(sizeof(select_t));
	if (!sel) {
		LM_ERR("no free memory\n");
		return -1;
	}
	memset(sel, 0, sizeof(select_t));
	if (w_parse_select(p, sel)<0) {
		pkg_free(sel);
		return -2;
	}
	*s=sel;
	return 0;
}

void free_select(select_t *s)
{
	if (s)
		pkg_free(s);
}

void shm_free_select(select_t *s)
{
	if (s)
		shm_free(s);
}

int shm_parse_select (char** p, select_t** s)
{
	select_t* sel;
	
	sel=(select_t*)shm_malloc(sizeof(select_t));
	if (!sel) {
		LM_ERR("no free shared memory\n");
		return -1;
	}
	if (w_parse_select(p, sel)<0) {
		shm_free(sel);
		return -2;
	}
	*s=sel;
	return 0;
}

int resolve_select(select_t* s)
{
	select_f f;
	int nested;
	int param_idx = 0;
	int table_idx = 0;
	select_table_t* t = NULL;
	int accept = 0;

	f = NULL;
	nested = 0;
	memset (s->f, 0, sizeof(s->f));
	while (param_idx<s->n) {
		accept = 0;
		switch (s->params[param_idx].type) {
		case SEL_PARAM_STR:
			LM_DBG("'%.*s'\n", s->params[param_idx].v.s.len, s->params[param_idx].v.s.s);
			break;
		case SEL_PARAM_INT:
			LM_DBG("[%d]\n", s->params[param_idx].v.i);
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
					if ((t->table[table_idx].flags & (NESTED | CONSUME_NEXT_INT | CONSUME_NEXT_STR)) == NESTED) {
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
				LM_ERR("Unable to resolve select '%.*s' at level %d\n", s->params[param_idx].v.s.len, s->params[param_idx].v.s.s, param_idx);
				break;
			case SEL_PARAM_INT:
				LM_ERR("Unable to resolve select [%d] at level %d\n", s->params[param_idx].v.i, param_idx);
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

		if (t->table[table_idx].flags & NESTED) {
			if (nested < MAX_NESTED_CALLS-1) { /* need space for final function */
				s->f[nested++] = f;
				s->param_offset[nested] = param_idx;
			} else {
				BUG("MAX_NESTED_CALLS too small to resolve select\n");
				goto not_found;
			}
		} else {
			param_idx++;
		}

		if (t->table[table_idx].flags & FIXUP_CALL) {
			select_level = nested;
			s->param_offset[nested+1] = param_idx;
			if (t->table[table_idx].new_f(NULL, s, NULL)<0) goto not_found;
		}

		f = t->table[table_idx].new_f;

		if (t->table[table_idx].flags & CONSUME_ALL) {
			/* sanity checks */
			if (t->table[table_idx].flags & NESTED)
				WARN("resolve_select: CONSUME_ALL should not be set "
					"together with NESTED flag!\n");
			if ((t->table[table_idx].flags & FIXUP_CALL) == 0)
				WARN("resolve_select: FIXUP_CALL should be defined "
					"if CONSUME_ALL flag is set!\n");
			break;
		}
	}

	if (t==NULL) {
		BUG ("final node not found\n");
		goto not_found;
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
		s->f[nested++] = f;
	}
	s->param_offset[nested] = s->n;

	return 0;

not_found:
	return -1;
}

int run_select(str* res, select_t* s, struct sip_msg* msg)
{
	int ret, orig_level;

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
	LM_DBG("Calling SELECT %p\n", s->f);

	/* reset the uri pointer */
	select_uri_p = NULL;

	/* save and restore the original select_level
	 * because of the nested selects */
	orig_level = select_level;
	ret = 0;
	for (	select_level=0;
		(ret == 0) && (select_level<MAX_NESTED_CALLS) && (s->f[select_level] !=0 );
		select_level++
	) {
		ret = s->f[select_level](res, s, msg);
	}
	select_level = orig_level;
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
		LM_ERR("No memory for new select_table structure\n");
		return -1;
	}

	t->table=mod_tab;
	t->next=select_list;
	select_list=t;
	return 0;
}
