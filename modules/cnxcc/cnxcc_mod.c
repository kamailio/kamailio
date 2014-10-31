/*
 * $Id$
 *
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
#include <time.h>
#include <ctype.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../shm_init.h"
#include "../../mem/shm_mem.h"
#include "../../pvar.h"
#include "../../locking.h"
#include "../../lock_ops.h"
#include "../../str_hash.h"
#include "../../timer_proc.h"
#include "../../modules/tm/tm_load.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/contact/contact.h"
#include "../../parser/parse_rr.h"
#include "../../mod_fix.h"
#include "../dialog/dlg_load.h"
#include "../dialog/dlg_hash.h"
#include "../../mi/mi_types.h"
#include "../../lib/kcore/faked_msg.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "cnxcc_mod.h"
#include "cnxcc.h"
#include "cnxcc_sip_msg_faker.h"
#include "cnxcc_check.h"
#include "cnxcc_rpc.h"
#include "cnxcc_select.h"
#include "cnxcc_redis.h"

MODULE_VERSION

#define HT_SIZE 229
#define MODULE_NAME	"cnxcc"
#define NUMBER_OF_TIMERS 2

#define TRUE	1
#define FALSE	!TRUE

data_t _data;
struct dlg_binds _dlgbinds;

static int __fixup_pvar(void** param, int param_no);

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

/*
 * PV management functions
 */
static int __pv_parse_calls_param(pv_spec_p sp, str *in);
static int __pv_get_calls(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

/*
 * Billing management functions
 */
static int __set_max_time(struct sip_msg* msg, char* number, char* str2);
static int __update_max_time(struct sip_msg* msg, char* number, char* str2);
static int __set_max_credit(struct sip_msg* msg, char *str_pv_client, char *str_pv_credit,
                            char *str_pv_cps, char *str_pv_inip, char *str_pv_finp);
static int __set_max_channels(struct sip_msg* msg, char* str_pv_client, char* str_pv_max_chan);
static int __get_channel_count(struct sip_msg* msg, char* str_pv_client, char* str_pv_max_chan);
static int __terminate_all(struct sip_msg* msg, char* str_pv_client);

static void __start_billing(str *callid, str *from_uri, str *to_uri, str tags[2]);
static void __setup_billing(str *callid, unsigned int h_entry, unsigned int h_id);
static void __stop_billing(str *callid);
static int __add_call_by_cid(str *cid, call_t *call, credit_type_t type);
static call_t *__alloc_new_call_by_time(credit_data_t *credit_data, struct sip_msg *msg, int max_secs);
static call_t *__alloc_new_call_by_money(credit_data_t *credit_data, struct sip_msg *msg, double credit,
		                                 double cost_per_second, int initial_pulse, int final_pulse);
static void __notify_call_termination(sip_data_t *data);
static void __free_call(call_t *call);
static int __has_to_tag(struct sip_msg *msg);
static credit_data_t *__alloc_new_credit_data(str *client_id, credit_type_t type);
static credit_data_t *__get_or_create_credit_data_entry(str *client_id, credit_type_t type);

/*
 * MI interface
 */
static struct mi_root *__mi_credit_control_stats(struct mi_root *tree, void *param);

/*
 * Dialog management callback functions
 */
static void __dialog_terminated_callback(struct dlg_cell *cell, int type, struct dlg_cb_params *params);
static void __dialog_confirmed_callback(struct dlg_cell *cell, int type, struct dlg_cb_params *params);
static void __dialog_created_callback(struct dlg_cell *cell, int type, struct dlg_cb_params *params);

static pv_export_t mod_pvs[] = {
	{ {"cnxcc", sizeof("cnxcc")-1 }, PVT_OTHER, __pv_get_calls, 0, __pv_parse_calls_param, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[] = {
	{"cnxcc_set_max_time", (cmd_function) __set_max_time, 2, fixup_pvar_pvar,
        fixup_free_pvar_pvar, ANY_ROUTE},
    {"cnxcc_update_max_time", (cmd_function) __update_max_time, 2, fixup_pvar_pvar,
        fixup_free_pvar_pvar, ANY_ROUTE},
    {"cnxcc_set_max_credit", (cmd_function) __set_max_credit, 5, __fixup_pvar,
        NULL, ANY_ROUTE},
    {"cnxcc_set_max_channels", (cmd_function) __set_max_channels, 2, fixup_pvar_pvar,
        NULL, ANY_ROUTE},
    {"cnxcc_get_channel_count", (cmd_function) __get_channel_count, 2, fixup_pvar_pvar,
        NULL, ANY_ROUTE},
    {"cnxcc_terminate_all", (cmd_function) __terminate_all, 1, fixup_pvar_null,
        NULL, ANY_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[] = {
	{"dlg_flag", INT_PARAM,	&_data.ctrl_flag },
	{"credit_check_period", INT_PARAM,	&_data.check_period },
	{"redis", STR_PARAM, &_data.redis_cnn_str.s },
	{ 0, 0, 0 }
};

static const char* rpc_active_clients_doc[2] = {
	"List of clients with active calls",
	NULL
};

static const char* rpc_check_client_stats_doc[2] = {
	"Check specific client calls",
	NULL
};

static const char* rpc_kill_call_doc[2] = {
	"Kill call using its call ID",
	NULL
};

rpc_export_t ul_rpc[] = {
    { "cnxcc.active_clients", rpc_active_clients, rpc_active_clients_doc,	0},
    { "cnxcc.check_client", rpc_check_client_stats, rpc_check_client_stats_doc,	0},
    { "cnxcc.kill_call", rpc_kill_call, rpc_kill_call_doc, 0},
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
	MODULE_NAME,
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	NULL,          	/* exported statistics */
	NULL, 		    /* exported MI functions */
	mod_pvs,  	    /* exported pseudo-variables */
	NULL,          	/* extra processes */
	__mod_init,   	/* module initialization function */
	NULL,
	NULL,
	__child_init	/* per-child init function */
};

static int __fixup_pvar(void** param, int param_no) {
	str var;

	var.s	= (char *) *param;
	var.len = strlen(var.s);

	if (fixup_pvar_null(param, 1))
	{
		LM_ERR("Invalid PV [%.*s] as parameter\n", var.len, var.s);
		return E_CFG;
	}
/*
	if (((pv_spec_t*)(*param))->setf == NULL)
	{
		LM_ERR("[%.*s] has to be writable\n", var.len, var.s);
		return E_CFG;
	} */

	return 0;
}

static int __mod_init(void) {
	int len;
	char *chr;

	LM_INFO("Loading " MODULE_NAME " module\n");

	_data.cs_route_number = route_get(&event_rt, "cnxcc:call-shutdown");

	if (_data.cs_route_number < 0)
		LM_INFO("No cnxcc:call-shutdown event route found\n");

	if (_data.cs_route_number > 0 && event_rt.rlist[_data.cs_route_number] == NULL) {
		LM_INFO("cnxcc:call-shutdown route is empty\n");
		_data.cs_route_number	= -1;
	}

	if (_data.check_period <= 0) {
		LM_INFO("credit_check_period cannot be less than 1 second\n");
		return -1;
	}

	if (_data.redis_cnn_str.s)
		_data.redis_cnn_str.len = strlen(_data.redis_cnn_str.s);

	_data.time.credit_data_by_client = shm_malloc(sizeof(struct str_hash_table));
	_data.time.call_data_by_cid = shm_malloc(sizeof(struct str_hash_table));
	_data.money.credit_data_by_client = shm_malloc(sizeof(struct str_hash_table));
	_data.money.call_data_by_cid = shm_malloc(sizeof(struct str_hash_table));
	_data.channel.credit_data_by_client	= shm_malloc(sizeof(struct str_hash_table));
	_data.channel.call_data_by_cid = shm_malloc(sizeof(struct str_hash_table));

	_data.stats = (stats_t *) shm_malloc(sizeof(stats_t));

	if (!_data.stats) {
		LM_ERR("Error allocating shared memory stats\n");
		return -1;
	}

	_data.stats->active = 0;
	_data.stats->dropped = 0;
	_data.stats->total = 0;

	if (__init_hashtable(_data.time.credit_data_by_client) != 0)
		return -1;

	if (__init_hashtable(_data.time.call_data_by_cid) != 0)
		return -1;

	if (__init_hashtable(_data.money.credit_data_by_client) != 0)
		return -1;

	if (__init_hashtable(_data.money.call_data_by_cid) != 0)
		return -1;

	if (__init_hashtable(_data.channel.credit_data_by_client) != 0)
		return -1;

	if (__init_hashtable(_data.channel.call_data_by_cid) != 0)
		return -1;

	lock_init(&_data.lock);
	lock_init(&_data.time.lock);
	lock_init(&_data.money.lock);
	lock_init(&_data.channel.lock);

	register_mi_cmd(__mi_credit_control_stats, "cnxcc_stats", NULL, NULL, 0);

	/*
	 * One for time based monitoring
	 * One for money based monitoring
	 */
	register_dummy_timers(NUMBER_OF_TIMERS);

	if (rpc_register_array(ul_rpc) != 0) {
		LM_ERR("Failed registering RPC commands\n");
		return -1;
	}

	if (load_dlg_api(&_dlgbinds) != 0) {
		LM_ERR("Error loading dialog API\n");
	    return -1;
	}

	_dlgbinds.register_dlgcb(NULL, DLGCB_CREATED, __dialog_created_callback, NULL, NULL);

	register_select_table(sel_declaration);

	// redis configuration setup
	if (_data.redis_cnn_str.len <= 0)
		return 0;

	// replace ";" for " ", so we can use a simpler pattern in sscanf()
	for(chr = _data.redis_cnn_str.s; *chr; chr++)
		if (*chr == ';')
			*chr = ' ';

	memset(_data.redis_cnn_info.host, 0, sizeof(_data.redis_cnn_info.host));
	sscanf(_data.redis_cnn_str.s, "addr=%s port=%d db=%d", _data.redis_cnn_info.host,
                                                           &_data.redis_cnn_info.port,
                                                           &_data.redis_cnn_info.db);

	len = strlen(_data.redis_cnn_info.host);
	//
	// Redis modparam validations
	//
	if (len == 0) {
		LM_ERR("invalid host address [%s]", _data.redis_cnn_info.host);
		return -1;
	}

	if (_data.redis_cnn_info.port <= 0) {
		LM_ERR("invalid port number [%d]", _data.redis_cnn_info.port);
		return -1;
	}

	if (_data.redis_cnn_info.db < 0) {
		LM_ERR("invalid db number [%d]",_data.redis_cnn_info.db);
		return -1;
	}

	LM_INFO("Redis connection info: ip=[%s], port=[%d], database=[%d]", _data.redis_cnn_info.host,
			                                                            _data.redis_cnn_info.port,
			                                                            _data.redis_cnn_info.db);

	register_procs(3/* 2 timers + 1 redis async receiver */);
	return 0;
}

static int __child_init(int rank) {
	int pid = 0;

	if (rank!=PROC_INIT && rank!=PROC_MAIN && rank!=PROC_TCP_MAIN) {
		if (_data.redis_cnn_str.len <= 0)
			return 0;

		_data.redis = redis_connect(_data.redis_cnn_info.host,
                                        _data.redis_cnn_info.port,
                                        _data.redis_cnn_info.db);
		return (!_data.redis) ? -1 : 0;
	}

	if (rank != PROC_MAIN)
		return 0;

	if(fork_dummy_timer(PROC_TIMER, "CNXCC TB TIMER", 1, check_calls_by_money, NULL, _data.check_period) < 0) {
		LM_ERR("Failed registering TB TIMER routine as process\n");
		return -1;
	}

	if(fork_dummy_timer(PROC_TIMER, "CNXCC MB TIMER", 1, check_calls_by_time, NULL, _data.check_period) < 0) {
		LM_ERR("Failed registering MB TIMER routine as process\n");
		return -1;
	}

	if (_data.redis_cnn_str.len <= 0)
		return 0;


	pid = fork_process(PROC_NOCHLDINIT, "Redis Async receiver", 1);

	if (pid < 0) {
		LM_ERR("error forking Redis receiver\n");
		return -1;
	}
	else if (pid == 0) {
		_data.redis = redis_connect_async(_data.redis_cnn_info.host,
                                          _data.redis_cnn_info.port,
                                          _data.redis_cnn_info.db);

		return (!_data.redis) ? -1 : 0;;
	}

	return 0;
}

static int __init_hashtable(struct str_hash_table *ht) {
	if (__shm_str_hash_alloc(ht, HT_SIZE) != 0) {
		LM_ERR("Error allocating shared memory hashtable\n");
		return -1;
	}

	str_hash_init(ht);
	return 0;
}

static void __dialog_created_callback(struct dlg_cell *cell, int type, struct dlg_cb_params *params) {
	struct sip_msg *msg = NULL;

	msg = params->direction == SIP_REPLY ? params->rpl : params->req;

	if (msg == NULL) {
		LM_ERR("Error getting direction of SIP msg\n");
		return;
	}

	if (isflagset(msg, _data.ctrl_flag) == -1) {
		LM_DBG("Flag is not set for this message. Ignoring\n");
		return;
	}

	LM_DBG("Dialog created for CID [%.*s]\n", cell->callid.len, cell->callid.s);

	_dlgbinds.register_dlgcb(cell, DLGCB_CONFIRMED, __dialog_confirmed_callback, NULL, NULL);
	_dlgbinds.register_dlgcb(cell, DLGCB_TERMINATED|DLGCB_FAILED|DLGCB_EXPIRED, __dialog_terminated_callback, NULL, NULL);

	__setup_billing(&cell->callid, cell->h_entry, cell->h_id);
}

static void __dialog_confirmed_callback(struct dlg_cell *cell, int type, struct dlg_cb_params *params) {
	LM_DBG("Dialog confirmed for CID [%.*s]\n", cell->callid.len, cell->callid.s);

	__start_billing(&cell->callid, &cell->from_uri, &cell->to_uri, cell->tag);
}

static void __dialog_terminated_callback(struct dlg_cell *cell, int type, struct dlg_cb_params *params) {
	LM_DBG("Dialog terminated for CID [%.*s]\n", cell->callid.len, cell->callid.s);

	__stop_billing(&cell->callid);
}

static void __notify_call_termination(sip_data_t *data) {
	struct run_act_ctx ra_ctx;
	struct sip_msg *msg;

	if (_data.cs_route_number < 0)
		return;

	if (faked_msg_init_with_dlg_info(&data->callid, &data->from_uri, &data->from_tag,
					&data->to_uri, &data->to_tag, &msg) != 0) {
		LM_ERR("[%.*s]: error generating faked sip message\n", data->callid.len, data->callid.s);
		return;
	}

	init_run_actions_ctx(&ra_ctx);
	//run_top_route(event_rt.rlist[_data.cs_route_number], msg, &ra_ctx);

	if (run_actions(&ra_ctx, event_rt.rlist[_data.cs_route_number], msg) < 0)
		LM_ERR("Error executing cnxcc:call-shutdown route\n");
}

int try_get_credit_data_entry(str *client_id, credit_data_t **credit_data) {
	struct str_hash_entry *cd_entry	= NULL;
	hash_tables_t *hts = NULL;
	*credit_data = NULL;

	/* by money */
	hts = &_data.money;
	lock_get(&hts->lock);

	cd_entry = str_hash_get(hts->credit_data_by_client, client_id->s, client_id->len);

	if (cd_entry != NULL) {
		*credit_data	= cd_entry->u.p;
		lock_release(&hts->lock);
		return 0;
	}

	lock_release(&hts->lock);

	/* by time */
	hts = &_data.time;
	lock_get(&hts->lock);

	cd_entry = str_hash_get(hts->credit_data_by_client, client_id->s, client_id->len);

	if (cd_entry != NULL) {
		*credit_data	= cd_entry->u.p;
		lock_release(&hts->lock);
		return 0;
	}

	lock_release(&hts->lock);

	/* by channel */
	hts = &_data.channel;
	lock_get(&hts->lock);

	cd_entry = str_hash_get(hts->credit_data_by_client, client_id->s, client_id->len);

	if (cd_entry != NULL) {
		*credit_data	= cd_entry->u.p;
		lock_release(&hts->lock);
		return 0;
	}

	lock_release(&hts->lock);
	return -1;
}

int try_get_call_entry(str *callid, call_t **call, hash_tables_t **hts) {
	struct str_hash_entry *call_entry = NULL;

	*call = NULL;

	/* by money */
	*hts = &_data.money;
	lock_get(&(*hts)->lock);

	call_entry = str_hash_get((*hts)->call_data_by_cid, callid->s, callid->len);

	if (call_entry != NULL) {
		*call = call_entry->u.p;
		lock_release(&(*hts)->lock);
		return 0;
	}

	lock_release(&(*hts)->lock);

	/* by time */
	*hts = &_data.time;
	lock_get(&(*hts)->lock);

	call_entry = str_hash_get((*hts)->call_data_by_cid, callid->s, callid->len);

	if (call_entry != NULL) {
		*call = call_entry->u.p;
		lock_release(&(*hts)->lock);
		return 0;
	}

	lock_release(&(*hts)->lock);

	/* by channel */
	*hts = &_data.channel;
	lock_get(&(*hts)->lock);

	call_entry = str_hash_get((*hts)->call_data_by_cid, callid->s, callid->len);

	if (call_entry != NULL) {
		*call = call_entry->u.p;
		lock_release(&(*hts)->lock);
		return 0;
	}

	lock_release(&(*hts)->lock);
	return -1;
}

static void __stop_billing(str *callid) {
	struct str_hash_entry *cd_entry	= NULL;
	call_t *call			= NULL;
	hash_tables_t *hts		= NULL;
	credit_data_t *credit_data	= NULL;

	/*
	 * Search call data by call-id
	 */
	if (try_get_call_entry(callid, &call, &hts) != 0) {
		LM_ERR("Call [%.*s] not found\n", callid->len, callid->s);
		return;
	}

	if (call == NULL) {
		LM_ERR("[%.*s] call pointer is null\n", callid->len, callid->s);
		return;
	}

	if (hts == NULL) {
		LM_ERR("[%.*s] result hashtable pointer is null\n", callid->len, callid->s);
		return;
	}

	lock_get(&hts->lock);

	/*
	 * Search credit_data by client_id
	 */
	cd_entry = str_hash_get(hts->credit_data_by_client, call->client_id.s, call->client_id.len);

	if (cd_entry == NULL) {
		LM_ERR("Credit data not found for CID [%.*s], client-ID [%.*s]\n", callid->len, callid->s, call->client_id.len, call->client_id.s);
		lock_release(&hts->lock);
		return;
	}

	credit_data = (credit_data_t *) cd_entry->u.p;

	if (credit_data == NULL) {
		LM_ERR("[%.*s]: credit_data pointer is null\n", callid->len, callid->s);
		lock_release(&hts->lock);
		return;
	}

	lock_release(&hts->lock);

	/*
	 * Update calls statistics
	 */
	lock_get(&_data.lock);

	_data.stats->active--;
	_data.stats->total--;

	lock_release(&_data.lock);

	lock(&credit_data->lock);

	LM_DBG("Call [%.*s] of client-ID [%.*s], ended\n", callid->len, callid->s, call->client_id.len, call->client_id.s);

	/*
	 * This call just ended and we need to remove it from the summ.
	 */
	if (call->confirmed) {
		credit_data->concurrent_calls--;
		credit_data->ended_calls_consumed_amount += call->consumed_amount;

		if (_data.redis) {
			redis_incr_by_int(credit_data, "concurrent_calls", -1);
			redis_incr_by_double(credit_data, "ended_calls_consumed_amount", call->consumed_amount);
		}
	}

	credit_data->number_of_calls--;

	if (_data.redis)
		redis_incr_by_int(credit_data, "number_of_calls", -1);

	if (credit_data->concurrent_calls < 0) {
		LM_ERR("[BUG]: number of concurrent calls dropped to negative value: %d\n", credit_data->concurrent_calls);
	}

	if (credit_data->number_of_calls < 0) {
		LM_ERR("[BUG]: number of calls dropped to negative value: %d\n", credit_data->number_of_calls);
	}

	/*
	 * Remove (and free) the call from the list of calls of the current credit_data
	 */
	clist_rm(call, next, prev);
	__free_call(call);

	/*
	 * In case there are no active calls for a certain client, we remove the client-id from the hash table.
	 * This way, we can save memory for useful clients.
	 */
	if (credit_data->number_of_calls == 0) {
		LM_DBG("Removing client [%.*s] and its calls from the list\n", credit_data->call_list->client_id.len, credit_data->call_list->client_id.s);

		credit_data->deallocating = 1;
		lock(&hts->lock);

		if (_data.redis) {
			redis_clean_up_if_last(credit_data);
			shm_free(credit_data->str_id);
		}

		/*
		 * Remove the credit_data_t from the hash table
		 */
		str_hash_del(cd_entry);

		lock_release(&hts->lock);

		/*
		 * Free client_id in list's root
		 */
		shm_free(credit_data->call_list->client_id.s);
		shm_free(credit_data->call_list);

		/*
		 * Release the lock since we are going to free the entry down below
		 */
		lock_release(&credit_data->lock);

		/*
		 * Free the whole entry
		 */
		__free_credit_data_hash_entry(cd_entry);

		/*
		 * return without releasing the acquired lock over credit_data. Why? Because we just freed it.
		 */
		return;
	}

	lock_release(&credit_data->lock);
}

static void __setup_billing(str *callid, unsigned int h_entry, unsigned int h_id) {
	call_t *call		= NULL;
	hash_tables_t *hts	= NULL;

	LM_DBG("Creating dialog for [%.*s], h_id [%u], h_entry [%u]\n", callid->len, callid->s, h_id, h_entry);

//	lock_get(&_data.lock);

	/*
	 * Search call data by call-id
	 */
	if (try_get_call_entry(callid, &call, &hts) != 0) {
		LM_ERR("Call [%.*s] not found\n", callid->len, callid->s);
		return;
	}

	if (call == NULL) {
		LM_ERR("[%.*s] call pointer is null\n", callid->len, callid->s);
		return;
	}

	if (hts == NULL) {
		LM_ERR("[%.*s] result hashtable pointer is null\n", callid->len, callid->s);
		return;
	}

	/*
	 * Update calls statistics
	 */
	lock_get(&_data.lock);

	_data.stats->active++;
	_data.stats->total++;

	lock_release(&_data.lock);

	lock_get(&call->lock);

	call->dlg_h_entry	= h_entry;
	call->dlg_h_id		= h_id;

	LM_DBG("Call [%.*s] from client [%.*s], created\n", callid->len, callid->s, call->client_id.len, call->client_id.s);

	lock_release(&call->lock);
}

static void __start_billing(str *callid, str *from_uri, str *to_uri, str tags[2]) {
	struct str_hash_entry *cd_entry	= NULL;
	call_t *call			= NULL;
	hash_tables_t *hts		= NULL;
	credit_data_t *credit_data	= NULL;

	LM_DBG("Billing started for call [%.*s]\n", callid->len, callid->s);

//	lock_get(&_data.lock);

	/*
	 * Search call data by call-id
	 */
	if (try_get_call_entry(callid, &call, &hts) != 0) {
		LM_ERR("Call [%.*s] not found\n", callid->len, callid->s);
		return;
	}

	if (call == NULL) {
		LM_ERR("[%.*s] call pointer is null\n", callid->len, callid->s);
		return;
	}

	if (hts == NULL) {
		LM_ERR("[%.*s] result hashtable pointer is null", callid->len, callid->s);
		return;
	}

	lock_get(&hts->lock);

	/*
	 * Search credit_data by client_id
	 */
	cd_entry = str_hash_get(hts->credit_data_by_client, call->client_id.s, call->client_id.len);

	if (cd_entry == NULL) {
		LM_ERR("Credit data not found for CID [%.*s], client-ID [%.*s]\n", callid->len, callid->s, call->client_id.len, call->client_id.s);
		lock_release(&hts->lock);
		return;
	}

	credit_data = (credit_data_t *) cd_entry->u.p;

	if (credit_data == NULL) {
		LM_ERR("[%.*s]: credit_data pointer is null\n", callid->len, callid->s);
		lock_release(&hts->lock);
		return;
	}

	lock_release(&hts->lock);

	lock(&credit_data->lock);

	/*
	 * Now that the call is confirmed, we can increase the count of "concurrent_calls".
	 * This will impact in the discount rate performed by the check_calls() function.
	 *
	 */
	credit_data->concurrent_calls++;

	if (_data.redis)
		redis_incr_by_int(credit_data, "concurrent_calls", 1);

	if (credit_data->max_amount == 0) {
		credit_data->max_amount	= call->max_amount; // first time setup

		if (_data.redis)
			redis_insert_double_value(credit_data, "max_amount", credit_data->max_amount);
	}

	if (call->max_amount > credit_data->max_amount) {
		LM_ALERT("Maximum-talk-time/credit changed, maybe a credit reload? %f > %f. Client [%.*s]\n", call->max_amount, credit_data->max_amount,
																							call->client_id.len, call->client_id.s);


		if (_data.redis)
			redis_insert_double_value(credit_data, "max_amount", call->max_amount - credit_data->max_amount);

		credit_data->max_amount += call->max_amount - credit_data->max_amount;
	}

	/*
	 * Update max_amount, discounting what was already consumed by other calls of the same client
	 */
	call->max_amount = credit_data->max_amount - credit_data->consumed_amount;

	lock_release(&credit_data->lock);

	lock_get(&call->lock);

	/*
	 * Store from-tag value
	 */
	if (shm_str_dup(&call->sip_data.from_tag, &tags[0]) != 0) {
		LM_ERR("No more pkg memory\n");
		goto exit;
	}

	/*
	 * Store to-tag value
	 */
	if (shm_str_dup(&call->sip_data.to_tag, &tags[1]) != 0) {
		LM_ERR("No more pkg memory\n");
		goto exit;
	}

	if(shm_str_dup(&call->sip_data.from_uri, from_uri) != 0 ||
	   shm_str_dup(&call->sip_data.to_uri  , to_uri)   != 0) {
		LM_ERR("No more pkg memory\n");
		goto exit;
	}

	call->start_timestamp	= get_current_timestamp();
	call->confirmed		= TRUE;

	LM_DBG("Call [%.*s] from client [%.*s], confirmed. from=<%.*s>;tag=%.*s, to=<%.*s>;tag=%.*s\n",
			callid->len, callid->s, call->client_id.len, call->client_id.s,
			call->sip_data.from_uri.len, call->sip_data.from_uri.s,
			call->sip_data.from_tag.len, call->sip_data.from_tag.s,
			call->sip_data.to_uri.len, call->sip_data.to_uri.s,
			call->sip_data.to_tag.len, call->sip_data.to_tag.s);
exit:
	lock_release(&call->lock);
}

// must be called with lock held on credit_data
void terminate_all_calls(credit_data_t *credit_data) {
	call_t *call = NULL,
           *tmp = NULL;

	credit_data->deallocating = 1;

	clist_foreach_safe(credit_data->call_list, call, tmp, next) {
		LM_DBG("Killing call with CID [%.*s]\n", call->sip_data.callid.len, call->sip_data.callid.s);

		/*
		 * Update number of calls forced to end
		 */
		_data.stats->dropped++;
		terminate_call(call);
	}
}

/*
 * WARNING: When calling this function, the proper lock should have been acquired
 */
static void __free_call(call_t *call) {
	struct str_hash_entry *e = NULL;

	LM_DBG("Freeing call [%.*s]\n", call->sip_data.callid.len, call->sip_data.callid.s);
	e = str_hash_get(_data.money.call_data_by_cid, call->sip_data.callid.s, call->sip_data.callid.len);

	if (e == NULL) {
		e = str_hash_get(_data.time.call_data_by_cid, call->sip_data.callid.s, call->sip_data.callid.len);

		if (e == NULL) {
			e = str_hash_get(_data.channel.call_data_by_cid, call->sip_data.callid.s, call->sip_data.callid.len);

			if (e == NULL) {
				LM_ERR("Call [%.*s] not found. Couldn't be able to free it from hashtable", call->sip_data.callid.len, call->sip_data.callid.s);
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
static void __free_credit_data_hash_entry(struct str_hash_entry *e) {
	shm_free(e->key.s);
//	shm_free(((credit_data_t *) e->u.p)->call);
	shm_free(e->u.p);
	shm_free(e);
}

static int __shm_str_hash_alloc(struct str_hash_table *ht, int size) {
	ht->table	= shm_malloc(sizeof(struct str_hash_head) * size);

	if (!ht->table)
		return -1;

	ht->size	= size;
	return 0;
}

int terminate_call(call_t *call) {
	LM_DBG("Got kill signal for call [%.*s] client [%.*s] h_id [%u] h_entry [%u]. Dropping it now\n",
		call->sip_data.callid.len,
		call->sip_data.callid.s,
		call->client_id.len,
		call->client_id.s,
		call->dlg_h_id,
		call->dlg_h_entry);

	struct mi_root *root, *result	= NULL;
	struct mi_node *node, *node1	= NULL;
	struct mi_cmd *end_dlg_cmd		= NULL;

	root	= init_mi_tree(0, 0, 0);
	if (root == NULL) {
		LM_ERR("Error initializing tree to terminate call\n");
		goto error;
	}

	node	= &root->node;
	node1	= addf_mi_node_child(node, MI_DUP_VALUE, MI_SSTR("h_entry"), "%u", call->dlg_h_entry);
	if (node1 == NULL) {
		LM_ERR("Error initializing h_entry node to terminate call\n");
		goto error;
	}

	node1	= addf_mi_node_child(node, MI_DUP_VALUE, MI_SSTR("h_id"), "%u", call->dlg_h_id);
	if (node1 == NULL) {
		LM_ERR("Error initializing dlg_h_id node to terminate call\n");
		goto error;
	}

	end_dlg_cmd = lookup_mi_cmd(MI_SSTR("dlg_end_dlg"));
	if (node == NULL) {
		LM_ERR("Error initializing dlg_end_dlg command\n");
		goto error;
	}

	result	= run_mi_cmd(end_dlg_cmd, root);
	if (result == NULL) {
		LM_ERR("Error executing dlg_end_dlg command\n");
		goto error;
	}

	if (result->code == 200) {
		LM_DBG("dlg_end_dlg sent to call [%.*s]\n", call->sip_data.callid.len, call->sip_data.callid.s);
		free_mi_tree(root);
		free_mi_tree(result);

		__notify_call_termination(&call->sip_data);
		return 0;
	}

	LM_ERR("Error executing dlg_end_dlg command. Return code was [%d]\n", result->code);
error:
	if (root)
		free_mi_tree(root);

	return -1;
}

static credit_data_t *__get_or_create_credit_data_entry(str *client_id, credit_type_t type) {
	struct str_hash_table *ht = NULL;
	gen_lock_t *lock = NULL;
	struct str_hash_entry *e = NULL;
	credit_data_t *credit_data = NULL;

	switch(type) {
	case CREDIT_MONEY:
		ht = _data.money.credit_data_by_client;
		lock =  &_data.money.lock;
		break;
	case CREDIT_TIME:
		ht = _data.time.credit_data_by_client;
		lock =  &_data.time.lock;
		break;
	case CREDIT_CHANNEL:
		ht = _data.channel.credit_data_by_client;
		lock =  &_data.channel.lock;
		break;
	default:
		LM_ERR("BUG: Something went terribly wrong\n");
		return NULL;
	}

	lock_get(lock);
	e = str_hash_get(ht, client_id->s, client_id->len);
	lock_release(lock);

	/*
	 * Alloc new call_array_t if it doesn't exist
	 */
	if (e != NULL)
		LM_DBG("Found key %.*s in hash table\n", e->key.len, e->key.s);
	else if (e == NULL) {
		e = shm_malloc(sizeof(struct str_hash_entry));
		if (e == NULL)
			goto no_memory;

		if (shm_str_dup(&e->key, client_id) != 0)
			goto no_memory;

		e->u.p = credit_data = __alloc_new_credit_data(client_id, type);
		e->flags = 0;

		if (credit_data == NULL)
			goto no_memory;

		lock_get(lock);
		str_hash_add(ht, e);
		lock_release(lock);

		LM_DBG("Call didn't exist. Allocated new entry\n");
	}

	return (credit_data_t *) e->u.p;

no_memory:
	LM_ERR("No shared memory left\n");
	return NULL;
}

static credit_data_t *__alloc_new_credit_data(str *client_id, credit_type_t type) {
	credit_data_t *credit_data = shm_malloc(sizeof(credit_data_t));;

	lock_init(&credit_data->lock);

	credit_data->call_list = shm_malloc(sizeof(call_t));

	if (credit_data->call_list == NULL)
		goto no_memory;

	clist_init(credit_data->call_list, next, prev);

	/*
	 * Copy the client_id value to the root of the calls list.
	 * This will be used later to get the credit_data_t of the
	 * call when it is being searched by call ID.
	 */
	if (shm_str_dup(&credit_data->call_list->client_id, client_id) != 0)
		goto no_memory;

	if (_data.redis) {
		credit_data->str_id = shm_malloc(client_id->len + 1);

		if (!credit_data->str_id)
			goto no_memory;

		memset(credit_data->str_id, 0, client_id->len + 1);
		snprintf(credit_data->str_id, client_id->len + 1, "%.*s", client_id->len, client_id->s);
	}

	credit_data->max_amount	= 0;
	credit_data->concurrent_calls = 0;
	credit_data->consumed_amount = 0;
	credit_data->ended_calls_consumed_amount= 0;
	credit_data->number_of_calls = 0;
	credit_data->type = type;
	credit_data->deallocating = 0;

	if (!_data.redis)
		return credit_data;

	if (redis_get_or_create_credit_data(credit_data) < 0)
		goto error;

	return credit_data;

no_memory:
 	 LM_ERR("No shared memory left\n");
error:
 	 return NULL;
}

static call_t *__alloc_new_call_by_money(credit_data_t *credit_data, struct sip_msg *msg, 
					double credit, double cost_per_second, int initial_pulse, int final_pulse) {
	call_t *call = NULL;
	lock_get(&credit_data->lock);

	if (credit_data->call_list == NULL) {
		LM_ERR("Credit data call list is NULL\n");
		goto error;
	}

	call = shm_malloc(sizeof(call_t));
	if (call == NULL) {
		LM_ERR("No shared memory left\n");
		goto error;
	}

	if ((!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) != 0) ||
		shm_str_dup(&call->sip_data.callid, &msg->callid->body) != 0 ) {
		LM_ERR("Error processing CALLID hdr\n");
		goto error;
	}

	call->sip_data.to_uri.s		= NULL;
	call->sip_data.to_uri.len 	= 0;
	call->sip_data.to_tag.s		= NULL;
	call->sip_data.to_tag.len 	= 0;

	call->sip_data.from_uri.s	= NULL;
	call->sip_data.from_uri.len	= 0;
	call->sip_data.from_tag.s	= NULL;
	call->sip_data.from_tag.len	= 0;

	call->consumed_amount = initial_pulse * cost_per_second;
	call->confirmed	= FALSE;
	call->max_amount = credit;

	call->money_based.cost_per_second	= cost_per_second;
	call->money_based.initial_pulse		= initial_pulse;
	call->money_based.final_pulse		= final_pulse;

	/*
	 * Reference the client_id from the root of the list
	 */
	call->client_id.s		= credit_data->call_list->client_id.s;
	call->client_id.len		= credit_data->call_list->client_id.len;

	/*
	 * Insert the newly created call to the list of calls
	 */
	clist_insert(credit_data->call_list, call, next, prev);

	lock_init(&call->lock);

	/*
	 * Increase the number of calls for this client. This call is not yet confirmed.
	 */
	credit_data->number_of_calls++;
	if (_data.redis)
		redis_incr_by_int(credit_data, "number_of_calls", 1);

	lock_release(&credit_data->lock);

	LM_DBG("New call allocated for client [%.*s]\n", call->client_id.len, call->client_id.s);

	return call;

error:
	lock_release(&credit_data->lock);
	return NULL;
}

static call_t *__alloc_new_call_by_time(credit_data_t *credit_data, struct sip_msg *msg, int max_secs) {
	call_t *call = NULL;

	lock_get(&credit_data->lock);

	if (credit_data->call_list == NULL) {
		LM_ERR("Credit data call list is NULL\n");
		goto error;
	}

	call = shm_malloc(sizeof(call_t));
	if (call == NULL) {
		LM_ERR("No shared memory left\n");
		goto error;
	}

	if ( (!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) != 0) ||
		   shm_str_dup(&call->sip_data.callid, &msg->callid->body) != 0 ) {
		LM_ERR("Error processing CALLID hdr\n");
		goto error;
	}
	
        call->sip_data.to_uri.s         = NULL;
        call->sip_data.to_uri.len       = 0;
        call->sip_data.to_tag.s         = NULL;
        call->sip_data.to_tag.len       = 0;

        call->sip_data.from_uri.s       = NULL;
        call->sip_data.from_uri.len     = 0;
        call->sip_data.from_tag.s       = NULL;
        call->sip_data.from_tag.len     = 0;

	call->consumed_amount	= 0;
	call->confirmed		= FALSE;
	call->max_amount	= max_secs;

	/*
	 * Reference the client_id from the root of the list
	 */
	call->client_id.s	= credit_data->call_list->client_id.s;
	call->client_id.len	= credit_data->call_list->client_id.len;

	/*
	 * Insert the newly created call to the list of calls
	 */
	clist_insert(credit_data->call_list, call, next, prev);

	lock_init(&call->lock);

	/*
	 * Increase the number of calls for this client. This call is not yet confirmed.
	 */
	credit_data->number_of_calls++;
	if (_data.redis)
		redis_incr_by_int(credit_data, "number_of_calls", 1);

	lock_release(&credit_data->lock);

	LM_DBG("New call allocated for client [%.*s]\n", call->client_id.len, call->client_id.s);

	return call;

error:
	lock_release(&credit_data->lock);
	return NULL;
}

static call_t *alloc_new_call_by_channel(credit_data_t *credit_data, struct sip_msg *msg, int max_chan) {
	call_t *call = NULL;

	lock_get(&credit_data->lock);

	if (credit_data->call_list == NULL) {
		LM_ERR("Credit data call list is NULL\n");
		goto error;
	}

	call = shm_malloc(sizeof(call_t));
	if (call == NULL) {
		LM_ERR("No shared memory left\n");
		goto error;
	}

	if ( (!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) != 0) ||
		shm_str_dup(&call->sip_data.callid, &msg->callid->body) != 0 ) {
		LM_ERR("Error processing CALLID hdr\n");
		goto error;
	}

        call->sip_data.to_uri.s         = NULL;
        call->sip_data.to_uri.len       = 0;
        call->sip_data.to_tag.s         = NULL;
        call->sip_data.to_tag.len       = 0;

        call->sip_data.from_uri.s       = NULL;
        call->sip_data.from_uri.len     = 0;
        call->sip_data.from_tag.s       = NULL;
        call->sip_data.from_tag.len     = 0;

	call->consumed_amount		= 0;
	call->confirmed			= FALSE;
	call->max_amount		= max_chan;

	/*
	 * Reference the client_id from the root of the list
	 */
	call->client_id.s		= credit_data->call_list->client_id.s;
	call->client_id.len		= credit_data->call_list->client_id.len;

	/*
	 * Insert the newly created call to the list of calls
	 */
	clist_insert(credit_data->call_list, call, next, prev);

	lock_init(&call->lock);

	/*
	 * Increase the number of calls for this client. This call is not yet confirmed.
	 */
	credit_data->number_of_calls++;
	if (_data.redis)
		redis_incr_by_int(credit_data, "number_of_calls", 1);

	lock_release(&credit_data->lock);

	LM_DBG("New call allocated for client [%.*s]\n", call->client_id.len, call->client_id.s);


	return call;

error:
	lock_release(&credit_data->lock);
	return NULL;
}

static int __add_call_by_cid(str *cid, call_t *call, credit_type_t type) {
	struct str_hash_table *ht	= NULL;
	gen_lock_t *lock		= NULL;
	struct str_hash_entry *e	= NULL;

	switch(type) {
	case CREDIT_MONEY:
		ht	= _data.money.call_data_by_cid;
		lock	=  &_data.money.lock;
		break;
	case CREDIT_TIME:
		ht	= _data.time.call_data_by_cid;
		lock	=  &_data.time.lock;
		break;
	case CREDIT_CHANNEL:
		ht	= _data.channel.call_data_by_cid;
		lock	=  &_data.channel.lock;
		break;
	default:
		LM_ERR("Something went terribly wrong\n");
		return -1;
	}

	e = str_hash_get(ht, cid->s, cid->len);

	if (e != NULL) {
		LM_DBG("e != NULL\n");

		call_t *value	= (call_t *) e->u.p;
		if (value == NULL) {
			LM_ERR("Value of CID [%.*s] is NULL\n", cid->len, cid->s);
			return -1;
		}

		LM_WARN("value cid: len=%d | value [%.*s]", value->sip_data.callid.len, value->sip_data.callid.len, value->sip_data.callid.s);
		LM_WARN("added cid: len=%d | value [%.*s]", cid->len, cid->len, cid->s);

		if (value->sip_data.callid.len != cid->len ||
			strncasecmp(value->sip_data.callid.s, cid->s, cid->len) != 0) {
			LM_ERR("Value of CID is [%.*s] and differs from value being added [%.*s]\n", cid->len, cid->s,
													value->sip_data.callid.len, value->sip_data.callid.s);
			return -1;
		}

		LM_DBG("CID already present\n");
		return 0;
	}

	e = shm_malloc(sizeof(struct str_hash_entry));

	if (e == NULL) {
		LM_ERR("No shared memory left\n");
		return -1;
	}

	if (shm_str_dup(&e->key, cid) != 0) {
		LM_ERR("No shared memory left\n");
		return -1;
	}

	e->u.p	= call;

	lock_get(lock);
	str_hash_add(ht, e);
	lock_release(lock);

	return 0;
}

static inline void set_ctrl_flag(struct sip_msg* msg) {
	if (_data.ctrl_flag != -1) {
		LM_DBG("Flag set!\n");
		setflag(msg, _data.ctrl_flag);
	}
}

static inline int get_pv_value(struct sip_msg* msg, pv_spec_t* spec, pv_value_t* value) {
	if (pv_get_spec_value(msg, spec, value) != 0) {
		LM_ERR("Can't get PV's value\n");
		return -1;
	}

	return 0;
}

static int __set_max_credit(struct sip_msg* msg,
				char *str_pv_client,
				char *str_pv_credit, char *str_pv_cps,
				char *str_pv_inip, char *str_pv_finp) {
	credit_data_t *credit_data 	= NULL;
	call_t *call			= NULL;

	pv_spec_t *client_id_spec = (pv_spec_t *) str_pv_client,
		  *credit_spec	  = (pv_spec_t *) str_pv_credit,
		  *cps_spec	  = (pv_spec_t *) str_pv_cps,
		  *initial_pulse_spec	= (pv_spec_t *) str_pv_inip,
		  *final_pulse_spec	= (pv_spec_t *) str_pv_finp;

	pv_value_t client_id_val,
			credit_val,
			cps_val,
			initial_pulse_val,
			final_pulse_val;

	double credit		= 0,
	       cost_per_second	= 0;

	unsigned int initial_pulse	= 0,
			final_pulse	= 0;

	if (msg->first_line.type == SIP_REQUEST && msg->first_line.u.request.method_value == METHOD_INVITE) {
		if (__has_to_tag(msg)) {
			LM_ERR("INVITE is a reINVITE\n");
			return -1;
		}

		if (pv_get_spec_value(msg, client_id_spec, &client_id_val) != 0) {
			LM_ERR("Can't get client_id's value\n");
			return -1;
		}

		if (pv_get_spec_value(msg, credit_spec, &credit_val) != 0) {
			LM_ERR("Can't get credit's value\n");
			return -1;
		}

		credit	= str2double(&credit_val.rs);

		if (credit <= 0) {
			LM_ERR("credit value must be > 0: %f", credit);
			return -1;
		}

		if (pv_get_spec_value(msg, cps_spec, &cps_val) != 0) {
			LM_ERR("Can't get cost_per_sec's value\n");
			return -1;
		}

		cost_per_second	= str2double(&cps_val.rs);

		if (cost_per_second <= 0) {
			LM_ERR("cost_per_second value must be > 0: %f\n", cost_per_second);
			return -1;
		}

		if (pv_get_spec_value(msg, initial_pulse_spec, &initial_pulse_val) != 0) {
			LM_ERR("Can't get initial_pulse's value\n");
			return -1;
		}

		if (str2int(&initial_pulse_val.rs, &initial_pulse) != 0) {
			LM_ERR("initial_pulse value is invalid: %.*s", initial_pulse_val.rs.len, initial_pulse_val.rs.s);
			return -1;
		}

		if (pv_get_spec_value(msg, final_pulse_spec, &final_pulse_val) != 0) {
			LM_ERR("Can't get final_pulse's value\n");
			return -1;
		}

		if (str2int(&final_pulse_val.rs, &final_pulse) != 0) {
			LM_ERR("final_pulse value is invalid: %.*s", final_pulse_val.rs.len, final_pulse_val.rs.s);
			return -1;
		}

		if (client_id_val.rs.len == 0 || client_id_val.rs.s == NULL) {
			LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len, msg->callid->body.s);
			return -1;
		}

		LM_DBG("Setting up new call for client [%.*s], max-credit[%f], "
				"cost-per-sec[%f], initial-pulse [%d], "
				"final-pulse [%d], call-id[%.*s]\n", client_id_val.rs.len, client_id_val.rs.s,
									 credit,
									 cost_per_second, initial_pulse,
									 final_pulse, msg->callid->body.len, msg->callid->body.s);
		set_ctrl_flag(msg);

		if ((credit_data = __get_or_create_credit_data_entry(&client_id_val.rs, CREDIT_MONEY)) == NULL) {
			LM_ERR("Error retrieving credit data from shared memory for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}

		if ((call = __alloc_new_call_by_money(credit_data, msg, credit, cost_per_second, initial_pulse, final_pulse)) == NULL) {
			LM_ERR("Unable to allocate new call for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}

		if (__add_call_by_cid(&call->sip_data.callid, call, CREDIT_MONEY) != 0) {
			LM_ERR("Unable to allocate new cid_by_client for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}
	}
	else {
		LM_ALERT("MSG was not an INVITE\n");
		return -1;
	}

	return 1;
}

static int __terminate_all(struct sip_msg* msg, char* str_pv_client) {
	credit_data_t *credit_data 	= NULL;
	pv_spec_t *client_id_spec	= (pv_spec_t *) str_pv_client;

	pv_value_t client_id_val;

	if (pv_get_spec_value(msg, client_id_spec, &client_id_val) != 0) {
		LM_ERR("[%.*s]: can't get client_id pvar value\n", msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	if (client_id_val.rs.len == 0 || client_id_val.rs.s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	if (try_get_credit_data_entry(&client_id_val.rs, &credit_data) != 0) {
		LM_DBG("[%.*s] not found\n", msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	terminate_all_calls(credit_data);
	return 1;
}

static int __get_channel_count(struct sip_msg* msg, char* str_pv_client, char* str_pv_chan_count) {
	credit_data_t *credit_data 	= NULL;
	pv_spec_t *chan_count_spec	= (pv_spec_t *) str_pv_chan_count,
		  *client_id_spec	= (pv_spec_t *) str_pv_client;

	pv_value_t chan_count_val, client_id_val;
	int value			= -1;

	if (pv_get_spec_value(msg, client_id_spec, &client_id_val) != 0) {
		LM_ERR("[%.*s]: can't get client_id pvar value\n", msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	if (client_id_val.rs.len == 0 || client_id_val.rs.s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	if (try_get_credit_data_entry(&client_id_val.rs, &credit_data) == 0)
		value	= credit_data->number_of_calls;
	else
		LM_ALERT("[%.*s] not found\n", msg->callid->body.len, msg->callid->body.s);

	if (!pv_is_w(chan_count_spec)) {
		LM_ERR("pvar is not writable\n");
		return -1;
	}

	memset(&chan_count_val, 0, sizeof(chan_count_val));

	chan_count_val.flags = PV_VAL_STR;

	if (value > 0)
		chan_count_val.rs.s = int2str(value, &chan_count_val.rs.len);
	else {
		char buff[2]		= { '-', '1' };
		chan_count_val.rs.s 	= buff;
		chan_count_val.rs.len	= 2;
	}

	if (pv_set_spec_value(msg, chan_count_spec, 0, &chan_count_val) != 0) {
		LM_ERR("Error writing value to pvar");
		return -1;
	}

	return 1;
}

static int __set_max_channels(struct sip_msg* msg, char* str_pv_client, char* str_pv_max_chan) {
	credit_data_t *credit_data 	= NULL;
	call_t *call			= NULL;
	pv_spec_t *max_chan_spec	= (pv_spec_t *) str_pv_max_chan,
		  *client_id_spec	= (pv_spec_t *) str_pv_client;
	pv_value_t max_chan_val, client_id_val;
	int max_chan			= 0;

	set_ctrl_flag(msg);

	if (parse_headers(msg, HDR_CALLID_F, 0) != 0) {
		LM_ERR("Error parsing Call-ID");
		return -1;
	}

	if (msg->first_line.type == SIP_REQUEST && msg->first_line.u.request.method_value == METHOD_INVITE) {
		if (__has_to_tag(msg))
		{
			LM_ERR("INVITE is a reINVITE\n");
			return -1;
		}

		if (pv_get_spec_value(msg, max_chan_spec, &max_chan_val) != 0) {
			LM_ERR("Can't get max_chan pvar value\n");
			return -1;
		}
		max_chan = max_chan_val.ri;

		if (max_chan <= 0) {
			LM_ERR("[%.*s] MAX_CHAN cannot be less than or equal to zero: %d\n", msg->callid->body.len, msg->callid->body.s, max_chan);
			return -1;
		}

		if (pv_get_spec_value(msg, client_id_spec, &client_id_val) != 0) {
			LM_ERR("[%.*s]: can't get client_id pvar value\n", msg->callid->body.len, msg->callid->body.s);
			return -1;
		}

		if (client_id_val.rs.len == 0 || client_id_val.rs.s == NULL) {
			LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len, msg->callid->body.s);
			return -1;
		}

		LM_DBG("Setting up new call for client [%.*s], max-chan[%d], call-id[%.*s]\n", client_id_val.rs.len, client_id_val.rs.s,
												max_chan,
												msg->callid->body.len, msg->callid->body.s);

		if ((credit_data = __get_or_create_credit_data_entry(&client_id_val.rs, CREDIT_CHANNEL)) == NULL) {
			LM_ERR("Error retrieving credit data from shared memory for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}

		if (credit_data->number_of_calls + 1 > max_chan)
			return -2; // you have, between calls being setup plus those established, more than you maximum quota

		if (credit_data->concurrent_calls + 1 > max_chan)
			return -3; // you have the max amount of established calls already

		if ((call = alloc_new_call_by_channel(credit_data, msg, max_chan)) == NULL) {
			LM_ERR("Unable to allocate new call for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}

		if (__add_call_by_cid(&call->sip_data.callid, call, CREDIT_CHANNEL) != 0) {
			LM_ERR("Unable to allocate new cid_by_client for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}

		return 1;
	}
	else {
		LM_ALERT("MSG was not an INVITE\n");
		return -1;
	}
}

static int __set_max_time(struct sip_msg* msg, char* str_pv_client, char* str_pv_maxsecs) {
	credit_data_t *credit_data 	= NULL;
	call_t *call			= NULL;
	pv_spec_t *max_secs_spec	= (pv_spec_t *) str_pv_maxsecs,
		  *client_id_spec	= (pv_spec_t *) str_pv_client;
	pv_value_t max_secs_val, client_id_val;
	int max_secs			= 0;

	set_ctrl_flag(msg);

	if (parse_headers(msg, HDR_CALLID_F, 0) != 0) {
		LM_ERR("Error parsing Call-ID");
		return -1;
	}

	if (msg->first_line.type == SIP_REQUEST && msg->first_line.u.request.method_value == METHOD_INVITE) {
		if (__has_to_tag(msg)) {
			LM_ERR("INVITE is a reINVITE\n");
			return -1;
		}

		if (pv_get_spec_value(msg, max_secs_spec, &max_secs_val) != 0) {
			LM_ERR("Can't get max_secs PV value\n");
			return -1;
		}
		max_secs = max_secs_val.ri;

		if (max_secs <= 0) {
			LM_ERR("[%.*s] MAXSECS cannot be less than or equal to zero: %d\n", msg->callid->body.len, msg->callid->body.s, max_secs);
			return -1;
		}

		if (pv_get_spec_value(msg, client_id_spec, &client_id_val) != 0) {
			LM_ERR("[%.*s]: can't get client_id PV value\n", msg->callid->body.len, msg->callid->body.s);
			return -1;
		}

		if (client_id_val.rs.len == 0 || client_id_val.rs.s == NULL) {
			LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len, msg->callid->body.s);
			return -1;
		}

		LM_DBG("Setting up new call for client [%.*s], max-secs[%d], call-id[%.*s]\n", client_id_val.rs.len, client_id_val.rs.s,
												max_secs,
												msg->callid->body.len, msg->callid->body.s);

		if ((credit_data = __get_or_create_credit_data_entry(&client_id_val.rs, CREDIT_TIME)) == NULL) {
			LM_ERR("Error retrieving credit data from shared memory for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}

		if ((call = __alloc_new_call_by_time(credit_data, msg, max_secs)) == NULL) {
			LM_ERR("Unable to allocate new call for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}

		if (__add_call_by_cid(&call->sip_data.callid, call, CREDIT_TIME) != 0) {
			LM_ERR("Unable to allocate new cid_by_client for client [%.*s]\n", client_id_val.rs.len, client_id_val.rs.s);
			return -1;
		}
	}
	else {
		LM_ALERT("MSG was not an INVITE\n");
		return -1;
	}

	return 1;
}

static int __update_max_time(struct sip_msg* msg, char* str_pv_client, char* str_pv_secs) {
	credit_data_t *credit_data 	= NULL;
	pv_spec_t *secs_spec		= (pv_spec_t *) str_pv_secs,
		  *client_id_spec	= (pv_spec_t *) str_pv_client;
	pv_value_t secs_val, client_id_val;
	int secs			= 0;

	set_ctrl_flag(msg);

	if (parse_headers(msg, HDR_CALLID_F, 0) != 0) {
		LM_ERR("Error parsing Call-ID");
		return -1;
	}

	if (pv_get_spec_value(msg, secs_spec, &secs_val) != 0) {
		LM_ERR("Can't get secs PV value\n");
		return -1;
	}
	secs	= secs_val.ri;

	if (secs <= 0) {
		LM_ERR("[%.*s] MAXSECS cannot be less than or equal to zero: %d\n", msg->callid->body.len, msg->callid->body.s, secs);
		return -1;
	}

	if (pv_get_spec_value(msg, client_id_spec, &client_id_val) != 0) {
		LM_ERR("[%.*s]: can't get client_id PV value\n", msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	if (client_id_val.rs.len == 0 || client_id_val.rs.s == NULL) {
		LM_ERR("[%.*s]: client ID cannot be null\n", msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	LM_DBG("Updating call for client [%.*s], max-secs[%d], call-id[%.*s]\n", client_id_val.rs.len, client_id_val.rs.s,
										secs,
										msg->callid->body.len, msg->callid->body.s);



	struct str_hash_table *ht	= NULL;
	struct str_hash_entry *e	= NULL;
	ht				= _data.time.credit_data_by_client;
	double update_fraction		= secs;
	call_t *call			= NULL,
	       *tmp_call		= NULL;

	lock_get(&_data.time.lock);
	e = str_hash_get(ht, client_id_val.rs.s, client_id_val.rs.len);
	lock_release(&_data.time.lock);

	if (e == NULL) {
		LM_ERR("Client [%.*s] was not found\n", client_id_val.rs.len, client_id_val.rs.s);
		return -1;
	}
		
	credit_data = (credit_data_t *) e->u.p;
	lock_get(&credit_data->lock);

	LM_DBG("Updating max-secs for [%.*s] from [%f] to [%f]\n", e->key.len, e->key.s, credit_data->max_amount, credit_data->max_amount + secs);
	
	credit_data->max_amount				+= secs;

	if (credit_data->number_of_calls > 0)
		update_fraction	= secs / credit_data->number_of_calls;

	clist_foreach_safe(credit_data->call_list, call, tmp_call, next) {
		if (!call->confirmed)
			continue;
		
		call->max_amount	+= update_fraction;
	}

//redit_data->consumed_amount			= 0;


	lock_release(&credit_data->lock);

	return 1;
}

static int __has_to_tag(struct sip_msg *msg) {
	if (msg->to == NULL && parse_headers(msg, HDR_TO_F, 0) != 0) {
		LM_ERR("Cannot parse to-tag\n");
		return 0;
	}

	return !(get_to(msg)->tag_value.s == NULL || get_to(msg)->tag_value.len == 0);
}

static int __pv_parse_calls_param(pv_spec_p sp, str *in) {
	if (sp == NULL || in == NULL || in->len == 0)
		return -1;

	switch(in->len) {
	case 5:
		if (strncmp("total", in->s, in->len) == 0)
			sp->pvp.pvn.u.isname.name.n	= CNX_PV_TOTAL;
		else
			return -1;
		break;
	case 6:
		if (strncmp("active", in->s, in->len) == 0)
			sp->pvp.pvn.u.isname.name.n	= CNX_PV_ACTIVE;
		else
			return -1;
		break;
	case 7:
		if (strncmp("dropped", in->s, in->len) == 0)
			sp->pvp.pvn.u.isname.name.n	= CNX_PV_DROPPED;
		else
			return -1;
		break;

	}

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;
}

static int __pv_get_calls(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	switch(param->pvn.u.isname.name.n) {
	case CNX_PV_ACTIVE:
		return pv_get_uintval(msg, param, res, _data.stats->active);
	case CNX_PV_TOTAL:
		return pv_get_uintval(msg, param, res, _data.stats->total);
	case CNX_PV_DROPPED:
		return pv_get_uintval(msg, param, res, _data.stats->dropped);
	default:
		LM_ERR("Unknown PV type %d\n", param->pvn.u.isname.name.n);
		break;
	}

	return -1;
}

static struct mi_root *__mi_credit_control_stats(struct mi_root *tree, void *param) {
	char *p;
	int len;
	struct mi_root *rpl_tree;
	struct mi_node *node, *node1;

	rpl_tree = init_mi_tree(200, "OK", 2);
	node	 = &rpl_tree->node;

	node1 = add_mi_node_child(node, 0, MI_SSTR("CNX Credit Control"), 0, 0);
	if (node1 == NULL) {
		LM_ERR("Error creating child node\n");
		goto error;
	}

	p = int2str((unsigned long) _data.stats->active, &len);
	if (p == NULL) {
		LM_ERR("Error converting INT to STR\n");
		goto error;
	}
	add_mi_node_child(node1, MI_DUP_VALUE, MI_SSTR("active"), p, len);

	p = int2str((unsigned long) _data.stats->dropped, &len);
	if (p == NULL) {
		LM_ERR("Error converting INT to STR\n");
		goto error;
	}

	add_mi_node_child(node1, MI_DUP_VALUE, MI_SSTR("dropped"), p, len);

	p = int2str((unsigned long) _data.stats->total, &len);
	if (p == NULL) {
		LM_ERR("Error converting INT to STR\n");
		goto error;
	}

	add_mi_node_child(node1, MI_DUP_VALUE, MI_SSTR("total"), p, len);

	return rpl_tree;

error:
	return init_mi_tree(500, MI_INTERNAL_ERR, MI_INTERNAL_ERR_LEN);
}
