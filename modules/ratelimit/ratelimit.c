/*
 * ratelimit module
 *
 * Copyright (C) 2006 Hendrik Scholz <hscholz@raisdorf.net>
 * Copyright (C) 2008-2010 Ovidiu Sas <osas@voipembedded.com>
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
#include "../../rpc_lookup.h"


MODULE_VERSION

#define MAX_PIPES       16
#define MAX_QUEUES      10

/*
 * timer interval length in seconds, tunable via modparam
 */
#define RL_TIMER_INTERVAL 10

#define RXLS(m, str, i) (int)((m)[i].rm_eo - (m)[i].rm_so), (str) + (m)[i].rm_so
#define RXL(m, str, i) (int)((m)[i].rm_eo - (m)[i].rm_so)
#define RXS(m, str, i) (str) + (m)[i].rm_so

static inline int str_cmp(const str * a, const str * b);
static inline int str_i_cmp(const str * a, const str * b);

typedef struct str_map {
	str     str;
	int     id;
} str_map_t;

static int str_map_str(const str_map_t * map, const str * key, int * ret);
static int str_map_int(const str_map_t * map, int key, str * ret);

/* PIPE_ALGO_FEEDBACK holds cpu usage to a fixed value using 
 * negative feedback according to the PID controller model
 *
 * <http://en.wikipedia.org/wiki/PID_controller>
 */
enum {
	PIPE_ALGO_NOP = 0,
	PIPE_ALGO_RED,
	PIPE_ALGO_TAILDROP,
	PIPE_ALGO_FEEDBACK,
	PIPE_ALGO_NETWORK
};

str_map_t algo_names[] = {
	{str_init("NOP"),	PIPE_ALGO_NOP},
	{str_init("RED"),	PIPE_ALGO_RED},
	{str_init("TAILDROP"),	PIPE_ALGO_TAILDROP},
	{str_init("FEEDBACK"),	PIPE_ALGO_FEEDBACK},
	{str_init("NETWORK"),	PIPE_ALGO_NETWORK},
	{{0, 0},		0},
};


/* at jiri@iptel.org's suggestion:
 *
 * set this to 'cpu' to have Kamailio look at /proc/stat every time_interval
 * or set it to 'external' and you can push data in from an external source
 * via the fifo interface
 */
enum {
	LOAD_SOURCE_CPU,
	LOAD_SOURCE_EXTERNAL
};

str_map_t source_names[] = {
	{str_init("cpu"),	LOAD_SOURCE_CPU},
	{str_init("external"),	LOAD_SOURCE_EXTERNAL},
	{{0, 0},		0},
};


typedef struct pipe {
	/* stuff that gets read as a modparam or set via fifo */
	int *   algo;
	int             algo_mp;
	int *   limit;
	int             limit_mp;

	/* updated values */
	int *   counter;
	int *   last_counter;
	int *   load;
} pipe_t;

typedef struct rl_queue {
	int     *       pipe;
	int             pipe_mp;
	str     *       method;
	str             method_mp;
} rl_queue_t;

static struct timer_ln* rl_timer;

/* === these change after startup */
gen_lock_t * rl_lock;

static double * load_value;     /* actual load, used by PIPE_ALGO_FEEDBACK */
static double * pid_kp, * pid_ki, * pid_kd, * pid_setpoint; /* PID tuning params */
static int * drop_rate;         /* updated by PIPE_ALGO_FEEDBACK */

static int * network_load_value;      /* network load */

/* where to get the load for feedback. values: cpu, external */
static int load_source_mp = LOAD_SOURCE_CPU;
static int * load_source;

typedef struct pipe_params {
	int no;
	int algo;
	int limit;
} pipe_params_t;

typedef struct rl_queue_params {
	int pipe;
	str method;
} rl_queue_params_t;

static pipe_t pipes[MAX_PIPES];
static rl_queue_t queues[MAX_QUEUES];

static int nqueues_mp = 0;
static int * nqueues;

static  str * rl_dbg_str = NULL;

/* these only change in the mod_init() process -- no locking needed */
static int timer_interval = RL_TIMER_INTERVAL;
static int cfg_setpoint;        /* desired load, used when reading modparams */
/* === */

#ifndef RL_DEBUG_LOCKS
# define LOCK_GET lock_get
# define LOCK_RELEASE lock_release
#else
# define LOCK_GET(l) do { \
	LM_INFO("%d: + get\n", __LINE__); \
	lock_get(l); \
	LM_INFO("%d: - get\n", __LINE__); \
} while (0)

# define LOCK_RELEASE(l) do { \
	LM_INFO("%d: + release\n", __LINE__); \
	lock_release(l); \
	LM_INFO("%d: - release\n", __LINE__); \
} while (0)
#endif

static int params_inited = 0;
static regex_t pipe_params_regex;
static regex_t queue_params_regex;

/** module functions */
static int mod_init(void);
static ticks_t rl_timer_handle(ticks_t, struct timer_ln*, void*);
static int w_rl_check_default(struct sip_msg*, char *, char *);
static int w_rl_check_forced(struct sip_msg*, char *, char *);
static int w_rl_check_forced_pipe(struct sip_msg*, char *, char *);
static int add_queue_params(modparam_t, void *);
static int add_pipe_params(modparam_t, void *);
/* RESERVED for future use
static int set_load_source(modparam_t, void *);
*/
static void destroy(void);

static cmd_export_t cmds[]={
	{"rl_check",      (cmd_function)w_rl_check_default,     0, 0,               0,               REQUEST_ROUTE|LOCAL_ROUTE},
	{"rl_check",      (cmd_function)w_rl_check_forced,      1, fixup_pvar_null,
		fixup_free_pvar_null, REQUEST_ROUTE|LOCAL_ROUTE},
	{"rl_check_pipe", (cmd_function)w_rl_check_forced_pipe, 1, fixup_uint_null, 0,               REQUEST_ROUTE|LOCAL_ROUTE},
	{0,0,0,0,0,0}
};
static param_export_t params[]={
	{"timer_interval", INT_PARAM,                &timer_interval},
	{"queue",          PARAM_STRING|USE_FUNC_PARAM, (void *)add_queue_params},
	{"pipe",           PARAM_STRING|USE_FUNC_PARAM, (void *)add_pipe_params},
	/* RESERVED for future use
	{"load_source",    PARAM_STRING|USE_FUNC_PARAM, (void *)set_load_source},
	*/
	{0,0,0}
};

static rpc_export_t rpc_methods[];

/** module exports */
struct module_exports exports= {
	"ratelimit",
	DEFAULT_DLFLAGS,		/* dlopen flags */
	cmds,
	params,
	0,				/* exported statistics */
	0,				/* exported MI functions */
	0,				/* exported pseudo-variables */
	0,				/* extra processes */
	mod_init,			/* module initialization function */
	0,
	(destroy_function) destroy,	/* module exit function */
	0				/* per-child init function */
};


/**
 * converts a mapped str to an int
 * \return	0 if found, -1 otherwise
 */
static int str_map_str(const str_map_t * map, const str * key, int * ret)
{
	for (; map->str.s; map++) 
		if (! str_cmp(&map->str, key)) {
			*ret = map->id;
			return 0;
		}
	LM_DBG("str_map_str() failed map=%p key=%.*s\n", map, key->len, key->s);
	return -1;
}

/**
 * converts a mapped int to a str
 * \return	0 if found, -1 otherwise
 */
static int str_map_int(const str_map_t * map, int key, str * ret)
{
	for (; map->str.s; map++) 
		if (map->id == key) {
			*ret = map->str;
			return 0;
		}
	LM_DBG("str_map_str() failed map=%p key=%d\n", map, key);
	return -1;
}

/**
 * strcpy for str's (does not allocate the str structure but only the .s member)
 * \return	0 if succeeded, -1 otherwise
 */
static int str_cpy(str * dest, str * src)
{
	dest->len = src->len;
	dest->s = shm_malloc(src->len);
	if (! dest->s) {
		LM_ERR("oom: '%.*s'\n", src->len, src->s);
		return -1;
	}
	memcpy(dest->s, src->s, src->len);
	return 0;
}

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
		/* Only output this error message five times */
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
 * (expects rl_lock to be taken)
 */
static void do_update_load(void)
{
	static char spcs[51];
	int load;
	double err, dif_err, output;

	/* PID update */
	err = *pid_setpoint - *load_value;

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
	int i;

	if (rpc_register_array(rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	rl_lock = lock_alloc();
	if (! rl_lock) {
		LM_ERR("oom in lock_alloc()\n");
		return -1;
	}

	if (lock_init(rl_lock)==0) {
		LM_ERR("failed to init lock\n");
		return -1;
	}

	/* register timer to reset counters */
	if ((rl_timer = timer_alloc()) == NULL) {
		LM_ERR("could not allocate timer\n");
		return -1;
	}
	timer_init(rl_timer, rl_timer_handle, 0, F_TIMER_FAST);
	timer_add(rl_timer, MS_TO_TICKS(1000*timer_interval));

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
	pid_setpoint = shm_malloc(sizeof(double));
	if (pid_setpoint==NULL) {
		LM_ERR("oom for pid_setpoint\n");
		return -1;
	}
	drop_rate = shm_malloc(sizeof(int));
	if (drop_rate==NULL) {
		LM_ERR("oom for drop_rate\n");
		return -1;
	}
	nqueues = shm_malloc(sizeof(int));
	if (nqueues==NULL) {
		LM_ERR("oom for nqueues\n");
		return -1;
	}
	rl_dbg_str = shm_malloc(sizeof(str));
	if (rl_dbg_str==NULL) {
		LM_ERR("oom for rl_dbg_str\n");
		return -1;
	}

	*network_load_value = 0;
	*load_value = 0.0;
	*load_source = load_source_mp;
	*pid_kp = 0.0;
	*pid_ki = -25.0;
	*pid_kd = 0.0;
	*pid_setpoint = 0.01 * (double)cfg_setpoint;
	*drop_rate      = 0;
	*nqueues = nqueues_mp;
	rl_dbg_str->s = NULL;
	rl_dbg_str->len = 0;

	for (i=0; i<MAX_PIPES; i++) {
		pipes[i].algo    = shm_malloc(sizeof(int));
		if (pipes[i].algo==NULL) {
			LM_ERR("oom for pipes[%d].algo\n", i);
			return -1;
		}
		pipes[i].limit   = shm_malloc(sizeof(int));
		if (pipes[i].limit==NULL) {
			LM_ERR("oom for pipes[%d].limit\n", i);
			return -1;
		}
		pipes[i].load    = shm_malloc(sizeof(int));
		if (pipes[i].load==NULL) {
			LM_ERR("oom for pipes[%d].load\n", i);
			return -1;
		}
		pipes[i].counter = shm_malloc(sizeof(int));
		if (pipes[i].counter==NULL) {
			LM_ERR("oom for pipes[%d].counter\n", i);
			return -1;
		}
		pipes[i].last_counter = shm_malloc(sizeof(int));
		if (pipes[i].last_counter==NULL) {
			LM_ERR("oom for pipes[%d].last_counter\n", i);
			return -1;
		}
		*pipes[i].algo    = pipes[i].algo_mp;
		*pipes[i].limit   = pipes[i].limit_mp;
		*pipes[i].load    = 0;
		*pipes[i].counter = 0;
		*pipes[i].last_counter = 0;
	}

	for (i=0; i<*nqueues; i++) {
		queues[i].pipe   = shm_malloc(sizeof(int));
		if (queues[i].pipe==NULL) {
			LM_ERR("oom for queues[%d].pipe\n", i);
			return -1;
		}
		queues[i].method = shm_malloc(sizeof(str));
		if (queues[i].method==NULL) {
			LM_ERR("oom for queues[%d].method\n", i);
			return -1;
		}

		*queues[i].pipe   = queues[i].pipe_mp;
		if (queues[i].method_mp.s == NULL) {
			LM_ERR("unexpected NULL method for queues[%d].method_mp\n", i);
			return -1;
		}
		if(str_cpy(queues[i].method, &queues[i].method_mp)) {
			LM_ERR("oom str_cpy(queues[%d].method\n", i);
			return -1;
		}
		pkg_free(queues[i].method_mp.s);
		queues[i].method_mp.s = NULL;
		queues[i].method_mp.len = 0;
	}

	return 0;
}


static void destroy(void)
{
	int i;

	regfree(&pipe_params_regex);
	regfree(&queue_params_regex);

	for (i=0;  i<MAX_PIPES; i++) {
		if (pipes[i].algo) {
			shm_free(pipes[i].algo);
			pipes[i].algo = NULL;
		}
		if (pipes[i].load) {
			shm_free(pipes[i].load);
			pipes[i].load = NULL;
		}
		if (pipes[i].counter) {
			shm_free(pipes[i].counter);
			pipes[i].counter = NULL;
		}
		if (pipes[i].last_counter) {
			shm_free(pipes[i].last_counter);
			pipes[i].last_counter = NULL;
		}
		if (pipes[i].limit) {
			shm_free(pipes[i].limit);
			pipes[i].limit = NULL;
		}
	}

	if (nqueues) {
		for (i=0; i<*nqueues; i++) {
			if (queues[i].pipe) {
				shm_free(queues[i].pipe);
				queues[i].pipe = NULL;
			}
			if (queues[i].method) {
				if (queues[i].method->s) {
					shm_free(queues[i].method->s);
					queues[i].method->s = NULL;
					queues[i].method->len = 0;
				}
				shm_free(queues[i].method);
				queues[i].method = NULL;
			}
		}
	}

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
	if (pid_setpoint) {
		shm_free(pid_setpoint);
		pid_setpoint = NULL;
	}
	if (drop_rate) {
		shm_free(drop_rate);
		drop_rate = NULL;
	}
	if (nqueues) {
		shm_free(nqueues);
		nqueues = NULL;
	}
	if (rl_dbg_str) {
		if (rl_dbg_str->s) {
			shm_free(rl_dbg_str->s);
			rl_dbg_str->s = NULL;
			rl_dbg_str->len = 0;
		}
		shm_free(rl_dbg_str);
		rl_dbg_str = NULL;
	}

	if (rl_timer) {
		timer_free(rl_timer);
		rl_timer = NULL;
	}

	if (rl_lock) {
		lock_destroy(rl_lock);
		lock_dealloc((void *)rl_lock);
	}
}


static inline int str_cmp(const str * a , const str * b)
{
	return ! (a->len == b->len && ! strncmp(a->s, b->s, a->len));
}

static inline int str_i_cmp(const str * a, const str * b)
{
	return ! (a->len == b->len && ! strncasecmp(a->s, b->s, a->len));
}

str queue_other = str_init("*");

/**
 * finds the queue associated with the message's method
 * (expects rl_lock to be taken)
 * \return	0 if a nueue was found, -1 otherwise
 */
static int find_queue(struct sip_msg * msg, int * queue)
{
	str method = msg->first_line.u.request.method;
	int i;

	*queue = -1;
	for (i=0; i<*nqueues; i++)
		if (! str_i_cmp(queues[i].method, &method)) {
			*queue = i;
			return 0;
		} else if (! str_i_cmp(queues[i].method, &queue_other)) {
			*queue = i;
		}
	
	if (*queue >= 0)
		return 0;
	
	LM_INFO("no queue matches\n");
	return -1;
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
 * (expects rl_lock to be taken), TODO revert to "return" instead of "ret ="
 * \return	-1 if drop needed, 1 if allowed
 */
static int pipe_push(struct sip_msg * msg, int id)
{
	int ret;

	(*pipes[id].counter)++;

	switch (*pipes[id].algo) {
		case PIPE_ALGO_NOP:
			LM_ERR("no algorithm defined for pipe %d\n", id);
			ret = 1;
			break;
		case PIPE_ALGO_TAILDROP:
			ret = (*pipes[id].counter <= *pipes[id].limit * timer_interval) ? 1 : -1;
			break;
		case PIPE_ALGO_RED:
			if (*pipes[id].load == 0)
				ret = 1;
			else
				ret = (! (*pipes[id].counter % *pipes[id].load)) ? 1 : -1;
			break;
		case PIPE_ALGO_FEEDBACK:
			ret = (hash[*pipes[id].counter % 100] < *drop_rate) ? -1 : 1;
			break;
		case PIPE_ALGO_NETWORK:
			ret = -1 * *pipes[id].load;
			break;
		default:
			LM_ERR("unknown ratelimit algorithm: %d\n", *pipes[id].algo);
			ret = 1;
	}

	return ret;     
}

/**     
 * runs the current request through the queues
 * \param       msg
 * \param       forced_pipe     is >= 0 if a specific pipe should be used, < 0 otherwise
 * \return	-1 if drop needed, 1 if allowed
 */
static int rl_check(struct sip_msg * msg, int forced_pipe)
{
	int que_id, pipe_id, ret;
	str method = msg->first_line.u.request.method;

	if (forced_pipe >=0 && (forced_pipe>=MAX_PIPES || *pipes[forced_pipe].algo==PIPE_ALGO_NOP)) {
		LM_ERR("forced pipe %d out of range or not defined", forced_pipe);
		return -1;
	}

	LOCK_GET(rl_lock);
	if (forced_pipe < 0) { 
		if (find_queue(msg, &que_id)) {
			pipe_id = que_id = 0;
			ret = -1;
			goto out_release;
		}
		pipe_id = *queues[que_id].pipe;
	} else {
		que_id = 0;
		pipe_id = forced_pipe;
	}

	ret = pipe_push(msg, pipe_id);
out_release:
	LOCK_RELEASE(rl_lock);

	/* no locks here because it's only read and pipes[pipe_id] is always alloc'ed */
	LM_DBG("meth=%.*s queue=%d pipe=%d algo=%d limit=%d pkg_load=%d counter=%d "
		"load=%2.1lf network_load=%d => %s\n",
		method.len, method.s, que_id, pipe_id,
		*pipes[pipe_id].algo, *pipes[pipe_id].limit,
		*pipes[pipe_id].load, *pipes[pipe_id].counter,
		*load_value, *network_load_value, (ret == 1) ? "ACCEPT" : "DROP");

	return ret;
}

static int w_rl_check_forced(struct sip_msg* msg, char *p1, char *p2)
{
	int pipe = -1;
	pv_value_t pv_val;

	if (p1 && (pv_get_spec_value(msg, (pv_spec_t *)p1, &pv_val) == 0)) {
		if (pv_val.flags & PV_VAL_INT) {
			pipe = pv_val.ri;
			LM_DBG("pipe=%d\n", pipe);
		} else if (pv_val.flags & PV_VAL_STR) {
			if(str2int(&(pv_val.rs), (unsigned int*)&pipe) != 0) {
				LM_ERR("Unable to get pipe from pv '%.*s'"
					"=> defaulting to method type checking\n",
					pv_val.rs.len, pv_val.rs.s);
				pipe = -1;
			}
		} else {
			LM_ERR("pv not a str or int => defaulting to method type checking\n");
			pipe = -1;
		}
	} else {
		LM_ERR("Unable to get pipe from pv:%p"
			" => defaulting to method type checking\n", p1);
		pipe = -1;
	}
	return rl_check(msg, pipe);
}
static int w_rl_check_forced_pipe(struct sip_msg* msg, char *p1, char *p2)
{
	int pipe;

	pipe = (int)(unsigned int)(unsigned long)p1;
	LM_DBG("trying pipe %d\n", pipe);
	return rl_check(msg, pipe);
}

static int w_rl_check_default(struct sip_msg* msg, char *p1, char *p2)
{
	return rl_check(msg, -1);
}

/* RESERVED for future use
static int set_load_source(modparam_t type, void * val)
{
	str src_name = { .s = val, .len = strlen(val) };
	int src_id;

	if (str_map_str(source_names, &src_name, &src_id)) {
		LM_ERR("unknown load source: %.*s\n", src_name.len, src_name.s);
		return -1;
	}

	load_source_mp = src_id;
	LM_INFO("switched to load source: %.*s\n", src_name.len, src_name.s);

	return 0;
}
*/

/**
 * compiles regexes for parsing modparams and clears the pipes and queues
 * \return      0 on success
 */
static int init_params(void)
{
	if (regcomp(&pipe_params_regex, "^([0-9]+):([^: ]+):([0-9]+)$", REG_EXTENDED|REG_ICASE) ||
		regcomp(&queue_params_regex, "^([0-9]+):([^: ]+)$", REG_EXTENDED|REG_ICASE)) {
		LM_ERR("can't compile modparam regexes\n");
		return -1;
	}

	memset(pipes, 0, sizeof(pipes));
	memset(queues, 0, sizeof(queues));

	params_inited = 1;
	return 0;
}


/**
 * parses a "pipe_no:algorithm:bandwidth" line
 * \return      0 on success
 */
static int parse_pipe_params(char * line, pipe_params_t * params)
{
	regmatch_t m[4];
	str algo_str;

	if (! params_inited && init_params())
		return -1;
	if (regexec(&pipe_params_regex, line, 4, m, 0)) {
		LM_ERR("invalid param tuple: %s\n", line);
		return -1;
	}
	LM_DBG("pipe: [%.*s|%.*s|%.*s]\n",
		RXLS(m, line, 1), RXLS(m, line, 2), RXLS(m, line, 3));
	
	params->no = atoi(RXS(m, line, 1));
	params->limit = atoi(RXS(m, line, 3));

	algo_str.s   = RXS(m, line, 2);
	algo_str.len = RXL(m, line, 2);
	if (str_map_str(algo_names, &algo_str, &params->algo))
		return -1;

	return 0;
}

/**
 * parses a "pipe_no:method" line
 * \return      0 on success
 */
static int parse_queue_params(char * line, rl_queue_params_t * params)
{
	regmatch_t m[3];
	int len;

	if (! params_inited && init_params())
		return -1;
	if (regexec(&queue_params_regex, line, 3, m, 0)) {
		LM_ERR("invalid param tuple: %s\n", line);
		return -1;
	}
	LM_DBG("queue: [%.*s|%.*s]\n",
		RXLS(m, line, 1), RXLS(m, line, 2));
	
	params->pipe = atoi(RXS(m, line, 1));

	len = RXL(m, line, 2);
	params->method.s = (char *)pkg_malloc(len+1);
	if (params->method.s == 0) {
		LM_ERR("no memory left for method in params\n");
		return -1;
	}
	params->method.len = len;
	memcpy(params->method.s, RXS(m, line, 2), len+1);

	return 0;
}

/**
 * checks that all FEEDBACK pipes use the same setpoint 
 * cpu load. also sets (common) cfg_setpoint value
 * \param	modparam 1 to check modparam (static) fields, 0 to use shm ones
 *
 * \return	0 if ok, -1 on error
 */
static int check_feedback_setpoints(int modparam)
{
        int i, sp;

	cfg_setpoint = -1;

	for (i=0; i<MAX_PIPES; i++)
		if (pipes[i].algo_mp == PIPE_ALGO_FEEDBACK) {
			sp = modparam ? pipes[i].limit_mp : *pipes[i].limit;

			if (sp < 0 || sp > 100) {
				LM_ERR("FEEDBACK cpu load must be >=0 and <= 100\n");
				return -1;
			} else if (cfg_setpoint == -1) {
				cfg_setpoint = sp;
			} else if (sp != cfg_setpoint) {
				LM_ERR("pipe %d: FEEDBACK cpu load values must "
					"be equal for all pipes\n", i);
				return -1;
			}
		}

	return 0;
}


static int add_pipe_params(modparam_t type, void * val)
{
	char * param_line = val;
	pipe_params_t params;

	if (parse_pipe_params(param_line, &params))
		return -1;
	
	if (params.no < 0 || params.no >= MAX_PIPES) {
		LM_ERR("pipe number %d not allowed (MAX_PIPES=%d, 0-based)\n",
			params.no, MAX_PIPES);
		return -1;
	}

	pipes[params.no].algo_mp = params.algo;
	pipes[params.no].limit_mp = params.limit;

	return check_feedback_setpoints(1);
}

static int add_queue_params(modparam_t type, void * val)
{
	char * param_line = val;
	rl_queue_params_t params;

	if (nqueues_mp >= MAX_QUEUES) {
		LM_ERR("MAX_QUEUES reached (%d)\n", MAX_QUEUES);
		return -1;
	}

	if (parse_queue_params(param_line, &params))
		return -1;

	if (params.pipe >= MAX_PIPES) {
		LM_ERR("pipe number %d not allowed (MAX_PIPES=%d, 0-based)\n",
			params.pipe, MAX_PIPES);
		return -1;
	}

	queues[nqueues_mp].pipe_mp = params.pipe;
	queues[nqueues_mp].method_mp = params.method;
	nqueues_mp++;

	return 0;
}


/* timer housekeeping, invoked each timer interval to reset counters */
static ticks_t rl_timer_handle(ticks_t ticks, struct timer_ln* tl, void* data)
{
	int i, len;
	char *c, *p;

	LOCK_GET(rl_lock);
	switch (*load_source) {
		case LOAD_SOURCE_CPU:
			update_cpu_load();
			break;
	}

	*network_load_value = get_total_bytes_waiting();

	if (rl_dbg_str->s) {
		c = p = rl_dbg_str->s;
		memset(c, ' ', rl_dbg_str->len);
		for (i=0; i<MAX_PIPES; i++) {
			c = int2str(*pipes[i].counter, &len);
			if (len < 4) {
				memcpy( p + (5-len), c, len );
			} else {
				memset(p, '*', 5);
				LM_WARN("Counter pipes[%d] to big: %d\n",
					i, *pipes[i].counter);
			}
			p = p + 5;
		}
		LM_WARN("%.*s\n", rl_dbg_str->len, rl_dbg_str->s);
	}

	for (i=0; i<MAX_PIPES; i++) {
		if( *pipes[i].algo == PIPE_ALGO_NETWORK ) {
			*pipes[i].load = ( *network_load_value > *pipes[i].limit ) ? 1 : -1;
		} else if (*pipes[i].limit && timer_interval) {
			*pipes[i].load = *pipes[i].counter / (*pipes[i].limit * timer_interval);
		}
		*pipes[i].last_counter = *pipes[i].counter;
		*pipes[i].counter = 0;
	}
	LOCK_RELEASE(rl_lock);
	return (ticks_t)(-1); /* periodical */
}


/* rpc function documentation */
static const char *rpc_stats_doc[2] = {
	"Print ratelimit statistics: PIPE[<pipe_id>]: \
<last_counter>/<pipe_limit> (drop rate: <drop_rate>)", 0
};

static const char *rpc_get_pipes_doc[2] = {
	"Print pipes info: PIPE[<pipe_id>]: \
<pipe_algo_id>:<pipe_algo> <last_counter>/<pipe_limit> (drop rate: <drop_rate>) [<current_counter>]", 0
};

static const char *rpc_set_pipe_doc[2] = {
	"Sets a pipe params: <pipe_id> <pipe_algorithm> <pipe_limit>", 0
};

static const char *rpc_get_queues_doc[2] = {
	"Print queues info: QUEUE[queue_id]: <pipe_id>:<queue_method>", 0
};

static const char *rpc_set_queue_doc[2] = {
	"Sets queue params: <quue_id> <queue_method> <pipe_id>", 0
};

static const char *rpc_get_pid_doc[2] = {
	"Print PID Controller parameters for the FEEDBACK algorithm: \
<ki> <kp> <kd>", 0
};

static const char *rpc_set_pid_doc[2] = {
	"Sets the PID Controller parameters for the FEEDBACK algorithm: \
<ki> <kp> <kd>", 0
};

static const char *rpc_push_load_doc[2] = {
	"Force the value of the load parameter for FEEDBACK algorithm: \
<load>", 0
};

static const char *rpc_set_dbg_doc[2] = {
	"Sets the ratelimit debug/monitoing logs: 0-off 1-on", 0
};


/* rpc function implementations */
static void rpc_stats(rpc_t *rpc, void *c) {
	int i;

	LOCK_GET(rl_lock);
	for (i=0; i<MAX_PIPES; i++) {
		if (rpc->rpl_printf(c, "PIPE[%d]: %d/%d (drop rate: %d)",
			i, *pipes[i].last_counter, *pipes[i].limit,
			*pipes[i].load) < 0) goto error;
	}
error:
	LOCK_RELEASE(rl_lock);
}

static void rpc_get_pipes(rpc_t *rpc, void *c) {
	str algo;
	int i;

	LOCK_GET(rl_lock);
	for (i=0; i<MAX_PIPES; i++) {
		if (*pipes[i].algo != PIPE_ALGO_NOP) {
			if (str_map_int(algo_names, *pipes[i].algo, &algo))
				goto error;
			if (rpc->rpl_printf(c, "PIPE[%d]: %d:%.*s %d/%d (drop rate: %d) [%d]",
				i, *pipes[i].algo, algo.len, algo.s,
				*pipes[i].last_counter, *pipes[i].limit,
				*pipes[i].load, *pipes[i].counter) < 0) goto error;
		}
	}
error:
	LOCK_RELEASE(rl_lock);
}

static void rpc_set_pipe(rpc_t *rpc, void *c) {
	int pipe_no = MAX_PIPES, algo_id, limit = 0;
	str algo_str;

	if (rpc->scan(c, "dSd", &pipe_no, &algo_str, &limit) < 3) return;

	if (str_map_str(algo_names, &algo_str, &algo_id)) {
		LM_ERR("unknown algorithm: '%.*s'\n", algo_str.len, algo_str.s);
		rpc->fault(c, 400, "Unknown algorithm");
		return;
	}

	LM_DBG("set_pipe: %d:%d:%d\n", pipe_no, algo_id, limit);

	if (pipe_no >= MAX_PIPES || pipe_no < 0) {
		LM_ERR("wrong pipe_no: %d\n", pipe_no);
		rpc->fault(c, 400, "Unknown pipe");
		return;
	}

	LOCK_GET(rl_lock);
	*pipes[pipe_no].algo = algo_id;
	*pipes[pipe_no].limit = limit;

	if (check_feedback_setpoints(0)) {
		LM_ERR("feedback limits don't match\n");
		rpc->fault(c, 400, "Feedback limits don't match");
	} else {
		*pid_setpoint = 0.01 * (double)cfg_setpoint;
	}

	LOCK_RELEASE(rl_lock);
}

static void rpc_get_queues(rpc_t *rpc, void *c) {
	int i;

	LOCK_GET(rl_lock);
	for (i=0; i<MAX_QUEUES; i++) {
		if (queues[i].pipe) {
			if (rpc->rpl_printf(c, "QUEUE[%d]: %d:%.*s",
				i, *queues[i].pipe,
				(*queues[i].method).len,
				(*queues[i].method).s) < 0) goto error;
		}
	}
error:
	LOCK_RELEASE(rl_lock);
}

static void rpc_set_queue(rpc_t *rpc, void *c) {
	unsigned int queue_no = MAX_QUEUES, pipe_no = MAX_PIPES;
	str method, method_buf;


	if (rpc->scan(c, "dSd", &queue_no, &method, &pipe_no) < 3) return;

	if (pipe_no >= MAX_PIPES || (int)pipe_no < 0) {
		LM_ERR("Invalid pipe number: %d\n", pipe_no);
		rpc->fault(c, 400, "Invalid pipe number");
		return;
	}

	if (str_cpy(&method_buf, &method)) {
		LM_ERR("out of memory\n");
		rpc->fault(c, 400, "OOM");
		return;
	}

	LOCK_GET(rl_lock);
	if (queue_no >= *nqueues) {
		LM_ERR("MAX_QUEUES reached for queue: %d\n", queue_no);
		rpc->fault(c, 400, "MAX_QUEUES reached");
		LOCK_RELEASE(rl_lock);
		return;
	}

	*queues[queue_no].pipe = pipe_no;
	if (!queues[queue_no].method->s)
		shm_free(queues[queue_no].method->s);
	queues[queue_no].method->s = method_buf.s;
	queues[queue_no].method->len = method_buf.len;
	LOCK_RELEASE(rl_lock);
}

static void rpc_get_pid(rpc_t *rpc, void *c) {
	rpc->rpl_printf(c, "ki[%f] kp[%f] kd[%f] ", *pid_ki, *pid_kp, *pid_kd);
}

static void rpc_set_pid(rpc_t *rpc, void *c) {
	double ki, kp, kd;

	if (rpc->scan(c, "fff", &ki, &kp, &kd) < 3) return;

	LOCK_GET(rl_lock);
	*pid_ki = ki;
	*pid_kp = kp;
	*pid_kd = kd;
	LOCK_RELEASE(rl_lock);
}


static void rpc_push_load(rpc_t *rpc, void *c) {
	double value;

	if (rpc->scan(c, "f", &value) < 1) return;

	if (value < 0.0 || value > 1.0) {
		LM_ERR("value out of range: %0.3f in not in [0.0,1.0]\n", value);
		rpc->fault(c, 400, "Value out of range");
		return;
	}
	LOCK_GET(rl_lock);
	*load_value = value;
	LOCK_RELEASE(rl_lock);

	do_update_load();
}

static void rpc_set_dbg(rpc_t *rpc, void *c) {
	int dbg_mode = 0;

	if (rpc->scan(c, "d", &dbg_mode) < 1) return;

	LOCK_GET(rl_lock);
	if (dbg_mode) {
		if (!rl_dbg_str->s) {
			rl_dbg_str->len = (MAX_PIPES * 5 * sizeof(char));
			rl_dbg_str->s = (char *)shm_malloc(rl_dbg_str->len);
			if (!rl_dbg_str->s) {
				rl_dbg_str->len = 0;
				LM_ERR("oom: %d\n", rl_dbg_str->len);
			}
		}
	} else {
		if (rl_dbg_str->s) {
			shm_free(rl_dbg_str->s);
			rl_dbg_str->s = NULL;
			rl_dbg_str->len = 0;
		}
	}
	LOCK_RELEASE(rl_lock);
}

static rpc_export_t rpc_methods[] = {
	{"rl.stats",		rpc_stats,	rpc_stats_doc,		0},
	{"rl.get_pipes",	rpc_get_pipes,	rpc_get_pipes_doc,	0},
	{"rl.set_pipe",		rpc_set_pipe,	rpc_set_pipe_doc,	0},
	{"rl.get_queues",	rpc_get_queues,	rpc_get_queues_doc,	0},
	{"rl.set_queue",	rpc_set_queue,	rpc_set_queue_doc,	0},
	{"rl.get_pid",		rpc_get_pid,	rpc_get_pid_doc,	0},
	{"rl.set_pid",		rpc_set_pid,	rpc_set_pid_doc,	0},
	{"rl.push_load",	rpc_push_load,	rpc_push_load_doc,	0},
	{"rl.set_dbg",		rpc_set_dbg,	rpc_set_dbg_doc,	0},
	{0, 0, 0, 0}
};

