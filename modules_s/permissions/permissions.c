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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *   2006-08-10: file operation functions are moved to allow_files.c
 *               memory is not allocated for file containers if not needed
 *               safe_file_load module parameter introduced (Miklos)
 *   2006-08-14: child processes do not keep the DB connection open
 *               if cache is enabled (Miklos)
 */

#include <stdio.h>
#include "../../mem/mem.h"
#include "permissions.h"
#include "trusted.h"
#include "allow_files.h"
#include "ipmatch.h"
#include "permissions_rpc.h"

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
db_func_t	perm_dbf;
db_con_t	*db_handle = 0;

#define TRUSTED_TABLE_VERSION	1
#define IPMATCH_TABLE_VERSION	1

/* fixup function prototypes */
static int fixup_files_1(void** param, int param_no);
static int fixup_files_2(void** param, int param_no);
static int fixup_w_im(void **, int);
static int fixup_w_im_onsend(void **, int);

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

/* checks if the DB table is up-to-date */
static int check_table_version(char *_name, int _version) {
	str	db_table;
	int	ver;

	db_table.s = _name;
	db_table.len = strlen(_name);
	ver = table_version(&perm_dbf, db_handle, &db_table);

	if (ver <= 0) {
		LOG(L_ERR, "ERROR: Error while quering version for sql table '%s'\n", _name);
		return -1;
	}
	if (ver < _version) {
		LOG(L_ERR, "ERROR: version of sql table '%s' is invalid (%d instead of %d)" \
			", update table structure!\n", _name, ver, _version);
		return -1;
	}
	return 0;
}

/*
 * module initialization function
 */
static int mod_init(void)
{
	LOG(L_INFO, "permissions - initializing\n");

	/* do not load the files if not necessary */
	if (strlen(default_allow_file) || strlen(default_deny_file)) {
		if (load_file(default_allow_file, &allow, &allow_rules_num, 1) != 0) return -1;
		if (load_file(default_deny_file, &deny, &deny_rules_num, 1) != 0) return -1;
	}

	if (db_url) {
		/* database backend is enabled */
		if (bind_dbmod(db_url, &perm_dbf)) {
			LOG(L_ERR, "ERROR: unable to bind database module, load it first!\n");
			return -1;
		}
		db_handle = perm_dbf.init(db_url);
		if (!db_handle) {
			LOG(L_ERR, "ERROR: Unable to connect to database\n");
			return -1;
		}

		/* check trusted table version */
		if (check_table_version(trusted_table, TRUSTED_TABLE_VERSION)) {
			perm_dbf.close(db_handle);
			return -1;
		}

		/* check ipmatch table version */
		if (check_table_version(ipmatch_table, IPMATCH_TABLE_VERSION)) {
			perm_dbf.close(db_handle);
			return -1;
		}

		/* init trusted tables */
		if (init_trusted() != 0) {
			LOG(L_ERR, "Error while initializing allow_trusted function\n");
			perm_dbf.close(db_handle);
			return -1;
		}

		/* init ipmatch table */
		if (init_ipmatch() != 0) {
			LOG(L_ERR, "Error while initializing ipmatch table\n");
			perm_dbf.close(db_handle);
			return -1;
		}
		
		perm_dbf.close(db_handle);
		db_handle = 0;
	}

	return 0;
}

static int child_init(int rank)
{
	if (rank <= 0) return 0;

	if (db_url && (db_mode == DISABLE_CACHE)) {
		/* we need DB connection for each child */
		db_handle = perm_dbf.init(db_url);
		if (!db_handle) {
			LOG(L_CRIT, "ERROR: Unable to connect to database\n");
			return -1;
		}
	}
	return 0;
}

/*
 * destroy function
 */
static void mod_exit(void)
{
	/* free file containers */
	delete_files(&allow, allow_rules_num);
	delete_files(&deny, deny_rules_num);

	clean_trusted();
	clean_ipmatch();
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
