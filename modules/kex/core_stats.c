/*
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * History:
 * ---------
 *  2006-01-23  first version (bogdan)
 *  2006-11-28  Added statistics for the number of bad URI's, methods, and 
 *              proxy requests (Jeffrey Magder - SOMA Networks)
 */

/*!
 * \file
 * \brief KEX :: Kamailio Core statistics
 * \ingroup kex
 */


#include <string.h>

#include "../../lib/kcore/statistics.h"
#include "../../lib/kmi/mi.h"
#include "../../events.h"
#include "../../dprint.h"
#include "../../timer.h"
#include "../../parser/msg_parser.h"
#include "../../script_cb.h"
#include "../../mem/meminfo.h"
#include "../../mem/shm_mem.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"


#ifdef STATISTICS

stat_var* rcv_reqs;				/*!< received requests        */
stat_var* rcv_rpls;				/*!< received replies         */
stat_var* fwd_reqs;				/*!< forwarded requests       */
stat_var* fwd_rpls;				/*!< forwarded replies        */
stat_var* drp_reqs;				/*!< dropped requests         */
stat_var* drp_rpls;				/*!< dropped replies          */
stat_var* err_reqs;				/*!< error requests           */
stat_var* err_rpls;				/*!< error replies            */
stat_var* bad_URIs;				/*!< number of bad URIs       */
stat_var* unsupported_methods;			/*!< unsupported methods      */
stat_var* bad_msg_hdr;				/*!< messages with bad header */


/*! exported core statistics */
stat_export_t core_stats[] = {
	{"rcv_requests" ,         0,  &rcv_reqs              },
	{"rcv_replies" ,          0,  &rcv_rpls              },
	{"fwd_requests" ,         0,  &fwd_reqs              },
	{"fwd_replies" ,          0,  &fwd_rpls              },
	{"drop_requests" ,        0,  &drp_reqs              },
	{"drop_replies" ,         0,  &drp_rpls              },
	{"err_requests" ,         0,  &err_reqs              },
	{"err_replies" ,          0,  &err_rpls              },
	{"bad_URIs_rcvd",         0,  &bad_URIs              },
	{"unsupported_methods",   0,  &unsupported_methods   },
	{"bad_msg_hdr",           0,  &bad_msg_hdr           },
	{0,0,0}
};

unsigned long shm_stats_get_size(void);
unsigned long shm_stats_get_used(void);
unsigned long shm_stats_get_rused(void);
unsigned long shm_stats_get_mused(void);
unsigned long shm_stats_get_free(void);
unsigned long shm_stats_get_frags(void);

stat_export_t shm_stats[] = {
	{"total_size" ,     STAT_IS_FUNC,    (stat_var**)shm_stats_get_size     },
	{"used_size" ,      STAT_IS_FUNC,    (stat_var**)shm_stats_get_used     },
	{"real_used_size" , STAT_IS_FUNC,    (stat_var**)shm_stats_get_rused    },
	{"max_used_size" ,  STAT_IS_FUNC,    (stat_var**)shm_stats_get_mused    },
	{"free_size" ,      STAT_IS_FUNC,    (stat_var**)shm_stats_get_free     },
	{"fragments" ,      STAT_IS_FUNC,    (stat_var**)shm_stats_get_frags    },
	{0,0,0}
};

static struct mi_root *mi_get_stats(struct mi_root *cmd, void *param);
static struct mi_root *mi_reset_stats(struct mi_root *cmd, void *param);
static struct mi_root *mi_clear_stats(struct mi_root *cmd, void *param);

static mi_export_t mi_stat_cmds[] = {
	{ "get_statistics",    mi_get_stats,    0  ,  0,  0 },
	{ "reset_statistics",  mi_reset_stats,  0  ,  0,  0 },
	{ "clear_statistics",  mi_clear_stats,  0  ,  0,  0 },
	{ 0, 0, 0, 0, 0}
};

int stats_proc_stats_init_rpc(void);


int register_mi_stats(void)
{
	/* register MI commands */
	if (register_mi_mod("core", mi_stat_cmds)<0) {
		LM_ERR("unable to register MI cmds\n");
		return -1;
	}
	return 0;
}

static int km_cb_req_stats(struct sip_msg *msg,
		unsigned int flags, void *param)
{
	update_stat(rcv_reqs, 1);
	if(!IS_SIP(msg))
		return 1;
	if(msg->first_line.u.request.method_value==METHOD_OTHER)
		update_stat(unsupported_methods, 1);
	return 1;
}

static int km_cb_rpl_stats(struct sip_msg *msg,
		unsigned int flags, void *param)
{
	update_stat(rcv_rpls, 1);
	return 1;
}


static int sts_update_core_stats(void *data)
{
	int type;

	type = (int)(long)data;
	switch(type) {
		case 1:
			/* fwd_requests */
			update_stat(fwd_reqs, 1);
		break;
		case 2:
			/* fwd_replies */
			update_stat(fwd_rpls, 1);
		break;
		case 3:
			/* drop_requests */
			update_stat(drp_reqs, 1);
		break;
		case 4:
			/* drop_replies */
			update_stat(drp_rpls, 1);
		break;
		case 5:
			/* err_requests */
			update_stat(err_reqs, 1);
		break;
		case 6:
			/* err_replies */
			update_stat(err_rpls, 1);
		break;
		case 7:
			/* bad_URIs_rcvd */
			update_stat(bad_URIs, 1);
		break;
		case 8:
			/* bad_msg_hdr */
			update_stat(bad_msg_hdr, 1);
		break;
	}
	return 0;
}

int register_core_stats(void)
{
	/* register core statistics */
	if (register_module_stats( "core", core_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
	/* register sh_mem statistics */
	if (register_module_stats( "shmem", shm_stats)!=0 ) {
		LM_ERR("failed to register sh_mem statistics\n");
		return -1;
	}
	if (register_script_cb(km_cb_req_stats, PRE_SCRIPT_CB|REQUEST_CB, 0)<0 ) {
		LM_ERR("failed to register PRE request callback\n");
		return -1;
	}
	if (register_script_cb(km_cb_rpl_stats, PRE_SCRIPT_CB|ONREPLY_CB, 0)<0 ) {
		LM_ERR("failed to register PRE request callback\n");
		return -1;
	}
	if (stats_proc_stats_init_rpc()<0) return -1;
	sr_event_register_cb(SREV_CORE_STATS, sts_update_core_stats);

	return 0;
}


/***************************** RPC STUFF *******************************/

/**
 * Parameters for RPC callback functions.
 */
struct rpc_list_params {
	rpc_t* rpc;
	void* ctx;
	int clear;
};


/**
 * Satistic getter RPC callback.
 */
static void rpc_get_grp_vars_cbk(void* p, str* g, str* n, counter_handle_t h)
{
	struct rpc_list_params *packed_params;
	rpc_t* rpc;
	void* ctx;

	packed_params = p;
	rpc = packed_params->rpc;
	ctx = packed_params->ctx;

	rpc->rpl_printf(ctx, "%.*s:%.*s = %lu",
		g->len, g->s, n->len, n->s, counter_get_val(h));
}

/**
 * Group statistic getter RPC callback.
 */
static void rpc_get_all_grps_cbk(void* p, str* g)
{
	counter_iterate_grp_vars(g->s, rpc_get_grp_vars_cbk, p);
}

/**
 * All statistic getter RPC callback.
 */
static void stats_get_all(rpc_t* rpc, void* ctx, char* stat)
{
	int len = strlen(stat);
	struct rpc_list_params packed_params;
	str s_statistic;
	stat_var *s_stat;

	if (len==3 && strcmp("all", stat)==0) {
		packed_params.rpc = rpc;
		packed_params.ctx = ctx;
		counter_iterate_grp_names(rpc_get_all_grps_cbk, &packed_params);
	}
	else if (stat[len-1]==':') {
		packed_params.rpc = rpc;
		packed_params.ctx = ctx;
		stat[len-1] = '\0';
		counter_iterate_grp_vars(stat, rpc_get_grp_vars_cbk, &packed_params);
		stat[len-1] = ':';
	}
	else {
		s_statistic.s = stat;
		s_statistic.len = strlen(stat);
		s_stat = get_stat(&s_statistic);
		if (s_stat) {
			rpc->rpl_printf(ctx, "%s:%s = %lu",
				ZSW(get_stat_module(s_stat)), ZSW(get_stat_name(s_stat)),
				get_stat_val(s_stat));
		}
	}
}

/**
 * RPC statistics getter.
 */
static void rpc_stats_get_statistics(rpc_t* rpc, void* ctx)
{
	char* stat;

	if (stats_support()==0) {
		rpc->fault(ctx, 400, "stats support not enabled");
		return;
	}
	if (rpc->scan(ctx, "s", &stat) < 1) {
		rpc->fault(ctx, 400, "Please provide which stats to retrieve");
		return;
	}
	stats_get_all(rpc, ctx, stat);
	while((rpc->scan(ctx, "*s", &stat)>0)) {
		stats_get_all(rpc, ctx, stat);
	}
	return;
}


/**
 * Satistic reset/clear-er RPC callback..
 */
static void rpc_reset_or_clear_grp_vars_cbk(void* p, str* g, str* n, counter_handle_t h)
{
	struct rpc_list_params *packed_params;
	rpc_t* rpc;
	void* ctx;
	int clear;
	stat_var *s_stat;
	long old_val, new_val;

	packed_params = p;
	rpc = packed_params->rpc;
	ctx = packed_params->ctx;
	clear = packed_params->clear;
	s_stat = get_stat(n);
	if (s_stat) {
		if (clear) {
			old_val=get_stat_val(s_stat);
			reset_stat(s_stat);
			new_val=get_stat_val(s_stat);
			if (old_val==new_val) {
				rpc->rpl_printf(ctx, "%s:%s = %lu",
					ZSW(get_stat_module(s_stat)), ZSW(get_stat_name(s_stat)),
					new_val);
			}
			else {
				rpc->rpl_printf(ctx, "%s:%s = %lu (%lu)",
					ZSW(get_stat_module(s_stat)), ZSW(get_stat_name(s_stat)),
					new_val, old_val);
			}
		}
		else {
			reset_stat(s_stat);
		}
	}
}

/**
 * Group statistics reset/clear-er RPC callback.
 */
static void rpc_reset_or_clear_all_grps_cbk(void* p, str* g)
{
	counter_iterate_grp_vars(g->s, rpc_reset_or_clear_grp_vars_cbk, p);
}

/**
 * All statistics reset/clear-er RPC callback.
 */
static void stats_reset_or_clear_all(rpc_t* rpc, void* ctx, char* stat, int clear)
{
	int len = strlen(stat);
	struct rpc_list_params packed_params;
	str s_statistic;
	stat_var *s_stat;
	long old_val, new_val;

	if (len==3 && strcmp("all", stat)==0) {
		packed_params.rpc   = rpc;
		packed_params.ctx   = ctx;
		packed_params.clear = clear;
		counter_iterate_grp_names(rpc_reset_or_clear_all_grps_cbk, &packed_params);
	}
	else if (stat[len-1]==':') {
		packed_params.rpc   = rpc;
		packed_params.ctx   = ctx;
		packed_params.clear = clear;
		stat[len-1] = '\0';
		counter_iterate_grp_vars(stat, rpc_reset_or_clear_grp_vars_cbk, &packed_params);
		stat[len-1] = ':';
	}
	else {
		s_statistic.s = stat;
		s_statistic.len = strlen(stat);
		s_stat = get_stat(&s_statistic);
		if (s_stat) {
			if (clear) {
				old_val=get_stat_val(s_stat);
				reset_stat(s_stat);
				new_val=get_stat_val(s_stat);
				if (old_val==new_val) {
					rpc->rpl_printf(ctx, "%s:%s = %lu",
						ZSW(get_stat_module(s_stat)), ZSW(get_stat_name(s_stat)),
						new_val);
				}
				else {
					rpc->rpl_printf(ctx, "%s:%s = %lu (%lu)",
						ZSW(get_stat_module(s_stat)), ZSW(get_stat_name(s_stat)),
						new_val, old_val);
				}
			}
			else {
				reset_stat(s_stat);
			}
		}
	}
}

/**
 * RPC statistics reseter/getter framework.
 */
static void stats_reset_or_clear_statistics(rpc_t* rpc, void* ctx, int clear)
{
	char* stat;

	if (stats_support()==0) {
		rpc->fault(ctx, 400, "stats support not enabled");
		return;
	}
	if (rpc->scan(ctx, "s", &stat) < 1) {
		rpc->fault(ctx, 400, "Please provide which stats to retrieve");
		return;
	}
	stats_reset_or_clear_all(rpc, ctx, stat, clear);
	while((rpc->scan(ctx, "*s", &stat)>0)) {
		stats_reset_or_clear_all(rpc, ctx, stat, clear);
	}
	return;
}


/**
 * RPC statistics reseter.
 */
static void rpc_stats_reset_statistics(rpc_t* rpc, void* ctx)
{
	stats_reset_or_clear_statistics(rpc, ctx, 0);
	return;
}


/**
 * RPC statistics clearer.
 */
static void rpc_stats_clear_statistics(rpc_t* rpc, void* ctx)
{
	stats_reset_or_clear_statistics(rpc, ctx, 1);
	return;
}


/**
 * RPC statistics getter doc.
 */
static const char* rpc_stats_get_statistics_doc[2] =
	{"get core and modules stats",   0};
/**
 * RPC statistics reseter doc.
 */
static const char* rpc_stats_reset_statistics_doc[2] =
	{"reset core and modules stats (silent operation)", 0};
/**
 * RPC statistics clearer doc.
 */
static const char* rpc_stats_clear_statistics_doc[2] =
	{"clear core and modules stats (verbose operation)", 0};

/**
 * Stats RPC  commands.
 */
rpc_export_t kex_stats_rpc[] =
{
	{"stats.get_statistics",   rpc_stats_get_statistics,
							rpc_stats_get_statistics_doc,   RET_ARRAY},
	{"stats.reset_statistics", rpc_stats_reset_statistics,
							rpc_stats_reset_statistics_doc, 0},
	{"stats.clear_statistics", rpc_stats_clear_statistics,
							rpc_stats_clear_statistics_doc, 0},
	{0, 0, 0, 0}
};

/**
 * Stats RPC initializer.
 */
int stats_proc_stats_init_rpc(void)
{
	if (rpc_register_array(kex_stats_rpc)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/***************************** MI STUFF ********************************/

inline static int mi_add_stat(struct mi_node *rpl, stat_var *stat)
{
	struct mi_node *node;

	if (stats_support()==0) return -1;

	node = addf_mi_node_child(rpl, 0, 0, 0, "%s:%s = %lu",
		ZSW(get_stat_module(stat)),
		ZSW(get_stat_name(stat)),
		get_stat_val(stat) );

	if (node==0)
		return -1;
	return 0;
}



/* callback for counter_iterate_grp_vars. */
static void mi_add_grp_vars_cbk(void* r, str* g, str* n, counter_handle_t h)
{
	struct mi_node *rpl;
	
	rpl = r;
	addf_mi_node_child(rpl, 0, 0, 0, "%.*s:%.*s = %lu",
							g->len, g->s, n->len, n->s, counter_get_val(h));
}


/* callback for counter_iterate_grp_names */
static void mi_add_all_grps_cbk(void* p, str* g)
{
	counter_iterate_grp_vars(g->s, mi_add_grp_vars_cbk, p);
}

static struct mi_root *mi_get_stats(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *arg;
	stat_var       *stat;
	str val;


	if(stats_support()==0)
		return init_mi_tree( 404, "Statistics Not Found", 20);

	if (cmd->node.kids==NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for( arg=cmd->node.kids ; arg ; arg=arg->next) {
		if (arg->value.len==0)
			continue;

		val = arg->value;

		if ( val.len==3 && memcmp(val.s,"all",3)==0) {
			/* add all statistic variables */
			/* use direct counters access for that */
			counter_iterate_grp_names(mi_add_all_grps_cbk, rpl);
		} else if ( val.len>1 && val.s[val.len-1]==':') {
			/* add module statistics */
			val.len--;
			val.s[val.len]=0; /* zero term. */
			/* use direct counters access for that */
			counter_iterate_grp_vars(val.s, mi_add_grp_vars_cbk, rpl);
			val.s[val.len]=':' /* restore */;
		} else {
			/* add only one statistic */
			stat = get_stat( &val );
			if (stat==0)
				continue;
			if (mi_add_stat(rpl,stat)!=0)
				goto error;
		}
	}

	if (rpl->kids==0) {
		free_mi_tree(rpl_tree);
		return init_mi_tree( 404, "Statistics Not Found", 20);
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return 0;
}



static struct mi_root *mi_reset_stats(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *arg;
	stat_var       *stat;
	int found;

	if (cmd->node.kids==NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	found = 0;

	for( arg=cmd->node.kids ; arg ; arg=arg->next) {
		if (arg->value.len==0)
			continue;

		stat = get_stat( &arg->value );
		if (stat==0)
			continue;

		reset_stat( stat );
		found = 1;
	}

	if (!found) {
		free_mi_tree(rpl_tree);
		return init_mi_tree( 404, "Statistics Not Found", 20);
	}

	return rpl_tree;
}


inline static int mi_reset_and_add_stat(struct mi_node *rpl, stat_var *stat)
{
	struct mi_node *node;
	long old_val, new_val;

	if (stats_support()==0) return -1;

	old_val=get_stat_val(stat);
	reset_stat(stat);
	new_val=get_stat_val(stat);

	if (old_val==new_val)
	{
		node = addf_mi_node_child(rpl, 0, 0, 0, "%s:%s = %lu",
				ZSW(get_stat_module(stat)),
				ZSW(get_stat_name(stat)),
				new_val);
	} else {
		node = addf_mi_node_child(rpl, 0, 0, 0, "%s:%s = %lu (%lu)",
				ZSW(get_stat_module(stat)),
				ZSW(get_stat_name(stat)),
				new_val, old_val );
	}

	if (node==0)
		return -1;
	return 0;
}


/* callback for counter_iterate_grp_vars to reset counters */
static void mi_add_grp_vars_cbk2(void* r, str* g, str* n, counter_handle_t h)
{
	struct mi_node *rpl;
	counter_val_t old_val, new_val;

	rpl = r;
	old_val = counter_get_val(h);
	counter_reset(h);
	new_val = counter_get_val(h);

	if (old_val==new_val)
	{
		addf_mi_node_child(rpl, 0, 0, 0, "%.*s:%.*s = %lu",
					g->len, g->s, n->len, n->s, new_val);
	} else {
		addf_mi_node_child(rpl, 0, 0, 0, "%.*s:%.*s = %lu (%lu)",
					g->len, g->s, n->len, n->s, new_val, old_val);
	}
}


/* callback for counter_iterate_grp_names to reset counters */
static void mi_add_all_grps_cbk2(void* p, str* g)
{
	counter_iterate_grp_vars(g->s, mi_add_grp_vars_cbk2, p);
}


static struct mi_root *mi_clear_stats(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *arg;
	stat_var       *stat;
	str val;

	if(stats_support()==0)
		return init_mi_tree( 404, "Statistics Not Found", 20);

	if (cmd->node.kids==NULL)
	return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for( arg=cmd->node.kids ; arg ; arg=arg->next)
	{
		if (arg->value.len==0)
			continue;

		val = arg->value;

		if ( val.len==3 && memcmp(val.s,"all",3)==0) {
			/* add all statistic variables */
			/* use direct counters access for that */
			counter_iterate_grp_names(mi_add_all_grps_cbk2, rpl);
		} else if ( val.len>1 && val.s[val.len-1]==':') {
			/* add module statistics */
			val.len--;
			val.s[val.len]=0; /* zero term. */
			/* use direct counters access for that */
			counter_iterate_grp_vars(val.s, mi_add_grp_vars_cbk2, rpl);
			val.s[val.len]=':' /* restore */;
		} else {
			/* reset & return only one statistic */
			stat = get_stat( &val );

			if (stat==0)
				continue;
			if (mi_reset_and_add_stat(rpl,stat)!=0)
				goto error;
		}
	}

	if (rpl->kids==0) {
		free_mi_tree(rpl_tree);
		return init_mi_tree( 404, "Statistics Not Found", 20);
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return 0;
}

/*** shm stats ***/

static struct mem_info _stats_shm_mi;
static ticks_t _stats_shm_tm = 0;
void stats_shm_update(void)
{
	ticks_t t;
	t = get_ticks();
	if(t!=_stats_shm_tm) {
		shm_info(&_stats_shm_mi);
		_stats_shm_tm = t;
	}
}
unsigned long shm_stats_get_size(void)
{
	stats_shm_update();
	return _stats_shm_mi.total_size;
}

unsigned long shm_stats_get_used(void)
{
	stats_shm_update();
	return _stats_shm_mi.used;
}

unsigned long shm_stats_get_rused(void)
{
	stats_shm_update();
	return _stats_shm_mi.real_used;
}

unsigned long shm_stats_get_mused(void)
{
	stats_shm_update();
	return _stats_shm_mi.max_used;
}

unsigned long shm_stats_get_free(void)
{
	stats_shm_update();
	return _stats_shm_mi.free;
}

unsigned long shm_stats_get_frags(void)
{
	stats_shm_update();
	return _stats_shm_mi.total_frags;
}

#endif
