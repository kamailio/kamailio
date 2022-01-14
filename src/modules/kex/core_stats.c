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
 */

/*!
 * \file
 * \brief KEX :: Kamailio Core statistics
 * \ingroup kex
 */


#include <string.h>

#include "../../core/counters.h"
#include "../../core/events.h"
#include "../../core/dprint.h"
#include "../../core/timer.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/script_cb.h"
#include "../../core/mem/meminfo.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"


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
/*! received requests by method  */
stat_var* rcv_reqs_invite;
stat_var* rcv_reqs_cancel;
stat_var* rcv_reqs_ack;
stat_var* rcv_reqs_bye;
stat_var* rcv_reqs_info;
stat_var* rcv_reqs_register;
stat_var* rcv_reqs_subscribe;
stat_var* rcv_reqs_notify;
stat_var* rcv_reqs_message;
stat_var* rcv_reqs_options;
stat_var* rcv_reqs_prack;
stat_var* rcv_reqs_update;
stat_var* rcv_reqs_refer;
stat_var* rcv_reqs_publish;
/*! extended received replies */
stat_var* rcv_rpls_1xx;
stat_var* rcv_rpls_18x;
stat_var* rcv_rpls_2xx;
stat_var* rcv_rpls_3xx;
stat_var* rcv_rpls_4xx;
stat_var* rcv_rpls_401;
stat_var* rcv_rpls_404;
stat_var* rcv_rpls_407;
stat_var* rcv_rpls_480;
stat_var* rcv_rpls_486;
stat_var* rcv_rpls_5xx;
stat_var* rcv_rpls_6xx;

#define VAR_NAME(method) rcv_rpls_ ## method

#define STAT_NAME(method, group) "rcv_replies_" #group "xx_" #method

#define DECLARE_STAT_VARS(method) stat_var* VAR_NAME(method)[6];

#define DECLARE_STATS(method) \
      { STAT_NAME(method, 1), 0, &VAR_NAME(method)[0] }, \
      { STAT_NAME(method, 2), 0, &VAR_NAME(method)[1] }, \
      { STAT_NAME(method, 3), 0, &VAR_NAME(method)[2] }, \
      { STAT_NAME(method, 4), 0, &VAR_NAME(method)[3] }, \
      { STAT_NAME(method, 5), 0, &VAR_NAME(method)[4] }, \
      { STAT_NAME(method, 6), 0, &VAR_NAME(method)[5] }

DECLARE_STAT_VARS(invite);
DECLARE_STAT_VARS(cancel);
DECLARE_STAT_VARS(bye);
DECLARE_STAT_VARS(reg);
DECLARE_STAT_VARS(message);
DECLARE_STAT_VARS(prack);
DECLARE_STAT_VARS(update);
DECLARE_STAT_VARS(refer);

/*! exported core statistics */
stat_export_t core_stats[] = {
	{"rcv_requests" ,         0,  &rcv_reqs              },
	{"rcv_requests_invite" ,      0,  &rcv_reqs_invite       },
	{"rcv_requests_cancel" ,      0,  &rcv_reqs_cancel       },
	{"rcv_requests_ack" ,         0,  &rcv_reqs_ack          },
	{"rcv_requests_bye" ,         0,  &rcv_reqs_bye          },
	{"rcv_requests_info" ,        0,  &rcv_reqs_info         },
	{"rcv_requests_register" ,    0,  &rcv_reqs_register     },
	{"rcv_requests_subscribe" ,   0,  &rcv_reqs_subscribe    },
	{"rcv_requests_notify" ,      0,  &rcv_reqs_notify       },
	{"rcv_requests_message" ,     0,  &rcv_reqs_message      },
	{"rcv_requests_options" ,     0,  &rcv_reqs_options      },
	{"rcv_requests_prack" ,       0,  &rcv_reqs_prack        },
	{"rcv_requests_update" ,      0,  &rcv_reqs_update       },
	{"rcv_requests_refer" ,       0,  &rcv_reqs_refer        },
	{"rcv_requests_publish" ,     0,  &rcv_reqs_publish      },
	{"rcv_replies" ,          0,  &rcv_rpls              },
	{"rcv_replies_1xx" ,      0,  &rcv_rpls_1xx          },
	{"rcv_replies_18x" ,      0,  &rcv_rpls_18x          },
	{"rcv_replies_2xx" ,      0,  &rcv_rpls_2xx          },
	{"rcv_replies_3xx" ,      0,  &rcv_rpls_3xx          },
	{"rcv_replies_4xx" ,      0,  &rcv_rpls_4xx          },
	{"rcv_replies_401" ,      0,  &rcv_rpls_401          },
	{"rcv_replies_404" ,      0,  &rcv_rpls_404          },
	{"rcv_replies_407" ,      0,  &rcv_rpls_407          },
	{"rcv_replies_480" ,      0,  &rcv_rpls_480          },
	{"rcv_replies_486" ,      0,  &rcv_rpls_486          },
	{"rcv_replies_5xx" ,      0,  &rcv_rpls_5xx          },
	{"rcv_replies_6xx" ,      0,  &rcv_rpls_6xx          },
	{"fwd_requests" ,         0,  &fwd_reqs              },
	{"fwd_replies" ,          0,  &fwd_rpls              },
	{"drop_requests" ,        0,  &drp_reqs              },
	{"drop_replies" ,         0,  &drp_rpls              },
	{"err_requests" ,         0,  &err_reqs              },
	{"err_replies" ,          0,  &err_rpls              },
	{"bad_URIs_rcvd",         0,  &bad_URIs              },
	{"unsupported_methods",   0,  &unsupported_methods   },
	{"bad_msg_hdr",           0,  &bad_msg_hdr           },
      DECLARE_STATS(invite),
      DECLARE_STATS(cancel),
      DECLARE_STATS(bye),
      DECLARE_STATS(reg),
      DECLARE_STATS(message),
      DECLARE_STATS(prack),
      DECLARE_STATS(update),
      DECLARE_STATS(refer),
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

int stats_proc_stats_init_rpc(void);


static int km_cb_req_stats(struct sip_msg *msg,
		unsigned int flags, void *param)
{
	update_stat(rcv_reqs, 1);
	if(!IS_SIP(msg))
		return 1;
	switch(msg->first_line.u.request.method_value) {
		case METHOD_INVITE:
			update_stat(rcv_reqs_invite, 1);
		break;
		case METHOD_CANCEL:
			update_stat(rcv_reqs_cancel, 1);
		break;
		case METHOD_ACK:
			update_stat(rcv_reqs_ack, 1);
		break;
		case METHOD_BYE:
			update_stat(rcv_reqs_bye, 1);
		break;
		case METHOD_INFO:
			update_stat(rcv_reqs_info, 1);
		break;
		case METHOD_REGISTER:
			update_stat(rcv_reqs_register, 1);
		break;
		case METHOD_SUBSCRIBE:
			update_stat(rcv_reqs_subscribe, 1);
		break;
		case METHOD_NOTIFY:
			update_stat(rcv_reqs_notify, 1);
		break;
		case METHOD_MESSAGE:
			update_stat(rcv_reqs_message, 1);
		break;
		case METHOD_OPTIONS:
			update_stat(rcv_reqs_options, 1);
		break;
		case METHOD_PRACK:
			update_stat(rcv_reqs_prack, 1);
		break;
		case METHOD_UPDATE:
			update_stat(rcv_reqs_update, 1);
		break;
		case METHOD_REFER:
			update_stat(rcv_reqs_refer, 1);
		break;
		case METHOD_PUBLISH:
			update_stat(rcv_reqs_publish, 1);
		break;
		case METHOD_OTHER:
			update_stat(unsupported_methods, 1);
		break;
	}
	return 1;
}

static int km_cb_rpl_stats_by_method(struct sip_msg *msg,
		unsigned int flags, void *param)
{
	int method = 0;
	int group = 0;

	if(msg==NULL) {
		return -1;
	}
	if (!msg->cseq && (parse_headers(msg, HDR_CSEQ_F, 0) < 0 || !msg->cseq)) {
		return -1;
	}
	method = get_cseq(msg)->method_id;
	group = msg->first_line.u.reply.statuscode / 100 - 1;

      if (group >= 0 && group <= 5) {
            switch(method) {
                  case METHOD_INVITE: update_stat( VAR_NAME(invite)[group], 1); break;
                  case METHOD_CANCEL: update_stat( VAR_NAME(cancel)[group], 1); break;
                  case METHOD_BYE: update_stat( VAR_NAME(bye)[group], 1); break;
                  case METHOD_REGISTER: update_stat( VAR_NAME(reg)[group], 1); break;
                  case METHOD_MESSAGE: update_stat( VAR_NAME(message)[group], 1); break;
                  case METHOD_PRACK: update_stat( VAR_NAME(prack)[group], 1); break;
                  case METHOD_UPDATE: update_stat( VAR_NAME(update)[group], 1); break;
                  case METHOD_REFER: update_stat( VAR_NAME(refer)[group], 1); break;
             }
      }

      return 1;
}

static int km_cb_rpl_stats(struct sip_msg *msg,
		unsigned int flags, void *param)
{
	update_stat(rcv_rpls, 1);
	if(msg->first_line.u.reply.statuscode > 99 &&
		msg->first_line.u.reply.statuscode <200)
	{
		update_stat(rcv_rpls_1xx, 1);
		if(msg->first_line.u.reply.statuscode > 179 &&
			msg->first_line.u.reply.statuscode <190) {
				update_stat(rcv_rpls_18x, 1);
		}
	}
	else if(msg->first_line.u.reply.statuscode > 199 &&
		msg->first_line.u.reply.statuscode <300)
	{
		update_stat(rcv_rpls_2xx, 1);
	}
	else if(msg->first_line.u.reply.statuscode > 299 &&
		msg->first_line.u.reply.statuscode <400)
	{
		update_stat(rcv_rpls_3xx, 1);
	}
	else if(msg->first_line.u.reply.statuscode > 399 &&
		msg->first_line.u.reply.statuscode <500)
	{
		update_stat(rcv_rpls_4xx, 1);
		switch(msg->first_line.u.reply.statuscode) {
			case 401:
				update_stat(rcv_rpls_401, 1);
			break;
			case 404:
				update_stat(rcv_rpls_404, 1);
			break;
			case 407:
				update_stat(rcv_rpls_407, 1);
			break;
			case 480:
				update_stat(rcv_rpls_480, 1);
			break;
			case 486:
				update_stat(rcv_rpls_486, 1);
			break;
		}
	}
	else if(msg->first_line.u.reply.statuscode > 499 &&
		msg->first_line.u.reply.statuscode <600)
	{
		update_stat(rcv_rpls_5xx, 1);
	}
	else if(msg->first_line.u.reply.statuscode > 599 &&
		msg->first_line.u.reply.statuscode <700)
	{
		update_stat(rcv_rpls_6xx, 1);
	}
	return 1;
}


static int sts_update_core_stats(sr_event_param_t *evp)
{
	int type;

	type = (int)(long)evp;
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
	if (register_script_cb(km_cb_rpl_stats_by_method, PRE_SCRIPT_CB|ONREPLY_CB, 0)<0 ) {
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
	void* hst;
	int numeric;
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


static void rpc_fetch_add_stat(rpc_t* rpc, void* ctx, void* hst, char* g, char* n, unsigned long val, int numeric) {
	char nbuf[128];
	int res;

	snprintf(nbuf, 127, "%s.%s", g, n);
	if (numeric) {
		res = rpc->struct_add(hst, "j", nbuf, val);
	} else {
		res = rpc->struct_printf(hst, nbuf, "%lu", val);
	}
	if (res<0)
	{
		rpc->fault(ctx, 500, "Internal error");
		return;
	}
}

/**
 * Statistic getter RPC callback.
 */
static void rpc_fetch_grp_vars_cbk(void* p, str* g, str* n, counter_handle_t h)
{
	struct rpc_list_params *packed_params = p;

	rpc_fetch_add_stat(packed_params->rpc, packed_params->ctx, packed_params->hst,
					   g->s, n->s, counter_get_val(h), packed_params->numeric);
}

/**
 * Group statistic getter RPC callback.
 */
static void rpc_fetch_all_grps_cbk(void* p, str* g)
{
	counter_iterate_grp_vars(g->s, rpc_fetch_grp_vars_cbk, p);
}

/**
 * All statistic getter RPC callback.
 */
static void stats_fetch_all(rpc_t* rpc, void* ctx, void* th, char* stat, int numeric)
{
	int len = strlen(stat);
	struct rpc_list_params packed_params;
	str s_statistic;
	stat_var *s_stat;
	char *m;
	char *n;
	int i;

	if (len==3 && strcmp("all", stat)==0) {
		packed_params.rpc = rpc;
		packed_params.ctx = ctx;
		packed_params.hst = th;
		packed_params.numeric = numeric;
		counter_iterate_grp_names(rpc_fetch_all_grps_cbk, &packed_params);
	}
	else if (stat[len-1]==':') {
		packed_params.rpc = rpc;
		packed_params.ctx = ctx;
		packed_params.hst = th;
		packed_params.numeric = numeric;
		stat[len-1] = '\0';
		counter_iterate_grp_vars(stat, rpc_fetch_grp_vars_cbk, &packed_params);
		stat[len-1] = ':';
	}
	else {
		s_statistic.s = stat;
		s_statistic.len = strlen(stat);
		s_stat = get_stat(&s_statistic);
		if (s_stat) {
			rpc_fetch_add_stat(rpc, ctx, th,
					ZSW(get_stat_module(s_stat)), ZSW(get_stat_name(s_stat)), get_stat_val(s_stat), numeric);
		} else {
			n = strchr(stat, '.');
			if(n==NULL) {
				n =strchr(stat, ':');
			}
			if(n==NULL) {
				return;
			}
			n++;
			s_statistic.s = n;
			s_statistic.len = strlen(n);
			s_stat = get_stat(&s_statistic);
			if (s_stat) {
				m = get_stat_module(s_stat);
				if(m==NULL) {
					return;
				}
				for(i=0;  m[i]!=0 && stat[i]!=0; i++) {
					if(stat[i]!=m[i]) {
						/* module name mismatch */
						return;
					}
				}
				if(m[i]!=0 || (stat[i]!='.' && stat[i]!=':')) {
					/* module name mismatch */
					return;
				}

				rpc_fetch_add_stat(rpc, ctx, th,
						ZSW(m), ZSW(get_stat_name(s_stat)), get_stat_val(s_stat), numeric);
			}
		}
	}
}

/**
 * RPC statistics getter
 */
static void rpc_stats_fetch_statistics(rpc_t* rpc, void* ctx, int numeric)
{
	char* stat;
	void *th;

	if (stats_support()==0) {
		rpc->fault(ctx, 400, "stats support not enabled");
		return;
	}
	if (rpc->scan(ctx, "s", &stat) < 1) {
		rpc->fault(ctx, 400, "Please provide which stats to retrieve");
		return;
	}
	if (rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error creating root struct");
		return;
	}
	stats_fetch_all(rpc, ctx, th, stat, numeric);
	while((rpc->scan(ctx, "*s", &stat)>0)) {
		stats_fetch_all(rpc, ctx, th, stat, numeric);
	}
	return;
}


/**
 * RPC statistics getter with string values.
 */
static void rpc_stats_fetchs_statistics(rpc_t* rpc, void* ctx)
{
	rpc_stats_fetch_statistics(rpc, ctx, 0);
	return;
}

/**
 * RPC statistics getter with number values.
 */
static void rpc_stats_fetchn_statistics(rpc_t* rpc, void* ctx)
{
	rpc_stats_fetch_statistics(rpc, ctx, 1);
	return;
}

/**
 * Satistic reset/clear-er RPC callback..
 */
static void rpc_reset_or_clear_grp_vars_cbk(void* p, str* g, str* n,
		counter_handle_t h)
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
		rpc->fault(ctx, 400, "Please provide stats name");
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
 * RPC statistics getter doc.
 */
static const char* rpc_stats_fetchs_statistics_doc[2] =
	{"fetch core and modules stats as string values",   0};

/**
 * RPC statistics getter doc.
 */
static const char* rpc_stats_fetchn_statistics_doc[2] =
	{"fetch core and modules stats as number values",   0};


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
	{"stats.fetch",			rpc_stats_fetchs_statistics,
							rpc_stats_fetchs_statistics_doc, 0},
	{"stats.fetchn",		rpc_stats_fetchn_statistics,
							rpc_stats_fetchn_statistics_doc, 0},
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

/*** shm stats ***/

static struct mem_info _stats_shm_rpc;
static ticks_t _stats_shm_tm = 0;
void stats_shm_update(void)
{
	ticks_t t;
	t = get_ticks();
	if(t!=_stats_shm_tm) {
		shm_info(&_stats_shm_rpc);
		_stats_shm_tm = t;
	}
}
unsigned long shm_stats_get_size(void)
{
	stats_shm_update();
	return _stats_shm_rpc.total_size;
}

unsigned long shm_stats_get_used(void)
{
	stats_shm_update();
	return _stats_shm_rpc.used;
}

unsigned long shm_stats_get_rused(void)
{
	stats_shm_update();
	return _stats_shm_rpc.real_used;
}

unsigned long shm_stats_get_mused(void)
{
	stats_shm_update();
	return _stats_shm_rpc.max_used;
}

unsigned long shm_stats_get_free(void)
{
	stats_shm_update();
	return _stats_shm_rpc.free;
}

unsigned long shm_stats_get_frags(void)
{
	stats_shm_update();
	return _stats_shm_rpc.total_frags;
}

#endif
