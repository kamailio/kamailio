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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../lib/srdb2/db.h"
#include "uid_uri_db_mod.h"
#include "../../usr_avp.h"
#include "../uid_domain/domain.h"

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
static int lookup_user(struct sip_msg* msg, char* s1, char* s2);
static int lookup_user_2(struct sip_msg* msg, char* s1, char* s2);
static int check_uri(struct sip_msg* msg, char* s1, char* s2);
static int header_fixup(void** param, int param_no);
static int lookup_user_fixup(void** param, int param_no);

#define URI_TABLE    "uid_uri"
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

static db_ctx_t* db = NULL;
static db_cmd_t* lookup_uid_cmd = NULL;

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
	"uid_uri_db",
	cmds,      /* Exported functions */
	0,         /* RPC methods */
	params,    /* Exported parameters */
	0,         /* module initialization function */
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
	db_fld_t lookup_uid_columns[] = {
		{.name = uid_col.s,   DB_STR},
		{.name = flags_col.s, DB_BITMAP},
		{.name = NULL}
	};

	db_fld_t lookup_uid_match[] = {
		{.name = username_col.s, DB_STR},
		{.name = did_col.s,      DB_STR},
		{.name = scheme_col.s,   DB_STR},
		{.name = NULL}
	};

	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main or tcp_main processes */

	db = db_ctx("uri_db");
	if (db == NULL) {
		ERR("Error while initializing database layer\n");
		return -1;
	}
	if (db_add_db(db, db_url.s) < 0) goto error;
	if (db_connect(db) < 0) goto error;
	
	lookup_uid_cmd = db_cmd(DB_GET, db, uri_table.s, lookup_uid_columns, lookup_uid_match, NULL);
	if (lookup_uid_cmd == NULL) {
		ERR("Error while building db query to load global attributes\n");
		goto error;
	}
	return 0;

error:
	if (lookup_uid_cmd) db_cmd_free(lookup_uid_cmd);
	if (db) db_ctx_free(db);
	return -1;
}


static void destroy(void)
{
	if (lookup_uid_cmd) db_cmd_free(lookup_uid_cmd);
	if (db) db_ctx_free(db);
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
	db_res_t* res;
	db_rec_t* rec;
	int flag, ret;

	int_str avp_name, avp_val;
	flag=0; /*warning fix*/

	did.s = 0; did.len = 0;

	if (id == USE_FROM) {
		get_from_did(&did, msg);
		flag = SRDB_IS_FROM;

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
		lookup_uid_cmd->match[0].v.lstr = puri.user;
		uri_type_to_str(puri.type, &(lookup_uid_cmd->match[2].v.lstr));
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
		lookup_uid_cmd->match[0].v.lstr = puri.user;
		uri_type_to_str(puri.type, &(lookup_uid_cmd->match[2].v.lstr));
		flag = SRDB_IS_TO;
	} else {
		get_to_did(&did, msg);
		flag = SRDB_IS_TO;

		if (parse_sip_msg_uri(msg) < 0) return -1;
		lookup_uid_cmd->match[0].v.lstr = msg->parsed_uri.user;
		uri_type_to_str(msg->parsed_uri.type, &(lookup_uid_cmd->match[2].v.lstr));
	}

	if (did.s && did.len) {
		lookup_uid_cmd->match[1].v.lstr = did;
	} else {
		LOG(L_DBG, "uri_db:lookup_uid: DID not found, using default value\n");
		lookup_uid_cmd->match[1].v.lstr = default_did;
	}

	if (db_exec(&res, lookup_uid_cmd) < 0) {
		LOG(L_ERR, "uri_db:lookup_uid: Error while executing database query\n");
		return -1;
	}

	rec = db_first(res);
	while(rec) {
		if (rec->fld[0].flags & DB_NULL ||
			rec->fld[1].flags & DB_NULL) {
			LOG(L_ERR, "uri_db:lookup_uid: Bogus line in %s table\n", uri_table.s);
			goto skip;
		}

		if ((rec->fld[1].v.int4 & SRDB_DISABLED)) goto skip; /* Skip disabled entries */
		if ((rec->fld[1].v.int4 & SRDB_LOAD_SER) == 0) goto skip; /* Not for SER */
		if ((rec->fld[1].v.int4 & flag) == 0) goto skip;        /* Not allowed in the header we are interested in */
		goto found;

	skip:
		rec = db_next(res);
	}
	ret = -1; /* Not found -> not allowed */
	goto freeres;

 found:
	if (store) {
		uid = rec->fld[0].v.lstr;
		if (id == USE_FROM) {
			set_from_uid(&uid);
		} else {
			set_to_uid(&uid);
			if (id == USE_RURI) {
				     /* store as str|int avp if alias is canonical or not (1,0) */
				if ((rec->fld[1].v.int4 & SRDB_CANON) != 0) {
					avp_name.s = canonical_avp;
					avp_val.s = canonical_avp_val;
					add_avp(AVP_CLASS_USER | AVP_TRACK_TO | AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val);
				}
			}
		}
	}
	ret = 1;

 freeres:
	db_res_free(res);
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
    db_res_t* res;
	db_rec_t* rec;
    str uri, did, uid;
    struct sip_uri puri;
    avp_ident_t* avp;
    int_str avp_val;
    int flag, ret;

    avp = &((fparam_t*)attr)->v.avp;

    if (!avp) {
		ERR("lookup_user: Invalid parameter 1, attribute name expected\n");
		return -1;
    }

    if (avp->flags & AVP_TRACK_TO) {
		flag = SRDB_IS_TO;
    } else {
		flag = SRDB_IS_FROM;
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

    /* don't lookup users with empty username -- wasted DB time */
    if (puri.user.len==0) {
    	return -1;
    }

	lookup_uid_cmd->match[0].v.lstr = puri.user;
    lookup_uid_cmd->match[1].v.lstr = did;
    uri_type_to_str(puri.type, &(lookup_uid_cmd->match[2].v.lstr));

    if (db_exec(&res, lookup_uid_cmd) < 0) {
		LOG(L_ERR, "lookup_user: Error in db_query\n");
		return -1;
    }

	rec = db_first(res);
	while(rec) {
		if (rec->fld[0].flags & DB_NULL ||
			rec->fld[1].flags & DB_NULL) {
			LOG(L_ERR, "lookup_user: Bogus line in %s table\n", uri_table.s);
			goto skip;
		}
		
		if ((rec->fld[1].v.int4 & SRDB_DISABLED)) goto skip; /* Skip disabled entries */
		if ((rec->fld[1].v.int4 & SRDB_LOAD_SER) == 0) goto skip; /* Not for SER */
		if ((rec->fld[1].v.int4 & flag) == 0) goto skip;        /* Not allowed in the header we are interested in */
		goto found;

	skip:
		rec = db_next(res);
    }

    DBG("lookup_user: UID not found for '%.*s'\n", uri.len, ZSW(uri.s));
    ret = -1;
    goto freeres;

 found:
	uid = rec->fld[0].v.lstr;
    avp_val.s = uid;

    if (add_avp(avp->flags | AVP_VAL_STR, avp->name, avp_val) < 0) {
		ERR("lookup_user: Error while creating attribute\n");
		ret = -1;
    } else {
		ret = 1;
    }

 freeres:
    db_res_free(res);
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
    if (param_no == 1) {
		if (fix_param(FPARAM_AVP, param) != 0) {
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
