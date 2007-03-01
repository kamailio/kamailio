/*
 * $Id$
 *
 * Various URI related functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 * History:
 * -------
 *  2003-03-11: New module interface (janakj)
 *  2003-03-16: flags export parameter added (janakj)
 *  2003-03-19  replaces all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-05: default_uri #define used (jiri)
 *  2004-03-20: has_totag introduced (jiri)
 *  2004-06-07  updated to the new DB api (andrei)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../id.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../db/db.h"
#include "uridb_mod.h"
#include "../../usr_avp.h"
#include "../domain/domain.h"

MODULE_VERSION

#define USE_RURI 0
#define USE_FROM 1
#define USE_TO 2


/*
 * Version of domain table required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define URI_TABLE_VERSION 2

static void destroy(void);       /* Module destroy function */
static int child_init(int rank); /* Per-child initialization function */
static int mod_init(void);       /* Module initialization function */
static int lookup_user(struct sip_msg* msg, char* s1, char* s2);
static int lookup_user_2(struct sip_msg* msg, char* s1, char* s2);
static int check_uri(struct sip_msg* msg, char* s1, char* s2);
static int header_fixup(void** param, int param_no);
static int lookup_user_fixup(void** param, int param_no);

#define URI_TABLE    "uri"
#define UID_COL      "uid"
#define DID_COL      "did"
#define USERNAME_COL "username"
#define FLAGS_COL    "flags"
#define SCHEME_COL   "scheme"

#define CANONICAL_AVP "ruri_canonical"
#define CANONICAL_AVP_VAL "1"

/*
 * Module parameter variables
 */
str db_url       = STR_STATIC_INIT(DEFAULT_RODB_URL);
str uri_table    = STR_STATIC_INIT(URI_TABLE);
str uid_col      = STR_STATIC_INIT(UID_COL);
str did_col      = STR_STATIC_INIT(DID_COL);
str username_col = STR_STATIC_INIT(USERNAME_COL);
str flags_col    = STR_STATIC_INIT(FLAGS_COL);
str scheme_col   = STR_STATIC_INIT(SCHEME_COL);
str canonical_avp = STR_STATIC_INIT(CANONICAL_AVP);
str canonical_avp_val = STR_STATIC_INIT(CANONICAL_AVP_VAL);

db_con_t* con = 0;
db_func_t db;

static domain_get_did_t dm_get_did = NULL;

/* default did value */
str default_did	= STR_STATIC_INIT("_default");

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"lookup_user", lookup_user,   1, header_fixup,      REQUEST_ROUTE | FAILURE_ROUTE},
	{"lookup_user", lookup_user_2, 2, lookup_user_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"check_uri",   check_uri,     1, header_fixup,      REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",          PARAM_STR, &db_url       },
	{"uri_table",       PARAM_STR, &uri_table    },
	{"uid_column",      PARAM_STR, &uid_col      },
	{"did_column",      PARAM_STR, &did_col      },
	{"username_column", PARAM_STR, &username_col },
	{"flags_column",    PARAM_STR, &flags_col    },
	{"scheme_column",   PARAM_STR, &scheme_col   },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"uri_db",
	cmds,      /* Exported functions */
	0,         /* RPC methods */
	params,    /* Exported parameters */
	mod_init,  /* module initialization function */
	0,         /* response function */
	destroy,   /* destroy function */
	0,         /* oncancel function */
	child_init /* child initialization function */
};


/*
 * Module initialization function callee in each child separately
 */
static int child_init(int rank)
{
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main or tcp_main processes */
	con = db.init(db_url.s);
	if (con == 0) {
		LOG(L_ERR, "uri_db:child_init: Unable to connect to the database\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


/*
 * Module initialization function that is called before the main process forks
 */
static int mod_init(void)
{
	int ver;

	if (bind_dbmod(db_url.s, &db) < 0) {
		LOG(L_ERR, "uri_db:mod_init: Unable to bind to the database module\n");
		return -1;
	}

	if (!DB_CAPABILITY(db, DB_CAP_QUERY)) {
		LOG(L_ERR, "uri_db:mod_init: Database module does not implement 'query' function\n");
		return -1;
	}

	if (db.init == 0) {
		LOG(L_CRIT, "uri_db:mod_init: Broken database driver\n");
		return -1;
	}

        con = db.init(db_url.s);
	if (con == 0) {
		LOG(L_ERR, "uri_db:mod_init: Unable to open database connection\n");
		return -1;
	}
	ver = table_version(&db, con, &uri_table);
	db.close(con);
	con = 0;

	if (ver < 0) {
		LOG(L_ERR, "uri_db:mod_init:"
		    " Error while querying table version\n");
		goto err;
	} else if (ver < URI_TABLE_VERSION) {
		LOG(L_ERR, "uri_db:mod_init: Invalid table version"
		    " of uri table (use ser_mysql.sh reinstall)\n");
		goto err;
	}
	return 0;

 err:
	return -1;
}


static void destroy(void)
{
	if (con && db.close) {
		db.close(con);
		con = 0;
	}
}

/*
 * Lookup UID from uri table. If store parameter is non-zero then
 * store the UID in an attribute.
 * The function returns 1 if UID was found, -1 if not or on an error
 */
static int lookup_uid(struct sip_msg* msg, long id, int store)
{
	struct to_body* from, *to;
	struct sip_uri puri;
	str did, uid;
	db_key_t keys[3], cols[2];
	db_val_t vals[3], *val;
	db_res_t* res;
	int flag, i, ret;

	int_str avp_name, avp_val;
	flag=0; /*warning fix*/

	keys[0] = username_col.s;
	keys[1] = did_col.s;
	keys[2] = scheme_col.s;
	cols[0] = uid_col.s;
	cols[1] = flags_col.s;

	vals[0].type = DB_STR;
	vals[0].nul = 0;

	vals[2].type = DB_STR;
	vals[2].nul = 0;

	did.s = 0; did.len = 0;

	if (id == USE_FROM) {
		get_from_did(&did, msg);
		flag = DB_IS_FROM;

		if (parse_from_header(msg) < 0) {
			LOG(L_ERR, "uri_db:lookup_uid: Error while parsing From header\n");
			return -1;
		}
		from = get_from(msg);
		if (!from) {
			LOG(L_ERR, "uri_db:lookup_uid: Unable to get From username\n");
			return -1;
		}
		if (parse_uri(from->uri.s, from->uri.len, &puri) < 0) {
			LOG(L_ERR, "uri_db:lookup_uid: Error while parsing From URI\n");
			return -1;
		}
		vals[0].val.str_val = puri.user;
		uri_type_to_str(puri.type, &(vals[2].val.str_val));
	} else if (id == USE_TO) {
		get_to_did(&did, msg);
		if (!msg->to) {
			if (parse_headers( msg, HDR_TO_F, 0 )==-1) {
				ERR("unable to parse To header\n");
				return -1;
			}
		}
		to = get_to(msg);
		if (!to) {
			LOG(L_ERR, "uri_db:lookup_uid: Unable to get To username\n");
			return -1;
		}
		if (parse_uri(to->uri.s, to->uri.len, &puri) < 0) {
			LOG(L_ERR, "uri_db:lookup_uid: Error while parsing To URI\n");
			return -1;
		}
		vals[0].val.str_val = puri.user;
		uri_type_to_str(puri.type, &(vals[2].val.str_val));
		flag = DB_IS_TO;
	} else {
		get_to_did(&did, msg);
		flag = DB_IS_TO;

		if (parse_sip_msg_uri(msg) < 0) return -1;
		vals[0].val.str_val = msg->parsed_uri.user;
		uri_type_to_str(msg->parsed_uri.type, &(vals[2].val.str_val));
	}

	vals[1].type = DB_STR;
	vals[1].nul = 0;
	if (did.s && did.len) {
		vals[1].val.str_val = did;
	} else {
		LOG(L_DBG, "uri_db:lookup_uid: DID not found, using default value\n");
		vals[1].val.str_val = default_did;
	}

	if (db.use_table(con, uri_table.s) < 0) {
		LOG(L_ERR, "uri_db:lookup_uid: Error in use_table\n");
		return -1;
	}

	if (db.query(con, keys, 0, vals, cols, 3, 2, 0, &res) < 0) {
		LOG(L_ERR, "uri_db:lookup_uid: Error in db_query\n");
		return -1;
	}

	for(i = 0; i < res->n; i++) {
		val = res->rows[i].values;

		if (val[0].nul || val[1].nul) {
			LOG(L_ERR, "uri_db:lookup_uid: Bogus line in %s table\n", uri_table.s);
			continue;
		}

		if ((val[1].val.int_val & DB_DISABLED)) continue; /* Skip disabled entries */
		if ((val[1].val.int_val & DB_LOAD_SER) == 0) continue; /* Not for SER */
		if ((val[1].val.int_val & flag) == 0) continue;        /* Not allowed in the header we are interested in */
		goto found;
	}
	ret = -1; /* Not found -> not allowed */
	goto freeres;
 found:
	if (store) {
		uid.s = (char*)val[0].val.string_val;
		uid.len = strlen(uid.s);
		if (id == USE_FROM) {
			set_from_uid(&uid);
		} else {
			set_to_uid(&uid);
			if (id == USE_RURI) {
				     /* store as str|int avp if alias is canonical or not (1,0) */
				if ((val[1].val.int_val & DB_CANON) != 0) {
					avp_name.s = canonical_avp;
					avp_val.s = canonical_avp_val;
					add_avp(AVP_CLASS_USER | AVP_TRACK_TO | AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val);
				}
			}
		}
	}
	ret = 1;

 freeres:
	db.free_result(con, res);
	return ret;
}


static int check_uri(struct sip_msg* msg, char* s1, char* s2)
{
	return lookup_uid(msg, (long)s1, 0);
}



static int lookup_user(struct sip_msg* msg, char* s1, char* s2)
{
	return lookup_uid(msg, (long)s1, 1);
}


static int lookup_user_2(struct sip_msg* msg, char* attr, char* select)
{
    db_key_t keys[3], cols[2];
    db_val_t vals[3], *val;
    db_res_t* res;
    str uri, did, uid;
    struct sip_uri puri;
    avp_ident_t* avp;
    int_str avp_val;
    int i, flag, ret;

    avp = &((fparam_t*)attr)->v.avp;

    if (!avp) {
	ERR("lookup_user: Invalid parameter 1, attribute name expected\n");
	return -1;
    }

    if (avp->flags & AVP_TRACK_TO) {
	flag = DB_IS_TO;
    } else {
	flag = DB_IS_FROM;
    }

    if (get_str_fparam(&uri, msg, (fparam_t*)select) != 0) {
	ERR("lookup_user: Unable to get SIP URI from %s\n", ((fparam_t*)select)->orig);
	return -1;
    }

    if (parse_uri(uri.s, uri.len, &puri) < 0) {
	ERR("Error while parsing URI '%.*s'\n", uri.len, ZSW(uri.s));
	return -1;
    }

    if (puri.host.len) {
	/* domain name is present */
	if (dm_get_did(&did, &puri.host) < 0) {
		DBG("Cannot lookup DID for domain '%.*s', using default value\n", puri.host.len, ZSW(puri.host.s));
		did = default_did;
	}
    } else {
	/* domain name is missing -- can be caused by Tel: URI */
	DBG("There is no domain name, using default value\n");
	did = default_did;
    }

    keys[0] = username_col.s;
    keys[1] = did_col.s;
    keys[2] = scheme_col.s;
    cols[0] = uid_col.s;
    cols[1] = flags_col.s;
    
    vals[0].type = DB_STR;
    vals[0].nul = 0;
    vals[0].val.str_val = puri.user;

    vals[1].type = DB_STR;
    vals[1].nul = 0;
    vals[1].val.str_val = did;

    vals[2].type = DB_STR;
    vals[2].nul = 0;
    uri_type_to_str(puri.type, &(vals[2].val.str_val));

    if (db.use_table(con, uri_table.s) < 0) {
	LOG(L_ERR, "lookup_user: Error in use_table\n");
	return -1;
    }

    if (db.query(con, keys, 0, vals, cols, 3, 2, 0, &res) < 0) {
	LOG(L_ERR, "lookup_user: Error in db_query\n");
	return -1;
    }

    for(i = 0; i < res->n; i++) {
	val = res->rows[i].values;
	
	if (val[0].nul || val[1].nul) {
	    LOG(L_ERR, "lookup_user: Bogus line in %s table\n", uri_table.s);
	    continue;
	}
	
	if ((val[1].val.int_val & DB_DISABLED)) continue; /* Skip disabled entries */
	if ((val[1].val.int_val & DB_LOAD_SER) == 0) continue; /* Not for SER */
	if ((val[1].val.int_val & flag) == 0) continue;        /* Not allowed in the header we are interested in */
	goto found;
    }

    DBG("lookup_user: UID not found for '%.*s'\n", uri.len, ZSW(uri.s));
    ret = -1;
    goto freeres;

 found:
    uid.s = (char*)val[0].val.string_val;
    uid.len = strlen(uid.s);
    avp_val.s = uid;

    if (add_avp(avp->flags | AVP_VAL_STR, avp->name, avp_val) < 0) {
	ERR("lookup_user: Error while creating attribute\n");
	ret = -1;
    } else {
	ret = 1;
    }

 freeres:
    db.free_result(con, res);
    return ret;
}



static int header_fixup(void** param, int param_no)
{
	long id = 0;

	if (param_no == 1) {
		if (!strcasecmp(*param, "Request-URI")) {
			id = USE_RURI;
		} else if (!strcasecmp(*param, "From")) {
			id = USE_FROM;
		} else if (!strcasecmp(*param, "To")) {
			id = USE_TO;
		} else {
			LOG(L_ERR, "uri_db:header_fixup Unknown parameter\n");
			return -1;
		}
	}

	pkg_free(*param);
	*param=(void*)id;
	return 0;
}


static int lookup_user_fixup(void** param, int param_no)
{
    int ret;
    
    if (param_no == 1) {
	if ((ret = fix_param(FPARAM_AVP, param)) != 0) {
	    ERR("lookup_user: Invalid parameter 1, attribute expected\n");
	    return -1;
	}
	dm_get_did = (domain_get_did_t)find_export("get_did", 0, 0);
	if (!dm_get_did) {
	    ERR("lookup_user: Could not find domain module\n");
	    return -1;
	}
	return 0;
    } else {
	return fixup_var_str_12(param, 2);
    }
}
