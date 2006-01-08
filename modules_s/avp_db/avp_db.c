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
#include "../../db/db.h"
#include "../../config.h"
#include "../../usr_avp.h"
#include "../../ut.h"
#include "../../id.h"
#include "avp_db.h"

MODULE_VERSION

typedef enum load_avp_param {
	LOAD_FROM_UID,  /* Use the caller's UID as the key */
	LOAD_TO_UID,    /* Use the callee's UID as the key */
} load_avp_param_t;


static char* db_url           = DEFAULT_RODB_URL;    /* Database URL */
static char* db_table         = "user_attrs";
static char* uid_column       = "uid";
static char* name_column      = "name";
static char* type_column      = "type";
static char* val_column       = "value";
static char* flags_column     = "flags";

db_con_t* con = 0;
db_func_t db;

static int mod_init(void);
static int child_init(int);
static int load_avps(struct sip_msg* msg, char* s1, char* s2);
static int load_avps_fixup(void** param, int param_no);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"load_attrs", load_avps, 1, load_avps_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",       PARAM_STRING, &db_url      },
	{"uid_column",   PARAM_STRING, &uid_column  },
	{"name_column",  PARAM_STRING, &name_column },
	{"type_column",  PARAM_STRING, &name_column },
	{"value_column", PARAM_STRING, &val_column  },
	{"flags_column", PARAM_STRING, &flags_column  },
	{0, 0, 0}
};




struct module_exports exports = {
	"avp_db",
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
	if (bind_dbmod(db_url, &db) < 0) {
		LOG(L_ERR, "avp_db:mod_init: Unable to bind a database driver\n");
		return -1;
	}

	if (!DB_CAPABILITY(db, DB_CAP_QUERY)) {
		LOG(L_ERR, "avp_db:mod_init: Selected database driver does not suppor the query capability\n");
		return -1;
	}

	return 0;
}


static int child_init(int rank)
{
	con = db.init(db_url);
	if (!con) {
		LOG(L_ERR, "avp_db:child_init: Could not initialize connection to %s\n", db_url);
		return -1;
	}
	return 0;
}


/*
 * Load attributes from domain_attrs table
 */
static int load_attrs(str* uid, int track)
{
	int_str name, v;

	str avp_name, avp_val;
	int i, type, n;
	db_key_t keys[1], cols[4];
	db_res_t* res;
	db_val_t kv[1], *val;
	unsigned short flags;

	if (!con) {
		LOG(L_ERR, "avp_db:load_attrs: Invalid database handle\n");
		return -1;
	}

	keys[0] = uid_column;
	kv[0].type = DB_STR;
	kv[0].nul = 0;
	kv[0].val.str_val = *uid;

	cols[0] = name_column;
	cols[1] = type_column;
	cols[2] = val_column;
	cols[3] = flags_column;

	if (db.use_table(con, db_table) < 0) {
		LOG(L_ERR, "avp_db:load_attrs: Error in use_table\n");
		return -1;
	}

	if (db.query(con, keys, 0, kv, cols, 1, 4, 0, &res) < 0) {
		LOG(L_ERR, "avp_db:load_attrs: Error while quering database\n");
		return -1;
	}

	n = 0;
	for(i = 0; i < res->n; i++) {
		val = res->rows[i].values;

		if (val[0].nul || val[1].nul || val[3].nul) {
			LOG(L_ERR, "avp_db:load_attrs: Skipping row containing NULL entries\n");
			continue;
		}

		if ((val[3].val.int_val & DB_LOAD_SER) == 0) continue;

		n++;
		     /* Get AVP name */
		avp_name.s = (char*)val[0].val.string_val;
		avp_name.len = strlen(avp_name.s);
		name.s = avp_name;

		     /* Get AVP type */
		type = val[1].val.int_val;

		     /* Test for NULL value */
		if (val[2].nul) {
			avp_val.s = 0;
			avp_val.len = 0;
		} else {
			avp_val.s = (char*)val[2].val.string_val;
			avp_val.len = strlen(avp_val.s);
		}

		flags = AVP_CLASS_USER | AVP_NAME_STR;
		if (track == LOAD_FROM_UID) {
			flags |= AVP_TRACK_FROM;
		} else {
			flags |= AVP_TRACK_TO;
		}

		if (type == AVP_VAL_STR) {
			     /* String AVP */
			v.s = avp_val;
			flags |= AVP_VAL_STR;
		} else {
			     /* Integer AVP */
			str2int(&avp_val, (unsigned*)&v.n);
		}

		if (add_avp(flags, name, v) < 0) {
			LOG(L_ERR, "avp_db:load_attrs: Error while adding user attribute %.*s, skipping\n",
			    avp_name.len, ZSW(avp_name.s));
			continue;
		}
	}
	DBG("avp_db:load_attrs: %d user attributes found, %d loaded\n", res->n, n);
	db.free_result(con, res);
	return 1;
}



static int load_avps(struct sip_msg* msg, char* attr, char* dummy)
{
	str uid;

	switch((load_avp_param_t)attr) {
	case LOAD_FROM_UID:
		if (get_from_uid(&uid, msg) < 0) return -1;
		return load_attrs(&uid, LOAD_FROM_UID);
		break;

	case LOAD_TO_UID:
		if (get_to_uid(&uid, msg) < 0) return -1;
		return load_attrs(&uid, LOAD_TO_UID);
		break;

	default:
		LOG(L_ERR, "load_avps: Unknown parameter value\n");
		return -1;
	}
}


static int load_avps_fixup(void** param, int param_no)
{
	long id = 0;

	if (param_no == 1) {
		if (!strcasecmp(*param, "from.uid")) {
			id = LOAD_FROM_UID;
		} else if (!strcasecmp(*param, "to.uid")) {
			id = LOAD_TO_UID;
		} else {
			LOG(L_ERR, "avp_db:load_avps_fixup: Unknown parameter\n");
			return -1;
		}
	}

	pkg_free(*param);
	*param=(void*)id;
	return 0;
}
