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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "../../db/db.h"
#include "../../select.h"
#include "../../script_cb.h"
#include "../xlog/xl_lib.h"
#include "../../route.h"
#include "../../action.h"
#include "../../ut.h"


MODULE_VERSION

static char* db_url = DEFAULT_DB_URL;
static int xlbuf_size = 4096;
static int max_queries = 10;   /* max possible opened queries */

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

struct dbops_action {
	char *db_url;
	db_func_t dbf;
        db_con_t *dbh;

	enum dbops_type operation;
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
	db_type_t *value_types;
	struct xlstr order;
	struct xlstr raw;

	db_res_t* result;   /* result of SELECT */

	struct dbops_action* next;
};

struct dbops_open_query {
	struct dbops_action* action;
	db_res_t* result;
	int cur_row_no;
};


/* list of all operations */
static struct dbops_action* dbops_actions = 0;

/* list of opened queries, close them in post script callback */
struct dbops_open_query* open_queries = 0;

#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
	(_p)++;}

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
		LOG(L_ERR, "ERROR: db_ops: string '%s' is not terminated\n", *s);
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
		LOG(L_ERR, "ERROR: db_ops: split_fields: not enough pkg memory\n");
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

static int parse_ops(char* act_s, struct dbops_action** action) {
	int res = 0, i;
	char *c, *s, *part;

	s = act_s;
	*action = pkg_malloc(sizeof(**action));
	if (!*action) return E_OUT_OF_MEM;
	memset(*action, 0, sizeof(**action));

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
		LOG(L_ERR, "db_ops: parse_ops: unknown type of query '%s'\n", part);
		return E_CFG;
	}
	if ((*action)->is_raw_query) {
		*c = ' ';
		(*action)->raw.s = part;
		DBG("db_ops: oper:%d database:'%s' rawtable:'%s'\n", (*action)->operation, (*action)->db_url, (*action)->raw.s);
		return 0;
	}

	eat_spaces(s);
	c = s;
	while ( (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || (*c == '_') ) {
		c++;
	}
	if (*c == ':') { /* database part is optional*/
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

	if (!*part) {
		LOG(L_ERR, "ERROR: db_ops: table not specified near '%s' in '%s'\n", s, act_s);
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
				LOG(L_ERR, "ERROR: dbops: no field specified near '%s' ?n '%s'\n", part, act_s);
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
			/* no break; */
		case REPLACE_OPS:
		case INSERT_OPS:
		case DELETE_OPS:
			res = split_fields(part, &(*action)->value_count, &(*action)->values);
			if (res < 0) return res;
			(*action)->value_types = pkg_malloc( (*action)->value_count*sizeof(*((*action)->value_types)));
			if (!(*action)->value_types) {
				LOG(L_ERR, "ERROR: db_ops: parse_ops: not enough pkg memory\n");
				return E_OUT_OF_MEM;
			}
			for (i=0; i<(*action)->value_count; i++) {
				if ((*action)->values[i].s && (*action)->values[i].s[0] && (*action)->values[i].s[1]==':') {
					switch ((*action)->values[i].s[0]) {
						case 't':
							(*action)->value_types[i] = DB_DATETIME;
							break;
						case 'i':
							(*action)->value_types[i] = DB_INT;
							break;
						case 'f':
							(*action)->value_types[i] = DB_FLOAT;
							break;
						case 'd':
							(*action)->value_types[i] = DB_DOUBLE;
							break;
						case 's':
						default:
							(*action)->value_types[i] = DB_STRING;
							break;
					}
					(*action)->values[i].s+=2;
				}
				else {
					(*action)->value_types[i] = DB_STRING;
				}
			}
			break;
		default:;
	}
	if (*s) {
		LOG(L_ERR, "ERROR: db_ops: parse_ops: too many parameters/parts, remaining '%s' in '%s'\n", s, act_s);
		return E_CFG;
	}
	/* check num of fields */
	if ((((*action)->operation==OPEN_QUERY_OPS)?0:(*action)->field_count)+(*action)->where_count != (*action)->value_count) {
		LOG(L_ERR, "ERROR: db_ops: parse_ops: number of values does not correspond to number of fields (%d+%d!=%d) in '%s'\n", ((*action)->operation==OPEN_QUERY_OPS)?0:(*action)->field_count,  (*action)->where_count, (*action)->value_count, act_s);
		return E_CFG;
	}

	DBG("db_ops: oper:%d database:'%s' table:'%s' 'field#:'%d' where#:'%d' order:'%s' value#:%d\n", (*action)->operation, (*action)->db_url, (*action)->table.s, (*action)->field_count,  (*action)->where_count, (*action)->order.s, (*action)->value_count);
	return 0;
}

static int parse_xlstr(struct xlstr* s) {

	if (!s->s) return 0;
	if (!strchr(s->s, '%')) return 0;
	/* probably xl_log formating */

	if (!xl_print) {
		xl_print=(xl_print_log_f*)find_export("xprint", NO_SCRIPT, 0);

		if (!xl_print) {
			LOG(L_CRIT,"ERROR: db_ops: cannot find \"xprint\", is module xlog loaded?\n");
			return E_UNSPEC;
		}
	}

	if (!xl_parse) {
		xl_parse=(xl_parse_format_f*)find_export("xparse", NO_SCRIPT, 0);

		if (!xl_parse) {
			LOG(L_CRIT,"ERROR: db_ops: cannot find \"xparse\", is module xlog loaded?\n");
			return E_UNSPEC;
		}
	}

	if (!xl_nul) {
		xl_getnul=(xl_get_nulstr_f*)find_export("xnulstr", NO_SCRIPT, 0);
		if (xl_getnul)
			xl_nul=xl_getnul();

		if (!xl_nul){
			LOG(L_CRIT,"ERROR: db_ops: cannot find \"xnulstr\", is module xlog loaded?\n");
			return E_UNSPEC;
		}
	else
		LOG(L_INFO,"INFO: xlog null is \"%.*s\"\n", xl_nul->len, xl_nul->s);
	}

	if(xl_parse(s->s, &s->xlfmt) < 0) {
		LOG(L_ERR, "ERROR: db_ops: wrong format '%s'\n", s->s);
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
			LOG(L_ERR, "ERROR: db_ops: eval_xlstr: Error while formating result\n");
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

static int dbops_func(struct sip_msg* m, struct dbops_action* action) {
	void* buf;
	db_key_t *wheres, *fields, order;
	db_op_t *ops;
	db_val_t *vals;
	int res, i;

	if (action->is_raw_query) {
		DBG("db_ops: dbops_func(raw, %d, '%s'\n", action->operation, action->raw.s);
		res = eval_xlstr(m, &action->raw);
		if (res < 0) return res;
		if (action->dbf.raw_query(action->dbh, action->raw.s, action->operation==OPEN_QUERY_OPS?&action->result:0) < 0) {
			LOG(L_ERR, "ERROR: db_ops: database operation (%d) error, raw: '%s'\n", action->operation, action->raw.s);
			return -1;
		}
		return 1;
	}

	DBG("db_ops: dbops_func(%d, '%s', %d, %d, %d)\n", action->operation, action->table.s, action->field_count, action->where_count, action->value_count);
	res = eval_xlstr(m, &action->table);
	if (res < 0) return res;
	if (action->dbf.use_table(action->dbh, action->table.s) < 0) {
		LOG(L_ERR, "ERROR: db_ops: func: Error while using table '%s'\n", action->table.s);
		return -1;
	}

	buf = pkg_malloc((action->field_count+action->where_count)*sizeof(db_key_t) + action->value_count*sizeof(db_val_t) + action->where_count*sizeof(db_op_t) );
	if (!buf) {
		LOG(L_ERR, "ERROR: db_ops: func: cannot allocate memory for keys and values\n");
		res = E_OUT_OF_MEM;
		goto cleanup;
	}
	wheres = buf;
	fields = &wheres[action->where_count];
	vals = (void*) &fields[action->field_count];
	ops = (void*) &vals[action->value_count];

	for (i=0; i<action->field_count; i++) {
		res = eval_xlstr(m, &action->fields[i]);
		if (res < 0) goto cleanup;
		fields[i] = action->fields[i].s;
	}
	for (i=0; i<action->where_count; i++) {
		res = eval_xlstr(m, &action->wheres[i]);
		if (res < 0) goto cleanup;
		wheres[i] = action->wheres[i].s;

		if (i < action->op_count) {
			res = eval_xlstr(m, &action->ops[i]);
			if (res < 0) goto cleanup;
			ops[i] = action->ops[i].s;
		}
		else {
			ops[i] = OP_EQ;
		}
	}

	for (i=0; i<action->value_count; i++) {
		char *end;
		res = eval_xlstr(m, &action->values[i]);
		if (res < 0) goto cleanup;
		vals[i].nul = !action->values[i].s || !action->values[i].s[0];
		switch (action->value_types[i]) {
			case DB_DATETIME:
				if (!vals[i].nul)
					vals[i].val.time_val = strtol(action->values[i].s, &end, 10);
				break;
			case DB_INT:
				if (!vals[i].nul)
					vals[i].val.int_val = strtol(action->values[i].s, &end, 10);
				break;
			case DB_FLOAT:
				if (!vals[i].nul)
				#ifdef  __USE_ISOC99
					vals[i].val.float_val = strtof(action->values[i].s, &end);
				#else
					vals[i].val.float_val = strtod(action->values[i].s, &end);
				#endif
				break;
			case DB_DOUBLE:
				if (!vals[i].nul)
					vals[i].val.double_val = strtod(action->values[i].s, &end);
				break;
			case DB_STRING:
				vals[i].val.string_val = action->values[i].s;
				vals[i].nul = 0;
				break;
			default:
				BUG("Unknown value type: %d\n", action->value_types[i]);
				goto err;
		}
		vals[i].type = action->value_types[i];
	}

	switch (action->operation) {
		case OPEN_QUERY_OPS:
			if (action->order.s) {
				res = eval_xlstr(m, &action->order);
				if (res < 0) return res;
			}
			order = action->order.s;
			if (action->dbf.query(action->dbh, wheres, ops, vals, fields, action->where_count, action->field_count, order, &action->result) < 0) goto err;
			break;
		case INSERT_OPS:
			if (action->dbf.insert(action->dbh, fields, vals, action->field_count) < 0) goto err;
			break;
		case UPDATE_OPS:
			if (action->dbf.update(action->dbh, wheres, ops, &vals[action->field_count], fields, vals, action->where_count, action->field_count) < 0) goto err;
			break;
		case REPLACE_OPS:
			if (action->dbf.replace(action->dbh, fields, vals, action->field_count) < 0) goto err;
			break;
		case DELETE_OPS:
			if (action->dbf.delete(action->dbh, wheres, ops, vals, action->where_count) < 0) goto err;
			break;
		default:;
	}
	res = 1;
cleanup:
	pkg_free(buf);
	return res;
err:
	LOG(L_ERR, "ERROR: db_ops: database operation (%d) error, table: '%s'\n", action->operation, action->table.s);
	res = -1;
	goto cleanup;
}

static int sel_get_field(str* res, int row_no, int field_no, db_res_t* result) {
/* return string in static buffer, I'm not sure if local static variable is OK, e.g. when comparing 2 selects */
	int len;
	len = xlbuf_size-(xlbuf_tail-xlbuf);
	res->s = xlbuf_tail;
	res->len = 0;
	if (field_no == -2) {  /* cur_row_no */
		res->len = snprintf(res->s, len, "%d", row_no);
	}
	else if (field_no < 0) {  /* count(*) */
		res->len = snprintf(res->s, len, "%d", RES_ROW_N(result));
	}
	else {
		if ( (row_no >= 0 && row_no >= RES_ROW_N(result)) || (row_no < 0 && -row_no > RES_ROW_N(result)) ) {
			LOG(L_ERR, "ERROR: db_ops: row (%d) does not exist, num rows: %d\n", row_no, RES_ROW_N(result));
			return -1;
		}
		if (field_no >= RES_COL_N(result)) {
			LOG(L_ERR, "ERROR: db_ops: field (%d) does not exist, num fields: %d\n", field_no, RES_COL_N(result));
			return -1;
		}
		if (row_no < 0) {
			row_no += RES_ROW_N(result);
		}

		if (!VAL_NULL(result->rows[row_no].values+field_no)) {
			switch (VAL_TYPE(result->rows[row_no].values+field_no)) {
				case DB_INT:
					res->len = snprintf(res->s, len, "%d", VAL_INT(result->rows[row_no].values+field_no));
					break;
				case DB_FLOAT:
					res->len = snprintf(res->s, len, "%f", VAL_FLOAT(result->rows[row_no].values+field_no));
					break;
				case DB_DOUBLE:
					res->len = snprintf(res->s, len, "%f", VAL_DOUBLE(result->rows[row_no].values+field_no));
					break;
				case DB_STR:
					res->len = snprintf(res->s, len, "%.*s", VAL_STR(result->rows[row_no].values+field_no).len, VAL_STR(result->rows[row_no].values+field_no).s);
					break;
				case DB_BLOB:
					res->len = snprintf(res->s, len, "%.*s", VAL_BLOB(result->rows[row_no].values+field_no).len, VAL_STR(result->rows[row_no].values+field_no).s);
					break;
				case DB_STRING:
					res->len = snprintf(res->s, len, "%s", VAL_STRING(result->rows[row_no].values+field_no));
					break;
				case DB_DATETIME:
					res->len = snprintf(res->s, len, "%u", (unsigned int) VAL_TIME(result->rows[row_no].values+field_no));
					break;
				case DB_BITMAP:
					res->len = snprintf(res->s, len, "%u", (unsigned int) VAL_BITMAP(result->rows[row_no].values+field_no));
					break;
				default:
					break;
			}
		}
	}
	xlbuf_tail += res->len;
	return 0;
}

static int sel_do_select(str* result, int query_no, int row_no, int field_no, struct sip_msg* msg) {
	struct dbops_action *a;
	int i, res;

	if (query_no < 0) {
		LOG(L_ERR, "ERROR: db_ops: select: query_no is negative (%d)\n", query_no);
		return -1;
	}
	for (a=dbops_actions, i=query_no; i>0 && a && a->operation == OPEN_QUERY_OPS; a = a->next, i--);
	if (!a || a->operation != OPEN_QUERY_OPS) {
		LOG(L_ERR, "ERROR: db_ops: select: query not found. Check declare_query params (%d)\n", query_no);
		return -1;
	}
	res = dbops_func(msg, a);
	if (res < 0) return res;
	res = sel_get_field(result, row_no, field_no, a->result);
	a->dbf.free_result(a->dbh, a->result);
	return res;
}

static int sel_do_fetch(str* result, int query_no, int row_no, int field_no, struct sip_msg* msg) {

	if (query_no < 0 || query_no >= max_queries) {
		LOG(L_ERR, "ERROR: db_ops: fetch: query_no (%d) must be from interval <0,%d)\n", query_no, max_queries);
		return -1;
	}
	if (!open_queries[query_no].result) {
		LOG(L_ERR, "ERROR: db_ops: fetch: query (%d) is not opened. Use db_query() first\n", query_no);
		return -1;
	}
	return sel_get_field(result, open_queries[query_no].cur_row_no+row_no, field_no, open_queries[query_no].result);
}

static int sel_dbops(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	return 0;
}

static int sel_select(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, s->params[2].v.i, 0, 0, msg);
}

static int sel_select_count(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, s->params[2].v.i, 0, -1, msg);
}

static int sel_select_field(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, s->params[2].v.i, 0, s->params[4].v.i, msg);
}

static int sel_select_row(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, s->params[2].v.i, s->params[4].v.i, 0, msg);
}

static int sel_select_row_field(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_select(res, s->params[2].v.i, s->params[4].v.i, s->params[6].v.i, msg);
}

static int sel_fetch(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, s->params[2].v.i, 0, 0, msg);
}

static int sel_fetch_count(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, s->params[2].v.i, 0, -1, msg);
}

static int sel_fetch_cur_row_no(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, s->params[2].v.i, 0, -2, msg);
}

static int sel_fetch_field(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, s->params[2].v.i, 0, s->params[4].v.i, msg);
}

static int sel_fetch_row(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, s->params[2].v.i, s->params[4].v.i, 0, msg);
}

static int sel_fetch_row_field(str* res, select_t* s, struct sip_msg* msg) {
	return sel_do_fetch(res, s->params[2].v.i, s->params[4].v.i, s->params[6].v.i, msg);
}

SELECT_F(select_any_nameaddr)
SELECT_F(select_any_uri)

select_row_t sel_declaration[] = {
        { NULL, SEL_PARAM_STR, STR_STATIC_INIT("db"), sel_dbops, SEL_PARAM_EXPECTED},

	{ sel_dbops, SEL_PARAM_STR, STR_STATIC_INIT("query"), sel_select, CONSUME_NEXT_INT},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("count"), sel_select_count, 0},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("field"), sel_select_field, CONSUME_NEXT_INT},
	{ sel_select_field, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_select_field, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ sel_select, SEL_PARAM_STR, STR_STATIC_INIT("row"), sel_select_row, CONSUME_NEXT_INT},
	{ sel_select_row, SEL_PARAM_STR, STR_STATIC_INIT("field"), sel_select_row_field, CONSUME_NEXT_INT},
	{ sel_select_row_field, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_select_row_field, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},

	{ sel_dbops, SEL_PARAM_STR, STR_STATIC_INIT("fetch"), sel_fetch, CONSUME_NEXT_INT},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("count"), sel_fetch_count, 0},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("row_no"), sel_fetch_cur_row_no, 0},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("field"), sel_fetch_field, CONSUME_NEXT_INT},
	{ sel_fetch_field, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_fetch_field, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},
	{ sel_fetch, SEL_PARAM_STR, STR_STATIC_INIT("row"), sel_fetch_row, CONSUME_NEXT_INT},
	{ sel_fetch_row, SEL_PARAM_STR, STR_STATIC_INIT("field"), sel_fetch_row_field, CONSUME_NEXT_INT},
	{ sel_fetch_row_field, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},
	{ sel_fetch_row_field, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED | CONSUME_NEXT_STR},

        { NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

static int dbops_close_query_func(struct sip_msg* m, char* query_no, char* dummy) {

	if (open_queries[(long) query_no].result) {
		open_queries[(long) query_no].action->dbf.free_result(open_queries[(long) query_no].action->dbh, open_queries[(long) query_no].result);
		open_queries[(long) query_no].result = 0;
	}
	return 1;
}

static int dbops_pre_script_cb(struct sip_msg *msg, void *param) {
	xlbuf_tail = xlbuf;
	return 1;
}

static int dbops_post_script_cb(struct sip_msg *msg, void *param) {
	int i;
	for (i=0; i<max_queries; i++) {
		dbops_close_query_func(msg, (char*)(long) i, 0);
	}
	return 1;
}

static int init_action(struct dbops_action* action) {
	int cap, res, i;

	if (!action->db_url)
		action->db_url = db_url;
	if (bind_dbmod(action->db_url, &action->dbf) < 0) { /* Find database module */
		LOG(L_ERR, "ERROR: db_ops: mod_init: Can't bind database module '%s'\n", action->db_url);
		return -1;
	}
	if (action->is_raw_query) {
		cap = DB_CAP_RAW_QUERY;
	}
	else {
		switch (action->operation) {
			case OPEN_QUERY_OPS:
				cap = DB_CAP_QUERY;
				break;
			case INSERT_OPS:
				cap = DB_CAP_INSERT;
				break;
			case UPDATE_OPS:
				cap = DB_CAP_UPDATE;
				break;
			case REPLACE_OPS:
				cap = DB_CAP_REPLACE;
				break;
			case DELETE_OPS:
				cap = DB_CAP_DELETE;
				break;
			default:
				cap = 0;
		}
	}
	if (!DB_CAPABILITY(action->dbf, cap)) {
		LOG(L_ERR, "ERROR: db_ops: mod_init: Database module does not implement"
		" all functions (%d) needed by the module '%s'\n", action->operation, action->db_url);
		return -1;
	}
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
	int res, declare_no, i;

	/* check if is it a declare_no that references to declare_xxxx */
	c = *param;
	eat_spaces(c);
	*param = c;
	while (*c >= '0' && *c <= '9') c++;
	if (c != *param && *c==0) {
		declare_no = atoi((char*)*param);
		for (a=dbops_actions, i=declare_no; i>0 && a; a = a->next, i--);
		if (!a) {
			LOG(L_ERR, "ERROR: db_ops: fixup_func: query (%d) not declared\n", declare_no);
			return -1;
		}
		*param = (void*) a;
		return 0;
	}

	for (p = &dbops_actions; *p; p=&(*p)->next);	/* add at the end of list */
	res = parse_ops(*param, p);
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

	open_queries = pkg_malloc(max_queries*sizeof(*open_queries));
	if (!open_queries) {
		LOG(L_ERR, "ERROR: db_ops: could not allocate memory for open_queries\n");
		return E_OUT_OF_MEM;
	}
	memset(open_queries, 0, max_queries*sizeof(*open_queries));

	xlbuf = pkg_malloc((xlbuf_size+1)*sizeof(char));
	if (!xlbuf) {
		LOG(L_ERR, "ERROR: db_ops: out of memory, cannot create xlbuf\n");
		return E_OUT_OF_MEM;
	}

	for (p=dbops_actions; p; p=p->next) {
		int res;
		res = init_action(p);
		if (res < 0)
			return res;
	}

	register_script_cb(dbops_pre_script_cb, REQ_TYPE_CB | RPL_TYPE_CB| PRE_SCRIPT_CB, 0);
	register_script_cb(dbops_post_script_cb, REQ_TYPE_CB | RPL_TYPE_CB| POST_SCRIPT_CB, 0);
	register_select_table(sel_declaration);

	return 0;
}

static int child_init(int rank) {
	struct dbops_action *p, *p2;
	if (rank != PROC_MAIN && rank != PROC_TCP_MAIN) {
		for (p=dbops_actions; p; p=p->next) {
			for (p2=dbops_actions; p!=p2; p2=p2->next) {  /* check if database is already opened */
				if (strcmp(p->db_url, p2->db_url) == 0) {
					p->dbh = p2->dbh;
					break;
				}
			}
			if (!p->dbh) {
				p->dbh = p->dbf.init(p->db_url);   /* Get a new database connection */
			}
			if (!p->dbh) {
				LOG(L_ERR, "ERROR: db_ops: child_init(%d): Error while connecting database '%s'\n", rank, p->db_url);
				return -1;
			}
		}
	}
	return 0;
}

static int dbops_close_query_fixup(void** param, int param_no) {
	int /*res, */ n;
/*	res = fixup_int_12(param, param_no); was changed to return fparam_t* 
	if (res < 0) return res;
	n = (int) *param; */
	n = atoi((char*)*param);
	if (n < 0 || n >= max_queries) {
		LOG(LOG_ERR, "ERROR: db_ops: query handle (%d) must be in interval <0..%d)\n", n, max_queries);
		return E_CFG;
	}
	pkg_free (*param);
	*param=(void*) (unsigned long) n;
	return 0;
}

static int dbops_query_fixup(void** param, int param_no) {
	int res = 0;
	if (param_no == 1) {
		res = dbops_fixup_func(param, 1);
		if (res < 0) return res;
		if (((struct dbops_action*)*param)->operation == OPEN_QUERY_OPS) {
			if (fixup_get_param_count(param, param_no) != 2) {
				LOG(L_ERR, "ERROR: db_ops: query_fixup: SELECT query requires 2 parameters\n");
				return E_CFG;
			}
		}
		else {
			if (fixup_get_param_count(param, param_no) != 1) {
				LOG(L_ERR, "ERROR: db_ops: query_fixup: non SELECT query requires only 1 parameter\n");
				return E_CFG;
			}
		}
	}
	else if (param_no == 2) {
		return dbops_close_query_fixup(param, param_no);
	}
	return res;
}

static int dbops_query_func(struct sip_msg* m, char* dbops_action, char* query_no) {
	if ( ((struct dbops_action*) dbops_action)->operation == OPEN_QUERY_OPS ) {
		int res;
		dbops_close_query_func(m, query_no, 0);
		res = dbops_func(m, (void*) dbops_action);
		if (res < 0) return res;
		open_queries[(long) query_no].action = (struct dbops_action*) dbops_action;
		open_queries[(long) query_no].cur_row_no = 0;
		open_queries[(long) query_no].result = ((struct dbops_action*) dbops_action)->result;
		return 1;
	}
	else
		return dbops_func(m, (void*) dbops_action);
}

static int dbops_foreach_fixup(void** param, int param_no) {
/*	int res;*/
	if (param_no == 1) {
		int n;
/*		res = fixup_int_12(param, param_no); was changed to return fparam_t*
		if (res < 0) return res;
		n = (int) *param; */
		n = atoi((char*)*param);
		if (n < 0 || n >= max_queries) {
			LOG(LOG_ERR, "ERROR: db_ops: query handle (%d) must be in interval <0..%d)\n", n, max_queries);
			return E_CFG;
		}
		pkg_free(*param);
		*param=(void*)(unsigned long) n;
	}
	else if (param_no == 2) {
		int n;
		n = route_get(&main_rt, (char*) *param);
		if (n == -1) {
			LOG(L_ERR, "ERROR: db_foreach: bad route\n");
			return E_CFG;
		}
		pkg_free(*param);
		*param=(void*) (unsigned long) n;
	}
	return 0;
}


static int dbops_foreach_func(struct sip_msg* m, char* query_no, char* route_no) {
	int save_row_no, res;
	if ((long)route_no >= main_rt.idx) {
		BUG("invalid routing table number #%ld of %d\n", (long) route_no, main_rt.idx);
		return -1;
	}
	if (!main_rt.rlist[(long) route_no]) {
		LOG(L_WARN, "WARN: route not declared (hash:%ld)\n", (long) route_no);
		return -1;
	}
	if (!open_queries[(long) query_no].result) {
		LOG(L_ERR, "ERROR: db_ops: fetch: query (%ld) is not opened. Use db_query() first\n", (long) query_no);
		return -1;
	}
	res = -1;
	save_row_no = open_queries[(long) query_no].cur_row_no;
	for (open_queries[(long) query_no].cur_row_no=0; open_queries[(long) query_no].cur_row_no < RES_ROW_N(open_queries[(long) query_no].result); open_queries[(long) query_no].cur_row_no++) {
		/* exec the routing script */
		res = run_actions(main_rt.rlist[(long) route_no], m);
		run_flags &= ~RETURN_R_F; /* absorb returns */
		if (res <= 0) break;
	}
	open_queries[(long) query_no].cur_row_no = save_row_no;
	return res;
}

static int declare_query(modparam_t type, char* param) {
	void* p = param;
	return dbops_fixup_func(&p, 0);	/* add at the end of the action list */
}

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"db_query", dbops_query_func, 1, dbops_query_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{"db_query", dbops_query_func, 2, dbops_query_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{"db_close", dbops_close_query_func, 1, dbops_close_query_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{"db_foreach", dbops_foreach_func, 2, dbops_foreach_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",    PARAM_STRING, &db_url},
	{"declare_query", PARAM_STRING|PARAM_USE_FUNC, (void*) declare_query},
	{"xlbuf_size", PARAM_INT, &xlbuf_size},
	{"max_queries", PARAM_INT, &max_queries},
	{0, 0, 0}
};


struct module_exports exports = {
	"db_ops",
	cmds,        /* Exported commands */
	0,	     /* RPC */
	params,      /* Exported parameters */
	mod_init,           /* module initialization function */
	0,           /* response function*/
	0,           /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};

