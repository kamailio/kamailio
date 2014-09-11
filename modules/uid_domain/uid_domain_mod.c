/*
 * Domain module
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version
 *
 * sip-router is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

#include "uid_domain_mod.h"
#include <stdio.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../usr_avp.h"
#include "domain_api.h"
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
 * Version of domain table required by the module, increment this value if you
 * change the table in an backwards incompatible way
 */
#define DOMAIN_TABLE_VERSION 2
#define DOMATTR_TABLE_VERSION 1

#define DOMAIN_TABLE  "uid_domain"
#define DOMAIN_COL    "domain"
#define DID_COL       "did"
#define FLAGS_COL     "flags"

#define DOMATTR_TABLE "uid_domain_attrs"
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

static db_ctx_t* db = NULL;
db_cmd_t* load_domains_cmd = NULL, *get_did_cmd = NULL, *load_attrs_cmd = NULL;

struct hash_entry*** active_hash = 0; /* Pointer to current hash table */
struct hash_entry** hash_1 = 0;       /* Pointer to hash table 1 */
struct hash_entry** hash_2 = 0;       /* Pointer to hash table 2 */

domain_t** domains_1 = 0;    /* List of domains 1 */
domain_t** domains_2 = 0;    /* List of domains 2 */

/* Global domain structure, this one is used to store data retrieved from
 * database when memory cache is disabled. There is one buffer for from and
 * one buffer for to track.
 */
static domain_t dom_buf[2];

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"is_local",      is_local,              1, fixup_var_str_1,
	 REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"lookup_domain", lookup_domain,         2, lookup_domain_fixup,
	 REQUEST_ROUTE|FAILURE_ROUTE },
	{"get_did",       (cmd_function)get_did, 0, 0, 0},
	{"bind_domain",   (cmd_function)bind_domain, 0, 0, 0},
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
	"uid_domain",
	cmds,       /* Exported functions */
	domain_rpc, /* RPC methods */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function*/
	destroy,    /* destroy function */
	0,          /* cancel function */
	child_init  /* per-child init function */
};


static int init_db(void)
{
	db_fld_t load_domains_columns[] = {
		{.name = did_col.s,    DB_STR},
		{.name = domain_col.s, DB_STR},
		{.name = flags_col.s,  DB_BITMAP},
		{.name = NULL}
	};
	db_fld_t get_did_columns[] = {
		{.name = did_col.s, DB_STR},
		{.name = NULL}
	};
	db_fld_t load_attrs_columns[] = {
		{.name = domattr_name.s, .type = DB_STR},
		{.name = domattr_type.s, .type = DB_INT},
		{.name = domattr_value.s, .type = DB_STR},
		{.name = domattr_flags.s, .type = DB_BITMAP},
		{.name = NULL}
	};
	db_fld_t get_did_match[] = {
		{.name = domain_col.s, DB_STR},
		{.name = NULL}
	};
	db_fld_t load_attrs_match[] = {
		{.name = domattr_did.s, .type = DB_STR},
		{.name = NULL}
	};

	db = db_ctx("domain");
	if (db == NULL) {
		ERR("Error while initializing database layer\n");
		return -1;
	}
	if (db_add_db(db, db_url.s) < 0) return -1;
	if (db_connect(db) < 0) return -1;

	DBG("prepare load_domains_cmd\n");
	load_domains_cmd = db_cmd(DB_GET, db, domain_table.s, load_domains_columns,
							  NULL, NULL);
	if (load_domains_cmd == NULL) {
		ERR("Error while preparing load_domains database command\n");
		return -1;
	}

	DBG("prepare get_did_cmd\n");
	get_did_cmd = db_cmd(DB_GET, db, domain_table.s, get_did_columns,
						 get_did_match, NULL);
	if (get_did_cmd == NULL) {
		ERR("Error while preparing get_did database command\n");
		return -1;
	}

	if (load_domain_attrs) {
		DBG("prepare load_attrs_cmd\n");
		load_attrs_cmd = db_cmd(DB_GET, db, domattr_table.s,
								load_attrs_columns, load_attrs_match, NULL);
		if (load_attrs_cmd == NULL) {
			ERR("Error while preparing load_attrs database command\n");
			return -1;
		}
	}

	return 0;
}


static int allocate_tables(void)
{
	active_hash = (struct hash_entry***)shm_malloc(sizeof(struct hash_entry**));
	hash_1 = (struct hash_entry**)shm_malloc(sizeof(struct hash_entry*)
											 * HASH_SIZE);
	hash_2 = (struct hash_entry**)shm_malloc(sizeof(struct hash_entry*)
											 * HASH_SIZE);
	domains_1 = (domain_t**)shm_malloc(sizeof(domain_t*));
	domains_2 = (domain_t**)shm_malloc(sizeof(domain_t*));

	if (!hash_1 || !hash_2 || !active_hash || !domains_1 || !domains_2) {
		ERR("No memory left\n");
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
	/* Check if cache needs to be loaded from domain table */
	if (db_mode) {
		if (init_db() < 0) goto error;

		if (allocate_tables() < 0) goto error;
		if (reload_domain_list() < 0) goto error;

		db_cmd_free(load_domains_cmd);
		load_domains_cmd = NULL;

		db_cmd_free(load_attrs_cmd);
		load_attrs_cmd = NULL;

		db_cmd_free(get_did_cmd);
		get_did_cmd = NULL;

		if (db) db_disconnect(db);
		db_ctx_free(db);
		db = NULL;
	}

	return 0;

 error:
	if (get_did_cmd) {
		db_cmd_free(get_did_cmd);
		get_did_cmd = NULL;
	}
	if (load_domains_cmd) {
		db_cmd_free(load_domains_cmd);
		load_domains_cmd = NULL;
	}
	if (load_attrs_cmd) {
		db_cmd_free(load_attrs_cmd);
		load_attrs_cmd = NULL;
	}
	if (db) db_disconnect(db);
	db_ctx_free(db);
	db = NULL;
	return -1;
}


static int child_init(int rank)
{
	/* Check if database is needed by child */
	if (rank > 0 || rank == PROC_RPC || rank == PROC_UNIXSOCK) {
		if (init_db() < 0) return -1;
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
	/* Destroy is called from the main process only, there is no need to close
	 * database here because it is closed in mod_init already
	 */
	if (!db_mode) {
		free_old_domain(&dom_buf[0]);
		free_old_domain(&dom_buf[1]);
	}

	if (load_domains_cmd) db_cmd_free(load_domains_cmd);
	if (get_did_cmd) db_cmd_free(get_did_cmd);
	if (load_attrs_cmd) db_cmd_free(load_attrs_cmd);

	if (db) {
		db_disconnect(db);
		db_ctx_free(db);
	}

	destroy_tables();
}



/*
 * Check if domain is local
 */
static int is_local(struct sip_msg* msg, char* fp, char* s2)
{
	str domain;

	if (get_str_fparam(&domain, msg, (fparam_t*)fp) != 0) {
		ERR("Unable to get domain to check\n");
		return -1;
	}

	return is_domain_local(&domain);
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
	if (add_avp_list(&p->attrs, AVP_CLASS_DOMAIN | AVP_NAME_STR | AVP_VAL_STR,
					 name, val) < 0) return -1;

	*d = p;
	return 0;
}


static int lookup_domain(struct sip_msg* msg, char* flags, char* fp)
{
	str domain, tmp;
	domain_t* d;
	int ret = -1;

	if (get_str_fparam(&domain, msg, (fparam_t*)fp) != 0) {
		DBG("lookup_domain: Cannot get the domain name to lookup\n");
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
