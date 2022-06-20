/*
 * Benchmarking module for Kamailio
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
 * Copyright (C) 2007 Voice Sistem SRL
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
 * \brief Benchmark :: Module Core
 *
 * \ingroup benchmark
 * - Module: benchmark
 */

/*! \defgroup benchmark Benchmark :: Developer benchmarking module
 *
 * This module is for Kamailio developers, as well as admins. It gives
 * a possibility to clock certain critical paths in module code or
 * configuration sections.
 *
 */

#define _GNU_SOURCE
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include "../../core/sr_module.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"


#include "benchmark.h"

#include "../../core/mem/shm_mem.h"


MODULE_VERSION

static int bm_init_rpc(void);
static int bm_init_mycfg(void);
int bm_register_timer_param(modparam_t type, void* val);

/* Exported functions */
int bm_start_timer(struct sip_msg* _msg, char* timer, char *foobar);
int bm_log_timer(struct sip_msg* _msg, char* timer, char* mystr);

/*
 * Module destroy function prototype
 */
static void destroy(void);


/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Exported parameters
 * Copied to mycfg on module initialization
 */
static int bm_enable_global = 0;
static int bm_granularity = 1;
static int bm_loglevel = L_INFO;

static int _bm_last_time_diff = 0;

/*
 * Module setup
 */

typedef struct bm_cfg {
	int enable_global;
	int granularity;
	int loglevel;
	/* The internal timers */
	int nrtimers;
	benchmark_timer_t *timers;
	benchmark_timer_t **tindex;
} bm_cfg_t;

/*
 * The setup is located in shared memory so that
 * all instances can access this variable
 */

bm_cfg_t *bm_mycfg = 0;

static inline int fixup_bm_timer(void** param, int param_no);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{ "bm_start_timer", (cmd_function)bm_start_timer, 1, fixup_bm_timer, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "bm_log_timer",   (cmd_function)bm_log_timer, 1, fixup_bm_timer, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{"load_bm",         (cmd_function)load_bm, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0 }
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"enable",      INT_PARAM, &bm_enable_global},
	{"granularity", INT_PARAM, &bm_granularity},
	{"loglevel",    INT_PARAM, &bm_loglevel},
	{"register",    PARAM_STRING|USE_FUNC_PARAM, (void*)bm_register_timer_param},

	{ 0, 0, 0 }
};


static int bm_get_time_diff(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

static pv_export_t mod_items[] = {
	{ {"BM_time_diff", sizeof("BM_time_diff")-1}, PVT_OTHER, bm_get_time_diff, 0,
		0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 * Module interface
 */
struct module_exports exports = {
	"benchmark",
	DEFAULT_DLFLAGS,
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported RPC methods */
	mod_items,  /* exported pseudo-variables */
	0,          /* response function */
	mod_init,   /* module initialization function */
	0,          /* child initialization function */
	destroy     /* destroy function */
};


/****************/

/*
 * mod_init
 * Called by Kamailio at init time
 */
static int mod_init(void)
{

	if(bm_init_rpc()<0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(bm_init_mycfg()<0) {
		return -1;
	}

	return 0;
}


/*
 * destroy
 * called by Kamailio at exit time
 */
static void destroy(void)
{
	benchmark_timer_t *bmt = 0;
	benchmark_timer_t *bmp = 0;

	if(bm_mycfg!=NULL)
	{
		/* free timers list */
		bmt = bm_mycfg->timers;
		while(bmt)
		{
			bmp = bmt;
			bmt = bmt->next;
			shm_free(bmp);
		}
		if(bm_mycfg->tindex) shm_free(bm_mycfg->tindex);
		shm_free(bm_mycfg);
	}
}

/**
 * 
 */
static int bm_init_mycfg(void)
{
	if(bm_mycfg!=NULL) {
		LM_DBG("config structure initialized\n");
		return 0;
	}
	bm_mycfg = (bm_cfg_t*)shm_malloc(sizeof(bm_cfg_t));
	if(bm_mycfg==NULL) {
		LM_ERR("failed to allocated shared memory\n");
		return -1;
	}
	memset(bm_mycfg, 0, sizeof(bm_cfg_t));
	bm_mycfg->enable_global = bm_enable_global;
	bm_mycfg->granularity   = bm_granularity;
	bm_mycfg->loglevel      = bm_loglevel;

	return 0;
}

void bm_reset_timer(int i)
{
	if(bm_mycfg==NULL || bm_mycfg->tindex[i]==NULL)
		return;
	bm_mycfg->tindex[i]->calls = 0;
	bm_mycfg->tindex[i]->sum = 0;
	bm_mycfg->tindex[i]->last_max = 0;
	bm_mycfg->tindex[i]->last_min = 0xffffffff;
	bm_mycfg->tindex[i]->last_sum = 0;
	bm_mycfg->tindex[i]->global_max = 0;
	bm_mycfg->tindex[i]->global_min = 0xffffffff;
	bm_mycfg->tindex[i]->period_sum = 0;
	bm_mycfg->tindex[i]->period_max = 0;
	bm_mycfg->tindex[i]->period_min = 0xffffffff;
}

void reset_timers(void)
{
	int i;
	if(bm_mycfg==NULL)
		return;

	for (i = 0; i < bm_mycfg->nrtimers; i++)
		bm_reset_timer(i);
}

/*! \brief
 * timer_active().
 *
 * Global enable mode can be:
 * -1 - All timing disabled
 *  0 - Timing enabled, watch for single timers enabled (default: off)
 *  1 - Timing enabled for all timers
 */

static inline int timer_active(unsigned int id)
{
	if (bm_mycfg->enable_global > 0 || bm_mycfg->timers[id].enabled > 0)
		return 1;
	else
		return 0;
}


/*! \brief
 * start_timer()
 */

int _bm_start_timer(unsigned int id)
{
	if (timer_active(id))
	{
		if(bm_get_time(bm_mycfg->tindex[id]->start)!=0)
		{
			LM_ERR("error getting current time\n");
			return -1;
		}
	}

	return 1;
}

int bm_start_timer(struct sip_msg* _msg, char* timer, char *foobar)
{
	return _bm_start_timer((unsigned int)(unsigned long)timer);
}


/*! \brief
 * log_timer()
 */

int _bm_log_timer(unsigned int id)
{
	/* BM_CLOCK_REALTIME */
	bm_timeval_t now;
	unsigned long long tdiff;

	if (!timer_active(id))
		return 1;

	if(bm_get_time(&now)<0)
	{
		LM_ERR("error getting current time\n");
		return -1;
	}

	tdiff = bm_diff_time(bm_mycfg->tindex[id]->start, &now);
	_bm_last_time_diff = (int)tdiff;

	/* What to do
	 * - update min, max, sum
	 * - if granularity hit: Log, reset min/max
	 */

	bm_mycfg->tindex[id]->sum += tdiff;
	bm_mycfg->tindex[id]->last_sum += tdiff;
	bm_mycfg->tindex[id]->calls++;

	if (tdiff < bm_mycfg->tindex[id]->last_min)
		bm_mycfg->tindex[id]->last_min = tdiff;

	if (tdiff > bm_mycfg->tindex[id]->last_max)
		bm_mycfg->tindex[id]->last_max = tdiff;

	if (tdiff < bm_mycfg->tindex[id]->global_min)
		bm_mycfg->tindex[id]->global_min = tdiff;

	if (tdiff > bm_mycfg->tindex[id]->global_max)
		bm_mycfg->tindex[id]->global_max = tdiff;


	if ((bm_mycfg->tindex[id]->calls % bm_mycfg->granularity) == 0)
	{
		LM_GEN1(bm_mycfg->loglevel, "benchmark (timer %s [%d]): %llu ["
			" msgs/total/min/max/avg - LR:"
			" %i/%llu/%llu/%llu/%f | GB: %llu/%llu/%llu/%llu/%f]\n",
			bm_mycfg->tindex[id]->name,
			id,
			tdiff,
			bm_mycfg->granularity,
			bm_mycfg->tindex[id]->last_sum,
			bm_mycfg->tindex[id]->last_min,
			bm_mycfg->tindex[id]->last_max,
			((double)bm_mycfg->tindex[id]->last_sum)/bm_mycfg->granularity,
			bm_mycfg->tindex[id]->calls,
			bm_mycfg->tindex[id]->sum,
			bm_mycfg->tindex[id]->global_min,
			bm_mycfg->tindex[id]->global_max,
			((double)bm_mycfg->tindex[id]->sum)/bm_mycfg->tindex[id]->calls);

		/* Fill data for last period. */
		bm_mycfg->tindex[id]->period_sum = bm_mycfg->tindex[id]->last_sum;
		bm_mycfg->tindex[id]->period_max = bm_mycfg->tindex[id]->last_max;
		bm_mycfg->tindex[id]->period_min = bm_mycfg->tindex[id]->last_min;
		
		bm_mycfg->tindex[id]->last_sum = 0;
		bm_mycfg->tindex[id]->last_max = 0;
		bm_mycfg->tindex[id]->last_min = 0xffffffff;
	}

	return 1;
}

int bm_log_timer(struct sip_msg* _msg, char* timer, char* mystr)
{
	return _bm_log_timer((unsigned int)(unsigned long)timer);
}


int _bm_register_timer(char *tname, int mode, unsigned int *id)
{
	benchmark_timer_t *bmt = 0;
	benchmark_timer_t **tidx = 0;

	if(tname==NULL || id==NULL || bm_mycfg==NULL || strlen(tname)==0
			|| strlen(tname)>BM_NAME_LEN-1)
		return -1;

	bmt = bm_mycfg->timers;
	while(bmt)
	{
		if(strcmp(bmt->name, tname)==0)
		{
			*id = bmt->id;
			return 0;
		}
		bmt = bmt->next;
	}
	if(mode==0)
		return -1;

	bmt = (benchmark_timer_t*)shm_malloc(sizeof(benchmark_timer_t));

	if(bmt==0)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(bmt, 0, sizeof(benchmark_timer_t));

	/* private memory, otherwise we have races */
	bmt->start = (bm_timeval_t*)pkg_malloc(sizeof(bm_timeval_t));
	if(bmt->start == NULL)
	{
		shm_free(bmt);
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(bmt->start, 0, sizeof(bm_timeval_t));

	strcpy(bmt->name, tname);
	if(bm_mycfg->timers==0)
	{
		bmt->id = 0;
		bm_mycfg->timers = bmt;
	} else {
		bmt->id = bm_mycfg->timers->id+1;
		bmt->next = bm_mycfg->timers;
		bm_mycfg->timers = bmt;
	}

	/* do the indexing */
	if(bmt->id%10==0)
	{
		if(bm_mycfg->tindex!=NULL)
			tidx = bm_mycfg->tindex;
		bm_mycfg->tindex = (benchmark_timer_t**)shm_malloc((10+bmt->id)*
								sizeof(benchmark_timer_t*));
		if(bm_mycfg->tindex==0)
		{
			LM_ERR("no more share memory\n");
			if(tidx!=0)
				shm_free(tidx);
			return -1;
		}
		memset(bm_mycfg->tindex, 0, (10+bmt->id)*sizeof(benchmark_timer_t*));
		if(tidx!=0)
		{
			memcpy(bm_mycfg->tindex, tidx, bmt->id*sizeof(benchmark_timer_t*));
			shm_free(tidx);
		}
	}
	bm_mycfg->tindex[bmt->id] = bmt;
	bm_mycfg->nrtimers = bmt->id + 1;
	bm_reset_timer(bmt->id);
	*id = bmt->id;
	LM_DBG("timer [%s] added with index <%u>\n", bmt->name, bmt->id);

	return 0;
}

int bm_register_timer_param(modparam_t type, void* val)
{
	unsigned int tid;

	if(bm_init_mycfg()<0) {
		return -1;
	}
	if((_bm_register_timer((char*)val, 1, &tid))!=0) {
		LM_ERR("cannot find timer [%s]\n", (char*)val);
		return -1;
	}
	LM_INFO("timer [%s] registered: %u\n", (char*)val, tid);
	return 0;
}

static int ki_bm_start_timer(struct sip_msg* _msg, str* tname)
{
	unsigned int tid;

	if((_bm_register_timer(tname->s, 0, &tid))!=0) {
			LM_ERR("cannot find timer [%s]\n", tname->s);
			return -1;
	}

	return _bm_start_timer(tid);
}

static int ki_bm_log_timer(sip_msg_t* _msg, str* tname)
{
	unsigned int tid;

	if((_bm_register_timer(tname->s, 0, &tid))!=0) {
			LM_ERR("cannot find timer [%s]\n", tname->s);
			return -1;
	}
	return _bm_log_timer(tid);
}

/*! \brief API Binding */
int load_bm( struct bm_binds *bmb)
{
	if(bmb==NULL)
		return -1;

	bmb->bm_register = _bm_register_timer;
	bmb->bm_start    = _bm_start_timer;
	bmb->bm_log	     = _bm_log_timer;

	return 1;
}


/*! \brief PV get function for time diff */
static int bm_get_time_diff(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL)
		return -1;
	return pv_get_sintval(msg, param, res, _bm_last_time_diff);
}


static inline int fixup_bm_timer(void** param, int param_no)
{
	unsigned int tid = 0;
	if (param_no == 1)
	{
		if((_bm_register_timer((char*)(*param), 1, &tid))!=0)
		{
			LM_ERR("cannot register timer [%s]\n", (char*)(*param));
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param = (void*)(unsigned long)tid;
	}
	return 0;
}

/*! \name benchmark rpc functions */
/*@{ */

/*! \brief
 * Expects 1 node: 0 for disable, 1 for enable
 */
void bm_rpc_enable_global(rpc_t* rpc, void* ctx)
{
	int v1=0;
	if(rpc->scan(ctx, "d", &v1)<1) {
		LM_WARN("no parameters\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if ((v1 < -1) || (v1 > 1)) {
		rpc->fault(ctx, 500, "Invalid Parameter Value");
		return;
	}
	bm_mycfg->enable_global = v1;
}


void bm_rpc_enable_timer(rpc_t* rpc, void* ctx)
{
	char *p1 = NULL;
	int v2 = 0;
	unsigned int id = 0;

	if(rpc->scan(ctx, "sd", &p1, &v2)<2) {
		LM_WARN("invalid parameters\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if ((v2 < 0) || (v2 > 1)) {
		rpc->fault(ctx, 500, "Invalid Parameter Value");
		return;
	}
	if(_bm_register_timer(p1, 0, &id)!=0) {
		rpc->fault(ctx, 500, "Register timer failure");
		return;
	}
	bm_mycfg->timers[id].enabled = v2;
}


void bm_rpc_granularity(rpc_t* rpc, void* ctx)
{
	int v1 = 0;
	if(rpc->scan(ctx, "d", &v1)<1) {
		LM_WARN("no parameters\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if (v1 < 1) {
		rpc->fault(ctx, 500, "Invalid Parameter Value");
		return;
	}
	bm_mycfg->granularity = v1;
}

void bm_rpc_loglevel(rpc_t* rpc, void* ctx)
{
	int v1 = 0;
	if(rpc->scan(ctx, "d", &v1)<1) {
		LM_WARN("no parameters\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if ((v1 < -1) || (v1 > 1)) {
		rpc->fault(ctx, 500, "Invalid Parameter Value");
		return;
	}
	bm_mycfg->loglevel = v1;
}

/**
 * Internal buffer to convert llu numbers into strings.
 */
#define BUFFER_S_LEN 100
static char buffer_s[BUFFER_S_LEN];

/**
 * Create a RPC structure for a timer.
 *
 * /return 0 on success.
 */
int bm_rpc_timer_struct(rpc_t* rpc, void* ctx, int id)
{
	void *handle; /* Handle for RPC structure. */

	/* Create empty structure and obtain its handle */
	if (rpc->add(ctx, "{", &handle) < 0) {
		return -1;
	}
		
	int enabled = timer_active(id);

	if (rpc->struct_add(handle, "s", "name", bm_mycfg->tindex[id]->name) < 0) {
		return -1;
	}
		
	if (rpc->struct_add(handle, "s", "state", (enabled==0)?"disabled":"enabled") < 0) {
		return -1;
	}

	if (rpc->struct_add(handle, "d", "id", id) < 0) {
		return -1;
	}

	if (rpc->struct_add(handle, "d", "granularity", bm_mycfg->granularity) < 0) {
		return -1;
	}

	/* We use a string to represent long long unsigned integers. */
	int len;
	len = snprintf(buffer_s, BUFFER_S_LEN, "%llu", bm_mycfg->tindex[id]->period_sum);
	if (len <= 0 || len >= BUFFER_S_LEN) {
		LM_ERR("Buffer overflow\n");
		return -1;
	}
	if (rpc->struct_add(handle, "s", "period_sum", buffer_s) < 0) {
		return -1;
	}

	len = snprintf(buffer_s, BUFFER_S_LEN, "%llu", bm_mycfg->tindex[id]->period_min);
	if (len <= 0 || len >= BUFFER_S_LEN) {
		LM_ERR("Buffer overflow\n");
		return -1;
	}
	if (rpc->struct_add(handle, "s", "period_min", buffer_s) < 0) {
		return -1;
	}

	len = snprintf(buffer_s, BUFFER_S_LEN, "%llu", bm_mycfg->tindex[id]->period_max);
	if (len <= 0 || len >= BUFFER_S_LEN) {
		LM_ERR("Buffer overflow\n");
		return -1;
	}
	if (rpc->struct_add(handle, "s", "period_max", buffer_s) < 0) {
		return -1;
	}

	if (bm_mycfg->granularity > 0) {
		double media = ((double)bm_mycfg->tindex[id]->period_sum)/bm_mycfg->granularity;

		if (rpc->struct_add(handle, "f", "period_media", media) < 0) {
			return -1;
		}
	}

	len = snprintf(buffer_s, BUFFER_S_LEN, "%llu", bm_mycfg->tindex[id]->calls);
	if (len <= 0 || len >= BUFFER_S_LEN) {
		LM_ERR("Buffer overflow\n");
		return -1;
	}
	if (rpc->struct_add(handle, "s", "calls", buffer_s) < 0) {
		return -1;
	}

	len = snprintf(buffer_s, BUFFER_S_LEN, "%llu", bm_mycfg->tindex[id]->sum);
	if (len <= 0 || len >= BUFFER_S_LEN) {
		LM_ERR("Buffer overflow\n");
		return -1;
	}
	if (rpc->struct_add(handle, "s", "sum", buffer_s) < 0) {
		return -1;
	}

	len = snprintf(buffer_s, BUFFER_S_LEN, "%llu", bm_mycfg->tindex[id]->global_min);
	if (len <= 0 || len >= BUFFER_S_LEN) {
		LM_ERR("Buffer overflow\n");
		return -1;
	}
	if (rpc->struct_add(handle, "s", "global_min", buffer_s) < 0) {
		return -1;
	}

	len = snprintf(buffer_s, BUFFER_S_LEN, "%llu", bm_mycfg->tindex[id]->global_max);
	if (len <= 0 || len >= BUFFER_S_LEN) {
		LM_ERR("Buffer overflow\n");
		return -1;
	}
	if (rpc->struct_add(handle, "s", "global_max", buffer_s) < 0) {
		return -1;
	}

	if (bm_mycfg->tindex[id]->calls > 0) {
		double media = ((double)bm_mycfg->tindex[id]->sum)/bm_mycfg->tindex[id]->calls;

		if (rpc->struct_add(handle, "f", "global_media", media) < 0) {
			return -1;
		}
	}

	return 0;
}

void bm_rpc_timer_list(rpc_t* rpc, void* ctx)
{
	int id;
	
	for (id = 0; id < bm_mycfg->nrtimers; id++) {

		if (bm_rpc_timer_struct(rpc, ctx, id)) {
			LM_ERR("Failure writing RPC structure for timer: %d\n", id);
			return;
		}

	} /* for (id = 0; id < bm_mycfg->nrtimers; id++) */

	return;
}

void bm_rpc_timer_name_list(rpc_t* rpc, void* ctx)
{
	char *name = NULL;
	unsigned int id = 0;

	if(rpc->scan(ctx, "s", &name) < 1) {
		LM_WARN("invalid timer name\n");
		rpc->fault(ctx, 400, "Invalid timer name");
		return;
	}
	if(_bm_register_timer(name, 0, &id)!=0) {
		rpc->fault(ctx, 500, "Register timer failure");
		return;
	}

	if (bm_rpc_timer_struct(rpc, ctx, id)) {
		LM_ERR("Failure writing RPC structure for timer: %d\n", id);
		return;
	}

	return;
}

static const char* bm_rpc_enable_global_doc[2] = {
	"Enable/disable benchmarking",
	0
};

static const char* bm_rpc_enable_timer_doc[2] = {
	"Enable/disable a benchmark timer",
	0
};

static const char* bm_rpc_granularity_doc[2] = {
	"Set benchmarking granularity",
	0
};

static const char* bm_rpc_loglevel_doc[2] = {
	"Set benchmarking log level",
	0
};

static const char* bm_rpc_timer_list_doc[2] = {
	"List all timers",
	0
};

static const char* bm_rpc_timer_name_list_doc[2] = {
	"List a timer based on its name",
	0
};

rpc_export_t bm_rpc_cmds[] = {
	{"benchmark.enable_global", bm_rpc_enable_global,
		bm_rpc_enable_global_doc, 0},
	{"benchmark.enable_timer", bm_rpc_enable_timer,
		bm_rpc_enable_timer_doc, 0},
	{"benchmark.granularity", bm_rpc_granularity,
		bm_rpc_granularity_doc, 0},
	{"benchmark.loglevel", bm_rpc_loglevel,
		bm_rpc_loglevel_doc, 0},
	{"benchmark.timer_list", bm_rpc_timer_list,
		bm_rpc_timer_list_doc, 0},
	{"benchmark.timer_name_list", bm_rpc_timer_name_list,
		bm_rpc_timer_name_list_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
static int bm_init_rpc(void)
{
	if (rpc_register_array(bm_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_benchmark_exports[] = {
	{ str_init("benchmark"), str_init("bm_start_timer"),
		SR_KEMIP_INT, ki_bm_start_timer,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("benchmark"), str_init("bm_log_timer"),
		SR_KEMIP_INT, ki_bm_log_timer,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_benchmark_exports);
	return 0;
}

/*@} */

/* End of file */
