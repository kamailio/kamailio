/*
 * pipelimit module
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2008 Ovidiu Sas <osas@voipembedded.com>
 * Copyright (C) 2006 Hendrik Scholz <hscholz@raisdorf.net>
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
 
/*! \file
 * \ingroup pipelimit
 * \brief pipelimit :: pl_db
 *
 */
/*! \defgroup pipelimit Pipelimit - rate limiting using BSD pipe model
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <math.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../timer.h"
#include "../../timer_ticks.h"
#include "../../ut.h"
#include "../../locking.h"
#include "../../mod_fix.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../lib/kcore/statistics.h"
#include "../../modules/sl/sl.h"
#include "../../lib/kmi/mi.h"
#include "../../rpc_lookup.h"

#include "pl_ht.h"
#include "pl_db.h"

MODULE_VERSION

/*
 * timer interval length in seconds, tunable via modparam
 */
#define RL_TIMER_INTERVAL 10

/** SL API structure */
sl_api_t slb;

enum {
	LOAD_SOURCE_CPU,
	LOAD_SOURCE_EXTERNAL
};

str_map_t source_names[] = {
	{str_init("cpu"),	LOAD_SOURCE_CPU},
	{str_init("external"),	LOAD_SOURCE_EXTERNAL},
	{{0, 0},		0},
};

static int pl_drop_code = 503;
static str pl_drop_reason = str_init("Server Unavailable");
static int pl_hash_size = 6;

typedef struct pl_queue {
	int     *       pipe;
	int             pipe_mp;
	str     *       method;
	str             method_mp;
} pl_queue_t;

static struct timer_ln* pl_timer;

/* === these change after startup */

static double * load_value;     /* actual load, used by PIPE_ALGO_FEEDBACK */
static double * pid_kp, * pid_ki, * pid_kd; /* PID tuning params */
double * _pl_pid_setpoint; /* PID tuning params */
static int * drop_rate;         /* updated by PIPE_ALGO_FEEDBACK */

static int * network_load_value;      /* network load */

/* where to get the load for feedback. values: cpu, external */
static int load_source_mp = LOAD_SOURCE_CPU;
static int * load_source;

/* these only change in the mod_init() process -- no locking needed */
static int timer_interval = RL_TIMER_INTERVAL;
int _pl_cfg_setpoint;        /* desired load, used when reading modparams */
/* === */


/** module functions */
static int mod_init(void);
static ticks_t pl_timer_handle(ticks_t, struct timer_ln*, void*);
static int w_pl_check(struct sip_msg*, char *, char *);
static int w_pl_check3(struct sip_msg*, char *, char *, char *);
static int w_pl_drop_default(struct sip_msg*, char *, char *);
static int w_pl_drop_forced(struct sip_msg*, char *, char *);
static int w_pl_drop(struct sip_msg*, char *, char *);
static void destroy(void);
static int fixup_pl_check3(void** param, int param_no);

static cmd_export_t cmds[]={
	{"pl_check",      (cmd_function)w_pl_check,        1, fixup_spve_null,
		0,               REQUEST_ROUTE|CORE_ONREPLY_ROUTE},
	{"pl_check",      (cmd_function)w_pl_check3,       3, fixup_pl_check3,
		0,               REQUEST_ROUTE|CORE_ONREPLY_ROUTE},
	{"pl_drop",       (cmd_function)w_pl_drop_default, 0, 0,
		0,               REQUEST_ROUTE},
	{"pl_drop",       (cmd_function)w_pl_drop_forced,  1, fixup_uint_null,
		0,               REQUEST_ROUTE},
	{"pl_drop",       (cmd_function)w_pl_drop,         2, fixup_uint_uint,
		0,               REQUEST_ROUTE},
	{0,0,0,0,0,0}
};
static param_export_t params[]={
	{"timer_interval",       INT_PARAM,          &timer_interval},
	{"reply_code",           INT_PARAM,          &pl_drop_code},
	{"reply_reason",         PARAM_STR,          &pl_drop_reason},
	{"db_url",               PARAM_STR,          &pl_db_url},
	{"plp_table_name",       PARAM_STR,          &rlp_table_name},
	{"plp_pipeid_column",    PARAM_STR,          &rlp_pipeid_col},
	{"plp_limit_column",     PARAM_STR,          &rlp_limit_col},
	{"plp_algorithm_column", PARAM_STR,          &rlp_algorithm_col},
	{"hash_size",            INT_PARAM,          &pl_hash_size},

	{0,0,0}
};

struct mi_root* mi_stats(struct mi_root* cmd_tree, void* param);
struct mi_root* mi_set_pipe(struct mi_root* cmd_tree, void* param);
struct mi_root* mi_get_pipes(struct mi_root* cmd_tree, void* param);
struct mi_root* mi_set_pid(struct mi_root* cmd_tree, void* param);
struct mi_root* mi_get_pid(struct mi_root* cmd_tree, void* param);
struct mi_root* mi_push_load(struct mi_root* cmd_tree, void* param);

static mi_export_t mi_cmds [] = {
	{"pl_stats",      mi_stats,      MI_NO_INPUT_FLAG, 0, 0},
	{"pl_set_pipe",   mi_set_pipe,   0,                0, 0},
	{"pl_get_pipes",  mi_get_pipes,  MI_NO_INPUT_FLAG, 0, 0},
	{"pl_set_pid",    mi_set_pid,    0,                0, 0},
	{"pl_get_pid",    mi_get_pid,    MI_NO_INPUT_FLAG, 0, 0},
	{"pl_push_load",  mi_push_load,  0,                0, 0},
	{0,0,0,0,0}
};

static rpc_export_t rpc_methods[];

/** module exports */
struct module_exports exports= {
	"pipelimit",
	DEFAULT_DLFLAGS,		/* dlopen flags */
	cmds,
	params,
	0,				/* exported statistics */
	mi_cmds,			/* exported MI functions */
	0,				/* exported pseudo-variables */
	0,				/* extra processes */
	mod_init,			/* module initialization function */
	0,
	(destroy_function) destroy,	/* module exit function */
	0				/* per-child init function */
};


#ifdef __OS_darwin
#include <sys/param.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

int get_num_cpus() {
	int count = 0;

#ifdef __OS_darwin
    int nm[2];
    size_t len;

    len = sizeof(count);

    nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);

    if(count < 1) {
        nm[1] = HW_NCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);
    }
#else
    count = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if(count < 1) return 1;
    return count;
}

/* not using /proc/loadavg because it only works when our_timer_interval == theirs */
static int get_cpuload(double * load)
{
	static 
	long long o_user, o_nice, o_sys, o_idle, o_iow, o_irq, o_sirq, o_stl;
	long long n_user, n_nice, n_sys, n_idle, n_iow, n_irq, n_sirq, n_stl;
	static int first_time = 1;
	FILE * f = fopen("/proc/stat", "r");
	double vload;
	int ncpu;
	static int errormsg = 0;

	if (! f) {
		/* Only write this error message five times. Otherwise you will annoy
		   BSD-ish system administrators. */
		if (errormsg < 5) {
			LM_ERR("could not open /proc/stat\n");
			errormsg++;
		}
		return -1;
	}
	if (fscanf(f, "cpu  %lld%lld%lld%lld%lld%lld%lld%lld",
			&n_user, &n_nice, &n_sys, &n_idle, &n_iow, &n_irq, &n_sirq, &n_stl) < 0) {
		  LM_ERR("could not parse load information\n");
		  return -1;
	}
	fclose(f);

	if (first_time) {
		first_time = 0;
		*load = 0;
	} else {		
		long long d_total =	(n_user - o_user)	+ 
					(n_nice	- o_nice)	+ 
					(n_sys	- o_sys)	+ 
					(n_idle	- o_idle)	+ 
					(n_iow	- o_iow)	+ 
					(n_irq	- o_irq)	+ 
					(n_sirq	- o_sirq)	+ 
					(n_stl	- o_stl);
		long long d_idle =	(n_idle - o_idle);

		vload = ((double)d_idle) / (double)d_total;

		/* divide by numbers of cpu */
		ncpu = get_num_cpus();
		vload = vload/ncpu;
		vload = 1.0 - vload;
		if(vload<0.0) vload = 0.0;
		else if (vload>1.0) vload = 1.0;

		*load = vload;
	}

	o_user	= n_user; 
	o_nice	= n_nice; 
	o_sys	= n_sys; 
	o_idle	= n_idle; 
	o_iow	= n_iow; 
	o_irq	= n_irq; 
	o_sirq	= n_sirq; 
	o_stl	= n_stl;
	
	return 0;
}

static double int_err = 0.0;
static double last_err = 0.0;

/* (*load_value) is expected to be in the 0.0 - 1.0 range
 * (expects pl_lock to be taken)
 */
static void do_update_load(void)
{
	static char spcs[51];
	int load;
	double err, dif_err, output;

	/* PID update */
	err = *_pl_pid_setpoint - *load_value;

	dif_err = err - last_err;

	/*
	 * TODO?: the 'if' is needed so low cpu loads for 
	 * long periods (which can't be compensated by 
	 * negative drop rates) don't confuse the controller
	 *
	 * NB: - "err < 0" means "desired_cpuload < actual_cpuload"
	 *     - int_err is integral(err) over time
	 */
	if (int_err < 0 || err < 0)
		int_err += err;

	output =	(*pid_kp) * err + 
				(*pid_ki) * int_err + 
				(*pid_kd) * dif_err;
	last_err = err;

	*drop_rate = (output > 0) ? output  : 0;

	load = 0.5 + 100.0 * *load_value; /* round instead of floor */

	memset(spcs, '-', load / 4);
	spcs[load / 4] = 0;

	/*
	LM_DBG("p=% 6.2lf i=% 6.2lf d=% 6.2lf o=% 6.2lf %s|%d%%\n",
		err, int_err, dif_err, output, spcs, load);
	*/
}

static void update_cpu_load(void)
{
	if (get_cpuload(load_value)) 
		return;

	do_update_load();
}

/* initialize ratelimit module */
static int mod_init(void)
{
	if(rpc_register_array(rpc_methods)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
	if(pl_hash_size<=0)
	{
		LM_ERR("invalid hash size parameter: %d\n", pl_hash_size);
		return -1;
	}
	if(pl_init_htable(1<<pl_hash_size)<0)
	{
		LM_ERR("could not allocate pipes htable\n");
		return -1;
	}
	if(pl_init_db()<0)
	{
		LM_ERR("could not load pipes description\n");
		return -1;
	}
	/* register timer to reset counters */
	if ((pl_timer = timer_alloc()) == NULL) {
		LM_ERR("could not allocate timer\n");
		return -1;
	}
	timer_init(pl_timer, pl_timer_handle, 0, F_TIMER_FAST);
	timer_add(pl_timer, MS_TO_TICKS(1000)); /* Start it after 1000ms */

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	network_load_value = shm_malloc(sizeof(int));
	if (network_load_value==NULL) {
		LM_ERR("oom for network_load_value\n");
		return -1;
	}

	load_value = shm_malloc(sizeof(double));
	if (load_value==NULL) {
		LM_ERR("oom for load_value\n");
		return -1;
	}
	load_source = shm_malloc(sizeof(int));
	if (load_source==NULL) {
		LM_ERR("oom for load_source\n");
		return -1;
	}
	pid_kp = shm_malloc(sizeof(double));
	if (pid_kp==NULL) {
		LM_ERR("oom for pid_kp\n");
		return -1;
	}
	pid_ki = shm_malloc(sizeof(double));
	if (pid_ki==NULL) {
		LM_ERR("oom for pid_ki\n");
		return -1;
	}
	pid_kd = shm_malloc(sizeof(double));
	if (pid_kd==NULL) {
		LM_ERR("oom for pid_kd\n");
		return -1;
	}
	_pl_pid_setpoint = shm_malloc(sizeof(double));
	if (_pl_pid_setpoint==NULL) {
		LM_ERR("oom for pid_setpoint\n");
		return -1;
	}
	drop_rate = shm_malloc(sizeof(int));
	if (drop_rate==NULL) {
		LM_ERR("oom for drop_rate\n");
		return -1;
	}

	*network_load_value = 0;
	*load_value = 0.0;
	*load_source = load_source_mp;
	*pid_kp = 0.0;
	*pid_ki = -25.0;
	*pid_kd = 0.0;
	*_pl_pid_setpoint = 0.01 * (double)_pl_cfg_setpoint;
	*drop_rate      = 0;

	return 0;
}


static void destroy(void)
{
	pl_destroy_htable();

	if (network_load_value) {
		shm_free(network_load_value);
		network_load_value = NULL;
	}
	if (load_value) {
		shm_free(load_value);
		load_value = NULL;
	}
	if (load_source) {
		shm_free(load_source);
		load_source = NULL;
	}
	if (pid_kp) {
		shm_free(pid_kp);
		pid_kp= NULL;
	}
	if (pid_ki) {
		shm_free(pid_ki);
		pid_ki = NULL;
	}
	if (pid_kd) {
		shm_free(pid_kd);
		pid_kd = NULL;
	}
	if (_pl_pid_setpoint) {
		shm_free(_pl_pid_setpoint);
		_pl_pid_setpoint = NULL;
	}
	if (drop_rate) {
		shm_free(drop_rate);
		drop_rate = NULL;
	}

	if (pl_timer) {
		timer_free(pl_timer);
		pl_timer = NULL;
	}

}


static int pl_drop(struct sip_msg * msg, unsigned int low, unsigned int high)
{
	str hdr;
	int ret;

	LM_DBG("(%d, %d)\n", low, high);

	if (slb.freply != 0) {
		if (low != 0 && high != 0) {
			hdr.s = (char *)pkg_malloc(64);
			if (hdr.s == 0) {
				LM_ERR("Can't allocate memory for Retry-After header\n");
				return 0;
			}
			hdr.len = 0;
			if (! hdr.s) {
				LM_ERR("no memory for hdr\n");
				return 0;
			}

			if (high == low) {
				hdr.len = snprintf(hdr.s, 63, "Retry-After: %d\r\n", low);
			} else {
				hdr.len = snprintf(hdr.s, 63, "Retry-After: %d\r\n", 
					low + rand() % (high - low + 1));
			}

			if (add_lump_rpl(msg, hdr.s, hdr.len, LUMP_RPL_HDR)==0) {
				LM_ERR("Can't add header\n");
				pkg_free(hdr.s);
				return 0;
			}

			ret = slb.freply(msg, pl_drop_code, &pl_drop_reason);

			pkg_free(hdr.s);
		} else {
			ret = slb.freply(msg, pl_drop_code, &pl_drop_reason);
		}
	} else {
		LM_ERR("Can't send reply\n");
		return 0;
	}
	return ret;
}

static int w_pl_drop(struct sip_msg* msg, char *p1, char *p2) 
{
	unsigned int low, high;

	low = (unsigned int)(unsigned long)p1;
	high = (unsigned int)(unsigned long)p2;

	if (high < low) {
		return pl_drop(msg, low, low);
	} else {
		return pl_drop(msg, low, high);
	}
}

static int w_pl_drop_forced(struct sip_msg* msg, char *p1, char *p2)
{
	unsigned int i;

	if (p1) {
		i = (unsigned int)(unsigned long)p1;
		LM_DBG("send retry in %d s\n", i);
	} else {
		i = 5;
		LM_DBG("send default retry in %d s\n", i);
	}
	return pl_drop(msg, i, i);
}

static int w_pl_drop_default(struct sip_msg* msg, char *p1, char *p2)
{
	return pl_drop(msg, 0, 0);
}

/* this is here to avoid using rand() ... which doesn't _always_ return
 * exactly what we want (see NOTES section in 'man 3 rand')
 */
int hash[100] = {18, 50, 51, 39, 49, 68, 8, 78, 61, 75, 53, 32, 45, 77, 31, 
	12, 26, 10, 37, 99, 29, 0, 52, 82, 91, 22, 7, 42, 87, 43, 73, 86, 70, 
	69, 13, 60, 24, 25, 6, 93, 96, 97, 84, 47, 79, 64, 90, 81, 4, 15, 63, 
	44, 57, 40, 21, 28, 46, 94, 35, 58, 11, 30, 3, 20, 41, 74, 34, 88, 62, 
	54, 33, 92, 76, 85, 5, 72, 9, 83, 56, 17, 95, 55, 80, 98, 66, 14, 16, 
	38, 71, 23, 2, 67, 36, 65, 27, 1, 19, 59, 89, 48};


/**
 * runs the pipe's algorithm
 * (expects pl_lock to be taken), TODO revert to "return" instead of "ret ="
 * \return	-1 if drop needed, 1 if allowed
 */
static int pipe_push_direct(pl_pipe_t *pipe)
{
	int ret;

	pipe->counter++;

	switch (pipe->algo) {
		case PIPE_ALGO_NOP:
			LM_ERR("no algorithm defined for pipe %.*s\n",
					pipe->name.len, pipe->name.s);
			ret = 2;
			break;
		case PIPE_ALGO_TAILDROP:
			ret = (pipe->counter <= pipe->limit * timer_interval) ? 1 : -1;
			break;
		case PIPE_ALGO_RED:
			if (pipe->load == 0)
				ret = 1;
			else
				ret = (! (pipe->counter % pipe->load)) ? 1 : -1;
			break;
		case PIPE_ALGO_FEEDBACK:
			ret = (hash[pipe->counter % 100] < *drop_rate) ? -1 : 1;
			break;
		case PIPE_ALGO_NETWORK:
			ret = -1 * pipe->load;
			break;
		default:
			LM_ERR("unknown ratelimit algorithm: %d\n", pipe->algo);
			ret = 1;
	}
	LM_DBG("pipe=%.*s algo=%d limit=%d pkg_load=%d counter=%d "
		"load=%2.1lf network_load=%d => %s\n",
		pipe->name.len, pipe->name.s,
		pipe->algo, pipe->limit,
		pipe->load, pipe->counter,
		*load_value, *network_load_value, (ret == 1) ? "ACCEPT" : "DROP");

	pl_pipe_release(&pipe->name);

	return ret;     
}

static int pipe_push(struct sip_msg * msg, str *pipeid)
{
	pl_pipe_t *pipe = NULL;

	pipe = pl_pipe_get(pipeid, 1);
	if(pipe==NULL)
	{
		LM_ERR("pipe not found [%.*s]\n", pipeid->len, pipeid->s);
		return -2;
	}
	return pipe_push_direct(pipe);
}

/**     
 * runs the current request through the queues
 * \param       msg
 * \param       forced_pipe     is >= 0 if a specific pipe should be used, < 0 otherwise
 * \return	-1 if drop needed, 1 if allowed
 */
static int pl_check(struct sip_msg * msg, str *pipeid)
{
	int ret;

	ret = pipe_push(msg, pipeid);

	return ret;
}

/**
 * limit checking with exiting pipes
 */
static int w_pl_check(struct sip_msg* msg, char *p1, char *p2)
{
	str pipeid = {0, 0};

	if(fixup_get_svalue(msg, (gparam_p)p1, &pipeid)!=0
			|| pipeid.s == 0)
	{
		LM_ERR("invalid pipeid parameter");
		return -1;
	}

	return pl_check(msg, &pipeid);
}


/**
 * limit checking with creation of pipe if it doesn't exist
 */
static int w_pl_check3(struct sip_msg* msg, char *p1pipe, char *p2alg,
		char *p3limit)
{
	int limit;
	str pipeid = {0, 0};
	str alg = {0, 0};
	pl_pipe_t *pipe = NULL;

	if(msg==NULL)
		return -1;

	if(fixup_get_ivalue(msg, (gparam_t*)p3limit, &limit)!=0 || limit<0)
	{
		LM_ERR("invalid limit value: %d\n", limit);
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)p1pipe, &pipeid)!=0
			|| pipeid.s == 0)
	{
		LM_ERR("invalid pipeid parameter");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)p2alg, &alg)!=0
			|| alg.s == 0)
	{
		LM_ERR("invalid algoritm parameter");
		return -1;
	}

	pipe = pl_pipe_get(&pipeid, 1);
	if(pipe==NULL)
	{
		LM_DBG("pipe not found [%.*s] - trying to add it\n",
				pipeid.len, pipeid.s);
		if(pl_pipe_add(&pipeid, &alg, limit)<0)
		{
			LM_ERR("failed to add pipe [%.*s]\n",
				pipeid.len, pipeid.s);
			return -2;
		}
		pipe = pl_pipe_get(&pipeid, 0);
		if(pipe==NULL)
		{
			LM_ERR("failed to retrieve pipe [%.*s]\n",
				pipeid.len, pipeid.s);
			return -2;
		}
	} else {
		if(limit>0) pipe->limit = limit;
		pl_pipe_release(&pipe->name);
	}

	return pl_check(msg, &pipeid);
}

static int fixup_pl_check3(void** param, int param_no)
{
	if(param_no==1) return fixup_spve_null(param, 1);
	if(param_no==2) return fixup_spve_null(param, 1);
	if(param_no==3) return fixup_igp_null(param, 1);
	return 0;
}

/* timer housekeeping, invoked each timer interval to reset counters */
static ticks_t pl_timer_handle(ticks_t ticks, struct timer_ln* tl, void* data)
{
	switch (*load_source) {
		case LOAD_SOURCE_CPU:
			update_cpu_load();
			break;
	}

	*network_load_value = get_total_bytes_waiting();

	pl_pipe_timer_update(timer_interval, *network_load_value);

	return (ticks_t)(-1); /* periodical */
}


struct mi_root* mi_get_pid(struct mi_root* cmd_tree, void* param)
{
	struct mi_root *rpl_tree;
	struct mi_node *node=NULL, *rpl=NULL;
	struct mi_attr* attr;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;
	node = add_mi_node_child(rpl, 0, "PID", 3, 0, 0);
	if(node == NULL)
		goto error;
	rpl_pipe_lock(0);
	attr= addf_mi_attr(node, 0, "ki", 2, "%0.3f", *pid_ki);
	if(attr == NULL)
		goto error;
	attr= addf_mi_attr(node, 0, "kp", 2, "%0.3f", *pid_kp);
	if(attr == NULL)
		goto error;
	attr= addf_mi_attr(node, 0, "kd", 2, "%0.3f", *pid_kd);
	rpl_pipe_release(0);
	if(attr == NULL)
		goto error;

	return rpl_tree;

error:
	rpl_pipe_release(0);
	LM_ERR("Unable to create reply\n");
	free_mi_tree(rpl_tree);
	return 0;
}

struct mi_root* mi_set_pid(struct mi_root* cmd_tree, void* param)
{
	struct mi_node *node;
	char i[5], p[5], d[5];

	node = cmd_tree->node.kids;
	if (node == NULL) return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	if ( !node->value.s || !node->value.len || node->value.len >= 5)
		goto bad_syntax;
	memcpy(i, node->value.s, node->value.len);
	i[node->value.len] = '\0';

	node = node->next;
	if ( !node->value.s || !node->value.len || node->value.len >= 5)
		goto bad_syntax;
	memcpy(p, node->value.s, node->value.len);
	p[node->value.len] = '\0';

	node = node->next;
	if ( !node->value.s || !node->value.len || node->value.len >= 5)
		goto bad_syntax;
	memcpy(d, node->value.s, node->value.len);
	d[node->value.len] = '\0';

	rpl_pipe_lock(0);
	*pid_ki = strtod(i, NULL);
	*pid_kp = strtod(p, NULL);
	*pid_kd = strtod(d, NULL);
	rpl_pipe_release(0);

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
bad_syntax:
	return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
}

struct mi_root* mi_push_load(struct mi_root* cmd_tree, void* param)
{
	struct mi_node *node;
	double value;
	char c[5];

	node = cmd_tree->node.kids;
	if (node == NULL) return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	if ( !node->value.s || !node->value.len || node->value.len >= 5)
		goto bad_syntax;
	memcpy(c, node->value.s, node->value.len);
	c[node->value.len] = '\0';
	value = strtod(c, NULL);
	if (value < 0.0 || value > 1.0) {
		LM_ERR("value out of range: %0.3f in not in [0.0,1.0]\n", value);
		goto bad_syntax;
	}
	rpl_pipe_lock(0);
	*load_value = value;
	rpl_pipe_release(0);

	do_update_load();

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
bad_syntax:
	return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
}

/* rpc function documentation */
const char *rpc_pl_stats_doc[2] = {
	"Print pipelimit statistics: \
<id> <load> <counter>", 0
};

const char *rpc_pl_get_pipes_doc[2] = {
	"Print pipes info: \
<id> <algorithm> <limit> <counter>", 0
};

const char *rpc_pl_set_pipe_doc[2] = {
	"Sets a pipe params: <pipe_id> <pipe_algorithm> <pipe_limit>", 0
};

const char *rpc_pl_get_pid_doc[2] = {
	"Print PID Controller parameters for the FEEDBACK algorithm: \
<ki> <kp> <kd>", 0
};

const char *rpc_pl_set_pid_doc[2] = {
	"Sets the PID Controller parameters for the FEEDBACK algorithm: \
<ki> <kp> <kd>", 0
};

const char *rpc_pl_push_load_doc[2] = {
	"Force the value of the load parameter for FEEDBACK algorithm: \
<load>", 0
};

/* rpc function implementations */
void rpc_pl_stats(rpc_t *rpc, void *c);
void rpc_pl_get_pipes(rpc_t *rpc, void *c);
void rpc_pl_set_pipe(rpc_t *rpc, void *c);

void rpc_pl_get_pid(rpc_t *rpc, void *c) {
	rpl_pipe_lock(0);
	rpc->rpl_printf(c, "ki[%f] kp[%f] kd[%f] ", *pid_ki, *pid_kp, *pid_kd);
	rpl_pipe_release(0);
}

void rpc_pl_set_pid(rpc_t *rpc, void *c) {
	double ki, kp, kd;

	if (rpc->scan(c, "fff", &ki, &kp, &kd) < 3) return;

	rpl_pipe_lock(0);
	*pid_ki = ki;
	*pid_kp = kp;
	*pid_kd = kd;
	rpl_pipe_release(0);
}

void rpc_pl_push_load(rpc_t *rpc, void *c) {
	double value;

	if (rpc->scan(c, "f", &value) < 1) return;

	if (value < 0.0 || value > 1.0) {
		LM_ERR("value out of range: %0.3f in not in [0.0,1.0]\n", value);
		rpc->fault(c, 400, "Value out of range");
		return;
	}
	rpl_pipe_lock(0);
	*load_value = value;
	rpl_pipe_release(0);

	do_update_load();
}

static rpc_export_t rpc_methods[] = {
	{"pl.stats",      rpc_pl_stats,     rpc_pl_stats_doc,     0},
	{"pl.get_pipes",  rpc_pl_get_pipes, rpc_pl_get_pipes_doc, 0},
	{"pl.set_pipe",   rpc_pl_set_pipe,  rpc_pl_set_pipe_doc,  0},
	{"pl.get_pid",    rpc_pl_get_pid,   rpc_pl_get_pid_doc,   0},
	{"pl.set_pid",    rpc_pl_set_pid,   rpc_pl_set_pid_doc,   0},
	{"pl.push_load",  rpc_pl_push_load, rpc_pl_push_load_doc, 0},
	{0, 0, 0, 0}
};

