/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <sys/types.h> /* for regex */
#include <regex.h>

#include "../../core/sr_module.h"
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/utils/sruid.h"
#include "../../modules/acc/acc_api.h"
#include "../../modules/tm/tm_load.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "rd_funcs.h"
#include "rd_filter.h"

MODULE_VERSION

/* internal global variables */
struct tm_binds rd_tmb;           /*imported functions from tm */

/* global parameter variables */
str uacred_acc_db_table = str_init("acc");
str uacred_acc_fct_s    = str_init("acc_log_request");

/* private parameter variables */
char *deny_filter_s = 0;
char *accept_filter_s = 0;
char *def_filter_s = 0;

unsigned int bflags = 0;
int flags_hdr_mode = 0;

#define ACCEPT_RULE_STR "accept"
#define DENY_RULE_STR   "deny"
#define DEFAULT_Q_VALUE 10

int _redirect_q_value = DEFAULT_Q_VALUE;

/* sruid to get internal uid */
sruid_t _redirect_sruid;

acc_api_t _uacred_accb = {0};


static int redirect_init(void);
static int child_init(int rank);
static int w_set_deny(struct sip_msg* msg, char *dir, char *foo);
static int w_set_accept(struct sip_msg* msg, char *dir, char *foo);
static int w_get_redirect1(struct sip_msg* msg, char *dir, char *foo);
static int w_get_redirect2(struct sip_msg* msg, char *dir, char *foo);
static int regexp_compile(char *re_s, regex_t **re);
static int get_redirect_fixup(void** param, int param_no);
static int setf_fixup(void** param, int param_no);


static cmd_export_t cmds[] = {
	{"set_deny_filter",   (cmd_function)w_set_deny,      2,  setf_fixup, 0,
			FAILURE_ROUTE },
	{"set_accept_filter", (cmd_function)w_set_accept,    2,  setf_fixup, 0,
			FAILURE_ROUTE },
	{"get_redirects",     (cmd_function)w_get_redirect2,  2,  get_redirect_fixup, 0,
			FAILURE_ROUTE },
	{"get_redirects",     (cmd_function)w_get_redirect1,  1,  get_redirect_fixup, 0,
			FAILURE_ROUTE },
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"deny_filter",     PARAM_STRING,  &deny_filter_s    },
	{"accept_filter",   PARAM_STRING,  &accept_filter_s  },
	{"default_filter",  PARAM_STRING,  &def_filter_s     },
	{"acc_function",    PARAM_STR,  &uacred_acc_fct_s        },
	{"acc_db_table",    PARAM_STR,  &uacred_acc_db_table     },
	{"bflags",    		INT_PARAM,  &bflags			  },
	{"flags_hdr_mode",	INT_PARAM,  &flags_hdr_mode	  },
	{"q_value",         INT_PARAM,  &_redirect_q_value   },
	{0, 0, 0}
};


struct module_exports exports = {
	"uac_redirect",  /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* exported RPC functions */
	0,               /* exported pseudo-variables */
	0,               /* response handling function */
	redirect_init,   /* module initialization function */
	child_init,      /* per-child init function */
	0                /* module destroy function */
};



int get_nr_max(char *s, unsigned char *max)
{
	unsigned short nr;
	int err;

	if ( s[0]=='*' && s[1]==0 ) {
		/* is '*' -> infinit ;-) */
		*max = 0;
		return 0;
	} else {
		/* must be a positive number less than 255 */
		nr = str2s(s, strlen(s), &err);
		if (err==0){
			if (nr>255){
				LM_ERR("number too big <%d> (max=255)\n",nr);
				return -1;
			}
			*max = (unsigned char)nr;
			return 0;
		}else{
			LM_ERR("bad  number <%s>\n",s);
			return -1;
		}
	}
}


static int get_redirect_fixup(void** param, int param_no)
{
	unsigned char maxb,maxt;
	char *p;
	char *s;

	s = (char*)*param;
	if (param_no==1) {
		if ( (p=strchr(s,':'))!=0 ) {
			/* have max branch also */
			*p = 0;
			if (get_nr_max(p+1, &maxb)!=0)
				return E_UNSPEC;
		} else {
			maxb = 0; /* infinit */
		}

		/* get max total */
		if (get_nr_max(s, &maxt)!=0)
			return E_UNSPEC;

		pkg_free(*param);
		*param=(void*)(long)( (((unsigned short)maxt)<<8) | maxb);
	} else if (param_no==2) {
		/* acc function loaded? */
		if (uacred_acc_fct_s.s==0 || uacred_acc_fct_s.s[0]=='\0') {
			LM_ERR("acc support enabled, but no acc function defined\n");
			return E_UNSPEC;
		}
		if (_uacred_accb.acc_request==NULL) {
			/* bind the ACC API */
			if(acc_load_api(&_uacred_accb) < 0) {
				LM_ERR("cannot bind to ACC API\n");
				return E_UNSPEC;
			}
		}
		return fixup_spve_null(param, 1);
	}

	return 0;
}


static int setf_fixup(void** param, int param_no)
{
	unsigned short nr;
	regex_t *filter;
	char *s;

	s = (char*)*param;
	if (param_no==1) {
		/* compile the filter */
		if (regexp_compile( s, &filter)<0) {
			LM_ERR("cannot init filter <%s>\n", s);
			return E_BAD_RE;
		}
		pkg_free(*param);
		*param = (void*)filter;
	} else if (param_no==2) {
		if (s==0 || s[0]==0) {
			nr = 0;
		} else if (strcasecmp(s,"reset_all")==0) {
			nr = RESET_ADDED|RESET_DEFAULT;
		} else if (strcasecmp(s,"reset_default")==0) {
			nr = RESET_DEFAULT;
		} else if (strcasecmp(s,"reset_added")==0) {
			nr = RESET_ADDED;
		} else {
			LM_ERR("unknown reset type <%s>\n",s);
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param = (void*)(long)nr;
	}

	return 0;
}



static int regexp_compile(char *re_s, regex_t **re)
{
	*re = 0;
	if (re_s==0 || strlen(re_s)==0 ) {
		return 0;
	} else {
		if ((*re=pkg_malloc(sizeof(regex_t)))==0) {
			PKG_MEM_ERROR;
			return E_OUT_OF_MEM;
		}
		if (regcomp(*re, re_s, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ){
			pkg_free(*re);
			*re = 0;
			LM_ERR("regexp_compile:bad regexp <%s>\n", re_s);
			return E_BAD_RE;
		}
	}
	return 0;
}



static int redirect_init(void)
{
	regex_t *filter;

	/* load the TM API */
	if (load_tm_api(&rd_tmb)!=0) {
		LM_ERR("failed to load TM API\n");
		goto error;
	}

	if(uacred_acc_fct_s.s != 0 && uacred_acc_fct_s.s[0] != '\0') {
		/* bind the ACC API */
		if(acc_load_api(&_uacred_accb) < 0) {
			LM_ERR("cannot bind to ACC API\n");
			return -1;
		}
	}

	/* init filter */
	init_filters();

	/* what's the default rule? */
	if (def_filter_s) {
		if ( !strcasecmp(def_filter_s,ACCEPT_RULE_STR) ) {
			set_default_rule( ACCEPT_RULE );
		} else if ( !strcasecmp(def_filter_s,DENY_RULE_STR) ) {
			set_default_rule( DENY_RULE );
		} else {
			LM_ERR("unknown default filter <%s>\n",def_filter_s);
		}
	}

	/* if accept filter specify, compile it */
	if (regexp_compile(accept_filter_s, &filter)<0) {
		LM_ERR("failed to init accept filter\n");
		goto error;
	}
	add_default_filter( ACCEPT_FILTER, filter);

	/* if deny filter specify, compile it */
	if (regexp_compile(deny_filter_s, &filter)<0) {
		LM_ERR("failed to init deny filter\n");
		goto error;
	}
	add_default_filter( DENY_FILTER, filter);

	if(sruid_init(&_redirect_sruid, '-', "rdir", SRUID_INC)<0)
		return -1;

	return 0;
error:
	return -1;
}

static int child_init(int rank)
{
	if(sruid_init(&_redirect_sruid, '-', "rdir", SRUID_INC)<0)
		return -1;
	return 0;
}

static inline void msg_tracer(struct sip_msg* msg, int reset)
{
	static unsigned int id  = 0;
	static unsigned int set = 0;

	if (reset) {
		set = 0;
	} else {
		if (set) {
			if (id!=msg->id) {
				LM_WARN("filters set but not used -> resetting to default\n");
				reset_filters();
				id = msg->id;
			}
		} else {
			id = msg->id;
			set = 1;
		}
	}
}


static int w_set_deny(struct sip_msg* msg, char *re, char *flags)
{
	msg_tracer( msg, 0);
	return (add_filter( DENY_FILTER, (regex_t*)re, (int)(long)flags)==0)?1:-1;
}


static int w_set_accept(struct sip_msg* msg, char *re, char *flags)
{
	msg_tracer( msg, 0);
	return (add_filter( ACCEPT_FILTER, (regex_t*)re, (int)(long)flags)==0)?1:-1;
}


static int w_get_redirect2(struct sip_msg* msg, char *max_c, char *reason)
{
	int n;
	unsigned short max;
	str sreason;

	if(fixup_get_svalue(msg, (gparam_t*)reason, &sreason)<0) {
		LM_ERR("failed to get reason parameter\n");
		return -1;
	}

	msg_tracer( msg, 0);
	/* get the contacts */
	max = (unsigned short)(long)max_c;
	n = get_redirect(msg , (max>>8)&0xff, max&0xff, &sreason, bflags);
	reset_filters();
	/* reset the tracer */
	msg_tracer( msg, 1);

	return n;
}


static int w_get_redirect1(struct sip_msg* msg, char *max_c, char *foo)
{
	return w_get_redirect2(msg, max_c, 0);
}

static int ki_get_redirects_acc(sip_msg_t* msg, int max_c, int max_b,
		str *reason)
{
	int n;

	msg_tracer(msg, 0);
	/* get the contacts */
	n = get_redirect(msg, max_c, max_b, (reason && reason->len>0)?reason:NULL,
			bflags);
	reset_filters();
	/* reset the tracer */
	msg_tracer(msg, 1);

	return n;
}

static int ki_get_redirects(sip_msg_t* msg, int max_c, int max_b)
{
	int n;

	msg_tracer(msg, 0);
	/* get the contacts */
	n = get_redirect(msg, max_c, max_b, NULL, bflags);
	reset_filters();
	/* reset the tracer */
	msg_tracer(msg, 1);

	return n;
}

static int ki_get_redirects_all(sip_msg_t* msg)
{
	int n;

	msg_tracer(msg, 0);
	/* get the contacts */
	n = get_redirect(msg, 0, 0, NULL, bflags);
	reset_filters();
	/* reset the tracer */
	msg_tracer(msg, 1);

	return n;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_uac_redirect_exports[] = {
	{ str_init("uac_redirect"), str_init("get_redirects_all"),
		SR_KEMIP_INT, ki_get_redirects_all,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac_redirect"), str_init("get_redirects"),
		SR_KEMIP_INT, ki_get_redirects,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac_redirect"), str_init("get_redirects_acc"),
		SR_KEMIP_INT, ki_get_redirects_acc,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_uac_redirect_exports);
	return 0;
}
