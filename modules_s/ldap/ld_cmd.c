/* 
 * $Id$ 
 *
 * LDAP Database Driver for SER
 *
 * Copyright (C) 2008 iptelorg GmbH
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

/** \addtogroup ldap
 * @{ 
 */

/** \file
 * Implementation of functions related to database commands.
 */

#include "ld_cmd.h"
#include "ld_fld.h"
#include "ld_con.h"
#include "ld_mod.h"
#include "ld_uri.h"
#include "ld_config.h"
#include "ld_res.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <string.h>


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
			ERR("ldap: No memory left\n");
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


/** Destroys a ld_cmd structure.
 * This function frees all memory used by ld_cmd structure.
 * @param cmd A pointer to generic db_cmd command being freed.
 * @param payload A pointer to ld_cmd structure to be freed.
 */
static void ld_cmd_free(db_cmd_t* cmd, struct ld_cmd* payload)
{
	db_drv_free(&payload->gen);
	if (payload->result) pkg_free(payload->result);
	pkg_free(payload);
}


static int build_result_array(char*** res, db_cmd_t* cmd)
{
	struct ld_fld* lfld;
	char** t;
	int i;
	if (cmd->result_count == 0) {
		*res = NULL;
		return 0;
	}
	
	t = (char**)pkg_malloc(sizeof(char*) * (cmd->result_count + 1));
	if (t == NULL) {
		ERR("ldap: No memory left\n");
		return -1;
	}
	t[cmd->result_count] = NULL;

	for(i = 0; i < cmd->result_count; i++) {
		lfld = DB_GET_PAYLOAD(cmd->result + i);
		/* Attribute names are always zero terminated */
		t[i] = lfld->attr.s;
	}

	*res = t;
	return 0;
}


static int build_search_filter(char** dst, db_fld_t* fld, str* filter_add)
{
	struct ld_fld* lfld;
	struct string_buffer sql_buf = {.s = NULL, .len = 0, 
									.size = 0, .increment = 128};
	int i, rv = 0;
	str tmpstr;
	static str zt = STR_STATIC_INIT("\0");

	/* Return NULL if there are no fields and no preconfigured search
	 * string supplied in the configuration file
	 */
	if ((DB_FLD_EMPTY(fld) || DB_FLD_LAST(fld[0])) &&
		((filter_add->s == NULL) || !filter_add->len)) {
		*dst = NULL;
		return 0;
	}

	rv = sb_add(&sql_buf, set_str(&tmpstr, "(&"));
	if (filter_add->s && filter_add->len) {
		/* Add the filter component specified in the config file */
		rv |= sb_add(&sql_buf, filter_add);
	}

	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {

		rv |= sb_add(&sql_buf, set_str(&tmpstr, "("));

		lfld = DB_GET_PAYLOAD(fld + i);	
		rv |= sb_add(&sql_buf, &lfld->attr);
		switch(fld[i].op) {
		case DB_EQ: /* The value of the field must be equal */
			rv |= sb_add(&sql_buf, set_str(&tmpstr, "="));
			break;

		case DB_LT:     /* The value of the field must be less than */
		case DB_LEQ:    /* The value of the field must be less than or equal */
			rv |= sb_add(&sql_buf, set_str(&tmpstr, "<="));
			break;

		case DB_GT:     /* The value of the field must be greater than */
		case DB_GEQ:     /* The value of the field must be greater than or equal */
			rv |= sb_add(&sql_buf, set_str(&tmpstr, ">="));
			break;

		default:
			ERR("ldap: Unsupported operator encountered: %d\n", fld[i].op);
			goto error;
		}

		if ((fld[i].flags & DB_NULL) == 0) {
			switch(fld[i].type) {
			case DB_CSTR:
				rv |= sb_add(&sql_buf, set_str(&tmpstr, fld[i].v.cstr));
				break;
				
			case DB_STR:
				rv |= sb_add(&sql_buf, &fld[i].v.lstr);
				break;
			
			default:
				ERR("ldap: Unsupported field type encountered: %d\n", fld[i].type);
				goto error;
			}
		}
		
		rv |= sb_add(&sql_buf, set_str(&tmpstr, ")"));
	}

	rv |= sb_add(&sql_buf, set_str(&tmpstr, ")"));
	rv |= sb_add(&sql_buf, &zt);
	if (rv) goto error;

	*dst = sql_buf.s;
	return 0;

 error:
	if (sql_buf.s) pkg_free(sql_buf.s);
	return -1;	
}


int ld_cmd(db_cmd_t* cmd)
{
	struct ld_cmd* lcmd;
	struct ld_config* cfg;
 
	lcmd = (struct ld_cmd*)pkg_malloc(sizeof(struct ld_cmd));
	if (lcmd == NULL) {
		ERR("ldap: No memory left\n");
		goto error;
	}
	memset(lcmd, '\0', sizeof(struct ld_cmd));
	if (db_drv_init(&lcmd->gen, ld_cmd_free) < 0) goto error;

	switch(cmd->type) {
	case DB_PUT:
		ERR("ldap: DB_PUT not supported\n");
		goto error;
		break;
		
	case DB_DEL:
		ERR("ldap: DB_DEL not supported\n");
		goto error;
		break;

	case DB_GET:		
		break;

	case DB_UPD:
		ERR("ldap: DB_UPD not supported\n");
		goto error;
		break;
		
	case DB_SQL:
		ERR("ldap: DB_SQL not supported\n");
		goto error;
        break;
	}

	cfg = ld_find_config(&cmd->table);
	if (cfg == NULL) {
		ERR("ldap: Cannot find configuration for '%.*s', giving up\n",
			STR_FMT(&cmd->table));
		goto error;
	}

	lcmd->base = cfg->base;
	lcmd->scope = cfg->scope;

	if (cfg->filter) {
		lcmd->filter.s = cfg->filter;
		lcmd->filter.len = strlen(cfg->filter);
	}

	if (ld_resolve_fld(cmd->match, cfg) < 0) goto error;
	if (ld_resolve_fld(cmd->result, cfg) < 0) goto error;

	if (build_result_array(&lcmd->result, cmd) < 0) goto error;

	DB_SET_PAYLOAD(cmd, lcmd);
	return 0;

 error:
	if (lcmd) {
		DB_SET_PAYLOAD(cmd, NULL);
		db_drv_free(&lcmd->gen);
		if (lcmd->result) pkg_free(lcmd->result);
		pkg_free(lcmd);
	}
	return -1;
}


int ld_cmd_exec(db_res_t* res, db_cmd_t* cmd)
{
	db_con_t* con;
	struct ld_res* lres;
	struct ld_cmd* lcmd;
	struct ld_con* lcon;
	char* filter;
	int ret;
	LDAPMessage* msg;

	/* First things first: retrieve connection info from the currently active
	 * connection and also mysql payload from the database command
	 */
	con = cmd->ctx->con[db_payload_idx];
	lcmd = DB_GET_PAYLOAD(cmd);
	lcon = DB_GET_PAYLOAD(con);

	if (build_search_filter(&filter, cmd->match, &lcmd->filter) < 0) {
		ERR("ldap: Error while building LDAP search filter\n");
		return -1;
	}

	ret = ldap_search_ext_s(lcon->con, lcmd->base, lcmd->scope, filter,
							lcmd->result, 0, NULL, NULL, NULL, 0, &msg);
	if (filter) pkg_free(filter);

	if (ret != LDAP_SUCCESS) {
		ERR("ldap: Error in ldap_search: %s\n", ldap_err2string(ret));
		return -1;
	}

	if (res) {
		lres = DB_GET_PAYLOAD(res);
		if (lres->msg) ldap_msgfree(lres->msg);
		lres->msg = msg;
	} else {
		ldap_msgfree(msg);
	}

	return 0;
}


int ld_cmd_first(db_res_t* res)
{
	db_con_t* con;
	struct ld_res* lres;
	struct ld_con* lcon;

	lres = DB_GET_PAYLOAD(res);
	/* FIXME */
	con = res->cmd->ctx->con[db_payload_idx];
	lcon = DB_GET_PAYLOAD(con);

	lres->current = ldap_first_message(lcon->con, lres->msg);
	while(lres->current) {
		if (ldap_msgtype(lres->current) == LDAP_RES_SEARCH_ENTRY) {
			break;
		}
		lres->current = ldap_next_message(lcon->con, lres->msg);
	}
	if (lres->current == NULL) return 1;

	if (ld_ldap2fld(res->cmd->result, lcon->con, lres->current) < 0) return -1;
	res->cur_rec->fld = res->cmd->result;
	return 0;
}


int ld_cmd_next(db_res_t* res)
{
	db_con_t* con;
	struct ld_res* lres;
	struct ld_con* lcon;

	lres = DB_GET_PAYLOAD(res);
	/* FIXME */
	con = res->cmd->ctx->con[db_payload_idx];
	lcon = DB_GET_PAYLOAD(con);

	if (lres->current == NULL) return 1;

	lres->current = ldap_next_message(lcon->con, lres->current);
	while(lres->current) {
		if (ldap_msgtype(lres->current) == LDAP_RES_SEARCH_ENTRY) {
			break;
		}
		lres->current = ldap_next_message(lcon->con, lres->current);
	}
	if (lres->current == NULL) return 1;
	if (ld_ldap2fld(res->cmd->result, lcon->con, lres->current) < 0) return -1;
	res->cur_rec->fld = res->cmd->result;
	return 0;
}

/** @} */
