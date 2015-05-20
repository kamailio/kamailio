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
 * \brief USERBLACKLIST :: module definitions
 * \ingroup userblacklist
 * - Module: \ref userblacklist
 */

/*!
 * \defgroup userblacklist USERBLACKLIST :: The Kamailio userblacklist Module
 *
 * The userblacklist module allows Kamailio to handle blacklists on a per user basis.
 * This information is stored in a database table, which is queried to decide if the
 * number (more exactly, the request URI user) is blacklisted or not.
 * An additional functionality that this module provides is the ability to handle
 * global blacklists. This lists are loaded on startup into memory, thus providing a
 * better performance then in the userblacklist case.
 */

#include <string.h>

#include "../../parser/parse_uri.h"
#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "../../usr_avp.h"
#include "../../locking.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mod_fix.h"

#include "../../lib/trie/dtrie.h"
#include "db.h"
#include "db_userblacklist.h"

MODULE_VERSION


#define MAXNUMBERLEN 31

#define BLACKLISTED_S		"blacklisted"
#define BLACKLISTED_LEN		(sizeof(BLACKLISTED_S)-1)
#define WHITELISTED_S		"whitelisted"
#define WHITELISTED_LEN		(sizeof(WHITELISTED_S)-1)
#define TRUE_S			"true"
#define TRUE_LEN		(sizeof(TRUE_S)-1)
#define FALSE_S			"false"
#define FALSE_LEN		(sizeof(FALSE_S)-1)

typedef struct _avp_check
{
	int avp_flags;
	int_str avp_name;
} avp_check_t;


struct check_blacklist_fs_t {
  struct dtrie_node_t *dtrie_root;
};

str userblacklist_db_url = str_init(DEFAULT_RODB_URL);
int use_domain = 0;
int match_mode = 10; /* numeric */
static struct dtrie_node_t *gnode = NULL;

/* ---- fixup functions: */
static int check_blacklist_fixup(void** param, int param_no);
static int check_user_blacklist_fixup(void** param, int param_no);
static int check_globalblacklist_fixup(void** param, int param_no);

/* ---- exported commands: */
static int check_user_blacklist(struct sip_msg *msg, char* str1, char* str2, char* str3, char* str4);
static int check_user_whitelist(struct sip_msg *msg, char* str1, char* str2, char* str3, char* str4);
static int check_user_blacklist2(struct sip_msg *msg, char* str1, char* str2);
static int check_user_whitelist2(struct sip_msg *msg, char* str1, char* str2);
static int check_user_blacklist3(struct sip_msg *msg, char* str1, char* str2, char* str3);
static int check_user_whitelist3(struct sip_msg *msg, char* str1, char* str2, char* str3);
static int check_blacklist(struct sip_msg *msg, struct check_blacklist_fs_t *arg1);
static int check_whitelist(struct sip_msg *msg, struct check_blacklist_fs_t *arg1);
static int check_globalblacklist(struct sip_msg *msg);


/* ---- module init functions: */
static int mod_init(void);
static int child_init(int rank);
static int mi_child_init(void);
static void mod_destroy(void);

/* --- fifo functions */
struct mi_root * mi_reload_blacklist(struct mi_root* cmd, void* param);  /* usage: kamctl fifo reload_blacklist */
struct mi_root * mi_dump_blacklist(struct mi_root* cmd, void* param);  /* usage: kamctl fifo dump_blacklist */
struct mi_root * mi_check_blacklist(struct mi_root* cmd, void* param);  /* usage: kamctl fifo check_blacklist prefix */
struct mi_root * mi_check_whitelist(struct mi_root* cmd, void* param);  /* usage: kamctl fifo check_whitelist prefix */
struct mi_root * mi_check_userblacklist(struct mi_root* cmd, void* param);  /* usage: kamctl fifo check_userblacklist */
struct mi_root * mi_check_userwhitelist(struct mi_root* cmd, void* param);  /* usage: kamctl fifo check_userwhitelist */


static cmd_export_t cmds[]={
	{ "check_user_blacklist", (cmd_function)check_user_blacklist2, 2, check_user_blacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ "check_user_whitelist", (cmd_function)check_user_whitelist2, 2, check_user_blacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ "check_user_blacklist", (cmd_function)check_user_blacklist3, 3, check_user_blacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ "check_user_whitelist", (cmd_function)check_user_whitelist3, 3, check_user_blacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ "check_user_blacklist", (cmd_function)check_user_blacklist, 4, check_user_blacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ "check_user_whitelist", (cmd_function)check_user_whitelist, 4, check_user_blacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ "check_blacklist", (cmd_function)check_blacklist, 1, check_blacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ "check_whitelist", (cmd_function)check_whitelist, 1, check_blacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ "check_blacklist", (cmd_function)check_globalblacklist, 0, check_globalblacklist_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ 0, 0, 0, 0, 0, 0}
};


static param_export_t params[] = {
	userblacklist_DB_URL
	userblacklist_DB_TABLE
	globalblacklist_DB_TABLE
	userblacklist_DB_COLS
	globalblacklist_DB_COLS
	{ "use_domain",      INT_PARAM, &use_domain },
	{ "match_mode",	     INT_PARAM, &match_mode },
	{ 0, 0, 0 }
};


/* Exported MI functions */
static mi_export_t mi_cmds[] = {
	{ "reload_blacklist", mi_reload_blacklist, MI_NO_INPUT_FLAG, 0, mi_child_init },
	{ "dump_blacklist", mi_dump_blacklist, MI_NO_INPUT_FLAG, 0, 0},
	{ "check_blacklist", mi_check_blacklist, 0, 0, 0 },
	{ "check_whitelist", mi_check_whitelist, 0, 0, 0 },
	{ "check_userblacklist", mi_check_userblacklist, 0, 0, 0 },
	{ "check_userwhitelist", mi_check_userwhitelist, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 }
};


struct module_exports exports= {
	"userblacklist",
	DEFAULT_DLFLAGS,
	cmds,
	params,
	0,
	mi_cmds,
	0,
	0,
	mod_init,
	0,
	mod_destroy,
	child_init
};


struct source_t {
	struct source_t *next;
	/** prefixes to be used are stored in this table */
	char *table;
	/** d-tree structure: will be built from data in database */
	struct dtrie_node_t *dtrie_root;
};


struct source_list_t {
  struct source_t *head;
};


static gen_lock_t *lock = NULL;
static struct source_list_t *sources = NULL;
static struct dtrie_node_t *dtrie_root = NULL;


static int check_user_blacklist_fixup(void** param, int param_no)
{
	pv_elem_t *model=NULL;
	str s;

	/* convert to str */
	s.s = (char*)*param;
	s.len = strlen(s.s);

	if (param_no > 0 && param_no <= 4) {
		if(s.len == 0 && param_no != 4) {
			LM_ERR("no parameter %d\n", param_no);
			return E_UNSPEC;
		}

		if(pv_parse_format(&s, &model) < 0 || !model) {
			LM_ERR("wrong format [%.*s] for parameter %d\n", s.len, s.s, param_no);
			return E_UNSPEC;
		}

		if(model->spec==NULL || model->spec->getf==NULL) {
			if(param_no == 1) {
				if(str2int(&s, (unsigned int*)&model->spec->pvp.pvn.u.isname.name.n) != 0) {
					LM_ERR("wrong value [%.*s] for parameter %d\n", s.len, s.s, param_no);
					return E_UNSPEC;
				}
			} else {
				if(param_no == 2 || param_no == 3) {
					LM_ERR("wrong value [%.*s] for parameter %d\n", s.len, s.s, param_no);
					return E_UNSPEC;
				} else {
					// only a string
					return 0;
				}
			}
		}
		*param = (void*)model;
	} else {
		LM_ERR("wrong number of parameters\n");
	}

	return 0;
}


static int check_user_list(struct sip_msg *msg, char* str1, char* str2, char* str3, char* str4, int listtype)
{
	str user = { .len = 0, .s = NULL };
	str domain = { .len = 0, .s = NULL};
	str table = { .len = 0, .s = NULL};
	str number = { .len = 0, .s = NULL};

	void **nodeflags;
	char *ptr;
	char req_number[MAXNUMBERLEN+1];

	/* user */
	if(((pv_elem_p)str1)->spec!=NULL && ((pv_elem_p)str1)->spec->getf!=NULL) {
		if(pv_printf_s(msg, (pv_elem_p)str1, &user) != 0) {
			LM_ERR("cannot print user pseudo-variable\n");
			return -1;
		}
	}
	/* domain */
	if(((pv_elem_p)str2)->spec!=NULL && ((pv_elem_p)str2)->spec->getf) {
		if(pv_printf_s(msg, (pv_elem_p)str2, &domain) != 0) {
			LM_ERR("cannot print domain pseudo-variable\n");
			return -1;
		}
	}
	/* source number */
	if(str3 != NULL && ((pv_elem_p)str3)->spec!=NULL && ((pv_elem_p)str3)->spec->getf!=NULL) {
		if(pv_printf_s(msg, (pv_elem_p)str3, &number) != 0) {
			LM_ERR("cannot print number pseudo-variable\n");
			return -1;
		}
	}
	/* table name */
	if(str4 != NULL && strlen(str4) > 0) {
		/* string */
		table.s=str4;
		table.len=strlen(str4);
	} else {
		/* use default table name */
		table.len=userblacklist_table.len;
		table.s=userblacklist_table.s;
	}

	if (msg->first_line.type != SIP_REQUEST) {
		LM_ERR("SIP msg is not a request\n");
		return -1;
	}

	if(number.s == NULL) {
		/* use R-URI */
		if ((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s) || (msg->parsed_uri.user.len > MAXNUMBERLEN)) {
			LM_ERR("cannot parse msg URI\n");
			return -1;
		}
		strncpy(req_number, msg->parsed_uri.user.s, msg->parsed_uri.user.len);
		req_number[msg->parsed_uri.user.len] = '\0';
	} else {
		if (number.len > MAXNUMBERLEN) {
			LM_ERR("number to long\n");
			return -1;
		}
		strncpy(req_number, number.s, number.len);
		req_number[number.len] = '\0';
	}

	LM_DBG("check entry %s for user %.*s on domain %.*s in table %.*s\n", req_number,
		user.len, user.s, domain.len, domain.s, table.len, table.s);
	if (db_build_userbl_tree(&user, &domain, &table, dtrie_root, use_domain) < 0) {
		LM_ERR("cannot build d-tree\n");
		return -1;
	}

	ptr = req_number;
	/* Skip over non-digits.  */
	while (match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	nodeflags = dtrie_longest_match(dtrie_root, ptr, strlen(ptr), NULL, match_mode);
	if (nodeflags) {
		if (*nodeflags == (void *)MARK_WHITELIST) {
			/* LM_ERR("whitelisted"); */
			return 1; /* found, but is whitelisted */
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
	LM_DBG("entry %s is blacklisted\n", req_number);
	return -1;
}


static int check_user_whitelist(struct sip_msg *msg, char* str1, char* str2, char* str3, char* str4)
{
	return check_user_list(msg, str1, str2, str3, str4, 1);
}


static int check_user_blacklist(struct sip_msg *msg, char* str1, char* str2, char* str3, char* str4)
{
	return check_user_list(msg, str1, str2, str3, str4, 0);
}

static int check_user_whitelist2(struct sip_msg *msg, char* str1, char* str2)
{
	return check_user_list(msg, str1, str2, NULL, NULL, 1);
}


static int check_user_blacklist2(struct sip_msg *msg, char* str1, char* str2)
{
	return check_user_list(msg, str1, str2, NULL, NULL, 0);
}

static int check_user_whitelist3(struct sip_msg *msg, char* str1, char* str2, char* str3)
{
	return check_user_list(msg, str1, str2, str3, NULL, 1);
}


static int check_user_blacklist3(struct sip_msg *msg, char* str1, char* str2, char* str3)
{
	return check_user_list(msg, str1, str2, str3, NULL, 0);
}


/**
 * Finds d-tree root for given table.
 * \return pointer to d-tree root on success, NULL otherwise
 */
static struct dtrie_node_t *table2dt(const char *table)
{
	struct source_t *src = sources->head;
	while (src) {
		if (strcmp(table, src->table) == 0) return src->dtrie_root;
		src = src->next;
	}

	LM_ERR("invalid table '%s'.\n", table);
	return NULL;
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
	while (src) {
		if (strcmp(table, src->table) == 0) return 0;
		src = src->next;
	}

	src = shm_malloc(sizeof(struct source_t));
	if (!src) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(src, 0, sizeof(struct source_t));

	src->next = sources->head;
	sources->head = src;

	src->table = shm_malloc(strlen(table)+1);
	if (!src->table) {
		SHM_MEM_ERROR;
		shm_free(src);
		return -1;
	}
	strcpy(src->table, table);
	LM_DBG("add table %s", table);

	src->dtrie_root = dtrie_init(match_mode);

	if (src->dtrie_root == NULL) {
		LM_ERR("could not initialize data");
		return -1;
	}

	return 0;
}


static int check_globalblacklist_fixup(void** param, int param_no)
{
	char * table = globalblacklist_table.s;
	if(param_no > 0){
		LM_ERR("Wrong number of parameters\n");
		return -1;
	}

	if (!table) {
		LM_ERR("no table name\n");
		return -1;
	}
	/* try to add the table */
	if (add_source(table) != 0) {
		LM_ERR("could not add table");
		return -1;
	}

	gnode = table2dt(table);
	if (!gnode) {
		LM_ERR("invalid table '%s'\n", table);
		return -1;
	}

	return 0;
}

static int check_globalblacklist(struct sip_msg* msg)
{
	static struct check_blacklist_fs_t* arg = NULL;
	if(!arg){
		arg = pkg_malloc(sizeof(struct check_blacklist_fs_t));
		if (!arg) {
			PKG_MEM_ERROR;
			return -1;
		}
		memset(arg, 0, sizeof(struct check_blacklist_fs_t));
		arg->dtrie_root = gnode;
	}
	return check_blacklist(msg, arg);
}

static int check_blacklist_fixup(void **arg, int arg_no)
{
	char *table = (char *)(*arg);
	struct dtrie_node_t *node = NULL;
	struct check_blacklist_fs_t *new_arg;
	
	if (arg_no != 1) {
		LM_ERR("wrong number of parameters\n");
		return -1;
	}

	if (!table) {
		LM_ERR("no table name\n");
		return -1;
	}
	/* try to add the table */
	if (add_source(table) != 0) {
		LM_ERR("could not add table");
		return -1;
	}	

	/* get the node that belongs to the table */
	node = table2dt(table);
	if (!node) {
		LM_ERR("invalid table '%s'\n", table);
		return -1;
	}

	new_arg = pkg_malloc(sizeof(struct check_blacklist_fs_t));
	if (!new_arg) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(new_arg, 0, sizeof(struct check_blacklist_fs_t));
	new_arg->dtrie_root = node;
	*arg=(void*)new_arg;

	return 0;
}


static int check_blacklist(struct sip_msg *msg, struct check_blacklist_fs_t *arg1)
{
	void **nodeflags;
	char *ptr;
	char req_number[MAXNUMBERLEN+1];
	int ret = -1;

	if (msg->first_line.type != SIP_REQUEST) {
		LM_ERR("SIP msg is not a request\n");
		return -1;
	}

	if ((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s) || (msg->parsed_uri.user.len > MAXNUMBERLEN)) {
		LM_ERR("cannot parse msg URI\n");
		return -1;
	}
	strncpy(req_number, msg->parsed_uri.user.s, msg->parsed_uri.user.len);
	req_number[msg->parsed_uri.user.len] = '\0';

	ptr = req_number;
	/* Skip over non-digits.  */
	while (match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
			ptr = ptr + 1;
	}

	LM_DBG("check entry %s\n", req_number);

	/* avoids dirty reads when updating d-tree */
	lock_get(lock);
	nodeflags = dtrie_longest_match(arg1->dtrie_root, ptr, strlen(ptr), NULL, match_mode);
	if (nodeflags) {
		if (*nodeflags == (void *)MARK_WHITELIST) {
			/* LM_DBG("whitelisted"); */
			ret = 1; /* found, but is whitelisted */
		}
	}
	else {
		/* LM_ERR("not found"); */
		ret = 1; /* not found is ok */
	}
	lock_release(lock);

	LM_DBG("entry %s is blacklisted\n", req_number);
	return ret;
}

static int check_whitelist(struct sip_msg *msg, struct check_blacklist_fs_t *arg1)
{
	void **nodeflags;
	char *ptr;
	char req_number[MAXNUMBERLEN+1];
	int ret = -1;

	if (msg->first_line.type != SIP_REQUEST) {
		LM_ERR("SIP msg is not a request\n");
		return -1;
	}

	if ((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s) || (msg->parsed_uri.user.len > MAXNUMBERLEN)) {
		LM_ERR("cannot parse msg URI\n");
		return -1;
	}
	strncpy(req_number, msg->parsed_uri.user.s, msg->parsed_uri.user.len);
	req_number[msg->parsed_uri.user.len] = '\0';

	ptr = req_number;
	/* Skip over non-digits.  */
	while (strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	LM_DBG("check entry %s\n", req_number);

	/* avoids dirty reads when updating d-tree */
	lock_get(lock);
	nodeflags = dtrie_longest_match(arg1->dtrie_root, ptr, strlen(ptr), NULL, 10);
	if (nodeflags) {
		if (*nodeflags == (void *)MARK_WHITELIST) {
			/* LM_DBG("whitelisted"); */
			ret = 1; /* found, but is whitelisted */
		}
	}
	else {
		/* LM_ERR("not found"); */
		ret = -1; /* not found is ok */
	}
	lock_release(lock);

	LM_DBG("entry %s is blacklisted\n", req_number);
	return ret;
}

/**
 * Fills the d-tree for all configured and prepared sources.
 * \return 0 on success, -1 otherwise
 */
static int reload_sources(void)
{
	int result = 0;
	str tmp;
	struct source_t *src;
	int n;

	/* critical section start: avoids dirty reads when updating d-tree */
	lock_get(lock);

	src = sources->head;
	while (src) {
		tmp.s = src->table;
		tmp.len = strlen(src->table);
		n = db_reload_source(&tmp, src->dtrie_root);
		if (n < 0) {
			LM_ERR("cannot reload source from '%.*s'\n", tmp.len, tmp.s);
			result = -1;
			break;
		}
		LM_INFO("got %d entries from '%.*s'\n", n, tmp.len, tmp.s);
		src = src->next;
	}

	/* critical section end */
	lock_release(lock);

	return result;
}


static int init_source_list(void)
{
	sources = shm_malloc(sizeof(struct source_list_t));
	if (!sources) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(sources, 0, sizeof(struct source_list_t));
	return 0;
}


static void destroy_source_list(void)
{
	if (sources) {
		while (sources->head) {
			struct source_t *src = sources->head;
			sources->head = src->next;

			if (src->table) shm_free(src->table);
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
	if (!lock) {
		LM_CRIT("cannot allocate memory for lock.\n");
		return -1;
	}
	if (lock_init(lock) == 0) {
		LM_CRIT("cannot initialize lock.\n");
		return -1;
	}

	return 0;
}


static void destroy_shmlock(void)
{
	if (lock) {
		lock_destroy(lock);
		lock_dealloc((void *)lock);
		lock = NULL;
	}
}

static void dump_dtrie_mi(const struct dtrie_node_t *root,
	const unsigned int branches, char *prefix, int *length, struct mi_root *reply)
{
	struct mi_node *crt_node;
	unsigned int i;
	char digit, *val = NULL;
	int val_len = 0;

	/* Sanity check - should not reach here anyway */
	if (NULL == root) {
		LM_ERR("root dtrie is NULL\n");
		return ;
	}

        /* If data found, add a new node to the reply tree */
        if (root->data) {
		/* Create new node and add it to the roots's kids */
		if(!(crt_node = add_mi_node_child(&reply->node, MI_DUP_NAME, prefix,
				*length, 0, 0)) ) {
			LM_ERR("cannot add the child node to the tree\n");
			return ;
		}

		/* Resolve the value of the whitelist attribute */
		if (root->data == (void *)MARK_BLACKLIST) {
			val = int2str(0, &val_len);
		} else if (root->data == (void *)MARK_WHITELIST) {
			val = int2str(1, &val_len);
		}

		/* Add the attribute to the current node */
		if((add_mi_attr(crt_node, MI_DUP_VALUE,
				userblacklist_whitelist_col.s,
				userblacklist_whitelist_col.len,
				val, val_len)) == 0) {
			LM_ERR("cannot add attributes to the node\n");
			return ;
		}
        }

	/* Perform a DFS search */
        for (i = 0; i < branches; i++) {
                /* If child branch found, traverse it */
                if (root->child[i]) {
                        if (branches == 10) {
                                digit = i + '0';
                        } else {
                                digit = i;
                        }

                        /* Push digit in prefix stack */
			if (*length >= MAXNUMBERLEN + 1) {
				LM_ERR("prefix length exceeds %d\n", MAXNUMBERLEN + 1);
				return ;
			}
                        prefix[(*length)++] = digit;

                        /* Recursive DFS call */
	                dump_dtrie_mi(root->child[i], branches, prefix, length, reply);

                        /* Pop digit from prefix stack */
                        (*length)--;
                }
        }

        return ;
}


static struct mi_root * check_list_mi(struct mi_root* cmd, int list_type)
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
	if (NULL == node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if (NULL == node->value.s || node->value.len == 0)
		return init_mi_tree(400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	prefix = node->value;
	strncpy(req_prefix, prefix.s, prefix.len);
	req_prefix[prefix.len] = '\0';

	/* Check that just 1 argument is given */
	node = node->next;
	if (node)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* Check that global blacklist exists */
	if (!gnode) {
		LM_ERR("the global blacklist is NULL\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Check that reply tree is successfully initialized */
	tmp = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (!tmp) {
		LM_ERR("the MI tree cannot be initialized!\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Skip over non-digits. */
	ptr = req_prefix;
	while (match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	/* Avoids dirty reads when updating d-tree */
	lock_get(lock);
	nodeflags = dtrie_longest_match(gnode, ptr, strlen(ptr), NULL, match_mode);
	if (nodeflags) {
		if (*nodeflags == (void *)MARK_WHITELIST) {
			LM_DBG("prefix %.*s is whitelisted in table %.*s\n",
				prefix.len, prefix.s, globalblacklist_table.len, globalblacklist_table.s);
			ret = MARK_WHITELIST;
		} else if (*nodeflags == (void *)MARK_BLACKLIST) {
			LM_DBG("prefix %.*s is blacklisted in table %.*s\n",
				prefix.len, prefix.s, globalblacklist_table.len, globalblacklist_table.s);
			ret = MARK_BLACKLIST;
		}
	}
	else {
		LM_DBG("prefix %.*s not found in table %.*s\n",
			prefix.len, prefix.s, globalblacklist_table.len, globalblacklist_table.s);
	}
	lock_release(lock);

	/* Create new node and add it to the reply roots's kids */
	if(!(crt_node = add_mi_node_child(&tmp->node, MI_DUP_NAME,
			prefix.s, prefix.len, 0, 0)) ) {
		LM_ERR("cannot add the child node to the tree\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Resolve the value of the attribute to be returned */
	val.s = FALSE_S;
	val.len = FALSE_LEN;

	switch (list_type) {
		case MARK_WHITELIST:
                        attr.s = WHITELISTED_S;
                        attr.len = WHITELISTED_LEN;

			if (ret == MARK_WHITELIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			break;
		case MARK_BLACKLIST:
                        attr.s = BLACKLISTED_S;
                        attr.len = BLACKLISTED_LEN;

			if (ret == MARK_BLACKLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			break;
		default:
			LM_ERR("list_type not found\n");
			return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Add the attribute to the current node */
	if (!(crt_attr = add_mi_attr(crt_node, MI_DUP_VALUE,
			attr.s, attr.len, val.s, val.len))) {
		LM_ERR("cannot add attribute to the node\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	return tmp;
}


static struct mi_root * check_userlist_mi(struct mi_root* cmd, int list_type)
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
	if (NULL == node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if (NULL == node->value.s || node->value.len == 0)
		return init_mi_tree(400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	user = node->value;

	/* Get the domain name */
	node = node->next;
	if (NULL == node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if (NULL == node->value.s || node->value.len == 0)
		return init_mi_tree(400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	domain = node->value;

	/* Get the prefix number */
	node = node->next;
	if (node) {
		/* Got 3 params, the third one is the prefix */
		if (NULL == node->value.s || node->value.len == 0)
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
	if (node)
		node = node->next;
	if (node)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* Build userblacklist dtrie */
	table = userblacklist_table;
	LM_DBG("check entry %s for user %.*s@%.*s in table %.*s, use domain=%d\n",
		req_prefix, user.len, user.s, domain.len, domain.s,
		table.len, table.s, local_use_domain);
	if (db_build_userbl_tree(&user, &domain, &table, dtrie_root, local_use_domain) < 0) {
		LM_ERR("cannot build d-tree\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Check that reply tree is successfully initialized */
	tmp = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (!tmp) {
		LM_ERR("the MI tree cannot be initialized!\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Skip over non-digits. */
	ptr = req_prefix;
	while (match_mode == 10 && strlen(ptr) > 0 && !isdigit(*ptr)) {
		ptr = ptr + 1;
	}

	/* Search for a match in dtrie */
	nodeflags = dtrie_longest_match(dtrie_root, ptr, strlen(ptr), NULL, match_mode);
	if (nodeflags) {
		if (*nodeflags == (void *)MARK_WHITELIST) {
			LM_DBG("user %.*s is whitelisted for prefix %.*s in table %.*s\n",
				user.len, user.s, prefix.len, prefix.s, table.len, table.s);
			ret = MARK_WHITELIST;
		} else if (*nodeflags == (void *)MARK_BLACKLIST) {
			LM_DBG("user %.*s is blacklisted for prefix %.*s in table %.*s\n",
				user.len, user.s, prefix.len, prefix.s, table.len, table.s);
			ret = MARK_BLACKLIST;
		}
	} else {
		LM_DBG("user %.*s, prefix %.*s not found in table %.*s\n",
			user.len, user.s, prefix.len, prefix.s, table.len, table.s);
	}


	/* Create new node and add it to the reply roots's kids */
	if(!(crt_node = add_mi_node_child(&tmp->node, MI_DUP_NAME,
			prefix.s, prefix.len, 0, 0)) ) {
		LM_ERR("cannot add the child node to the tree\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Resolve the value of the attribute to be returned */
	val.s = FALSE_S;
	val.len = FALSE_LEN;

	switch (list_type) {
		case MARK_WHITELIST:
                        attr.s = WHITELISTED_S;
                        attr.len = WHITELISTED_LEN;

			if (ret == MARK_WHITELIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			break;
		case MARK_BLACKLIST:
                        attr.s = BLACKLISTED_S;
                        attr.len = BLACKLISTED_LEN;

			if (ret == MARK_BLACKLIST) {
				val.s = TRUE_S;
				val.len = TRUE_LEN;
			}

			break;
		default:
			LM_ERR("list_type not found\n");
			return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* Add the attribute to the current node */
	if (!(crt_attr = add_mi_attr(crt_node, MI_DUP_VALUE,
			attr.s, attr.len, val.s, val.len))) {
		LM_ERR("cannot add attribute to the node\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	return tmp;
}


struct mi_root * mi_reload_blacklist(struct mi_root* cmd, void* param)
{
	struct mi_root * tmp = NULL;
	if(reload_sources() == 0) {
		tmp = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	} else {
		tmp = init_mi_tree( 500, "cannot reload blacklist", 21);
	}

	return tmp;
}


struct mi_root * mi_dump_blacklist(struct mi_root* cmd, void* param)
{
	char prefix_buff[MAXNUMBERLEN + 1];
	int length = 0;
	struct mi_root *tmp = NULL;

	/* Check that global blacklist exists */
	if (!gnode) {
		LM_ERR("the global blacklist is NULL\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	tmp = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (!tmp) {
		LM_ERR("the MI tree cannot be initialized!\n");
		return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	dump_dtrie_mi(gnode, match_mode, prefix_buff, &length, tmp);

	return tmp;
}


struct mi_root * mi_check_blacklist(struct mi_root* cmd, void* param)
{
	return check_list_mi(cmd, MARK_BLACKLIST);
}


struct mi_root * mi_check_whitelist(struct mi_root* cmd, void* param)
{
	return check_list_mi(cmd, MARK_WHITELIST);
}


struct mi_root * mi_check_userblacklist(struct mi_root* cmd, void* param)
{
	return check_userlist_mi(cmd, MARK_BLACKLIST);
}


struct mi_root * mi_check_userwhitelist(struct mi_root* cmd, void* param)
{
	return check_userlist_mi(cmd, MARK_WHITELIST);
}


static int mod_init(void)
{
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if (userblacklist_db_init() != 0) return -1;
	if (init_shmlock() != 0) return -1;
	if (init_source_list() != 0) return -1;
	return 0;
}


static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	return mi_child_init();
}

static int userblacklist_child_initialized = 0;
static int blacklist_child_initialized = 0;

static int mi_child_init(void)
{
	/* global blacklist init */
	if (check_globalblacklist_fixup(NULL, 0) != 0) {
		LM_ERR("could not add global table when init the module");
	}

	/* user blacklist init */
	if(userblacklist_child_initialized)
		return 0;
	if (userblacklist_db_open() != 0) return -1;
	dtrie_root=dtrie_init(match_mode);
	if (dtrie_root == NULL) {
		LM_ERR("could not initialize data");
		return -1;
	}

	/* because we've added new sources during the fixup */
	if (reload_sources() != 0) return -1;

	userblacklist_child_initialized = 1;
	blacklist_child_initialized = 1;

	return 0;
}


static void mod_destroy(void)
{
	destroy_source_list();
	destroy_shmlock();
	userblacklist_db_close();
	dtrie_destroy(&dtrie_root, NULL, match_mode);
}
