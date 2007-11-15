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

/** @addtogroup mysql
 *  @{
 */

#define _XOPEN_SOURCE 4     /* bsd */
#define _XOPEN_SOURCE_EXTENDED 1    /* solaris */
#define _SVID_SOURCE 1 /* timegm */

#include <strings.h>
#include <stdio.h>
#include <time.h>  /*strptime, XOPEN issue must be >=4 */
#include <string.h>
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>
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
	STR_SET,
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
	STR_STATIC_INIT("UPDATE "),
	STR_STATIC_INIT("select "),
	STR_STATIC_INIT("replace "),
	STR_STATIC_INIT(" SET "),
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


static int upload_query(db_cmd_t* cmd);


static void my_cmd_free(db_cmd_t* cmd, struct my_cmd* payload)
{
	db_drv_free(&payload->gen);
	if (payload->query.s) pkg_free(payload->query.s);
	if (payload->st) mysql_stmt_close(payload->st);
	pkg_free(payload);
}


/**
 *  Builds DELETE statement where cmd->match specify WHERE clause.
 * @param query  SQL statement as a result of this function
 * @param cmd    input for statement creation
 */
static int build_delete_query(str* query, db_cmd_t* cmd)
{
	db_fld_t* fld;
	int i;
	char* p;

	query->len = strings[STR_DELETE].len;
	query->len += cmd->table.len;

	if (!DB_FLD_EMPTY(cmd->match)) {
		query->len += strings[STR_WHERE].len;

		for(i = 0, fld = cmd->match; !DB_FLD_LAST(fld[i]); i++) {
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

	if (!DB_FLD_EMPTY(cmd->match)) {
		APPEND_STR(p, strings[STR_WHERE]);

		for(i = 0, fld = cmd->match; !DB_FLD_LAST(fld[i]); i++) {
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


/**
 *  Builds SELECT statement where cmd->values specify column names
 *  and cmd->match specify WHERE clause.
 * @param query  SQL statement as a result of this function
 * @param cmd    input for statement creation
 */
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

	if (!DB_FLD_EMPTY(cmd->match)) {
		query->len += strings[STR_WHERE].len;

		for(i = 0, fld = cmd->match; !DB_FLD_LAST(fld[i]); i++) {
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

	if (!DB_FLD_EMPTY(cmd->match)) {
		APPEND_STR(p, strings[STR_WHERE]);

		for(i = 0, fld = cmd->match; !DB_FLD_LAST(fld[i]); i++) {
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


/**
 *  Builds REPLACE statement where cmd->values specify column names.
 * @param query  SQL statement as a result of this function
 * @param cmd    input for statement creation
 */
static int build_replace_query(str* query, db_cmd_t* cmd)
{
	db_fld_t* fld;
	int i;
	char* p;

	query->len = strings[STR_REPLACE].len;
	query->len += cmd->table.len;
	query->len += 2; /* " (" */

	for(i = 0, fld = cmd->vals; !DB_FLD_LAST(fld[i]); i++) {
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

	for(i = 0, fld = cmd->vals; !DB_FLD_LAST(fld[i]); i++) {
		APPEND_CSTR(p, fld[i].name);
		if (!DB_FLD_LAST(fld[i + 1])) *p++ = ',';
	}
	APPEND_STR(p, strings[STR_VALUES]);

	for(i = 0, fld = cmd->vals; !DB_FLD_LAST(fld[i]); i++) {
		APPEND_STR(p, strings[STR_ESC]);
		if (!DB_FLD_LAST(fld[i + 1])) *p++ = ',';
	}
	*p++ = ')';
	*p = '\0';
	return 0;
}

/**
 *  Reallocatable string buffer.
 */
struct string_buffer {
	char *s;			/**< allocated memory itself */
	int   len;			/**< used memory */
	int   size;			/**< total size of allocated memory */
	int   increment;	/**< increment when realloc is necessary */ 
};
/**
 *  Add new string into string buffer.
 * @param sb    string buffer
 * @param nstr  string to add
 * @return      0 if OK, -1 if failed
 */
static inline int sb_add(struct string_buffer *sb, str *nstr)
{
	int new_size = 0;
	int rsize = sb->len + nstr->len;
	int asize;
	char *newp;
	
	if ( rsize > sb->size ) {
		asize = rsize - sb->size;
		new_size = sb->size + (asize / sb->increment  + (asize % sb->increment > 0)) * sb->increment;
		newp = pkg_malloc(new_size);
		if (!newp) {
			ERR("not enough memory\n");
			return -1;
		}
		memcpy(newp, sb->s, sb->len);
		pkg_free(sb->s);
		sb->s = newp;
		sb->size = new_size;
	}
	memcpy(sb->s + sb->len, nstr->s, nstr->len);
	sb->len += nstr->len;
	return 0;
}
/**
 *  Set members of str variable.
 *  Used for temporary str variables. 
 */
static inline str* set_str(str *str, const char *s)
{
	str->s = (char *)s;
	str->len = strlen(s);
	return str;
}


/**
 *  Builds UPDATE statement where cmd->valss specify column name-value pairs
 *  and cmd->match specify WHERE clause.
 * @param query  SQL statement as a result of this function
 * @param cmd    input for statement creation
 */
static int build_update_query(str* query, db_cmd_t* cmd)
{
	struct string_buffer sql_buf = {.s = NULL, .len = 0, .size = 0, .increment = 128};
	db_fld_t* fld;
	int i;
	int rv = 0;
	str tmpstr;

	rv = sb_add(&sql_buf, &strings[STR_UPDATE]);	/* "UPDATE " */
	rv |= sb_add(&sql_buf, &cmd->table);			/* table name */
	rv |= sb_add(&sql_buf, &strings[STR_SET]);		/* " SET " */

	/* column name-value pairs */
	for(i = 0, fld = cmd->vals; !DB_FLD_LAST(fld[i]); i++) {
		rv |= sb_add(&sql_buf, set_str(&tmpstr, fld[i].name));
		rv |= sb_add(&sql_buf, set_str(&tmpstr, " = "));
		rv |= sb_add(&sql_buf, &strings[STR_ESC]);
		if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, set_str(&tmpstr, ", "));
	}
	if (rv) {
		goto err;
	}

	if (!DB_FLD_EMPTY(cmd->match)) {
		rv |= sb_add(&sql_buf, &strings[STR_WHERE]);

		for(i = 0, fld = cmd->match; !DB_FLD_LAST(fld[i]); i++) {
			rv |= sb_add(&sql_buf, set_str(&tmpstr, fld[i].name));

			switch(fld[i].op) {
			case DB_EQ:  rv |= sb_add(&sql_buf, &strings[STR_OP_EQ]);  break;
			case DB_LT:  rv |= sb_add(&sql_buf, &strings[STR_OP_LT]);  break;
			case DB_GT:  rv |= sb_add(&sql_buf, &strings[STR_OP_GT]);  break;
			case DB_LEQ: rv |= sb_add(&sql_buf, &strings[STR_OP_LEQ]); break;
			case DB_GEQ: rv |= sb_add(&sql_buf, &strings[STR_OP_GEQ]); break;
			}
			
			rv |= sb_add(&sql_buf, &strings[STR_ESC]);
			if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, &strings[STR_AND]);
		}
	}
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\0"));
	if (rv) {
		goto err;
	}
	query->s = sql_buf.s;

	return 0;

err:
	if (sql_buf.s) pkg_free(sql_buf.s);
	return -1;
}


static inline void update_field(MYSQL_BIND *param, db_fld_t* fld)
{
	struct my_fld* fp;      /* field payload */
	struct tm* t;
	
	fp = DB_GET_PAYLOAD(fld);

	fp->is_null = fld->flags & DB_NULL;
	if (fp->is_null) return;

	switch(fld->type) {
	case DB_STR:
		param->buffer = fld->v.lstr.s;
		fp->length = fld->v.lstr.len;
		break;

	case DB_BLOB:
		param->buffer = fld->v.blob.s;
		fp->length = fld->v.blob.len;
		break;

	case DB_CSTR:
		param->buffer = (char*)fld->v.cstr;
		fp->length = strlen(fld->v.cstr);
		break;

	case DB_DATETIME:
		t = gmtime(&fld->v.time);
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

/**
 *  Update params with given values.
 *  Up to two sets of parameters are provided.
 *  Both of them are used in UPDATE query, params1 as colspecs and values and
 *  params2 as WHERE clause.
 * @param st MySQL query statement
 * @param params1 first set of params
 * @param params2 second set of params
 * @see bind_params
 */
static inline int update_params(MYSQL_STMT* st, db_fld_t* params1, db_fld_t* params2)
{
	int  my_idx, fld_idx;
	int  count1, count2;

	/* Calculate the number of parameters */
	for(count1 = 0; !DB_FLD_EMPTY(params1) && !DB_FLD_LAST(params1[count1]); count1++);
	for(count2 = 0; !DB_FLD_EMPTY(params2) && !DB_FLD_LAST(params2[count2]); count2++);
	
	if (st->param_count != count1 + count2) {
		ERR("MySQL param count do not match with given parameter arrays\n");
		return -1;
	}

	/* Iterate through all the query parameters and update
	 * their values if needed
	 */

	/* FIXME: We are updating internals of the prepared statement here,
	 * this is probably not nice but I could not find another way of
	 * updating the pointer to the buffer without the need to run
	 * mysql_stmt_bind_param again (which would be innefficient)
	 */
	/* params1 */
	my_idx = 0;
	for (fld_idx = 0; fld_idx < count1; fld_idx++, my_idx++) {
		update_field(&st->params[my_idx], params1 + fld_idx);
	}
	/* params2 */
	for (fld_idx = 0; fld_idx < count2; fld_idx++, my_idx++) {
		update_field(&st->params[my_idx], params2 + fld_idx);
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
			result[i].v.lstr.len = rp->length;
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

/**
 *  DB_DEL uses cmd-&gt;match
 *  DB_PUT uses cmd-&gt;vals
 */
int my_cmd_write(db_res_t* res, db_cmd_t* cmd)
{
	struct my_cmd* mcmd;
	int ret, myerr;
	
	mcmd = DB_GET_PAYLOAD(cmd);
	if (cmd->type == DB_DEL && mcmd->st->param_count && update_params(mcmd->st, cmd->match, NULL) < 0) return -1;
	if (cmd->type == DB_PUT && mcmd->st->param_count && update_params(mcmd->st, cmd->vals, NULL) < 0) return -1;
	ret = mysql_stmt_execute(mcmd->st);

	/* If the connection to the server was lost then try to resubmit the query,
	 * it will fail if the connection is not yet working, and try to execute
	 * it again if the upload was successful
	 */
	if (ret) {
		myerr = mysql_stmt_errno(mcmd->st);
		if (myerr == ER_UNKNOWN_STMT_HANDLER || myerr == CR_SERVER_LOST) {
			if (upload_query(cmd) < 0) {
				LOG(L_CRIT, "Failed to re-initialize DB connection, DB server is not working\n");
				return -1;
			}

			if (cmd->type == DB_DEL && mcmd->st->param_count && update_params(mcmd->st, cmd->match, NULL) < 0) return -1;
			if (cmd->type == DB_PUT && mcmd->st->param_count && update_params(mcmd->st, cmd->vals, NULL) < 0) return -1;
			ret = mysql_stmt_execute(mcmd->st);
		}
	}

	if (ret) {
		ERR("Error while executing query: %d, %s\n", mysql_stmt_errno(mcmd->st), mysql_stmt_error(mcmd->st));
		return -1;
	}

	return 0;
}


int my_cmd_read(db_res_t* res, db_cmd_t* cmd)
{
	int ret, myerr;
	struct my_cmd* mcmd;
   
	mcmd = DB_GET_PAYLOAD(cmd);

	if (mcmd->st->param_count && update_params(mcmd->st, cmd->match, NULL) < 0) return -1;
	ret = mysql_stmt_execute(mcmd->st);


	/* If the connection to the server was lost then try to resubmit the query,
	 * it will fail if the connection is not yet working, and try to execute
	 * it again if the upload was successful
	 */
	if (ret) {
		myerr = mysql_stmt_errno(mcmd->st);
		if (myerr == ER_UNKNOWN_STMT_HANDLER || myerr == CR_SERVER_LOST) {
			if (upload_query(cmd) < 0) {
				LOG(L_CRIT, "Failed to re-initialize DB connection, DB server is not working\n");
				return -1;
			}

			if (mcmd->st->param_count && update_params(mcmd->st, cmd->match, NULL) < 0) return -1;
			ret = mysql_stmt_execute(mcmd->st);
		}
	}

	if (ret) {
		ERR("Error while executing query: %d, %s\n", mysql_stmt_errno(mcmd->st), mysql_stmt_error(mcmd->st));
		return -1;
	}

	return 0;
}


int my_cmd_update(db_res_t* res, db_cmd_t* cmd)
{
	struct my_cmd* mcmd;
	int ret, myerr;

	mcmd = DB_GET_PAYLOAD(cmd);
	if (mcmd->st->param_count && update_params(mcmd->st, cmd->vals, cmd->match) < 0) return -1;
	ret = mysql_stmt_execute(mcmd->st);

	/* If the connection to the server was lost then try to resubmit the query,
	 * it will fail if the connection is not yet working, and try to execute
	 * it again if the upload was successful
	 */
	if (ret) {
		myerr = mysql_stmt_errno(mcmd->st);
		if (myerr == ER_UNKNOWN_STMT_HANDLER || myerr == CR_SERVER_LOST) {
			if (upload_query(cmd) < 0) {
				LOG(L_CRIT, "Failed to re-initialize DB connection, DB server is not working\n");
				return -1;
			}

			if (mcmd->st->param_count && update_params(mcmd->st, cmd->vals, cmd->match) < 0) return -1;
			ret = mysql_stmt_execute(mcmd->st);
		}
	}

	if (ret) {
		ERR("Error while executing query: %d, %s\n", mysql_stmt_errno(mcmd->st), mysql_stmt_error(mcmd->st));
		return -1;
	}

	return 0;
}


int my_cmd_sql(db_res_t* res, db_cmd_t* cmd)
{
	struct my_cmd* mcmd;
	int ret, myerr;
   
	mcmd = DB_GET_PAYLOAD(cmd);
	ret = mysql_stmt_execute(mcmd->st);

	/* If the connection to the server was lost then try to resubmit the query,
	 * it will fail if the connection is not yet working, and try to execute
	 * it again if the upload was successful
	 */
	if (ret) {
		myerr = mysql_stmt_errno(mcmd->st);
		if (myerr == ER_UNKNOWN_STMT_HANDLER || myerr == CR_SERVER_LOST) {
			if (upload_query(cmd) < 0) {
				LOG(L_CRIT, "Failed to re-initialize DB connection, DB server is not working\n");
				return -1;
			}

			ret = mysql_stmt_execute(mcmd->st);
		}
	}

	if (ret) {
		ERR("Error while executing query: %d, %s\n", mysql_stmt_errno(mcmd->st), mysql_stmt_error(mcmd->st));
		return -1;
	}

	return 0;
}

/**
 * Set MYSQL_BIND item.
 * @param bind destination
 * @param fld  source
 */
static void set_field(MYSQL_BIND *bind, db_fld_t* fld )
{
	struct my_fld* f;
	
	f = DB_GET_PAYLOAD(fld);
	bind->is_null = &f->is_null;
	/* We can do it for all the types here, mysql will ignore it
	 * for fixed-size types such as MYSQL_TYPE_LONG
	 */
	bind->length = &f->length;
	switch(fld->type) {
	case DB_INT:
	case DB_BITMAP:
		bind->buffer_type = MYSQL_TYPE_LONG;
		bind->buffer = &fld->v.int4;
		break;
	
	case DB_FLOAT:
		bind->buffer_type = MYSQL_TYPE_FLOAT;
		bind->buffer = &fld->v.flt;
		break;
		
	case DB_DOUBLE:
		bind->buffer_type = MYSQL_TYPE_DOUBLE;
		bind->buffer = &fld->v.dbl;
		break;
	
	case DB_DATETIME:
		bind->buffer_type = MYSQL_TYPE_DATETIME;
		bind->buffer = &f->time;
		break;
	
	case DB_STR:
	case DB_CSTR:
		bind->buffer_type = MYSQL_TYPE_VAR_STRING;
		bind->buffer = ""; /* Updated on runtime */
		break;
	
	case DB_BLOB:
		bind->buffer_type = MYSQL_TYPE_BLOB;
		bind->buffer = ""; /* Updated on runtime */
		break;
	
	case DB_NONE:
		/* Eliminates gcc warning */
		break;
	
	}
}

/**
 *  Bind params, give real values into prepared statement.
 *  Up to two sets of parameters are provided.
 *  Both of them are used in UPDATE query, params1 as colspecs and values and
 *  params2 as WHERE clause. In other cases one set could be enough because values
 *  or match (WHERE clause) is needed.
 * @param st MySQL query statement
 * @param params1 first set of params
 * @param params2 second set of params
 * @see update_params
 */
static int bind_params(MYSQL_STMT* st, db_fld_t* params1, db_fld_t* params2)
{
	int my_idx, fld_idx;
	int count1, count2;
	MYSQL_BIND* my_params;

	/* Calculate the number of parameters */
	for(count1 = 0; !DB_FLD_EMPTY(params1) && !DB_FLD_LAST(params1[count1]); count1++);
	for(count2 = 0; !DB_FLD_EMPTY(params2) && !DB_FLD_LAST(params2[count2]); count2++);

	my_params = (MYSQL_BIND*)pkg_malloc(sizeof(MYSQL_BIND) * (count1+count2));
	if (my_params == NULL) {
		ERR("No memory left\n");
		return -1;
	}
	memset(my_params, '\0', sizeof(MYSQL_BIND) * (count1+count2));

	/* params1 */
	my_idx = 0;
	for (fld_idx = 0; fld_idx < count1; fld_idx++, my_idx++) {
		set_field(&my_params[my_idx], params1 + fld_idx);
	}
	/* params2 */
	for (fld_idx = 0; fld_idx < count2; fld_idx++, my_idx++) {
		set_field(&my_params[my_idx], params2 + fld_idx);
	}
	if (mysql_stmt_bind_param(st, my_params)) {
		ERR("Error while binding parameters: %s\n", mysql_stmt_error(st));
		goto error;
	}

	/* We do not need the array of MYSQL_BIND anymore, mysql_stmt_bind_param
	 * creates a copy in the statement and we will update it there
	 */
	pkg_free(my_params);
	return 0;
   
 error:
	if (my_params) pkg_free(my_params);
	return -1;
}

/* FIXME: Add support for DB_NONE, in this case the function should determine
 * the type of the column in the database and set the field type appropriately
 */
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
			if (!f->buf.s) f->buf.s = pkg_malloc(STR_BUF_SIZE);
			if (f->buf.s == NULL) {
				ERR("No memory left\n");
				return -1;
			}
			result[i].buffer = f->buf.s;
			fld[i].v.lstr.s = f->buf.s;
			result[i].buffer_length = STR_BUF_SIZE - 1;
			break;

		case DB_CSTR:
			result[i].buffer_type = MYSQL_TYPE_VAR_STRING;
			if (!f->buf.s) f->buf.s = pkg_malloc(STR_BUF_SIZE);
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
			if (!f->buf.s) f->buf.s = pkg_malloc(STR_BUF_SIZE);
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
 	ERR("bind_result failed\n");
	if (result) pkg_free(result);
	return -1;
}


static int upload_query(db_cmd_t* cmd)
{
	struct my_cmd* res;
	struct my_con* mcon;
	MYSQL_STMT* st;

	res = DB_GET_PAYLOAD(cmd);

	/* FIXME: The function should take the connection as one of parameters */
	mcon = DB_GET_PAYLOAD(cmd->ctx->con[db_payload_idx]);

	st = mysql_stmt_init(mcon->con);
	if (st == NULL) goto error;

	/* Close the previously created statement if exists */
	if (res->st) mysql_stmt_close(res->st);
    res->st = st;

	if (mysql_stmt_prepare(res->st, res->query.s, res->query.len)) {
		goto error;
	}

	switch(cmd->type) {
	case DB_PUT:
		if (bind_params(res->st, cmd->vals, NULL) < 0) goto error;
		break;

	case DB_DEL:
		if (!DB_FLD_EMPTY(cmd->match)) {
			if (bind_params(res->st, NULL, cmd->match) < 0) goto error;
		}
		break;

	case DB_GET:
		if (!DB_FLD_EMPTY(cmd->match)) {
			if (bind_params(res->st, NULL, cmd->match) < 0) goto error;
		}
		if (bind_result(res->st, cmd->result) < 0) goto error;
		break;

	case DB_UPD:
		if (mysql_stmt_prepare(res->st, res->query.s, res->query.len)) {
			goto error;
		}

        /* FIXME: remove ELSE */
        if (!DB_FLD_EMPTY(cmd->vals)) {
            if (bind_params(res->st, cmd->vals, cmd->match) < 0) goto error;
        } else {
            if (bind_params(res->st, NULL, cmd->match) < 0) goto error;
        }
		break;

	case DB_SQL:
		if (!DB_FLD_EMPTY(cmd->result)) {
			if (bind_result(res->st, cmd->result) < 0) goto error;
		}
		break;
	}
	return 0;

 error:
	ERR("Error while uploading query to server: %s\n", 
		mysql_stmt_error(res->st));

	return -1;
}


int my_cmd(db_cmd_t* cmd)
{
	struct my_cmd* res;
 
	res = (struct my_cmd*)pkg_malloc(sizeof(struct my_cmd));
	if (res == NULL) {
		ERR("No memory left\n");
		goto error;
	}
	memset(res, '\0', sizeof(struct my_cmd));
	if (db_drv_init(&res->gen, my_cmd_free) < 0) goto error;

	switch(cmd->type) {
	case DB_PUT:
		if (DB_FLD_EMPTY(cmd->vals)) {
			ERR("BUG: No parameters provided for DB_PUT in context '%.*s'\n", 
				cmd->ctx->id.len, ZSW(cmd->ctx->id.s));
			goto error;
		}
		if (build_replace_query(&res->query, cmd) < 0) goto error;
		break;

	case DB_DEL:
		if (build_delete_query(&res->query, cmd) < 0) goto error;
		break;

	case DB_GET:
		if (build_select_query(&res->query, cmd) < 0) goto error;
		break;

	case DB_UPD:
		if (build_update_query(&res->query, cmd) < 0) goto error;
		break;

	case DB_SQL:
		break;
	}

	DB_SET_PAYLOAD(cmd, res);
	if (upload_query(cmd) < 0) goto error;
	return 0;

 error:
	if (res) {
		DB_SET_PAYLOAD(cmd, NULL);
		db_drv_free(&res->gen);
		if (res->query.s) pkg_free(res->query.s);
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

/** @} */
