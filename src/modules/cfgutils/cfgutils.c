/*
 * Copyright (C) 2012 Edvina AB
 * Copyright (C) 2007 1&1 Internet AG
 * Copyright (C) 2007 BASIS AudioNet GmbH
 * Copyright (C) 2004 FhG
 * Copyright (C) 2005-2006 Voice Sistem S.R.L.
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
 * along with this program; if not, write to the Free Softwar
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * cfgutils module: various config functions for Kamailio;
 * it provide functions to make a decision in the script
 * of the server based on a probability function.
 * The benefit of this module is the value of the probability function
 * can be manipulated by external applications such as web interface
 * or command line tools.
 * Furthermore it provides some functions to let the server wait a
 * specific time interval.
 *
 * gflags module: global flags; it keeps a bitmap of flags
 * in shared memory and may be used to change behaviour
 * of server based on value of the flags. E.g.,
 *    if (is_gflag("1")) { t_relay_to_udp("10.0.0.1","5060"); }
 *    else { t_relay_to_udp("10.0.0.2","5060"); }
 * The benefit of this module is the value of the switch flags
 * can be manipulated by external applications such as web interface
 * or command line tools.
 */


#include "../../core/sr_module.h"
#include "../../core/error.h"
#include "../../core/pvar.h"
#include "../../core/ut.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/mod_fix.h"
#include "../../core/md5.h"
#include "../../core/md5utils.h"
#include "../../core/globals.h"
#include "../../core/hashes.h"
#include "../../core/locking.h"
#include "../../core/route.h"
#include "../../core/kemi.h"
#include "../../core/rand/kam_rand.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "api.h"

MODULE_VERSION

static int set_prob(struct sip_msg*, char *, char *);
static int reset_prob(struct sip_msg*, char *, char *);
static int get_prob(struct sip_msg*, char *, char *);
static int rand_event(struct sip_msg*, char *, char *);
static int m_sleep(struct sip_msg*, char *, char *);
static int m_usleep(struct sip_msg*, char *, char *);
static int dbg_abort(struct sip_msg*, char*,char*);
static int dbg_pkg_status(struct sip_msg*, char*,char*);
static int dbg_shm_status(struct sip_msg*, char*,char*);
static int dbg_pkg_summary(struct sip_msg*, char*,char*);
static int dbg_shm_summary(struct sip_msg*, char*,char*);
static int w_route_exists(struct sip_msg*, char*);
static int w_check_route_exists(struct sip_msg*, char*);

static int set_gflag(struct sip_msg*, char *, char *);
static int reset_gflag(struct sip_msg*, char *, char *);
static int is_gflag(struct sip_msg*, char *, char *);

static int w_cfg_lock(struct sip_msg*, char *, char *);
static int w_cfg_unlock(struct sip_msg*, char *, char *);
static int w_cfg_trylock(struct sip_msg*, char *, char *);


static int pv_get_random_val(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);


static int fixup_prob( void** param, int param_no);
static int fixup_gflags( void** param, int param_no);

static int fixup_core_hash(void **param, int param_no);
static int w_core_hash(struct sip_msg *msg, char *p1, char *p2, char *p3);

int bind_cfgutils(cfgutils_api_t *api);

static int mod_init(void);
static void mod_destroy(void);

static int initial_prob = 10;
static int *probability = NULL;

static char config_hash[MD5_LEN];
static char* hash_file = NULL;

static int initial_gflags=0;
static unsigned int *gflags=0;
static gen_lock_t *gflags_lock = NULL;

static gen_lock_set_t *_cfg_lock_set = NULL;
static unsigned int _cfg_lock_size = 0;

static cmd_export_t cmds[]={
	{"rand_set_prob", /* action name as in scripts */
		(cmd_function)set_prob,  /* C function name */
		1,          /* number of parameters */
		fixup_prob, 0,         /* */
		/* can be applied to original/failed requests and replies */
		ANY_ROUTE},
	{"rand_reset_prob", (cmd_function)reset_prob, 0, 0, 0,
		ANY_ROUTE},
	{"rand_get_prob",   (cmd_function)get_prob,   0, 0, 0,
		ANY_ROUTE},
	{"rand_event",      (cmd_function)rand_event, 0, 0, 0,
		ANY_ROUTE},
	{"sleep",  (cmd_function)m_sleep,  1, fixup_igp_null, fixup_free_igp_null,
		ANY_ROUTE},
	{"usleep", (cmd_function)m_usleep, 1, fixup_igp_null, fixup_free_igp_null,
		ANY_ROUTE},
	{"abort",      (cmd_function)dbg_abort,        0, 0, 0,
		ANY_ROUTE},
	{"pkg_status", (cmd_function)dbg_pkg_status,   0, 0, 0,
		ANY_ROUTE},
	{"shm_status", (cmd_function)dbg_shm_status,   0, 0, 0,
		ANY_ROUTE},
	{"pkg_summary", (cmd_function)dbg_pkg_summary,   0, 0, 0,
		ANY_ROUTE},
	{"shm_summary", (cmd_function)dbg_shm_summary,   0, 0, 0,
		ANY_ROUTE},
	{"set_gflag",    (cmd_function)set_gflag,   1,   fixup_gflags, 0,
		ANY_ROUTE},
	{"reset_gflag",  (cmd_function)reset_gflag, 1,   fixup_gflags, 0,
		ANY_ROUTE},
	{"is_gflag",     (cmd_function)is_gflag,    1,   fixup_gflags, 0,
		ANY_ROUTE},
	{"lock",         (cmd_function)w_cfg_lock,    1,   fixup_spve_null, 0,
		ANY_ROUTE},
	{"unlock",       (cmd_function)w_cfg_unlock,  1,   fixup_spve_null, 0,
		ANY_ROUTE},
	{"trylock",       (cmd_function)w_cfg_trylock,  1,   fixup_spve_null, 0,
		ANY_ROUTE},
	{"core_hash",    (cmd_function)w_core_hash, 3,   fixup_core_hash, 0,
		ANY_ROUTE},
	{"check_route_exists",    (cmd_function)w_check_route_exists, 1,   fixup_spve_null, fixup_free_spve_null,
		ANY_ROUTE},
	{"route_if_exists",    (cmd_function)w_route_exists, 1,   fixup_spve_null, fixup_free_spve_null,
		ANY_ROUTE},
	{"bind_cfgutils", (cmd_function)bind_cfgutils,  0,
		0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


static param_export_t params[]={
	{"initial_probability", INT_PARAM, &initial_prob   },
	{"initial_gflags",      INT_PARAM, &initial_gflags },
	{"hash_file",           PARAM_STRING, &hash_file      },
	{"lock_set_size",       INT_PARAM, &_cfg_lock_size },
	{0,0,0}
};



static pv_export_t mod_items[] = {
	{ {"RANDOM", sizeof("RANDOM")-1}, PVT_OTHER, pv_get_random_val, 0,
		0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


struct module_exports exports = {
	"cfgutils",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        /* exported functions */
	params,      /* exported parameters */
	0,           /* exported statistics */
	0,           /* exported MI functions */
	mod_items,   /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0            /* per-child init function */
};


/**************************** fixup functions ******************************/
static int fixup_prob( void** param, int param_no)
{
	unsigned int myint;
	str param_str;

	/* we only fix the parameter #1 */
	if (param_no!=1)
		return 0;

	param_str.s=(char*) *param;
	param_str.len=strlen(param_str.s);
	str2int(&param_str, &myint);

	if (myint > 100) {
		LM_ERR("invalid probability <%d>\n", myint);
		return E_CFG;
	}

	pkg_free(*param);
	*param=(void *)(long)myint;
	return 0;
}

/**
 * convert char* to int and do bitwise right-shift
 * char* must be pkg_alloced and will be freed by the function
 */
static int fixup_gflags( void** param, int param_no)
{
	unsigned int myint;
	str param_str;

	/* we only fix the parameter #1 */
	if (param_no!=1)
		return 0;

	param_str.s=(char*) *param;
	param_str.len=strlen(param_str.s);

	if (str2int(&param_str, &myint )<0) {
		LM_ERR("bad number <%s>\n", (char *)(*param));
		return E_CFG;
	}
	if ( myint >= 8*sizeof(*gflags) ) {
		LM_ERR("flag <%d> out of "
			"range [0..%lu]\n", myint, ((unsigned long)8*sizeof(*gflags))-1 );
		return E_CFG;
	}
	/* convert from flag index to flag bitmap */
	myint = 1 << myint;
	/* success -- change to int */
	pkg_free(*param);
	*param=(void *)(long)myint;
	return 0;
}


/************************** module functions **********************************/

static int set_gflag(struct sip_msg *bar, char *flag, char *foo)
{
	lock_get(gflags_lock);
	(*gflags) |= (unsigned int)(long)flag;
	lock_release(gflags_lock);
	return 1;
}


static int reset_gflag(struct sip_msg *bar, char *flag, char *foo)
{
	lock_get(gflags_lock);
	(*gflags) &= ~ ((unsigned int)(long)flag);
	lock_release(gflags_lock);
	return 1;
}


static int is_gflag(struct sip_msg *bar, char *flag, char *foo)
{
	return ( (*gflags) & ((unsigned int)(long)flag)) ? 1 : -1;
}


void cfgutils_rpc_set_gflag(rpc_t* rpc, void* ctx)
{
	long int flag;
	if(rpc->scan(ctx, "d", (int*)(&flag))<1) {
		LM_WARN("no parameters\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	lock_get(gflags_lock);
	(*gflags) |= flag;
	lock_release(gflags_lock);
}

void cfgutils_rpc_reset_gflag(rpc_t* rpc, void* ctx)
{
	long int flag;
	if(rpc->scan(ctx, "d", (int*)(&flag))<1) {
		LM_WARN("no parameters\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	lock_get(gflags_lock);
	(*gflags) &= ~ flag;
	lock_release(gflags_lock);
}

void cfgutils_rpc_is_gflag(rpc_t* rpc, void* ctx)
{
	long int flag;
	if(rpc->scan(ctx, "d", (int*)(&flag))<1) {
		LM_WARN("no parameters\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if (((*gflags) & flag) == flag)
		rpc->add(ctx, "s", "TRUE");
	else
		rpc->add(ctx, "s", "FALSE");
}

void cfgutils_rpc_get_gflags(rpc_t* rpc, void* ctx)
{
	static unsigned int flags;

	flags = *gflags;

	if (rpc->rpl_printf(ctx, "0x%X (%u)", flags, flags) < 0) {
		rpc->fault(ctx, 500, "Faiure building the response");
		return;
	}
}

void cfgutils_rpc_set_prob(rpc_t* rpc, void* ctx)
{
	unsigned int percent;

	if(rpc->scan(ctx, "d", (int*)(&percent))<1) {
		LM_WARN("no parameters\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if (percent > 100) {
		LM_ERR("incorrect probability <%u>\n", percent);
		rpc->fault(ctx, 500, "Invalid Percent");
		return;
	}
	*probability = percent;
}


void cfgutils_rpc_reset_prob(rpc_t* rpc, void* ctx)
{
	*probability = initial_prob;
}

void cfgutils_rpc_get_prob(rpc_t* rpc, void* ctx)
{
	if (rpc->rpl_printf(ctx, "actual probability: %u percent",
				(*probability)) < 0) {
		rpc->fault(ctx, 500, "Faiure building the response");
		return;
	}
}

void cfgutils_rpc_get_hash(rpc_t* rpc, void* ctx)
{
	if (rpc->rpl_printf(ctx, "%.*s",
				MD5_LEN, config_hash) < 0) {
		rpc->fault(ctx, 500, "Faiure building the response");
		return;
	}
}


/*! \brief
  * Calculate a MD5 digest over a file.
  * This function assumes 32 bytes in the destination buffer.
  * \param dest destination
  * \param file_name file for that the digest should be calculated
  * \return zero on success, negative on errors
  */
static int MD5File(char *dest, const char *file_name)
{
	MD5_CTX context;
	FILE *input;
	unsigned char buffer[32768];
	unsigned char hash[16];
	unsigned int counter, size;

	struct stat stats;

	if (!dest || !file_name) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

    if (stat(file_name, &stats) != 0) {
		LM_ERR("could not stat file %s\n", file_name);
		return -1;
	}
	size = stats.st_size;

	MD5Init(&context);
	if((input = fopen(file_name, "rb")) == NULL) {
		LM_ERR("could not open file %s\n", file_name);
		return -1;
	}

	while(size) {
		counter = (size > sizeof(buffer)) ? sizeof(buffer) : size;
		if ((counter = fread(buffer, 1, counter, input)) <= 0) {
			fclose(input);
			return -1;
		}
		U_MD5Update(&context, buffer, counter);
		size -= counter;
	}
	fclose(input);
	U_MD5Final(hash, &context);

	string2hex(hash, 16, dest);
	LM_DBG("MD5 calculated: %.*s for file %s\n", MD5_LEN, dest, file_name);

	return 0;
}


void cfgutils_rpc_check_hash(rpc_t* rpc, void* ctx)
{
	char tmp[MD5_LEN];
	memset(tmp, 0, MD5_LEN);

	if (!hash_file) {
		rpc->fault(ctx, 500, "No hash file");
		return;
	}

	if (MD5File(tmp, hash_file) != 0) {
		LM_ERR("could not hash the config file");
		rpc->fault(ctx, 500, "Failed to hash the file");
		return;
	}

	if (strncmp(config_hash, tmp, MD5_LEN) == 0) {
		if (rpc->rpl_printf(ctx, "Identical hash") < 0) {
			rpc->fault(ctx, 500, "Faiure building the response");
			return;
		}
	} else {
		if (rpc->rpl_printf(ctx, "Different hash") < 0) {
			rpc->fault(ctx, 500, "Faiure building the response");
			return;
		}
	}
}

static const char* cfgutils_rpc_is_gflag_doc[2] = {
	"Checks if the bits specified by the argument are all set.",
	0
};


static const char* cfgutils_rpc_set_gflag_doc[2] = {
	"Sets the bits specified by the argument.",
	0
};


static const char* cfgutils_rpc_get_gflags_doc[2] = {
	"Return the value for global flags.",
	0
};


static const char* cfgutils_rpc_reset_gflag_doc[2] = {
	"Resets the bits specified by the argument.",
	0
};

static const char* cfgutils_rpc_set_prob_doc[2] = {
	"Set the random probability.",
	0
};
static const char* cfgutils_rpc_reset_prob_doc[2] = {
	"Reset the random probability.",
	0
};

static const char* cfgutils_rpc_get_prob_doc[2] = {
	"Get the random probability.",
	0
};

static const char* cfgutils_rpc_get_hash_doc[2] = {
	"Get config hash value.",
	0
};

static const char* cfgutils_rpc_check_hash_doc[2] = {
	"Check config hash value.",
	0
};

static rpc_export_t rpc_cmds[] = {
	{"cfgutils.is_gflag", cfgutils_rpc_is_gflag,
		cfgutils_rpc_is_gflag_doc, 0},
	{"cfgutils.set_gflag", cfgutils_rpc_set_gflag,
		cfgutils_rpc_set_gflag_doc, 0},
	{"cfgutils.get_gflags", cfgutils_rpc_get_gflags,
		cfgutils_rpc_get_gflags_doc, 0},
	{"cfgutils.reset_gflag", cfgutils_rpc_reset_gflag,
		cfgutils_rpc_reset_gflag_doc, 0},
	{"cfgutils.rand_set_prob", cfgutils_rpc_set_prob,
		cfgutils_rpc_set_prob_doc, 0},
	{"cfgutils.rand_reset_prob", cfgutils_rpc_reset_prob,
		cfgutils_rpc_reset_prob_doc, 0},
	{"cfgutils.rand_get_prob", cfgutils_rpc_get_prob,
		cfgutils_rpc_get_prob_doc, 0},
	{"cfgutils.get_config_hash", cfgutils_rpc_get_hash,
		cfgutils_rpc_get_hash_doc, 0},
	{"cfgutils.check_config_hash", cfgutils_rpc_check_hash,
		cfgutils_rpc_check_hash_doc, 0},

	{0, 0, 0, 0}
};

static int set_prob(struct sip_msg *bar, char *percent_par, char *foo)
{
	*probability=(int)(long)percent_par;
	return 1;
}

static int ki_rand_set_prob(sip_msg_t *msg, int percent_par)
{
	*probability=percent_par;
	return 1;
}

static int reset_prob(struct sip_msg *bar, char *percent_par, char *foo)
{
	*probability=initial_prob;
	return 1;
}

static int ki_rand_reset_prob(sip_msg_t *msg)
{
	*probability=initial_prob;
	return 1;
}

static int get_prob(struct sip_msg *bar, char *foo1, char *foo2)
{
	return *probability;
}

static int ki_rand_get_prob(sip_msg_t *bar)
{
	return *probability;
}

static int ki_rand_event(sip_msg_t *msg)
{
	double tmp;
	/* most of the time this will be disabled completly. Tis will also fix the
	 * problem with the corner cases if rand() returned zero or RAND_MAX */
	if ((*probability) == 0) return -1;
	if ((*probability) == 100) return 1;

	tmp = ((double) kam_rand() / KAM_RAND_MAX);
	LM_DBG("generated random %f\n", tmp);
	if (tmp < ((double) (*probability) / 100)) {
		LM_DBG("return true\n");
		return 1;
	}
	else {
		LM_DBG("return false\n");
		return -1;
	}
}

static int rand_event(struct sip_msg *bar, char *foo1, char *foo2)
{
	return ki_rand_event(bar);
}

static int pv_get_random_val(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int n;
	int l = 0;
	char *ch;

	if(msg==NULL || res==NULL)
		return -1;

	n = kam_rand();
	ch = int2str(n , &l);
	res->rs.s = ch;
	res->rs.len = l;
	res->ri = n;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;

	return 0;
}

static int m_sleep(struct sip_msg *msg, char *time, char *str2)
{
	int s;
	if(fixup_get_ivalue(msg, (gparam_t*)time, &s)!=0)
	{
		LM_ERR("cannot get time interval value\n");
		return -1;
	}
	sleep((unsigned int)s);
	return 1;
}

static int m_usleep(struct sip_msg *msg, char *time, char *str2)
{
	int s;
	if(fixup_get_ivalue(msg, (gparam_t*)time, &s)!=0)
	{
		LM_ERR("cannot get time interval value\n");
		return -1;
	}
	sleep_us((unsigned int)s);
	return 1;
}

static int dbg_abort(struct sip_msg* msg, char* foo, char* bar)
{
	LM_CRIT("abort called\n");
	abort();
	return 0;
}

static int ki_abort(sip_msg_t* msg)
{
	LM_CRIT("abort called\n");
	abort();
	return 0;
}

static int dbg_pkg_status(struct sip_msg* msg, char* foo, char* bar)
{
	pkg_status();
	return 1;
}

static int ki_pkg_status(sip_msg_t* msg)
{
	pkg_status();
	return 1;
}

static int dbg_shm_status(struct sip_msg* msg, char* foo, char* bar)
{
	shm_status();
	return 1;
}

static int ki_shm_status(sip_msg_t* msg)
{
	shm_status();
	return 1;
}

static int dbg_pkg_summary(struct sip_msg* msg, char* foo, char* bar)
{
	pkg_sums();
	return 1;
}

static int ki_pkg_summary(sip_msg_t* msg)
{
	pkg_sums();
	return 1;
}

static int dbg_shm_summary(struct sip_msg* msg, char* foo, char* bar)
{
	shm_sums();
	return 1;
}

static int ki_shm_summary(sip_msg_t* msg)
{
	shm_sums();
	return 1;
}

static int cfg_lock_helper(str *lkey, int mode)
{
	unsigned int pos;

	if(_cfg_lock_set==NULL) {
		LM_ERR("lock set not initialized (attempt to do op: %d on: %.*s)\n",
				mode, lkey->len, lkey->s);
		return -1;
	}
	pos = core_case_hash(lkey, 0, _cfg_lock_size);

	LM_DBG("cfg_lock mode %d on %u (%.*s)\n", mode, pos, lkey->len, lkey->s);

	if(mode==0) {
		/* Lock */
		lock_set_get(_cfg_lock_set, pos);
	} else if (mode == 1) {
		/* Unlock */
		lock_set_release(_cfg_lock_set, pos);
	} else {
		int res;
		/* Trylock */
		res = lock_set_try(_cfg_lock_set, pos);
		if (res != 0) {
			LM_DBG("Failed to trylock \n");
			/* Failed to lock */
			return -1;
		}
		LM_DBG("Succeeded with trylock \n");
		/* Succeeded in locking */
		return 1;
	}
	return 1;
}

static int cfg_lock(sip_msg_t *msg, str *lkey)
{
	return cfg_lock_helper(lkey, 0);
}

static int cfg_unlock(sip_msg_t *msg, str *lkey)
{
	return cfg_lock_helper(lkey, 1);
}

static int cfg_trylock(sip_msg_t *msg, str *lkey)
{
	return cfg_lock_helper(lkey, 2);
}

static int w_cfg_lock_wrapper(struct sip_msg *msg, gparam_p key, int mode)
{
	str s;
	if(key==NULL) {
		return -1;
	}
	if(fixup_get_svalue(msg, key, &s)!=0) {
		LM_ERR("cannot get first parameter\n");
		return -1;
	}
	return cfg_lock_helper(&s, mode);
}

static int w_cfg_lock(struct sip_msg *msg, char *key, char *s2)
{
	return w_cfg_lock_wrapper(msg, (gparam_p)key, 0);
}

static int w_cfg_unlock(struct sip_msg *msg, char *key, char *s2)
{
	return w_cfg_lock_wrapper(msg, (gparam_p)key, 1);
}

static int w_cfg_trylock(struct sip_msg *msg, char *key, char *s2)
{
	return w_cfg_lock_wrapper(msg, (gparam_p)key, 2);
}

/*! Check if a route block exists - only request routes
 */
static int w_check_route_exists(struct sip_msg *msg, char *route)
{
	str s;

	if (fixup_get_svalue(msg, (gparam_p) route, &s) != 0)
	{
			LM_ERR("invalid route parameter\n");
			return -1;
	}
	if (route_lookup(&main_rt, s.s)<0) {
		/* not found */
		return -1;
	}
	return 1;
}

/*! Run a request route block if it exists
 */
static int w_route_exists(struct sip_msg *msg, char *route)
{
	struct run_act_ctx ctx;
	int newroute, ret;
	str s;

	if (fixup_get_svalue(msg, (gparam_p) route, &s) != 0) {
			LM_ERR("invalid route parameter\n");
			return -1;
	}

	newroute = route_lookup(&main_rt, s.s);
	if (newroute<0) {
		return -1;
	}
	init_run_actions_ctx(&ctx);
	ret=run_actions(&ctx, main_rt.rlist[newroute], msg);
	if (ctx.run_flags & EXIT_R_F) {
		return 0;
	}
	return ret;
}

static int mod_init(void)
{
	/* Register RPC commands */
	if (rpc_register_array(rpc_cmds)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if (!hash_file) {
		LM_INFO("no hash_file given, disable hash functionality\n");
	} else {
		if (MD5File(config_hash, hash_file) != 0) {
			LM_ERR("could not hash the config file");
			return -1;
		}
		LM_DBG("config file hash is %.*s", MD5_LEN, config_hash);
	}

	if (initial_prob > 100) {
		LM_ERR("invalid probability <%d>\n", initial_prob);
		return -1;
	}
	LM_DBG("initial probability %d percent\n", initial_prob);

	probability=(int *) shm_malloc(sizeof(int));

	if (!probability) {
		LM_ERR("no shmem available\n");
		return -1;
	}
	*probability = initial_prob;

	gflags=(unsigned int *) shm_malloc(sizeof(unsigned int));
	if (!gflags) {
		LM_ERR(" no shmem available\n");
		return -1;
	}
	*gflags=initial_gflags;
	gflags_lock = lock_alloc();
	if (gflags_lock==0) {
		LM_ERR("cannot allocate gflgas lock\n");
		return -1;
	}
	if (lock_init(gflags_lock)==NULL) {
		LM_ERR("cannot initiate gflags lock\n");
		lock_dealloc(gflags_lock);
		return -1;
	}
	if(_cfg_lock_size>0) {
		if(_cfg_lock_size>14) {
			LM_WARN("lock set size too large (%d), making it 14\n",
					_cfg_lock_size);
			_cfg_lock_size = 14;
		}
		_cfg_lock_size = 1<<_cfg_lock_size;
		_cfg_lock_set = lock_set_alloc(_cfg_lock_size);
		if(_cfg_lock_set==NULL || lock_set_init(_cfg_lock_set)==NULL) {
			LM_ERR("cannot initiate lock set\n");
			return -1;
		}
	}
	return 0;
}


static void mod_destroy(void)
{
	if (probability)
		shm_free(probability);
	if (gflags)
		shm_free(gflags);
	if (gflags_lock) {
		lock_destroy(gflags_lock);
		lock_dealloc(gflags_lock);
	}
	if(_cfg_lock_set!=NULL)
	{
		lock_set_destroy(_cfg_lock_set);
		lock_set_dealloc(_cfg_lock_set);
	}
}

/**
 *
 */
int cfgutils_lock(str *lkey)
{
	return cfg_lock_helper(lkey, 0);
}

/**
 *
 */
int cfgutils_unlock(str *lkey)
{
	return cfg_lock_helper(lkey, 1);
}

static int fixup_core_hash(void **param, int param_no)
{
	if (param_no == 1)
		return fixup_spve_null(param, 1);
	else if (param_no == 2)
		return fixup_spve_null(param, 1);
	else if (param_no == 3)
		return fixup_igp_null(param, 1);
	else
		return 0;
}

static int w_core_hash(struct sip_msg *msg, char *p1, char *p2, char *p3)
{
        str s1, s2;
        int size;

        if (fixup_get_svalue(msg, (gparam_p) p1, &s1) != 0)
        {
                LM_ERR("invalid s1 paramerer\n");
                return -1;
        }
        if (fixup_get_svalue(msg, (gparam_p) p2, &s2) != 0)
        {
                LM_ERR("invalid s2 paramerer\n");
                return -1;
        }
        if (fixup_get_ivalue(msg, (gparam_p) p3, &size) != 0)
        {
                LM_ERR("invalid size paramerer\n");
                return -1;
        }

        if (size <= 0) size = 2;
        else size = 1 << size;

	/* Return value _MUST_ be > 0 */
        return core_hash(&s1, s2.len ? &s2 : NULL, size) + 1;
}

static int ki_core_hash(sip_msg_t *msg, str *s1, str *s2, int sz)
{
	int size;

	size = sz;

	if (size <= 0) size = 2;
	else size = 1 << size;

	return core_hash(s1, (s2 && s2->len>0)?s2:NULL, size) + 1;
}

/**
 * @brief bind functions to CFGUTILS API structure
 */
int bind_cfgutils(cfgutils_api_t *api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->mlock   = cfgutils_lock;
	api->munlock = cfgutils_unlock;

	return 0;
}


/**
 * KEMI exports
 */
static sr_kemi_t sr_kemi_cfgutils_exports[] = {
	{ str_init("cfgutils"), str_init("lock"),
		SR_KEMIP_INT, cfg_lock,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("unlock"),
		SR_KEMIP_INT, cfg_unlock,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("trylock"),
		SR_KEMIP_INT, cfg_trylock,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("rand_set_prob"),
		SR_KEMIP_INT, ki_rand_set_prob,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("rand_reset_prob"),
		SR_KEMIP_INT, ki_rand_reset_prob,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("rand_get_prob"),
		SR_KEMIP_INT, ki_rand_get_prob,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("rand_event"),
		SR_KEMIP_INT, ki_rand_event,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("abort"),
		SR_KEMIP_INT, ki_abort,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("pkg_status"),
		SR_KEMIP_INT, ki_pkg_status,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("shm_status"),
		SR_KEMIP_INT, ki_shm_status,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("pkg_summary"),
		SR_KEMIP_INT, ki_pkg_summary,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("shm_summary"),
		SR_KEMIP_INT, ki_shm_summary,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cfgutils"), str_init("core_hash"),
		SR_KEMIP_INT, ki_core_hash,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_cfgutils_exports);
	return 0;
}
