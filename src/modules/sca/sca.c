/*
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for Kamailio, a free SIP server.
 *
 * The sca module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The sca module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA. 02110-1301 USA
 */
/*!
 * \file
 * \brief SCA shared call appearance module
 * \ingroup sca
 * - Module: \ref sca
 * \author Andrew Mortensen
 */

/*!
 * \defgroup sca :: The Kamailio shared call appearance Module
 *
 * The sca module implements Shared Call Appearances. It handles SUBSCRIBE messages for call-info 
 * and line-seize events, and sends call-info NOTIFYs to line subscribers to implement line bridging.
 * The module implements SCA as defined in Broadworks SIP Access Side Extensions Interface 
 * Specifications, Release 13.0, version 1, sections 2, 3 and 4.
 */
#include "sca_common.h"

#include <sys/types.h>
#include <stdlib.h>

#include "../../core/timer.h"
#include "../../core/timer_proc.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "sca.h"
#include "sca_appearance.h"
#include "sca_db.h"
#include "sca_call_info.h"
#include "sca_rpc.h"
#include "sca_subscribe.h"

MODULE_VERSION

/*
 *  MODULE OBJECT
 */
sca_mod *sca = NULL;

/*
 * EXTERNAL API
 */
db_func_t dbf;		 // db api
struct tm_binds tmb; // tm functions for sending messages
sl_api_t slb;		 // sl callback, function for getting to-tag

/*
 * PROTOTYPES
 */
static int sca_mod_init(void);
static int sca_child_init(int);
static void sca_mod_destroy(void);
static int sca_set_config(sca_mod *);
static int sca_call_info_update_0_f(sip_msg_t *msg, char *, char *);
static int sca_call_info_update_1_f(sip_msg_t *msg, char *, char *);
static int sca_call_info_update_2_f(sip_msg_t *msg, char *, char *);
static int sca_call_info_update_3_f(sip_msg_t *msg, char *, char *, char *);
int fixup_ciu(void **, int);
int fixup_free_ciu(void **param, int param_no);

/*
 * EXPORTED COMMANDS
 */
static cmd_export_t cmds[] = {
		{"sca_handle_subscribe", (cmd_function)sca_handle_subscribe, 0, NULL, 0,
				REQUEST_ROUTE},
		{"sca_call_info_update", (cmd_function)sca_call_info_update_0_f, 0,
				NULL, 0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
		{"sca_call_info_update", (cmd_function)sca_call_info_update_1_f, 1,
				fixup_ciu, fixup_free_ciu,
				REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
		{"sca_call_info_update", (cmd_function)sca_call_info_update_2_f, 2,
				fixup_ciu, fixup_free_ciu,
				REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
		{"sca_call_info_update", (cmd_function)sca_call_info_update_3_f, 3,
				fixup_ciu, fixup_free_ciu,
				REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

/*
 * EXPORTED RPC INTERFACE
 */
static rpc_export_t sca_rpc[] = {
		{"sca.all_subscriptions", sca_rpc_show_all_subscriptions,
				sca_rpc_show_all_subscriptions_doc, 0},
		{"sca.subscription_count", sca_rpc_subscription_count,
				sca_rpc_subscription_count_doc, 0},
		{"sca.show_subscription", sca_rpc_show_subscription,
				sca_rpc_show_subscription_doc, 0},
		{"sca.subscribers", sca_rpc_show_subscribers,
				sca_rpc_show_subscribers_doc, 0},
		{"sca.deactivate_all_subscriptions",
				sca_rpc_deactivate_all_subscriptions,
				sca_rpc_deactivate_all_subscriptions_doc, 0},
		{"sca.deactivate_subscription", sca_rpc_deactivate_subscription,
				sca_rpc_deactivate_subscription_doc, 0},
		{"sca.all_appearances", sca_rpc_show_all_appearances,
				sca_rpc_show_all_appearances_doc, 0},
		{"sca.show_appearance", sca_rpc_show_appearance,
				sca_rpc_show_appearance_doc, 0},
		{"sca.seize_appearance", sca_rpc_seize_appearance,
				sca_rpc_seize_appearance_doc, 0},
		{"sca.update_appearance", sca_rpc_update_appearance,
				sca_rpc_update_appearance_doc, 0},
		{"sca.release_appearance", sca_rpc_release_appearance,
				sca_rpc_release_appearance_doc, 0},
		{NULL, NULL, NULL, 0},
};

/*
 * EXPORTED PARAMETERS
 */
str outbound_proxy = STR_NULL;
str db_url = STR_STATIC_INIT(DEFAULT_DB_URL);
str db_subs_table = STR_STATIC_INIT("sca_subscriptions");
str db_state_table = STR_STATIC_INIT("sca_state");
int db_update_interval = 300;
int hash_table_size = -1;
int call_info_max_expires = 3600;
int line_seize_max_expires = 15;
int purge_expired_interval = 120;
int onhold_bflag = -1;
str server_address = STR_NULL;

static param_export_t params[] = {
		{"outbound_proxy", PARAM_STR, &outbound_proxy},
		{"db_url", PARAM_STR, &db_url},
		{"subs_table", PARAM_STR, &db_subs_table},
		{"state_table", PARAM_STR, &db_state_table},
		{"db_update_interval", INT_PARAM, &db_update_interval},
		{"hash_table_size", INT_PARAM, &hash_table_size},
		{"call_info_max_expires", INT_PARAM, &call_info_max_expires},
		{"line_seize_max_expires", INT_PARAM, &line_seize_max_expires},
		{"purge_expired_interval", INT_PARAM, &purge_expired_interval},
		{"onhold_bflag", INT_PARAM, &onhold_bflag},
		{"server_address", PARAM_STR, &server_address},
		{NULL, 0, NULL},
};

/*
 * MODULE EXPORTS
 */
/* clang-format off */
struct module_exports exports= {
	"sca",           /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd exports */
	params,          /* param exports */
	0,               /* exported RPC functions */
	0,               /* exported pseudo-variables */
	0,               /* response handling function */
	sca_mod_init,    /* module init function */
	sca_child_init,  /* per-child init function */
	sca_mod_destroy  /* module destroy function */
};
/* clang-format on */

static int sca_bind_sl(sca_mod *scam, sl_api_t *sl_api)
{
	sl_cbelem_t sl_cbe;

	assert(scam != NULL);
	assert(sl_api != NULL);

	if(sl_load_api(sl_api) != 0) {
		LM_ERR("Failed to initialize required sl API. Check if the \"sl\" "
			   "module is loaded.\n");
		return (-1);
	}
	scam->sl_api = sl_api;

	sl_cbe.type = SLCB_REPLY_READY;
	sl_cbe.cbf = (sl_cbf_f)sca_call_info_sl_reply_cb;

	if(scam->sl_api->register_cb(&sl_cbe) < 0) {
		LM_ERR("Failed to register sl reply callback\n");
		return (-1);
	}

	return (0);
}

static int sca_bind_srdb1(sca_mod *scam, db_func_t *db_api)
{
	db1_con_t *db_con = NULL;
	int rc = -1;

	if(db_bind_mod(scam->cfg->db_url, db_api) != 0) {
		LM_ERR("Failed to initialize required DB API - %.*s\n",
				STR_FMT(scam->cfg->db_url));
		goto done;
	}
	scam->db_api = db_api;

	if(!DB_CAPABILITY((*db_api), DB_CAP_ALL)) {
		LM_ERR("Selected database %.*s lacks required capabilities\n",
				STR_FMT(scam->cfg->db_url));
		goto done;
	}

	// ensure database exists and table schemas are correct
	db_con = db_api->init(scam->cfg->db_url);
	if(db_con == NULL) {
		LM_ERR("sca_bind_srdb1: failed to connect to DB %.*s\n",
				STR_FMT(scam->cfg->db_url));
		goto done;
	}

	if(db_check_table_version(db_api, db_con, scam->cfg->subs_table,
			   SCA_DB_SUBSCRIPTIONS_TABLE_VERSION)
			< 0) {
		str tmp = *scam->cfg->subs_table;
		DB_TABLE_VERSION_ERROR(tmp);
		goto done;
	}

	// DB and tables are OK, close DB handle. reopen in each child.
	rc = 0;

done:
	if(db_con != NULL) {
		db_api->close(db_con);
		db_con = NULL;
	}

	return (rc);
}

static int sca_set_config(sca_mod *scam)
{
	scam->cfg = (sca_config *)shm_malloc(sizeof(sca_config));
	if(scam->cfg == NULL) {
		LM_ERR("Failed to shm_malloc module configuration\n");
		return (-1);
	}
	memset(scam->cfg, 0, sizeof(sca_config));

	if(outbound_proxy.s) {
		scam->cfg->outbound_proxy = &outbound_proxy;
	}

	if(!db_url.s || db_url.len <= 0) {
		LM_ERR("sca_set_config: db_url must be set!\n");
		return (-1);
	}
	scam->cfg->db_url = &db_url;

	if(!db_subs_table.s || db_subs_table.len <= 0) {
		LM_ERR("sca_set_config: subs_table must be set!\n");
		return (-1);
	}
	scam->cfg->subs_table = &db_subs_table;

	if(!db_state_table.s || db_state_table.len <= 0) {
		LM_ERR("sca_set_config: state_table must be set!\n");
		return (-1);
	}
	scam->cfg->state_table = &db_state_table;

	if(hash_table_size > 0) {
		scam->cfg->hash_table_size = 1 << hash_table_size;
	} else {
		scam->cfg->hash_table_size = 512;
	}

	scam->cfg->db_update_interval = db_update_interval;
	scam->cfg->call_info_max_expires = call_info_max_expires;
	scam->cfg->line_seize_max_expires = line_seize_max_expires;
	scam->cfg->purge_expired_interval = purge_expired_interval;
	if(onhold_bflag > 31) {
		LM_ERR("sca_set_config: onhold_bflag value > 31\n");
		return (-1);
	}
	scam->cfg->onhold_bflag = onhold_bflag;

	if(server_address.s) {
		scam->cfg->server_address = &server_address;
	}

	return (0);
}

static int sca_child_init(int rank)
{
	if(rank == PROC_INIT || rank == PROC_TCP_MAIN) {
		return (0);
	}

	if(rank == PROC_MAIN) {
		if(fork_dummy_timer(PROC_TIMER, "SCA DB SYNC PROCESS",
				   0, // we don't need sockets, just writing to DB
				   sca_subscription_db_update_timer, // timer cb
				   NULL, // parameter passed to callback
				   sca->cfg->db_update_interval)
				< 0) {
			LM_ERR("sca_child_init: failed to register subscription DB "
				   "sync timer process\n");
			return (-1);
		}

		return (0);
	}

	if(sca->db_api == NULL || sca->db_api->init == NULL) {
		LM_CRIT("sca_child_init: DB API not loaded!\n");
		return (-1);
	}

	return (0);
}

static int sca_mod_init(void)
{
	sca = (sca_mod *)shm_malloc(sizeof(sca_mod));
	if(sca == NULL) {
		LM_ERR("Failed to shm_malloc module object\n");
		return (-1);
	}
	memset(sca, 0, sizeof(sca_mod));

	if(sca_set_config(sca) != 0) {
		LM_ERR("Failed to set configuration\n");
		goto error;
	}

	if(rpc_register_array(sca_rpc) != 0) {
		LM_ERR("Failed to register RPC commands\n");
		goto error;
	}

	if(sca_bind_srdb1(sca, &dbf) != 0) {
		LM_ERR("Failed to initialize required DB API\n");
		goto error;
	}

	if(load_tm_api(&tmb) != 0) {
		LM_ERR("Failed to initialize required tm API. Check that the \"tm\" "
			   "module is loaded before this module.\n");
		goto error;
	}
	sca->tm_api = &tmb;

	if(sca_bind_sl(sca, &slb) != 0) {
		LM_ERR("Failed to initialize required sl API. Check that the \"sl\" "
			   "module is loaded before this module.\n");
		goto error;
	}

	if(sca_hash_table_create(&sca->subscriptions, sca->cfg->hash_table_size)
			!= 0) {
		LM_ERR("Failed to create subscriptions hash table\n");
		goto error;
	}
	if(sca_hash_table_create(&sca->appearances, sca->cfg->hash_table_size)
			!= 0) {
		LM_ERR("Failed to create appearances hash table\n");
		goto error;
	}

	sca_subscriptions_restore_from_db(sca);

	register_timer(sca_subscription_purge_expired, sca,
			sca->cfg->purge_expired_interval);
	//register_timer(
	//		sca_appearance_purge_stale, sca, sca->cfg->purge_expired_interval);

	// register separate timer process to write subscriptions to DB.
	// move to 3.3+ timer API (register_basic_timer) at some point.
	// timer process forks in sca_child_init, above.
	register_dummy_timers(1);

	LM_INFO("SCA initialized \n");

	return (0);

error:
	if(sca != NULL) {
		if(sca->cfg != NULL) {
			shm_free(sca->cfg);
		}
		if(sca->subscriptions != NULL) {
			sca_hash_table_free(sca->subscriptions);
		}
		if(sca->appearances != NULL) {
			sca_hash_table_free(sca->appearances);
		}
		shm_free(sca);
		sca = NULL;
	}

	return (-1);
}

void sca_mod_destroy(void)
{
	if(sca == 0)
		return;

	// write back to the DB to retain most current subscription info
	if(sca_subscription_db_update() != 0) {
		if(sca && sca->cfg && sca->cfg->db_url) {
			LM_ERR("sca_mod_destroy: failed to save current subscriptions \n"
				   "in DB %.*s",
					STR_FMT(sca->cfg->db_url));
		}
	}

	sca_db_disconnect();
}

static int sca_call_info_update_0_f(sip_msg_t *msg, char *p1, char *p2)
{
	return sca_call_info_update(msg, SCA_CALL_INFO_SHARED_BOTH, NULL, NULL);
}

static int sca_call_info_update_1_f(sip_msg_t *msg, char *p1, char *p2)
{
	int update_mask = SCA_CALL_INFO_SHARED_BOTH;

	if(get_int_fparam(&update_mask, msg, (fparam_t *)p1) < 0) {
		LM_ERR("sca_call_info_update: argument 1: bad value "
			   "(integer expected)\n");
		return (-1);
	}

	return sca_call_info_update(msg, update_mask, NULL, NULL);
}

static int sca_call_info_update_2_f(sip_msg_t *msg, char *p1, char *p2)
{
	str uri_to = STR_NULL;
	int update_mask = SCA_CALL_INFO_SHARED_BOTH;

	if(get_int_fparam(&update_mask, msg, (fparam_t *)p1) < 0) {
		LM_ERR("sca_call_info_update: argument 1: bad value "
			   "(integer expected)\n");
		return (-1);
	}
	if(get_str_fparam(&uri_to, msg, (gparam_p)p2) != 0) {
		LM_ERR("unable to get value from param pvar_to\n");
		return -1;
	}
	return sca_call_info_update(msg, update_mask, &uri_to, NULL);
}

static int sca_call_info_update_3_f(
		sip_msg_t *msg, char *p1, char *p2, char *p3)
{
	str uri_to = STR_NULL;
	str uri_from = STR_NULL;
	int update_mask = SCA_CALL_INFO_SHARED_BOTH;

	if(get_int_fparam(&update_mask, msg, (fparam_t *)p1) < 0) {
		LM_ERR("sca_call_info_update: argument 1: bad value "
			   "(integer expected)\n");
		return (-1);
	}
	if(get_str_fparam(&uri_to, msg, (gparam_p)p2) != 0) {
		LM_ERR("unable to get value from param pvar_to\n");
		return -1;
	}
	if(get_str_fparam(&uri_from, msg, (gparam_p)p3) != 0) {
		LM_ERR("unable to get value from param pvar_from\n");
		return -1;
	}
	return sca_call_info_update(msg, update_mask, &uri_to, &uri_from);
}

int ki_sca_call_info_update_default(sip_msg_t *msg)
{
	return sca_call_info_update(msg, SCA_CALL_INFO_SHARED_BOTH, NULL, NULL);
}
int ki_sca_call_info_update_mask(sip_msg_t *msg, int umask)
{
	return sca_call_info_update(msg, umask, NULL, NULL);
}
int ki_sca_call_info_update_turi(sip_msg_t *msg, int umask, str *sto)
{
	return sca_call_info_update(msg, umask, sto, NULL);
}

int fixup_ciu(void **param, int param_no)
{
	switch(param_no) {
		case 1:
			return fixup_var_int_1(param, param_no);
		case 2:
		case 3:
			return fixup_spve_null(param, 1);
		default:
			return E_UNSPEC;
	}
}

int fixup_free_ciu(void **param, int param_no)
{
	switch(param_no) {
		case 1:
			return 0;
		case 2:
		case 3:
			return fixup_free_spve_null(param, 1);
		default:
			return E_UNSPEC;
	}
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_sca_exports[] = {
	{ str_init("sca"), str_init("handle_subscribe"),
		SR_KEMIP_INT, ki_sca_handle_subscribe,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sca"), str_init("call_info_update_default"),
		SR_KEMIP_INT, ki_sca_call_info_update_default,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sca"), str_init("call_info_update_mask"),
		SR_KEMIP_INT, ki_sca_call_info_update_mask,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sca"), str_init("call_info_update_turi"),
		SR_KEMIP_INT, ki_sca_call_info_update_turi,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sca"), str_init("call_info_update"),
		SR_KEMIP_INT, sca_call_info_update,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_sca_exports);
	return 0;
}
