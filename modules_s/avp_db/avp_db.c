/*
 * $Id$
 *
 * Copyright (C) 2004 FhG Fokus
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
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../str.h"
#include "../../db/db.h"
#include "../../config.h"
#include "../../usr_avp.h"


MODULE_VERSION


#define CALLER_PREFIX "caller_"
#define CALLER_PREFIX_LEN (sizeof(CALLER_PREFIX) - 1)

#define CALLEE_PREFIX "callee_"
#define CALLEE_PREFIX_LEN (sizeof(CALLEE_PREFIX) - 1)


typedef enum load_avp_param {
	LOAD_CALLER_UUID,  /* Use the caller's UUID as the key */
	LOAD_CALLEE_UUID,  /* Use the callee's UUID as the key */
	LOAD_CALLER,       /* Use the caller's username and domain as the key */
	LOAD_CALLEE        /* Use the callee's username and domain as the key */
} load_avp_param_t;


static char* db_url          = DEFAULT_RODB_URL;    /* Database URL */
static char* db_table        = "usr_preferences";
static char* uuid_column     = "uuid";
static char* username_column = "username";
static char* domain_column   = "domain";
static char* attr_column     = "attribute";
static char* val_column      = "value";
static str caller_prefix     = {CALLER_PREFIX, CALLER_PREFIX_LEN};
static str callee_prefix     = {CALLEE_PREFIX, CALLEE_PREFIX_LEN};
static int caller_uuid_avp   = 1;
static int callee_uuid_avp   = 2;
static int use_domain        = 0;

static db_con_t* db_handle;
static db_func_t dbf;


static int load_avp(struct sip_msg*, char*, char*);
static int mod_init(void);
static int child_init(int);
static int load_avp_fixup(void**, int);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"avp_load", load_avp, 1, load_avp_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"caller_uuid_avp", INT_PARAM, &caller_uuid_avp},
	{"callee_uuid_avp", INT_PARAM, &callee_uuid_avp},
	{"db_url",          STR_PARAM, &db_url         },
	{"pref_table",      STR_PARAM, &db_table       },
	{"uuid_column",     STR_PARAM, &uuid_column    },
	{"username_column", STR_PARAM, &username_column},
	{"domain_column",   STR_PARAM, &domain_column  },
	{"attr_column",     STR_PARAM, &attr_column    },
	{"val_column",      INT_PARAM, &val_column     },
	{"use_domain",      INT_PARAM, &use_domain     },
	{0, 0, 0}
};





struct module_exports exports = {
	"avp_db",
	cmds,        /* Exported commands */
	params,      /* Exported parameters */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	0,           /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};


static int mod_init(void)
{
        DBG("avp_db - initializing\n");

	if (bind_dbmod(db_url, &dbf) < 0) {
		LOG(L_ERR, "avpdb_mod_init: Unable to bind a database driver\n");
		return -1;
	}

	if (!DB_CAPABILITY(dbf, DB_CAP_QUERY)) {
		LOG(L_ERR, "avpdb_mod_init: Selected database driver does not suppor the query capability\n");
		return -1;
	}

	caller_prefix.len = strlen(caller_prefix.s);
	callee_prefix.len = strlen(callee_prefix.s);
    
        return 0;
}


static int child_init(int rank)
{
	DBG("avp_db - Initializing child %i\n", rank);

	db_handle = dbf.init(db_url);
	
	if (!db_handle) {
		LOG(L_ERR, "avpdb_init_child: could not initialize connection to %s\n",  db_url);
		return -1;
	}
	
	return 0;
}


static int query_db(str* prefix, str* uuid, str* username, str* domain)
{
	int name_len, err = -1;
	
	db_key_t  cols[2], keys[2];
	db_val_t  vals[2];
	db_res_t* res;
	db_row_t* cur_row;
	
	int_str name, val;
	str name_str, val_str;

	cols[0] = attr_column;
	cols[1] = val_column;
	
	if (uuid) {
		keys[0] = uuid_column;
		VAL_TYPE(vals) = DB_STR;
		VAL_NULL(vals) = 0;
		VAL_STR(vals)  = *uuid;
	} else {
		keys[0] = username_column;
		VAL_TYPE(vals) = DB_STR;
		VAL_NULL(vals) = 0;
		VAL_STR(vals) = *username;
	}

	if (use_domain) {
		keys[1] = domain_column;
		VAL_TYPE(vals + 1) = DB_STR;
		VAL_NULL(vals + 1) = 0;
		VAL_STR(vals + 1) = *domain;
	}
	
	if (dbf.use_table(db_handle, db_table) < 0) {
		LOG(L_ERR, "query_db: Unable to change the table\n");
	}

	err = dbf.query(db_handle, keys, 0, vals, cols, (use_domain ? 2 : 1), 2, 0, &res);
	if (err) {
		LOG(L_ERR,"query_db: db_query failed.");
		return -1;
	}
	
	name.s = &name_str;
	val.s  = &val_str;

	for (cur_row = res->rows; cur_row < res->rows + res->n; cur_row++) {
		if (VAL_NULL(ROW_VALUES(cur_row)) || VAL_NULL(ROW_VALUES(cur_row) + 1)) {
			continue;
		}

		name_len = strlen((char*)VAL_STRING(ROW_VALUES(cur_row)));
		name_str.len = prefix->len + name_len;
		name_str.s = pkg_malloc(name_str.len);
		if (name_str.s == 0) {
			LOG(L_ERR, "query_db: Out of memory");
			dbf.free_result(db_handle, res);
			return -1;
		}
		    
		memcpy(name_str.s, prefix->s, prefix->len);
		memcpy(name_str.s + prefix->len, 
		       (char*)VAL_STRING(ROW_VALUES(cur_row)), name_len);
	
		val_str.len = strlen((char*)VAL_STRING(ROW_VALUES(cur_row) + 1));
		val_str.s = (char*)VAL_STRING(ROW_VALUES(cur_row) + 1);
		
		err = add_avp(AVP_NAME_STR | AVP_VAL_STR, name, val);
		if (err != 0) {
			LOG(L_ERR, "query_db: add_avp failed\n");
			pkg_free(name_str.s);
			dbf.free_result(db_handle, res);
		}
	
		DBG("query_db: AVP '%.*s'='%.*s' has been added\n", name_str.len, 
		    name_str.s,
		    val_str.len,
		    val_str.s);
	}
	
	dbf.free_result(db_handle, res);
	return 1;
}


static int load_avp_uuid(struct sip_msg* msg, str* prefix, int avp_id)
{
	struct usr_avp *uuid;
	int_str attr_istr, val_istr;

	attr_istr.n = avp_id;
	
	uuid = search_first_avp(AVP_VAL_STR, attr_istr, &val_istr);
	if (!uuid) {
		LOG(L_ERR, "load_avp_uuid: no AVP with id %d was found\n", avp_id);
		return -1;
	}
	
	if (!(uuid->flags & AVP_VAL_STR)) {
		LOG(L_ERR, "load_avp_uuid: value for <%d> should "
		    "be of type string\n", avp_id);
		return -1;
	}
	
	return query_db(prefix, val_istr.s, 0, 0);

}


static int load_avp_user(struct sip_msg* msg, str* prefix, load_avp_param_t param)
{
	str* uri;
	struct sip_uri puri;

	if (param == LOAD_CALLER) {
		if (parse_from_header(msg) < 0) {
			LOG(L_ERR, "load_avp_user: Error while parsing From header field\n");
			return -1;
		}

		uri = &get_from(msg)->uri;
		if (parse_uri(uri->s, uri->len, &puri) == -1) {
			LOG(L_ERR, "load_avp_user: Error while parsing From URI\n");
			return -1;
		}

		return query_db(prefix, 0, &puri.user, &puri.host);
	} else if (param == LOAD_CALLEE) {
		if (parse_sip_msg_uri(msg) < 0) {
			LOG(L_ERR, "load_avp_user: Request-URI parsing failed\n");
			return -1;
		}

		if (msg->parsed_uri_ok != 1) {
			LOG(L_ERR, "load_avp_user: Unable to parse Request-URI\n");
			return -1;
		}

		return query_db(prefix, 0, &msg->parsed_uri.user, &msg->parsed_uri.host);

	} else {
		LOG(L_ERR, "load_avp_user: Unknown header field type\n");
		return -1;
	}
}



static int load_avp(struct sip_msg* msg, char* attr, char* _dummy)
{
	switch((load_avp_param_t)attr) {
	case LOAD_CALLER_UUID:
		return load_avp_uuid(msg, &caller_prefix, caller_uuid_avp);
		break;

	case LOAD_CALLEE_UUID:
		return load_avp_uuid(msg, &callee_prefix, callee_uuid_avp);
		break;

	case LOAD_CALLER:
		return load_avp_user(msg, &caller_prefix, LOAD_CALLER);
		break;

	case LOAD_CALLEE:
		return load_avp_user(msg, &callee_prefix, LOAD_CALLEE);
		break;
		
	default:
		LOG(L_ERR, "load_avp: Unknown parameter value\n");
		return -1;
	}
}


static int load_avp_fixup(void** param, int param_no)
{
	long id = 0;

	if (param_no == 1) {
		if (!strcasecmp(*param, "caller_uuid")) {
			id = LOAD_CALLER_UUID;
		} else if (!strcasecmp(*param, "callee_uuid")) {
			id = LOAD_CALLEE_UUID;
		} else if (!strcasecmp(*param, "caller")) {
			id = LOAD_CALLER;
		} else if (!strcasecmp(*param, "callee")) {
			id = LOAD_CALLEE;
		} else {
			LOG(L_ERR, "load_avp_fixup: Unknown parameter\n");
			return -1;
		}
	}

	pkg_free(*param);
	*param=(void*)id;
	return 0;
}
