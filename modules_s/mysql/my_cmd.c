/* 
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006-2007 iptelorg GmbH
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

#define _XOPEN_SOURCE 4     /* bsd */
#define _XOPEN_SOURCE_EXTENDED 1    /* solaris */
#define _SVID_SOURCE 1 /* timegm */

#include <strings.h>
#include <stdio.h>
#include <time.h>  /*strptime, XOPEN issue must be >=4 */
#include <string.h>
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../db/db_cmd.h"
#include "../../ut.h"
#include "my_con.h"
#include "my_fld.h"
#include "my_cmd.h"

#define STR_BUF_SIZE 256

enum {
	STR_DELETE,
	STR_INSERT,
	STR_UPDATE,
	STR_SELECT,
	STR_REPLACE,
	STR_WHERE,
	STR_IS,
	STR_AND,
	STR_OR,
	STR_ESC,
	STR_OP_EQ,
	STR_OP_LT,
	STR_OP_GT,
	STR_OP_LEQ,
	STR_OP_GEQ,
	STR_VALUES,
	STR_FROM
};

static str strings[] = {
	STR_STATIC_INIT("delete from "),
	STR_STATIC_INIT("insert into "),
	STR_STATIC_INIT("update "),
	STR_STATIC_INIT("select "),
	STR_STATIC_INIT("replace "),
	STR_STATIC_INIT(" where "),
	STR_STATIC_INIT(" is "),
	STR_STATIC_INIT(" and "),
	STR_STATIC_INIT(" or "),
	STR_STATIC_INIT("?"),
	STR_STATIC_INIT("="),
	STR_STATIC_INIT("<"),
	STR_STATIC_INIT(">"),
	STR_STATIC_INIT("<="),
	STR_STATIC_INIT(">="),
	STR_STATIC_INIT(") values ("),
	STR_STATIC_INIT(" from ")
};


#define APPEND_STR(p, str) do {		 \
	memcpy((p), (str).s, (str).len); \
	(p) += (str).len;				 \
} while(0)


#define APPEND_CSTR(p, cstr) do { \
    int _len = strlen(cstr);      \
	memcpy((p), (cstr), _len);	  \
	(p) += _len;				  \
} while(0)



static void my_cmd_free(db_cmd_t* cmd, struct my_cmd* payload)
{
	db_drv_free(&payload->gen);
	if (payload->query.s) pkg_free(payload->query.s);
	if (payload->st) mysql_stmt_close(payload->st);
	pkg_free(payload);
}


static int build_delete_query(str* query, db_cmd_t* cmd)
{
	db_fld_t* fld;
	int i;
	char* p;

	query->len = strings[STR_DELETE].len;
	query->len += cmd->table.len;

	if (!DB_FLD_EMPTY(cmd->params)) {
		query->len += strings[STR_WHERE].len;

		for(i = 0, fld = cmd->params; !DB_FLD_LAST(fld[i]); i++) {
			query->len += strlen(fld[i].name);

			switch(fld[i].op) {
			case DB_EQ:  query->len += strings[STR_OP_EQ].len; break;
			case DB_LT:  query->len += strings[STR_OP_LT].len; break;
			case DB_GT:  query->len += strings[STR_OP_GT].len; break;
			case DB_LEQ: query->len += strings[STR_OP_LEQ].len; break;
			case DB_GEQ: query->len += strings[STR_OP_GEQ].len; break;
			default:
				ERR("Unsupported db_fld operator %d\n", fld[i].op);
				return -1;
			}

			query->len += strings[STR_ESC].len;
			
			if (!DB_FLD_LAST(fld[i + 1])) query->len += strings[STR_AND].len;
		}
	}

	query->s = pkg_malloc(query->len + 1);
	if (query->s == NULL) {
		ERR("No memory left\n");
		return -1;
	}
	p = query->s;
	
	APPEND_STR(p, strings[STR_DELETE]);
	APPEND_STR(p, cmd->table);

	if (!DB_FLD_EMPTY(cmd->params)) {
		APPEND_STR(p, strings[STR_WHERE]);

		for(i = 0, fld = cmd->params; !DB_FLD_LAST(fld[i]); i++) {
			APPEND_CSTR(p, fld[i].name);

			switch(fld[i].op) {
			case DB_EQ:  APPEND_STR(p, strings[STR_OP_EQ]);  break;
			case DB_LT:  APPEND_STR(p, strings[STR_OP_LT]);  break;
			case DB_GT:  APPEND_STR(p, strings[STR_OP_GT]);  break;
			case DB_LEQ: APPEND_STR(p, strings[STR_OP_LEQ]); break;
			case DB_GEQ: APPEND_STR(p, strings[STR_OP_GEQ]); break;
			}
			
			APPEND_STR(p, strings[STR_ESC]);
			if (!DB_FLD_LAST(fld[i + 1])) APPEND_STR(p, strings[STR_AND]);
		}
	}
			
	*p = '\0';
	return 0;
}


static int build_select_query(str* query, db_cmd_t* cmd)
{
	db_fld_t* fld;
	int i;
	char* p;

	query->len = strings[STR_SELECT].len;

	if (DB_FLD_EMPTY(cmd->result)) {
		query->len += 1; /* "*" */
	} else {
		for(i = 0, fld = cmd->result; !DB_FLD_LAST(fld[i]); i++) {
			query->len += strlen(fld[i].name);
			if (!DB_FLD_LAST(fld[i + 1])) query->len += 1; /* , */
		}
	}
	query->len += strings[STR_FROM].len;
	query->len += cmd->table.len;

	if (!DB_FLD_EMPTY(cmd->params)) {
		query->len += strings[STR_WHERE].len;

		for(i = 0, fld = cmd->params; !DB_FLD_LAST(fld[i]); i++) {
			query->len += strlen(fld[i].name);

			switch(fld[i].op) {
			case DB_EQ:  query->len += strings[STR_OP_EQ].len; break;
			case DB_LT:  query->len += strings[STR_OP_LT].len; break;
			case DB_GT:  query->len += strings[STR_OP_GT].len; break;
			case DB_LEQ: query->len += strings[STR_OP_LEQ].len; break;
			case DB_GEQ: query->len += strings[STR_OP_GEQ].len; break;
			default:
				ERR("Unsupported db_fld operator %d\n", fld[i].op);
				return -1;
			}

			query->len += strings[STR_ESC].len;
			
			if (!DB_FLD_LAST(fld[i + 1])) query->len += strings[STR_AND].len;
		}
	}

	query->s = pkg_malloc(query->len + 1);
	if (query->s == NULL) {
		ERR("No memory left\n");
		return -1;
	}
	p = query->s;
	
	APPEND_STR(p, strings[STR_SELECT]);
	if (DB_FLD_EMPTY(cmd->result)) {
		*p++ = '*';
	} else {
		for(i = 0, fld = cmd->result; !DB_FLD_LAST(fld[i]); i++) {
			APPEND_CSTR(p, fld[i].name);
			if (!DB_FLD_LAST(fld[i + 1])) *p++ = ',';
		}
	}
	APPEND_STR(p, strings[STR_FROM]);
	APPEND_STR(p, cmd->table);

	if (!DB_FLD_EMPTY(cmd->params)) {
		APPEND_STR(p, strings[STR_WHERE]);

		for(i = 0, fld = cmd->params; !DB_FLD_LAST(fld[i]); i++) {
			APPEND_CSTR(p, fld[i].name);

			switch(fld[i].op) {
			case DB_EQ:  APPEND_STR(p, strings[STR_OP_EQ]);  break;
			case DB_LT:  APPEND_STR(p, strings[STR_OP_LT]);  break;
			case DB_GT:  APPEND_STR(p, strings[STR_OP_GT]);  break;
			case DB_LEQ: APPEND_STR(p, strings[STR_OP_LEQ]); break;
			case DB_GEQ: APPEND_STR(p, strings[STR_OP_GEQ]); break;
			}
			
			APPEND_STR(p, strings[STR_ESC]);
			if (!DB_FLD_LAST(fld[i + 1])) APPEND_STR(p, strings[STR_AND]);
		}
	}
			
	*p = '\0';
	return 0;
}


static int build_replace_query(str* query, db_cmd_t* cmd)
{
	db_fld_t* fld;
	int i;
	char* p;

	query->len = strings[STR_REPLACE].len;
	query->len += cmd->table.len;
	query->len += 2; /* " (" */

	for(i = 0, fld = cmd->params; !DB_FLD_LAST(fld[i]); i++) {
		query->len += strlen(fld[i].name);
		query->len += strings[STR_ESC].len;
		if (!DB_FLD_LAST(fld[i + 1])) query->len += 2; /* , twice */
	}
	query->len += strings[STR_VALUES].len;
	query->len += 1; /* ) */

	query->s = pkg_malloc(query->len + 1);
	if (query->s == NULL) {
		ERR("No memory left\n");
		return -1;
	}
	p = query->s;
	
	APPEND_STR(p, strings[STR_REPLACE]);
	APPEND_STR(p, cmd->table);
	*p++ = ' ';
	*p++ = '(';

	for(i = 0, fld = cmd->params; !DB_FLD_LAST(fld[i]); i++) {
		APPEND_CSTR(p, fld[i].name);
		if (!DB_FLD_LAST(fld[i + 1])) *p++ = ',';
	}
	APPEND_STR(p, strings[STR_VALUES]);

	for(i = 0, fld = cmd->params; !DB_FLD_LAST(fld[i]); i++) {
		APPEND_STR(p, strings[STR_ESC]);
		if (!DB_FLD_LAST(fld[i + 1])) *p++ = ',';
	}
	*p++ = ')';
	*p = '\0';
	return 0;
}


static inline int update_params(MYSQL_STMT* st, db_fld_t* params)
{
	int i;
	struct my_fld* fp; /* Current field payload */
	struct tm* t;

	/* Iterate through all the query parameters and update
	 * their values if needed
	 */

	/* FIXME: We are updating internals of the prepared statement here,
	 * this is probably not nice but I could not find another way of
	 * updating the pointer to the buffer without the need to run
	 * mysql_stmt_bind_param again (which would be innefficient
	 */

	for(i = 0; i < st->param_count; i++) {
		fp = DB_GET_PAYLOAD(params + i);

		fp->is_null = params[i].flags & DB_NULL;
		if (fp->is_null) continue;

		switch(params[i].type) {
		case DB_STR:
			st->params[i].buffer = params[i].v.str.s;
			fp->length = params[i].v.str.len;
			break;

		case DB_BLOB:
			st->params[i].buffer = params[i].v.blob.s;
			fp->length = params[i].v.blob.len;
			break;

		case DB_CSTR:
			st->params[i].buffer = (char*)params[i].v.cstr;
			fp->length = strlen(params[i].v.cstr);
			break;

		case DB_DATETIME:
			t = gmtime(&params[i].v.time);
			fp->time.second = t->tm_sec;
			fp->time.minute = t->tm_min;
			fp->time.hour = t->tm_hour;
			fp->time.day = t->tm_mday;
			fp->time.month = t->tm_mon + 1;
			fp->time.year = t->tm_year + 1900;
			break;
			
		case DB_NONE:
		case DB_INT:
		case DB_FLOAT:
		case DB_DOUBLE:
		case DB_BITMAP:
			/* No need to do anything for these types */
			break;

		}
	}

	return 0;
}



static inline int update_result(db_fld_t* result, MYSQL_STMT* st)
{
	int i;
	struct my_fld* rp; /* Payload of the current field in result */
	struct tm t;

	/* Iterate through all the fields returned by MySQL and convert
	 * them to DB API representation if necessary
	 */

	for(i = 0; i < st->field_count; i++) {
		rp = DB_GET_PAYLOAD(result + i);

		if (rp->is_null) {
			result[i].flags |= DB_NULL;
			continue;
		} else {
			result[i].flags &= ~DB_NULL;
		}

		switch(result[i].type) {
		case DB_STR:
			result[i].v.str.len = rp->length;
			break;

		case DB_BLOB:
			result[i].v.blob.len = rp->length;
			break;

		case DB_CSTR:
			result[i].v.cstr[rp->length] = '\0';
			break;

		case DB_DATETIME:
			memset(&t, '\0', sizeof(struct tm));
			t.tm_sec = rp->time.second;
			t.tm_min = rp->time.minute;
			t.tm_hour = rp->time.hour;
			t.tm_mday = rp->time.day;
			t.tm_mon = rp->time.month - 1;
			t.tm_year = rp->time.year - 1900;;

			/* Daylight saving information got lost in the database
			 * so let timegm to guess it. This eliminates the bug when
			 * contacts reloaded from the database have different time
			 * of expiration by one hour when daylight saving is used
			 */ 
			t.tm_isdst = -1;
#ifdef HAVE_TIMEGM
			result[i].v.time = timegm(&t);
#else
			result[i].v.time = _timegm(&t);
#endif /* HAVE_TIMEGM */
			break;

		case DB_NONE:
		case DB_INT:
		case DB_FLOAT:
		case DB_DOUBLE:
		case DB_BITMAP:
			/* No need to do anything for these types */
			break;
		}
	}

	return 0;
}


int my_cmd_write(db_res_t* res, db_cmd_t* cmd)
{
	struct my_cmd* mcmd;

	mcmd = DB_GET_PAYLOAD(cmd);
	if (mcmd->st->param_count && update_params(mcmd->st, cmd->params) < 0) return -1;
	if (mysql_stmt_execute(mcmd->st)) {
		ERR("Error while executing query: %s\n", mysql_stmt_error(mcmd->st));
		return -1;
	}
	return 0;
}


int my_cmd_read(db_res_t* res, db_cmd_t* cmd)
{
	struct my_cmd* mcmd;
   
	mcmd = DB_GET_PAYLOAD(cmd);
	if (mcmd->st->param_count && update_params(mcmd->st, cmd->params) < 0) return -1;
	if (mysql_stmt_execute(mcmd->st)) {
		ERR("Error while executing query: %s\n", mysql_stmt_error(mcmd->st));
		return -1;
	}
	return 0;
}


static int bind_params(MYSQL_STMT* st, db_fld_t* fld)
{
	int i, n;
	struct my_fld* f;
	MYSQL_BIND* params;

	/* Calculate the number of parameters */
	for(n = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[n]); n++);

	params = (MYSQL_BIND*)pkg_malloc(sizeof(MYSQL_BIND) * n);
	if (params == NULL) {
		ERR("No memory left\n");
		return -1;
	}
	memset(params, '\0', sizeof(MYSQL_BIND) * n);
	
	for(i = 0; i < n; i++) {
		f = DB_GET_PAYLOAD(fld + i);
		params[i].is_null = &f->is_null;
		/* We can do it for all the types here, mysql will ignore it
		 * for fixed-size types such as MYSQL_TYPE_LONG
		 */
		params[i].length = &f->length;
		switch(fld[i].type) {
		case DB_INT:
		case DB_BITMAP:
			params[i].buffer_type = MYSQL_TYPE_LONG;
			params[i].buffer = &fld[i].v.int4;
			break;

		case DB_FLOAT:
			params[i].buffer_type = MYSQL_TYPE_FLOAT;
			params[i].buffer = &fld[i].v.flt;
			break;
			
		case DB_DOUBLE:
			params[i].buffer_type = MYSQL_TYPE_DOUBLE;
			params[i].buffer = &fld[i].v.dbl;
			break;

		case DB_DATETIME:
			params[i].buffer_type = MYSQL_TYPE_DATETIME;
			params[i].buffer = &f->time;
			break;

		case DB_STR:
		case DB_CSTR:
			params[i].buffer_type = MYSQL_TYPE_VAR_STRING;
			params[i].buffer = ""; /* Updated on runtime */
			break;

		case DB_BLOB:
			params[i].buffer_type = MYSQL_TYPE_BLOB;
			params[i].buffer = ""; /* Updated on runtime */
			break;

		case DB_NONE:
			/* Eliminates gcc warning */
			break;

		}
	}

	if (mysql_stmt_bind_param(st, params)) {
		ERR("Error while binding parameters: %s\n", mysql_stmt_error(st));
		goto error;
	}

	/* We do not need the array of MYSQL_BIND anymore, mysql_stmt_bind_param
	 * creates a copy in the statement and we will update it there
	 */
	pkg_free(params);
	return 0;
   
 error:
	if (params) pkg_free(params);
	return -1;
}


static int bind_result(MYSQL_STMT* st, db_fld_t* fld)
{
	int i, n;
	struct my_fld* f;
	MYSQL_BIND* result;

	/* Calculate the number of fields in the result */
	for(n = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[n]); n++);

	result = (MYSQL_BIND*)pkg_malloc(sizeof(MYSQL_BIND) * n);
	if (result == NULL) {
		ERR("No memory left\n");
		return -1;
	}
	memset(result, '\0', sizeof(MYSQL_BIND) * n);
	
	for(i = 0; i < n; i++) {
		f = DB_GET_PAYLOAD(fld + i);
		result[i].is_null = &f->is_null;
		/* We can do it for all the types here, mysql will ignore it
		 * for fixed-size types such as MYSQL_TYPE_LONG
		 */
		result[i].length = &f->length;
		switch(fld[i].type) {
		case DB_INT:
		case DB_BITMAP:
			result[i].buffer_type = MYSQL_TYPE_LONG;
			result[i].buffer = &fld[i].v.int4;
			break;

		case DB_FLOAT:
			result[i].buffer_type = MYSQL_TYPE_FLOAT;
			result[i].buffer = &fld[i].v.flt;
			break;
			
		case DB_DOUBLE:
			result[i].buffer_type = MYSQL_TYPE_DOUBLE;
			result[i].buffer = &fld[i].v.dbl;
			break;

		case DB_DATETIME:
			result[i].buffer_type = MYSQL_TYPE_DATETIME;
			result[i].buffer = &f->time;
			break;

		case DB_STR:
			result[i].buffer_type = MYSQL_TYPE_VAR_STRING;
			f->buf.s = pkg_malloc(STR_BUF_SIZE);
			if (f->buf.s == NULL) {
				ERR("No memory left\n");
				return -1;
			}
			result[i].buffer = f->buf.s;
			fld[i].v.str.s = f->buf.s;
			result[i].buffer_length = STR_BUF_SIZE - 1;
			break;

		case DB_CSTR:
			result[i].buffer_type = MYSQL_TYPE_VAR_STRING;
			f->buf.s = pkg_malloc(STR_BUF_SIZE);
			if (f->buf.s == NULL) {
				ERR("No memory left\n");
				return -1;
			}
			result[i].buffer = f->buf.s;
			fld[i].v.cstr = f->buf.s;
			result[i].buffer_length = STR_BUF_SIZE - 1;
			break;

		case DB_BLOB:
			result[i].buffer_type = MYSQL_TYPE_BLOB;
			f->buf.s = pkg_malloc(STR_BUF_SIZE);
			if (f->buf.s == NULL) {
				ERR("No memory left\n");
				return -1;
			}
			result[i].buffer = f->buf.s;
			fld[i].v.blob.s = f->buf.s;
			result[i].buffer_length = STR_BUF_SIZE - 1;
			break;

		case DB_NONE:
			/* Eliminates gcc warning */
			break;

		}
	}

	if (mysql_stmt_bind_result(st, result)) {
		ERR("Error while binding result: %s\n", mysql_stmt_error(st));
		goto error;
	}

	/* We do not need the array of MYSQL_BIND anymore, mysql_stmt_bind_param
	 * creates a copy in the statement and we will update it there
	 */
	pkg_free(result);
	return 0;
   
 error:
	if (result) pkg_free(result);
	return -1;
}


int my_cmd(db_cmd_t* cmd)
{
	struct my_cmd* res;
	struct my_con* mcon;

	res = (struct my_cmd*)pkg_malloc(sizeof(struct my_cmd));
	if (res == NULL) {
		ERR("No memory left\n");
		goto error;
	}
	memset(res, '\0', sizeof(struct my_cmd));
	if (db_drv_init(&res->gen, my_cmd_free) < 0) goto error;

	/* FIXME */
	mcon = DB_GET_PAYLOAD(cmd->ctx->con[db_payload_idx]);
	res->st = mysql_stmt_init(mcon->con);
	if (res->st == NULL) {
		ERR("No memory left\n");
		goto error;
	}

	switch(cmd->type) {
	case DB_PUT:
		if (DB_FLD_EMPTY(cmd->params)) {
			ERR("BUG: No parameters provided for DB_PUT in context '%.*s'\n", 
				cmd->ctx->id.len, ZSW(cmd->ctx->id.s));
			goto error;
		}
		if (build_replace_query(&res->query, cmd) < 0) goto error;
		if (mysql_stmt_prepare(res->st, res->query.s, res->query.len)) {
			ERR("Error while preparing replace query: %s\n", 
				mysql_stmt_error(res->st));
			goto error;
		}
		if (bind_params(res->st, cmd->params) < 0) goto error;
		break;

	case DB_DEL:
		if (build_delete_query(&res->query, cmd) < 0) goto error;
		if (mysql_stmt_prepare(res->st, res->query.s, res->query.len)) {
			ERR("Error while preparing delete query: %s\n",
				mysql_stmt_error(res->st));
				goto error;
		}
		if (!DB_FLD_EMPTY(cmd->params)) {
			if (bind_params(res->st, cmd->params) < 0) goto error;
		}
		break;

	case DB_GET:
		if (build_select_query(&res->query, cmd) < 0) goto error;
		if (mysql_stmt_prepare(res->st, res->query.s, res->query.len)) {
			ERR("Error while preparing select query: %s\n",
				mysql_stmt_error(res->st));
			goto error;
		}
		if (!DB_FLD_EMPTY(cmd->params)) {
			if (bind_params(res->st, cmd->params) < 0) goto error;
		}
		if (bind_result(res->st, cmd->result) < 0) goto error;
		break;
	}

	DB_SET_PAYLOAD(cmd, res);
	return 0;

 error:
	if (res) {
		db_drv_free(&res->gen);
		if (res->query.s) pkg_free(res->query.s);
		if (res->st) mysql_stmt_close(res->st);
		pkg_free(res);
	}
	return -1;
}


int my_cmd_next(db_res_t* res)
{
	int ret;
	struct my_cmd* mcmd;

	mcmd = DB_GET_PAYLOAD(res->cmd);
	ret = mysql_stmt_fetch(mcmd->st);
	
	if (ret == MYSQL_NO_DATA) return 1;

	if (ret != 0) {
		ERR("Error in mysql_stmt_fetch: %s\n", mysql_stmt_error(mcmd->st));
		return -1;
	}

	if (update_result(res->cmd->result, mcmd->st) < 0) {
		mysql_stmt_free_result(mcmd->st);
		return -1;
	}

	res->cur_rec->fld = res->cmd->result;
	return 0;
}

