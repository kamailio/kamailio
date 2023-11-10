/*
 * Copyright (C) 2007 1&1 Internet AG
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
 */

/*!
 * \file
 * \brief USERBLOCKLIST :: module definitions
 * \ingroup userblocklist
 * - Module: \ref userblocklist
 */

/*!
 * \defgroup userblocklist USERBLOCKLIST :: The Kamailio userblocklist Module
 *
 * The userblocklist module allows Kamailio to handle blocklists on a per user basis.
 * This information is stored in a database table, which is queried to decide if the
 * number (more exactly, the request URI user) is blocklisted or not.
 * An additional functionality that this module provides is the ability to handle
 * global blocklists. This lists are loaded on startup into memory, thus providing a
 * better performance then in the userblocklist case.
 */

#include <string.h>

#include "../../core/parser/parse_uri.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/sr_module.h"
#include "../../core/mem/mem.h"
#include "../../core/usr_avp.h"
#include "../../core/locking.h"
#include "../../core/error.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "../../lib/trie/dtrie.h"
#include "db.h"
#include "db_userblocklist.h"

MODULE_VERSION


#define MAXNUMBERLEN 31

#define BLOCKLISTED_S "blocklisted"
#define BLOCKLISTED_LEN (sizeof(BLOCKLISTED_S) - 1)
#define ALLOWLISTED_S "allowlisted"
#define ALLOWLISTED_LEN (sizeof(ALLOWLISTED_S) - 1)
#define TRUE_S "true"
#define TRUE_LEN (sizeof(TRUE_S) - 1)
#define FALSE_S "false"
#define FALSE_LEN (sizeof(FALSE_S) - 1)

typedef struct _avp_check
{
	int avp_flags;
	int_str avp_name;
} avp_check_t;


struct check_blocklist_fs_t
{
	struct dtrie_node_t *dtrie_root;
};

str userblocklist_db_url = str_init(DEFAULT_RODB_URL);
int use_domain = 0;
int match_mode = 10; /* numeric */
static struct dtrie_node_t *gnode = NULL;

/* ---- fixup functions: */
static int check_blocklist_fixup(void **param, int param_no);
static int check_user_blocklist_fixup(void **param, int param_no);
static int check_globalblocklist_fixup(void **param, int param_no);

/* ---- exported commands: */
static int check_user_blocklist(sip_msg_t *msg, char *puser, char *pdomain,
		char *pnumber, char *ptable);
static int check_user_allowlist(sip_msg_t *msg, char *puser, char *pdomain,
		char *pnumber, char *ptable);
static int check_user_blocklist2(sip_msg_t *msg, char *puser, char *pdomain);
static int check_user_allowlist2(sip_msg_t *msg, char *puser, char *pdomain);
static int check_user_blocklist3(
		sip_msg_t *msg, char *puser, char *pdomain, char *pnumber);
static int check_user_allowlist3(
		sip_msg_t *msg, char *puser, char *pdomain, char *pnumber);
static int check_blocklist(sip_msg_t *msg, struct check_blocklist_fs_t *arg1);
static int check_allowlist(sip_msg_t *msg, struct check_blocklist_fs_t *arg1);
static int check_globalblocklist(sip_msg_t *msg);


/* ---- module init functions: */
static int mod_init(void);
static int child_init(int rank);
static int rpc_child_init(void);
static void mod_destroy(void);

static cmd_export_t cmds[] = {
		{"check_user_blocklist", (cmd_function)check_user_blocklist2, 2,
				check_user_blocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"check_user_allowlist", (cmd_function)check_user_allowlist2, 2,
				check_user_blocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"check_user_blocklist", (cmd_function)check_user_blocklist3, 3,
				check_user_blocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"check_user_allowlist", (cmd_function)check_user_allowlist3, 3,
				check_user_blocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"check_user_blocklist", (cmd_function)check_user_blocklist, 4,
				check_user_blocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"check_user_allowlist", (cmd_function)check_user_allowlist, 4,
				check_user_blocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"check_blocklist", (cmd_function)check_blocklist, 1,
				check_blocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"check_allowlist", (cmd_function)check_allowlist, 1,
				check_blocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"check_blocklist", (cmd_function)check_globalblocklist, 0,
				check_globalblocklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{0, 0, 0, 0, 0, 0}};


static param_export_t params[] = {
		userblocklist_DB_URL userblocklist_DB_TABLE globalblocklist_DB_TABLE
				userblocklist_DB_COLS globalblocklist_DB_COLS{
						"use_domain", INT_PARAM, &use_domain},
		{"match_mode", INT_PARAM, &match_mode}, {0, 0, 0}};


#ifdef MI_REMOVED
/* Exported MI functions */
static mi_export_t mi_cmds[] = {{"reload_blocklist", mi_reload_blocklist,
										MI_NO_INPUT_FLAG, 0, mi_child_init},
		{"dump_blocklist", mi_dump_blocklist, MI_NO_INPUT_FLAG, 0, 0},
		{"check_blocklist", mi_check_blocklist, 0, 0, 0},
		{"check_allowlist", mi_check_allowlist, 0, 0, 0},
		{"check_userblocklist", mi_check_userblocklist, 0, 0, 0},
		{"check_userallowlist", mi_check_userallowlist, 0, 0, 0},
		{0, 0, 0, 0, 0}};
#endif

struct module_exports exports = {
		"userblocklist", /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* cmd (cfg function) exports */
		params,			 /* param exports */
		0,				 /* RPC method exports */
		0,				 /* pseudo-variables exports */
		0,				 /* response handling function */
		mod_init,		 /* module init function */
		child_init,		 /* per-child init function */
		mod_destroy		 /* module destroy function */
};


struct source_t
{
	struct source_t *next;
	/** prefixes to be used are stored in this table */
	char *table;
	/** d-tree structure: will be built from data in database */
	struct dtrie_node_t *dtrie_root;
};


struct source_list_t
{
	struct source_t *head;
};


static gen_lock_t *lock = NULL;
static struct source_list_t *sources = NULL;
static struct dtrie_node_t *dtrie_root = NULL;


static int check_user_blocklist_fixup(void **param, int param_no)
{
	if(param_no > 0 && param_no <= 4) {
		if(strlen((char *)*param) == 0 && param_no != 4) {
			LM_ERR("no parameter %d\n", param_no);
			return E_UNSPEC;
		}
		return fixup_spve_null(param, 1);

	} else {
		LM_ERR("wrong number of parameters\n");
	}

	return 0;
}


static int ki_check_user_list(sip_msg_t *msg, str *suser, str *sdomain,
		str *snumber, str *stable, int listtype)
{
	str table = {.len = 0, .s = NULL};

	void **nodeflags;
	char *ptr;
	char req_number[MAXNUMBERLEN + 1];

	if(stable == NULL || stable->len <= 0) {
		/* use default table name */
		table.len = userblocklist_table.len;
		table.s = userblocklist_table.s;
	} else {
		table.len = stable->len;
		table.s = stable->s;
	}

	if(msg->first_line.type != SIP_REQUEST) {
		LM_ERR("SIP msg is not a request\n");
		return -1;
	}

	if(snumber == NULL || snumber->s == NULL) {
		/* use R-URI */
		if((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s)
				|| (msg->parsed_uri.user.len > MAXNUMBERLEN)) {
			LM_ERR("cannot parse msg URI\n");
			return -1;
		}
		strncpy(req_number, msg->parsed_uri.user.s, msg->parsed_uri.user.len);
		req_number[msg->parsed_uri.user.len] = '\0';
	} else {
		if(snumber->len > MAXNUMBERLEN) {
			LM_ERR("number to long\n");
			return -1;
		}
		strncpy(req_number, snumber->s, snumber->len);
		req_number[snumber->len] = '\0';
	}

	LM_DBG("check entry %s for user %.*s on domain %.*s in table %.*s\n",
			req_number, suser->len, suser->s, sdomain->len, sdomain->s,
			table.len, table.s);
	if(db_build_userbl_tree(suser, sdomain, &table, dtrie_root, use_domain)
			< 0) {
		LM_ERR("cannot build d-tree\n");
		return -1;
	}

	ptr = req_number;
	/* Skip over non-digits.  */
	while(match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	nodeflags =
			dtrie_longest_match(dtrie_root, ptr, strlen(ptr), NULL, match_mode);
	if(nodeflags) {
		if(*nodeflags == (void *)MARK_ALLOWLIST) {
			/* LM_ERR("allowlisted"); */
			return 1; /* found, but is allowlisted */
		}
	} else {
		if(!listtype) {
			/* LM_ERR("not found return 1"); */
			return 1; /* not found is ok */
		} else {
			/* LM_ERR("not found return -1"); */
			return -1; /* not found is not ok */
		}
	}
	LM_DBG("entry %s is blocklisted\n", req_number);
	return -1;
}

static int check_user_list(sip_msg_t *msg, char *puser, char *pdomain,
		char *pnumber, char *ptable, int listtype)
{
	str user = {.len = 0, .s = NULL};
	str domain = {.len = 0, .s = NULL};
	str table = {.len = 0, .s = NULL};
	str number = {.len = 0, .s = NULL};

	/* user */
	if(fixup_get_svalue(msg, (gparam_t *)puser, &user) != 0) {
		LM_ERR("cannot print user pseudo-variable\n");
		return -1;
	}
	/* domain */
	if(fixup_get_svalue(msg, (gparam_t *)pdomain, &domain) != 0) {
		LM_ERR("cannot print domain pseudo-variable\n");
		return -1;
	}
	/* source number */
	if(pnumber != NULL) {
		if(fixup_get_svalue(msg, (gparam_t *)pnumber, &number) != 0) {
			LM_ERR("cannot print number pseudo-variable\n");
			return -1;
		}
	}
	/* table name */
	if(ptable != NULL) {
		if(fixup_get_svalue(msg, (gparam_t *)ptable, &table) != 0) {
			LM_ERR("cannot print table pseudo-variable\n");
			return -1;
		}
	}

	return ki_check_user_list(msg, &user, &domain, &number, &table, listtype);
}

static int check_user_allowlist(
		sip_msg_t *msg, char *puser, char *pdomain, char *pnumber, char *ptable)
{
	return check_user_list(msg, puser, pdomain, pnumber, ptable, 1);
}

static int ki_check_user_allowlist_table(
		sip_msg_t *msg, str *suser, str *sdomain, str *snumber, str *stable)
{
	return ki_check_user_list(msg, suser, sdomain, snumber, stable, 1);
}

static int check_user_blocklist(
		sip_msg_t *msg, char *puser, char *pdomain, char *pnumber, char *ptable)
{
	return check_user_list(msg, puser, pdomain, pnumber, ptable, 0);
}

static int ki_check_user_blocklist_table(
		sip_msg_t *msg, str *suser, str *sdomain, str *snumber, str *stable)
{
	return ki_check_user_list(msg, suser, sdomain, snumber, stable, 0);
}

static int check_user_allowlist2(sip_msg_t *msg, char *puser, char *pdomain)
{
	return check_user_list(msg, puser, pdomain, NULL, NULL, 1);
}

static int ki_check_user_allowlist(sip_msg_t *msg, str *suser, str *sdomain)
{
	return ki_check_user_list(msg, suser, sdomain, NULL, NULL, 1);
}

static int check_user_blocklist2(sip_msg_t *msg, char *puser, char *pdomain)
{
	return check_user_list(msg, puser, pdomain, NULL, NULL, 0);
}

static int ki_check_user_blocklist(sip_msg_t *msg, str *suser, str *sdomain)
{
	return ki_check_user_list(msg, suser, sdomain, NULL, NULL, 0);
}

static int check_user_allowlist3(
		sip_msg_t *msg, char *puser, char *pdomain, char *pnumber)
{
	return check_user_list(msg, puser, pdomain, pnumber, NULL, 1);
}

static int ki_check_user_allowlist_number(
		sip_msg_t *msg, str *suser, str *sdomain, str *snumber)
{
	return ki_check_user_list(msg, suser, sdomain, snumber, NULL, 1);
}

static int check_user_blocklist3(
		sip_msg_t *msg, char *puser, char *pdomain, char *pnumber)
{
	return check_user_list(msg, puser, pdomain, pnumber, NULL, 0);
}

static int ki_check_user_blocklist_number(
		sip_msg_t *msg, str *suser, str *sdomain, str *snumber)
{
	return ki_check_user_list(msg, suser, sdomain, snumber, NULL, 0);
}

/**
 * Finds d-tree root for given table.
 * \return pointer to d-tree root on success, NULL otherwise
 */
static struct dtrie_node_t *table2dt(const char *table)
{
	struct source_t *src = sources->head;
	while(src) {
		if(strcmp(table, src->table) == 0)
			return src->dtrie_root;
		src = src->next;
	}

	LM_ERR("invalid table '%s'.\n", table);
	return NULL;
}


/**
 * Prepares source d-tree table and loads data.
 * \return entries count on success, -1 otherwise
 */
static int load_source(struct source_t *src)
{
	str tmp;
	int result;

	if(!src || !src->table) {
		LM_ERR("could not access source or no table defined\n");
		return -1;
	}

	tmp.s = src->table;
	tmp.len = strlen(src->table);

	result = db_reload_source(&tmp, src->dtrie_root);
	if(result < 0) {
		LM_ERR("cannot load source from '%.*s'\n", tmp.len, tmp.s);
		return 0;
	}

	LM_INFO("got %d entries from '%.*s'\n", result, tmp.len, tmp.s);
	return result;
}


/**
 * Adds a new table to the list, if the table is
 * already present, nothing will be done.
 * \return zero on success, negative on errors
 */
static int add_source(const char *table)
{
	/* check if the table is already present */
	struct source_t *src = sources->head;
	while(src) {
		if(strcmp(table, src->table) == 0) {
			LM_DBG("table %s is already present", src->table);
			return 0;
		}
		src = src->next;
	}

	src = shm_malloc(sizeof(struct source_t));
	if(!src) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(src, 0, sizeof(struct source_t));

	/* avoids dirty reads when adding source and d-tree */
	lock_get(lock);

	src->next = sources->head;
	sources->head = src;

	src->table = shm_malloc(strlen(table) + 1);
	if(!src->table) {
		SHM_MEM_ERROR;
		shm_free(src);
		lock_release(lock);
		return -1;
	}
	strcpy(src->table, table);
	LM_DBG("add table %s", table);

	src->dtrie_root = dtrie_init(match_mode);

	if(src->dtrie_root == NULL) {
		LM_ERR("could not initialize data");
		lock_release(lock);
		return -1;
	}

	if(load_source(src) < 0) {
		LM_ERR("could not load table data");
		lock_release(lock);
		return -1;
	}

	lock_release(lock);
	return 0;
}


static int check_globalblocklist_fixup(void **param, int param_no)
{
	char *table = globalblocklist_table.s;
	if(param_no > 0) {
		LM_ERR("Wrong number of parameters\n");
		return -1;
	}

	if(!table) {
		LM_ERR("no table name\n");
		return -1;
	}
	/* try to add the table */
	if(add_source(table) != 0) {
		LM_ERR("could not add table");
		return -1;
	}

	gnode = table2dt(table);
	if(!gnode) {
		LM_ERR("invalid table '%s'\n", table);
		return -1;
	}

	return 0;
}

static int ki_check_globalblocklist(sip_msg_t *msg)
{
	char *table = globalblocklist_table.s;
	struct check_blocklist_fs_t *arg = NULL;
	int result;

	if(!table) {
		LM_ERR("no table name\n");
		return -1;
	}
	/* try to add the table */
	if(add_source(table) != 0) {
		LM_ERR("could not add table");
		return -1;
	}

	gnode = table2dt(table);
	if(!gnode) {
		LM_ERR("invalid table '%s'\n", table);
		return -1;
	}

	arg = pkg_malloc(sizeof(struct check_blocklist_fs_t));
	if(!arg) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(arg, 0, sizeof(struct check_blocklist_fs_t));
	arg->dtrie_root = gnode;

	result = check_blocklist(msg, arg);
	pkg_free(arg);

	return result;
}

static int check_globalblocklist(sip_msg_t *msg)
{
	static struct check_blocklist_fs_t *arg = NULL;
	if(!arg) {
		arg = pkg_malloc(sizeof(struct check_blocklist_fs_t));
		if(!arg) {
			PKG_MEM_ERROR;
			return -1;
		}
		memset(arg, 0, sizeof(struct check_blocklist_fs_t));
		arg->dtrie_root = gnode;
	}
	return check_blocklist(msg, arg);
}

static int check_blocklist_fixup(void **arg, int arg_no)
{
	char *table = (char *)(*arg);
	struct dtrie_node_t *node = NULL;
	struct check_blocklist_fs_t *new_arg;

	if(arg_no != 1) {
		LM_ERR("wrong number of parameters\n");
		return -1;
	}

	if(!table) {
		LM_ERR("no table name\n");
		return -1;
	}
	/* try to add the table */
	if(add_source(table) != 0) {
		LM_ERR("could not add table");
		return -1;
	}

	/* get the node that belongs to the table */
	node = table2dt(table);
	if(!node) {
		LM_ERR("invalid table '%s'\n", table);
		return -1;
	}

	new_arg = pkg_malloc(sizeof(struct check_blocklist_fs_t));
	if(!new_arg) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(new_arg, 0, sizeof(struct check_blocklist_fs_t));
	new_arg->dtrie_root = node;
	*arg = (void *)new_arg;

	return 0;
}

static int ki_check_blocklist(sip_msg_t *msg, str *stable)
{
	struct dtrie_node_t *node = NULL;
	struct check_blocklist_fs_t *arg = NULL;
	int result;

	if(stable == NULL || stable->len <= 0) {
		LM_ERR("no table name\n");
		return -1;
	}

	/* try to add the table */
	if(add_source(stable->s) != 0) {
		LM_ERR("could not add table '%s'\n", stable->s);
		return -1;
	}

	/* get the node that belongs to the table */
	node = table2dt(stable->s);
	if(!node) {
		LM_ERR("invalid table '%s'\n", stable->s);
		return -1;
	}

	arg = pkg_malloc(sizeof(struct check_blocklist_fs_t));
	if(!arg) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(arg, 0, sizeof(struct check_blocklist_fs_t));
	arg->dtrie_root = node;

	result = check_blocklist(msg, arg);
	pkg_free(arg);

	return result;
}

static int check_blocklist(sip_msg_t *msg, struct check_blocklist_fs_t *arg1)
{
	void **nodeflags;
	char *ptr;
	char req_number[MAXNUMBERLEN + 1];
	int ret = -1;

	if(msg->first_line.type != SIP_REQUEST) {
		LM_ERR("SIP msg is not a request\n");
		return -1;
	}

	if((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s)
			|| (msg->parsed_uri.user.len > MAXNUMBERLEN)) {
		LM_ERR("cannot parse msg URI\n");
		return -1;
	}
	strncpy(req_number, msg->parsed_uri.user.s, msg->parsed_uri.user.len);
	req_number[msg->parsed_uri.user.len] = '\0';

	ptr = req_number;
	/* Skip over non-digits.  */
	while(match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	LM_DBG("check entry %s\n", req_number);

	/* avoids dirty reads when updating d-tree */
	lock_get(lock);
	nodeflags = dtrie_longest_match(
			arg1->dtrie_root, ptr, strlen(ptr), NULL, match_mode);
	if(nodeflags) {
		if(*nodeflags == (void *)MARK_ALLOWLIST) {
			/* LM_DBG("allowlisted"); */
			ret = 1; /* found, but is allowlisted */
		} else {
			LM_DBG("entry %s is blocklisted\n", req_number);
		}
	} else {
		/* LM_ERR("not found"); */
		ret = 1; /* not found is ok */
	}
	lock_release(lock);

	return ret;
}

static int ki_check_allowlist(sip_msg_t *msg, str *stable)
{
	struct dtrie_node_t *node = NULL;
	struct check_blocklist_fs_t *arg = NULL;
	int result;

	if(stable == NULL || stable->len <= 0) {
		LM_ERR("no table name\n");
		return -1;
	}

	/* try to add the table */
	if(add_source(stable->s) != 0) {
		LM_ERR("could not add table '%s'\n", stable->s);
		return -1;
	}

	/* get the node that belongs to the table */
	node = table2dt(stable->s);
	if(!node) {
		LM_ERR("invalid table '%s'\n", stable->s);
		return -1;
	}

	arg = pkg_malloc(sizeof(struct check_blocklist_fs_t));
	if(!arg) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(arg, 0, sizeof(struct check_blocklist_fs_t));
	arg->dtrie_root = node;

	result = check_allowlist(msg, arg);
	pkg_free(arg);

	return result;
}

static int check_allowlist(sip_msg_t *msg, struct check_blocklist_fs_t *arg1)
{
	void **nodeflags;
	char *ptr;
	char req_number[MAXNUMBERLEN + 1];
	int ret = -1;

	if(msg->first_line.type != SIP_REQUEST) {
		LM_ERR("SIP msg is not a request\n");
		return -1;
	}

	if((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s)
			|| (msg->parsed_uri.user.len > MAXNUMBERLEN)) {
		LM_ERR("cannot parse msg URI\n");
		return -1;
	}
	strncpy(req_number, msg->parsed_uri.user.s, msg->parsed_uri.user.len);
	req_number[msg->parsed_uri.user.len] = '\0';

	ptr = req_number;
	/* Skip over non-digits.  */
	while(match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	LM_DBG("check entry %s\n", req_number);

	/* avoids dirty reads when updating d-tree */
	lock_get(lock);
	nodeflags = dtrie_longest_match(
			arg1->dtrie_root, ptr, strlen(ptr), NULL, match_mode);
	if(nodeflags) {
		if(*nodeflags == (void *)MARK_ALLOWLIST) {
			/* LM_DBG("allowlisted"); */
			ret = 1; /* found, but is allowlisted */
		} else {
			LM_DBG("entry %s is blocklisted\n", req_number);
		}
	} else {
		/* LM_ERR("not found"); */
		ret = -1; /* not found is ok */
	}
	lock_release(lock);

	return ret;
}

/**
 * Fills the d-tree for all configured and prepared sources.
 * \return 0 on success, -1 otherwise
 */
static int reload_sources(void)
{
	int result = 0;
	struct source_t *src;

	/* critical section start: avoids dirty reads when updating d-tree */
	lock_get(lock);

	src = sources->head;
	while(src) {
		LM_INFO("Reloading source table '%s' with dtrie root '%p'\n",
				src->table, src->dtrie_root);
		if(load_source(src) < 0) {
			result = -1;
			break;
		}
		src = src->next;
	}

	/* critical section end */
	lock_release(lock);

	return result;
}


static int init_source_list(void)
{
	sources = shm_malloc(sizeof(struct source_list_t));
	if(!sources) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(sources, 0, sizeof(struct source_list_t));
	return 0;
}


static void destroy_source_list(void)
{
	if(sources) {
		while(sources->head) {
			struct source_t *src = sources->head;
			sources->head = src->next;

			if(src->table)
				shm_free(src->table);
			dtrie_destroy(&(src->dtrie_root), NULL, match_mode);
			shm_free(src);
		}

		shm_free(sources);
		sources = NULL;
	}
}


static int init_shmlock(void)
{
	lock = lock_alloc();
	if(!lock) {
		LM_CRIT("cannot allocate memory for lock.\n");
		return -1;
	}
	if(lock_init(lock) == 0) {
		LM_CRIT("cannot initialize lock.\n");
		return -1;
	}

	return 0;
}


static void destroy_shmlock(void)
{
	if(lock) {
		lock_destroy(lock);
		lock_dealloc((void *)lock);
		lock = NULL;
	}
}

#ifdef MI_REMOVED
static void dump_dtrie_mi(const struct dtrie_node_t *root,
		const unsigned int branches, char *prefix, int *length,
		struct mi_root *reply)
{
	struct mi_node *crt_node;
	unsigned int i;
	char digit, *val = NULL;
	int val_len = 0;

	/* Sanity check - should not reach here anyway */
	if(NULL == root) {
		LM_ERR("root dtrie is NULL\n");
		return;
	}

	/* If data found, add a new node to the reply tree */
	if(root->data) {
		/* Create new node and add it to the roots's kids */
		if(!(crt_node = add_mi_node_child(
					 &reply->node, MI_DUP_NAME, prefix, *length, 0, 0))) {
			LM_ERR("cannot add the child node to the tree\n");
			return;
		}

		/* Resolve the value of the allowlist attribute */
		if(root->data == (void *)MARK_BLOCKLIST) {
			val = int2str(0, &val_len);
		} else if(root->data == (void *)MARK_ALLOWLIST) {
			val = int2str(1, &val_len);
		}

		/* Add the attribute to the current node */
		if((add_mi_attr(crt_node, MI_DUP_VALUE, userblocklist_allowlist_col.s,
				   userblocklist_allowlist_col.len, val, val_len))
				== 0) {
			LM_ERR("cannot add attributes to the node\n");
			return;
		}
	}

	/* Perform a DFS search */
	for(i = 0; i < branches; i++) {
		/* If child branch found, traverse it */
		if(root->child[i]) {
			if(branches == 10) {
				digit = i + '0';
			} else {
				digit = i;
			}

			/* Push digit in prefix stack */
			if(*length >= MAXNUMBERLEN + 1) {
				LM_ERR("prefix length exceeds %d\n", MAXNUMBERLEN + 1);
				return;
			}
			prefix[(*length)++] = digit;

			/* Recursive DFS call */
			dump_dtrie_mi(root->child[i], branches, prefix, length, reply);

			/* Pop digit from prefix stack */
			(*length)--;
		}
	}

	return;
}


static struct mi_root *check_list_mi(struct mi_root *cmd, int list_type)
{
	struct mi_root *tmp = NULL;
	struct mi_node *crt_node, *node;
	struct mi_attr *crt_attr;
	void **nodeflags;
	int ret = -1;
	char *ptr;
	char req_prefix[MAXNUMBERLEN + 1];
	str prefix, val, attr;

	node = cmd->node.kids;

	/* Get the prefix number */
	if(NULL == node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if(NULL == node->value.s || node->value.len == 0)
		return init_mi_tree(400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	prefix = node->value;
	strncpy(req_prefix, prefix.s, prefix.len);
	req_prefix[prefix.len] = '\0';

	/* Check that just 1 argument is given */
	node = node->next;
	if(node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* Check that global blocklist exists */
	if(!gnode) {
		LM_ERR("the global blocklist is NULL\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Check that reply tree is successfully initialized */
	tmp = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if(!tmp) {
		LM_ERR("the MI tree cannot be initialized!\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Skip over non-digits. */
	ptr = req_prefix;
	while(match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	/* Avoids dirty reads when updating d-tree */
	lock_get(lock);
	nodeflags = dtrie_longest_match(gnode, ptr, strlen(ptr), NULL, match_mode);
	if(nodeflags) {
		if(*nodeflags == (void *)MARK_ALLOWLIST) {
			LM_DBG("prefix %.*s is allowlisted in table %.*s\n", prefix.len,
					prefix.s, globalblocklist_table.len,
					globalblocklist_table.s);
			ret = MARK_ALLOWLIST;
		} else if(*nodeflags == (void *)MARK_BLOCKLIST) {
			LM_DBG("prefix %.*s is blocklisted in table %.*s\n", prefix.len,
					prefix.s, globalblocklist_table.len,
					globalblocklist_table.s);
			ret = MARK_BLOCKLIST;
		}
	} else {
		LM_DBG("prefix %.*s not found in table %.*s\n", prefix.len, prefix.s,
				globalblocklist_table.len, globalblocklist_table.s);
	}
	lock_release(lock);

	/* Create new node and add it to the reply roots's kids */
	if(!(crt_node = add_mi_node_child(
				 &tmp->node, MI_DUP_NAME, prefix.s, prefix.len, 0, 0))) {
		LM_ERR("cannot add the child node to the tree\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Resolve the value of the attribute to be returned */
	val.s = FALSE_S;
	val.len = FALSE_LEN;

	switch(list_type) {
		case MARK_ALLOWLIST:
			attr.s = ALLOWLISTED_S;
			attr.len = ALLOWLISTED_LEN;

			if(ret == MARK_ALLOWLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			break;
		case MARK_BLOCKLIST:
			attr.s = BLOCKLISTED_S;
			attr.len = BLOCKLISTED_LEN;

			if(ret == MARK_BLOCKLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			break;
		default:
			LM_ERR("list_type not found\n");
			return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Add the attribute to the current node */
	if(!(crt_attr = add_mi_attr(
				 crt_node, MI_DUP_VALUE, attr.s, attr.len, val.s, val.len))) {
		LM_ERR("cannot add attribute to the node\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	return tmp;
}


static struct mi_root *check_userlist_mi(struct mi_root *cmd, int list_type)
{
	struct mi_root *tmp = NULL;
	struct mi_node *crt_node, *node;
	struct mi_attr *crt_attr;
	void **nodeflags;
	int ret = -1;
	int local_use_domain = 0;
	char *ptr;
	char req_prefix[MAXNUMBERLEN + 1];
	str user, prefix, table, val, attr, domain;

	node = cmd->node.kids;

	/* Get the user number */
	if(NULL == node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if(NULL == node->value.s || node->value.len == 0)
		return init_mi_tree(400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	user = node->value;

	/* Get the domain name */
	node = node->next;
	if(NULL == node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if(NULL == node->value.s || node->value.len == 0)
		return init_mi_tree(400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	domain = node->value;

	/* Get the prefix number */
	node = node->next;
	if(node) {
		/* Got 3 params, the third one is the prefix */
		if(NULL == node->value.s || node->value.len == 0)
			return init_mi_tree(400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
		prefix = node->value;
		local_use_domain = 1;
	} else {
		/* Got 2 params, the second one is the prefix */
		prefix = domain;
		local_use_domain = 0;
	}

	strncpy(req_prefix, prefix.s, prefix.len);
	req_prefix[prefix.len] = '\0';

	/* Check that a maximum of 3 arguments are given */
	if(node)
		node = node->next;
	if(node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* Build userblocklist dtrie */
	table = userblocklist_table;
	LM_DBG("check entry %s for user %.*s@%.*s in table %.*s, use domain=%d\n",
			req_prefix, user.len, user.s, domain.len, domain.s, table.len,
			table.s, local_use_domain);
	if(db_build_userbl_tree(
			   &user, &domain, &table, dtrie_root, local_use_domain)
			< 0) {
		LM_ERR("cannot build d-tree\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Check that reply tree is successfully initialized */
	tmp = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if(!tmp) {
		LM_ERR("the MI tree cannot be initialized!\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Skip over non-digits. */
	ptr = req_prefix;
	while(match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	/* Search for a match in dtrie */
	nodeflags =
			dtrie_longest_match(dtrie_root, ptr, strlen(ptr), NULL, match_mode);
	if(nodeflags) {
		if(*nodeflags == (void *)MARK_ALLOWLIST) {
			LM_DBG("user %.*s is allowlisted for prefix %.*s in table %.*s\n",
					user.len, user.s, prefix.len, prefix.s, table.len, table.s);
			ret = MARK_ALLOWLIST;
		} else if(*nodeflags == (void *)MARK_BLOCKLIST) {
			LM_DBG("user %.*s is blocklisted for prefix %.*s in table %.*s\n",
					user.len, user.s, prefix.len, prefix.s, table.len, table.s);
			ret = MARK_BLOCKLIST;
		}
	} else {
		LM_DBG("user %.*s, prefix %.*s not found in table %.*s\n", user.len,
				user.s, prefix.len, prefix.s, table.len, table.s);
	}


	/* Create new node and add it to the reply roots's kids */
	if(!(crt_node = add_mi_node_child(
				 &tmp->node, MI_DUP_NAME, prefix.s, prefix.len, 0, 0))) {
		LM_ERR("cannot add the child node to the tree\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Resolve the value of the attribute to be returned */
	val.s = FALSE_S;
	val.len = FALSE_LEN;

	switch(list_type) {
		case MARK_ALLOWLIST:
			attr.s = ALLOWLISTED_S;
			attr.len = ALLOWLISTED_LEN;

			if(ret == MARK_ALLOWLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			break;
		case MARK_BLOCKLIST:
			attr.s = BLOCKLISTED_S;
			attr.len = BLOCKLISTED_LEN;

			if(ret == MARK_BLOCKLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			break;
		default:
			LM_ERR("list_type not found\n");
			return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Add the attribute to the current node */
	if(!(crt_attr = add_mi_attr(
				 crt_node, MI_DUP_VALUE, attr.s, attr.len, val.s, val.len))) {
		LM_ERR("cannot add attribute to the node\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	return tmp;
}


struct mi_root *mi_reload_blocklist(struct mi_root *cmd, void *param)
{
	struct mi_root *tmp = NULL;
	if(reload_sources() == 0) {
		tmp = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	} else {
		tmp = init_mi_tree(500, "cannot reload blocklist", 21);
	}

	return tmp;
}


struct mi_root *mi_dump_blocklist(struct mi_root *cmd, void *param)
{
	char prefix_buff[MAXNUMBERLEN + 1];
	int length = 0;
	struct mi_root *tmp = NULL;

	/* Check that global blocklist exists */
	if(!gnode) {
		LM_ERR("the global blocklist is NULL\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	tmp = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if(!tmp) {
		LM_ERR("the MI tree cannot be initialized!\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	dump_dtrie_mi(gnode, match_mode, prefix_buff, &length, tmp);

	return tmp;
}


struct mi_root *mi_check_blocklist(struct mi_root *cmd, void *param)
{
	return check_list_mi(cmd, MARK_BLOCKLIST);
}


struct mi_root *mi_check_allowlist(struct mi_root *cmd, void *param)
{
	return check_list_mi(cmd, MARK_ALLOWLIST);
}


struct mi_root *mi_check_userblocklist(struct mi_root *cmd, void *param)
{
	return check_userlist_mi(cmd, MARK_BLOCKLIST);
}


struct mi_root *mi_check_userallowlist(struct mi_root *cmd, void *param)
{
	return check_userlist_mi(cmd, MARK_ALLOWLIST);
}
#endif

static void dump_dtrie_rpc(rpc_t *rpc, void *ctx,
		const struct dtrie_node_t *root, const unsigned int branches,
		char *prefix, int *length)
{
	unsigned int i;
	char digit, *val = NULL;
	int val_len = 0;
	void *out;

	/* Sanity check - should not reach here anyway */
	if(NULL == root) {
		LM_ERR("root dtrie is NULL\n");
		return;
	}

	/* If data found, add a new node to the reply tree */
	if(root->data) {
		if(rpc->add(ctx, "{", &out) < 0)
			goto error;

		/* Resolve the value of the allowlist attribute */
		if(root->data == (void *)MARK_BLOCKLIST) {
			val = int2str(0, &val_len);
		} else if(root->data == (void *)MARK_ALLOWLIST) {
			val = int2str(1, &val_len);
		}

		prefix[*length] = '\0';

		rpc->struct_add(out, "ss", "prefix", prefix,
				userblocklist_allowlist_col.s, val);
	}

	/* Perform a DFS search */
	for(i = 0; i < branches; i++) {
		/* If child branch found, traverse it */
		if(root->child[i]) {
			if(branches == 10) {
				digit = i + '0';
			} else {
				digit = i;
			}

			/* Push digit in prefix stack */
			if(*length >= MAXNUMBERLEN + 1) {
				LM_ERR("prefix length exceeds %d\n", MAXNUMBERLEN + 1);
				return;
			}
			prefix[(*length)++] = digit;

			/* Recursive DFS call */
			dump_dtrie_rpc(rpc, ctx, root->child[i], branches, prefix, length);

			/* Pop digit from prefix stack */
			(*length)--;
		}
	}

	return;

error:
	rpc->fault(ctx, 500, "Dump dtrie failed");
	return;
}

static void dump_blocklist_rpc(rpc_t *rpc, void *ctx)
{
	char prefix_buff[MAXNUMBERLEN + 1];
	int length = 0;

	/* Check that global blocklist exists */
	if(!gnode) {
		LM_ERR("the global blocklist is NULL\n");
		goto error;
	}

	dump_dtrie_rpc(rpc, ctx, gnode, match_mode, prefix_buff, &length);

	return;

error:
	rpc->fault(ctx, 500, "Dump blocklist failed");
	return;
}

static void check_list_rpc(rpc_t *rpc, void *ctx, int list_type)
{
	str prefix, val;
	char req_prefix[MAXNUMBERLEN + 1], *ptr;
	void **nodeflags, *out;
	int ret = 0;

	/* Sanity checks */
	if(rpc->scan(ctx, ".S", &prefix) < 1)
		goto error_scan;
	if(prefix.s == NULL || prefix.len == 0)
		goto error_scan;
	if(rpc->add(ctx, "{", &out) < 0)
		goto error;

	strncpy(req_prefix, prefix.s, prefix.len);
	req_prefix[prefix.len] = '\0';

	/* Check that global blocklist exists */
	if(!gnode) {
		LM_ERR("global gnode not found\n");
		goto error;
	}

	/* Skip over non-digits. */
	ptr = req_prefix;
	while(match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	/* Avoids dirty reads when updating d-tree */
	lock_get(lock);
	nodeflags = dtrie_longest_match(gnode, ptr, strlen(ptr), NULL, match_mode);
	if(nodeflags) {
		if(*nodeflags == (void *)MARK_ALLOWLIST) {
			LM_DBG("prefix %.*s is allowlisted in table %.*s\n", prefix.len,
					prefix.s, globalblocklist_table.len,
					globalblocklist_table.s);
			ret = MARK_ALLOWLIST;
		} else if(*nodeflags == (void *)MARK_BLOCKLIST) {
			LM_DBG("prefix %.*s is blocklisted in table %.*s\n", prefix.len,
					prefix.s, globalblocklist_table.len,
					globalblocklist_table.s);
			ret = MARK_BLOCKLIST;
		}
	} else {
		LM_DBG("prefix %.*s not found in table %.*s\n", prefix.len, prefix.s,
				globalblocklist_table.len, globalblocklist_table.s);
	}
	lock_release(lock);

	/* Resolve the value of the attribute to be returned */
	val.s = FALSE_S;
	val.len = FALSE_LEN;

	switch(list_type) {
		case MARK_ALLOWLIST:
			if(ret == MARK_ALLOWLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			rpc->struct_add(out, "SS", "prefix", &prefix, ALLOWLISTED_S, &val);
			break;

		case MARK_BLOCKLIST:
			if(ret == MARK_BLOCKLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			rpc->struct_add(out, "SS", "prefix", &prefix, BLOCKLISTED_S, &val);
			break;

		default:
			LM_ERR("list_type not found\n");
			goto error;
	}

	return;

error_scan:
	rpc->fault(ctx, 500, "Check failed: 1 argument needed (\"prefix\")");
	return;

error:
	rpc->fault(ctx, 500, "Check failed");
	return;
}

static void check_userlist_rpc(rpc_t *rpc, void *ctx, int list_type)
{
	str prefix, user, domain, table, val;
	char req_prefix[MAXNUMBERLEN + 1], *ptr;
	void **nodeflags, *out;
	int ret = 0, local_use_domain = 0;

	/* Sanity checks */
	if(rpc->scan(ctx, ".S.S", &prefix, &user) < 2)
		goto error_scan;
	if(rpc->scan(ctx, ".S", &domain) < 1) {
		domain.s = "";
		domain.len = 0;
	}
	if(prefix.s == NULL || prefix.len == 0 || user.s == NULL || user.len == 0)
		goto error_scan;
	if(rpc->add(ctx, "{", &out) < 0)
		goto error;
	if(domain.s != NULL && domain.len != 0)
		local_use_domain = 1;

	strncpy(req_prefix, prefix.s, prefix.len);
	req_prefix[prefix.len] = '\0';

	/* Check that global blocklist exists */
	if(!gnode) {
		LM_ERR("global gnode not found\n");
		goto error;
	}

	/* Build userblocklist dtrie */
	table = userblocklist_table;
	LM_DBG("check entry %s for user %.*s@%.*s in table %.*s, use domain=%d\n",
			req_prefix, user.len, user.s, domain.len, domain.s, table.len,
			table.s, local_use_domain);
	if(db_build_userbl_tree(
			   &user, &domain, &table, dtrie_root, local_use_domain)
			< 0) {
		LM_ERR("cannot build d-tree\n");
		goto error;
	}

	/* Skip over non-digits. */
	ptr = req_prefix;
	while(match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	/* Avoids dirty reads when updating d-tree */
	/* Search for a match in dtrie */
	nodeflags =
			dtrie_longest_match(dtrie_root, ptr, strlen(ptr), NULL, match_mode);
	if(nodeflags) {
		if(*nodeflags == (void *)MARK_ALLOWLIST) {
			LM_DBG("user %.*s is allowlisted for prefix %.*s in table %.*s\n",
					user.len, user.s, prefix.len, prefix.s, table.len, table.s);
			ret = MARK_ALLOWLIST;
		} else if(*nodeflags == (void *)MARK_BLOCKLIST) {
			LM_DBG("user %.*s is blocklisted for prefix %.*s in table %.*s\n",
					user.len, user.s, prefix.len, prefix.s, table.len, table.s);
			ret = MARK_BLOCKLIST;
		}
	} else {
		LM_DBG("user %.*s, prefix %.*s not found in table %.*s\n", user.len,
				user.s, prefix.len, prefix.s, table.len, table.s);
	}

	/* Resolve the value of the attribute to be returned */
	val.s = FALSE_S;
	val.len = FALSE_LEN;

	switch(list_type) {
		case MARK_ALLOWLIST:
			if(ret == MARK_ALLOWLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			rpc->struct_add(out, "SSSS", "prefix", &prefix, "user", &user,
					"domain", &domain, ALLOWLISTED_S, &val);
			break;

		case MARK_BLOCKLIST:
			if(ret == MARK_BLOCKLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			rpc->struct_add(out, "SSSS", "prefix", &prefix, "user", &user,
					"domain", &domain, BLOCKLISTED_S, &val);
			break;

		default:
			LM_ERR("list_type not found\n");
			goto error;
	}

	return;

error_scan:
	rpc->fault(ctx, 500,
			"Check failed: 2 or 3 arguments needed (\"prefix\" \"user\" "
			"\"domain\"(optional))");
	return;

error:
	rpc->fault(ctx, 500, "Check failed");
	return;
}

static void ubl_rpc_reload_blocklist(rpc_t *rpc, void *ctx)
{
	if(reload_sources() != 0) {
		rpc->fault(ctx, 500, "Reload failed");
		return;
	}

	rpc->add(ctx, "s", "200 ok");
	return;
}

static void ubl_rpc_dump_blocklist(rpc_t *rpc, void *ctx)
{
	return dump_blocklist_rpc(rpc, ctx);
}

static void ubl_rpc_check_blocklist(rpc_t *rpc, void *ctx)
{
	return check_list_rpc(rpc, ctx, MARK_BLOCKLIST);
}


static void ubl_rpc_check_allowlist(rpc_t *rpc, void *ctx)
{
	return check_list_rpc(rpc, ctx, MARK_ALLOWLIST);
}


static void ubl_rpc_check_userblocklist(rpc_t *rpc, void *ctx)
{
	return check_userlist_rpc(rpc, ctx, MARK_BLOCKLIST);
}


static void ubl_rpc_check_userallowlist(rpc_t *rpc, void *ctx)
{
	return check_userlist_rpc(rpc, ctx, MARK_ALLOWLIST);
}

static const char *ubl_rpc_reload_blocklist_doc[2] = {
		"Reload user blocklist records.", 0};

static const char *ubl_rpc_dump_blocklist_doc[2] = {
		"Dump user blocklist records.", 0};

static const char *ubl_rpc_check_blocklist_doc[2] = {
		"Check blocklist records.", 0};

static const char *ubl_rpc_check_allowlist_doc[2] = {
		"Check allowlist records.", 0};

static const char *ubl_rpc_check_userblocklist_doc[2] = {
		"Check user blocklist records.", 0};

static const char *ubl_rpc_check_userallowlist_doc[2] = {
		"Check user allowlist records.", 0};

rpc_export_t ubl_rpc[] = {
		{"userblocklist.reload_blocklist", ubl_rpc_reload_blocklist,
				ubl_rpc_reload_blocklist_doc, 0},
		{"userblocklist.dump_blocklist", ubl_rpc_dump_blocklist,
				ubl_rpc_dump_blocklist_doc, 0},
		{"userblocklist.check_blocklist", ubl_rpc_check_blocklist,
				ubl_rpc_check_blocklist_doc, 0},
		{"userblocklist.check_allowlist", ubl_rpc_check_allowlist,
				ubl_rpc_check_allowlist_doc, 0},
		{"userblocklist.check_userblocklist", ubl_rpc_check_userblocklist,
				ubl_rpc_check_userblocklist_doc, 0},
		{"userblocklist.check_userallowlist", ubl_rpc_check_userallowlist,
				ubl_rpc_check_userallowlist_doc, 0},
		{0, 0, 0, 0}};

static int ubl_rpc_init(void)
{
	if(rpc_register_array(ubl_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

static int mod_init(void)
{
	if(ubl_rpc_init() < 0)
		return -1;
	if(userblocklist_db_init() != 0)
		return -1;
	if(init_shmlock() != 0)
		return -1;
	if(init_source_list() != 0)
		return -1;
	return 0;
}


static int child_init(int rank)
{
	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	return rpc_child_init();
}

static int userblocklist_child_initialized = 0;
static int blocklist_child_initialized = 0;

static int rpc_child_init(void)
{
	/* user blocklist init */
	if(userblocklist_child_initialized)
		return 0;
	if(userblocklist_db_open() != 0)
		return -1;
	dtrie_root = dtrie_init(match_mode);
	if(dtrie_root == NULL) {
		LM_ERR("could not initialize data");
		return -1;
	}

	/* global blocklist init */
	if(check_globalblocklist_fixup(NULL, 0) != 0) {
		LM_ERR("could not add global table when init the module");
	}

	/* because we've added new sources during the fixup */
	if(reload_sources() != 0)
		return -1;

	userblocklist_child_initialized = 1;
	blocklist_child_initialized = 1;

	return 0;
}


static void mod_destroy(void)
{
	destroy_source_list();
	destroy_shmlock();
	userblocklist_db_close();
	dtrie_destroy(&dtrie_root, NULL, match_mode);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_userblocklist_exports[] = {
	{ str_init("userblocklist"), str_init("check_user_blocklist"),
		SR_KEMIP_INT, ki_check_user_blocklist,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("userblocklist"), str_init("check_user_allowlist"),
		SR_KEMIP_INT, ki_check_user_allowlist,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("userblocklist"), str_init("check_user_blocklist_number"),
		SR_KEMIP_INT, ki_check_user_blocklist_number,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("userblocklist"), str_init("check_user_allowlist_number"),
		SR_KEMIP_INT, ki_check_user_allowlist_number,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("userblocklist"), str_init("check_user_blocklist_table"),
		SR_KEMIP_INT, ki_check_user_blocklist_table,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("userblocklist"), str_init("check_user_allowlist_table"),
		SR_KEMIP_INT, ki_check_user_allowlist_table,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("userblocklist"), str_init("check_allowlist"),
		SR_KEMIP_INT, ki_check_allowlist,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("userblocklist"), str_init("check_blocklist"),
		SR_KEMIP_INT, ki_check_blocklist,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("userblocklist"), str_init("check_global_blocklist"),
		SR_KEMIP_INT, ki_check_globalblocklist,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_userblocklist_exports);
	return 0;
}
