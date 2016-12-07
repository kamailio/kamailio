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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * History:
 * --------
 * 2004-06-14 added ability to read default values from DB table usr_preferences_types (kozlik)
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
#include "../../lib/srdb2/db.h"
#include "../../config.h"
#include "../../usr_avp.h"
#include "../../ut.h"
#include "../../id.h"
#include "../uid_domain/domain.h"
#include "uid_avp_db.h"
#include "extra_attrs.h"

MODULE_VERSION

static char* db_url           = DEFAULT_RODB_URL;    /* Database URL */
static char* user_attrs_table = "uid_user_attrs";
static char* uri_attrs_table  = "uid_uri_attrs";
static char* uid_column       = "uid";
static char* username_column  = "username";
static char* did_column       = "did";
static char* name_column      = "name";
static char* type_column      = "type";
static char* val_column       = "value";
static char* flags_column     = "flags";
static char* scheme_column    = "scheme";
int   auto_unlock      = 0;

db_ctx_t* ctx = 0;
db_cmd_t *load_user_attrs_cmd = NULL;
db_cmd_t *load_uri_attrs_cmd = NULL;

static int mod_init(void);
static int child_init(int);
static int load_attrs(struct sip_msg* msg, char* s1, char* s2);
static int attrs_fixup(void** param, int param_no);


static domain_get_did_t dm_get_did = NULL;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"load_attrs", load_attrs, 2, attrs_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	
	/* functions for loading/storing flagged attributes into DB */
    {"load_extra_attrs", load_extra_attrs, 2, extra_attrs_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"save_extra_attrs", save_extra_attrs, 2, extra_attrs_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"remove_extra_attrs", remove_extra_attrs, 2, extra_attrs_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},

	/* locking attrs - needed for proper work! */
    {"lock_extra_attrs", lock_extra_attrs, 2, extra_attrs_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"unlock_extra_attrs", unlock_extra_attrs, 2, extra_attrs_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},	
	
    {0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"db_url",           PARAM_STRING, &db_url          },
    {"user_attrs_table", PARAM_STRING, &user_attrs_table},
    {"uri_attrs_table",  PARAM_STRING, &uri_attrs_table },
    {"uid_column",       PARAM_STRING, &uid_column      },
    {"username_column",  PARAM_STRING, &username_column },
    {"did_column",       PARAM_STRING, &did_column      },
    {"name_column",      PARAM_STRING, &name_column     },
    {"type_column",      PARAM_STRING, &type_column     },
    {"value_column",     PARAM_STRING, &val_column      },
    {"flags_column",     PARAM_STRING, &flags_column    },
    {"scheme_column",    PARAM_STRING, &scheme_column   },

	{"attr_group", PARAM_STR | PARAM_USE_FUNC, (void*)declare_attr_group },
	{"auto_unlock_extra_attrs", PARAM_INT, &auto_unlock },
    {0, 0, 0}
};


struct module_exports exports = {
    "uid_avp_db",
    cmds,        /* Exported commands */
    0,           /* RPC methods */
    params,      /* Exported parameters */
    mod_init,    /* module initialization function */
    0,           /* response function*/
    0,           /* destroy function */
    0,           /* oncancel function */
    child_init   /* per-child init function */
};


static int mod_init(void)
{
	return init_extra_avp_locks();
}


static int child_init(int rank)
{
	db_fld_t res_cols[] = {
		{.name = name_column, .type = DB_STR},
		{.name = type_column, .type = DB_INT},
		{.name = val_column, .type = DB_STR},
		{.name = flags_column, .type = DB_BITMAP},
		{.name = NULL}
	};
	db_fld_t match_uri[] = {
		{.name = username_column, .type = DB_STR, .op = DB_EQ},
		{.name = did_column, .type = DB_STR, .op = DB_EQ},
		{.name = scheme_column, .type = DB_STR, .op = DB_EQ},
		{.name = NULL}
	};
	db_fld_t match_user[] = {
		{.name = uid_column, .type = DB_STR, .op = DB_EQ},
		{.name = NULL}
	};

	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */
	
	ctx = db_ctx("avp_db");
	if (!ctx) goto err;
	if (db_add_db(ctx, db_url) < 0) goto err;
	if (db_connect(ctx) < 0) goto err;
	
	load_uri_attrs_cmd = db_cmd(DB_GET, ctx, uri_attrs_table, res_cols, match_uri, NULL);
	if (!load_uri_attrs_cmd) goto err;

	load_user_attrs_cmd = db_cmd(DB_GET, ctx, user_attrs_table, res_cols, match_user, NULL);
	if (!load_user_attrs_cmd) goto err;

	if (init_extra_avp_queries(ctx) < 0) goto err;

    return 0;

err:
	if (load_uri_attrs_cmd) db_cmd_free(load_uri_attrs_cmd);
	if (load_user_attrs_cmd) db_cmd_free(load_user_attrs_cmd);

	if (ctx) db_ctx_free(ctx);

	ERR("Error while initializing database layer\n");
	return -1;
}

#define IS_DB_NULL(f)	(f.flags & DB_NULL)

static void read_attrs(db_res_t *res, unsigned long flags)
{
    int_str name, v;
    str avp_val;
    int type, n, found;
	db_rec_t* row;

	n = 0;
	found = 0;
	/* AVP names from DB are always strings */
	flags |= AVP_NAME_STR;
	if (res) row = db_first(res);
	else row = NULL;
	while (row) {
		found++;

		if (IS_DB_NULL(row->fld[0]) || 
				IS_DB_NULL(row->fld[1]) ||
				IS_DB_NULL(row->fld[3])) {
			ERR("Skipping row containing NULL entries\n");
			row = db_next(res);
			continue;
		}

		if ((row->fld[3].v.int4 & SRDB_LOAD_SER) == 0) {
			row = db_next(res);
			continue;
		}

		n++;
		/* Get AVP name */
		name.s = row->fld[0].v.lstr;

		/* Get AVP type */
		type = row->fld[1].v.int4;

		/* Test for NULL value */
		if (IS_DB_NULL(row->fld[2])) {
			avp_val.s = 0;
			avp_val.len = 0;
		} else {
			avp_val = row->fld[2].v.lstr;
		}

		if (type == AVP_VAL_STR) {
			/* String AVP */
			v.s = avp_val;
			flags |= AVP_VAL_STR;
		} else {
			/* Integer AVP */
			str2int(&avp_val, (unsigned*)&v.n);
			/* reset val_str as the value could be an integer */
			flags &= ~AVP_VAL_STR;
		}

		if (add_avp(flags, name, v) < 0) {
			ERR("Error while adding user attribute %.*s, skipping\n",
					name.s.len, ZSW(name.s.s));
		}
		row = db_next(res);

	}
	DBG("avp_db:load_attrs: %d attributes found, %d loaded\n", found, n);
}

static int load_uri_attrs(struct sip_msg* msg, unsigned long flags, fparam_t* fp)
{
    db_res_t* res;
    str uri;
    struct sip_uri puri;
	static str default_did = STR_STATIC_INIT(DEFAULT_DID);

	if (get_str_fparam(&uri, msg, (fparam_t*)fp) != 0) {
		DBG("Unable to get URI from load_uri_attrs parameters\n");
		return -1;
	}

	if (parse_uri(uri.s, uri.len, &puri) < 0) {
		ERR("Error while parsing URI '%.*s'\n", uri.len, uri.s);
		return -1;
	}

    load_uri_attrs_cmd->match[0].v.lstr = puri.user;

	if (puri.host.len) {
		/* domain name is present */
		if (dm_get_did(&load_uri_attrs_cmd->match[1].v.lstr, &puri.host) < 0) {
			DBG("Cannot lookup DID for domain %.*s, using default value\n", puri.host.len, ZSW(puri.host.s));
			load_uri_attrs_cmd->match[1].v.lstr = default_did;
		}
	} else {
		/* domain name is missing -- can be caused by tel: URI */
		DBG("There is no domain name, using default value\n");
		load_uri_attrs_cmd->match[1].v.lstr = default_did;
	}

    uri_type_to_str(puri.type, &(load_uri_attrs_cmd->match[2].v.lstr));

	if (db_exec(&res, load_uri_attrs_cmd) < 0) {
		ERR("Error while querying database\n");
		return -1;
    }
    
	if (res) {
		read_attrs(res, flags);
		db_res_free(res);
	}
	return 1;
}


static int load_user_attrs(struct sip_msg* msg, unsigned long flags, fparam_t* fp)
{
    db_res_t* res;

	if (get_str_fparam(&load_user_attrs_cmd->match[0].v.lstr, msg, (fparam_t*)fp) < 0) {
		DBG("Unable to get UID from load_user_attrs parameter\n");
		return -1;
	}

	if (db_exec(&res, load_user_attrs_cmd) < 0) {
		ERR("Error while querying database\n");
		return -1;
    }
    
	if (res) {
		read_attrs(res, flags);
		db_res_free(res);
	}
	return 1;
}


/*
 * Load user attributes
 */
static int load_attrs(struct sip_msg* msg, char* fl, char* fp)
{
	unsigned long flags;

	flags = (unsigned long)fl;

	if (flags & AVP_CLASS_URI) {
		return load_uri_attrs(msg, flags, (fparam_t*)fp);
	} else {
		return load_user_attrs(msg, flags, (fparam_t*)fp);
	}
}


static int attrs_fixup(void** param, int param_no)
{
    unsigned long flags;
    char* s;
    
	if (param_no == 1) {
		/* Determine the track and class of attributes to be loaded */
		s = (char*)*param;
		flags = 0;
		if (*s != '$' || (strlen(s) != 3)) {
			ERR("Invalid parameter value, $xy expected\n");
			return -1;
		}
		switch((s[1] << 8) + s[2]) {
			case 0x4655: /* $fu */
			case 0x6675:
			case 0x4675:
			case 0x6655:
				flags = AVP_TRACK_FROM | AVP_CLASS_USER;
				break;

			case 0x4652: /* $fr */
			case 0x6672:
			case 0x4672:
			case 0x6652:
				flags = AVP_TRACK_FROM | AVP_CLASS_URI;
				break;

			case 0x5455: /* $tu */
			case 0x7475:
			case 0x5475:
			case 0x7455:
				flags = AVP_TRACK_TO | AVP_CLASS_USER;
				break;

			case 0x5452: /* $tr */
			case 0x7472:
			case 0x5472:
			case 0x7452:
				flags = AVP_TRACK_TO | AVP_CLASS_URI;
				break;

			default:
				ERR("Invalid parameter value: '%s'\n", s);
				return -1;
		}

		if ((flags & AVP_CLASS_URI) && !dm_get_did) {
			dm_get_did = (domain_get_did_t)find_export("get_did", 0, 0);
			if (!dm_get_did) {
				ERR("Domain module required but not found\n");
				return -1;
			}
		}

		pkg_free(*param);
		*param = (void*)flags;
	} else if (param_no == 2) {
		return fixup_var_str_12(param, 2);
	}
    return 0;
}
