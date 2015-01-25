/*
 * Copyright (C) 2006 iptelorg GmbH
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
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../error.h"
#include "../../config.h"
#include "../../trim.h"
#include "../../lib/srdb2/db.h"
#include "../../select.h"
#include "../../script_cb.h"
#include "../../modules/xprint/xp_lib.h"
#include "../../route.h"
#include "../../action.h"
#include "../../ut.h"
#include "../../str_hash.h"


MODULE_VERSION

#define MODULE_NAME "db2_ops"
#define MODULE_NAME2 "db"

static char* db_url = DEFAULT_DB_URL;
static int xlbuf_size = 4096;

enum dbops_type {OPEN_QUERY_OPS, INSERT_OPS, UPDATE_OPS, REPLACE_OPS, DELETE_OPS};

#define FLD_DELIM ','
#define PART_DELIM '/'

#define NO_SCRIPT -1

static str* xl_nul = NULL;
static xl_print_log_f* xl_print = NULL;
static xl_parse_format_f* xl_parse = NULL;
static xl_get_nulstr_f* xl_getnul = NULL;

static char *xlbuf = 0;
static char *xlbuf_tail;

struct xlstr {
	char *s;
	xl_elog_t* xlfmt;
};

struct extra_ops { 
	char *name;
	int type;
	char *value;
};

struct dbops_action {
	char *query_name;
	char *db_url;

	db_ctx_t* ctx;
	db_cmd_t* cmd;

	enum dbops_type operation;
	int query_no;
	int is_raw_query;
	struct xlstr table;

	int field_count;
	struct xlstr* fields;

	int where_count;
	struct xlstr* wheres;
	int op_count;

	struct xlstr* ops;
	int value_count;
	struct xlstr* values;
	int* value_types;

	struct xlstr order;
	struct xlstr raw;

	int extra_ops_count;	
	struct extra_ops* extra_ops;

	db_res_t* result;   /* result of SELECT */

	struct dbops_action* next;
};

struct dbops_handle {
	char *handle_name;
	struct dbops_action* action;
	db_res_t* result;
	int cur_row_no;
	struct dbops_handle *next;
};


/* list of all operations */
static struct dbops_action* dbops_actions = 0;

/* list of declared handles, close them in post script callback */
static struct dbops_handle* dbops_handles = 0;

#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
	(_p)++;}

#define eat_alphanum(_p) \
	while ( (*(_p) >= 'a' && *(_p) <= 'z') || (*(_p) >= 'A' && *(_p) <= 'Z') || (*(_p) >= '0' && *(_p) <= '9') || (*(_p) == '_') ) {\
		(_p)++;\
	}

static struct dbops_action* find_action_by_name(char *name, int len) {
	struct dbops_action *a;
	if (len == -1) len = strlen(name);
	for (a=dbops_actions; a; a = a->next) {		
		if (a->query_name && strlen(a->query_name)==len && strncmp(name, a->query_name, len) == 0)
			return a;
	}
	return NULL;
}

static struct dbops_handle* find_handle_by_name(char *name, int len) {
	struct dbops_handle *a;
	if (len == -1) len = strlen(name);
	for (a=dbops_handles; a; a = a->next) {		
		if (a->handle_name && strlen(a->handle_name)==len && strncmp(name, a->handle_name, len) == 0)
			return a;
	}
	return NULL;
}

static void trim_apostr(char **s) {
	int i;
	while ( **s == '\'') {
		(*s)++;
	}
	i = strlen(*s);
	while (i && (*s)[i-1] == '\'') {
		i--;
		(*s)[i] = 0;
	}
}

static int get_next_part(char** s, char** part, char delim, int read_only) {
	char *c, *c2;
	char flag = 0;

	c = c2 = *s;
	eat_spaces(c);

	while (!(((*c2 == delim) && !flag) || *c2==0)) {
		if (*c2=='\'')
			flag = !flag;
		c2++;
	}
	if ((*c2)==0 && flag) {
		ERR(MODULE_NAME": string '%s' is not terminated\n", *s);
		return E_CFG;
	}
	if (*c2) {
		if (!read_only) *c2 = 0;
		*s = c2+1;
	}
	else {
		*s = c2;
	}
	eat_spaces(*s);
	c2--;
	/* rtrim */
	while ( c2 > c && ((*c2 == ' ')||(*c2 == '\t')) ) {
		if (!read_only) *c2 = 0;
		c2--;
	}
	*part = c;
	return 0;
}

static int split_fields(char *part, int *n, struct xlstr **strs) {
	int i, res;
	char *c, *fld;

	*n = 0;
	*strs = 0;
	c = part;
	while (*c) {
		res = get_next_part(&c, &fld, FLD_DELIM, 1);
		if (res < 0) return res;
		(*n)++;
	}
	*strs = pkg_malloc( (*n)*sizeof(**strs));
	if (!strs) {
		ERR(MODULE_NAME": split_fields: not enough pkg memory\n");
		return E_OUT_OF_MEM;
	}
	memset(*strs, 0, (*n)*sizeof(**strs));
	i = 0;
	c = part;
	while (*c) {
		res = get_next_part(&c, &(*strs)[i].s, FLD_DELIM, 0);
		if (res < 0) return res;
		trim_apostr(&(*strs)[i].s);
		i++;
	}
	return 0;
}

static int get_type(char **s, int *type) {
	if (*s && (*s)[0] && (*s)[1]==':') {
		switch ((*s)[0]) {
			case 't':
				*type = DB_DATETIME;
				break;
			case 'i':
				*type = DB_INT;
				break;
			case 'f':
				*type = DB_FLOAT;
				break;
			case 'd':
				*type = DB_DOUBLE;
				break;
			case 's':
				*type = DB_CSTR;
				break;
			default:
				ERR(MODULE_NAME": get_type: bad param type in '%s'\n", *s);
				return E_CFG;
		}
		(*s)+=2;
	}
	return 0;
}

static int parse_ops(char* act_s, struct dbops_action** action, int has_name) {
	int res = 0, i;
	char *c, *s, *part;
	static int query_no = 0;

	s = act_s;
	*action = pkg_malloc(sizeof(**action));
	if (!*action) return E_OUT_OF_MEM;
	memset(*action, 0, sizeof(**action));
	(*action)->query_no = query_no++;

	eat_spaces(s);
	c = s;
	eat_alphanum(c);
	if (has_name) {
		char *c2;
		c2 = c;
		eat_spaces(c2);
		if (c != s && *c2 == '=') {
			*c = '\0';
			if (find_action_by_name(s, -1) != NULL) {
				ERR(MODULE_NAME": parse_ops: duplicate query name: %s\n", s);
				return E_CFG;
			}		
			(*action)->query_name = s;
			s = c2+1;
			eat_spaces(s);
			c = s;
			eat_alphanum(c);
		}
		else {
			ERR(MODULE_NAME": parse_ops: query_no: %d, valid query name not found in '%s'\n%s\n%s\n", (*action)->query_no, s, c, c2);
			return E_CFG;
		}
	}

	if (c[0] == ':' && c[1] == '/' && c[2] == '/') { /* database part is optional */
		for (c=s; *c!=':'; c++) {
			*c = tolower(*c);                       /* _type_://user:host/database_name/ */
		}
		(*action)->db_url = s;
		s = c+1;
		while (*s == '/') s++;
		res = get_next_part(&s, &part, PART_DELIM, 1);  /* type://_user:host_/database_name/ */
		if (res < 0) return res;


		res = get_next_part(&s, &part, PART_DELIM, 0);  /* type://user:host/_database_name_/ */
		if (res < 0) return res;
	}
	res = get_next_part(&s, &part, PART_DELIM, 0);
	if (res < 0) return res;

	for (c = part; *c && *c != PART_DELIM; c++) {
		if (*c == ' ') {
			(*action)->is_raw_query = 1;
			*c = '\0';
			break;
		}
	}
	if (strcasecmp(part, "select") == 0)
		(*action)->operation = OPEN_QUERY_OPS;
	else if (strcasecmp(part, "insert") == 0)
		(*action)->operation = INSERT_OPS;
	else if (strcasecmp(part, "update") == 0)
		(*action)->operation = UPDATE_OPS;
	else if (strcasecmp(part, "replace") == 0)
		(*action)->operation = REPLACE_OPS;
	else if (strcasecmp(part, "delete") == 0)
		(*action)->operation = DELETE_OPS;
	else {
		if ((*action)->is_raw_query) *c = ' ';
		ERR(MODULE_NAME": parse_ops: query: %s(%d), unknown type of query '%s'\n", (*action)->query_name, (*action)->query_no, part);
		return E_CFG;
	}
	if ((*action)->is_raw_query) {
		*c = ' ';
		(*action)->raw.s = part;
		(*action)->table.s = part;
	}

	res = get_next_part(&s, &part, PART_DELIM, 0);
	if (res < 0) return res;
	if (!(*action)->is_raw_query) {

		if (!*part) {
			ERR(MODULE_NAME": parse_ops: query: %s(%d), table not specified near '%s' in '%s'\n", (*action)->query_name, (*action)->query_no, s, act_s);
			return E_CFG;
		}
		trim_apostr(&part);
		(*action)->table.s = part;

		res = get_next_part(&s, &part, PART_DELIM, 0);
		if (res < 0) return res;
		switch ((*action)->operation) {
			case OPEN_QUERY_OPS:
			case UPDATE_OPS:
			case REPLACE_OPS:
			case INSERT_OPS:
				res = split_fields(part, &(*action)->field_count, &(*action)->fields);
				if (res < 0) return res;
				if ((*action)->field_count == 0) {
					ERR(MODULE_NAME": parse_ops: query: %s(%d), no field specified near '%s' ?n '%s'\n", (*action)->query_name, (*action)->query_no, part, act_s);
					return E_CFG;
				}
				break;
			case DELETE_OPS:
				res = split_fields(part, &(*action)->where_count, &(*action)->wheres);
				if (res < 0) return res;
				res = get_next_part(&s, &part, PART_DELIM, 0);
				if (res < 0) return res;
				res = split_fields(part, &(*action)->op_count, &(*action)->ops);
				if (res < 0) return res;
				break;
			default:;
		}

		res = get_next_part(&s, &part, PART_DELIM, 0);
		if (res < 0) return res;
		switch ((*action)->operation) {
			case OPEN_QUERY_OPS:
			case UPDATE_OPS:
				res = split_fields(part, &(*action)->where_count, &(*action)->wheres);
				if (res < 0) return res;
				res = get_next_part(&s, &part, PART_DELIM, 0);
				if (res < 0) return res;
				res = split_fields(part, &(*action)->op_count, &(*action)->ops);
				if (res < 0) return res;
				res = get_next_part(&s, &part, PART_DELIM, 0);
				if (res < 0) return res;
				switch ((*action)->operation) {
					case OPEN_QUERY_OPS:
						if (*part) {
							(*action)->order.s = part;
						}
						res = get_next_part(&s, &part, PART_DELIM, 0);
						if (res < 0) return res;
						break;
					default:;
				}
				break;
			default:
				;
		}
	}

	/* values */
	res = split_fields(part, &(*action)->value_count, &(*action)->values);
	if (res < 0) return res;

	if ((*action)->value_count) {
		(*action)->value_types = (int*)pkg_malloc(sizeof(int) * (*action)->value_count);
		if ((*action)->value_types == NULL) {
			ERR(MODULE_NAME": No memory left\n");
			return -1;
		}

		for (i=0; i<(*action)->value_count; i++) {
			(*action)->value_types[i] = DB_CSTR; // DB_NONE; /* let decide db driver itself, FIXME: until jjanak changes then default type is string */
			res = get_type(&(*action)->values[i].s, &(*action)->value_types[i]);
			if (res < 0) return res;
		}
	}

	/* extra options */
	res = get_next_part(&s, &part, PART_DELIM, 0);
	if (res < 0) return res;

	(*action)->extra_ops_count = 0;
	c = part;
	while (*c) {
		char *fld;
		res = get_next_part(&c, &fld, FLD_DELIM, 1);
		if (res < 0) return res;
		(*action)->extra_ops_count++;
	}
	if ((*action)->extra_ops_count > 0) {
		(*action)->extra_ops = pkg_malloc( (*action)->extra_ops_count*sizeof(*(*action)->extra_ops));
		if (!(*action)->extra_ops) {
			ERR(MODULE_NAME": parse_ops: not enough pkg memory\n");
			return E_OUT_OF_MEM;
		}
		memset((*action)->extra_ops, 0, (*action)->extra_ops_count*sizeof(*(*action)->extra_ops));

		i = 0;
		c = part;
		while (*c) {
			char *fld;
			res = get_next_part(&c, &fld, FLD_DELIM, 0);
			if (res < 0) return res;
			/* name=[i|s]:value */
			(*action)->extra_ops[i].name = fld;
			eat_alphanum(fld);
			if (*fld != '=') {
				ERR(MODULE_NAME": parse_ops: query: %s(%d), bad extra parameter format in '%s'\n", (*action)->query_name, (*action)->query_no, (*action)->extra_ops[i].name);
				return E_CFG;
			}
			*fld = '\0';
			fld++;
			while (*fld==' ' || *fld=='\t') fld++;
			(*action)->extra_ops[i].type = DB_NONE;
			res = get_type(&fld, &(*action)->extra_ops[i].type);
			if (res < 0) return res;
			trim_apostr(&fld);
			(*action)->extra_ops[i].value = fld;
			DEBUG(MODULE_NAME": extra_ops #%d, name='%s', type=%d, val='%s'\n", i, (*action)->extra_ops[i].name, (*action)->extra_ops[i].type, (*action)->extra_ops[i].value);
			i++;
		}
	}

	if (*s) {
		ERR(MODULE_NAME": parse_ops: query: %s(%d), too many parameters/parts, remaining '%s' in '%s'\n", (*action)->query_name, (*action)->query_no, s, act_s);
		return E_CFG;
	}
	if ((*action)->is_raw_query) {
		DEBUG(MODULE_NAME": query: %s(%d) oper:%d database:'%s' query:'%s' value#:%d extra_ops#:%d\n", (*action)->query_name, (*action)->query_no, (*action)->operation, (*action)->db_url, (*action)->raw.s, (*action)->value_count, (*action)->extra_ops_count);
	}
	else {
		/* check num of fields */
		if ((((*action)->operation==OPEN_QUERY_OPS)?0:(*action)->field_count)+(*action)->where_count != (*action)->value_count) {
			ERR(MODULE_NAME": parse_ops: query: %s(%d), number of values does not correspond to number of fields (%d+%d!=%d) in '%s'\n", (*action)->query_name, (*action)->query_no, ((*action)->operation==OPEN_QUERY_OPS)?0:(*action)->field_count,  (*action)->where_count, (*action)->value_count, act_s);
			return E_CFG;
		}
		DEBUG(MODULE_NAME": query_no:%d oper:%d database:'%s' table:'%s' 'field#:'%d' where#:'%d' order:'%s' value#:%d extra_ops#:%d\n", (*action)->query_no, (*action)->operation, (*action)->db_url, (*action)->table.s, (*action)->field_count,  (*action)->where_count, (*action)->order.s, (*action)->value_count, (*action)->extra_ops_count);
	}
	return 0;
}

static int parse_xlstr(struct xlstr* s) {

	if (!s->s) return 0;
	if (!strchr(s->s, '%')) return 0;
	/* probably xl_log formating */

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

	if (!xl_nul) {
		xl_getnul=(xl_get_nulstr_f*)find_export("xnulstr", NO_SCRIPT, 0);
		if (xl_getnul)
			xl_nul=xl_getnul();

		if (!xl_nul){
			ERR(MODULE_NAME": cannot find \"xnulstr\", is module xprint loaded?\n");
			return E_UNSPEC;
		}
	else
		INFO(MODULE_NAME": xprint null is \"%.*s\"\n", xl_nul->len, xl_nul->s);
	}

	if(xl_parse(s->s, &s->xlfmt) < 0) {
		ERR(MODULE_NAME": wrong format '%s'\n", s->s);
		return E_UNSPEC;
	}

	return 0;
}

static int eval_xlstr(struct sip_msg* msg, struct xlstr* s) {
	static char* null_str = "";
	int len;
	if (s->xlfmt) {
		len = xlbuf_size - (xlbuf_tail-xlbuf);
		if (xl_print(msg, s->xlfmt, xlbuf_tail, &len) < 0) {
			ERR(MODULE_NAME": eval_xlstr: Error while formating result\n");
			return E_UNSPEC;
		}

		/* note: xl_null value is returned as "<null>" string. It's pretty useless checking "if xlbuf_tail==xl_null then xlbuf_tail="";" because xl_null may be also inside string. What about implementing xl_set_nullstr to xl_lib? */
		if ((xl_nul) && (xl_nul->len==len) && strncmp(xl_nul->s, xlbuf_tail, len)==0) {
			s->s = null_str;
		}
		else {
			s->s = xlbuf_tail;
			s->s[len] = '\0';
			xlbuf_tail += len+1;
		}
	}
	else {
		if (!s->s)
			s->s = null_str;
	}
	return 0;
}

static int dbops_func(struct sip_msg* m, struct dbops_action* action) 
{
/*	char* order;*/
	int res, i;
/* raw query is pre-compiled too 
	if (action->is_raw_query) {
		DEBUG(MODULE_NAME": dbops_func(raw, %d, '%s'\n", action->operation, action->raw.s);
		res = eval_xlstr(m, &action->raw);
		if (res < 0) return res;
		
	   	 * FIXME: We have to make sure that we do not use pre-compiled statements
		 * here because these must not change at runtime: to be checked by janakj
		 *
		action->cmd->table.s = action->raw.s;
		action->cmd->table.len = strlen(action->raw.s);
		
		if (db_exec((action->operation==OPEN_QUERY_OPS?&action->result:NULL), action->cmd) < 0) {
			ERR(MODULE_NAME": database operation (%d) error, raw: '%s'\n", action->operation, action->raw.s);
			return -1;
		}
		return 1;
	}
*/
	if (action->is_raw_query) {
		DEBUG(MODULE_NAME": dbops_func(%s, %d, raw, %d, '%s', %d)\n", action->query_name, action->query_no, action->operation, action->cmd->table.s, action->value_count);
		/* raw query is pre-compiled too 
		res = eval_xlstr(m, &action->raw);
		if (res < 0) return res;
		
	   	 * FIXME: We have to make sure that we do not use pre-compiled statements
		 * here because these must not change at runtime: to be checked by janakj
		 *
		action->cmd->table.s = action->raw.s;
		action->cmd->table.len = strlen(action->raw.s);
		*/
	}
	else {
		DEBUG(MODULE_NAME": dbops_func(%s, %d, %d, '%s', %d, %d, %d)\n", action->query_name, action->query_no, action->operation, action->table.s, action->field_count, action->where_count, action->value_count);
	
		/* FIXME: We do not support volatile table names yet */
		/*	res = eval_xlstr(m, &action->table); 
			if (res < 0) return res;
		*/

		/*
		 * FIXME: Changing field names in result set is not yet supported
			for (i=0; i<action->field_count; i++) {
				res = eval_xlstr(m, &action->fields[i]);
				if (res < 0) goto cleanup;
				action->cmd->result[i].v.name = action->fields[i].s;
			}
		*/
		/*
		 * FIXME: Changing parameters names and operation not yet
		 * supported at runtime
			for (i=0; i<action->where_count; i++) {
				res = eval_xlstr(m, &action->wheres[i]);
				if (res < 0) goto cleanup;
				action->cmd->params[i].name = action->wheres[i].s;

				if (i < action->op_count) {
					res = eval_xlstr(m, &action->ops[i]);
					if (res < 0) goto cleanup;
					action->cmd->params[i].op = action->ops[i].s;
				}
				else {
					action->cmd->params[i].op = OP_EQ;
				}
			}
		*/
		/* FIXME: Changing parameters names and operation not yet
		 * supported at runtime
			if (action->operation == OPEN_QUERY_OPS) {
				if (action->order.s) {
					res = eval_xlstr(m, &action->order);
					if (res < 0) return res;
				}
				order = action->order.s;
			}
		*/
	}
	for (i=0; i<action->value_count; i++) {
		char *end;
		db_fld_t *cmd_params;
		res = eval_xlstr(m, &action->values[i]);
		if (res < 0) goto cleanup;

		/* split single db_ops values range to db_api matches and vals ranges */
		if (action->is_raw_query) {
			cmd_params = action->cmd->match+i;
		}
		else {
			switch (action->operation) {
				case OPEN_QUERY_OPS:
				case DELETE_OPS:
					cmd_params = action->cmd->match+i;
					break;
				case UPDATE_OPS:
					if (i < action->field_count)
						cmd_params = action->cmd->vals+i;
					else
						cmd_params = action->cmd->match+i-action->field_count;
					break;
				case REPLACE_OPS:
				case INSERT_OPS:
					cmd_params = action->cmd->vals+i;
					break;
				default:
					BUG("Unknown operation type: %d\n", action->operation);
					goto err;
			}
		}
		
		if (!action->values[i].s || !action->values[i].s[0]) cmd_params->flags |= DB_NULL;
		switch (cmd_params->type) {
			case DB_DATETIME:
				if (!(cmd_params->flags & DB_NULL))
					cmd_params->v.time = strtol(action->values[i].s, &end, 10);
				break;
			case DB_INT:
				if (!(cmd_params->flags & DB_NULL))
					cmd_params->v.int4 = strtol(action->values[i].s, &end, 10);
				break;
			case DB_FLOAT:
				if (!(cmd_params->flags & DB_NULL))
				#ifdef  __USE_ISOC99
					cmd_params->v.flt = strtof(action->values[i].s, &end);
				#else
					cmd_params->v.flt = strtod(action->values[i].s, &end);
				#endif
				break;
			case DB_DOUBLE:
				if (!(cmd_params->flags & DB_NULL))
					cmd_params->v.dbl = strtod(action->values[i].s, &end);
				break;
			case DB_CSTR:
				cmd_params->v.cstr = action->values[i].s;
				cmd_params->flags &= ~DB_NULL;
				break;
		default:
				BUG("Unknown value type: %d\n", cmd_params->type);
				goto err;
		}
	}
	if (db_exec((action->operation==OPEN_QUERY_OPS?&action->result:NULL), action->cmd) < 0) goto err;
	res = 1;
cleanup:
	return res;
err:
	ERR(MODULE_NAME": query: %s(%d), database operation (%d) error, table: '%s'\n", action->query_name, action->query_no, action->operation, action->table.s);
	res = -1;
	goto cleanup;
}

static int do_seek(db_res_t* result, int *cur_row_no, int row_no) {

	if (row_no == *cur_row_no) return 0;
	if (row_no < *cur_row_no) *cur_row_no = -1;

	DEBUG(MODULE_NAME": do_seek: currowno:%d, rowno=%d\n", *cur_row_no, row_no);
	if (*cur_row_no < 0) {
		if (!db_first(result)) return -1;
		*cur_row_no = 0;
	}
	while (*cur_row_no < row_no) {
		if (!db_next(result)) {
			*cur_row_no = -1;
			return -1;
		}
		(*cur_row_no)++;
	}
	return 0;
}

static int sel_get_field(str* res, int *cur_row_no, int field_no, db_res_t* result) {
/* return string in static buffer, I'm not sure if local static variable is OK, e.g. when comparing 2 selects */
	int len;
	db_rec_t* rec;
	len = xlbuf_size-(xlbuf_tail-xlbuf);
	res->s = xlbuf_tail;
	res->len = 0;
	if (field_no == -2) {  /* cur_row_no */
		res->len = snprintf(res->s, len, "%d", *cur_row_no);
	}
	else if (field_no < 0) {  /* count(*) | is empty */
		int n;
		if (*cur_row_no < 0) {
			rec = db_first(result);
			if (rec) {
				*cur_row_no = 0;
			}
		}
		if (field_no == -3) { /* is_empty */
			if (*cur_row_no >= 0) 
				n = 0;
			else
				n = 1;
		}
		else {
			n = 0;
			if (*cur_row_no >= 0) {
				do {
					n++;
					rec = db_next(result);
				} while (rec);
			}
			*cur_row_no = -1;
		}
		res->len = snprintf(res->s, len, "%d", n);
	}
	else {
		if (*cur_row_no < 0) {
			ERR(MODULE_NAME": cursor points beyond data\n");
			return -1;	
		}
		if (field_no >= result->field_count) {
			ERR(MODULE_NAME": field (%d) does not exist, num fields: %d\n", field_no, result->field_count);
			return -1;
		}
		rec = result->cur_rec;
		if (!(rec->fld[field_no].flags & DB_NULL)) {
			switch (rec->fld[field_no].type) {
				case DB_INT:
					res->len = snprintf(res->s, len, "%d", rec->fld[field_no].v.int4);
					break;
				case DB_FLOAT:
					res->len = snprintf(res->s, len, "%f", rec->fld[field_no].v.flt);
					break;
				case DB_DOUBLE:
					res->len = snprintf(res->s, len, "%f", rec->fld[field_no].v.dbl);
					break;
				case DB_STR:
					res->len = snprintf(res->s, len, "%.*s", 
										rec->fld[field_no].v.lstr.len,
										rec->fld[field_no].v.lstr.s);
					break;
				case DB_BLOB:
					res->len = snprintf(res->s, len, "%.*s", 
										rec->fld[field_no].v.blob.len,
										rec->fld[field_no].v.blob.s);
					break;
				case DB_CSTR:
					res->len = snprintf(res->s, len, "%s", rec->fld[field_no].v.cstr);
					break;
				case DB_DATETIME:
					res->len = snprintf(res->s, len, "%u", (unsigned int) rec->fld[field_no].v.time);
					break;
				case DB_BITMAP:
					res->len = snprintf(res->s, len, "%u", (unsigned int) rec->fld[field_no].v.bitmap);
					break;
				default:
					break;
			}
		}
	}
	xlbuf_tail += res->len;
	return 0;
}

static int sel_do_select(str* result, str *query_name, int row_no, int field_no, struct sip_msg* msg) {
	struct dbops_action *a;
	int cur_row_no, res;

	a = find_action_by_name(query_name->s, query_name->len);
	if (!a) {
		ERR(MODULE_NAME": select: query: %.*s not declared using declare_query param\n", query_name->len, query_name->s);
		return -1;
	}
	if (a->operation != OPEN_QUERY_OPS) {
		ERR(MODULE_NAME": select: query: %.*s is not select\n", query_name->len, query_name->s);
		return -1;
	}

	if (row_no < 0) {
		ERR(MODULE_NAME": select: Row number must not be negative: %d\n", row_no);
		return -1;
	}

	res = dbops_func(msg, a);
	if (res < 0) return res;
	cur_row_no = -1;
	if (field_no >= 0) {
		if (do_seek(a->result, &cur_row_no, row_no) < 0)
			return -1;
	}

	res = sel_get_field(result, &cur_row_no, field_no, a->result);
	db_res_free(a->result);
	return res;
}

static inline int check_query_opened(struct dbops_handle *handle, char *f) {
	if (!handle->result) {
		ERR(MODULE_NAME": %s: handle '%s' is not opened. Use db_query() first\n", f, handle->handle_name);
		return -1;
	}
	else
		return 1;
}

static int sel_do_fetch(str* result, str *handle_name, int field_no, struct sip_msg* msg) {
	struct dbops_handle *a;
	a = find_handle_by_name(handle_name->s, handle_name->len);
	if (!a) {
		ERR(MODULE_NAME": fetch: handle (%.*s) is not declared\n", handle_name->len, handle_name->s);
		return -1;
	}
	if (check_query_opened(a, "fetch") < 0) return -1;	
	return sel_get_field(result, &a->cur_row_no, field_no, a->result);
}

static int sel_dbops(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	return 0;
}

static int sel_select(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, &s->params[2].v.s, 0, 0, msg);
}

static int sel_select_is_empty(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, &s->params[2].v.s, 0, -3, msg);
}

static int sel_select_count(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, &s->params[2].v.s, 0, -1, msg);
}

static int sel_select_row(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, &s->params[2].v.s, s->params[4].v.i, 0, msg);
}


static int sel_select_row_field(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, &s->params[2].v.s, s->params[4].v.i, s->params[6].v.i, msg);
}

static int sel_select_field(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, &s->params[2].v.s, 0, s->params[4].v.i, msg);
}

static int sel_fetch(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, &s->params[2].v.s, 0, msg);
}

static int sel_fetch_field(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, &s->params[2].v.s, s->params[4].v.i, msg);
}

static int sel_fetch_cur_row_no(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, &s->params[2].v.s, -2, msg);
}

static int sel_fetch_is_empty(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, &s->params[2].v.s, -3, msg);
}

static int sel_fetch_count(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, &s->params[2].v.s, -1, msg);
}


SELECT_F(select_any_nameaddr)
SELECT_F(select_any_uri)

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT(MODULE_NAME2), sel_dbops, SEL_PARAM_EXPECTED},

	{ sel_dbops, SEL_PARAM_STR, STR_STATIC_INIT("query"), sel_select, CONSUME_NEXT_STR},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("is_empty"), sel_select_is_empty},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("count"), sel_select_count},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("field"), sel_select_field, CONSUME_NEXT_INT},
	{ sel_select_field, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_select_field, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("row"), sel_select_row, CONSUME_NEXT_INT},
	{ sel_select_row, SEL_PARAM_STR, STR_STATIC_INIT("field"), sel_select_row_field, CONSUME_NEXT_INT},
	{ sel_select_row_field, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_select_row_field, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	
	{ sel_dbops, SEL_PARAM_STR, STR_STATIC_INIT("fetch"), sel_fetch, CONSUME_NEXT_STR},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("row_no"), sel_fetch_cur_row_no, 0},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("count"), sel_fetch_count, 0},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("is_empty"), sel_fetch_is_empty, 0},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("field"), sel_fetch_field, CONSUME_NEXT_INT},
	{ sel_fetch_field, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_fetch_field, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
		
	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

static int dbops_close_query_func(struct sip_msg* m, char* handle, char* dummy) {
	struct dbops_handle *a = (void *)handle;
	if (a->result) {
		db_res_free(a->result);
		a->result = 0;
	}
	return 1;
}

static int dbops_pre_script_cb(struct sip_msg *msg, unsigned int flags, void *param) {
	xlbuf_tail = xlbuf;
	return 1;
}

static int dbops_post_script_cb(struct sip_msg *msg, unsigned int flags, void *param) {
	struct dbops_handle *a;
	for (a = dbops_handles; a; a=a->next) {
		dbops_close_query_func(msg, (char*) a, 0);
	}
	return 1;
}

static int init_action(struct dbops_action* action) {
	int res, i;

	if (!action->db_url)
		action->db_url = db_url;

	res = parse_xlstr(&action->table);
	if (res < 0) return res;
	for (i=0; i<action->field_count; i++) {
		res = parse_xlstr(&action->fields[i]);
		if (res < 0) return res;
	}

	for (i=0; i<action->where_count; i++) {
		res = parse_xlstr(&action->wheres[i]);
		if (res < 0) return res;
	}
	for (i=0; i<action->value_count; i++) {
		res = parse_xlstr(&action->values[i]);
		if (res < 0) return res;
	}
	res = parse_xlstr(&action->order);
	if (res < 0) return res;
	res = parse_xlstr(&action->raw);

        return res;
}

static int dbops_fixup_func(void** param, int init_act) {
	struct dbops_action **p, *a;
	char *c;
	int res;

	/* check if is it a declare_no that references to declare_xxxx */
	c = *param;
	eat_spaces(c);
	*param = c;
	eat_alphanum(c);
	if (*c == 0) {
		a = find_action_by_name(*param, -1);
		if (!a) {
			ERR(MODULE_NAME": fixup_func: query (%s) not declared\n", (char*) *param);
			return -1;
		}
		*param = (void*) a;
		return 0;
	}

	for (p = &dbops_actions; *p; p=&(*p)->next);	/* add at the end of list */
	res = parse_ops(*param, p, init_act == 0 /* declare query has name */);
	if (res < 0) return res;
	/* pkg_free(*param); do not free it!*/
	*param = (void*) *p;
	if (init_act)
		return init_action(*p);   /* fixup is acquired after init_mod() therefore initialize new action */
	else
		return 0;
}

static int mod_init(void) {
	struct dbops_action* p;

	xlbuf = pkg_malloc((xlbuf_size+1)*sizeof(char));
	if (!xlbuf) {
		ERR(MODULE_NAME": out of memory, cannot create xlbuf\n");
		return E_OUT_OF_MEM;
	}

	for (p=dbops_actions; p; p=p->next) {
		int res;
		res = init_action(p);
		if (res < 0)
			return res;
	}

	register_script_cb(dbops_pre_script_cb, REQUEST_CB | ONREPLY_CB | PRE_SCRIPT_CB, 0);
	register_script_cb(dbops_post_script_cb, REQUEST_CB | ONREPLY_CB | POST_SCRIPT_CB, 0);
	register_select_table(sel_declaration);

	return 0;
}


static int build_match(db_fld_t** match, struct dbops_action* p)
{
	int i;
	db_fld_t* newp;

	if (!p->where_count) {
		*match = NULL;
		return 0;
	}

	newp = (db_fld_t*)pkg_malloc(sizeof(db_fld_t) * (p->where_count + 1));
	if (newp == NULL) {
		ERR(MODULE_NAME": No memory left\n");
		return -1;
	}
	memset(newp, '\0', sizeof(db_fld_t) * p->where_count);

	for(i = 0; i < p->where_count; i++) {
		newp[i].name = p->wheres[i].s;
		newp[i].type = p->value_types[i];

		if (i < p->op_count) {
			if (!strcmp(p->ops[i].s, "=")) {
				newp[i].op = DB_EQ;
			} else if (!strcmp(p->ops[i].s, "<=")) {
				newp[i].op = DB_LEQ;
			} else if (!strcmp(p->ops[i].s, "<")) {
				newp[i].op = DB_LT;
			} else if (!strcmp(p->ops[i].s, ">")) {
				newp[i].op = DB_GT;
			} else if (!strcmp(p->ops[i].s, ">=")) {
				newp[i].op = DB_GEQ;
			} else if (!strcmp(p->ops[i].s, "<>")) {
				newp[i].op = DB_NE;
			} else if (!strcmp(p->ops[i].s, "!=")) {
				newp[i].op = DB_NE;
			} else {
				ERR(MODULE_NAME": Unsupported operator type: %s\n", p->ops[i].s);
				pkg_free(newp);
				return -1;
			}
		}
		else {
			newp[i].op = DB_EQ;
		}
	}
	newp[i].name = NULL;
	
	*match = newp;
	return 0;
}


static int build_result(db_fld_t** result, struct dbops_action* p)
{
	int i;
	db_fld_t* newp;

	if (!p->field_count) {
		*result = NULL;
		return 0;
	}

	newp = (db_fld_t*)pkg_malloc(sizeof(db_fld_t) * (p->field_count + 1));
	if (newp == NULL) {
		ERR(MODULE_NAME": No memory left\n");
		return -1;
	}
	memset(newp, '\0', sizeof(db_fld_t) * p->field_count);

	for(i = 0; i < p->field_count; i++) {
		newp[i].name = p->fields[i].s;
		newp[i].type = DB_NONE;
	}
	newp[i].name = NULL;
	*result = newp;
	return 0;
}


static int build_params(db_fld_t** params, struct dbops_action* p)
{
	int i;
	db_fld_t* newp;

	if (!p->value_count) {
		*params = NULL;
		return 0;
	}

	newp = (db_fld_t*)pkg_malloc(sizeof(db_fld_t) * (p->value_count - p->where_count + 1));
	if (newp == NULL) {
		ERR(MODULE_NAME": No memory left\n");
		return -1;
	}
	memset(newp, '\0', sizeof(db_fld_t) * p->value_count);

	for(i = 0; i < p->value_count - p->where_count; i++) {
		newp[i].name = (i < p->field_count)?p->fields[i].s:""; /* in case of raw query it's empty */
		newp[i].type = p->value_types[i];
	}
	newp[i].name = NULL;
	
	*params = newp;
	return 0;
}



static int init_db(struct dbops_action* p)
{
	db_fld_t* matches = NULL, *result = NULL, *values = NULL;
	int type, i;

	DEBUG(MODULE_NAME": init_db: query: %s(%d)\n", p->query_name, p->query_no);
	if (p->db_url == NULL) {
		ERR(MODULE_NAME": No database URL specified\n");
		return -1;
	}
	p->ctx = db_ctx(MODULE_NAME);
	if (p->ctx == NULL) {
		ERR(MODULE_NAME": Error while initializing database layer\n");
		return -1;
	}
	
	if (db_add_db(p->ctx, p->db_url) < 0) return -1;
	if (db_connect(p->ctx) < 0) return -1;

	if (p->is_raw_query) {
		type = DB_SQL;
		if (build_params(&matches, p) < 0) return -1;
	}
	else {
		switch(p->operation) {
		case INSERT_OPS:
		case REPLACE_OPS:
			type = DB_PUT;
			if (build_params(&values, p) < 0) return -1;
			break;

		case UPDATE_OPS:
			type = DB_UPD;
			if (build_match(&matches, p) < 0) return -1;
			if (build_params(&values, p) < 0) {
				if (matches) pkg_free(matches);
				return -1;
			}
			break;

		case DELETE_OPS:
			type = DB_DEL;

			if (build_match(&matches, p) < 0) return -1;
			break;

		case OPEN_QUERY_OPS:
			type = DB_GET;
			if (build_match(&matches, p) < 0) return -1;
			if (build_result(&result, p) < 0) {
				if (matches) pkg_free(matches);
				return -1;
			}
			break;
		default:
			BUG("Unknown operation %d\n", p->operation);
			return -1;
		}
	}

	p->cmd = db_cmd(type, p->ctx, p->table.s, result, matches, values);
	if (p->cmd == NULL) {
		ERR(MODULE_NAME": init_db: query: %s(%d), error while compiling database query\n", p->query_name, p->query_no);
		if (values) pkg_free(values);
		if (matches) pkg_free(matches);
		if (result) pkg_free(result);
		db_disconnect(p->ctx);
		db_ctx_free(p->ctx);
		return -1;
	}
	if (values) pkg_free(values);
	if (matches) pkg_free(matches);
	if (result) pkg_free(result);

	for (i=0; i<p->extra_ops_count; i++) {
		char *end;
		DEBUG(MODULE_NAME": init_db: query_no: %s(%d), setopt('%s', %i, '%s'\n", p->query_name, p->query_no, p->extra_ops[i].name, p->extra_ops[i].type, p->extra_ops[i].value);
		switch (p->extra_ops[i].type) {
			case DB_NONE: 
				/* set null ?? */
				break;			
			case DB_DATETIME: {
				time_t v;
				v = strtol(p->extra_ops[i].value, &end, 10);
				if (db_setopt(p->cmd, p->extra_ops[i].name, v) < 0) return -1;
				break;
			}
			case DB_INT: {
				int v;
				v = strtol(p->extra_ops[i].value, &end, 10);
				if (db_setopt(p->cmd, p->extra_ops[i].name, v) < 0) return -1;
				break;
			}
			case DB_FLOAT: {
				float v;
				#ifdef  __USE_ISOC99
				v = strtof(p->extra_ops[i].value, &end);
				#else
				v = strtod(p->extra_ops[i].value, &end);
				#endif
				if (db_setopt(p->cmd, p->extra_ops[i].name, v) < 0) return -1;
				break;
			}
			case DB_DOUBLE: {
				double v;
				v = strtod(p->extra_ops[i].value, &end);
				if (db_setopt(p->cmd, p->extra_ops[i].name, v) < 0) return -1;
				break;
			}
			case DB_CSTR:
				if (db_setopt(p->cmd, p->extra_ops[i].name, p->extra_ops[i].value) < 0) return -1;
				break;
		default:
				BUG("Unknown extra_op type: %d\n", p->extra_ops[i].type);
				return -1;
		}
	}
	return 0;
}

static int child_init(int rank) {
	struct dbops_action *p, *p2;
	if (rank!=PROC_INIT && rank != PROC_MAIN && rank != PROC_TCP_MAIN) {
		for (p=dbops_actions; p; p=p->next) {
			for (p2=dbops_actions; p!=p2; p2=p2->next) {  /* check if database is already opened */
				if (strcmp(p->db_url, p2->db_url) == 0) {
					p->ctx = p2->ctx;
					break;
				}
			}

			/* FIXME: Initialize database layer here */

			if (init_db(p) < 0) {
				ERR(MODULE_NAME": CHILD INIT #err\n");

				return -1;
			}
		}
	}
	return 0;
}

static int dbops_close_query_fixup(void** param, int param_no) {
	struct dbops_handle *a;
	a = find_handle_by_name((char*) *param, -1);
	if (!a) {
		ERR(MODULE_NAME": handle '%s' is not declared\n", (char*) *param);
		return E_CFG;
	}
	pkg_free (*param);
	*param = (void*) a;
	return 0;
}

static int dbops_query_fixup(void** param, int param_no) {
	int res = 0;
	if (param_no == 1) {
		res = dbops_fixup_func(param, 1);
		if (res < 0) return res;
		if (((struct dbops_action*)*param)->operation == OPEN_QUERY_OPS) {
			if (fixup_get_param_count(param, param_no) != 2) {
				ERR(MODULE_NAME": query_fixup: SELECT query requires 2 parameters\n");
				return E_CFG;
			}
		}
		else {
			if (fixup_get_param_count(param, param_no) != 1) {
				ERR(MODULE_NAME": query_fixup: non SELECT query requires only 1 parameter\n");
				return E_CFG;
			}
		}
	}
	else if (param_no == 2) {
		return dbops_close_query_fixup(param, param_no);
	}
	return res;
}

static int dbops_query_func(struct sip_msg* m, char* dbops_action, char* handle) {
	if ( ((struct dbops_action*) dbops_action)->operation == OPEN_QUERY_OPS ) {
		int res;
		struct dbops_handle *a = (void*) handle;
		dbops_close_query_func(m, handle, 0);
		res = dbops_func(m, (void*) dbops_action);
		if (res < 0) return res;
		a->action = (struct dbops_action*) dbops_action;
		a->cur_row_no = -1;
		a->result = ((struct dbops_action*) dbops_action)->result;
		res = do_seek(((struct dbops_action*) dbops_action)->result, &a->cur_row_no, 0);
		if (res < 0) return res;
		return 1;
	}
	else
		return dbops_func(m, (void*) dbops_action);
}

static int dbops_seek_fixup(void** param, int param_no) {
	if (param_no == 1) {
		return dbops_close_query_fixup(param, param_no);
	}
	else if (param_no == 2) {
		return fixup_int_12(param, param_no);
	}
	return 0;
}

static int dbops_seek_func(struct sip_msg* m, char* handle, char* row_no) {
	int res, n;
	struct dbops_handle *a = (void *) handle;
	res = check_query_opened(a, "seek");
	if (res < 0) return res;

	if (get_int_fparam(&n, m, (fparam_t*) row_no) < 0) {
		return -1;
	}
	res = do_seek(a->result, &a->cur_row_no, n);
	if (res < 0) return res;
	return 1;
}

static int dbops_first_func(struct sip_msg* m, char* handle, char* row_no) {
	int res;
	struct dbops_handle *a = (void *) handle;
	res = check_query_opened(a, "first");
	if (res < 0) return res;
	
	a->cur_row_no = -1; /* force seek */
	res = do_seek(a->result, &a->cur_row_no, 0);
	if (res < 0) return res;
	return 1;
}

static int dbops_next_func(struct sip_msg* m, char* handle, char* row_no) {
	int res;
	struct dbops_handle *a = (void *) handle;
	res = check_query_opened(a, "next");
	if (res < 0) return res;
	
	res = do_seek(a->result, &a->cur_row_no, a->cur_row_no+1);
	if (res < 0) return res;
	return 1;
}

static int dbops_foreach_fixup(void** param, int param_no) {
	if (param_no == 1) {
		return dbops_close_query_fixup(param, param_no);
	}
	else if (param_no == 2) {
		int n;
		n = route_get(&main_rt, (char*) *param);
		if (n == -1) {
			ERR(MODULE_NAME": db_foreach: bad route\n");
			return E_CFG;
		}
		pkg_free(*param);
		*param=(void*) (unsigned long) n;
	}
	return 0;
}


static int dbops_foreach_func(struct sip_msg* m, char* handle, char* route_no) {
	int res;
	db_rec_t* rec;
	struct dbops_handle *a = (void *) handle;
	struct run_act_ctx ra_ctx;
	
	if ((long)route_no >= main_rt.idx) {
		BUG("invalid routing table number #%ld of %d\n", (long) route_no, main_rt.idx);
		return -1;
	}
	if (!main_rt.rlist[(long) route_no]) {
		WARN(MODULE_NAME": route not declared (hash:%ld)\n", (long) route_no);
		return -1;
	}
	res = check_query_opened(a, "for_each");
	if (res < 0) return res;

	res = -1;
	a->cur_row_no = 0;
	for(rec = db_first(a->result);
			rec != NULL;
			rec = db_next(a->result),
			a->cur_row_no++) {
		/* exec the routing script */
		init_run_actions_ctx(&ra_ctx);
		res = run_actions(&ra_ctx, main_rt.rlist[(long) route_no], m);
		if (res <= 0) break;
	}
	if (!rec) a->cur_row_no = -1;
	return res;
}

static int declare_query(modparam_t type, char* param) {
	void* p = param;
	return dbops_fixup_func(&p, 0);	/* add at the end of the action list */
}

static int declare_handle(modparam_t type, char* param) {
	struct dbops_handle *a;
	if (strlen(param) == 0) {
		ERR(MODULE_NAME": declare_handle: handle name is empty\n");
		return E_CFG;
	}
	a = find_handle_by_name(param, -1);
	if (a) {
		ERR(MODULE_NAME": declare_handle: handle '%s' already exists\n", param);
		return E_CFG;
	}
	
	a = pkg_malloc(sizeof(*a));
	if (!a) {
		ERR(MODULE_NAME": Out od memory\n");
		return E_OUT_OF_MEM;
	}
	memset(a, 0, sizeof(*a));
	a->handle_name = param;
	a->next = dbops_handles;
	dbops_handles = a;
	return 0;
}

static int dbops_proper_func(struct sip_msg* m, char* dummy1, char* dummy2) {
	dbops_pre_script_cb(m, 0, NULL);
	dbops_post_script_cb(m, 0, NULL);
	return 1;
}

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{MODULE_NAME2"_query", dbops_query_func, 1, dbops_query_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME2"_query", dbops_query_func, 2, dbops_query_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME2"_first", dbops_first_func, 1, dbops_close_query_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME2"_next", dbops_next_func, 1, dbops_close_query_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME2"_seek", dbops_seek_func, 2, dbops_seek_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME2"_close", dbops_close_query_func, 1, dbops_close_query_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME2"_foreach", dbops_foreach_func, 2, dbops_foreach_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{MODULE_NAME2"_proper", dbops_proper_func, 0, 0, FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",    PARAM_STRING, &db_url},
	{"declare_query", PARAM_STRING|PARAM_USE_FUNC, (void*) declare_query},
	{"declare_handle", PARAM_STRING|PARAM_USE_FUNC, (void*) declare_handle},
	{"xlbuf_size", PARAM_INT, &xlbuf_size},
	{0, 0, 0}
};


struct module_exports exports = {
	MODULE_NAME,
	cmds,        /* Exported commands */
	0,	     /* RPC */
	params,      /* Exported parameters */
	mod_init,           /* module initialization function */
	0,           /* response function*/
	0,           /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};
