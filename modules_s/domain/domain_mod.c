/*
 * $Id$
 *
 * Domain module
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-06: db connection closed in mod_init (janakj)
 * 2004-06-06  updated to the new DB api, cleanup: static dbf & handler,
 *              calls to domain_db_{bind,init,close,ver} (andrei)
 */


#include "domain_mod.h"
#include <stdio.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../usr_avp.h"
#include "domain_rpc.h"
#include "hash.h"
#include "domain.h"


/*
 * Module management function prototypes
 */
static int mod_init(void);
static void destroy(void);
static int child_init(int rank);

static int is_local(struct sip_msg* msg, char* s1, char* s2);
static int lookup_domain(struct sip_msg* msg, char* s1, char* s2);
static int get_did(str* did, str* domain);

static int lookup_domain_fixup(void** param, int param_no);

MODULE_VERSION

/*
 * Version of domain table required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define DOMAIN_TABLE_VERSION 2
#define DOMATTR_TABLE_VERSION 1

#define DOMAIN_TABLE  "domain"
#define DOMAIN_COL    "domain"
#define DID_COL       "did"
#define FLAGS_COL     "flags"

#define DOMATTR_TABLE "domain_attrs"
#define DOMATTR_DID   "did"
#define DOMATTR_NAME  "name"
#define DOMATTR_TYPE  "type"
#define DOMATTR_VALUE "value"
#define DOMATTR_FLAGS "flags"
#define DOMAIN_COL    "domain"

int db_mode = 1;  /* Enable/disable domain cache */

/*
 * Module parameter variables
 */
static str db_url = STR_STATIC_INIT(DEFAULT_RODB_URL);

str domain_table = STR_STATIC_INIT(DOMAIN_TABLE); /* Name of domain table */
str domain_col   = STR_STATIC_INIT(DOMAIN_COL);   /* Name of domain column */
str did_col      = STR_STATIC_INIT(DID_COL);      /* Domain id */
str flags_col    = STR_STATIC_INIT(FLAGS_COL);    /* Domain flags */

str domattr_table = STR_STATIC_INIT(DOMATTR_TABLE);
str domattr_did   = STR_STATIC_INIT(DOMATTR_DID);
str domattr_name  = STR_STATIC_INIT(DOMATTR_NAME);
str domattr_type  = STR_STATIC_INIT(DOMATTR_TYPE);
str domattr_value = STR_STATIC_INIT(DOMATTR_VALUE);
str domattr_flags = STR_STATIC_INIT(DOMATTR_FLAGS);

int load_domain_attrs = 0;  /* Load attributes for each domain by default */

db_con_t* con = 0;
db_func_t db;

struct hash_entry*** active_hash = 0; /* Pointer to current hash table */
struct hash_entry** hash_1 = 0;       /* Pointer to hash table 1 */
struct hash_entry** hash_2 = 0;       /* Pointer to hash table 2 */

domain_t** domains_1 = 0;    /* List of domains 1 */
domain_t** domains_2 = 0;    /* List of domains 2 */

/* Global domain structure, this one is used to store data retrieved from
 * database when memory cache is disabled. There is one buffer for from
 * and one buffer for to track.
 */
static domain_t dom_buf[2];

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"is_local",      is_local,              1, fixup_var_str_1,     REQUEST_ROUTE|FAILURE_ROUTE },
    {"lookup_domain", lookup_domain,         2, lookup_domain_fixup, REQUEST_ROUTE|FAILURE_ROUTE },
    {"get_did",       (cmd_function)get_did, 0, 0,                   0},
    {0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"db_url",	          PARAM_STR, &db_url           },
    {"db_mode",           PARAM_INT, &db_mode          },
    {"domain_table",      PARAM_STR, &domain_table     },
    {"domain_col",        PARAM_STR, &domain_col       },
    {"did_col",           PARAM_STR, &did_col          },
    {"flags_col",         PARAM_STR, &flags_col        },
    {"domattr_table",     PARAM_STR, &domattr_table    },
    {"domattr_did",       PARAM_STR, &domattr_did      },
    {"domattr_name",      PARAM_STR, &domattr_name     },
    {"domattr_type",      PARAM_STR, &domattr_type     },
    {"domattr_value",     PARAM_STR, &domattr_value    },
    {"domattr_flags",     PARAM_STR, &domattr_flags    },
    {"load_domain_attrs", PARAM_INT, &load_domain_attrs},
    {0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
    "domain",
    cmds,       /* Exported functions */
    domain_rpc, /* RPC methods */
    params,     /* Exported parameters */
    mod_init,   /* module initialization function */
    0,          /* response function*/
    destroy,    /* destroy function */
    0,          /* cancel function */
    child_init  /* per-child init function */
};


static int connect_db(void)
{
    if (db.init == 0) {
	ERR("BUG: No database module found\n");
	return -1;
    }
    
    con = db.init(db_url.s);
    if (con == 0){
	ERR("Unable to connect database %s", db_url.s);
	return -1;
    }
    return 0;
}


static void disconnect_db(void)
{
    if (con && db.close){
	db.close(con);
	con = 0;
    }
}


/*
 * Check version of domain and domain_attrs tables
 */
static int check_version(void)
{
    int ver;
    
	 /* Check table version */
    ver = table_version(&db, con, &domain_table);
    if (ver < 0) {
	ERR("Error while querying table version\n");
	return -1;
    } else if (ver < DOMAIN_TABLE_VERSION) {
	ERR("Invalid table version, update your database schema\n");
	return -1;
    }
    
    ver = table_version(&db, con, &domattr_table);
    if (ver < 0) {
	ERR("Error while querying table version\n");
	return -1;
    } else if (ver < DOMATTR_TABLE_VERSION) {
	ERR("Invalid table version, update your database schema\n");
	return -1;
    }
    return 0;
}

static int allocate_tables(void)
{

    active_hash = (struct hash_entry***)shm_malloc(sizeof(struct hash_entry**));
    hash_1 = (struct hash_entry**)shm_malloc(sizeof(struct hash_entry*) * HASH_SIZE);
    hash_2 = (struct hash_entry**)shm_malloc(sizeof(struct hash_entry*) * HASH_SIZE);
    domains_1 = (domain_t**)shm_malloc(sizeof(domain_t*));
    domains_2 = (domain_t**)shm_malloc(sizeof(domain_t*));
    
    if (!hash_1 || !hash_2 || !active_hash || !domains_1 || !domains_2) {
	LOG(L_ERR, "ERROR:domain:allocate_tables: No memory left\n");
	return -1;
    }
    memset(hash_1, 0, sizeof(struct hash_entry*) * HASH_SIZE);
    memset(hash_2, 0, sizeof(struct hash_entry*) * HASH_SIZE);
    *active_hash = hash_1;
    *domains_1 = 0;
    *domains_2 = 0;
    return 0;
}

static void destroy_tables(void)
{
    free_table(hash_1);
    free_table(hash_2);
    if (active_hash) shm_free(active_hash);
    
    if (domains_1) {
	free_domain_list(*domains_1);
	shm_free(domains_1);
    }
    
    if (domains_2) {
	free_domain_list(*domains_2);
	shm_free(domains_2);
    }
}


static int mod_init(void)
{
    if (bind_dbmod(db_url.s, &db )) {
	LOG(L_CRIT, "Cannot bind to database module! "
	     "Did you forget to load a database module ?\n");
	return -1;
    }
    
	 /* Check if cache needs to be loaded from domain table */
    if (db_mode) {
	if (connect_db() < 0) goto error;
	if (check_version() < 0) goto error;
	if (allocate_tables() < 0) goto error;
	if (reload_domain_list() < 0) goto error;
	disconnect_db();
    }
    
    return 0;
    
 error:
    disconnect_db();
    return -1;
}


static int child_init(int rank)
{
	 /* Check if database is needed by child */
    if (rank > 0 || rank == PROC_RPC || rank == PROC_UNIXSOCK) {
	if (connect_db() < 0) return -1;
    }
    return 0;
}


static void free_old_domain(domain_t* d)
{
    int i;
    if (!d) return;
    if (d->did.s) {
	pkg_free(d->did.s);
	d->did.s = NULL;
    }
    
    if (d->domain) {
	for(i = 0; i < d->n; i++) {
	    if (d->domain[i].s) pkg_free(d->domain[i].s);
	}
	pkg_free(d->domain);
	d->domain = NULL;
    }
    
    if (d->flags) {
	pkg_free(d->flags);
	d->flags = NULL;
    }

    if (d->attrs) {
	destroy_avp_list(&d->attrs);
    }
}


static void destroy(void)
{
	 /* Destroy is called from the main process only,
	  * there is no need to close database here because
	  * it is closed in mod_init already
	  */
    if (!db_mode) {
	free_old_domain(&dom_buf[0]);
	free_old_domain(&dom_buf[1]);
    }
    destroy_tables();
}


/*
 * Retrieve did directly from database, without using
 * memory cache. Use 0 as the value of first parameter
 * if you only want to know whether the entry is in the
 * database. The function returns 1 if there is such
 * entry, 0 if not, and -1 on error.
 * The result is allocated using pkg_malloc and must be
 * freed.
 */
static int db_get_did(str* did, str* domain)
{
    db_key_t keys[1], cols[2];
    db_val_t vals[1], *val;
    db_res_t* res;
    str t;
    
    keys[0] = domain_col.s;
    cols[0] = did_col.s;
    cols[1] = flags_col.s;
    res = 0;
    
    if (!domain) {
	ERR("BUG:Invalid parameter value\n");
	goto err;
    }
    
    if (db.use_table(con, domain_table.s) < 0) {
	ERR("Error while trying to use domain table\n");
	goto err;
    }
    
    vals[0].type = DB_STR;
    vals[0].nul = 0;
    vals[0].val.str_val = *domain;
    
    if (db.query(con, keys, 0, vals, cols, 1, 2, 0, &res) < 0) {
	ERR("Error while querying database\n");
	goto err;
    }
    
    if (res->n > 0) {
	val = res->rows[0].values;
	
	     /* Test flags first, we are only interested in rows
	      * that are not disabled
	      */
	if (val[1].nul || (val[1].val.int_val & DB_DISABLED)) {
	    db.free_result(con, res);
	    return 0;
	}
	
	if (did) {
	    if (val[0].nul) {
		did->len = 0;
		did->s = 0;
		WARN("Domain '%.*s' has NULL did\n", domain->len, ZSW(domain->s));
	    } else {
		t.s = (char*)val[0].val.string_val;
		t.len = strlen(t.s);
		did->s = pkg_malloc(t.len);
		if (!did->s) {
		    ERR("No memory left\n");
		    goto err;
		}
		memcpy(did->s, t.s, t.len);
		did->len = t.len;
	    }
	}

	db.free_result(con, res);
	return 1;
    } else {
	db.free_result(con, res);
	return 0;
    }

 err:
    if (res) {
	db.free_result(con, res);
	res = 0;
    }
    return -1;
}


/*
 * Check if domain is local
 */
static int is_local(struct sip_msg* msg, char* fp, char* s2)
{
    str domain, tmp;

    if (get_str_fparam(&domain, msg, (fparam_t*)fp) != 0) {
	ERR("Unable to get domain to check\n");
	return -1;
    }

    tmp.s = pkg_malloc(domain.len);
    if (!tmp.s) {
	ERR("No memory left\n");
	return -1;
    }
    memcpy(tmp.s, domain.s, domain.len);
    tmp.len = domain.len;
    strlower(&tmp);
    
    if (!db_mode) {
	switch(db_get_did(0, &tmp)) {
	case 1:  goto found;
	default: goto not_found;
	}
    } else {
	if (hash_lookup(0, *active_hash, &tmp) == 1) goto found;
	else goto not_found;
    }

 found:
    pkg_free(tmp.s);
    return 1;
 not_found:
    pkg_free(tmp.s);
    return -1;
}


static int db_load_domain(domain_t** d, unsigned long flags, str* domain)
{
    int ret;
    int_str name, val;
    domain_t* p;
    str name_s = STR_STATIC_INIT(AVP_DID);

    if (flags & AVP_TRACK_FROM) {
	p = &dom_buf[0];
    } else {
	p = &dom_buf[1];
    }

    free_old_domain(p);

    ret = db_get_did(&p->did, domain);
    if (ret != 1) return ret;
    if (load_domain_attrs) {
	if (db_load_domain_attrs(p) < 0) return -1;
    }

	 /* Create an attribute containing did of the domain */
    name.s = name_s;
    val.s = p->did;
    if (add_avp_list(&p->attrs, AVP_CLASS_DOMAIN | AVP_NAME_STR | AVP_VAL_STR, name, val) < 0) return -1;

    *d = p;
    return 0;
}


static int lookup_domain(struct sip_msg* msg, char* flags, char* fp)
{
    str domain, tmp;
    domain_t* d;
    unsigned int track;
    int ret = -1;
    
    track = 0;
    
    if (get_str_fparam(&domain, msg, (fparam_t*)fp) != 0) {
	ERR("Cannot get domain name to lookup\n");
	return -1;
    }
    
    tmp.s = pkg_malloc(domain.len);
    if (!tmp.s) {
	ERR("No memory left\n");
	return -1;
    }
    memcpy(tmp.s, domain.s, domain.len);
    tmp.len = domain.len;
    strlower(&tmp);

    if (db_mode) {
	if (hash_lookup(&d, *active_hash, &tmp) == 1) {
	    set_avp_list((unsigned long)flags, &d->attrs);
	    ret = 1;
	}
    } else {
	if (db_load_domain(&d, (unsigned long)flags, &tmp) == 0) {
	    set_avp_list((unsigned long)flags, &d->attrs);
	    ret = 1;
	}
    }

    pkg_free(tmp.s);
    return ret;
}


static int get_did(str* did, str* domain)
{
    str tmp;
    domain_t* d;
    unsigned int track;
    
    track = 0;
    
    if (!db_mode) {
	ERR("lookup_domain only works in cache mode\n");
	return -1;
    }
    
    tmp.s = pkg_malloc(domain->len);
    if (!tmp.s) {
	ERR("No memory left\n");
	return -1;
    }
    memcpy(tmp.s, domain->s, domain->len);
    tmp.len = domain->len;
    strlower(&tmp);
    
    if (hash_lookup(&d, *active_hash, &tmp) == 1) {
	*did = d->did;
	pkg_free(tmp.s);
	return 1;
    } else {
	pkg_free(tmp.s);
	return -1;
    }
}


int reload_domain_list(void)
{
    struct hash_entry** new_table;
    domain_t** new_list;
    
	 /* Choose new hash table and free its old contents */
    if (*active_hash == hash_1) {
	free_table(hash_2);
	new_table = hash_2;
	new_list = domains_2;
    } else {
	free_table(hash_1);
	new_table = hash_1;
	new_list = domains_1;
    }
    
    if (load_domains(new_list) < 0) goto error;
    if (gen_domain_table(new_table, *new_list) < 0) goto error;
    *active_hash = new_table;
    return 0;
    
 error:
    free_table(new_table);
    free_domain_list(*new_list);
    return -1;
}


static int lookup_domain_fixup(void** param, int param_no)
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
	case 0x4644: /* $fd */
	case 0x6664:
	case 0x4664:
	case 0x6644:
	    flags = AVP_TRACK_FROM | AVP_CLASS_DOMAIN;
	    break;
	    
	case 0x5444: /* $td */
	case 0x7464:
	case 0x5464:
	case 0x7444:
	    flags = AVP_TRACK_TO | AVP_CLASS_DOMAIN;
	    break;
	    
	default:
	    ERR("Invalid parameter value: '%s'\n", s);
	    return -1;
	}
	
	pkg_free(*param);
	*param = (void*)flags;
    } else if (param_no == 2) {
	return fixup_var_str_12(param, 2);
    }
    
    return 0;
}
