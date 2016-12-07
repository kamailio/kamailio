/*
 * $Id$
 *
 * PERMISSIONS module
 *
 * Copyright (C) 2003 Miklós Tirpák (mtirpak@sztaki.hu)
 * Copyright (C) 2003 iptel.org
 * Copyright (C) 2003 Juha Heinanen (jh@tutpro.com)
 * Copyright (C) 2006 iptelorg GmbH
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
 * --------
 *   2006-08-10: file operation functions are moved to allow_files.c
 *               memory is not allocated for file containers if not needed
 *               safe_file_load module parameter introduced (Miklos)
 *   2006-08-14: child processes do not keep the DB connection open
 *               if cache is enabled (Miklos)
 *   2008-07-xx: added ip_is_trusted function (tma)
 *   2008-08-01: added ipset manipulable via RPC (tma)
 */

#include <stdio.h>
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "permissions.h"
#include "trusted.h"
#include "allow_files.h"
#include "ipmatch.h"
#include "im_db.h"
#include "permissions_rpc.h"
#include "ip_set.h"
#include "../../resolve.h"

MODULE_VERSION

rule_file_t	*allow = NULL;	/* Parsed allow files */
rule_file_t	*deny = NULL;	/* Parsed deny files */
static int	allow_rules_num = 0;	/* Number of parsed allow files (excluding default file) */
static int	deny_rules_num = 0;	/* Number of parsed deny files (excluding default file) */

/* Module parameter variables */
/* general parameters */
char* db_url = 0;                  /* Don't connect to the database by default */
int db_mode = DISABLE_CACHE;	   /* Database usage mode: 0=no cache, 1=cache */
int max_rule_files = MAX_RULE_FILES;	/* maximum nuber of allowed allow/deny file pairs */

/* parameters for allow_* functions */
static char* default_allow_file = DEFAULT_ALLOW_FILE;
static char* default_deny_file = DEFAULT_DENY_FILE;
static char* allow_suffix = ".allow";
static char* deny_suffix = ".deny";
int check_all_branches = 1;	/* By default we check all branches */
int safe_file_load = 0;

/* for allow_trusted function */
char* trusted_table = "trusted";   /* Name of trusted table */
char* source_col = "src_ip";       /* Name of source address column */
char* proto_col = "proto";         /* Name of protocol column */
char* from_col = "from_pattern";   /* Name of from pattern column */

/* parameters for ipmatch functions */
char	*ipmatch_table = "ipmatch";

/* Database API */
db_ctx_t	*db_conn = NULL;

static str *ip_set_list_names = NULL;    /* declared names */
static struct ip_set_ref **ip_set_list_local = NULL;  /* local copy of ip set in shared memory */
static int ip_set_list_count = 0;  /* number of declared names */

/* fixup function prototypes */
static int fixup_files_1(void** param, int param_no);
static int fixup_files_2(void** param, int param_no);
static int fixup_ip_is_trusted(void** param, int param_no);
static int fixup_w_im(void **, int);
static int fixup_w_im_onsend(void **, int);
static int fixup_param_declare_ip_set( modparam_t type, void* val);

/* module function prototypes */
static int allow_routing_0(struct sip_msg* msg, char* str1, char* str2);
static int allow_routing_1(struct sip_msg* msg, char* basename, char* str2);
static int allow_routing_2(struct sip_msg* msg, char* allow_file, char* deny_file);
static int allow_register_1(struct sip_msg* msg, char* basename, char* s);
static int allow_register_2(struct sip_msg* msg, char* allow_file, char* deny_file);
static int allow_refer_to_1(struct sip_msg* msg, char* basename, char* s);
static int allow_refer_to_2(struct sip_msg* msg, char* allow_file, char* deny_file);
int w_im_2(struct sip_msg *msg, char *str1, char *str2);
int w_im_1(struct sip_msg *msg, char *str1, char *str2);
int w_im_onsend(struct sip_msg *msg, char *str1, char *str2);
int w_im_filter(struct sip_msg *msg, char *str1, char *str2);
static int w_ip_is_trusted(struct sip_msg *msg, char *str1, char *str2);


/* module interface function prototypes */
static int mod_init(void);
static void mod_exit(void);
static int child_init(int rank);


/* Exported functions */
static cmd_export_t cmds[] = {
        {"allow_routing",  allow_routing_0,  0, 0,              REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_routing",  allow_routing_1,  1, fixup_files_1,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_routing",  allow_routing_2,  2, fixup_files_2,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_register", allow_register_1, 1, fixup_files_1,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_register", allow_register_2, 2, fixup_files_2,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_refer_to", allow_refer_to_1, 1, fixup_files_1,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_refer_to", allow_refer_to_2, 2, fixup_files_2,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_trusted",  allow_trusted,    0, 0,              REQUEST_ROUTE | FAILURE_ROUTE},
	{"ipmatch",        w_im_1,           1, fixup_w_im,     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
	{"ipmatch",        w_im_2,           2, fixup_w_im,     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
	{"ipmatch_onsend", w_im_onsend,      1, fixup_w_im_onsend, ONSEND_ROUTE },
	{"ipmatch_filter", w_im_filter,      1, fixup_int_1,     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | ONSEND_ROUTE},
	{"ip_is_trusted",  w_ip_is_trusted,  2, fixup_ip_is_trusted, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | ONSEND_ROUTE | BRANCH_ROUTE},
	{"ip_is_in_ipset", w_ip_is_trusted,  2, fixup_ip_is_trusted, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | ONSEND_ROUTE | BRANCH_ROUTE},
	       	       
        {0, 0, 0, 0, 0}
};

/* Exported parameters */
static param_export_t params[] = {
	{"db_url",             PARAM_STRING, &db_url            },
	{"db_mode",            PARAM_INT,    &db_mode           },
        {"default_allow_file", PARAM_STRING, &default_allow_file},
        {"default_deny_file",  PARAM_STRING, &default_deny_file },
	{"check_all_branches", PARAM_INT,    &check_all_branches},
	{"allow_suffix",       PARAM_STRING, &allow_suffix      },
	{"deny_suffix",        PARAM_STRING, &deny_suffix       },
	{"max_rule_files",     PARAM_INT,    &max_rule_files    },
        {"safe_file_load",     PARAM_INT,    &safe_file_load    },
	{"trusted_table",      PARAM_STRING, &trusted_table     },
	{"source_col",         PARAM_STRING, &source_col        },
	{"proto_col",          PARAM_STRING, &proto_col         },
	{"from_col",           PARAM_STRING, &from_col          },
	{"ipmatch_table",      PARAM_STRING, &ipmatch_table     },
	{"declare_ipset",      PARAM_STRING|PARAM_USE_FUNC, fixup_param_declare_ip_set},
	
        {0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
        "permissions",
        cmds,      /* Exported functions */
	permissions_rpc, /* RPC methods */
        params,    /* Exported parameters */
        mod_init,  /* module initialization function */
        0,         /* response function */
        mod_exit,  /* destroy function */
        0,         /* oncancel function */
        child_init /* child initialization function */
};

/*
 * Convert the name of the files into table index
 */
static int fixup_files_2(void** param, int param_no)
{
	int idx;

	if (param_no == 1) {
		idx = load_file(*param, &allow, &allow_rules_num, 0);
	} else if( param_no == 2) {
		idx = load_file(*param, &deny, &deny_rules_num, 0);
	} else {
		return 0;
	}
	if (idx < 0) return -1;

	pkg_free(*param);
	*param = (void*)(long)idx;
	return 0;
}


/*
 * Convert the name of the file into table index
 */
static int fixup_files_1(void** param, int param_no)
{
	char* buffer;
	int param_len, suffix_len;
	int	idx1, idx2;

	if (param_no != 1) return 0;

	param_len = strlen((char*)*param);
	if (strlen(allow_suffix) > strlen(deny_suffix)) {
		suffix_len = strlen(allow_suffix);
	} else {
		suffix_len = strlen(deny_suffix);
	}

	buffer = pkg_malloc(param_len + suffix_len + 1);
	if (!buffer) {
		LOG(L_ERR, "fixup_files_1(): No memory left\n");
		return -1;
	}

	strcpy(buffer, (char*)*param);
	strcat(buffer, allow_suffix);

	/* load allow file */
	idx1 = load_file(buffer, &allow, &allow_rules_num, 0);
	if (idx1 < 0) {
		pkg_free(buffer);
		return -1;
	}

	strcpy(buffer + param_len, deny_suffix);
	idx2 = load_file(buffer, &deny, &deny_rules_num, 0);
	if (idx2 < 0) {
		pkg_free(buffer);
		return -1;
	}

	if (idx1 != idx2) {
		LOG(L_ERR, "fixup_files_1(): allow and deny indexes are not equal!\n");
		pkg_free(buffer);
		return -1;
	}

	pkg_free(*param);
	*param = (void*)(long)idx1;
	pkg_free(buffer);
	return 0;
}

/* connect to the database */
static int perm_init_db(void)
{
	db_conn = db_ctx("permissions");
	if (db_conn == NULL) {
		LOG(L_ERR, "perm_init_db(): Unable to create database context\n");
		return -1;
	}
	if (db_add_db(db_conn, db_url) < 0) {
		LOG(L_ERR, "perm_init_db(): cannot add the url to database context\n");
		return -1;
	}
	if (db_connect(db_conn) < 0) {
		LOG(L_ERR, "perm_init_db(): Unable to connect to database\n");
		return -1;
	}
	return 0;
}

/* destroy the DB connection */
static void perm_destroy_db(void)
{
	if (db_conn) {
		db_disconnect(db_conn);
		db_ctx_free(db_conn);
		db_conn = NULL;
	}
}

/*
 * module initialization function
 */
static int mod_init(void)
{
	LOG(L_INFO, "permissions - initializing\n");


	/* do not load the files if not necessary */
	if (strlen(default_allow_file) || strlen(default_deny_file)) {
		if (load_file(default_allow_file, &allow, &allow_rules_num, 1) != 0) goto error;
		if (load_file(default_deny_file, &deny, &deny_rules_num, 1) != 0) goto error;
	}

	if (db_url && (db_mode == ENABLE_CACHE)) {
		/* database backend is enabled, and cache is requested -- load the DB */
		if (perm_init_db()) goto error;

		/* prepare DB commands for trusted table */
		if (init_trusted_db()) {
			LOG(L_ERR, "Error while preparing DB commands for trusted table\n");
			goto error;
		}

		/* init trusted tables */
		if (init_trusted() != 0) {
			LOG(L_ERR, "Error while initializing allow_trusted function\n");
			goto error;
		}

		/* prepare DB commands for ipmatch table */
		if (init_im_db()) {
			LOG(L_ERR, "Error while preparing DB commands for ipmatch table\n");
			goto error;
		}

		/* init ipmatch table */
		if (init_ipmatch() != 0) {
			LOG(L_ERR, "Error while initializing ipmatch table\n");
			goto error;
		}
		
		/* Destory DB connection, we do not need it anymore,
		each child process will create its own connection */
		destroy_trusted_db();
		destroy_im_db();
		perm_destroy_db();
	}

	if (ip_set_list_malloc(ip_set_list_count, ip_set_list_names) < 0) goto error;
	if (ip_set_list_count > 0) {
		ip_set_list_local = pkg_malloc(ip_set_list_count*sizeof(*ip_set_list_local));
		if (!ip_set_list_local) goto error;
		memset(ip_set_list_local, 0, sizeof(*ip_set_list_local)*ip_set_list_count);
	}
	if (ip_set_list_names) 
		pkg_free(ip_set_list_names);  /* we need not longer names in pkg memory */

	return 0;

error:
	/* free file containers */
	delete_files(&allow, allow_rules_num);
	delete_files(&deny, deny_rules_num);

	/* destroy DB cmds */
	destroy_trusted_db();
	destroy_im_db();

	/* destory DB connection */
	perm_destroy_db();

	/* free the cache */
	clean_trusted();
	clean_ipmatch();

	ip_set_list_free();

	return -1;
}

static int child_init(int rank)
{
	if ((rank <= 0) && (rank != PROC_RPC) && (rank != PROC_UNIXSOCK))
		return 0;

	if (db_url) {
		/* Connect to the DB regarless of cache or non-cache mode,
		because we either have to query the DB runtime, or reload
		the cache via RPC call */
		if (perm_init_db()) goto error;

		/* prepare DB commands for trusted tables */
		if (init_trusted_db()) {
			LOG(L_ERR, "Error while preparing DB commands for trusted table\n");
			goto error;
		}

		/* prepare DB commands for ipmatch tables */
		if (init_im_db()) {
			LOG(L_ERR, "Error while preparing DB commands for ipmatch table\n");
			goto error;
		}
	}
	return 0;

error:
	/* destroy DB cmds */
	destroy_trusted_db();
	destroy_im_db();

	/* destroy DB connection */
	perm_destroy_db();

	return -1;
}

/*
 * destroy function
 */
static void mod_exit(void)
{
	int i;
	/* free file containers */
	delete_files(&allow, allow_rules_num);
	delete_files(&deny, deny_rules_num);

	clean_trusted();
	clean_ipmatch();

	if (ip_set_list_local) {
		for (i=0; i<ip_set_list_count; i++) {
			/* we need delete all cloned sets because might not exist in global list after commit, they have refcnt>1 */
			if (ip_set_list_local[i])
				shm_free(ip_set_list_local[i]);
		}
	}
	ip_set_list_free();
}


/*
 * Uses default rule files from the module parameters
 */
int allow_routing_0(struct sip_msg* msg, char* str1, char* str2)
{
	return check_routing(msg, 0);
}


int allow_routing_1(struct sip_msg* msg, char* basename, char* s)
{
	return check_routing(msg, (int)(long)basename);
}


/*
 * Accepts allow and deny files as parameters
 */
int allow_routing_2(struct sip_msg* msg, char* allow_file, char* deny_file)
{
	     /* Index converted by fixup_files_2 */
	return check_routing(msg, (int)(long)allow_file);
}

int allow_register_1(struct sip_msg* msg, char* basename, char* s)
{
	return check_register(msg, (int)(long)basename);
}


int allow_register_2(struct sip_msg* msg, char* allow_file, char* deny_file)
{
	return check_register(msg, (int)(long)allow_file);
}

int allow_refer_to_1(struct sip_msg* msg, char* basename, char* s)
{
	return check_refer_to(msg, (int)(long)basename);
}


int allow_refer_to_2(struct sip_msg* msg, char* allow_file, char* deny_file)
{
	return check_refer_to(msg, (int)(long)allow_file);
}

/* fixup function for w_ipmatch_* */
static int fixup_w_im(void **param, int param_no)
{
	int	ret;
	str	*s;

	if (param_no == 1) {
		ret = fix_param(FPARAM_AVP, param);
		if (ret <= 0) return ret;
		ret = fix_param(FPARAM_SELECT, param);
		if (ret <= 0) return ret;
		ret = fix_param(FPARAM_STR, param);
		if (ret == 0) {
			s = &((fparam_t *)*param)->v.str;
			if ((s->len == 3) && (memcmp(s->s, "src", 3) == 0)) return 0;
			if ((s->len == 4) && (memcmp(s->s, "via2", 4) == 0)) return 0;

			LOG(L_ERR, "ERROR: fixup_w_im(): unknown string parameter\n");
			return -1;

		} else if (ret < 0) {
			return ret;
		}

		LOG(L_ERR, "ERROR: fixup_w_im(): unknown parameter type\n");
		return -1;

	} else if (param_no == 2) {
		if (fix_param(FPARAM_AVP, param) != 0) {
			LOG(L_ERR, "ERROR: fixup_w_im(): unknown AVP identifier: %s\n", (char*)*param);
			return -1;
		}
		return 0;
	}

	return 0;
}

/* fixup function for w_ipmatch_onsend */
static int fixup_w_im_onsend(void **param, int param_no)
{
	char	*ch;

	if (param_no == 1) {
		ch = (char *)*param;
		if ((ch[0] != 'd') && (ch[0] != 'r')) {
			LOG(L_ERR, "ERROR: fixup_w_im_onsend(): unknown string parameter\n");
			return -1;
		}
		return 0;
	}

	return 0;
}

/* wrapper function for ipmatch */
int w_im_2(struct sip_msg *msg, char *str1, char *str2)
{
	if (db_mode != ENABLE_CACHE) {
		LOG(L_ERR, "ERROR: w_im_2(): ipmatch function supports only cache mode, set db_mode module parameter!\n");
		return -1;
	}

	return ipmatch_2(msg, str1, str2);
}

/* wrapper function for ipmatch */
int w_im_1(struct sip_msg *msg, char *str1, char *str2)
{
	if (db_mode != ENABLE_CACHE) {
		LOG(L_ERR, "ERROR: w_im_1(): ipmatch function supports only cache mode, set db_mode module parameter!\n");
		return -1;
	}

	return ipmatch_1(msg, str1, str2);
}

/* wrapper function for ipmatch */
int w_im_onsend(struct sip_msg *msg, char *str1, char *str2)
{
	if (db_mode != ENABLE_CACHE) {
		LOG(L_ERR, "ERROR: w_im_onsend(): ipmatch function supports only cache mode, set db_mode module parameter!\n");
		return -1;
	}

	return ipmatch_onsend(msg, str1, str2);
}

/* wrapper function for ipmatch_filter */
int w_im_filter(struct sip_msg *msg, char *str1, char *str2)
{
	if (db_mode != ENABLE_CACHE) {
		LOG(L_ERR, "ERROR: w_im_filter(): ipmatch function supports only cache mode, set db_mode module parameter!\n");
		return -1;
	}

	return ipmatch_filter(msg, str1, str2);
}

struct ip_set_param {
	enum {IP_SET_PARAM_KIND_GLOBAL, IP_SET_PARAM_KIND_LOCAL} kind;
	union {
		struct {
			str s;
			unsigned int sz;
			struct ip_set ip_set;
			fparam_t *fparam;
		} local;
		struct {
			struct ip_set_list_item *ip_set;
		} global;			
	}u;
};

#define MODULE_NAME "permissions"

static inline int is_ip_set_name(str *s) {
	return (s->len && ((s->s[0] >= 'A' && s->s[0] <= 'Z') || (s->s[0] >= 'a' && s->s[0] <= 'z') || s->s[0] == '_'));
}


static int fixup_param_declare_ip_set( modparam_t type, void* val) {
	str *p;
	int i;
	str s;
	s.s = val;
	s.len = strlen(s.s);
	for (i=0; i<s.len && s.s[i]!='='; i++);
	s.len = i;
	
	for (i=0; i<ip_set_list_count; i++) {
		if (ip_set_list_names[i].len>=s.len && memcmp(val, ip_set_list_names[i].s, s.len) == 0) {
			ERR(MODULE_NAME": declare_ip_set: ip set '%.*s' already exists\n", s.len, s.s);
			return E_CFG;
		}
	}
	if (!is_ip_set_name(&s)) {
		ERR(MODULE_NAME": declare_ip_set: ip set '%.*s' is not correct identifier\n", s.len, s.s);
		return E_CFG;
	}
	s.len = strlen(s.s);
	p = pkg_realloc(ip_set_list_names, sizeof(*p)*(ip_set_list_count+1));
	if (!p) return E_OUT_OF_MEM;
	p[ip_set_list_count] = s;
	ip_set_list_count++;
	ip_set_list_names = p;
	return E_OK;
};
	

static int w_ip_is_trusted(struct sip_msg* msg, char* _ip_set, char* _ip) {
	str ip_set_s, ip_s;
	struct ip_addr *ip, ip_buf;
	struct ip_set new_ip_set, *ip_set;
	struct ip_set_list_item *isli = NULL;
	int kind;
	kind = ((struct ip_set_param*)_ip_set)->kind;
	if (kind == IP_SET_PARAM_KIND_LOCAL) {
		if (get_str_fparam(&ip_set_s, msg, ((struct ip_set_param*)_ip_set)->u.local.fparam) < 0) {
		    ERR(MODULE_NAME": ip_is_trusted: Error while obtaining ip_set parameter value\n");
			return -1;
		}
		if (is_ip_set_name(&ip_set_s)) {
			isli = ip_set_list_find_by_name(ip_set_s);
			if (!isli) {
				ERR(MODULE_NAME": ip_is_trusted: ip set '%.*s' is not declared\n", ip_set_s.len, ip_set_s.s);
				return -1;
			}
			kind = IP_SET_PARAM_KIND_GLOBAL;
			goto force_global;
		}		
		ip_set = &((struct ip_set_param*)_ip_set)->u.local.ip_set;
	}
	else {
		isli = ((struct ip_set_param*)_ip_set)->u.global.ip_set;
	force_global:
		if (!isli->ip_set) return -1; /* empty ip set */
		
		if (unlikely(isli->ip_set != ip_set_list_local[isli->idx])) {   /* global ip set has changed ? */
			if (ip_set_list_local[isli->idx]) {
				if (atomic_dec_and_test(&ip_set_list_local[isli->idx]->refcnt)) {
					ip_set_destroy(&ip_set_list_local[isli->idx]->ip_set);
					shm_free(ip_set_list_local[isli->idx]);
					ip_set_list_local[isli->idx] = NULL;
				}
			}
			lock_get(&isli->read_lock);			
			atomic_inc(&isli->ip_set->refcnt);
			ip_set_list_local[isli->idx] = isli->ip_set;
			lock_release(&isli->read_lock);
		}
		ip_set = &ip_set_list_local[isli->idx]->ip_set;
	}
	
	if (get_str_fparam(&ip_s, msg, (fparam_t*)_ip) < 0) {
	    ERR(MODULE_NAME": ip_is_trusted: Error while obtaining ip parameter value\n");
	    return -1;
	}
	if (!ip_s.len || !ip_set_s.len) return -1;
	switch (ip_s.s[0]) {
		case 's':	/* src */
		case 'S':
			ip = &msg->rcv.src_ip;
			break;
		case 'd':	/* dst */
		case 'D':
			ip = &msg->rcv.dst_ip;
			break;
		case 'r':	/* rcv */
		case 'R':
			ip = &msg->rcv.bind_address->address;
			break;			
		default:
			/* string -> ip */

			if ( ((ip = str2ip(&ip_s))==0)
			                  && ((ip = str2ip6(&ip_s))==0)
							                  ){
				ERR(MODULE_NAME": ip_is_trusted: string to ip conversion error '%.*s'\n", ip_s.len, ip_s.s);
				return -1;
			}
			ip_buf = *ip;
			ip = &ip_buf;  /* value has been in static buffer */			
			break;
	}

	/* test if ip_set string has changed since last call */
	if (kind == IP_SET_PARAM_KIND_LOCAL) {
		if (((struct ip_set_param*)_ip_set)->u.local.s.len != ip_set_s.len || 
			memcmp(((struct ip_set_param*)_ip_set)->u.local.s.s, ip_set_s.s, ip_set_s.len) != 0) {

			ip_set_init(&new_ip_set, 0);
			if (ip_set_add_list(&new_ip_set, ip_set_s) < 0) {
				ip_set_destroy(&new_ip_set);
				return -1;
			};
			if (((struct ip_set_param*)_ip_set)->u.local.sz < ip_set_s.len) {
				void *p;
				p = pkg_realloc(((struct ip_set_param*)_ip_set)->u.local.s.s, ip_set_s.len);
				if (!p) {
					ip_set_destroy(&new_ip_set);
					return E_OUT_OF_MEM;
				}
				((struct ip_set_param*)_ip_set)->u.local.s.s = p;			
				((struct ip_set_param*)_ip_set)->u.local.sz = ip_set_s.len;			
			}
			memcpy(((struct ip_set_param*)_ip_set)->u.local.s.s, ip_set_s.s, ip_set_s.len);
			((struct ip_set_param*)_ip_set)->u.local.s.len = ip_set_s.len;
			ip_set_destroy(&((struct ip_set_param*)_ip_set)->u.local.ip_set);
			((struct ip_set_param*)_ip_set)->u.local.ip_set = new_ip_set;
		}
	}
/* ip_set_print(stderr, &ip_set); */
	switch (ip_set_ip_exists(ip_set, ip)) {
		case IP_TREE_FIND_FOUND:
		case IP_TREE_FIND_FOUND_UPPER_SET:
			return 1;
		default:
			return -1;
	}
}

static int fixup_ip_is_trusted(void** param, int param_no) {
	int ret = E_CFG;
	struct ip_set_param *p;
	str s;
	if (param_no == 1) {
		
		p = pkg_malloc(sizeof(*p));
		if (!p) return E_OUT_OF_MEM;
		memset(p, 0, sizeof(*p));
		s.s = *param;
		s.len = strlen(s.s);

		if (is_ip_set_name(&s)) {
			p->u.global.ip_set = ip_set_list_find_by_name(s);
			if (!p->u.global.ip_set) {
				ERR(MODULE_NAME": fixup_ip_is_trusted: ip set '%.*s' is not declared\n", s.len, s.s);			
				goto err;
			}
			p->kind = IP_SET_PARAM_KIND_GLOBAL;
		} else {
			ret = fixup_var_str_12(param, param_no);
			if (ret < 0) goto err;
			ip_set_init(&p->u.local.ip_set, 0);
			p->u.local.fparam = *param;
			*param = p;
			p->kind = IP_SET_PARAM_KIND_LOCAL;
		}
	}
	else {
		return fixup_var_str_12(param, param_no);
	
	}
	return E_OK;
err:
	pkg_free(p);
	return ret;
}


