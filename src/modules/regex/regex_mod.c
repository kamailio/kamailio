/*
 * regex module - pcre operations
 *
 * Copyright (C) 2008 Iñaki Baz Castillo
 * Copyright (C) 2023 Victor Seva
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

/*!
 * \file
 * \brief REGEX :: Perl-compatible regular expressions using PCRE library
 * Copyright (C) 2008 Iñaki Baz Castillo
 * Copyright (C) 2023 Victor Seva
 * \ingroup regex
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/pt.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/str.h"
#include "../../core/locking.h"
#include "../../core/mod_fix.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

MODULE_VERSION

#define START 0
#define RELOAD 1

#define FILE_MAX_LINE 500	/*!< Max line size in the file */
#define MAX_GROUPS 20		/*!< Max number of groups */
#define GROUP_MAX_SIZE 8192 /*!< Max size of a group */


static int regex_init_rpc(void);

/*
 * Locking variables
 */
gen_lock_t *reload_lock;


/*
 * Module exported parameter variables
 */
static char *file = NULL;
static int max_groups = MAX_GROUPS;
static int group_max_size = GROUP_MAX_SIZE;
static int pcre_caseless = 0;
static int pcre_multiline = 0;
static int pcre_dotall = 0;
static int pcre_extended = 0;


/*
 * Module internal parameter variables
 */
static pcre2_general_context *pcres_gctx = NULL;
static pcre2_match_context *pcres_mctx = NULL;
static pcre2_compile_context *pcres_ctx = NULL;
static pcre2_code **pcres = NULL;
static pcre2_code ***pcres_addr = NULL;
static int *num_pcres = NULL;
static int pcre_options = 0x00000000;


/*
 * Module core functions
 */
static int mod_init(void);
static void destroy(void);


/*
 * Module internal functions
 */
static int load_pcres(int);
static void free_shared_memory(void);


/*
 * Script functions
 */
static int w_pcre_match(struct sip_msg *_msg, char *_s1, char *_s2);
static int w_pcre_match_group(struct sip_msg *_msg, char *_s1, char *_s2);


/*
 * Exported functions
 */
/* clang-format off */
static cmd_export_t cmds[] = {
	{"pcre_match", (cmd_function)w_pcre_match, 2,
		fixup_spve_spve, 0,
		REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
	{"pcre_match_group", (cmd_function)w_pcre_match_group, 2,
		fixup_spve_spve, 0,
		REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
	{"pcre_match_group", (cmd_function)w_pcre_match_group, 1,
		fixup_spve_null, 0,
		REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"file", PARAM_STRING, &file},
	{"max_groups", PARAM_INT, &max_groups},
	{"group_max_size", PARAM_INT, &group_max_size},
	{"pcre_caseless", PARAM_INT, &pcre_caseless},
	{"pcre_multiline", PARAM_INT, &pcre_multiline},
	{"pcre_dotall", PARAM_INT, &pcre_dotall},
	{"pcre_extended", PARAM_INT, &pcre_extended},
	{0, 0, 0}
};

/*
 * Module interface
 */
struct module_exports exports = {
	"regex",         /*!< module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* RPC method exports */
	0,               /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module initialization function */
	0,               /* per-child init function */
	destroy          /* module destroy function */
};
/* clang-format on */


static void *pcre2_malloc(size_t size, void *ext)
{
	return shm_malloc(size);
}

static void pcre2_free(void *ptr, void *ext)
{
	if(ptr) {
		shm_free(ptr);
		ptr = NULL;
	}
}

/*! \brief
 * Init module function
 */
static int mod_init(void)
{
	if(regex_init_rpc() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* Group matching feature */
	if(file == NULL) {
		LM_NOTICE("'file' parameter is not set, group matching disabled\n");
	} else {
		/* Create and init the lock */
		reload_lock = lock_alloc();
		if(reload_lock == NULL) {
			LM_ERR("cannot allocate reload_lock\n");
			goto err;
		}
		if(lock_init(reload_lock) == NULL) {
			LM_ERR("cannot init the reload_lock\n");
			lock_dealloc(reload_lock);
			goto err;
		}

		/* PCRE options */
		if(pcre_caseless != 0) {
			LM_DBG("PCRE CASELESS enabled\n");
			pcre_options = pcre_options | PCRE2_CASELESS;
		}
		if(pcre_multiline != 0) {
			LM_DBG("PCRE MULTILINE enabled\n");
			pcre_options = pcre_options | PCRE2_MULTILINE;
		}
		if(pcre_dotall != 0) {
			LM_DBG("PCRE DOTALL enabled\n");
			pcre_options = pcre_options | PCRE2_DOTALL;
		}
		if(pcre_extended != 0) {
			LM_DBG("PCRE EXTENDED enabled\n");
			pcre_options = pcre_options | PCRE2_EXTENDED;
		}
		LM_DBG("PCRE options: %i\n", pcre_options);

		if((pcres_gctx = pcre2_general_context_create(
					pcre2_malloc, pcre2_free, NULL))
				== NULL) {
			LM_ERR("pcre2 general context creation failed\n");
			return -1;
		}
		if((pcres_ctx = pcre2_compile_context_create(pcres_gctx)) == NULL) {
			LM_ERR("pcre2 compile context creation failed\n");
			return -1;
		}
		if((pcres_mctx = pcre2_match_context_create(pcres_gctx)) == NULL) {
			LM_ERR("pcre2 match context creation failed\n");
			return -1;
		}

		/* Pointer to pcres */
		if((pcres_addr = shm_malloc(sizeof(pcre2_code **))) == 0) {
			SHM_MEM_ERROR;
			goto err;
		}

		/* Integer containing the number of pcres */
		if((num_pcres = shm_malloc(sizeof(int))) == 0) {
			SHM_MEM_ERROR;
			goto err;
		}

		/* Load the pcres */
		LM_DBG("loading pcres...\n");
		if(load_pcres(START)) {
			LM_ERR("failed to load pcres\n");
			goto err;
		}
	}

	return 0;

err:
	free_shared_memory();
	return -1;
}


static void destroy(void)
{
	free_shared_memory();

	if(pcres_ctx) {
		pcre2_compile_context_free(pcres_ctx);
	}

	if(pcres_mctx) {
		pcre2_match_context_free(pcres_mctx);
	}

	if(pcres_gctx) {
		pcre2_general_context_free(pcres_gctx);
	}
}


/*! \brief Convert the file content into regular expressions and store them in pcres */
static int load_pcres(int action)
{
	int i, j;
	FILE *f;
	char line[FILE_MAX_LINE];
	char **patterns = NULL;
	int pcre_error_num = 0;
	char pcre_error[128];
	size_t pcre_erroffset;
	int num_pcres_tmp = 0;
	pcre2_code **pcres_tmp = NULL;
	int llen;

	/* Get the lock */
	lock_get(reload_lock);

	if(!(f = fopen(file, "r"))) {
		LM_ERR("could not open file '%s'\n", file);
		goto err;
	}

	/* Array containing each pattern in the file */
	if((patterns = pkg_malloc(sizeof(char *) * max_groups)) == 0) {
		LM_ERR("no more memory for patterns\n");
		fclose(f);
		goto err;
	}
	memset(patterns, 0, sizeof(char *) * max_groups);

	for(i = 0; i < max_groups; i++) {
		if((patterns[i] = pkg_malloc(sizeof(char) * group_max_size)) == 0) {
			LM_ERR("no more memory for patterns[%d]\n", i);
			fclose(f);
			goto err;
		}
		memset(patterns[i], 0, group_max_size);
	}

	/* Read the file and extract the patterns */
	memset(line, 0, FILE_MAX_LINE);
	i = -1;
	while(fgets(line, FILE_MAX_LINE - 4, f) != NULL) {

		/* Ignore comments and lines starting by space, tab, CR, LF */
		if(isspace(line[0]) || line[0] == '#') {
			memset(line, 0, FILE_MAX_LINE);
			continue;
		}

		/* First group */
		if(i == -1 && line[0] != '[') {
			LM_ERR("first group must be initialized with [0] before any "
				   "regular expression\n");
			fclose(f);
			goto err;
		}

		/* New group */
		if(line[0] == '[') {
			i++;
			/* Check if there are more patterns than the max value */
			if(i >= max_groups) {
				LM_ERR("max patterns exceeded\n");
				fclose(f);
				goto err;
			}
			/* Start the regular expression with '(' */
			patterns[i][0] = '(';
			patterns[i][1] = '\0';
			memset(line, 0, FILE_MAX_LINE);
			continue;
		}

		llen = strlen(line);
		/* Check if the pattern size is too big (approx) */
		if(strlen(patterns[i]) + llen >= group_max_size - 4) {
			LM_ERR("pattern max file exceeded\n");
			fclose(f);
			goto err;
		}

		/* Append ')' at the end of the line */
		if(line[llen - 1] == '\n') {
			line[llen - 1] = ')';
			line[llen] = '\n';
			line[llen + 1] = '\0';
		} else {
			/* This is the last char in the file and it's not \n */
			line[llen] = ')';
			line[llen + 1] = '\0';
		}

		/* Append '(' at the beginning of the line */
		llen = strlen(patterns[i]);
		memcpy(patterns[i] + llen, "(", 1);
		llen++;

		/* Append the line to the current pattern (including the ending 0) */
		memcpy(patterns[i] + llen, line, strlen(line) + 1);

		memset(line, 0, FILE_MAX_LINE);
	}
	num_pcres_tmp = i + 1;

	fclose(f);

	if(num_pcres_tmp == 0) {
		LM_ERR("no expressions in the file\n");
		goto err;
	}

	/* Fix the patterns */
	for(i = 0; i < num_pcres_tmp; i++) {

		/* Convert empty groups in unmatcheable regular expression ^$ */
		if(strlen(patterns[i]) == 1) {
			patterns[i][0] = '^';
			patterns[i][1] = '$';
			patterns[i][2] = '\0';
			continue;
		}

		/* Delete possible '\n' at the end of the pattern */
		if(patterns[i][strlen(patterns[i]) - 1] == '\n') {
			patterns[i][strlen(patterns[i]) - 1] = '\0';
		}

		/* Replace '\n' with '|' (except at the end of the pattern) */
		for(j = 0; j < strlen(patterns[i]); j++) {
			if(patterns[i][j] == '\n' && j != strlen(patterns[i]) - 1) {
				patterns[i][j] = '|';
			}
		}

		/* Add ')' at the end of the pattern */
		patterns[i][strlen(patterns[i])] = ')';
	}

	/* Log the group patterns */
	LM_INFO("num groups = %d\n", num_pcres_tmp);
	for(i = 0; i < num_pcres_tmp; i++) {
		LM_INFO("<group[%d]>%s</group[%d]> (size = %i)\n", i, patterns[i], i,
				(int)strlen(patterns[i]));
	}

	/* Temporal pointer of pcres */
	if((pcres_tmp = shm_malloc(sizeof(pcre2_code *) * num_pcres_tmp)) == 0) {
		LM_ERR("no more memory for pcres_tmp\n");
		goto err;
	}
	memset(pcres_tmp, 0, sizeof(pcre2_code *) * num_pcres_tmp);

	/* Compile the patterns */
	for(i = 0; i < num_pcres_tmp; i++) {
		pcres_tmp[i] = pcre2_compile((PCRE2_SPTR)patterns[i],
				PCRE2_ZERO_TERMINATED, pcre_options, &pcre_error_num,
				&pcre_erroffset, pcres_ctx);
		if(pcres_tmp[i] == NULL) {
			switch(pcre2_get_error_message(
					pcre_error_num, (PCRE2_UCHAR *)pcre_error, 128)) {
				case PCRE2_ERROR_NOMEMORY:
					snprintf(pcre_error, 128,
							"unknown error[%d]: pcre2 error buffer too small",
							pcre_error_num);
					break;
				case PCRE2_ERROR_BADDATA:
					snprintf(pcre_error, 128, "unknown pcre2 error[%d]",
							pcre_error_num);
					break;
			}
			LM_ERR("pcre_tmp compilation of '%s' failed at offset %zu: %s\n",
					patterns[i], pcre_erroffset, pcre_error);
			goto err;
		}
		pkg_free(patterns[i]);
		patterns[i] = NULL;
	}

	/* Copy to shared memory */
	if(action == RELOAD) {
		for(i = 0; i < *num_pcres; i++) { /* Use the previous num_pcres value */
			if(pcres[i]) {
				pcre2_code_free(pcres[i]);
			}
		}
		shm_free(pcres);
	}

	*num_pcres = num_pcres_tmp;
	pcres = pcres_tmp;
	*pcres_addr = pcres;

	/* Free allocated slots for unused patterns */
	for(i = num_pcres_tmp; i < max_groups; i++) {
		pkg_free(patterns[i]);
	}
	pkg_free(patterns);
	lock_release(reload_lock);

	return 0;

err:
	if(patterns) {
		for(i = 0; i < max_groups; i++) {
			if(patterns[i]) {
				pkg_free(patterns[i]);
			}
		}
		pkg_free(patterns);
	}
	if(pcres_tmp) {
		for(i = 0; i < num_pcres_tmp; i++) {
			if(pcres_tmp[i]) {
				pcre2_code_free(pcres_tmp[i]);
			}
		}
		pkg_free(pcres_tmp);
	}
	if(reload_lock) {
		lock_release(reload_lock);
	}
	if(action == START) {
		free_shared_memory();
	}
	return -1;
}


static void free_shared_memory(void)
{
	int i;

	if(pcres) {
		for(i = 0; i < *num_pcres; i++) {
			if(pcres[i]) {
				shm_free(pcres[i]);
			}
		}
		shm_free(pcres);
		pcres = NULL;
	}

	if(num_pcres) {
		shm_free(num_pcres);
		num_pcres = NULL;
	}

	if(pcres_addr) {
		shm_free(pcres_addr);
		pcres_addr = NULL;
	}

	if(reload_lock) {
		lock_destroy(reload_lock);
		lock_dealloc(reload_lock);
		reload_lock = NULL;
	}
}


/*
 * Script functions
 */

/*! \brief Return true if the argument matches the regular expression parameter */
static int ki_pcre_match(sip_msg_t *msg, str *string, str *regex)
{
	pcre2_code *pcre_re = NULL;
	pcre2_match_data *pcre_md = NULL;
	int pcre_rc;
	int pcre_error_num = 0;
	char pcre_error[128];
	size_t pcre_erroffset;

	pcre_re = pcre2_compile((PCRE2_SPTR)regex->s, PCRE2_ZERO_TERMINATED,
			pcre_options, &pcre_error_num, &pcre_erroffset, pcres_ctx);
	if(pcre_re == NULL) {
		switch(pcre2_get_error_message(
				pcre_error_num, (PCRE2_UCHAR *)pcre_error, 128)) {
			case PCRE2_ERROR_NOMEMORY:
				snprintf(pcre_error, 128,
						"unknown error[%d]: pcre2 error buffer too small",
						pcre_error_num);
				break;
			case PCRE2_ERROR_BADDATA:
				snprintf(pcre_error, 128, "unknown pcre2 error[%d]",
						pcre_error_num);
				break;
		}
		LM_ERR("pcre_re compilation of '%s' failed at offset %zu: %s\n",
				regex->s, pcre_erroffset, pcre_error);
		return -4;
	}

	pcre_md = pcre2_match_data_create_from_pattern(pcre_re, pcres_gctx);
	pcre_rc = pcre2_match(pcre_re,	   /* the compiled pattern */
			(PCRE2_SPTR)string->s,	   /* the matching string */
			(PCRE2_SIZE)(string->len), /* the length of the subject */
			0,						   /* start at offset 0 in the string */
			0,						   /* default options */
			pcre_md,				   /* the match data block */
			pcres_mctx); /* a match context; NULL means use defaults */

	/* Matching failed: handle error cases */
	if(pcre_rc < 0) {
		switch(pcre_rc) {
			case PCRE2_ERROR_NOMATCH:
				LM_DBG("'%s' doesn't match '%s'\n", string->s, regex->s);
				break;
			default:
				switch(pcre2_get_error_message(
						pcre_rc, (PCRE2_UCHAR *)pcre_error, 128)) {
					case PCRE2_ERROR_NOMEMORY:
						snprintf(pcre_error, 128,
								"unknown error[%d]: pcre2 error buffer too "
								"small",
								pcre_rc);
						break;
					case PCRE2_ERROR_BADDATA:
						snprintf(pcre_error, 128, "unknown pcre2 error[%d]",
								pcre_rc);
						break;
				}
				LM_ERR("matching error:'%s' failed[%d]\n", pcre_error, pcre_rc);
				break;
		}
		if(pcre_md)
			pcre2_match_data_free(pcre_md);
		pcre2_code_free(pcre_re);
		return -1;
	}
	if(pcre_md)
		pcre2_match_data_free(pcre_md);
	pcre2_code_free(pcre_re);
	LM_DBG("'%s' matches '%s'\n", string->s, regex->s);
	return 1;
}

/*! \brief Return true if the argument matches the regular expression parameter */
static int w_pcre_match(struct sip_msg *_msg, char *_s1, char *_s2)
{
	str string;
	str regex;

	if(_s1 == NULL) {
		LM_ERR("bad parameters\n");
		return -2;
	}

	if(_s2 == NULL) {
		LM_ERR("bad parameters\n");
		return -2;
	}

	if(fixup_get_svalue(_msg, (gparam_p)_s1, &string)) {
		LM_ERR("cannot print the format for string\n");
		return -3;
	}
	if(fixup_get_svalue(_msg, (gparam_p)_s2, &regex)) {
		LM_ERR("cannot print the format for regex\n");
		return -3;
	}

	return ki_pcre_match(_msg, &string, &regex);
}

/*! \brief Return true if the string argument matches the pattern group parameter */
static int ki_pcre_match_group(sip_msg_t *_msg, str *string, int num_pcre)
{
	int pcre_rc;
	pcre2_match_data *pcre_md = NULL;
	char pcre_error[128];

	/* Check if group matching feature is enabled */
	if(file == NULL) {
		LM_ERR("group matching is disabled\n");
		return -2;
	}

	if(num_pcre >= *num_pcres) {
		LM_ERR("invalid pcre index '%i', there are %i pcres\n", num_pcre,
				*num_pcres);
		return -4;
	}

	lock_get(reload_lock);
	pcre_md = pcre2_match_data_create_from_pattern(
			(*pcres_addr)[num_pcre], pcres_gctx);
	pcre_rc = pcre2_match((*pcres_addr)[num_pcre], /* the compiled pattern */
			(PCRE2_SPTR)string->s,				   /* the matching string */
			(PCRE2_SIZE)(string->len), /* the length of the subject */
			0,						   /* start at offset 0 in the string */
			0,						   /* default options */
			pcre_md,				   /* the match data block */
			pcres_mctx); /* a match context; NULL means use defaults */

	lock_release(reload_lock);
	if(pcre_md)
		pcre2_match_data_free(pcre_md);

	/* Matching failed: handle error cases */
	if(pcre_rc < 0) {
		switch(pcre_rc) {
			case PCRE2_ERROR_NOMATCH:
				LM_DBG("'%s' doesn't match pcres[%i]\n", string->s, num_pcre);
				break;
			default:
				switch(pcre2_get_error_message(
						pcre_rc, (PCRE2_UCHAR *)pcre_error, 128)) {
					case PCRE2_ERROR_NOMEMORY:
						snprintf(pcre_error, 128,
								"unknown error[%d]: pcre2 error buffer too "
								"small",
								pcre_rc);
						break;
					case PCRE2_ERROR_BADDATA:
						snprintf(pcre_error, 128, "unknown pcre2 error[%d]",
								pcre_rc);
						break;
				}
				LM_ERR("matching error:'%s' failed[%d]\n", pcre_error, pcre_rc);
				break;
		}
		return -1;
	} else {
		LM_DBG("'%s' matches pcres[%i]\n", string->s, num_pcre);
		return 1;
	}
}

/*! \brief Return true if the string argument matches the pattern group parameter */
static int w_pcre_match_group(struct sip_msg *_msg, char *_s1, char *_s2)
{
	str string, group;
	unsigned int num_pcre = 0;

	if(_s1 == NULL) {
		LM_ERR("bad parameters\n");
		return -3;
	}

	if(_s2 == NULL) {
		num_pcre = 0;
	} else {
		if(fixup_get_svalue(_msg, (gparam_p)_s2, &group)) {
			LM_ERR("cannot print the format for second param\n");
			return -5;
		}
		str2int(&group, &num_pcre);
	}

	if(fixup_get_svalue(_msg, (gparam_p)_s1, &string)) {
		LM_ERR("cannot print the format for first param\n");
		return -5;
	}

	return ki_pcre_match_group(_msg, &string, (int)num_pcre);
}


/*
 * RPC functions
 */

/*! \brief Reload pcres by reading the file again */
void regex_rpc_reload(rpc_t *rpc, void *ctx)
{
	/* Check if group matching feature is enabled */
	if(file == NULL) {
		LM_NOTICE("'file' parameter is not set, group matching disabled\n");
		rpc->fault(ctx, 500, "Group matching not enabled");
		return;
	}
	LM_INFO("reloading pcres...\n");
	if(load_pcres(RELOAD)) {
		LM_ERR("failed to reload pcres\n");
		rpc->fault(ctx, 500, "Failed to reload");
		return;
	}
	LM_INFO("reload success\n");
}

static const char *regex_rpc_reload_doc[2] = {"Reload regex file", 0};

rpc_export_t regex_rpc_cmds[] = {
		{"regex.reload", regex_rpc_reload, regex_rpc_reload_doc, 0},
		{0, 0, 0, 0}};

/**
 * register RPC commands
 */
static int regex_init_rpc(void)
{
	if(rpc_register_array(regex_rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_regex_exports[] = {
	{ str_init("regex"), str_init("pcre_match"),
		SR_KEMIP_INT, ki_pcre_match,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("regex"), str_init("pcre_match_group"),
		SR_KEMIP_INT, ki_pcre_match_group,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_regex_exports);
	return 0;
}
