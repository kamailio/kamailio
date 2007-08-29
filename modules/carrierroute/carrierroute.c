/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/**
 * @file carrierroute.c
 *
 * @author: Jonas Appel <jonas dot appel at schlund dot de>
 *
 * @date Mi Jan 24 2007
 *
 * Copyright: 2007 1 & 1 Internet AG
 *
 * @brief contains the functions exported by the moduls
 *
 */

#include "../../sr_module.h"
#include "../../str.h"
#include "../../dset.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../prime_hash.h"
#include "carrierroute.h"
#include "load_data.h"
#include "route_fifo.h"
#include "carrier_tree.h"
#include "route_func.h"

MODULE_VERSION

char * db_url = NULL;
char * db_table = "carrierroute";
char * subscriber_table = "subscriber";
char * carrier_table = "route_tree";

char * columns[COLUMN_NUM] = {
                                 "id",
                                 "carrier",
                                 "scan_prefix",
                                 "level",
                                 "prob",
                                 "rewrite_host",
                                 "strip",
                                 "rewrite_prefix",
                                 "rewrite_suffix",
                                 "comment"
                             };

char * subscriber_columns[SUBSCRIBER_COLUMN_NUM] = {
            "username",
            "domain",
            "cr_preferred_carrier"
        };

char * carrier_columns[CARRIER_COLUMN_NUM] = {
            "id",
            "carrier"
        };

char * config_source = "file";
char * config_file = CFG_DIR"carrierroute.conf";

char * default_tree = "default";

int mode = 0;
int use_domain = 0;

int fallback_default = 1;

/************* Declaration of Interface Functions **************************/
static int mod_init(void);
static int child_init(int);
static int mi_child_init(void);
static void mod_destroy(void);
static int route_fixup(void ** param, int param_no);
static int user_route_fixup(void ** param, int param_no);
static int tree_route_fixup(void ** param, int param_no);

/************* Declaration of Helper Functions *****************************/
static enum hash_source hash_fixup(const char * domain);
static int parse_avp_param(char *s, int *flags, int_str *name);

/************* Module Exports **********************************************/
static cmd_export_t cmds[]={
	{"cr_rewrite_uri",           route_uri,             2, route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_prime_balance_uri",     prime_balance_uri,     2, route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_rewrite_by_to",         route_by_to,           2, route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_prime_balance_by_to",   prime_balance_by_to,   2, route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_rewrite_by_from",       route_by_from,         2, route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_prime_balance_by_from", prime_balance_by_from, 2, route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_user_rewrite_uri",      user_route_uri,        2, user_route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_tree_rewrite_uri",      tree_route_uri,        2, tree_route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
// this function is only needed for the 0700 functionality, and obselete
// needs the add-rewrite-branches patch for the rewrite_branches function in core
#ifdef SP_ROUTE2_0700
	{"cr_rewrite_branches",      rewrite_branches,      2, route_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
#endif
	{0, 0, 0, 0, 0}
};

static param_export_t params[]= {
	{"db_url",                 STR_PARAM, &db_url },
	{"db_table",               STR_PARAM, &db_table },
	{"carrier_table",          STR_PARAM, &carrier_table },
	{"subscriber_table",       STR_PARAM, &subscriber_table },
	{"id_column",              STR_PARAM, &columns[COL_ID] },
	{"carrier_column",         STR_PARAM, &columns[COL_CARRIER] },
	{"scan_prefix_column",     STR_PARAM, &columns[COL_SCAN_PREFIX] },
	{"level_column",           STR_PARAM, &columns[COL_LEVEL] },
	{"prob_column",            STR_PARAM, &columns[COL_PROB] },
	{"rewrite_host_column",    STR_PARAM, &columns[COL_REWRITE_HOST] },
	{"strip_column",           STR_PARAM, &columns[COL_STRIP] },
	{"rewrite_prefix_column",  STR_PARAM, &columns[COL_REWRITE_PREFIX] },
	{"rewrite_suffix_column",  STR_PARAM, &columns[COL_REWRITE_SUFFIX] },
	{"comment_column",         STR_PARAM, &columns[COL_COMMENT] },
	{"subscriber_user_col",    STR_PARAM, &subscriber_columns[SUBSCRIBER_USERNAME_COL] },
	{"subscriber_domain_col",  STR_PARAM, &subscriber_columns[SUBSCRIBER_DOMAIN_COL] },
	{"subscriber_carrier_col", STR_PARAM, &subscriber_columns[SUBSCRIBER_CARRIER_COL] },
	{"carrier_id_col",         STR_PARAM, &carrier_columns[CARRIER_ID_COL] },
	{"carrier_name_col",       STR_PARAM, &carrier_columns[CARRIER_NAME_COL] },
	{"config_source",          STR_PARAM, &config_source },
	{"default_tree",           STR_PARAM, &default_tree },
	{"config_file",            STR_PARAM, &config_file },
	{"use_domain",             INT_PARAM, &use_domain },
	{"fallback_default",       INT_PARAM, &fallback_default },
	{0,0,0}
};

static mi_export_t mi_cmds[] = {
	{ "cr_reload_routes",   reload_fifo,     MI_NO_INPUT_FLAG, 0,  mi_child_init },
	{ "cr_dump_routes",     dump_fifo,       MI_NO_INPUT_FLAG, 0,  0 },
	{ "cr_replace_host",    replace_host,    0,                0,  0 },
	{ "cr_deactivate_host", deactivate_host, 0,                0,  0 },
	{ "cr_activate_host",   activate_host,   0,                0,  0 },
	{ "cr_add_host",        add_host,        0,                0,  0 },
	{ "cr_delete_host",     delete_host,     0,                0,  0 },
	{ 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"carrierroute",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Export parameters */
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* Module initialization function */
	0,          /* Response function */
	mod_destroy,/* Destroy function */
	child_init  /* Child initialization function */
};
/************* Interface Functions *****************************************/

/**
 * Initialises the module, i.e. it binds the necessary API functions
 * and registers the fifo commands
 *
 * @return 0 on success, -1 on failure
 */
static int mod_init(void) {
	if (init_route_data(config_source) < 0) {
		LM_ERR("could not init route data\n");
		return -1;
	}
	if (prepare_route_tree() == -1) {
		LM_ERR("could not prepare route tree\n");
		return -1;
	}
	if(data_main_finalize() < 0) {
		return -1;
	}
	LM_INFO("module initialized, pid [%d]\n", getpid());
	return 0;
}

/**
 * fixes the module functions' parameters, i.e. it maps
 * the routing domain names to numbers for faster access
 * at runtime
 *
 * @param param the parameter
 * @param param_no the number of the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int route_fixup(void ** param, int param_no) {
	enum hash_source my_hash_source;
	int domain;
	if (param_no == 1) {
		if ((domain = add_domain((char *)*param)) < 0) {
			return -1;
		}
		LM_NOTICE("domain %s has id %i\n", (char *)*param, domain);
		pkg_free(*param);
		*param = (void *)domain;
	} else if (param_no == 2) {
		if ((my_hash_source = hash_fixup((char *)*param)) == shs_error) {
			return -1;
		}
		pkg_free(*param);
		*param = (void *)my_hash_source;
	}
	return 0;
}

static int user_route_fixup(void ** param, int param_no) {
	int domain;
	void* ptr;
	user_param_t * gcp=NULL;
	if (mode == SP_ROUTE_MODE_FILE) {
		LM_ERR("command cr_user_rewrite_uri can't be used in file mode\n");
		return -1;
	}
	if (param_no == 1) {
		ptr = *param;

		gcp = (user_param_t *)pkg_malloc(sizeof(user_param_t));
		if (gcp == NULL) {
			LM_ERR("out of private memory\n");
			return -1;
		}
		memset(gcp, 0, sizeof(user_param_t));

		if (!strcasecmp((char*)*param, "Request-URI")) {
			gcp->id=REQ_URI;
		} else if (!strcasecmp((char*)*param, "To")) {
			gcp->id=TO_URI;
		} else if (!strcasecmp((char*)*param, "From")) {
			gcp->id=FROM_URI;
		} else if (!strcasecmp((char*)*param, "Credentials")) {
			gcp->id=CREDENTIALS;
		} else {
			if (parse_avp_param((char*)*param, &gcp->avp_flags, &gcp->avp_name)<0) {
				LM_ERR("Unsupported Header Field identifier\n");
				pkg_free(gcp);
				return -1;
			}
			gcp->id=AVP;
		}
		*param=(void*)gcp;
		if (gcp->id!=5) pkg_free(ptr);
	} else if (param_no == 2) {
		if ((domain = add_domain((char *)*param)) < 0) {
			return -1;
		}
		LM_NOTICE("domain %s has id %i\n", (char *)*param, domain);
		pkg_free(*param);
		*param = (void *)domain;
	}
	return 0;
}

static int tree_route_fixup(void ** param, int param_no) {
	int tree, domain;
	if (param_no == 1) {
		if ((tree = find_tree((char *)*param)) < 0) {
			LM_ERR("could not find tree\n");
			return -1;
		}
		LM_NOTICE("tree %s has id %i\n", (char *)*param, tree);
		pkg_free(*param);
		*param = (void *)tree;
	} else if (param_no == 2) {
		if ((domain = add_domain((char *)*param)) < 0) {
			LM_ERR("could not add domain\n");
			return -1;
		}
		LM_NOTICE("domain %s has id %i\n", (char *)*param, domain);
		pkg_free(*param);
		*param = (void *)domain;
	}
	return 0;
}

static int child_init(int rank) {
	return data_child_init();
}

static int mi_child_init(void) {
	return data_child_init();
}

static void mod_destroy(void) {
	destroy_route_data();
}


/************* Helper Functions ********************************************/

/**
 * Fixes the hash source to enum values
 *
 * @param hash_source the hash source as string
 *
 * @return the enum value on success, -1 on failure
 */
static enum hash_source hash_fixup(const char * my_hash_source) {
	if (strcasecmp("call_id", my_hash_source) == 0) {
		return shs_call_id;
	} else if (strcasecmp("from_uri", my_hash_source) == 0) {
		return shs_from_uri;
	} else if (strcasecmp("from_user", my_hash_source) == 0) {
		return shs_from_user;
	} else if (strcasecmp("to_uri", my_hash_source) == 0) {
		return shs_to_uri;
	} else if (strcasecmp("to_user", my_hash_source) == 0) {
		return shs_to_user;
	} else {
		LM_ERR("Invalid second parameter "
		    "to balance_uri().\n");
		return shs_error;
	}
}

static int parse_avp_param(char *s, int *flags, int_str *name) {
	unsigned int uint;
	int len = strlen(s);
	str name_str;

	if (s==0 || len==0)
		return -1;

	if (*(s+1)==':') {
		if (*s=='i' || *s=='I')
			*flags = 0;
		else if (*s=='s' || *s=='S')
			*flags = AVP_NAME_STR;
		else {
			LM_ERR("unknown value type <%c>\n",*s);
			return -1;
		}
		s += 2;
		len -= 2;
		if (*s==0 || len<=0) {
			LM_ERR("parse error\n");
			return -1;
		}
	} else {
		*flags = AVP_NAME_STR;
	}
	/* get the value */
	name_str.s = s;
	name_str.len = len;
	if ((*flags&AVP_NAME_STR)==0) {
		/* convert the value to integer */
		if (str2int(&name_str, &uint)==-1) {
			LM_ERR("value is not int as type says <%.*s>\n",
				name_str.len, name_str.s);
			return -1;
		}
		name->n = (int)uint;
	} else { // changed struct int_str, not more a pointer to a "str"
		name->s.s = pkg_malloc(name_str.len);
		if (!name->s.s) {
			LM_ERR("out of private memory\n");
			pkg_free(name->s.s);
			return -1;
		}

		memcpy(name->s.s, name_str.s, name_str.len);
		name->s.len=name_str.len;
	}

	return 0;
}
