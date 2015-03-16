/* 
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006-2007 iptelorg GmbH
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

/** @addtogroup mysql
 *  @{
 */

/* the following macro will break the compile on solaris */
#if !defined (__SVR4) && !defined (__sun)
   #define _XOPEN_SOURCE 4     /* bsd */
#endif
#define _XOPEN_SOURCE_EXTENDED 1    /* solaris */
#define _SVID_SOURCE 1 /* timegm */

#include "my_cmd.h"

#include "my_con.h"
#include "mysql_mod.h"
#include "my_fld.h"

#include "../../mem/mem.h"
#include "../../str.h"
#include "../../lib/srdb2/db_cmd.h"
#include "../../ut.h"
#include "../../dprint.h"

#include <strings.h>
#include <stdio.h>
#include <time.h>  /*strptime, XOPEN issue must be >=4 */
#include <string.h>
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>

#define STR_BUF_SIZE 1024

#ifdef MYSQL_FAKE_NULL

#define FAKE_NULL_STRING "[~NULL~]"
static str  FAKE_NULL_STR = STR_STATIC_INIT(FAKE_NULL_STRING);

/* avoid warning: this decimal constant is unsigned only in ISO C90 :-) */
#define FAKE_NULL_INT (-2147483647 - 1)
#endif

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
	STR_OP_NE,
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
	STR_STATIC_INIT(" set "),
	STR_STATIC_INIT(" where "),
	STR_STATIC_INIT(" is "),
	STR_STATIC_INIT(" and "),
	STR_STATIC_INIT(" or "),
	STR_STATIC_INIT("?"),
	STR_STATIC_INIT("="),
	STR_STATIC_INIT("!="),
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


static int upload_cmd(db_cmd_t* cmd);


static void my_cmd_free(db_cmd_t* cmd, struct my_cmd* payload)
{
	db_drv_free(&payload->gen);
	if (payload->sql_cmd.s) pkg_free(payload->sql_cmd.s);
	if (payload->st) mysql_stmt_close(payload->st);
	pkg_free(payload);
}


/** Builds a DELETE SQL statement.The function builds DELETE statement where
 * cmd->match specify WHERE clause.  
 * @param sql_cmd SQL statement as a result of this function 
 * @param cmd input for statement creation
 * @return -1 on error, 0 on success
 */
static int build_delete_cmd(str* sql_cmd, db_cmd_t* cmd)
{
	db_fld_t* fld;
	int i;
	char* p;

	sql_cmd->len = strings[STR_DELETE].len;
	sql_cmd->len += cmd->table.len;

	if (!DB_FLD_EMPTY(cmd->match)) {
		sql_cmd->len += strings[STR_WHERE].len;

		for(i = 0, fld = cmd->match; !DB_FLD_LAST(fld[i]); i++) {
			sql_cmd->len += strlen(fld[i].name);

			switch(fld[i].op) {
			case DB_EQ:  sql_cmd->len += strings[STR_OP_EQ].len; break;
			case DB_NE:  sql_cmd->len += strings[STR_OP_NE].len; break;
			case DB_LT:  sql_cmd->len += strings[STR_OP_LT].len; break;
			case DB_GT:  sql_cmd->len += strings[STR_OP_GT].len; break;
			case DB_LEQ: sql_cmd->len += strings[STR_OP_LEQ].len; break;
			case DB_GEQ: sql_cmd->len += strings[STR_OP_GEQ].len; break;
			default:
				ERR("mysql: Unsupported db_fld operator %d\n", fld[i].op);
				return -1;
			}

			sql_cmd->len += strings[STR_ESC].len;
			
			if (!DB_FLD_LAST(fld[i + 1])) sql_cmd->len += strings[STR_AND].len;
		}
	}

	sql_cmd->s = pkg_malloc(sql_cmd->len + 1);
	if (sql_cmd->s == NULL) {
		ERR("mysql: No memory left\n");
		return -1;
	}
	p = sql_cmd->s;
	
	APPEND_STR(p, strings[STR_DELETE]);
	APPEND_STR(p, cmd->table);

	if (!DB_FLD_EMPTY(cmd->match)) {
		APPEND_STR(p, strings[STR_WHERE]);

		for(i = 0, fld = cmd->match; !DB_FLD_LAST(fld[i]); i++) {
			APPEND_CSTR(p, fld[i].name);

			switch(fld[i].op) {
			case DB_EQ:  APPEND_STR(p, strings[STR_OP_EQ]);  break;
			case DB_NE:  APPEND_STR(p, strings[STR_OP_NE]);  break;
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
 * @param sql_cmd SQL statement as a result of this function
 * @param cmd     input for statement creation
 */
static int build_select_cmd(str* sql_cmd, db_cmd_t* cmd)
{
	db_fld_t* fld;
	int i;
	char* p;

	sql_cmd->len = strings[STR_SELECT].len;

	if (DB_FLD_EMPTY(cmd->result)) {
		sql_cmd->len += 1; /* "*" */
	} else {
		for(i = 0, fld = cmd->result; !DB_FLD_LAST(fld[i]); i++) {
			sql_cmd->len += strlen(fld[i].name);
			if (!DB_FLD_LAST(fld[i + 1])) sql_cmd->len += 1; /* , */
		}
	}
	sql_cmd->len += strings[STR_FROM].len;
	sql_cmd->len += cmd->table.len;

	if (!DB_FLD_EMPTY(cmd->match)) {
		sql_cmd->len += strings[STR_WHERE].len;

		for(i = 0, fld = cmd->match; !DB_FLD_LAST(fld[i]); i++) {
			sql_cmd->len += strlen(fld[i].name);

			switch(fld[i].op) {
			case DB_EQ:  sql_cmd->len += strings[STR_OP_EQ].len; break;
			case DB_NE:  sql_cmd->len += strings[STR_OP_NE].len; break;
			case DB_LT:  sql_cmd->len += strings[STR_OP_LT].len; break;
			case DB_GT:  sql_cmd->len += strings[STR_OP_GT].len; break;
			case DB_LEQ: sql_cmd->len += strings[STR_OP_LEQ].len; break;
			case DB_GEQ: sql_cmd->len += strings[STR_OP_GEQ].len; break;
			default:
				ERR("mysql: Unsupported db_fld operator %d\n", fld[i].op);
				return -1;
			}

			sql_cmd->len += strings[STR_ESC].len;
			
			if (!DB_FLD_LAST(fld[i + 1])) sql_cmd->len += strings[STR_AND].len;
		}
	}

	sql_cmd->s = pkg_malloc(sql_cmd->len + 1);
	if (sql_cmd->s == NULL) {
		ERR("mysql: No memory left\n");
		return -1;
	}
	p = sql_cmd->s;
	
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
			case DB_NE:  APPEND_STR(p, strings[STR_OP_NE]);  break;
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
 * @param sql_cmd SQL statement as a result of this function
 * @param cmd     input for statement creation
 */
static int build_replace_cmd(str* sql_cmd, db_cmd_t* cmd)
{
	db_fld_t* fld;
	int i;
	char* p;

	sql_cmd->len = strings[STR_REPLACE].len;
	sql_cmd->len += cmd->table.len;
	sql_cmd->len += 2; /* " (" */

	for(i = 0, fld = cmd->vals; !DB_FLD_LAST(fld[i]); i++) {
		sql_cmd->len += strlen(fld[i].name);
		sql_cmd->len += strings[STR_ESC].len;
		if (!DB_FLD_LAST(fld[i + 1])) sql_cmd->len += 2; /* , twice */
	}
	sql_cmd->len += strings[STR_VALUES].len;
    sql_cmd->len += 1; /* ) */

	sql_cmd->s = pkg_malloc(sql_cmd->len + 1);
	if (sql_cmd->s == NULL) {
		ERR("mysql: No memory left\n");
		return -1;
	}
	p = sql_cmd->s;
	
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
			ERR("mysql: No memory left\n");
			return -1;
		}
		if (sb->s) {
			memcpy(newp, sb->s, sb->len);
			pkg_free(sb->s);
		}
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
 * @param sql_cmd  SQL statement as a result of this function
 * @param cmd      input for statement creation
 */
static int build_update_cmd(str* sql_cmd, db_cmd_t* cmd)
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
			case DB_NE:  rv |= sb_add(&sql_buf, &strings[STR_OP_NE]);  break;
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
	sql_cmd->s = sql_buf.s;
	sql_cmd->len = sql_buf.len;
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

#ifndef MYSQL_FAKE_NULL
	fp->is_null = fld->flags & DB_NULL;
	if (fp->is_null) return;
#else
	if (fld->flags & DB_NULL) {
		switch(fld->type) {
		case DB_STR:
		case DB_CSTR:
			param->buffer = FAKE_NULL_STR.s;
			fp->length = FAKE_NULL_STR.len;
			break;
		case DB_INT:
			*(int*)param->buffer = FAKE_NULL_INT;
			break;
		case DB_BLOB:
		case DB_DATETIME:
		case DB_NONE:
		case DB_FLOAT:
		case DB_DOUBLE:
		case DB_BITMAP:
			/* we don't have fake null value for these types */
			fp->is_null = DB_NULL;
			break;
		}
		return;
	}
#endif
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
 * Update values of MySQL bound parameters with values from
 * the DB API.
 * @param cmd Command structure which contains pointers to MYSQL_STMT and parameters values
 * @see bind_mysql_params
 */
static inline void set_mysql_params(db_cmd_t* cmd)
{
	struct my_cmd* mcmd;
	int i;

	mcmd = DB_GET_PAYLOAD(cmd);

	/* FIXME: We are updating internals of the prepared statement here,
	 * this is probably not nice but I could not find another way of
	 * updating the pointer to the buffer without the need to run
	 * mysql_stmt_bind_param again (which would be innefficient)
	 */
	for(i = 0; i < cmd->vals_count; i++) {
		update_field(mcmd->st->params + i, cmd->vals + i);
	}

	for(i = 0; i < cmd->match_count; i++) {
		update_field(mcmd->st->params + cmd->vals_count + i, cmd->match + i);
	}
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
#ifdef MYSQL_FAKE_NULL
			if (STR_EQ(FAKE_NULL_STR,result[i].v.lstr)) {
				result[i].flags |= DB_NULL;
			}
#endif
			break;

		case DB_BLOB:
			result[i].v.blob.len = rp->length;
			break;

		case DB_CSTR:
			if (rp->length < STR_BUF_SIZE) {
				result[i].v.cstr[rp->length] = '\0';
			} else {
				/* Truncated field but rp->length contains full size,
				 * zero terminated the last byte in the buffer
				 */
				result[i].v.cstr[STR_BUF_SIZE - 1] = '\0';
			}
#ifdef MYSQL_FAKE_NULL
			if (strcmp(FAKE_NULL_STR.s,result[i].v.cstr)==0) {
				result[i].flags |= DB_NULL;
			}
#endif
			break;
			
		case DB_DATETIME:
			memset(&t, '\0', sizeof(struct tm));
			t.tm_sec = rp->time.second;
			t.tm_min = rp->time.minute;
			t.tm_hour = rp->time.hour;
			t.tm_mday = rp->time.day;
			t.tm_mon = rp->time.month - 1;
			t.tm_year = rp->time.year - 1900;

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

		case DB_INT:
#ifdef MYSQL_FAKE_NULL
			if (FAKE_NULL_INT==result[i].v.int4) {
				result[i].flags |= DB_NULL;
			}
			break;
#endif
		case DB_NONE:
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
 * This is the main command execution function. The function contains
 * all the necessary logic to detect reset or disconnected database
 * connections and uploads commands to the server if necessary.
 * @param cmd Command to be executed
 * @return    0 if OK, <0 on MySQL failure, >0 on DB API failure
 */
static int exec_cmd_safe(db_cmd_t* cmd)
{
    int i, err;
    db_con_t* con;
    struct my_cmd* mcmd;
    struct my_con* mcon;
	
    /* First things first: retrieve connection info
     * from the currently active connection and also
     * mysql payload from the database command
     */
    mcmd = DB_GET_PAYLOAD(cmd);
    con = cmd->ctx->con[db_payload_idx];
    mcon = DB_GET_PAYLOAD(con);
    
    for(i = 0; i <= my_retries; i++) {
	if ((mcon->flags & MY_CONNECTED) == 0) {
	    /* The connection is disconnected, try to reconnect */
	    if (my_con_connect(con)) {
		INFO("mysql: exec_cmd_safe failed to re-connect\n");
		continue;
	    }
	}	
	
	/* Next check the number of resets in the database connection, if this
	 * number is higher than the number we keep in my_cmd structure in
	 * last_reset variable then the connection was reset and we need to
	 * upload the command again to the server before executing it, because
	 * the server recycles all server side information upon disconnect.
	 */
	if (mcon->resets > mcmd->last_reset) {
	    INFO("mysql: Connection reset detected, uploading command to server\n");
	    err = upload_cmd(cmd);
	    if (err < 0) {
		INFO("mysql: Error while uploading command\n");
		continue;
	    } else if (err > 0) {
		/* DB API error, this is a serious problem such as memory
		 * allocation failure, bail out
		 */
		return 1;
	    }
	}
	
	set_mysql_params(cmd);
	err = mysql_stmt_execute(mcmd->st);
	if (err == 0) {
	    /* The command was executed successfully, now fetch all data to
	     * the client if it was requested by the user */
	    if (mcmd->flags & MY_FETCH_ALL) {
		err = mysql_stmt_store_result(mcmd->st);
		if (err) {
		    INFO("mysql: Error while fetching data to client.\n");
		    goto error;
		}
	    }
	    return 0;
	}
	
    error:
	/* Command execution failed, log a message and try to reconnect */
	INFO("mysql: libmysql: %d, %s\n", mysql_stmt_errno(mcmd->st),
	     mysql_stmt_error(mcmd->st));
	INFO("mysql: Error while executing command on server, trying to reconnect\n");

	my_con_disconnect(con);
	if (my_con_connect(con)) {
	    INFO("mysql: Failed to reconnect server\n");
	} else {
	    INFO("mysql: Successfully reconnected server\n");
	}
    }
    
    INFO("mysql: Failed to execute command, giving up\n");
    return -1;
}


int my_cmd_exec(db_res_t* res, db_cmd_t* cmd)
{
	struct my_cmd* mcmd;

	mcmd = DB_GET_PAYLOAD(cmd);

	mcmd->next_flag = -1;
	return exec_cmd_safe(cmd);
}


/**
 * Set MYSQL_BIND item.
 * @param bind destination
 * @param fld  source
 */
static void set_field(MYSQL_BIND *bind, db_fld_t* fld)
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
 * Bind params, give real values into prepared statement.
 * Up to two sets of parameters are provided.
 * Both of them are used in UPDATE command, params1 as colspecs and values and
 * params2 as WHERE clause. In other cases one set could be enough because values
 * or match (WHERE clause) is needed.
 * @param st MySQL command statement
 * @param params1 first set of params
 * @param params2 second set of params
 * @return 0 if OK, <0 on MySQL error, >0 on DB API error
 * @see update_params
 */
static int bind_mysql_params(MYSQL_STMT* st, db_fld_t* params1, db_fld_t* params2)
{
	int my_idx, fld_idx;
	int count1, count2;
	MYSQL_BIND* my_params;
	int err = 0;

	/* Calculate the number of parameters */
	for(count1 = 0; !DB_FLD_EMPTY(params1) && !DB_FLD_LAST(params1[count1]); count1++);
	for(count2 = 0; !DB_FLD_EMPTY(params2) && !DB_FLD_LAST(params2[count2]); count2++);
	if (st->param_count != count1 + count2) {
		BUG("mysql: Number of parameters in SQL command does not match number of DB API parameters\n");
		return 1;
	}
	
	my_params = (MYSQL_BIND*)pkg_malloc(sizeof(MYSQL_BIND) * (count1 + count2));
	if (my_params == NULL) {
		ERR("mysql: No memory left\n");
		return -1;
	}
	memset(my_params, '\0', sizeof(MYSQL_BIND) * (count1 + count2));

	/* params1 */
	my_idx = 0;
	for (fld_idx = 0; fld_idx < count1; fld_idx++, my_idx++) {
		set_field(&my_params[my_idx], params1 + fld_idx);
	}
	/* params2 */
	for (fld_idx = 0; fld_idx < count2; fld_idx++, my_idx++) {
		set_field(&my_params[my_idx], params2 + fld_idx);
	}

	err = mysql_stmt_bind_param(st, my_params);
	if (err) {
		ERR("mysql: libmysqlclient: %d, %s\n", 
			mysql_stmt_errno(st), mysql_stmt_error(st));
		goto error;
	}

	/* We do not need the array of MYSQL_BIND anymore, mysql_stmt_bind_param
	 * creates a copy in the statement and we will update it there
	 */
	pkg_free(my_params);
	return err;
   
 error:
	if (my_params) pkg_free(my_params);
	return err;
}


/*
 * FIXME: This function will only work if we have one db connection
 * in every context, otherwise it would initialize the result set
 * from the first connection in the context.
 */
static int check_result(db_cmd_t* cmd, struct my_cmd* payload)
{
	int i, n;
	MYSQL_FIELD *fld;
	MYSQL_RES *meta = NULL;

	meta = mysql_stmt_result_metadata(payload->st);
	if (meta == NULL) {
		/* No error means no result set to be checked */
		if (mysql_stmt_errno(payload->st) == 0) return 0;
		ERR("mysql: Error while getting metadata of SQL command: %d, %s\n",
			mysql_stmt_errno(payload->st), mysql_stmt_error(payload->st));
		return -1;
	}
	n = mysql_num_fields(meta);
	if (cmd->result == NULL) {
		/* The result set parameter of db_cmd function was empty, that
		 * means the command is select * and we have to create the array
		 * of result fields in the cmd structure manually.
		 */
		cmd->result = db_fld(n + 1);
		cmd->result_count = n;
		for(i = 0; i < cmd->result_count; i++) {
			struct my_fld *f;
			if (my_fld(cmd->result + i, cmd->table.s) < 0) goto error;
			f = DB_GET_PAYLOAD(cmd->result + i);
			fld = mysql_fetch_field_direct(meta, i);
			f->name = pkg_malloc(strlen(fld->name)+1);
			if (f->name == NULL) {
				ERR("mysql: Out of private memory\n");
				goto error;
			}
			strcpy(f->name, fld->name);
			cmd->result[i].name = f->name;
		}
	} else {
		if (cmd->result_count != n) {
			BUG("mysql: Number of fields in MySQL result does not match number of parameters in DB API\n");
			goto error;
		}
	}

	/* Now iterate through all the columns in the result set and replace
	 * any occurrence of DB_UNKNOWN type with the type of the column
	 * retrieved from the database and if no column name was provided then
	 * update it from the database as well. 
	 */
	for(i = 0; i < cmd->result_count; i++) {
		fld = mysql_fetch_field_direct(meta, i);
		if (cmd->result[i].type != DB_NONE) continue;
		switch(fld->type) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
			cmd->result[i].type = DB_INT;
			break;

		case MYSQL_TYPE_FLOAT:
			cmd->result[i].type = DB_FLOAT;
			break;

		case MYSQL_TYPE_DOUBLE:
			cmd->result[i].type = DB_DOUBLE;
			break;

		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_DATETIME:
			cmd->result[i].type = DB_DATETIME;
			break;

		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
			cmd->result[i].type = DB_STR;
			break;

		default:
			ERR("mysql: Unsupported MySQL column type: %d, table: %s, column: %s\n",
				fld->type, cmd->table.s, fld->name);
			goto error;
		}
	}
	
	if (meta) mysql_free_result(meta);
	return 0;

error:
	if (meta) mysql_free_result(meta);
	return 1;
}


/* FIXME: Add support for DB_NONE, in this case the function should determine
 * the type of the column in the database and set the field type appropriately.
 * This function must be called after check_result.
 */
static int bind_result(MYSQL_STMT* st, db_fld_t* fld)
{
	int i, n, err = 0;
	struct my_fld* f;
	MYSQL_BIND* result;

	/* Calculate the number of fields in the result */
	for(n = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[n]); n++);
	/* Return immediately if there are no fields in the result set */
	if (n == 0) return 0;

	result = (MYSQL_BIND*)pkg_malloc(sizeof(MYSQL_BIND) * n);
	if (result == NULL) {
		ERR("mysql: No memory left\n");
		return 1;
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
				ERR("mysql: No memory left\n");
				err = 1;
				goto error;
			}
			result[i].buffer = f->buf.s;
			fld[i].v.lstr.s = f->buf.s;
			result[i].buffer_length = STR_BUF_SIZE - 1;
			break;

		case DB_CSTR:
			result[i].buffer_type = MYSQL_TYPE_VAR_STRING;
			if (!f->buf.s) f->buf.s = pkg_malloc(STR_BUF_SIZE);
			if (f->buf.s == NULL) {
				ERR("mysql: No memory left\n");
				err = 1;
				goto error;
			}
			result[i].buffer = f->buf.s;
			fld[i].v.cstr = f->buf.s;
			result[i].buffer_length = STR_BUF_SIZE - 1;
			break;

		case DB_BLOB:
			result[i].buffer_type = MYSQL_TYPE_BLOB;
			if (!f->buf.s) f->buf.s = pkg_malloc(STR_BUF_SIZE);
			if (f->buf.s == NULL) {
				ERR("mysql: No memory left\n");
				err = 1;
				goto error;
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

	err = mysql_stmt_bind_result(st, result);
	if (err) {
		ERR("mysql: Error while binding result: %s\n", mysql_stmt_error(st));
		goto error;
	}

	/* We do not need the array of MYSQL_BIND anymore, mysql_stmt_bind_param
	 * creates a copy in the statement and we will update it there
	 */
	if (result) pkg_free(result);
	return 0;
   
 error:
	if (result) pkg_free(result);
	return err;
}


/**
 * Upload database command to the server
 * @param cmd  Command to be uploaded
 * @return     0 if OK, >0 on DB API errors, <0 on MySQL errors
 */
static int upload_cmd(db_cmd_t* cmd)
{
    struct my_cmd* res;
    struct my_con* mcon;
    int err = 0;
    
    res = DB_GET_PAYLOAD(cmd);
    
    /* FIXME: The function should take the connection as one of parameters */
    mcon = DB_GET_PAYLOAD(cmd->ctx->con[db_payload_idx]);
    /* Do not upload the command if the connection is not connected */
    if ((mcon->flags & MY_CONNECTED) == 0) {
	err = 1;
	goto error;
    }

    /* If there is a previous pre-compiled statement, close it first */
    if (res->st) mysql_stmt_close(res->st);
    res->st = NULL;
    
    /* Create a new pre-compiled statement data structure */
    res->st = mysql_stmt_init(mcon->con);
    if (res->st == NULL) {
	ERR("mysql: Error while creating new MySQL_STMT data structure (no memory left)\n");
	err = 1;
	goto error;
    }
    
    /* Try to upload the command to the server */
    if (mysql_stmt_prepare(res->st, res->sql_cmd.s, res->sql_cmd.len)) {
	err = mysql_stmt_errno(res->st);    
	ERR("mysql: libmysql: %d, %s\n", err, mysql_stmt_error(res->st));
	ERR("mysql: An error occurred while uploading command to server\n");
    }
    if (err == CR_SERVER_LOST ||
	err == CR_SERVER_GONE_ERROR) {
	/* Connection to the server was lost, mark the connection as
	 * disconnected. In this case mysql_stmt_prepare invalidates the
	 * connection internally and calling another mysql function on that
	 * connection would crash. To make sure that no other mysql function
	 * gets called unless the connection is reconnected we disconnect it
	 * explicitly here. This is a workaround for mysql bug #33384. */
	my_con_disconnect(cmd->ctx->con[db_payload_idx]);
    }
    if (err) {
	/* Report mysql error to the caller */
	err = -1;
	goto error;
    }
    
    err = bind_mysql_params(res->st, cmd->vals, cmd->match);
    if (err) goto error;
    
    if (cmd->type == DB_GET || cmd->type == DB_SQL) {
	err = check_result(cmd, res);
	if (err) goto error;
	err = bind_result(res->st, cmd->result);
	if (err) goto error;
    }
    
    res->last_reset = mcon->resets;
    return 0;
    
error:
    if (res->st) {
	mysql_stmt_close(res->st);
	res->st = NULL;
    }
    return err;
}


int my_cmd(db_cmd_t* cmd)
{
	struct my_cmd* res;
 
	res = (struct my_cmd*)pkg_malloc(sizeof(struct my_cmd));
	if (res == NULL) {
		ERR("mysql: No memory left\n");
		goto error;
	}
	memset(res, '\0', sizeof(struct my_cmd));
	/* Fetch all data to client at once by default */
	res->flags |= MY_FETCH_ALL;
	if (db_drv_init(&res->gen, my_cmd_free) < 0) goto error;

	switch(cmd->type) {
	case DB_PUT:
		if (DB_FLD_EMPTY(cmd->vals)) {
			BUG("mysql: No parameters provided for DB_PUT in context '%.*s'\n", 
				cmd->ctx->id.len, ZSW(cmd->ctx->id.s));
			goto error;
		}
		if (build_replace_cmd(&res->sql_cmd, cmd) < 0) goto error;
		break;

	case DB_DEL:
		if (build_delete_cmd(&res->sql_cmd, cmd) < 0) goto error;
		break;

	case DB_GET:
		if (build_select_cmd(&res->sql_cmd, cmd) < 0) goto error;
		break;

	case DB_UPD:
		if (build_update_cmd(&res->sql_cmd, cmd) < 0) goto error;
		break;

	case DB_SQL:
		res->sql_cmd.s = (char*)pkg_malloc(cmd->table.len);
		if (res->sql_cmd.s == NULL) {
			ERR("mysql: Out of private memory\n");
			goto error;
		}
		memcpy(res->sql_cmd.s,cmd->table.s, cmd->table.len);
		res->sql_cmd.len = cmd->table.len;
        break;
	}

	DB_SET_PAYLOAD(cmd, res);

	/* In order to check all the parameters and results, we need to upload
	 * the command to the server. We need to do that here before we report
	 * back that the command was created successfully. Hence, this
	 * function requires the corresponding connection be established. We
	 * would not be able to check parameters if we don't do that there and
	 * that could result in repeated execution failures at runtime.
	 */
	if (upload_cmd(cmd)) goto error;
	return 0;

 error:
	if (res) {
		DB_SET_PAYLOAD(cmd, NULL);
		db_drv_free(&res->gen);
		if (res->sql_cmd.s) pkg_free(res->sql_cmd.s);
		pkg_free(res);
	}
	return -1;
}


int my_cmd_first(db_res_t* res) {
	struct my_cmd* mcmd;

	mcmd = DB_GET_PAYLOAD(res->cmd);
	switch (mcmd->next_flag) {
	case -2: /* table is empty */
		return 1;
	case 0:  /* cursor position is 0 */
		return 0;
	case 1:  /* next row */
	case 2:  /* EOF */
		ERR("mysql: Unbuffered queries do not support cursor reset.\n");
		return -1;
	default:
		return my_cmd_next(res);
	}
}


int my_cmd_next(db_res_t* res)
{
	int ret;
	struct my_cmd* mcmd;

	mcmd = DB_GET_PAYLOAD(res->cmd);
	if (mcmd->next_flag == 2 || mcmd->next_flag == -2) return 1;

	if (mcmd->st == NULL) {
		ERR("mysql: Prepared statement not found\n");
		return -1;
	}

	ret = mysql_stmt_fetch(mcmd->st);
	
	if (ret == MYSQL_NO_DATA) {
		mcmd->next_flag =  mcmd->next_flag<0?-2:2;
		return 1;
	}
	/* MYSQL_DATA_TRUNCATED is only defined in mysql >= 5.0 */
#if defined MYSQL_DATA_TRUNCATED
	if (ret == MYSQL_DATA_TRUNCATED) {
		int i;
		ERR("mysql: mysql_stmt_fetch, data truncated, fields: %d\n", res->cmd->result_count);
		for (i = 0; i < res->cmd->result_count; i++) {
			if (mcmd->st->bind[i].error /*&& mcmd->st->bind[i].buffer_length*/) {
				ERR("mysql: truncation, bind %d, length: %lu, buffer_length: %lu\n", 
					i, *(mcmd->st->bind[i].length), mcmd->st->bind[i].buffer_length);
			}
		}
		ret = 0;
	}
#endif
	if (mcmd->next_flag <= 0) {
		mcmd->next_flag++;
	}
	if (ret != 0) {
		ERR("mysql: Error in mysql_stmt_fetch (ret=%d): %s\n", ret, mysql_stmt_error(mcmd->st));
		return -1;
	}

	if (update_result(res->cmd->result, mcmd->st) < 0) {
		mysql_stmt_free_result(mcmd->st);
		return -1;
	}

	res->cur_rec->fld = res->cmd->result;
	return 0;
}


int my_getopt(db_cmd_t* cmd, char* optname, va_list ap)
{
	struct my_cmd* mcmd;
	long long* id;
	int* val;

	mcmd = (struct my_cmd*)DB_GET_PAYLOAD(cmd);

	if (!strcasecmp("last_id", optname)) {
		id = va_arg(ap, long long*);
		if (id == NULL) {
			BUG("mysql: NULL pointer passed to 'last_id' option\n");
			goto error;
		}

		if (mcmd->st->last_errno != 0) {
			BUG("mysql: Option 'last_id' called but previous command failed, "
				"check your code\n");
			return -1;
		}

		*id = mysql_stmt_insert_id(mcmd->st);
		if ((*id) == 0) {
			BUG("mysql: Option 'last_id' called but there is no auto-increment"
				" column in table, SQL command: %.*s\n", STR_FMT(&mcmd->sql_cmd));
			return -1;
		}
	} else if (!strcasecmp("fetch_all", optname)) {
		val = va_arg(ap, int*);
		if (val == NULL) {
			BUG("mysql: NULL pointer passed to 'fetch_all' DB option\n");
			goto error;
		}
		*val = mcmd->flags;
	} else {
		return 1;
	}
	return 0;

 error:
	return -1;
}


int my_setopt(db_cmd_t* cmd, char* optname, va_list ap)
{
	struct my_cmd* mcmd;
	int* val;

	mcmd = (struct my_cmd*)DB_GET_PAYLOAD(cmd);
	if (!strcasecmp("fetch_all", optname)) {
		val = va_arg(ap, int*);
		if (val != 0) {
			mcmd->flags |= MY_FETCH_ALL;
		} else {
			mcmd->flags &= ~MY_FETCH_ALL;
		}
	} else {
		return 1;
	}
	return 0;
}

/** @} */
