/*
 * Domain module
 *
 * Copyright (C) 2002-2012 Juha Heinanen
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "domain_mod.h"
#include <stdio.h>
#include "../../core/mem/shm_mem.h"
#include "../../core/mem/mem.h"
#include "../../core/sr_module.h"
#include "../../core/pvar.h"
#include "../../core/forward.h"
#include "../../core/mod_fix.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/locking.h"
#include "../../core/kemi.h"
#include "domain.h"
#include "hash.h"
#include "api.h"

/*
 * Module management function prototypes
 */
static int mod_init(void);
static void destroy(void);
static int child_init(int rank);

MODULE_VERSION

/*
 * Version of domain table required by the module,
 * increment this value if you change the table in
 * a backwards incompatible way
 */
#define DOMAIN_TABLE_VERSION 2
#define DOMAIN_ATTRS_TABLE_VERSION 1

#define DOMAIN_TABLE "domain"
#define DOMAIN_ATTRS_TABLE "domain_attrs"
#define DID_COL "did"
#define DOMAIN_COL "domain"
#define NAME_COL "name"
#define TYPE_COL "type"
#define VALUE_COL "value"

static int domain_init_rpc(void);

/*
 * Module parameter variables
 */
str d_db_url = str_init(DEFAULT_RODB_URL);
str domain_table = str_init(DOMAIN_TABLE); /* Name of domain table */
str domain_attrs_table = str_init(DOMAIN_ATTRS_TABLE);
str did_col = str_init(DID_COL);	   /* Name of domain id column */
str domain_col = str_init(DOMAIN_COL); /* Name of domain column */
str name_col = str_init(NAME_COL);	 /* Name of attribute name column */
str type_col = str_init(TYPE_COL);	 /* Name of attribute type column */
str value_col = str_init(VALUE_COL);   /* Name of attribute value column */
int domain_reg_myself = 0;

/*
 * Other module variables
 */
struct domain_list ***hash_table =
		0; /* Pointer to current hash table pointer */
struct domain_list **hash_table_1 = 0; /* Pointer to hash table 1 */
struct domain_list **hash_table_2 = 0; /* Pointer to hash table 2 */
gen_lock_t *reload_lock;

/*
 * Exported functions
 */
/* clang-format off */
static cmd_export_t cmds[] = {
	{"is_from_local", (cmd_function)is_from_local, 0, 0, 0,
		REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"is_uri_host_local", (cmd_function)is_uri_host_local, 0, 0, 0,
		REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"is_domain_local", (cmd_function)w_is_domain_local, 1, fixup_spve_null,
		fixup_free_spve_null,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"lookup_domain", (cmd_function)w_lookup_domain_no_prefix, 1,
		fixup_spve_null, fixup_free_spve_null,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"lookup_domain", (cmd_function)w_lookup_domain, 2, fixup_spve_spve,
		fixup_free_spve_spve,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"bind_domain", (cmd_function)bind_domain, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};
/* clang-format on */

/*
 * Exported parameters
 */
/* clang-format off */
static param_export_t params[] = {
	{"db_url",         PARAM_STR, &d_db_url      },
	{"domain_table",   PARAM_STR, &domain_table},
	{"domain_attrs_table",   PARAM_STR, &domain_attrs_table},
	{"did_col",        PARAM_STR, &did_col  },
	{"domain_col",     PARAM_STR, &domain_col  },
	{"name_col",       PARAM_STR, &name_col  },
	{"type_col",       PARAM_STR, &type_col  },
	{"value_col",      PARAM_STR, &value_col  },
	{"register_myself",INT_PARAM, &domain_reg_myself},
	{0, 0, 0}
};
/* clang-format on */

/*
 * Module interface
 */
/* clang-format off */
struct module_exports exports = {
	"domain",			/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	child_init,			/* per-child init function */
	destroy				/* module destroy function */
};
/* clang-format on */

static int mod_init(void)
{
	LM_DBG("initializing\n");

	if(domain_init_rpc() != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(domain_reg_myself != 0) {
		if(register_check_self_func(domain_check_self) < 0) {
			LM_ERR("failed to register check self function\n");
			return -1;
		}
	}

	/* Bind database */
	if(domain_db_bind(&d_db_url)) {
		LM_DBG("Usign db_url [%.*s]\n", d_db_url.len, d_db_url.s);
		LM_ERR("no database module found. Have you configure"
			   " the \"db_url\" modparam properly?\n");
		return -1;
	}

	/* Check table versions */
	if(domain_db_init(&d_db_url) < 0) {
		LM_ERR("unable to open database connection\n");
		return -1;
	}
	if(domain_db_ver(&domain_table, DOMAIN_TABLE_VERSION) < 0) {
		DB_TABLE_VERSION_ERROR(domain_table);
		domain_db_close();
		goto error;
	}
	if(domain_db_ver(&domain_attrs_table, DOMAIN_ATTRS_TABLE_VERSION) < 0) {
		DB_TABLE_VERSION_ERROR(domain_attrs_table);
		domain_db_close();
		goto error;
	}
	domain_db_close();

	/* Initializing hash tables and hash table variable */
	hash_table =
			(struct domain_list ***)shm_malloc(sizeof(struct domain_list **));
	hash_table_1 = (struct domain_list **)shm_malloc(
			sizeof(struct domain_list *) * (DOM_HASH_SIZE + 1));
	hash_table_2 = (struct domain_list **)shm_malloc(
			sizeof(struct domain_list *) * (DOM_HASH_SIZE + 1));
	if((hash_table == 0) || (hash_table_1 == 0) || (hash_table_2 == 0)) {
		LM_ERR("no memory for hash table\n");
		goto error;
	}
	memset(hash_table_1, 0, sizeof(struct domain_list *) * (DOM_HASH_SIZE + 1));
	memset(hash_table_2, 0, sizeof(struct domain_list *) * (DOM_HASH_SIZE + 1));
	*hash_table = hash_table_1;

	/* Allocate and initialize locks */
	reload_lock = lock_alloc();
	if(reload_lock == NULL) {
		LM_ERR("cannot allocate reload_lock\n");
		goto error;
	}
	if(lock_init(reload_lock) == NULL) {
		LM_ERR("cannot init reload_lock\n");
		goto error;
	}

	/* First reload */
	lock_get(reload_lock);
	if(reload_tables() == -1) {
		lock_release(reload_lock);
		LM_CRIT("domain reload failed\n");
		goto error;
	}
	lock_release(reload_lock);

	return 0;

error:
	destroy();
	return -1;
}


static int child_init(int rank)
{
	return 0;
}


static void destroy(void)
{
	/* Destroy is called from the main process only,
	 * there is no need to close database here because
	 * it is closed in mod_init already
	 */
	if(hash_table) {
		shm_free(hash_table);
		hash_table = 0;
	}
	if(hash_table_1) {
		hash_table_free(hash_table_1);
		shm_free(hash_table_1);
		hash_table_1 = 0;
	}
	if(hash_table_2) {
		hash_table_free(hash_table_2);
		shm_free(hash_table_2);
		hash_table_2 = 0;
	}
}


static const char *domain_rpc_reload_doc[2] = {
		"Reload domain tables from database", 0};


/*
 * RPC command to reload domain table
 */
static void domain_rpc_reload(rpc_t *rpc, void *ctx)
{
	lock_get(reload_lock);
	if(reload_tables() < 0) {
		rpc->fault(ctx, 400, "Reload of domain tables failed");
	}
	lock_release(reload_lock);
}


static const char *domain_rpc_dump_doc[2] = {
		"Return the contents of domain and domain_attrs tables", 0};


/*
 * Fifo function to print domains from current hash table
 */
static void domain_rpc_dump(rpc_t *rpc, void *ctx)
{
	int i;
	struct domain_list *np;
	struct attr_list *ap;
	struct domain_list **ht;
	void *rt;
	void *at;
	void *st;

	if(hash_table == 0 || *hash_table == 0) {
		rpc->fault(ctx, 404, "Server Domain Cache Empty");
		return;
	}
	if(rpc->add(ctx, "{", &rt) < 0) {
		rpc->fault(ctx, 500, "Failed to create root struct");
		return;
	}
	if(rpc->struct_add(rt, "[", "domains", &at) < 0) {
		rpc->fault(ctx, 500, "Failed to create domains struct");
		return;
	}

	ht = *hash_table;
	for(i = 0; i < DOM_HASH_SIZE; i++) {
		np = ht[i];
		while(np) {
			if(rpc->array_add(at, "{", &st) < 0)
				return;
			rpc->struct_add(st, "SS", "domain", &np->domain, "did", &np->did);
			np = np->next;
		}
	}
	if(rpc->struct_add(rt, "[", "attributes", &at) < 0) {
		rpc->fault(ctx, 500, "Failed to create attributes struct");
		return;
	}
	np = ht[DOM_HASH_SIZE];
	while(np) {
		if(rpc->array_add(at, "{", &st) < 0)
			return;
		rpc->struct_add(st, "S", "did", &np->did);
		rpc->struct_add(st, "[", "attrs", &st);
		ap = np->attrs;
		while(ap) {
			rpc->array_add(st, "S", &ap->name);
			ap = ap->next;
		}
		np = np->next;
	}

	return;
}


rpc_export_t domain_rpc_list[] = {
		{"domain.reload", domain_rpc_reload, domain_rpc_reload_doc, 0},
		{"domain.dump", domain_rpc_dump, domain_rpc_dump_doc, 0},
		{0, 0, 0, 0}
};

static int domain_init_rpc(void)
{
	if(rpc_register_array(domain_rpc_list) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_domain_exports[] = {
	{ str_init("domain"), str_init("is_from_local"),
		SR_KEMIP_INT, ki_is_from_local,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("domain"), str_init("is_uri_host_local"),
		SR_KEMIP_INT, ki_is_uri_host_local,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("domain"), str_init("is_domain_local"),
		SR_KEMIP_INT, ki_is_domain_local,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("domain"), str_init("lookup_domain"),
		SR_KEMIP_INT, ki_lookup_domain,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("domain"), str_init("lookup_domain_prefix"),
		SR_KEMIP_INT, ki_lookup_domain_prefix,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_domain_exports);
	return 0;
}
