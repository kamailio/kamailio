/* 
 * $Id$ 
 *
 * PostgreSQL Database Driver for SER
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * For a license to use the ser software under conditions other than those
 * described here, or to purchase support for this software, please contact
 * iptel.org by e-mail at the following addresses: info@iptel.org
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/** \addtogroup postgres
 * @{ 
 */

/** \file
 * Implementation of various functions that assemble SQL query strings for
 * PostgreSQL.
 */

#include "pg_sql.h"

#include "../../lib/srdb2/db_cmd.h"
#include "../../lib/srdb2/db_fld.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <string.h>


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
	STR_FROM,
	STR_OID,
	STR_TIMESTAMP,
	STR_ZT
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
	STR_STATIC_INIT(" from "),
	STR_STATIC_INIT("select typname,pg_type.oid from pg_type"),
	STR_STATIC_INIT("select timestamp '2000-01-01 00:00:00' + time '00:00:01'"), 
	STR_STATIC_INIT("\0")
};


/**
 * Reallocatable string buffer.
 */
struct string_buffer {
	char *s;			/**< allocated memory itself */
	int   len;			/**< used memory */
	int   size;			/**< total size of allocated memory */
	int   increment;	/**< increment when realloc is necessary */ 
};


/** Appends string to string buffer.
 * This function appends string to dynamically created string buffer,
 * the buffer is automatically extended if there is not enough room
 * in the buffer. The buffer is allocated using pkg_malloc.
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
	
	if (rsize > sb->size) {
		asize = rsize - sb->size;
		new_size = sb->size + (asize / sb->increment  + 
							   (asize % sb->increment > 0)) * sb->increment;
		newp = pkg_malloc(new_size);
		if (!newp) {
			ERR("postgres: No memory left\n");
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


/** Creates str string from zero terminated string without copying.
 * This function initializes members of a temporary str structure
 * with the pointer and lenght of the string from s parameter.
 *
 * @param str A pointer to temporary str structure.
 * @param s   A zero terminated string.
 * @return Pointer to the str structure.
 */
static inline str* set_str(str *str, const char *s)
{
	str->s = (char *)s;
	str->len = strlen(s);
	return str;
}


/** Returns a parameter marker for PostgreSQL with number i
 * The function builds a parameter marker for use in
 * PostgreSQL SQL queries, such as $1, $2, etc.
 * @param i Number of the parameter
 * @retval A pointer to static string with the marker
 */
static str* get_marker(unsigned int i)
{
	static char buf[INT2STR_MAX_LEN + 1];
	static str res;
	const char* c;

	buf[0] = '$';
	res.s = buf;

	c = int2str(i, &res.len);
	memcpy(res.s + 1, c, res.len);
	res.len++;
	return &res;
}


int build_update_sql(str* sql_cmd, db_cmd_t* cmd)
{
	struct string_buffer sql_buf = {.s = NULL, .len = 0, 
							  .size = 0, .increment = 128};
	db_fld_t* fld;
	int i, rv = 0;
	str tmpstr;

	rv = sb_add(&sql_buf, &strings[STR_UPDATE]); /* "UPDATE " */
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\""));
	rv |= sb_add(&sql_buf, &cmd->table);		 /* table name */
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\""));
	rv |= sb_add(&sql_buf, &strings[STR_SET]);	 /* " SET " */

	/* column name-value pairs */
	for(i = 0, fld = cmd->vals; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		rv |= sb_add(&sql_buf, set_str(&tmpstr, fld[i].name));
		rv |= sb_add(&sql_buf, set_str(&tmpstr, "="));
		rv |= sb_add(&sql_buf, &strings[STR_ESC]);
		if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, set_str(&tmpstr, ","));
	}
	if (rv) goto error;

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
			
			rv |= sb_add(&sql_buf, get_marker(i + 1));
			if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, &strings[STR_AND]);
		}
	}
	rv |= sb_add(&sql_buf, &strings[STR_ZT]);
	if (rv) goto error;

	sql_cmd->s = sql_buf.s;
	sql_cmd->len = sql_buf.len;
	return 0;

error:
	if (sql_buf.s) pkg_free(sql_buf.s);
	return -1;
}


int build_insert_sql(str* sql_cmd, db_cmd_t* cmd)
{
	struct string_buffer sql_buf = {.s = NULL, .len = 0, 
									.size = 0, .increment = 128};
	db_fld_t* fld;
	int i, rv = 0;
	str tmpstr;

	rv = sb_add(&sql_buf, &strings[STR_INSERT]); /* "INSERT INTO " */
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\""));
	rv |= sb_add(&sql_buf, &cmd->table);		 /* table name */
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\" ("));

	/* column names */
	for(i = 0, fld = cmd->vals; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		rv |= sb_add(&sql_buf, set_str(&tmpstr, fld[i].name));
		if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, set_str(&tmpstr, ","));
	}
	if (rv) goto error;

	rv |= sb_add(&sql_buf, &strings[STR_VALUES]);

	for(i = 0, fld = cmd->vals; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		rv |= sb_add(&sql_buf, get_marker(i + 1));
		if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, set_str(&tmpstr, ","));
	}
	rv |= sb_add(&sql_buf, set_str(&tmpstr, ")"));
	rv |= sb_add(&sql_buf, &strings[STR_ZT]);
	if (rv) goto error;
				 
	sql_cmd->s = sql_buf.s;
	sql_cmd->len = sql_buf.len;
	return 0;

error:
	if (sql_buf.s) pkg_free(sql_buf.s);
	return -1;
}


int build_delete_sql(str* sql_cmd, db_cmd_t* cmd)
{
	struct string_buffer sql_buf = {.s = NULL, .len = 0, 
									.size = 0, .increment = 128};
	db_fld_t* fld;
	int i, rv = 0;
	str tmpstr;

	rv = sb_add(&sql_buf, &strings[STR_DELETE]); /* "DELETE FROM " */
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\""));
	rv |= sb_add(&sql_buf, &cmd->table);		 /* table name */
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\""));

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
			
			rv |= sb_add(&sql_buf, get_marker(i + 1));
			if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, &strings[STR_AND]);
		}
	}
	rv |= sb_add(&sql_buf, &strings[STR_ZT]);
	if (rv) goto error;

	sql_cmd->s = sql_buf.s;
	sql_cmd->len = sql_buf.len;
	return 0;

error:
	if (sql_buf.s) pkg_free(sql_buf.s);
	return -1;
}


int build_select_sql(str* sql_cmd, db_cmd_t* cmd)
{
	struct string_buffer sql_buf = {.s = NULL, .len = 0, 
									.size = 0, .increment = 128};
	db_fld_t* fld;
	int i, rv = 0;
	str tmpstr;

	rv = sb_add(&sql_buf, &strings[STR_SELECT]); /* "SELECT " */

	if (DB_FLD_EMPTY(cmd->result)) {
		rv |= sb_add(&sql_buf, set_str(&tmpstr, "*"));
	} else {
		for(i = 0, fld = cmd->result; !DB_FLD_LAST(fld[i]); i++) {
			rv |= sb_add(&sql_buf, set_str(&tmpstr, fld[i].name));
			if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, set_str(&tmpstr, ","));
		}
	}

	rv |= sb_add(&sql_buf, &strings[STR_FROM]);  /* " FROM " */
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\""));
	rv |= sb_add(&sql_buf, &cmd->table);		 /* table name */
	rv |= sb_add(&sql_buf, set_str(&tmpstr, "\""));

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
			
			rv |= sb_add(&sql_buf, get_marker(i + 1));
			if (!DB_FLD_LAST(fld[i + 1])) rv |= sb_add(&sql_buf, &strings[STR_AND]);
		}
	}
	rv |= sb_add(&sql_buf, &strings[STR_ZT]);
	if (rv) goto error;

	sql_cmd->s = sql_buf.s;
	sql_cmd->len = sql_buf.len;
	return 0;

error:
	if (sql_buf.s) pkg_free(sql_buf.s);
	return -1;
}


int build_select_oid_sql(str* sql_cmd)
{
	struct string_buffer sql_buf = {.s = NULL, .len = 0, 
									.size = 0, .increment = 128};
	int rv = 0;
	
	rv = sb_add(&sql_buf, &strings[STR_OID]);
	rv |= sb_add(&sql_buf, &strings[STR_ZT]);
	if (rv) goto error;

	sql_cmd->s = sql_buf.s;
	sql_cmd->len = sql_buf.len;
	return 0;

 error:
	if (sql_buf.s) pkg_free(sql_buf.s);
	return -1;
}


int build_timestamp_format_sql(str* sql_cmd)
{
	struct string_buffer sql_buf = {.s = NULL, .len = 0, 
									.size = 0, .increment = 128};
	int rv = 0;
	
	rv = sb_add(&sql_buf, &strings[STR_TIMESTAMP]);
	rv |= sb_add(&sql_buf, &strings[STR_ZT]);
	if (rv) goto error;

	sql_cmd->s = sql_buf.s;
	sql_cmd->len = sql_buf.len;
	return 0;

 error:
	if (sql_buf.s) pkg_free(sql_buf.s);
	return -1;
}

/** @} */
