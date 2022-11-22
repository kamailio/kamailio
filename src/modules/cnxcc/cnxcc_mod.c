/*
 * Copyright (C) 2012 Carlos Ruiz Díaz (caruizdiaz.com),
 *                    ConexionGroup (www.conexiongroup.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"

#include "../../core/lock_ops.h"
#include "../../core/timer_proc.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_cseq.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/contact/contact.h"

#include "../tm/tm_load.h"
#include "../dialog/dlg_load.h"
#include "../dialog/dlg_hash.h"

#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "cnxcc_mod.h"
#include "cnxcc.h"
#include "cnxcc_sip_msg_faker.h"
#include "cnxcc_check.h"
#include "cnxcc_rpc.h"
#include "cnxcc_select.h"
#include "cnxcc_redis.h"

MODULE_VERSION

#define HT_SIZE 229
#define MODULE_NAME "cnxcc"
#define NUMBER_OF_TIMERS 2

#define TRUE 1
#define FALSE !TRUE

data_t _data;
struct dlg_binds _dlgbinds;

static int cnxcc_set_max_credit_fixup(void **param, int param_no);

/*
 *  module core functions
 */
static int __mod_init(void);
static int __child_init(int);
static int __init_hashtable(struct str_hash_table *ht);

/*
 * Memory management functions
 */
static int __shm_str_hash_alloc(struct str_hash_table *ht, int size);
static void __free_credit_data_hash_entry(struct str_hash_entry *e);
static void __free_credit_data(credit_data_t *credit_data, hash_tables_t *hts,
		struct str_hash_entry *cd_entry);

/*
 * PV management functions
 */
static int __pv_parse_calls_param(pv_spec_p sp, str *in);
static int __pv_get_calls(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

/*
 * Billing management functions
 */
static int __set_max_credit(sip_msg_t *msg, char *pclient, char *pcredit,
		char *pconnect, char *pcps, char *pinitp, char *pfinishp);
static int __set_max_time(sip_msg_t *msg, char *pclient, char *pmaxsecs);
static int __update_max_time(sip_msg_t *msg, char *pclient, char *psecs);
static int __set_max_channels(sip_msg_t *msg, char *pclient, char *pmaxchan);
static int __get_channel_count(sip_msg_t *msg, char *pclient, char *pcount);
static int __terminate_all(sip_msg_t *msg, char *pclient, char *p2);

static void __start_billing(
		str *callid, str *from_uri, str *to_uri, str tags[2]);
static void __setup_billing(
		str *callid, unsigned int h_entry, unsigned int h_id);
static void __stop_billing(str *callid);
static int __add_call_by_cid(str *cid, call_t *call, credit_type_t type);
static call_t *__alloc_new_call_by_time(
		credit_data_t *credit_data, struct sip_msg *msg, int max_secs);
static call_t *__alloc_new_call_by_money(credit_data_t *credit_data,
		struct sip_msg *msg, double credit, double connect_cost,
		double cost_per_second, int initial_pulse, int final_pulse);
static void __notify_call_termination(sip_msg_t *msg);
static void __free_call(call_t *call);
static void __delete_call(call_t *call, credit_data_t *credit_data);
static int __has_to_tag(struct sip_msg *msg);
static credit_data_t *__alloc_new_credit_data(
		str *client_id, credit_type_t type);
static credit_data_t *__get_or_create_credit_data_entry(
		str *client_id, credit_type_t type);

/*
 * control interface
 */
void rpc_credit_control_stats(rpc_t *rpc, void *ctx);

/*
 * Dialog management callback functions
 */
static void __dialog_terminated_callback(
		struct dlg_cell *cell, int type, struct dlg_cb_params *params);
static void __dialog_confirmed_callback(
		struct dlg_cell *cell, int type, struct dlg_cb_params *params);
static void __dialog_created_callback(
		struct dlg_cell *cell, int type, struct dlg_cb_params *params);

/* clang-format off */
static pv_export_t mod_pvs[] = {
	{ {"cnxcc", sizeof("cnxcc")-1 }, PVT_OTHER, __pv_get_calls, 0,
			__pv_parse_calls_param, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[] = {
	{"cnxcc_set_max_credit", (cmd_function) __set_max_credit, 6,
		cnxcc_set_max_credit_fixup, NULL, ANY_ROUTE},
	{"cnxcc_set_max_time", (cmd_function) __set_max_time, 2,
		fixup_spve_igp, fixup_free_spve_igp, ANY_ROUTE},
	{"cnxcc_update_max_time", (cmd_function) __update_max_time, 2,
		fixup_spve_igp, fixup_free_spve_igp, ANY_ROUTE},
	{"cnxcc_set_max_channels", (cmd_function) __set_max_channels, 2,
		fixup_spve_igp, fixup_free_spve_igp, ANY_ROUTE},
	{"cnxcc_get_channel_count", (cmd_function) __get_channel_count, 2,
		fixup_spve_pvar, fixup_free_spve_pvar, ANY_ROUTE},
	{"cnxcc_terminate_all", (cmd_function) __terminate_all, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[] = {
	{"dlg_flag", INT_PARAM,	&_data.ctrl_flag },
	{"credit_check_period", INT_PARAM,	&_data.check_period },
	{"redis", PARAM_STR, &_data.redis_cnn_str },
	{ 0, 0, 0 }
};
/* clang-format on */

static const char *rpc_active_clients_doc[2] = {
		"List of clients with active calls", NULL};

static const char *rpc_check_client_stats_doc[2] = {
		"Check specific client calls", NULL};

static const char *rpc_kill_call_doc[2] = {"Kill call using its call ID", NULL};

static const char *rpc_credit_control_stats_doc[2] = {
		"List credit control stats", NULL};

/* clang-format off */
rpc_export_t cnxcc_rpc[] = {
	{ "cnxcc.active_clients", rpc_active_clients, rpc_active_clients_doc,	0},
	{ "cnxcc.check_client", rpc_check_client_stats, rpc_check_client_stats_doc,	0},
	{ "cnxcc.kill_call", rpc_kill_call, rpc_kill_call_doc, 0},
	{ "cnxcc.stats", rpc_credit_control_stats, rpc_credit_control_stats_doc, 0},
	{ NULL, NULL, NULL, 0}
};

/* selects declaration */
select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("cnxcc"), sel_root,
		SEL_PARAM_EXPECTED},
	{ sel_root, SEL_PARAM_STR, STR_STATIC_INIT("channels"), sel_channels,
		SEL_PARAM_EXPECTED|CONSUME_NEXT_STR|FIXUP_CALL},
	{ sel_channels, SEL_PARAM_STR, STR_STATIC_INIT("count"), sel_channels_count,
		0},
	{ NULL, SEL_PARAM_STR, STR_NULL, NULL, 0}
};

/** module exports */
struct module_exports exports = {
	MODULE_NAME,     /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	__mod_init,      /* module init function */
	__child_init,    /* per-child init function */
	0                /* module destroy function */
};
/* clang-format on */

static int cnxcc_set_max_credit_fixup(void **param, int param_no)
{
	switch(param_no) {
		case 1:
		case 2:
		case 3:
			return fixup_spve_all(param, param_no);
		case 4:
			return fixup_spve_all(param, param_no);
		case 5:
		case 6:
			return fixup_igp_all(param, param_no);
		default:
			LM_ERR("unexpected parameter number: %d\n", param_no);
			return E_CFG;
	}
}

static int __mod_init(void)
{
	int len;
	char *chr;

	LM_INFO("Loading " MODULE_NAME " module\n");

	_data.cs_route_number = route_get(&event_rt, "cnxcc:call-shutdown");

	if(_data.cs_route_number < 0)
		LM_INFO("No cnxcc:call-shutdown event route found\n");

	if(_data.cs_route_number > 0
			&& event_rt.rlist[_data.cs_route_number] == NULL) {
		LM_INFO("cnxcc:call-shutdown route is empty\n");
		_data.cs_route_number = -1;
	}

	if(_data.check_period <= 0) {
		LM_INFO("credit_check_period cannot be less than 1 second\n");
		return -1;
	}

	_data.time.credit_data_by_client =
			shm_malloc(sizeof(struct str_hash_table));
	_data.time.call_data_by_cid = shm_malloc(sizeof(struct str_hash_table));
	_data.money.credit_data_by_client =
			shm_malloc(sizeof(struct str_hash_table));
	_data.money.call_data_by_cid = shm_malloc(sizeof(struct str_hash_table));
	_data.channel.credit_data_by_client =
			shm_malloc(sizeof(struct str_hash_table));
	_data.channel.call_data_by_cid = shm_malloc(sizeof(struct str_hash_table));

	memset(_data.time.credit_data_by_client, 0, sizeof(struct str_hash_table));
	memset(_data.time.call_data_by_cid, 0, sizeof(struct str_hash_table));
	memset(_data.money.credit_data_by_client, 0, sizeof(struct str_hash_table));
	memset(_data.money.call_data_by_cid, 0, sizeof(struct str_hash_table));
	memset(_data.channel.credit_data_by_client, 0,
			sizeof(struct str_hash_table));
	memset(_data.channel.call_data_by_cid, 0, sizeof(struct str_hash_table));

	_data.stats = (stats_t *)shm_malloc(sizeof(stats_t));
	if(!_data.stats) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(_data.stats, 0, sizeof(stats_t));

	if(__init_hashtable(_data.time.credit_data_by_client) != 0)
		return -1;

	if(__init_hashtable(_data.time.call_data_by_cid) != 0)
		return -1;

	if(__init_hashtable(_data.money.credit_data_by_client) != 0)
		return -1;

	if(__init_hashtable(_data.money.call_data_by_cid) != 0)
		return -1;

	if(__init_hashtable(_data.channel.credit_data_by_client) != 0)
		return -1;

	if(__init_hashtable(_data.channel.call_data_by_cid) != 0)
		return -1;

	cnxcc_lock_init(_data.lock);
	cnxcc_lock_init(_data.time.lock);
	cnxcc_lock_init(_data.money.lock);
	cnxcc_lock_init(_data.channel.lock);

	/*
	 * One for time based monitoring
	 * One for money based monitoring
	 */
	register_dummy_timers(NUMBER_OF_TIMERS);

	if(rpc_register_array(cnxcc_rpc) != 0) {
		LM_ERR("Failed registering RPC commands\n");
		return -1;
	}

	if(load_dlg_api(&_dlgbinds) != 0) {
		LM_ERR("Error loading dialog API\n");
		return -1;
	}

	_dlgbinds.register_dlgcb(
			NULL, DLGCB_CREATED, __dialog_created_callback, NULL, NULL);

	register_select_table(sel_declaration);

	// redis configuration setup
	if(_data.redis_cnn_str.len <= 0)
		return 0;

	// replace ";" for " ", so we can use a simpler pattern in sscanf()
	for(chr = _data.redis_cnn_str.s; *chr; chr++)
		if(*chr == ';')
			*chr = ' ';

	memset(_data.redis_cnn_info.host, 0, sizeof(_data.redis_cnn_info.host));
	sscanf(_data.redis_cnn_str.s, "addr=%s port=%d db=%d",
			_data.redis_cnn_info.host, &_data.redis_cnn_info.port,
			&_data.redis_cnn_info.db);

	len = strlen(_data.redis_cnn_info.host);
	//
	// Redis modparam validations
	//
	if(len == 0) {
		LM_ERR("invalid host address [%s]", _data.redis_cnn_info.host);
		return -1;
	}

	if(_data.redis_cnn_info.port <= 0) {
		LM_ERR("invalid port number [%d]", _data.redis_cnn_info.port);
		return -1;
	}

	if(_data.redis_cnn_info.db < 0) {
		LM_ERR("invalid db number [%d]", _data.redis_cnn_info.db);
		return -1;
	}

	LM_INFO("Redis connection info: ip=[%s], port=[%d], database=[%d]",
			_data.redis_cnn_info.host, _data.redis_cnn_info.port,
			_data.redis_cnn_info.db);

	register_procs(3 /* 2 timers + 1 redis async receiver */);
	return 0;
}

static int __child_init(int rank)
{
	int pid = 0;

	if(rank != PROC_INIT && rank != PROC_MAIN && rank != PROC_TCP_MAIN) {
		if(_data.redis_cnn_str.len <= 0)
			return 0;

		_data.redis = redis_connect(_data.redis_cnn_info.host,
				_data.redis_cnn_info.port, _data.redis_cnn_info.db);
		return (!_data.redis) ? -1 : 0;
	}

	if(rank != PROC_MAIN)
		return 0;

	if(fork_dummy_timer(PROC_TIMER, "CNXCC TB TIMER", 1, check_calls_by_money,
			   NULL, _data.check_period)
			< 0) {
		LM_ERR("Failed registering TB TIMER routine as process\n");
		return -1;
	}

	if(fork_dummy_timer(PROC_TIMER, "CNXCC MB TIMER", 1, check_calls_by_time,
			   NULL, _data.check_period)
			< 0) {
		LM_ERR("Failed registering MB TIMER routine as process\n");
		return -1;
	}

	if(_data.redis_cnn_str.len <= 0)
		return 0;


	pid = fork_process(PROC_NOCHLDINIT, "Redis Async receiver", 1);

	if(pid < 0) {
		LM_ERR("error forking Redis receiver\n");
		return -1;
	} else if(pid == 0) {
		_data.redis = redis_connect_async(_data.redis_cnn_info.host,
				_data.redis_cnn_info.port, _data.redis_cnn_info.db);

		return (!_data.redis) ? -1 : 0;
		;
	}

	return 0;
}

static int __init_hashtable(struct str_hash_table *ht)
{
	if(ht == NULL)
		return -1;

	if(__shm_str_hash_alloc(ht, HT_SIZE) != 0)
		return -1;

	str_hash_init(ht);
	return 0;
}

static void __dialog_created_callback(
		struct dlg_cell *cell, int type, struct dlg_cb_params *_params)
{
	struct sip_msg *msg = NULL;

	msg = _params->direction == SIP_REPLY ? _params->rpl : _params->req;

	if(msg == NULL) {
		LM_ERR("Error getting direction of SIP msg\n");
		return;
	}

	if(isflagset(msg, _data.ctrl_flag) == -1) {
		LM_DBG("Flag is not set for this message. Ignoring\n");
		return;
	}

	LM_DBG("Dialog created for CID [%.*s]\n", cell->callid.len, cell->callid.s);

	_dlgbinds.register_dlgcb(
			cell, DLGCB_CONFIRMED, __dialog_confirmed_callback, NULL, NULL);
	_dlgbinds.register_dlgcb(cell,
			DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED,
			__dialog_terminated_callback, NULL, NULL);

	__setup_billing(&cell->callid, cell->h_entry, cell->h_id);
}

static void __dialog_confirmed_callback(
		struct dlg_cell *cell, int type, struct dlg_cb_params *_params)
{
	LM_DBG("Dialog confirmed for CID [%.*s]\n", cell->callid.len,
			cell->callid.s);

	__start_billing(&cell->callid, &cell->from_uri, &cell->to_uri, cell->tag);
}

static void __dialog_terminated_callback(
		struct dlg_cell *cell, int type, struct dlg_cb_params *_params)
{
	LM_DBG("Dialog terminated for CID [%.*s]\n", cell->callid.len,
			cell->callid.s);

	__stop_billing(&cell->callid);
}

static void __notify_call_termination(sip_msg_t *msg)
{
	struct run_act_ctx ra_ctx;

	init_run_actions_ctx(&ra_ctx);
	//run_top_route(event_rt.rlist[_data.cs_route_number], msg, &ra_ctx);

	if(run_actions(&ra_ctx, event_rt.rlist[_data.cs_route_number], msg) < 0)
		LM_ERR("Error executing cnxcc:call-shutdown route\n");
}

int try_get_credit_data_entry(str *client_id, credit_data_t **credit_data)
{
	struct str_hash_entry *cd_entry = NULL;
	hash_tables_t *hts = NULL;
	*credit_data = NULL;

	/* by money */
	hts = &_data.money;
	cnxcc_lock(hts->lock);

	cd_entry = str_hash_get(
			hts->credit_data_by_client, client_id->s, client_id->len);

	if(cd_entry != NULL) {
		*credit_data = cd_entry->u.p;
		cnxcc_unlock(hts->lock);
		return 0;
	}

	cnxcc_unlock(hts->lock);

	/* by time */
	hts = &_data.time;
	cnxcc_lock(hts->lock);

	cd_entry = str_hash_get(
			hts->credit_data_by_client, client_id->s, client_id->len);

	if(cd_entry != NULL) {
		*credit_data = cd_entry->u.p;
		cnxcc_unlock(hts->lock);
		return 0;
	}

	cnxcc_unlock(hts->lock);

	/* by channel */
	hts = &_data.channel;
	cnxcc_lock(hts->lock);

	cd_entry = str_hash_get(
			hts->credit_data_by_client, client_id->s, client_id->len);

	if(cd_entry != NULL) {
		*credit_data = cd_entry->u.p;
		cnxcc_unlock(hts->lock);
		return 0;
	}

	cnxcc_unlock(hts->lock);
	return -1;
}

int try_get_call_entry(str *callid, call_t **call, hash_tables_t **hts)
{
	struct str_hash_entry *call_entry = NULL;

	*call = NULL;

	/* by money */
	*hts = &_data.money;
	cnxcc_lock((*hts)->lock);

	call_entry = str_hash_get((*hts)->call_data_by_cid, callid->s, callid->len);

	if(call_entry != NULL) {
		*call = call_entry->u.p;
		cnxcc_unlock((*hts)->lock);
		return 0;
	}

	cnxcc_unlock((*hts)->lock);

	/* by time */
	*hts = &_data.time;
	cnxcc_lock((*hts)->lock);

	call_entry = str_hash_get((*hts)->call_data_by_cid, callid->s, callid->len);

	if(call_entry != NULL) {
		*call = call_entry->u.p;
		cnxcc_unlock((*hts)->lock);
		return 0;
	}

	cnxcc_unlock((*hts)->lock);

	/* by channel */
	*hts = &_data.channel;
	cnxcc_lock((*hts)->lock);

	call_entry = str_hash_get((*hts)->call_data_by_cid, callid->s, callid->len);

	if(call_entry != NULL) {
		*call = call_entry->u.p;
		cnxcc_unlock((*hts)->lock);
		return 0;
	}

	cnxcc_unlock((*hts)->lock);
	return -1;
}

static void __stop_billing(str *callid)
{
	struct str_hash_entry *cd_entry = NULL;
	call_t *call = NULL;
	hash_tables_t *hts = NULL;
	credit_data_t *credit_data = NULL;

	/*
	 * Search call data by call-id
	 */
	if(try_get_call_entry(callid, &call, &hts) != 0) {
		LM_ERR("Call [%.*s] not found\n", callid->len, callid->s);
		return;
	}

	if(call == NULL) {
		LM_ERR("[%.*s] call pointer is null\n", callid->len, callid->s);
		return;
	}
	if(hts == NULL) {
		LM_ERR("[%.*s] result hashtable pointer is null\n", callid->len,
				callid->s);
		return;
	}

	cnxcc_lock(hts->lock);
	// Search credit_data by client_id
	cd_entry = str_hash_get(
			hts->credit_data_by_client, call->client_id.s, call->client_id.len);
	if(cd_entry == NULL) {
		LM_ERR("Credit data not found for CID [%.*s], client-ID [%.*s]\n",
				callid->len, callid->s, call->client_id.len, call->client_id.s);
		cnxcc_unlock(hts->lock);
		return;
	}

	credit_data = (credit_data_t *)cd_entry->u.p;
	if(credit_data == NULL) {
		LM_ERR("[%.*s]: credit_data pointer is null\n", callid->len, callid->s);
		cnxcc_unlock(hts->lock);
		return;
	}
	cnxcc_unlock(hts->lock);

	LM_DBG("Call [%.*s] of client-ID [%.*s], ended\n", callid->len, callid->s,
			call->client_id.len, call->client_id.s);
	cnxcc_lock(credit_data->lock);
	__delete_call(call, credit_data);

	/*
	 * In case there are no active calls for a certain client, we remove the client-id from the hash table.
	 * This way, we can save memory for useful clients.
	 */
	if(credit_data->number_of_calls == 0) {
		__free_credit_data(credit_data, hts, cd_entry);
		return;
	}

	cnxcc_unlock(credit_data->lock);
}

static void __setup_billing(
		str *callid, unsigned int h_entry, unsigned int h_id)
{
	call_t *call = NULL;
	hash_tables_t *hts = NULL;

	LM_DBG("Creating dialog for [%.*s], h_id [%u], h_entry [%u]\n", callid->len,
			callid->s, h_id, h_entry);

	//	cnxcc_lock(&_data);

	/*
	 * Search call data by call-id
	 */
	if(try_get_call_entry(callid, &call, &hts) != 0) {
		LM_ERR("Call [%.*s] not found\n", callid->len, callid->s);
		return;
	}

	if(call == NULL) {
		LM_ERR("[%.*s] call pointer is null\n", callid->len, callid->s);
		return;
	}

	if(hts == NULL) {
		LM_ERR("[%.*s] result hashtable pointer is null\n", callid->len,
				callid->s);
		return;
	}

	/*
	 * Update calls statistics
	 */
	cnxcc_lock(_data.lock);

	_data.stats->active++;
	_data.stats->total++;

	cnxcc_unlock(_data.lock);

	cnxcc_lock(call->lock);

	call->dlg_h_entry = h_entry;
	call->dlg_h_id = h_id;

	LM_DBG("Call [%.*s] from client [%.*s], created\n", callid->len, callid->s,
			call->client_id.len, call->client_id.s);

	cnxcc_unlock(call->lock);
}

static void __start_billing(
		str *callid, str *from_uri, str *to_uri, str tags[2])
{
	struct str_hash_entry *cd_entry = NULL;
	call_t *call = NULL;
	hash_tables_t *hts = NULL;
	credit_data_t *credit_data = NULL;

	LM_DBG("Billing started for call [%.*s]\n", callid->len, callid->s);

	//	cnxcc_lock(&_data);

	/*
	 * Search call data by call-id
	 */
	if(try_get_call_entry(callid, &call, &hts) != 0) {
		LM_ERR("Call [%.*s] not found\n", callid->len, callid->s);
		return;
	}

	if(call == NULL) {
		LM_ERR("[%.*s] call pointer is null\n", callid->len, callid->s);
		return;
	}

	if(hts == NULL) {
		LM_ERR("[%.*s] result hashtable pointer is null", callid->len,
				callid->s);
		return;
	}

	cnxcc_lock(hts->lock);

	/*
	 * Search credit_data by client_id
	 */
	cd_entry = str_hash_get(
			hts->credit_data_by_client, call->client_id.s, call->client_id.len);

	if(cd_entry == NULL) {
		LM_ERR("Credit data not found for CID [%.*s], client-ID [%.*s]\n",
				callid->len, callid->s, call->client_id.len, call->client_id.s);
		cnxcc_unlock(hts->lock);
		return;
	}

	credit_data = (credit_data_t *)cd_entry->u.p;

	if(credit_data == NULL) {
		LM_ERR("[%.*s]: credit_data pointer is null\n", callid->len, callid->s);
		cnxcc_unlock(hts->lock);
		return;
	}

	cnxcc_unlock(hts->lock);

	cnxcc_lock(credit_data->lock);

	/*
	 * Now that the call is confirmed, we can increase the count of "concurrent_calls".
	 * This will impact in the discount rate performed by the check_calls() function.
	 *
	 */
	credit_data->concurrent_calls++;

	if(_data.redis)
		redis_incr_by_int(credit_data, "concurrent_calls", 1);

	if(credit_data->max_amount == 0) {
		credit_data->max_amount = call->max_amount; // first time setup

		if(_data.redis)
			redis_insert_double_value(
					credit_data, "max_amount", credit_data->max_amount);
	}

	if(call->max_amount > credit_data->max_amount) {
		LM_ALERT("Maximum-talk-time/credit changed, maybe a credit reload? %f "
				 "> %f. Client [%.*s]\n",
				call->max_amount, credit_data->max_amount, call->client_id.len,
				call->client_id.s);


		if(_data.redis)
			redis_insert_double_value(credit_data, "max_amount",
					call->max_amount - credit_data->max_amount);

		credit_data->max_amount += call->max_amount - credit_data->max_amount;
	}

	/*
	 * Update max_amount, discounting what was already consumed by other calls of the same client
	 */
	call->max_amount = credit_data->max_amount - credit_data->consumed_amount;

	cnxcc_unlock(credit_data->lock);

	cnxcc_lock(call->lock);

	/*
	 * Store from-tag value
	 */
	if(shm_str_dup(&call->sip_data.from_tag, &tags[0]) != 0) {
		SHM_MEM_ERROR;
		goto exit;
	}

	/*
	 * Store to-tag value
	 */
	if(shm_str_dup(&call->sip_data.to_tag, &tags[1]) != 0) {
		SHM_MEM_ERROR;
		goto exit;
	}

	if(shm_str_dup(&call->sip_data.from_uri, from_uri) != 0
			|| shm_str_dup(&call->sip_data.to_uri, to_uri) != 0) {
		SHM_MEM_ERROR;
		goto exit;
	}

	call->start_timestamp = get_current_timestamp();
	call->confirmed = TRUE;

	LM_DBG("Call [%.*s] from client [%.*s], confirmed. from=<%.*s>;tag=%.*s, "
		   "to=<%.*s>;tag=%.*s\n",
			callid->len, callid->s, call->client_id.len, call->client_id.s,
			call->sip_data.from_uri.len, call->sip_data.from_uri.s,
			call->sip_data.from_tag.len, call->sip_data.from_tag.s,
			call->sip_data.to_uri.len, call->sip_data.to_uri.s,
			call->sip_data.to_tag.len, call->sip_data.to_tag.s);
exit:
	cnxcc_unlock(call->lock);
}

static void __delete_call(call_t *call, credit_data_t *credit_data)
{
	// Update calls statistics
	cnxcc_lock(_data.lock);
	_data.stats->active--;
	_data.stats->total--;
	cnxcc_unlock(_data.lock);

	// This call just ended and we need to remove it from the summ.
	if(call->confirmed) {
		credit_data->concurrent_calls--;
		credit_data->ended_calls_consumed_amount += call->consumed_amount;

		if(_data.redis) {
			redis_incr_by_int(credit_data, "concurrent_calls", -1);
			redis_incr_by_double(credit_data, "ended_calls_consumed_amount",
					call->consumed_amount);
		}
	}

	credit_data->number_of_calls--;

	if(_data.redis)
		redis_incr_by_int(credit_data, "number_of_calls", -1);

	if(credit_data->concurrent_calls < 0) {
		LM_BUG("number of concurrent calls dropped to negative value: %d\n",
				credit_data->concurrent_calls);
	}

	if(credit_data->number_of_calls < 0) {
		LM_BUG("number of calls dropped to negative value: %d\n",
				credit_data->number_of_calls);
	}

	// Remove (and free) the call from the list of calls of the current credit_data
	clist_rm(call, next, prev);
	__free_call(call);
}

// must be called with lock held on credit_data
static void __free_credit_data(credit_data_t *credit_data, hash_tables_t *hts,
		struct str_hash_entry *cd_entry)
{
	if(credit_data->deallocating) {
		LM_DBG("deallocating, skip\n");
		return;
	}
	LM_DBG("Removing client [%.*s] and its calls from the list\n",
			credit_data->call_list->client_id.len,
			credit_data->call_list->client_id.s);
	credit_data->deallocating = 1;
	cnxcc_lock(hts->lock);
	if(_data.redis) {
		redis_clean_up_if_last(credit_data);
		shm_free(credit_data->str_id);
	}
	// Remove the credit_data_t from the hash table
	str_hash_del(cd_entry);
	cnxcc_unlock(hts->lock);

	// Free client_id in list's root
	shm_free(credit_data->call_list->client_id.s);
	shm_free(credit_data->call_list);

	// Release the lock since we are going to free the entry down below
	cnxcc_unlock(credit_data->lock);

	// Free the whole entry
	__free_credit_data_hash_entry(cd_entry);
}

// must be called with lock held on credit_data
/* terminate all calls and remove credit_data */
void terminate_all_calls(credit_data_t *credit_data)
{
	call_t *call = NULL, *tmp = NULL;
	struct str_hash_entry *cd_entry = NULL;
	hash_tables_t *hts = NULL;
	unsigned int pending = 0;

	switch(credit_data->type) {
		case CREDIT_MONEY:
			hts = &_data.money;
			break;
		case CREDIT_TIME:
			hts = &_data.time;
			break;
		case CREDIT_CHANNEL:
			hts = &_data.channel;
			break;
		default:
			LM_ERR("BUG: Something went terribly wrong\n");
			return;
	}

	cd_entry = str_hash_get(hts->credit_data_by_client,
			credit_data->call_list->client_id.s,
			credit_data->call_list->client_id.len);

	if(cd_entry == NULL) {
		LM_WARN("credit data item not found\n");
		return;
	}
	// tell __stop_billing() not to __free_credit_data
	credit_data->deallocating = 1;

	clist_foreach_safe(credit_data->call_list, call, tmp, next)
	{
		if(call->sip_data.callid.s != NULL) {
			if(call->confirmed) {
				LM_DBG("Killing call with CID [%.*s]\n",
						call->sip_data.callid.len, call->sip_data.callid.s);

				// Update number of calls forced to end
				_data.stats->dropped++;
				terminate_call(call);
				// call memory will be cleaned by __stop_billing() when
				// __dialog_terminated_callback() is triggered
			} else {
				LM_DBG("Non confirmed call with CID[%.*s], setting "
					   "max_amount:%f to 0\n",
						call->sip_data.callid.len, call->sip_data.callid.s,
						call->max_amount);
				call->max_amount = 0;
				pending = 1;
			}
		} else {
			LM_WARN("invalid call structure %p\n", call);
		}
	}

	credit_data->deallocating = 0;
	if(!pending) {
		__free_credit_data(credit_data, hts, cd_entry);
	} else {
		LM_DBG("credit data item left\n");
		cnxcc_unlock(credit_data->lock);
	}
}

/*
 * WARNING: When calling this function, the proper lock should have been acquired
 */
static void __free_call(call_t *call)
{
	struct str_hash_entry *e = NULL;

	if(call->sip_data.callid.s == NULL)
		return;

	LM_DBG("Freeing call [%.*s]\n", call->sip_data.callid.len,
			call->sip_data.callid.s);
	e = str_hash_get(_data.money.call_data_by_cid, call->sip_data.callid.s,
			call->sip_data.callid.len);

	if(e == NULL) {
		e = str_hash_get(_data.time.call_data_by_cid, call->sip_data.callid.s,
				call->sip_data.callid.len);

		if(e == NULL) {
			e = str_hash_get(_data.channel.call_data_by_cid,
					call->sip_data.callid.s, call->sip_data.callid.len);

			if(e == NULL) {
				LM_ERR("Call [%.*s] not found. Couldn't be able to free it "
					   "from hashtable",
						call->sip_data.callid.len, call->sip_data.callid.s);
				return;
			}
		}
	}

	str_hash_del(e);

	shm_free(e->key.s);
	shm_free(e);

	str_shm_free_if_not_null(call->sip_data.callid);
	str_shm_free_if_not_null(call->sip_data.to_uri);
	str_shm_free_if_not_null(call->sip_data.to_tag);
	str_shm_free_if_not_null(call->sip_data.from_uri);
	str_shm_free_if_not_null(call->sip_data.from_tag);

	shm_free(call);
}

/*
 * WARNING: When calling this function, the proper lock should have been acquired
 */
static void __free_credit_data_hash_entry(struct str_hash_entry *e)
{
	shm_free(e->key.s);
	//	shm_free(((credit_data_t *) e->u.p)->call);
	shm_free(e->u.p);
	shm_free(e);
}

static int __shm_str_hash_alloc(struct str_hash_table *ht, int size)
{
	ht->table = shm_malloc(sizeof(struct str_hash_head) * size);

	if(!ht->table) {
		SHM_MEM_ERROR;
		return -1;
	}
	ht->size = size;
	return 0;
}

int terminate_call(call_t *call)
{
	sip_msg_t *dmsg = NULL;
	sip_data_t *data = NULL;

	dlg_cell_t *cell;

	LM_DBG("Got kill signal for call [%.*s] client [%.*s] h_id [%u] h_entry "
		   "[%u]. Dropping it now\n",
			call->sip_data.callid.len, call->sip_data.callid.s,
			call->client_id.len, call->client_id.s, call->dlg_h_id,
			call->dlg_h_entry);

	data = &call->sip_data;
	if(cnxcc_faked_msg_init_with_dlg_info(&data->callid, &data->from_uri,
			   &data->from_tag, &data->to_uri, &data->to_tag, &dmsg)
			!= 0) {
		LM_ERR("[%.*s]: error generating faked sip message\n", data->callid.len,
				data->callid.s);
		goto error;
	}

	cell = _dlgbinds.get_dlg(dmsg);
	if(!cell) {
		LM_ERR("[%.*s]: cannot get dialog\n", data->callid.len, data->callid.s);
		goto error;
	}

	if(!_dlgbinds.terminate_dlg(cell, NULL)) {
		LM_DBG("dlg_end_dlg sent to call [%.*s]\n", cell->callid.len,
				cell->callid.s);

		if(_data.cs_route_number >= 0)
			__notify_call_termination(dmsg);
		return 0;
	}

	LM_ERR("Error executing terminate_dlg command");
error:
	return -1;
}

static credit_data_t *__get_or_create_credit_data_entry(
		str *client_id, credit_type_t type)
{
	struct str_hash_table *sht = NULL;
	struct hash_tables *ht;
	struct str_hash_entry *e = NULL;
	credit_data_t *credit_data = NULL;

	switch(type) {
		case CREDIT_MONEY:
			sht = _data.money.credit_data_by_client;
			ht = &_data.money;
			break;
		case CREDIT_TIME:
			sht = _data.time.credit_data_by_client;
			ht = &_data.time;
			break;
		case CREDIT_CHANNEL:
			sht = _data.channel.credit_data_by_client;
			ht = &_data.channel;
			break;
		default:
			LM_ERR("BUG: Something went terribly wrong\n");
			return NULL;
	}

	cnxcc_lock(ht->lock);
	e = str_hash_get(sht, client_id->s, client_id->len);
	cnxcc_unlock(ht->lock);

	/*
	 * Alloc new call_array_t if it doesn't exist
	 */
	if(e != NULL)
		LM_DBG("Found key %.*s in hash table\n", e->key.len, e->key.s);
	else if(e == NULL) {
		e = shm_malloc(sizeof(struct str_hash_entry));
		if(e == NULL)
			goto no_memory;

		if(shm_str_dup(&e->key, client_id) != 0)
			goto no_memory;

		e->u.p = credit_data = __alloc_new_credit_data(client_id, type);
		e->flags = 0;

		if(credit_data == NULL)
			goto no_memory;

		cnxcc_lock(ht->lock);
		str_hash_add(sht, e);
		cnxcc_unlock(ht->lock);

		LM_DBG("Credit entry didn't exist. Allocated new entry [%p]\n", e);
	}

	return (credit_data_t *)e->u.p;

no_memory:
	SHM_MEM_ERROR;
	return NULL;
}

static credit_data_t *__alloc_new_credit_data(
		str *client_id, credit_type_t type)
{
	credit_data_t *credit_data = shm_malloc(sizeof(credit_data_t));
	if(credit_data == NULL)
		goto no_memory;
	memset(credit_data, 0, sizeof(credit_data_t));

	cnxcc_lock_init(credit_data->lock);

	credit_data->call_list = shm_malloc(sizeof(call_t));
	if(credit_data->call_list == NULL)
		goto no_memory;

	clist_init(credit_data->call_list, next, prev);

	/*
	 * Copy the client_id value to the root of the calls list.
	 * This will be used later to get the credit_data_t of the
	 * call when it is being searched by call ID.
	 */
	if(shm_str_dup(&credit_data->call_list->client_id, client_id) != 0)
		goto no_memory;

	if(_data.redis) {
		credit_data->str_id = shm_malloc(client_id->len + 1);
		if(!credit_data->str_id)
			goto no_memory;

		memset(credit_data->str_id, 0, client_id->len + 1);
		snprintf(credit_data->str_id, client_id->len + 1, "%.*s",
				client_id->len, client_id->s);
	}
	credit_data->type = type;

	if(!_data.redis)
		return credit_data;

	if(redis_get_or_create_credit_data(credit_data) < 0)
		goto error;

	return credit_data;

no_memory:
	SHM_MEM_ERROR;
error:
	return NULL;
}

static call_t *__alloc_new_call_by_money(credit_data_t *credit_data,
		struct sip_msg *msg, double credit, double connect_cost,
		double cost_per_second, int initial_pulse, int final_pulse)
{
	call_t *call = NULL;

	cnxcc_lock(credit_data->lock);

	if(credit_data->call_list == NULL) {
		LM_ERR("Credit data call list is NULL\n");
		goto error;
	}

	call = shm_malloc(sizeof(call_t));
	if(call == NULL) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(call, 0, sizeof(call_t));

	if((!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) != 0)
			|| shm_str_dup(&call->sip_data.callid, &msg->callid->body) != 0) {
		LM_ERR("Error processing CALLID hdr\n");
		goto error;
	}

	call->consumed_amount = initial_pulse * cost_per_second;
	call->connect_amount = connect_cost;
	call->confirmed = FALSE;
	call->max_amount = credit;

	call->money_based.connect_cost = connect_cost;
	call->money_based.cost_per_second = cost_per_second;
	call->money_based.initial_pulse = initial_pulse;
	call->money_based.final_pulse = final_pulse;

	/*
	 * Reference the client_id from the root of the list
	 */
	call->client_id.s = credit_data->call_list->client_id.s;
	call->client_id.len = credit_data->call_list->client_id.len;

	/*
	 * Insert the newly created call to the list of calls
	 */
	clist_insert(credit_data->call_list, call, next, prev);

	cnxcc_lock_init(call->lock);

	/*
	 * Increase the number of calls for this client. This call is not yet confirmed.
	 */
	credit_data->number_of_calls++;
	if(_data.redis)
		redis_incr_by_int(credit_data, "number_of_calls", 1);

	cnxcc_unlock(credit_data->lock);

	LM_DBG("New call allocated for client [%.*s]\n", call->client_id.len,
			call->client_id.s);

	return call;

error:
	cnxcc_unlock(credit_data->lock);
	return NULL;
}

static call_t *__alloc_new_call_by_time(
		credit_data_t *credit_data, struct sip_msg *msg, int max_secs)
{
	call_t *call = NULL;

	cnxcc_lock(credit_data->lock);

	if(credit_data->call_list == NULL) {
		LM_ERR("Credit data call list is NULL\n");
		goto error;
	}

	call = shm_malloc(sizeof(call_t));
	if(call == NULL) {
		LM_ERR("No shared memory left\n");
		goto error;
	}
	memset(call, 0, sizeof(call_t));

	if((!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) != 0)
			|| shm_str_dup(&call->sip_data.callid, &msg->callid->body) != 0) {
		LM_ERR("Error processing CALLID hdr\n");
		goto error;
	}

	call->consumed_amount = 0;
	call->confirmed = FALSE;
	call->max_amount = max_secs;

	/*
	 * Reference the client_id from the root of the list
	 */
	call->client_id.s = credit_data->call_list->client_id.s;
	call->client_id.len = credit_data->call_list->client_id.len;

	/*
	 * Insert the newly created call to the list of calls
	 */
	clist_insert(credit_data->call_list, call, next, prev);

	cnxcc_lock_init(call->lock);

	/*
	 * Increase the number of calls for this client. This call is not yet confirmed.
	 */
	credit_data->number_of_calls++;
	if(_data.redis)
		redis_incr_by_int(credit_data, "number_of_calls", 1);

	cnxcc_unlock(credit_data->lock);

	LM_DBG("New call allocated for client [%.*s]\n", call->client_id.len,
			call->client_id.s);

	return call;

error:
	cnxcc_unlock(credit_data->lock);
	return NULL;
}

static call_t *alloc_new_call_by_channel(
		credit_data_t *credit_data, struct sip_msg *msg, int max_chan)
{
	call_t *call = NULL;

	cnxcc_lock(credit_data->lock);

	if(credit_data->call_list == NULL) {
		LM_ERR("Credit data call list is NULL\n");
		goto error;
	}

	call = shm_malloc(sizeof(call_t));
	if(call == NULL) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(call, 0, sizeof(call_t));

	if((!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) != 0)
			|| shm_str_dup(&call->sip_data.callid, &msg->callid->body) != 0) {
		LM_ERR("Error processing CALLID hdr\n");
		goto error;
	}

	call->consumed_amount = 0;
	call->confirmed = FALSE;
	call->max_amount = max_chan;

	/*
	 * Reference the client_id from the root of the list
	 */
	call->client_id.s = credit_data->call_list->client_id.s;
	call->client_id.len = credit_data->call_list->client_id.len;

	/*
	 * Insert the newly created call to the list of calls
	 */
	clist_insert(credit_data->call_list, call, next, prev);

	cnxcc_lock_init(call->lock);

	/*
	 * Increase the number of calls for this client. This call is not yet confirmed.
	 */
	credit_data->number_of_calls++;
	if(_data.redis)
		redis_incr_by_int(credit_data, "number_of_calls", 1);

	cnxcc_unlock(credit_data->lock);

	LM_DBG("New call allocated for client [%.*s]\n", call->client_id.len,
			call->client_id.s);


	return call;

error:
	cnxcc_unlock(credit_data->lock);
	return NULL;
}

static int __add_call_by_cid(str *cid, call_t *call, credit_type_t type)
{
	struct str_hash_table *ht = NULL;
	cnxcc_lock_t lock;
	struct str_hash_entry *e = NULL;

	switch(type) {
		case CREDIT_MONEY:
			ht = _data.money.call_data_by_cid;
			lock = _data.money.lock;
			break;
		case CREDIT_TIME:
			ht = _data.time.call_data_by_cid;
			lock = _data.time.lock;
			break;
		case CREDIT_CHANNEL:
			ht = _data.channel.call_data_by_cid;
			lock = _data.channel.lock;
			break;
		default:
			LM_ERR("Something went terribly wrong\n");
			return -1;
	}

	e = str_hash_get(ht, cid->s, cid->len);

	if(e != NULL) {
		LM_DBG("e != NULL\n");

		call_t *value = (call_t *)e->u.p;
		if(value == NULL) {
			LM_ERR("Value of CID [%.*s] is NULL\n", cid->len, cid->s);
			return -1;
		}

		if(value->sip_data.callid.len != cid->len
				|| strncasecmp(value->sip_data.callid.s, cid->s, cid->len)
						   != 0) {
			LM_ERR("Value of CID is [%.*s] and differs from value being added "
				   "[%.*s]\n",
					cid->len, cid->s, value->sip_data.callid.len,
					value->sip_data.callid.s);
			return -1;
		}

		LM_DBG("CID[%.*s] already present\n", cid->len, cid->s);
		return 0;
	}

	e = shm_malloc(sizeof(struct str_hash_entry));
	if(e == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}

	if(shm_str_dup(&e->key, cid) != 0) {
		SHM_MEM_ERROR;
		return -1;
	}

	e->u.p = call;

	cnxcc_lock(lock);
	str_hash_add(ht, e);
	cnxcc_unlock(lock);

	return 0;
}

static inline void set_ctrl_flag(struct sip_msg *msg)
{
	if(_data.ctrl_flag != -1) {
		LM_DBG("Flag set!\n");
		setflag(msg, _data.ctrl_flag);
	}
}

static inline int get_pv_value(
		struct sip_msg *msg, pv_spec_t *spec, pv_value_t *value)
{
	if(pv_get_spec_value(msg, spec, value) != 0) {
		LM_ERR("Can't get PV's value\n");
		return -1;
	}

	return 0;
}

static int ki_set_max_credit(sip_msg_t *msg, str *sclient, str *scredit,
		str *sconnect, str *scps, int initp, int finishp)
{
	credit_data_t *credit_data = NULL;
	call_t *call = NULL;
	hash_tables_t *hts = NULL;

	double credit = 0, connect_cost = 0, cost_per_second = 0;

	if(msg->first_line.type != SIP_REQUEST
			|| msg->first_line.u.request.method_value != METHOD_INVITE) {
		LM_ERR("not supported - it has to be used for INVITE\n");
		return -1;
	}

	if(__has_to_tag(msg)) {
		LM_ERR("INVITE is a reINVITE\n");
		return -1;
	}

	if(sclient->len == 0 || sclient->s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}

	credit = str2double(scredit);

	if(credit <= 0) {
		LM_ERR("credit value must be > 0: %f", credit);
		return -1;
	}

	connect_cost = str2double(sconnect);

	if(connect_cost < 0) {
		LM_ERR("connect_cost value must be >= 0: %f\n", connect_cost);
		return -1;
	}

	cost_per_second = str2double(scps);

	if(cost_per_second <= 0) {
		LM_ERR("cost_per_second value must be > 0: %f\n", cost_per_second);
		return -1;
	}

	if(try_get_call_entry(&msg->callid->body, &call, &hts) == 0) {
		LM_ERR("call-id[%.*s] already present\n",
		msg->callid->body.len, msg->callid->body.s);
		return -4;
	}

	LM_DBG("Setting up new call for client [%.*s], max-credit[%f], "
		   "connect-cost[%f], cost-per-sec[%f], initial-pulse [%d], "
		   "final-pulse [%d], call-id[%.*s]\n",
			sclient->len, sclient->s, credit, connect_cost, cost_per_second,
			initp, finishp, msg->callid->body.len, msg->callid->body.s);

	set_ctrl_flag(msg);

	if((credit_data = __get_or_create_credit_data_entry(sclient, CREDIT_MONEY))
			== NULL) {
		LM_ERR("Error retrieving credit data from shared memory for client "
			   "[%.*s]\n",
				sclient->len, sclient->s);
		return -1;
	}

	if((call = __alloc_new_call_by_money(credit_data, msg, credit, connect_cost,
				cost_per_second, initp, finishp))
			== NULL) {
		LM_ERR("Unable to allocate new call for client [%.*s]\n", sclient->len,
				sclient->s);
		return -1;
	}

	if(__add_call_by_cid(&call->sip_data.callid, call, CREDIT_MONEY) != 0) {
		LM_ERR("Unable to allocate new cid_by_client for client [%.*s]\n",
				sclient->len, sclient->s);
		return -1;
	}

	return 1;
}

static int __set_max_credit(sip_msg_t *msg, char *pclient, char *pcredit,
		char *pconnect, char *pcps, char *pinitp, char *pfinishp)
{
	str sclient;
	str scredit;
	str sconnect;
	str scps;
	int initp;
	int finishp;

	if(msg == NULL || pclient == NULL || pcredit == NULL || pconnect == NULL
			|| pcps == NULL || pinitp == NULL || pfinishp == NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)pclient, &sclient) < 0) {
		LM_ERR("failed to get client parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pcredit, &scredit) < 0) {
		LM_ERR("failed to get credit parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pconnect, &sconnect) < 0) {
		LM_ERR("failed to get connect parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pcps, &scps) < 0) {
		LM_ERR("failed to get cps parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)pinitp, &initp) < 0) {
		LM_ERR("failed to get init pulse parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)pfinishp, &finishp) < 0) {
		LM_ERR("failed to get finish pulse parameter\n");
		return -1;
	}

	return ki_set_max_credit(
			msg, &sclient, &scredit, &sconnect, &scps, initp, finishp);
}

static int ki_terminate_all(sip_msg_t *msg, str *sclient)
{
	credit_data_t *credit_data = NULL;

	if(sclient->len == 0 || sclient->s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}

	if(try_get_credit_data_entry(sclient, &credit_data) != 0) {
		LM_DBG("credit data for [%.*s] on [%.*s] not found\n", sclient->len,
				sclient->s, msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	terminate_all_calls(credit_data);
	return 1;
}

static int __terminate_all(sip_msg_t *msg, char *pclient, char *p2)
{
	str sclient;

	if(fixup_get_svalue(msg, (gparam_t *)pclient, &sclient) < 0) {
		LM_ERR("failed to get client parameter\n");
		return -1;
	}

	return ki_terminate_all(msg, &sclient);
}

static int __get_channel_count_helper(
		sip_msg_t *msg, str *sclient, pv_spec_t *pvcount)
{
	credit_data_t *credit_data = NULL;
	pv_value_t countval;
	int value = -1;

	if(!pv_is_w(pvcount)) {
		LM_ERR("pvar is not writable\n");
		return -1;
	}

	if(sclient->len == 0 || sclient->s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}

	if(try_get_credit_data_entry(sclient, &credit_data) == 0)
		value = credit_data->number_of_calls;
	else
		LM_ALERT("[%.*s] [%.*s] not found\n", sclient->len, sclient->s,
				msg->callid->body.len, msg->callid->body.s);

	memset(&countval, 0, sizeof(countval));

	countval.flags = PV_VAL_STR;

	countval.rs.s = sint2str(value, &countval.rs.len);

	if(pv_set_spec_value(msg, pvcount, 0, &countval) != 0) {
		LM_ERR("Error writing value to pseudo-variable\n");
		return -1;
	}

	return 1;
}

static int __get_channel_count(sip_msg_t *msg, char *pclient, char *pcount)
{
	str sclient;

	if(fixup_get_svalue(msg, (gparam_t *)pclient, &sclient) < 0) {
		LM_ERR("failed to get client parameter\n");
		return -1;
	}

	return __get_channel_count_helper(msg, &sclient, (pv_spec_t *)pcount);
}

static int ki_get_channel_count(sip_msg_t *msg, str *sclient, str *pvname)
{
	pv_spec_t *pvcount = NULL;

	pvcount = pv_cache_get(pvname);

	if(pvcount == NULL) {
		LM_ERR("failed to get pv spec for [%.*s]\n", pvname->len, pvname->s);
		return -1;
	}
	return __get_channel_count_helper(msg, sclient, pvcount);
}

static int ki_set_max_channels(sip_msg_t *msg, str *sclient, int max_chan)
{
	credit_data_t *credit_data = NULL;
	call_t *call = NULL;
	hash_tables_t *hts = NULL;

	if(parse_headers(msg, HDR_CALLID_F, 0) != 0) {
		LM_ERR("Error parsing Call-ID");
		return -1;
	}

	if(msg->first_line.type != SIP_REQUEST
			|| msg->first_line.u.request.method_value != METHOD_INVITE) {
		LM_ALERT("MSG was not an INVITE\n");
		return -1;
	}
	if(__has_to_tag(msg)) {
		LM_ERR("INVITE is a reINVITE\n");
		return -1;
	}

	set_ctrl_flag(msg);

	if(max_chan <= 0) {
		LM_ERR("[%.*s] MAX_CHAN cannot be less than or equal to zero: %d\n",
				msg->callid->body.len, msg->callid->body.s, max_chan);
		return -1;
	}

	if(sclient->len == 0 || sclient->s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}

	if(try_get_call_entry(&msg->callid->body, &call, &hts) == 0) {
		LM_ERR("call-id[%.*s] already present\n",
		msg->callid->body.len, msg->callid->body.s);
		return -4;
	}

	LM_DBG("Setting up new call for client [%.*s], max-chan[%d], "
		   "call-id[%.*s]\n",
			sclient->len, sclient->s, max_chan, msg->callid->body.len,
			msg->callid->body.s);

	if((credit_data = __get_or_create_credit_data_entry(
				sclient, CREDIT_CHANNEL))
			== NULL) {
		LM_ERR("Error retrieving credit data from shared memory for client "
			   "[%.*s]\n",
				sclient->len, sclient->s);
		return -1;
	}

	if(credit_data->number_of_calls + 1 > max_chan)
		return -2; // you have, between calls being setup plus those established, more than you maximum quota

	if(credit_data->concurrent_calls + 1 > max_chan)
		return -3; // you have the max amount of established calls already

	if((call = alloc_new_call_by_channel(credit_data, msg, max_chan)) == NULL) {
		LM_ERR("Unable to allocate new call for client [%.*s]\n", sclient->len,
				sclient->s);
		return -1;
	}

	if(__add_call_by_cid(&call->sip_data.callid, call, CREDIT_CHANNEL) != 0) {
		LM_ERR("Unable to allocate new cid_by_client for client [%.*s]\n",
				sclient->len, sclient->s);
		return -1;
	}

	return 1;
}

static int __set_max_channels(sip_msg_t *msg, char *pclient, char *pmaxchan)
{
	str sclient;
	int max_chan = 0;

	if(fixup_get_svalue(msg, (gparam_t *)pclient, &sclient) < 0) {
		LM_ERR("failed to get client parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)pmaxchan, &max_chan) < 0) {
		LM_ERR("failed to get max chan parameter\n");
		return -1;
	}

	return ki_set_max_channels(msg, &sclient, max_chan);
}

static int ki_set_max_time(sip_msg_t *msg, str *sclient, int max_secs)
{
	credit_data_t *credit_data = NULL;
	call_t *call = NULL;
	hash_tables_t *hts = NULL;

	if(parse_headers(msg, HDR_CALLID_F, 0) != 0) {
		LM_ERR("Error parsing Call-ID");
		return -1;
	}

	if(msg->first_line.type != SIP_REQUEST
			|| msg->first_line.u.request.method_value != METHOD_INVITE) {
		LM_ALERT("MSG was not an INVITE\n");
		return -1;
	}

	if(__has_to_tag(msg)) {
		LM_ERR("INVITE is a reINVITE\n");
		return -1;
	}

	set_ctrl_flag(msg);

	if(max_secs <= 0) {
		LM_ERR("[%.*s] MAXSECS cannot be less than or equal to zero: %d\n",
				msg->callid->body.len, msg->callid->body.s, max_secs);
		return -1;
	}

	if(sclient->len <= 0 || sclient->s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}

	if(try_get_call_entry(&msg->callid->body, &call, &hts) == 0) {
		LM_ERR("call-id[%.*s] already present\n",
		msg->callid->body.len, msg->callid->body.s);
		return -4;
	}

	LM_DBG("Setting up new call for client [%.*s], max-secs[%d], "
		   "call-id[%.*s]\n",
			sclient->len, sclient->s, max_secs, msg->callid->body.len,
			msg->callid->body.s);

	if((credit_data = __get_or_create_credit_data_entry(sclient, CREDIT_TIME))
			== NULL) {
		LM_ERR("Error retrieving credit data from shared memory for client "
			   "[%.*s]\n",
				sclient->len, sclient->s);
		return -1;
	}

	if((call = __alloc_new_call_by_time(credit_data, msg, max_secs)) == NULL) {
		LM_ERR("Unable to allocate new call for client [%.*s]\n", sclient->len,
				sclient->s);
		return -1;
	}

	if(__add_call_by_cid(&call->sip_data.callid, call, CREDIT_TIME) != 0) {
		LM_ERR("Unable to allocate new cid_by_client for client [%.*s]\n",
				sclient->len, sclient->s);
		return -1;
	}

	return 1;
}

static int __set_max_time(sip_msg_t *msg, char *pclient, char *pmaxsecs)
{
	str sclient;
	int max_secs = 0;

	if(fixup_get_svalue(msg, (gparam_t *)pclient, &sclient) < 0) {
		LM_ERR("failed to get client parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)pmaxsecs, &max_secs) < 0) {
		LM_ERR("failed to get max secs parameter\n");
		return -1;
	}

	return ki_set_max_time(msg, &sclient, max_secs);
}

static int ki_update_max_time(sip_msg_t *msg, str *sclient, int secs)
{
	credit_data_t *credit_data = NULL;
	struct str_hash_table *ht = NULL;
	struct str_hash_entry *e = NULL;
	double update_fraction = secs;
	call_t *call = NULL, *tmp_call = NULL;

	set_ctrl_flag(msg);

	if(parse_headers(msg, HDR_CALLID_F, 0) != 0) {
		LM_ERR("Error parsing Call-ID");
		return -1;
	}

	if(secs <= 0) {
		LM_ERR("[%.*s] MAXSECS cannot be less than or equal to zero: %d\n",
				msg->callid->body.len, msg->callid->body.s, secs);
		return -1;
	}

	if(sclient->len == 0 || sclient->s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}

	LM_DBG("Updating call for client [%.*s], max-secs[%d], call-id[%.*s]\n",
			sclient->len, sclient->s, secs, msg->callid->body.len,
			msg->callid->body.s);

	ht = _data.time.credit_data_by_client;

	cnxcc_lock(_data.time.lock);
	e = str_hash_get(ht, sclient->s, sclient->len);
	cnxcc_unlock(_data.time.lock);

	if(e == NULL) {
		LM_ERR("Client [%.*s] was not found\n", sclient->len, sclient->s);
		return -1;
	}

	credit_data = (credit_data_t *)e->u.p;
	cnxcc_lock(credit_data->lock);

	LM_DBG("Updating max-secs for [%.*s] from [%f] to [%f]\n", e->key.len,
			e->key.s, credit_data->max_amount, credit_data->max_amount + secs);

	credit_data->max_amount += secs;

	if(credit_data->number_of_calls > 0)
		update_fraction = secs / credit_data->number_of_calls;

	clist_foreach_safe(credit_data->call_list, call, tmp_call, next)
	{
		if(!call->confirmed)
			continue;

		call->max_amount += update_fraction;
	}

	//redit_data->consumed_amount = 0;

	cnxcc_unlock(credit_data->lock);

	return 1;
}

static int __update_max_time(sip_msg_t *msg, char *pclient, char *psecs)
{
	str sclient;
	int secs = 0;

	if(fixup_get_svalue(msg, (gparam_t *)pclient, &sclient) < 0) {
		LM_ERR("failed to get client parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)psecs, &secs) < 0) {
		LM_ERR("failed to get secs parameter\n");
		return -1;
	}

	return ki_update_max_time(msg, &sclient, secs);
}

static int __has_to_tag(struct sip_msg *msg)
{
	if(msg->to == NULL && parse_headers(msg, HDR_TO_F, 0) != 0) {
		LM_ERR("Cannot parse to-tag\n");
		return 0;
	}

	return !(get_to(msg)->tag_value.s == NULL
			 || get_to(msg)->tag_value.len == 0);
}

static int __pv_parse_calls_param(pv_spec_p sp, str *in)
{
	if(sp == NULL || in == NULL || in->len == 0)
		return -1;

	switch(in->len) {
		case 5:
			if(strncmp("total", in->s, in->len) == 0)
				sp->pvp.pvn.u.isname.name.n = CNX_PV_TOTAL;
			else
				return -1;
			break;
		case 6:
			if(strncmp("active", in->s, in->len) == 0)
				sp->pvp.pvn.u.isname.name.n = CNX_PV_ACTIVE;
			else
				return -1;
			break;
		case 7:
			if(strncmp("dropped", in->s, in->len) == 0)
				sp->pvp.pvn.u.isname.name.n = CNX_PV_DROPPED;
			else
				return -1;
			break;
	}

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;
}

static int __pv_get_calls(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	switch(param->pvn.u.isname.name.n) {
		case CNX_PV_ACTIVE:
			return pv_get_uintval(msg, param, res, _data.stats->active);
		case CNX_PV_TOTAL:
			return pv_get_uintval(msg, param, res, _data.stats->total);
		case CNX_PV_DROPPED:
			return pv_get_uintval(msg, param, res, _data.stats->dropped);
		default:
			LM_ERR("Unknown PV type %ld\n", param->pvn.u.isname.name.n);
			break;
	}

	return -1;
}

void rpc_credit_control_stats(rpc_t *rpc, void *ctx)
{
	void *rh;

	if(rpc->add(ctx, "{", &rh) < 0) {
		rpc->fault(ctx, 500, "Server failure");
		return;
	}

	rpc->struct_add(rh, "sddd", "info", "CNX Credit Control", "active",
			_data.stats->active, "dropped", _data.stats->dropped, "total",
			_data.stats->total);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_cnxcc_exports[] = {
	{ str_init("cnxcc"), str_init("set_max_credit"),
		SR_KEMIP_INT, ki_set_max_credit,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_INT }
	},
	{ str_init("cnxcc"), str_init("set_max_time"),
		SR_KEMIP_INT, ki_set_max_time,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cnxcc"), str_init("update_max_time"),
		SR_KEMIP_INT, ki_update_max_time,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cnxcc"), str_init("set_max_channels"),
		SR_KEMIP_INT, ki_set_max_channels,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cnxcc"), str_init("get_channel_count"),
		SR_KEMIP_INT, ki_get_channel_count,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("cnxcc"), str_init("terminate_all"),
		SR_KEMIP_INT, ki_terminate_all,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_cnxcc_exports);
	return 0;
}
