/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "../../route.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../error.h"
#include "../../config.h"
#include "../../trim.h"
#include "../../select.h"
#include "../../ut.h"
#include "../../modules/xprint/xp_lib.h"
#include "../../select_buf.h"

#include "../../globals.h"
#include "../../route.h"
#include "../../parser/msg_parser.h"
#include "../../action.h"
#include "../../script_cb.h"
#include "../../dset.h"
#include "../../usr_avp.h"

MODULE_VERSION

#define MODULE_NAME "eval"

enum {evtVoid=0, evtInt, evtStr};

struct eval_str {
	str s;
	int cnt;
};

struct eval_value {
	union {
		long n;
		struct eval_str *s;
	} u;
	int type;
};

struct register_item {
	char *name;
	struct eval_value value;
	struct register_item *next;
};

struct stack_item {
	struct eval_value value;
	struct stack_item *prev;
	struct stack_item *next;
};

static int stack_no = 0;
static struct stack_item *stack_head = 0;
static struct stack_item *stack_tail = 0;

static struct register_item* registers = 0;


#define destroy_value(val) { \
	if ((val).type == evtStr && (val).u.s && (val).u.s->cnt > 0) { \
		(val).u.s->cnt--; \
		if ((val).u.s->cnt == 0) \
			pkg_free((val).u.s); \
	} \
	(val).type = evtVoid; \
}

#define assign_value(dest, src) { \
	if (&(dest) != &(src)) { \
		destroy_value(dest); \
		dest = src; \
		if ((dest).type == evtStr && (dest).u.s && (dest).u.s->cnt > 0) \
			(dest).u.s->cnt++; \
	} \
}

static int get_as_int(struct eval_value *value, long* val) {
	switch (value->type) {
		case evtInt:
			*val = value->u.n;
			return 1;
		case evtStr:
			if (value->u.s->s.s && value->u.s->s.len && value->u.s->s.len <= 25) {
				char *err;
				char buf[25+1];
				memcpy(buf, value->u.s->s.s, value->u.s->s.len);
				buf[value->u.s->s.len] = '\0';
				*val = strtol(buf, &err, 10);
				if (*err == 0)
					return 1;
			}
			ERR(MODULE_NAME": cannot convert '%.*s' as int\n", value->u.s->s.len, value->u.s->s.s);
			return -1;
		default:
			BUG("Bad value type %d\n", value->type);
			return -1;
	}
}

static void get_as_str(struct eval_value *value, str *s) {
	static char buf[25];
	switch (value->type) {
		case evtInt:
			s->len = snprintf(buf, sizeof(buf)-1, "%ld", value->u.n);
			s->s = buf;
			break;
		case evtStr:
			*s = value->u.s->s;
			break;
		default:
			s->s = 0;
			s->len = 0;
			break;
	}
}

static int get_as_bool(struct eval_value *value) {
	switch (value->type) {
		case evtVoid:
			return 0;
		case evtInt:
			return value->u.n != 0;
		case evtStr:
			return (value->u.s->s.s && value->u.s->s.len > 0);
		default:
			BUG("Bad value type %d\n", value->type);
			return -1;
	}
}

static struct eval_str* eval_str_malloc(str* s) {
	struct eval_str* p;
	p = pkg_malloc(sizeof(*p)+s->len);
	if (p) {
		p->s.s = (char*)p+sizeof(*p);
		if (s->len && s->s != 0)
			memcpy(p->s.s, s->s, s->len);
		if (s->s == 0 && s->len)
			s->s = p->s.s;
		p->s.len = s->len;
		p->cnt = 1;
	}
	return p;
}

/* taken from modules/textops */
#define is_space(_p) ((_p) == '\t' || (_p) == '\n' || (_p) == '\r' || (_p) == ' ')

static void get_uri_and_skip_until_params(str *param_area, str *uri) {
	int i, quoted, uri_pos, uri_done;

	uri->len = 0;
	uri->s = 0;
	uri_done = 0;
	for (i=0; i<param_area->len && param_area->s[i]!=';'; ) {	/* [ *(token LSW)/quoted-string ] "<" addr-spec ">" | addr-spec */
		/* skip name */

		for (quoted=0, uri_pos=i; i<param_area->len; i++) {
			if (!quoted) {
				if (param_area->s[i] == '\"') {
					quoted = 1;
					uri_pos = -1;
				}
				else if (param_area->s[i] == '<' || param_area->s[i] == ';' || is_space(param_area->s[i])) break;
			}
			else if (param_area->s[i] == '\"' && param_area->s[i-1] != '\\') quoted = 0;
		}
		if (uri_pos >= 0 && !uri_done) {
			uri->s = param_area->s+uri_pos;
			uri->len = param_area->s+i-uri->s;
		}
		/* skip uri */
		while (i<param_area->len && is_space(param_area->s[i])) i++;
		if (i<param_area->len && param_area->s[i]=='<') {
			uri->s = param_area->s+i;
			uri->len = 0;
			for (quoted=0; i<param_area->len; i++) {
				if (!quoted) {
					if (param_area->s[i] == '\"') quoted = 1;
					else if (param_area->s[i] == '>') {
						uri->len = param_area->s+i-uri->s+1;
						uri_done = 1;
						break;
					}
				}
				else if (param_area->s[i] == '\"' && param_area->s[i-1] != '\\') quoted = 0;
			}
		}
	}
        param_area->s+= i;
	param_area->len-= i;
}

static int find_next_value(char** start, char* end, str* val, str* lump_val) {
	int quoted = 0;
	lump_val->s = *start;
	while (*start < end && is_space(**start) ) (*start)++;
	val->s = *start;
	while ( *start < end && (**start != ',' || quoted) ) {
		if (**start == '\"' && (!quoted || (*start)[-1]!='\\') )
			quoted = ~quoted;
		(*start)++;
	}
	val->len = *start - val->s;
	while (val->len > 0 && is_space(val->s[val->len-1])) val->len--;
/* we cannot automatically strip quotes!!! an example why: "name" <sip:ssss>;param="bar"
	if (val->len >= 2 && val->s[0] == '\"' && val->s[val->len-1] == '\"') {
		val->s++;
		val->len -= 2;
	}
*/
	while (*start < end && **start != ',') (*start)++;
	if (*start < end) {
		(*start)++;
	}
	lump_val->len = *start - lump_val->s;
	return (*start < end);
}

#define MAX_HF_VALUES 30

static int parse_hf_values(str s, int* n, str** vals) {
	static str values[MAX_HF_VALUES];
	char *start, *end;
	str lump_val;
	*n = 0;
	*vals = values;
	if (!s.s) return 1;
	start = s.s;
	end = start+s.len;
	while (start < end) {
		find_next_value(&start, end, &values[*n], &lump_val);
		if (*n >= MAX_HF_VALUES) {
			ERR(MODULE_NAME": too many values\n");
			return -1;
		}
		(*n)++;
	}
	return 1;
}

static void destroy_stack() {
	struct stack_item *p;
	while (stack_head) {
		destroy_value(stack_head->value);
		p = stack_head;
		stack_head = stack_head->next;
		pkg_free(p);
	}
	stack_tail = stack_head;
	stack_no = 0;
}

static void destroy_register_values() {
	struct register_item *p;
	for (p=registers; p; p=p->next) {
		destroy_value(p->value);
	}
}

static void remove_stack_item(struct stack_item *s) {
	if (s->prev)
		s->prev->next = s->next;
	else
		stack_head = s->next;
	if (s->next)
		s->next->prev = s->prev;
	else
		stack_tail = s->prev;
	destroy_value(s->value);
	pkg_free(s);
	stack_no--;
}

static void insert_stack_item(struct stack_item *s, struct stack_item *pivot, int behind) {
	if (stack_head == NULL) {
		s->prev = s->next = 0;
	}
	else if (behind) {
		if (pivot) {
			s->next = pivot->next;
			s->prev = pivot;
		}
		else {
			s->next = 0;
			s->prev = stack_tail;  /* bottom (tail) */
		}
	}
	else {
		if (pivot) {
			s->prev = pivot->prev;
			s->next = pivot;
		}
		else {
			s->next = stack_head;  /* top (head) */
			s->prev = 0;
		}
	}
	if (!s->prev)
		stack_head = s;
	else
		s->prev->next = s;
	if (!s->next)
		stack_tail = s;
	else
		s->next->prev = s;
	stack_no++;
}

static int declare_register(modparam_t type, char* param) {
	struct register_item **p;
	char *c;
	for (c=param; *c; c++) {
		if (	(*c >= 'a' && *c <= 'z') ||
			(*c >= 'A' && *c <= 'Z') ||
			(*c >= '0' && *c <= '9') ||
			(*c == '_') ) {
			;
		} else {
			ERR(MODULE_NAME": illegal register name\n");
			return E_CFG;
		}
	}
	for (p = &registers; *p!= 0; p = &(*p)->next);
	*p = pkg_malloc(sizeof(**p));
	if (!*p) return E_OUT_OF_MEM;

	memset(*p, 0, sizeof(**p));
	(*p)->name = param;
	return 0;
}

static int mod_pre_script_cb(struct sip_msg *msg, unsigned int flags, void *param) {
	destroy_stack();
	destroy_register_values();
	return 1;
}

static struct register_item* find_register(char* s, int len) {
	struct register_item *p;
	for (p=registers; p; p=p->next) {
		if (strlen(p->name) == len && strncasecmp(p->name, s, len) == 0)
			break;
	}
	return p;
}

static struct stack_item* find_stack_item(int n) {
	struct stack_item *p;
	if ((n >= 0 && n >= stack_no) || (n<0 && -n > stack_no)) {
		return NULL;
	}
	p = NULL;
	if (n >= 0) {
		for (p = stack_head; p && n>0; p=p->next, n--);
	}
	else {
		for (p = stack_tail, n=-n-1; p && n>0; p=p->prev, n--);
	}
	return p;
}

/* module exported functions */
static void print_eval_value(struct eval_value* v) {
	switch (v->type) {
		case evtStr:
			if (v->u.s)
				fprintf(stderr, "s:'%.*s', cnt:%d\n", v->u.s->s.len, v->u.s->s.s, v->u.s->cnt);
			else
				fprintf(stderr, "s:<null>\n");
			break;
		case evtInt:
			fprintf(stderr, "i:%ld\n", v->u.n);
			break;
		default:;
			fprintf(stderr, "type:%d\n", v->type);
			break;
	}
}

static int eval_dump_func(struct sip_msg *msg, char *param1, char *param2) {
	struct stack_item *si;
	struct register_item *ri;
	int i;
	fprintf(stderr, "Stack (no=%d):\n", stack_no);
	for (si=stack_head, i=0; si; si=si->next, i++) {
		fprintf(stderr, "# %.2d ", i);
		print_eval_value(&si->value);
	}
	for (si=stack_tail, i=-1; si; si=si->prev, i--) {
		fprintf(stderr, "#%.2d ", i);
		print_eval_value(&si->value);
	}
	fprintf(stderr, "Registers:\n");
	for (ri=registers; ri; ri=ri->next) {
	        fprintf(stderr, "%s: ", ri->name);
		print_eval_value(&ri->value);
	}
	return 1;
}


static int xlbuf_size = 4096;
static xl_print_log_f* xl_print = NULL;
static xl_parse_format_f* xl_parse = NULL;
#define NO_SCRIPT -1


enum {esotAdd, esotInsert, esotXchg, esotPut, esotGet, esotPop, esotAddValue, esotInsertValue};
enum {esovtInt, esovtStr, esovtAvp, esovtXStr, esovtRegister, esovtFunc, esovtSelect};
enum {esofNone=0, esofTime, esofUuid, esofStackNo};

struct eval_location_func {
	int type;
	char *name;
};

static struct eval_location_func loc_functions[] = {
	{esofTime, "time"},
	{esofUuid, "uuid"},
	{esofStackNo, "stackno"},

	{esofNone, NULL}
};

struct eval_location {
	int value_type;
	union {
		int n;
		struct eval_str s;
		xl_elog_t* xl;
		struct register_item *reg;
		avp_ident_t avp;
		select_t* select;
		struct eval_location_func *func;
	} u;
};

struct eval_stack_oper {
	int oper_type;
	struct eval_location loc;
};

static int parse_location(str s, struct eval_location *p) {
	if (s.len >= 2 && s.s[1] == ':') {
		switch (s.s[0]) {
			case 'r':
				p->u.reg = find_register(s.s+2, s.len-2);
				if (!p->u.reg) {
					ERR(MODULE_NAME": register '%.*s' not found\n", s.len-2, s.s+2);
					return E_CFG;
				}
				p->value_type = esovtRegister;
				break;
			case 'x':
				if (!xl_print) {
					xl_print=(xl_print_log_f*)find_export("xprint", NO_SCRIPT, 0);
					if (!xl_print) {
						ERR(MODULE_NAME": cannot find \"xprint\", is module xprint loaded?\n");
						return E_UNSPEC;
					}
				}

				if (!xl_parse) {
					xl_parse=(xl_parse_format_f*)find_export("xparse", NO_SCRIPT, 0);

					if (!xl_parse) {
						ERR(MODULE_NAME": cannot find \"xparse\", is module xprint loaded?\n");
						return E_UNSPEC;
					}
				}

				if(xl_parse(s.s+2, &p->u.xl) < 0) {
					ERR(MODULE_NAME": wrong xl_lib format '%s'\n", s.s+2);
					return E_UNSPEC;
				}
				p->value_type = esovtXStr;
				break;
			case 'f': {
				struct eval_location_func* f;
				s.s += 2;
				s.len -= 2;
				for (f=loc_functions; f->type != esofNone; f++) {
					if (strlen(f->name)==s.len && strncasecmp(s.s, f->name, s.len) == 0) {
						p->value_type = esovtFunc;
						p->u.func = f;
						break;
					}
				}
				if (!f) {
					ERR(MODULE_NAME": unknown function '%.*s'\n", s.len, s.s);
					return E_CFG;
				}
				break;
			}
			case 's':
				s.s += 2;
				s.len -= 2;
				/* no break */
			default:
				p->u.s.s = s;
				p->u.s.cnt = 0;
				p->value_type = esovtStr;
				break;
		}
	}
	else {
		char *err;
		if (s.len > 1 && s.s[0]=='$') {
			s.s++;
			s.len--;
			if (parse_avp_ident(&s, &p->u.avp) == 0) {
				if (p->u.avp.flags & AVP_NAME_RE) {
					ERR(MODULE_NAME": avp regex not allowed\n");
					return E_CFG;
				}
				p->value_type = esovtAvp;
				return 1;
			}
			s.s--;
			s.len++;
		}
		else if (s.len > 1 && s.s[0]=='@') {
			if (parse_select(&s.s, &p->u.select) >= 0) {
				p->value_type = esovtSelect;
				return 1;
			}
		}
		p->u.n = strtol(s.s, &err, 10);
		if (*err) {
			p->u.s.s = s;
			p->u.s.cnt = 0;
			p->value_type = esovtStr;
		}
		else {
			p->value_type = esovtInt;
		}
	}
	return 1;
}

static int eval_xl(struct sip_msg *msg, xl_elog_t* xl, str* s) {
	static char *xlbuf=NULL;
	int xllen = 0;

	if (!xlbuf) {
		xlbuf = (char*) pkg_malloc((xlbuf_size+1)*sizeof(char));
		if (!xlbuf) {
			ERR(MODULE_NAME": eval_xl: No memory left for format buffer\n");
			return E_OUT_OF_MEM;
		}
	}
	xllen = xlbuf_size;
	if (xl_print(msg, xl, xlbuf, &xllen) < 0) {
		ERR(MODULE_NAME": eval_xl: Error while formatting result\n");
		return E_UNSPEC;
	}
	s->s = xlbuf;
	s->len = xllen;
	return 1;
}

SELECT_F(select_sys_unique)

static int eval_location(struct sip_msg *msg, struct eval_location* so, struct eval_value* v, int get_static_str) {
	static struct eval_str ss;

	v->type = evtVoid;
	switch (so->value_type) {
		case esovtInt:
			v->type = evtInt;
			v->u.n = so->u.n;
			break;
		case esovtStr:
			v->type = evtStr;
			v->u.s = &so->u.s;
			break;
		case esovtXStr: {
			str s;
			int ret;
			ret = eval_xl(msg, so->u.xl, &s);
			if (ret < 0) return ret;
			if (get_static_str) {
				ss.s = s;
				ss.cnt = 0;
				v->u.s = &ss;
			}
			else {
				v->u.s = eval_str_malloc(&s);
				if (!v->u.s) {
					ERR(MODULE_NAME": out of memory to allocate xl string\n");
					return E_OUT_OF_MEM;
				}
			}
			v->type = evtStr;
			break;
		}
		case esovtRegister:
			if (get_static_str)
				*v = so->u.reg->value;  /* do not incement cnt */
			else
				assign_value(*v, so->u.reg->value);
			break;
		case esovtAvp: {
			avp_t* avp;
			avp_value_t val;

			if (so->u.avp.flags & AVP_INDEX_ALL)
				avp = search_first_avp(so->u.avp.flags & ~AVP_INDEX_ALL, so->u.avp.name, &val, NULL);
			else
				avp = search_avp_by_index(so->u.avp.flags, so->u.avp.name, &val, so->u.avp.index);
			if (!avp) {
				ERR(MODULE_NAME": avp '%.*s'[%d] not found\n", so->u.avp.name.s.len, so->u.avp.name.s.s, so->u.avp.index);
				return -1;
			}
			if (avp->flags & AVP_VAL_STR) {
				if (get_static_str) {
					ss.s = val.s;
					ss.cnt = 0;
					v->u.s = &ss;
				}
				else {
					v->u.s = eval_str_malloc(&val.s);
					if (!v->u.s) {
						ERR(MODULE_NAME": out of memory to allocate avp string\n");
						return E_OUT_OF_MEM;
					}
				}
				v->type = evtStr;
			}
			else {
				v->type = evtInt;
				v->u.n = val.n;
			}
			break;
		}
		case esovtSelect: {
			str s;
			int ret = run_select(&s, so->u.select, msg);
			if (ret < 0 || ret > 0) return -1;
			if (get_static_str) {
				ss.s = s;
				ss.cnt = 0;
				v->u.s = &ss;
			}
			else {
				v->u.s = eval_str_malloc(&s);
				if (!v->u.s) {
					ERR(MODULE_NAME": out of memory to allocate select string\n");
					return E_OUT_OF_MEM;
				}
			}
			v->type = evtStr;
			break;
		}
		case esovtFunc: {
			switch (so->u.func->type) {
			        case esofTime: {
					time_t stamp;
					stamp = time(NULL);
					v->type = evtInt;
					v->u.n = stamp;
					break;
				}
				case esofUuid: {
					str s;
					select_sys_unique(&s, 0, msg);
					if (get_static_str) {
						ss.s = s;
						ss.cnt = 0;
						v->u.s = &ss;
					}
					else {
						v->u.s = eval_str_malloc(&s);
						if (!v->u.s) {
							ERR(MODULE_NAME": out of memory to allocate uuid string\n");
							return E_OUT_OF_MEM;
						}
					}
					v->type = evtStr;
					break;
        			}
				case esofStackNo:
					v->type = evtInt;
					v->u.n = stack_no;
					break;
				default:
					BUG("bad func type (%d)\n", so->u.func->type);
					return -1;
			}
			break;
		}
		default:
			BUG("Bad value type (%d)\n", so->value_type);
			return -1;
	}
	return 1;
}

static int fixup_location_12( void** param, int param_no) {
	struct eval_location *so;
	str s;
	s.s = *param;
	s.len = strlen(s.s);
	so = pkg_malloc(sizeof(*so));
	if (!so) return E_OUT_OF_MEM;
	if (parse_location(s, so) < 0) {
		ERR(MODULE_NAME": parse location error '%s'\n", s.s);
		return E_CFG;
	}
	*param = so;
	return 0;
}

static int fixup_stack_oper(void **param, int param_no, int oper_type) {
	str s;
	struct eval_stack_oper *p;
	int ret;

	if (param_no == 2) {
		return fixup_location_12(param, param_no);
	}
	p = pkg_malloc(sizeof(*p));
	if (!p) return E_OUT_OF_MEM;
	p->oper_type = oper_type;
	s.s = *param;
	s.len = strlen(s.s);
	*param = p;
	ret = parse_location(s, &p->loc);
	if (ret < 0) return ret;

	switch (p->oper_type) {
		case esotXchg:
			if (p->loc.value_type == esovtAvp || p->loc.value_type == esovtSelect) {
				ERR(MODULE_NAME": avp non supported for xchg\n");
				return E_CFG;
			}
			/* no break */
		case esotPop:
		case esotGet:
			if (p->loc.value_type != esovtRegister && p->loc.value_type != esovtAvp) {
				ERR(MODULE_NAME": non supported read only location\n");
				return E_CFG;
			}
			break;
		default:;
	}
	return 0;
}

static int eval_stack_oper_func(struct sip_msg *msg, char *param1, char *param2) {
	int ret, idx;
	struct stack_item *pivot;
	struct eval_stack_oper *so;
	struct run_act_ctx ra_ctx;

	so = (struct eval_stack_oper *)param1;
	if (param2) {
		long l;
		struct eval_value v;
		eval_location(msg, (struct eval_location*) param2, &v, 1);
		ret = get_as_int(&v, &l);
		if (ret < 0) return ret;
		idx = l;
	}
	else {
		switch (so->oper_type) {  /* default values */
			case esotAdd:
			case esotAddValue:
				idx = -1;
				break;
			default:
				idx = 0;
				break;
		}
	}

	pivot = find_stack_item(idx);
	if ( !(pivot!=NULL || ((so->oper_type == esotAdd || so->oper_type == esotAddValue) && idx == -1) || ((so->oper_type == esotInsert || so->oper_type == esotInsertValue) && idx == 0)) )
		return -1;

	switch (so->oper_type) {
		case esotGet:
		case esotPop:
			switch (so->loc.value_type) {
				case esovtRegister:
					assign_value(so->loc.u.reg->value, pivot->value);
					if (so->oper_type == esotPop)
						remove_stack_item(pivot);
					return 1;
				case esovtAvp: {
					struct action a;
					avp_spec_t attr;

					a.type = ASSIGN_T;
					a.count = 2;
					a.val[0].type = AVP_ST;
					attr.type = so->loc.u.avp.flags;
					attr.name = so->loc.u.avp.name;
					attr.index = so->loc.u.avp.index;
					a.val[0].u.attr = &attr;
					switch (pivot->value.type) {
						case evtInt:
							a.val[1].type = NUMBER_ST;
							a.val[1].u.number = pivot->value.u.n;
							break;
						case evtStr:
							if (pivot->value.u.s)
								a.val[1].u.str = pivot->value.u.s->s;
							else
								a.val[1].u.str.len = 0;
							a.val[1].type = STRING_ST;
							break;
						default:
							return -1;
					}
					a.next = 0;
					init_run_actions_ctx(&ra_ctx);
					ret = do_action(&ra_ctx, &a, msg);
					if (so->oper_type == esotPop)
						remove_stack_item(pivot);
					return ret<0?-1:1;
				}
				default:
					BUG("Bad value type (%d) for get/pop\n", so->loc.value_type);
					return -1;
			}
			break;
		case esotXchg:
			switch (so->loc.value_type) {
				case esovtRegister: {
					struct eval_value v;

					v = so->loc.u.reg->value;
					so->loc.u.reg->value = pivot->value;
					pivot->value = v;
					return 1;
				}
				default:
					BUG("Bad value type (%d) for xchg\n", so->loc.value_type);
					return -1;
			}
			break;
		case esotInsert:
		case esotAdd:
		case esotPut: {
			struct eval_value v;
			eval_location(msg, &so->loc, &v, 0);

			if (so->oper_type == esotInsert || so->oper_type == esotAdd) {
				struct stack_item *si;
				si = pkg_malloc(sizeof(*si));
				if (!si) {
					ERR(MODULE_NAME": out of memory\n");
					destroy_value(v);
					return -1;
				}
				si->value = v;
				insert_stack_item(si, pivot, so->oper_type == esotAdd);
				return 1;
			}
			else {
				destroy_value(pivot->value);
				pivot->value = v;
				return 1;
			}
			break;
		}
		case esotInsertValue:
		case esotAddValue: {
			struct eval_value v;
			str s, *vals;
			int i, n;
			struct eval_str* es;
			struct stack_item *si;
			eval_location(msg, &so->loc, &v, 0);
			get_as_str(&v, &s);
			if ((parse_hf_values(s, &n, &vals) < 0) || n == 0) {
				destroy_value(v);
				return -1;
			}
			si = pkg_malloc(sizeof(*si));
			if (!si) {
				ERR(MODULE_NAME": out of memory\n");
				destroy_value(v);
				return -1;
			}
			si->value.type = evtInt;
			si->value.u.n = n;
			insert_stack_item(si, pivot, so->oper_type == esotAddValue);
			pivot = si;
			for (i=0; i<n; i++) {
				si = pkg_malloc(sizeof(*si));
				if (!si) {
					ERR(MODULE_NAME": out of memory\n");
					destroy_value(v);
					return -1;
				}
				es = eval_str_malloc(vals+i);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					destroy_value(v);
					return -1;
				}
				si->value.type = evtStr;
				si->value.u.s = es;
				insert_stack_item(si, pivot, 1);
				pivot = si;
			}
			destroy_value(v);
			return 1;
		}
		default:
			BUG("Unexpected operation (%d)\n", so->oper_type);
			return -1;
	}
}

static int eval_add_fixup( void** param, int param_no) {
	return fixup_stack_oper(param, param_no, esotAdd);
}

static int eval_insert_fixup( void** param, int param_no) {
	return fixup_stack_oper(param, param_no, esotInsert);
}

static int eval_put_fixup( void** param, int param_no) {
	return fixup_stack_oper(param, param_no, esotPut);
}

static int eval_get_fixup( void** param, int param_no) {
	return fixup_stack_oper(param, param_no, esotGet);
}

static int eval_pop_fixup( void** param, int param_no) {
	return fixup_stack_oper(param, param_no, esotPop);
}

static int eval_xchg_fixup( void** param, int param_no) {
	return fixup_stack_oper(param, param_no, esotXchg);
}

static int eval_add_value_fixup( void** param, int param_no) {
	return fixup_stack_oper(param, param_no, esotAddValue);
}

static int eval_insert_value_fixup( void** param, int param_no) {
	return fixup_stack_oper(param, param_no, esotInsertValue);
}


static int eval_remove_func(struct sip_msg *msg, char *param1, char *param2) {
	struct stack_item *p, *p2;
	int ret, len, start;
	struct eval_value v;

	if (param1) {
		long l;
		eval_location(msg, (struct eval_location*) param1, &v, 1);
		ret = get_as_int(&v, &l);
		if (ret < 0) return ret;
		start = l;
	}
	else
		start = 0;
	p = find_stack_item(start);
	if (p) {
		if (param2) {
			long l;
			eval_location(msg, (struct eval_location*) param2, &v, 1);
			ret = get_as_int(&v, &l);
			if (ret < 0) return ret;
			len = l;
		}
		else
			len = 1;

		if (start < 0) {
			start = stack_no + start;
			if (start < 0) start = 0;
		}
		else {
			if (start > stack_no) start = stack_no;
		}
		if (len < 0) {
			len = stack_no - start + len;
			if (len < 0)
				len = 0;
		}
		else {
			if (start + len > stack_no)
				len = stack_no - start;
		}

		for (; len > 0 && p; len--) {
			p2 = p;
			p = p->next;
			remove_stack_item(p2);
		}
		return 1;
	}
	else
		return -1;
}

static int eval_clear_func(struct sip_msg *msg, char *param1, char *param2) {
	int n;
	if (get_int_fparam(&n, msg, (fparam_t*)param1)<0) {
		ERR(MODULE_NAME": eval_clear: Invalid number specified\n");
		return -1;
	}
	if (n & 1)
		destroy_stack();
	if (n & 2)
		destroy_register_values();
	return 1;
}

enum {esftNone=0, esftAdd, esftSub, esftMultiplication, esftDivision, esftModulo, esftNeg, esftAbs, esftSgn, esftDec, esftInc,
esftConcat, esftSubstr, esftStrLen, esftStrStr, esftStrDel, esftStrUpper, esftStrLower,
esftCastAsInt, esftCastAsStr,
esftValueAt, esftValueUris, esftValueRev, esftSubValue, esftValueCount, esftValueConcat, esftStrValueAt,
esftGetUri,
esftAnd, esftOr, esftNot, esftBitAnd, esftBitOr, esftBitNot, esftBitXor, esftEQ, esftNE, esftGT, esftGE, esftLW, esftLE};

struct eval_function_def {
	int type;
	char *name;
	int arg_no;
};

static struct eval_function_def eval_functions[] = {
	{esftAdd, "+", 2},
	{esftSub, "-", 2},
	{esftMultiplication, "*", 2},
	{esftDivision, "/", 2},
	{esftModulo, "%", 2},
	{esftNeg, "neg", 1},
	{esftAbs, "abs", 1},
	{esftDec, "dec", 1},
	{esftInc, "inc", 1},
	{esftSgn, "sgn", 1},
	{esftConcat, "concat", 2},
	{esftSubstr, "substr", 3},
	{esftStrLen, "strlen", 1},
	{esftStrStr, "strstr", 2},
	{esftStrDel, "strdel", 3},
	{esftStrUpper, "strupper", 1},
	{esftStrLower, "strlower", 1},
	{esftCastAsInt, "(int)", 1},
	{esftCastAsStr, "(str)", 1},
	{esftValueAt, "valat", 2},
	{esftValueUris, "valuris", 1},
	{esftValueRev, "valrev", 1},
	{esftSubValue, "subval", 3},
	{esftValueCount, "valcount", 1},
	{esftValueConcat, "valconcat", 1},
	{esftStrValueAt, "strvalat", 2},
	{esftGetUri, "geturi", 1},
	{esftAnd, "&&", 2},
	{esftOr, "||", 2},
	{esftNot, "!", 1},
	{esftBitAnd, "&", 2},
	{esftBitOr, "|", 2},
	{esftBitNot, "~", 1},
	{esftBitXor, "^", 2},

	{esftEQ, "==", 2},
	{esftNE, "!=", 2},
	{esftGT, ">", 2},
	{esftGE, ">=", 2},
	{esftLW, "<", 2},
	{esftLE, "<=", 2},

	{esftNone, NULL}
};

struct eval_function {
	int resolved;  /* is oper.d valid ? */
	union {
		struct eval_function_def *d;
		struct eval_location loc;
	} oper;
	struct eval_function* next;
};

static int eval_stack_func_fixup( void** param, int param_no) {
	char *c,  *c2;
	struct eval_function_def* d;
	struct eval_function **p, *head;
	if (param_no == 2) {
		return fixup_location_12(param, param_no);
	}

	head = 0;
	p = &head;
	c = *param;
	while (*c) {
		str s;
		struct eval_location so;
		while( (*c<=' ' || *c == ',') && *c ) c++;
		if (*c == '\0')
			break;
		c2 = c;
		while (*c && *c!=',') c++;
		while (c > c2 && *(c-1) <= ' ') c--;

		s.s = c2;
		s.len = c-c2;

		if (parse_location(s, &so) < 0) {
			ERR(MODULE_NAME": parse operation error near '%s'\n", c2);
			return E_CFG;
		}
		*p = pkg_malloc(sizeof(**p));
		if (!*p) return E_OUT_OF_MEM;
		(*p)->next = 0;
		switch (so.value_type) {
			case esovtStr:
				for (d=eval_functions; d->type; d++) {
					if (strlen(d->name) == so.u.s.s.len && strncasecmp(d->name, so.u.s.s.s, so.u.s.s.len)==0) {
						(*p)->oper.d = d;
						break;
					}
				}
				if (!d->type) {
					ERR(MODULE_NAME": unknown eval function near '%s'\n", so.u.s.s.s);
					return E_CFG;
				}
				(*p)->resolved = 1;
				break;
			case esovtAvp:
			case esovtXStr:
			case esovtRegister:
			case esovtSelect:
			case esovtFunc:
				(*p)->oper.loc = so;
				(*p)->resolved = 0;
				break;
			default:
				ERR(MODULE_NAME": location %d not allowed\n", so.value_type);
				return E_CFG;
		}
		p = &(*p)->next;

	}
	*param = head;
	return 0;
}

#ifndef _GNU_SOURCE
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
#endif

static int eval_stack_func_func(struct sip_msg *msg, char *param1, char *param2) {
	struct eval_function *f;
	struct stack_item *pivot;
	struct eval_function_def *d;
	int stack_idx = 0;
	int ret = -1;

	if (param2) {
		long l;
		int ret;
		struct eval_value v;
		eval_location(msg, (struct eval_location*) param2, &v, 1);
		ret = get_as_int(&v, &l);
		if (ret < 0) return ret;
		stack_idx = l;
	}

	for (f = (struct eval_function*) param1; f; f=f->next, ret = 1) {
		if (f->resolved) {
			d = f->oper.d;
		}
		else {
			str fn;
			struct eval_value v;
			eval_location(msg, &f->oper.loc, &v, 1);
			get_as_str(&v, &fn);
			for (d=eval_functions; d->type; d++) {
				if (strlen(d->name) == fn.len && strncasecmp(d->name, fn.s, fn.len)==0) {
					break;
				}
			}
			if (!d->type) {
				ERR(MODULE_NAME": unknown eval function '%.*s'\n", fn.len, fn.s);
				return -1;
			}
		}
		DEBUG(MODULE_NAME": eval_oper: %s, stack_idx: %d, stack_no: %d\n", d->name, stack_idx, stack_no);
		if ( ((stack_idx >= 0) && (stack_idx+d->arg_no > stack_no)) ||
		     ((stack_idx <  0) && (stack_no+stack_idx < 0 || stack_no+stack_idx+d->arg_no > stack_no)) ) {
			ERR(MODULE_NAME": operation out of stack range\n");
			return -1;
		}
		pivot = find_stack_item(stack_idx);
		if (!pivot) {
			BUG("stack test error\n");
			return -1;
		}
		switch (d->type) {

			case esftAdd:
			case esftSub:
			case esftMultiplication:
			case esftDivision:
			case esftModulo:
			case esftAnd:
			case esftOr:
			case esftBitAnd:
			case esftBitOr:
			case esftBitXor: {
				long a, b;
				if (get_as_int(&pivot->value, &a) < 0) return -1;
				if (get_as_int(&pivot->next->value, &b) < 0) return -1;
				switch (d->type) {
					case esftAdd:
						a = a + b;
						break;
					case esftSub:
						a = a - b;
						break;
					case esftMultiplication:
						a = a * b;
						break;
					case esftDivision:
						if (b == 0) {
							ERR(MODULE_NAME": division by zero\n");
							return -1;
						}
						a = a / b;
						break;
					case esftModulo:
						if (b == 0) {
							ERR(MODULE_NAME": division by zero\n");
							return -1;
						}
						a = a % b;
						break;
					case esftAnd:
						a = a && b;
						break;
					case esftOr:
						a = a || b;
						break;
					case esftBitAnd:
						a = a & b;
						break;
					case esftBitOr:
						a = a | b;
						break;
					case esftBitXor:
						a = a ^ b;
						break;
				}
				destroy_value(pivot->value);
				pivot->value.type = evtInt;
				pivot->value.u.n = a;
				remove_stack_item(pivot->next);
				break;
			}
			case esftNeg:
			case esftAbs:
			case esftSgn:
			case esftDec:
			case esftInc:
			case esftNot:
			case esftBitNot:
			case esftCastAsInt: {
				long a;
				if (get_as_int(&pivot->value, &a) < 0) return -1;
				switch (d->type) {
					case esftNeg:
						a = -a;
						break;
					case esftAbs:
						a = abs(a);
						break;
					case esftSgn:
						if (a < 0)
							a = -1;
						else if (a > 0)
							a = 1;
						else
							a = 0;
						break;
					case esftDec:
						a--;
						break;
					case esftInc:
						a++;
						break;
					case esftNot:
						a = !a;
						break;
					case esftBitNot:
						a = ~a;
						break;
					case esftCastAsInt:
						break;
				}
				destroy_value(pivot->value);
				pivot->value.type = evtInt;
				pivot->value.u.n = a;
				break;
			}
			case esftCastAsStr:
				if (pivot->value.type != evtStr) {
					str s;
					get_as_str(&pivot->value, &s);
					destroy_value(pivot->value);
					pivot->value.u.s = eval_str_malloc(&s);
					if (!pivot->value.u.s) {
						ERR(MODULE_NAME": out of memory\n");
						return -1;
					}
					pivot->value.type = evtStr;
				}
				break;
			case esftEQ:
			case esftNE:
			case esftGT:
			case esftGE:
			case esftLW:
			case esftLE: {
				long a;
				if (pivot->value.type == evtStr || pivot->next->value.type == evtStr) {
					str s1, s2;
					int l;
					get_as_str(&pivot->value, &s1);
					get_as_str(&pivot->next->value, &s2);
					l = (s1.len < s2.len)?s1.len:s2.len;
					if (l > 0)
						a = strncasecmp(s1.s, s2.s, l);
					else
						a = 0;
					switch (d->type) {
						case esftEQ:
							a = a == 0 && s1.len == s2.len;
							break;
						case esftNE:
							a = a != 0 || s1.len != s2.len;
							break;
						case esftGT:
							a = a > 0 || (a == 0 && s1.len > s2.len);
							break;
						case esftGE:
							a = a > 0 || (a == 0 && s1.len >= s2.len);
							break;
						case esftLW:
							a = a < 0 || (a == 0 && s1.len < s2.len);
							break;
						case esftLE:
							a = a < 0 || (a == 0 && s1.len <= s2.len);
							break;
					}
				}
				else {
					long b;
					if (get_as_int(&pivot->value, &a) < 0) return -1;
					if (get_as_int(&pivot->next->value, &b) < 0) return -1;
					switch (d->type) {
						case esftEQ:
							a = a == b;
							break;
						case esftNE:
							a = a != b;
							break;
						case esftGT:
							a = a > b;
							break;
						case esftGE:
							a = a >= b;
							break;
						case esftLW:
							a = a < b;
							break;
						case esftLE:
							a = a <= b;
							break;
					}
				}
				destroy_value(pivot->value);
				pivot->value.type = evtInt;
				pivot->value.u.n = a;
				remove_stack_item(pivot->next);
				break;
			}
			case esftConcat: {
				char buf[25];
				str s, s1, s2;
				struct eval_str* es;
				get_as_str(&pivot->value, &s1);
				if (pivot->value.type == evtInt && pivot->next->value.type == evtInt) {
					memcpy(buf, s1.s, s1.len);  /* result in static buffer */
					s1.s = buf;
				}
				get_as_str(&pivot->next->value, &s2);
				s.len = s1.len + s2.len;
				s.s = 0;
				es = eval_str_malloc(&s);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				memcpy(s.s, s1.s, s1.len);
				memcpy(s.s+s1.len, s2.s, s2.len);
				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				remove_stack_item(pivot->next);
				break;
			}
			case esftSubstr: {
				long start, len;
				str s1;
				struct eval_str* es;
				get_as_str(&pivot->value, &s1);
				if (get_as_int(&pivot->next->value, &start) < 0) return -1;
				if (get_as_int(&pivot->next->next->value, &len) < 0) return -1;

				if (start < 0) {
					start = s1.len + start;
					if (start < 0) start = 0;
				}
				else {
					if (start > s1.len) start = s1.len;
				}
				if (len < 0) {
					len = s1.len - start + len;
					if (len < 0)
						len = 0;
				}
				else {
					if (start + len > s1.len)
						len = s1.len - start;
				}
				s1.s += start;
				s1.len = len;
				es = eval_str_malloc(&s1);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				remove_stack_item(pivot->next);
				remove_stack_item(pivot->next);
				break;
			}
			case esftStrLen: {
				long len;
				str s1;
				get_as_str(&pivot->value, &s1);
				len = s1.len;
				destroy_value(pivot->value);
				pivot->value.type = evtInt;
				pivot->value.u.n = len;
				break;
			}
			case esftStrStr: {
				char buf[25], *p;
				str s1, s2;

				get_as_str(&pivot->value, &s1);
				if (pivot->value.type == evtInt && pivot->next->value.type == evtInt) {
					memcpy(buf, s1.s, s1.len);  /* result in static buffer */
					s1.s = buf;
				}
				get_as_str(&pivot->next->value, &s2);
				p = (char *) memmem(s1.s, s1.len, s2.s, s2.len);
				destroy_value(pivot->value);
				pivot->value.type = evtInt;
				pivot->value.u.n = p?p-s1.s:-1;
				remove_stack_item(pivot->next);
				break;
			}
			case esftStrDel: {
				long start, len;
				str s1, s;
				struct eval_str* es;
				get_as_str(&pivot->value, &s1);
				if (get_as_int(&pivot->next->value, &start) < 0) return -1;
				if (get_as_int(&pivot->next->next->value, &len) < 0) return -1;

				if (start < 0) {
					start = s1.len + start;
					if (start < 0) start = 0;
				}
				else {
					if (start > s1.len) start = s1.len;
				}
				if (len < 0) {
					len = s1.len - start + len;
					if (len < 0)
						len = 0;
				}
				else {
					if (start + len > s1.len)
						len = s1.len - start;
				}
				s.s = 0;
				s.len = s1.len - len;
				es = eval_str_malloc(&s);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				if (start > 0)
					memcpy(s.s, s1.s, start);
				memcpy(s.s+start, s1.s+start+len, s1.len-(start+len));
				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				remove_stack_item(pivot->next);
				remove_stack_item(pivot->next);
				break;
			}
			case esftStrUpper:
			case esftStrLower: {
				str s1;
				int i;
				struct eval_str* es;
				get_as_str(&pivot->value, &s1);

				es = eval_str_malloc(&s1);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				for (i=0; i<es->s.len; i++)
					es->s.s[i] = (d->type == esftStrUpper) ? toupper(es->s.s[i]) : tolower(es->s.s[i]);
				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				break;
			}
			case esftValueAt: {
				str s1, *vals;
				long idx;
				int n;
				struct eval_str* es;

				get_as_str(&pivot->value, &s1);
				if (get_as_int(&pivot->next->value, &idx) < 0) return -1;
				if (parse_hf_values(s1, &n, &vals) < 0) return -1;
				if (idx < 0|| idx >= n) {
					ERR(MODULE_NAME": index (%ld) of of range (%d)\n", idx, n);
					return -1;
				}
				es = eval_str_malloc(vals+idx);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				remove_stack_item(pivot->next);
				break;
			}
			case esftStrValueAt: {
				char buf[25];
				str s1, s2, *vals;
				int i, n;

				get_as_str(&pivot->value, &s1);
				if (pivot->value.type == evtInt && pivot->next->value.type == evtInt) {
					memcpy(buf, s1.s, s1.len);  /* result in static buffer */
					s1.s = buf;
				}
				get_as_str(&pivot->next->value, &s2);
				if (parse_hf_values(s1, &n, &vals) < 0) return -1;
				for (i=0; i<n; i++) {
					if (s2.len == vals[i].len && strncmp(s2.s, vals[i].s, s2.len) == 0)
						break;
				}
				destroy_value(pivot->value);
				pivot->value.type = evtInt;
				pivot->value.u.n = (i>=n)?-1:i;
				remove_stack_item(pivot->next);
				break;
			}
			case esftValueCount: {
				str s1, *vals;
				int n;

				get_as_str(&pivot->value, &s1);
				if (parse_hf_values(s1, &n, &vals) < 0) return -1;
				destroy_value(pivot->value);
				pivot->value.type = evtInt;
				pivot->value.u.n = n;
				break;
			}
			case esftSubValue: {
				long start, len;
				int i, n, pos;
				str s1, s, *vals;
				struct eval_str* es;
				get_as_str(&pivot->value, &s1);
				if (get_as_int(&pivot->next->value, &start) < 0) return -1;
				if (get_as_int(&pivot->next->next->value, &len) < 0) return -1;
				if (parse_hf_values(s1, &n, &vals) < 0) return -1;

				if (start < 0) {
					start = n + start;
					if (start < 0) start = 0;
				}
				else {
					if (start > n) start = n;
				}
				if (len < 0) {
					len = n - start + len;
					if (len < 0)
						len = 0;
				}
				else {
					if (start + len > n)
						len = n - start;
				}
				s.len = 0;
				for (i=0; i<len; i++) {
					s.len += vals[start+i].len+1/*delim*/;
				}
				if (s.len)
					s.len--;
				s.s = 0;
				es = eval_str_malloc(&s);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				for (i=0, pos=0; i<len; i++) {
					if (pos > 0)
						s.s[pos++] = ',';
					memcpy(s.s+pos, vals[start+i].s, vals[start+i].len);
					pos += vals[start+i].len;
				}

				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				remove_stack_item(pivot->next);
				remove_stack_item(pivot->next);
				break;
			}
			case esftValueConcat: {
				long n;
				int i, pos;
				str s1, s;
				struct eval_str* es;
				struct stack_item *si;

				if (get_as_int(&pivot->value, &n) < 0) return -1;
				for (si=pivot->next, s.len=0, i=0; i<n && si; i++, si=si->next) {
					get_as_str(&si->value, &s1);
					s.len += s1.len+1;
				}
				if (s.len)
					s.len--;
				s.s = 0;
				es = eval_str_malloc(&s);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				for (si=pivot->next, i=0, pos=0; i<n && si; i++, si=si->next) {
					if (pos > 0)
						s.s[pos++] = ',';
					get_as_str(&si->value, &s1);
					memcpy(s.s+pos, s1.s, s1.len);
					pos += s1.len;
				}

				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				for (si=pivot->next, i=0; i<n && si; i++) {
					struct stack_item *si2;
					si2 = si;
					si=si->next;
					remove_stack_item(si2);
				}
				break;

			}
			case esftValueRev: {
				int i, n, pos;
				str s1, s, *vals;
				struct eval_str* es;
				get_as_str(&pivot->value, &s1);
				if (parse_hf_values(s1, &n, &vals) < 0) return -1;

				s.len = 0;
				for (i=0; i<n; i++) {
					s.len += vals[i].len+1/*delim*/;
				}
				if (s.len)
					s.len--;
				s.s = 0;
				es = eval_str_malloc(&s);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				for (i=n-1, pos=0; i>=0; i--) {
					if (pos > 0)
						s.s[pos++] = ',';
					memcpy(s.s+pos, vals[i].s, vals[i].len);
					pos += vals[i].len;
				}

				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				break;
			}
			case esftValueUris: {
				int i, n, pos;
				str s1, s, *vals;
				struct eval_str* es;
				get_as_str(&pivot->value, &s1);
				if (parse_hf_values(s1, &n, &vals) < 0) return -1;

				s.len = 0;
				for (i=0; i<n; i++) {
					s.len += vals[i].len+1/*delim*/;
				}
				if (s.len)
					s.len--;
				s.s = 0;
				es = eval_str_malloc(&s);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				for (i=0, pos=0; i<n; i++) {
					str hval1, huri;
					if (pos > 0)
						s.s[pos++] = ',';
					hval1 = *(vals+i);
					get_uri_and_skip_until_params(&hval1, &huri);
					if (huri.len) {
					/* TODO: normalize uri, lowercase except quoted params */
						memcpy(s.s+pos, huri.s, huri.len);
						pos += huri.len;
					}
				}
				es->s.len = pos;

				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				break;
			}
			case esftGetUri: {
				str s1, huri;
				struct eval_str* es;
				get_as_str(&pivot->value, &s1);
				get_uri_and_skip_until_params(&s1, &huri);
				if (huri.len && *(huri.s) == '<') {
					huri.s++;   	/* strip < & > */
					huri.len-=2;
				}
				es = eval_str_malloc(&huri);
				if (!es) {
					ERR(MODULE_NAME": out of memory\n");
					return -1;
				}
				destroy_value(pivot->value);
				pivot->value.type = evtStr;
				pivot->value.u.s = es;
				break;
			}
			default:
				BUG("Bad operation %d\n", d->type);
				return -1;
		}
	}
        return ret;
}

static int eval_while_fixup(void **param, int param_no) {

	if (param_no == 2) {
		return fixup_location_12(param, param_no);
	}
	else if (param_no == 1) {
		int n;
		n = route_get(&main_rt, (char*) *param);
		if (n == -1) {
			ERR(MODULE_NAME": eval_while: bad route\n");
			return E_CFG;
		}
		pkg_free(*param);
		*param=(void*) (intptr_t) n;
	}
	return 0;
}

static int eval_while_func(struct sip_msg *msg, char *route_no, char *param2) {
	int ret, idx;
	struct stack_item *pivot;
	struct run_act_ctx ra_ctx;

	if (param2) {
		long l;
		struct eval_value v;
		eval_location(msg, (struct eval_location*) param2, &v, 1);
		ret = get_as_int(&v, &l);
		if (ret < 0) return ret;
		idx = l;
	}
	else {
		idx = 0;      /* default values */
	}

	ret = -1;
	while (1) {
		pivot = find_stack_item(idx);
		if (!pivot) break;
		if (get_as_bool(&pivot->value) <= 0) break;
		if ((intptr_t)route_no >= main_rt.idx) {
			BUG("invalid routing table number #%d of %d\n", (int)(intptr_t) route_no, main_rt.idx);
			return -1;
		}
		if (!main_rt.rlist[(intptr_t) route_no]) {
			WARN(MODULE_NAME": route not declared (hash:%d)\n", (int)(intptr_t) route_no);
			return -1;
		}
		/* exec the routing script */
		init_run_actions_ctx(&ra_ctx);
		ret = run_actions(&ra_ctx, main_rt.rlist[(intptr_t) route_no], msg);
		if (ret <= 0) break;
	}
	return ret;
}

static int eval_while_stack_func(struct sip_msg *msg, char *route_no, char *param2) {
	int ret, count;
	struct run_act_ctx ra_ctx;
	
	if (param2) {
		long l;
		struct eval_value v;
		eval_location(msg, (struct eval_location*) param2, &v, 1);
		ret = get_as_int(&v, &l);
		if (ret < 0) return ret;
		count = l;
	}
	else {
		count = 0;      /* default values */
	}
	ret = -1;
	while ((count >= 0 && stack_no > count) || (count < 0 && stack_no < -count)) {
		if ((intptr_t)route_no >= main_rt.idx) {
			BUG("invalid routing table number #%d of %d\n", (int)(intptr_t) route_no, main_rt.idx);
			return -1;
		}
		if (!main_rt.rlist[(intptr_t) route_no]) {
			WARN(MODULE_NAME": route not declared (hash:%d)\n", (int)(intptr_t) route_no);
			return -1;
		}
		/* exec the routing script */
		init_run_actions_ctx(&ra_ctx);
		ret = run_actions(&ra_ctx, main_rt.rlist[(intptr_t) route_no], msg);
		if (ret <= 0) break;
	}
	return ret;
}

/* select functions */
static int sel_value2str(str* res, struct eval_value *v, int force_copy) {
	res->len = 0;
	switch (v->type) {
		case evtInt: {
			char buf[30];
			res->len = snprintf(buf, sizeof(buf)-1, "%ld", v->u.n);
			res->s = get_static_buffer(res->len);
			if (res->s) 
				memcpy(res->s, buf, res->len);
			else
				res->len = 0;
			break;
		}
		case evtStr:
			if (v->u.s) {
				*res = v->u.s->s;
				if (force_copy && res->len) {
					res->s = get_static_buffer(res->len);
					if (res->s)
						memcpy(res->s, v->u.s->s.s, res->len);
					else
						res->len = 0;
				}
			}
			break;
	}
	return 0;
}

static int sel_eval(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	return 0;
}

static int sel_register(str* res, select_t* s, struct sip_msg* msg) {
	if (msg == 0) {
		struct register_item *p = find_register(s->params[2].v.s.s, s->params[2].v.s.len);
		if (p == 0) {
			ERR(MODULE_NAME": select: register '%.*s' not found\n", s->params[2].v.s.len, s->params[2].v.s.s);
			return E_CFG;
		}
		s->params[2].v.p = p;
		s->params[2].type = SEL_PARAM_PTR;
	}
	else {
		return sel_value2str(res, &((struct register_item *)s->params[2].v.p)->value, 0);
	}
	return 0;
}

static int sel_get_and_remove(str* res, select_t* s, struct sip_msg* msg) {
	struct stack_item* p;
	res->len = 0;
	p = find_stack_item(s->params[2].v.i);
	if (p) {
		sel_value2str(res, &p->value, 1);
		remove_stack_item(p);
	}
	return 0;
}

static int sel_get(str* res, select_t* s, struct sip_msg* msg) {
	struct stack_item* p;
	res->len = 0;
	p = find_stack_item(s->params[2].v.i);
	if (p) {
		sel_value2str(res, &p->value, 0);
	}
	return 0;
}

SELECT_F(select_any_nameaddr)
SELECT_F(select_any_uri)
SELECT_F(select_anyheader_params)

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT(MODULE_NAME), sel_eval, SEL_PARAM_EXPECTED},

	{ sel_eval, SEL_PARAM_STR, STR_STATIC_INIT("pop"), sel_get_and_remove, CONSUME_NEXT_INT },
	{ sel_eval, SEL_PARAM_STR, STR_STATIC_INIT("get"), sel_get, CONSUME_NEXT_INT },
	{ sel_eval, SEL_PARAM_STR, STR_STATIC_INIT("reg"), sel_register, CONSUME_NEXT_STR|FIXUP_CALL },

	{ sel_get, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_get, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ sel_get, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_anyheader_params, NESTED},
	{ sel_register, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_register, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ sel_register, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_anyheader_params, NESTED},

	/* for backward compatability only, use @sys.unique */
	{ sel_eval, SEL_PARAM_STR, STR_STATIC_INIT("uuid"), select_sys_unique, 0},

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};


static int mod_init() {

	register_script_cb(mod_pre_script_cb, REQUEST_CB | ONREPLY_CB | PRE_SCRIPT_CB, 0);
	register_select_table(sel_declaration);
	return 0;
}

static int child_init(int rank) {

	return 0;
}

static void destroy_mod(void) {
	struct register_item *p;
	destroy_stack();
	destroy_register_values();
	while (registers) {
		p = registers;
		registers = registers->next;
		pkg_free(p);
	}
}

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{MODULE_NAME"_add", eval_stack_oper_func, 2, eval_add_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_add", eval_stack_oper_func, 1, eval_add_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_push", eval_stack_oper_func, 2, eval_add_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_push", eval_stack_oper_func, 1, eval_add_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_insert", eval_stack_oper_func, 2, eval_insert_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_insert", eval_stack_oper_func, 1, eval_insert_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_xchg", eval_stack_oper_func, 2, eval_xchg_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_xchg", eval_stack_oper_func, 1, eval_xchg_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_get", eval_stack_oper_func, 2, eval_get_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_get", eval_stack_oper_func, 1, eval_get_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_put", eval_stack_oper_func, 2, eval_put_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_put", eval_stack_oper_func, 1, eval_put_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_pop", eval_stack_oper_func, 2, eval_pop_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_pop", eval_stack_oper_func, 1, eval_pop_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_add_value", eval_stack_oper_func, 2, eval_add_value_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_add_value", eval_stack_oper_func, 1, eval_add_value_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_insert_value", eval_stack_oper_func, 2, eval_insert_value_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_insert_value", eval_stack_oper_func, 1, eval_insert_value_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},

	{MODULE_NAME"_remove", eval_remove_func, 0, fixup_location_12, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_remove", eval_remove_func, 1, fixup_location_12, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_remove", eval_remove_func, 2, fixup_location_12, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_clear", eval_clear_func, 1, fixup_int_12, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},

	{MODULE_NAME"_oper", eval_stack_func_func, 2, eval_stack_func_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_oper", eval_stack_func_func, 1, eval_stack_func_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},

	{MODULE_NAME"_while", eval_while_func, 1, eval_while_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_while", eval_while_func, 2, eval_while_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_while_stack", eval_while_stack_func, 1, eval_while_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME"_while_stack", eval_while_stack_func, 2, eval_while_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},

	{MODULE_NAME"_dump", eval_dump_func, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},

	{0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"declare_register", PARAM_STRING|PARAM_USE_FUNC, (void*) declare_register},
	{"xlbuf_size",       PARAM_INT, &xlbuf_size},
	{0, 0, 0}
};


struct module_exports exports = {
	MODULE_NAME,
	cmds,        /* Exported commands */
	0,	     /* RPC */
	params,      /* Exported parameters */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	destroy_mod, /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};
