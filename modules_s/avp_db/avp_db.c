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
#include "../domain/domain.h"
#include "avp_db.h"

MODULE_VERSION

static char* db_url           = DEFAULT_RODB_URL;    /* Database URL */
static char* user_attrs_table = "user_attrs";
static char* uri_attrs_table  = "uri_attrs";
static char* uid_column       = "uid";
static char* username_column  = "username";
static char* did_column       = "did";
static char* name_column      = "name";
static char* type_column      = "type";
static char* val_column       = "value";
static char* flags_column     = "flags";
static char* scheme_column    = "scheme";

db_con_t* con = 0;
db_func_t db;

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
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */
    con = db.init(db_url);
    if (!con) {
	LOG(L_ERR, "avp_db:child_init: Could not initialize connection to %s\n", db_url);
	return -1;
    }
    return 0;
}

static int load_uri_attrs(struct sip_msg* msg, unsigned long flags, fparam_t* fp)
{
    int_str name, v;
    str avp_name, avp_val;
    int i, type, n;
    db_key_t keys[3], cols[4];
    db_res_t* res;
    db_val_t kv[3], *val;
    str uri;
    struct sip_uri puri;

    keys[0] = username_column;
    keys[1] = did_column;
    keys[2] = scheme_column;
    kv[0].type = DB_STR;
    kv[0].nul = 0;
    kv[1].type = DB_STR;
    kv[1].nul = 0;
    kv[2].type = DB_STR;
    kv[2].nul = 0;

    if (get_str_fparam(&uri, msg, (fparam_t*)fp) != 0) {
	ERR("Unable to get URI\n");
	return -1;
    }

    if (parse_uri(uri.s, uri.len, &puri) < 0) {
	ERR("Error while parsing URI '%.*s'\n", uri.len, uri.s);
	return -1;
    }

    kv[0].val.str_val = puri.user;

    if (puri.host.len) {
	/* domain name is present */
	if (dm_get_did(&kv[1].val.str_val, &puri.host) < 0) {
		DBG("Cannot lookup DID for domain %.*s, using default value\n", puri.host.len, ZSW(puri.host.s));
		kv[1].val.str_val.s = DEFAULT_DID;
		kv[1].val.str_val.len = sizeof(DEFAULT_DID) - 1;
	}
    } else {
	/* domain name is missing -- can be caused by tel: URI */
	DBG("There is no domain name, using default value\n");
	kv[1].val.str_val.s = DEFAULT_DID;
	kv[1].val.str_val.len = sizeof(DEFAULT_DID) - 1;
    }

    uri_type_to_str(puri.type, &(kv[2].val.str_val));

    cols[0] = name_column;
    cols[1] = type_column;
    cols[2] = val_column;
    cols[3] = flags_column;
    
    if (db.use_table(con, uri_attrs_table) < 0) {
	ERR("Error in use_table\n");
	return -1;
    }
    
    if (db.query(con, keys, 0, kv, cols, 3, 4, 0, &res) < 0) {
	ERR("Error while quering database\n");
	return -1;
    }
    
    n = 0;
    /* AVP names from DB are always strings */
    flags |= AVP_NAME_STR;
    for(i = 0; i < res->n; i++) {
	/* reset val_str as the value could be an integer */
	flags &= ~AVP_VAL_STR;
	val = res->rows[i].values;
	
	if (val[0].nul || val[1].nul || val[3].nul) {
	    ERR("Skipping row containing NULL entries\n");
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

	if (type == AVP_VAL_STR) {
		 /* String AVP */
	    v.s = avp_val;
	    flags |= AVP_VAL_STR;
	} else {
		 /* Integer AVP */
	    str2int(&avp_val, (unsigned*)&v.n);
	}
	
	if (add_avp(flags, name, v) < 0) {
	    ERR("Error while adding user attribute %.*s, skipping\n",
		avp_name.len, ZSW(avp_name.s));
	    continue;
	}
    }
    DBG("avp_db:load_attrs: %d user attributes found, %d loaded\n", res->n, n);
    db.free_result(con, res);
    return 1;
}


static int load_user_attrs(struct sip_msg* msg, unsigned long flags, fparam_t* fp)
{
    int_str name, v;
    str avp_name, avp_val;
    int i, type, n;
    db_key_t keys[1], cols[4];
    db_res_t* res;
    db_val_t kv[1], *val;

    keys[0] = uid_column;
    kv[0].type = DB_STR;
    kv[0].nul = 0;
    if (get_str_fparam(&kv[0].val.str_val, msg, (fparam_t*)fp) < 0) {
	ERR("Unable to get UID\n");
	return -1;
    }

    cols[0] = name_column;
    cols[1] = type_column;
    cols[2] = val_column;
    cols[3] = flags_column;
    
    if (db.use_table(con, user_attrs_table) < 0) {
	ERR("Error in use_table\n");
	return -1;
    }
    
    if (db.query(con, keys, 0, kv, cols, 1, 4, 0, &res) < 0) {
	ERR("Error while quering database\n");
	return -1;
    }
    
    n = 0;
    /* AVP names from DB are always strings */
    flags |= AVP_NAME_STR;
    for(i = 0; i < res->n; i++) {
	/* reset val_str as the value could be an integer */
	flags &= ~AVP_VAL_STR;
	val = res->rows[i].values;
	
	if (val[0].nul || val[1].nul || val[3].nul) {
	    ERR("Skipping row containing NULL entries\n");
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

	if (type == AVP_VAL_STR) {
		 /* String AVP */
	    v.s = avp_val;
	    flags |= AVP_VAL_STR;
	} else {
		 /* Integer AVP */
	    str2int(&avp_val, (unsigned*)&v.n);
	}
	
	if (add_avp(flags, name, v) < 0) {
	    ERR("Error while adding user attribute %.*s, skipping\n",
		avp_name.len, ZSW(avp_name.s));
	    continue;
	}
    }
    DBG("avp_db:load_attrs: %d user attributes found, %d loaded\n", res->n, n);
    db.free_result(con, res);
    return 1;
}


/*
 * Load user attributes
 */
static int load_attrs(struct sip_msg* msg, char* fl, char* fp)
{
    unsigned long flags;
    
    if (!con) {
	ERR("Invalid database handle\n");
	return -1;
    }
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
