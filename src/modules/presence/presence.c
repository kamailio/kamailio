/*
 * Copyright (C) 2006 Voice Sistem S.R.L.
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

/*!
 * \defgroup presence Presence :: A generic implementation of the SIP event package (PUBLISH, SUBSCRIBE, NOTIFY)
 * The Kamailio presence module is a generic module for SIP event packages, which is much more than presence.
 * It is extensible by developing other modules that use the internal developer API.
 * Examples:
 *- \ref presence_mwi
 *- \ref presence_xml
 */

/*!
 * \file
 * \brief Kamailio presence module :: Core
 * \ingroup presence
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../lib/srdb1/db.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/ut.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_from.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/usr_avp.h"
#include "../../core/rand/kam_rand.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../core/pt.h"
#include "../../core/hashes.h"
#include "../pua/hash.h"
#include "presence.h"
#include "publish.h"
#include "subscribe.h"
#include "event_list.h"
#include "bind_presence.h"
#include "notify.h"
#include "presence_dmq.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/timer_proc.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

MODULE_VERSION

#define S_TABLE_VERSION 3
#define P_TABLE_VERSION 5
#define ACTWATCH_TABLE_VERSION 12

static int pres_clean_period = 100;
static int pres_db_update_period = 100;
int pres_local_log_level = L_INFO;

static char *pres_log_facility_str =
		0; /*!< Syslog: log facility that is used */
int pres_local_log_facility;

/* database connection */
db1_con_t *pa_db = NULL;
db_func_t pa_dbf;
str presentity_table = str_init("presentity");
str active_watchers_table = str_init("active_watchers");
str watchers_table = str_init("watchers");

int pres_fetch_rows = 500;
static int pres_library_mode = 0;
str pres_server_address = {0, 0};
evlist_t *pres_evlist = NULL;
int pres_subs_remove_match = 0;
int _pres_subs_mode = 1;
static int pres_timer_mode = 1;

int pres_uri_match = 0;
/* sip uri match function pointer */
sip_uri_match_f presence_sip_uri_match;
static int sip_uri_case_sensitive_match(str *s1, str *s2);
static int sip_uri_case_insensitive_match(str *s1, str *s2);

/* TM bind */
struct tm_binds tmb;
/* SL API structure */
sl_api_t slb;

/** module functions */

static int mod_init(void);
static int child_init(int);
static void destroy(void);
int stored_pres_info(struct sip_msg *msg, char *pres_uri, char *s);
static int fixup_presence(void **param, int param_no);
static int fixup_subscribe(void **param, int param_no);
static int update_pw_dialogs(
		subs_t *subs, unsigned int hash_code, subs_t **subs_array);
static int w_pres_auth_status(struct sip_msg *_msg, char *_sp1, char *_sp2);
static int w_pres_refresh_watchers(
		struct sip_msg *msg, char *puri, char *pevent, char *ptype);
static int w_pres_refresh_watchers5(struct sip_msg *msg, char *puri,
		char *pevent, char *ptype, char *furi, char *fname);
static int w_pres_update_watchers(
		struct sip_msg *msg, char *puri, char *pevent);
static int fixup_refresh_watchers(void **param, int param_no);
static int fixup_update_watchers(void **param, int param_no);
static int presence_init_rpc(void);

static int w_pres_has_subscribers(struct sip_msg *_msg, char *_sp1, char *_sp2);
static int fixup_has_subscribers(void **param, int param_no);

int pres_counter = 0;
int pres_pid = 0;
char pres_prefix = 'a';
unsigned int pres_startup_time = 0;
str pres_db_url = {0, 0};
int pres_expires_offset = 0;
int pres_cseq_offset = 0;
uint32_t pres_min_expires = 0;
int pres_min_expires_action = 1;
uint32_t pres_max_expires = 3600;
int shtable_size = 9;
shtable_t subs_htable = NULL;
int pres_subs_dbmode = WRITE_BACK;
int pres_sphere_enable = 0;
int pres_timeout_rm_subs = 1;
int pres_send_fast_notify = 1;
int publ_cache_mode = PS_PCACHE_HYBRID;
int pres_waitn_time = 5;
int pres_notifier_poll_rate = 10;
int pres_notifier_processes = 1;
int pres_force_delete = 0;
int pres_startup_mode = 1;
str pres_xavp_cfg = {0};
int pres_retrieve_order = 0;
str pres_retrieve_order_by = str_init("priority");
int pres_enable_dmq = 0;
int pres_delete_same_subs = 0;
int pres_subs_respond_200 = 1;

int pres_db_table_lock_type = 1;
db_locking_t pres_db_table_lock = DB_LOCKING_WRITE;

int *pres_notifier_id = NULL;

int phtable_size = 9;
phtable_t *pres_htable = NULL;

sruid_t pres_sruid;

/* clang-format off */
static cmd_export_t cmds[]=
{
	{"handle_publish",        (cmd_function)w_handle_publish,        0,
		fixup_presence, 0, REQUEST_ROUTE},
	{"handle_publish",        (cmd_function)w_handle_publish,        1,
		fixup_presence, 0, REQUEST_ROUTE},
	{"handle_subscribe",      (cmd_function)handle_subscribe0,       0,
		fixup_subscribe, 0, REQUEST_ROUTE},
	{"handle_subscribe",      (cmd_function)w_handle_subscribe,      1,
		fixup_subscribe, 0, REQUEST_ROUTE},
	{"pres_auth_status",      (cmd_function)w_pres_auth_status,      2,
		fixup_spve_spve, fixup_free_spve_spve, REQUEST_ROUTE},
	{"pres_refresh_watchers", (cmd_function)w_pres_refresh_watchers, 3,
		fixup_refresh_watchers, 0, ANY_ROUTE},
	{"pres_refresh_watchers", (cmd_function)w_pres_refresh_watchers5,5,
		fixup_refresh_watchers, 0, ANY_ROUTE},
	{"pres_update_watchers",  (cmd_function)w_pres_update_watchers,  2,
		fixup_update_watchers, 0, ANY_ROUTE},
	{"pres_has_subscribers",  (cmd_function)w_pres_has_subscribers,  2,
                fixup_has_subscribers, 0, ANY_ROUTE},
 	{"bind_presence",         (cmd_function)bind_presence,           1,
		0, 0, 0},
	{ 0, 0, 0, 0, 0, 0}
};
/* clang-format on */

/* clang-format off */
static param_export_t params[]={
	{ "db_url",                 PARAM_STR, &pres_db_url},
	{ "presentity_table",       PARAM_STR, &presentity_table},
	{ "active_watchers_table",  PARAM_STR, &active_watchers_table},
	{ "watchers_table",         PARAM_STR, &watchers_table},
	{ "clean_period",           INT_PARAM, &pres_clean_period },
	{ "db_update_period",       INT_PARAM, &pres_db_update_period },
	{ "waitn_time",             INT_PARAM, &pres_waitn_time },
	{ "notifier_poll_rate",     INT_PARAM, &pres_notifier_poll_rate },
	{ "notifier_processes",     INT_PARAM, &pres_notifier_processes },
	{ "force_delete",           INT_PARAM, &pres_force_delete },
	{ "startup_mode",           INT_PARAM, &pres_startup_mode },
	{ "expires_offset",         INT_PARAM, &pres_expires_offset },
	{ "max_expires",            INT_PARAM, &pres_max_expires },
	{ "min_expires",            INT_PARAM, &pres_min_expires },
	{ "min_expires_action",     INT_PARAM, &pres_min_expires_action },
	{ "server_address",         PARAM_STR, &pres_server_address},
	{ "subs_htable_size",       INT_PARAM, &shtable_size},
	{ "pres_htable_size",       INT_PARAM, &phtable_size},
	{ "subs_db_mode",           INT_PARAM, &pres_subs_dbmode},
	{ "publ_cache",             INT_PARAM, &publ_cache_mode},
	{ "enable_sphere_check",    INT_PARAM, &pres_sphere_enable},
	{ "timeout_rm_subs",        INT_PARAM, &pres_timeout_rm_subs},
	{ "send_fast_notify",       INT_PARAM, &pres_send_fast_notify},
	{ "fetch_rows",             INT_PARAM, &pres_fetch_rows},
	{ "db_table_lock_type",     INT_PARAM, &pres_db_table_lock_type},
	{ "local_log_level",        PARAM_INT, &pres_local_log_level},
	{ "local_log_facility",     PARAM_STRING, &pres_log_facility_str},
	{ "subs_remove_match",      PARAM_INT, &pres_subs_remove_match},
	{ "xavp_cfg",               PARAM_STR, &pres_xavp_cfg},
	{ "retrieve_order",         PARAM_INT, &pres_retrieve_order},
	{ "retrieve_order_by",      PARAM_STR, &pres_retrieve_order_by},
	{ "sip_uri_match",          PARAM_INT, &pres_uri_match},
	{ "cseq_offset",            PARAM_INT, &pres_cseq_offset},
	{ "enable_dmq",             PARAM_INT, &pres_enable_dmq},
	{ "pres_subs_mode",         PARAM_INT, &_pres_subs_mode},
	{ "delete_same_subs",       PARAM_INT, &pres_delete_same_subs},
	{ "timer_mode",             PARAM_INT, &pres_timer_mode},
	{ "subs_respond_200",       PARAM_INT, &pres_subs_respond_200},

	{0,0,0}
};
/* clang-format on */

/* clang-format off */
static pv_export_t pres_mod_pvs[] = {
	{ {"subs", (sizeof("subs")-1)}, PVT_OTHER,
		pv_get_subscription, 0, pv_parse_subscription_name, 0, 0, 0},
	{ {"notify_reply", (sizeof("notify_reply")-1)}, PVT_OTHER,
		pv_get_notify_reply, 0, pv_parse_notify_reply_var_name, 0, 0, 0},
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};
/* clang-format on */

/** module exports */
/* clang-format off */
struct module_exports exports= {
	"presence",			/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	pres_mod_pvs,		/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	child_init,			/* per-child init function */
	destroy				/* module destroy function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{
	if(pres_uri_match == 1) {
		presence_sip_uri_match = sip_uri_case_insensitive_match;
	} else {
		presence_sip_uri_match = sip_uri_case_sensitive_match;
	}

	if(presence_init_rpc() != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	LM_DBG("db_url=%s (len=%d addr=%p)\n", ZSW(pres_db_url.s), pres_db_url.len,
			pres_db_url.s);

	if(pres_db_url.s == NULL || pres_db_url.len == 0) {
		if(publ_cache_mode != PS_PCACHE_RECORD) {
			LM_DBG("db url is not set - switch to library mode\n");
			pres_library_mode = 1;
		}
	}

	pres_evlist = init_evlist();
	if(!pres_evlist) {
		LM_ERR("unsuccessful initialize event list\n");
		return -1;
	}

	if(pres_library_mode == 1) {
		LM_DBG("Presence module used for API library purpose only\n");
		return 0;
	}

	if(sruid_init(&pres_sruid, '-', "pres", SRUID_INC) < 0) {
		return -1;
	}

	if(pres_expires_offset < 0) {
		pres_expires_offset = 0;
	}

	if(pres_max_expires <= 0) {
		pres_max_expires = 3600;
	}

	if(pres_min_expires > pres_max_expires) {
		pres_min_expires = pres_max_expires;
	}

	if(pres_min_expires_action < 1 || pres_min_expires_action > 2) {
		LM_ERR("min_expires_action must be 1 = RFC 6665/3261 Reply 423, 2 = "
			   "force min_expires value\n");
		return -1;
	}

	if(pres_server_address.s == NULL || pres_server_address.len==0) {
		LM_DBG("server_address parameter not set in configuration file\n");
	}

	/* bind the SL API */
	if(sl_load_api(&slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* load all TM stuff */
	if(load_tm_api(&tmb) == -1) {
		LM_ERR("Can't load tm functions. Module TM not loaded?\n");
		return -1;
	}

	if(publ_cache_mode==PS_PCACHE_HYBRID || publ_cache_mode==PS_PCACHE_RECORD) {
		if(phtable_size < 1) {
			phtable_size = 256;
		} else {
			phtable_size = 1 << phtable_size;
		}
	}

	if(publ_cache_mode==PS_PCACHE_RECORD) {
		if(ps_ptable_init(phtable_size) < 0) {
			return -1;
		}
	}

	if(pres_subs_dbmode != DB_ONLY) {
		if(shtable_size < 1) {
			shtable_size = 512;
		} else {
			shtable_size = 1 << shtable_size;
		}

		subs_htable = new_shtable(shtable_size);
		if(subs_htable == NULL) {
			LM_ERR(" initializing subscribe hash table\n");
			goto dberror;
		}
	}

	if(publ_cache_mode==PS_PCACHE_HYBRID) {
		pres_htable = new_phtable();
		if(pres_htable == NULL) {
			LM_ERR("initializing presentity hash table\n");
			goto dberror;
		}
	}

	if(publ_cache_mode != PS_PCACHE_RECORD || pres_subs_dbmode != NO_DB) {
		if(pres_db_url.s == NULL) {
			LM_ERR("database url not set!\n");
			return -1;
		}

		/* binding to database module  */
		if(db_bind_mod(&pres_db_url, &pa_dbf)) {
			LM_ERR("Database module not found\n");
			return -1;
		}

		if(!DB_CAPABILITY(pa_dbf, DB_CAP_ALL)) {
			LM_ERR("Database module does not implement all functions"
				   " needed by presence module\n");
			return -1;
		}

		pa_db = pa_dbf.init(&pres_db_url);
		if(!pa_db) {
			LM_ERR("Connection to database failed\n");
			return -1;
		}

		/*verify table versions */
		if((db_check_table_version(
					&pa_dbf, pa_db, &presentity_table, P_TABLE_VERSION)
				   < 0)
				|| (db_check_table_version(
							&pa_dbf, pa_db, &watchers_table, S_TABLE_VERSION)
						   < 0)) {
			DB_TABLE_VERSION_ERROR(presentity_table);
			goto dberror;
		}

		if(pres_subs_dbmode != NO_DB
				&& db_check_table_version(&pa_dbf, pa_db, &active_watchers_table,
						   ACTWATCH_TABLE_VERSION)
						   < 0) {
			DB_TABLE_VERSION_ERROR(active_watchers_table);
			goto dberror;
		}


		if(pres_subs_dbmode != DB_ONLY) {
			if(restore_db_subs() < 0) {
				LM_ERR("restoring subscribe info from database\n");
				goto dberror;
			}
		}

		if(publ_cache_mode==PS_PCACHE_HYBRID) {
			if(pres_htable_db_restore() < 0) {
				LM_ERR("filling in presentity hash table from database\n");
				goto dberror;
			}
		}
	}


	pres_startup_time = (unsigned int)(uint64_t)time(NULL);
	if(pres_clean_period > 0) {
		if(pres_timer_mode==0) {
			register_timer(ps_presentity_db_timer_clean, 0, pres_clean_period);
			register_timer(ps_watchers_db_timer_clean, 0, pres_clean_period);
			if(publ_cache_mode==PS_PCACHE_RECORD) {
				register_timer(ps_ptable_timer_clean, 0, pres_clean_period);
			}
		} else {
			sr_wtimer_add(ps_presentity_db_timer_clean, 0, pres_clean_period);
			sr_wtimer_add(ps_watchers_db_timer_clean, 0, pres_clean_period);
			if(publ_cache_mode==PS_PCACHE_RECORD) {
				sr_wtimer_add(ps_ptable_timer_clean, 0, pres_clean_period);
			}
		}
	}

	if(pres_db_update_period > 0) {
		if(pres_timer_mode==0) {
			register_timer(timer_db_update, 0, pres_db_update_period);
		} else {
			sr_wtimer_add(timer_db_update, 0, pres_db_update_period);
		}
	}

	if(pres_waitn_time <= 0) {
		pres_waitn_time = 5;
	}

	if(pres_notifier_poll_rate <= 0) {
		pres_notifier_poll_rate = 10;
	}

	if(pres_notifier_processes < 0 || pres_subs_dbmode != DB_ONLY) {
		pres_notifier_processes = 0;
	}

	if(pres_notifier_processes > 0) {
		if((pres_notifier_id =
						   shm_malloc(sizeof(int) * pres_notifier_processes))
				== NULL) {
			LM_ERR("allocating shared memory\n");
			goto dberror;
		}

		register_basic_timers(pres_notifier_processes);
	}

	if(pres_force_delete > 0)
		pres_force_delete = 1;

	if(pres_log_facility_str) {
		int tmp = str2facility(pres_log_facility_str);

		if(tmp != -1) {
			pres_local_log_facility = tmp;
		} else {
			LM_ERR("invalid log facility configured\n");
			goto dberror;
		}
	} else {
		pres_local_log_facility = cfg_get(core, core_cfg, log_facility);
	}

	if(pres_db_table_lock_type != 1) {
		pres_db_table_lock = DB_LOCKING_NONE;
	}

	if(pa_db) {
		pa_dbf.close(pa_db);
		pa_db = NULL;
	}

	goto_on_notify_reply = route_lookup(&event_rt, "presence:notify-reply");
	if(goto_on_notify_reply >= 0 && event_rt.rlist[goto_on_notify_reply] == 0)
		goto_on_notify_reply = -1; /* disable */

	if(pres_enable_dmq > 0 && pres_dmq_initialize() != 0) {
		LM_ERR("failed to initialize dmq integration\n");
		return -1;
	}

	return 0;

dberror:
	if(pa_db) {
		pa_dbf.close(pa_db);
		pa_db = NULL;
	}
	return -1;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	if(rank == PROC_INIT || rank == PROC_TCP_MAIN) {
		return 0;
	}

	pres_pid = my_pid();

	if(pres_library_mode) {
		return 0;
	}

	if(sruid_init(&pres_sruid, '-', "pres", SRUID_INC) < 0) {
		return -1;
	}

	if(publ_cache_mode == PS_PCACHE_RECORD && pres_subs_dbmode == NO_DB) {
		return 0;
	}

	if(rank == PROC_MAIN) {
		int i;

		for(i = 0; i < pres_notifier_processes; i++) {
			char tmp[30];
			snprintf(tmp, 30, "PRESENCE NOTIFIER %d", i);
			pres_notifier_id[i] = i;

			if(fork_basic_utimer(PROC_TIMER, tmp, 1, pres_timer_send_notify,
					   &pres_notifier_id[i], 1000000 / pres_notifier_poll_rate)
					< 0) {
				LM_ERR("Failed to start PRESENCE NOTIFIER %d\n", i);
				return -1;
			}
		}

		return 0;
	}

	if(pa_dbf.init == 0) {
		LM_CRIT("child_init: database not bound\n");
		return -1;
	}
	/* Do not pool the connections where possible when running notifier
	 * processes. */
	if(pres_notifier_processes > 0 && pa_dbf.init2)
		pa_db = pa_dbf.init2(&pres_db_url, DB_POOLING_NONE);
	else
		pa_db = pa_dbf.init(&pres_db_url);
	if(!pa_db) {
		LM_ERR("child %d: unsuccessful connecting to database\n", rank);
		return -1;
	}

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("child %d:unsuccessful use_table presentity_table\n", rank);
		return -1;
	}

	if(pa_dbf.use_table(pa_db, &active_watchers_table) < 0) {
		LM_ERR("child %d:unsuccessful use_table active_watchers_table\n", rank);
		return -1;
	}

	if(pa_dbf.use_table(pa_db, &watchers_table) < 0) {
		LM_ERR("child %d:unsuccessful use_table watchers_table\n", rank);
		return -1;
	}

	LM_DBG("child %d: Database connection opened successfully\n", rank);

	return 0;
}


/*
 * destroy function
 */
static void destroy(void)
{
	if(subs_htable && pres_subs_dbmode == WRITE_BACK) {
		/* open database connection */
		pa_db = pa_dbf.init(&pres_db_url);
		if(!pa_db) {
			LM_ERR("mod_destroy: unsuccessful connecting to database\n");
		} else
			timer_db_update(0, 0);
	}

	if(subs_htable) {
		destroy_shtable(subs_htable, shtable_size);
	}

	if(pres_htable) {
		destroy_phtable();
	}

	if(pa_db && pa_dbf.close) {
		pa_dbf.close(pa_db);
	}

	if(pres_notifier_id != NULL) {
		shm_free(pres_notifier_id);
	}

	destroy_evlist();

	ps_ptable_destroy();
}

static int fixup_presence(void **param, int param_no)
{
	if(pres_library_mode) {
		LM_ERR("Bad config - you can not call 'handle_publish' function"
			   " (db_url not set)\n");
		return -1;
	}
	if(param_no == 0)
		return 0;

	return fixup_spve_null(param, 1);
}

static int fixup_subscribe(void **param, int param_no)
{

	if(pres_library_mode) {
		LM_ERR("Bad config - you can not call 'handle_subscribe' function"
			   " (db_url not set)\n");
		return -1;
	}
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

int pres_refresh_watchers(
		str *pres, str *event, int type, str *file_uri, str *filename)
{
	pres_ev_t *ev;
	struct sip_uri uri;
	str *rules_doc = NULL;
	int result;

	ev = contains_event(event, NULL);
	if(ev == NULL) {
		LM_ERR("wrong event parameter\n");
		return -1;
	}

	if(type == 0) {
		/* if a request to refresh watchers authorization */
		if(ev->get_rules_doc == NULL) {
			LM_ERR("wrong request for a refresh watchers authorization status"
				   "for an event that does not require authorization\n");
			goto error;
		}

		if(parse_uri(pres->s, pres->len, &uri) < 0) {
			LM_ERR("parsing uri [%.*s]\n", pres->len, pres->s);
			goto error;
		}

		result = ev->get_rules_doc(&uri.user, &uri.host, &rules_doc);
		if(result < 0 || rules_doc == NULL || rules_doc->s == NULL) {
			LM_ERR("no rules doc found for the user\n");
			goto error;
		}

		if(update_watchers_status(pres, ev, rules_doc) < 0) {
			LM_ERR("failed to update watchers\n");
			goto error;
		}

		pkg_free(rules_doc->s);
		pkg_free(rules_doc);
		rules_doc = NULL;

	} else {
		if(type == 2) {
			if(update_hard_presentity(pres, ev, file_uri, filename) < 0) {
				LM_ERR("updating hard presentity\n");
				goto error;
			}
		}

		/* if a request to refresh notified info */
		if(query_db_notify(pres, ev, NULL) < 0) {
			LM_ERR("sending Notify requests\n");
			goto error;
		}
	}
	return 0;

error:
	if(rules_doc) {
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	return -1;
}

int _api_pres_refresh_watchers(str *pres, str *event, int type)
{
	return pres_refresh_watchers(pres, event, type, NULL, NULL);
}

int ki_pres_refresh_watchers(sip_msg_t *msg, str *pres, str *event, int type)
{
	return pres_refresh_watchers(pres, event, type, NULL, NULL);
}

int ki_pres_refresh_watchers_file(sip_msg_t *msg, str *pres, str *event,
		int type, str *file_uri, str *filename)
{
	return pres_refresh_watchers(pres, event, type, file_uri, filename);
}

int pres_update_status(subs_t *subs, str reason, db_key_t *query_cols,
		db_val_t *query_vals, int n_query_cols, subs_t **subs_array)
{
	db_key_t update_cols[5];
	db_val_t update_vals[5];
	int n_update_cols = 0;
	int u_status_col, u_reason_col, q_wuser_col, q_wdomain_col;
	int status;
	query_cols[q_wuser_col = n_query_cols] = &str_watcher_username_col;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].type = DB1_STR;
	n_query_cols++;

	query_cols[q_wdomain_col = n_query_cols] = &str_watcher_domain_col;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].type = DB1_STR;
	n_query_cols++;

	update_cols[u_status_col = n_update_cols] = &str_status_col;
	update_vals[u_status_col].nul = 0;
	update_vals[u_status_col].type = DB1_INT;
	n_update_cols++;

	update_cols[u_reason_col = n_update_cols] = &str_reason_col;
	update_vals[u_reason_col].nul = 0;
	update_vals[u_reason_col].type = DB1_STR;
	n_update_cols++;

	status = subs->status;
	if(subs->event->get_auth_status(subs) < 0) {
		LM_ERR("getting status from rules document\n");
		return -1;
	}
	LM_DBG("subs.status= %d\n", subs->status);
	if(get_status_str(subs->status) == NULL) {
		LM_ERR("wrong status: %d\n", subs->status);
		return -1;
	}

	if(subs->status != status || reason.len != subs->reason.len
			|| (reason.s && subs->reason.s
					   && strncmp(reason.s, subs->reason.s, reason.len))) {
		/* update in watchers_table */
		query_vals[q_wuser_col].val.str_val = subs->watcher_user;
		query_vals[q_wdomain_col].val.str_val = subs->watcher_domain;

		update_vals[u_status_col].val.int_val = subs->status;
		update_vals[u_reason_col].val.str_val = subs->reason;

		if(pa_dbf.use_table(pa_db, &watchers_table) < 0) {
			LM_ERR("in use_table\n");
			return -1;
		}

		if(pa_dbf.update(pa_db, query_cols, 0, query_vals, update_cols,
				   update_vals, n_query_cols, n_update_cols)
				< 0) {
			LM_ERR("in sql update\n");
			return -1;
		}
		/* save in the list all affected dialogs */
		/* if status switches to terminated -> delete dialog */
		if(update_pw_dialogs(subs, subs->db_flag, subs_array) < 0) {
			LM_ERR("extracting dialogs from [watcher]=%.*s@%.*s to"
				   " [presentity]=%.*s\n",
					subs->watcher_user.len, subs->watcher_user.s,
					subs->watcher_domain.len, subs->watcher_domain.s,
					subs->pres_uri.len, subs->pres_uri.s);
			return -1;
		}
	}
	return 0;
}

int pres_db_delete_status(subs_t *s)
{
	int n_query_cols = 0;
	db_key_t query_cols[5];
	db_val_t query_vals[5];

	if(pa_dbf.use_table(pa_db, &active_watchers_table) < 0) {
		LM_ERR("sql use table failed\n");
		return -1;
	}

	query_cols[n_query_cols] = &str_event_col;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].val.str_val = s->event->name;
	n_query_cols++;

	query_cols[n_query_cols] = &str_presentity_uri_col;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].val.str_val = s->pres_uri;
	n_query_cols++;

	query_cols[n_query_cols] = &str_watcher_username_col;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].val.str_val = s->watcher_user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_watcher_domain_col;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].val.str_val = s->watcher_domain;
	n_query_cols++;

	if(pa_dbf.delete(pa_db, query_cols, 0, query_vals, n_query_cols) < 0) {
		LM_ERR("sql delete failed\n");
		return -1;
	}
	return 0;
}

int update_watchers_status(str *pres_uri, pres_ev_t *ev, str *rules_doc)
{
	subs_t subs;
	db_key_t query_cols[6], result_cols[5];
	db_val_t query_vals[6];
	int n_result_cols = 0, n_query_cols = 0;
	db1_res_t *result = NULL;
	db_row_t *row;
	db_val_t *row_vals;
	int i;
	str w_user, w_domain, reason = {0, 0};
	unsigned int status;
	int status_col, w_user_col, w_domain_col, reason_col;
	subs_t *subs_array = NULL, *s;
	unsigned int hash_code;
	int err_ret = -1;
	int n = 0;

	typedef struct ws
	{
		int status;
		str reason;
		str w_user;
		str w_domain;
	} ws_t;

	ws_t *ws_list = NULL;

	LM_DBG("start\n");

	if(ev->content_type.s == NULL) {
		ev = contains_event(&ev->name, NULL);
		if(ev == NULL) {
			LM_ERR("wrong event parameter\n");
			return 0;
		}
	}

	memset(&subs, 0, sizeof(subs_t));
	subs.pres_uri = *pres_uri;
	subs.event = ev;
	subs.auth_rules_doc = rules_doc;

	/* update in watchers_table */
	query_cols[n_query_cols] = &str_presentity_uri_col;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].val.str_val = *pres_uri;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].val.str_val = ev->name;
	n_query_cols++;

	result_cols[status_col = n_result_cols++] = &str_status_col;
	result_cols[reason_col = n_result_cols++] = &str_reason_col;
	result_cols[w_user_col = n_result_cols++] = &str_watcher_username_col;
	result_cols[w_domain_col = n_result_cols++] = &str_watcher_domain_col;

	if(pa_dbf.use_table(pa_db, &watchers_table) < 0) {
		LM_ERR("in use_table\n");
		goto done;
	}

	if(pa_dbf.query(pa_db, query_cols, 0, query_vals, result_cols, n_query_cols,
			   n_result_cols, 0, &result)
			< 0) {
		LM_ERR("in sql query\n");
		goto done;
	}
	if(result == NULL)
		return 0;

	if(result->n <= 0) {
		err_ret = 0;
		goto done;
	}

	LM_DBG("found %d record-uri in watchers_table\n", result->n);
	hash_code = core_case_hash(pres_uri, &ev->name, shtable_size);
	subs.db_flag = hash_code;

	/* must do a copy as sphere_check requires database queries */
	if(pres_sphere_enable) {
		n = result->n;
		ws_list = (ws_t *)pkg_malloc(n * sizeof(ws_t));
		if(ws_list == NULL) {
			LM_ERR("No more private memory\n");
			goto done;
		}
		memset(ws_list, 0, n * sizeof(ws_t));

		for(i = 0; i < result->n; i++) {
			row = &result->rows[i];
			row_vals = ROW_VALUES(row);

			status = row_vals[status_col].val.int_val;

			reason.s = (char *)row_vals[reason_col].val.string_val;
			reason.len = reason.s ? strlen(reason.s) : 0;

			w_user.s = (char *)row_vals[w_user_col].val.string_val;
			w_user.len = strlen(w_user.s);

			w_domain.s = (char *)row_vals[w_domain_col].val.string_val;
			w_domain.len = strlen(w_domain.s);

			if(reason.len) {
				ws_list[i].reason.s =
						(char *)pkg_malloc(reason.len * sizeof(char));
				if(ws_list[i].reason.s == NULL) {
					LM_ERR("No more private memory\n");
					goto done;
				}
				memcpy(ws_list[i].reason.s, reason.s, reason.len);
				ws_list[i].reason.len = reason.len;
			} else
				ws_list[i].reason.s = NULL;

			ws_list[i].w_user.s = (char *)pkg_malloc(w_user.len * sizeof(char));
			if(ws_list[i].w_user.s == NULL) {
				LM_ERR("No more private memory\n");
				goto done;
			}
			memcpy(ws_list[i].w_user.s, w_user.s, w_user.len);
			ws_list[i].w_user.len = w_user.len;

			ws_list[i].w_domain.s =
					(char *)pkg_malloc(w_domain.len * sizeof(char));
			if(ws_list[i].w_domain.s == NULL) {
				LM_ERR("No more private memory\n");
				goto done;
			}
			memcpy(ws_list[i].w_domain.s, w_domain.s, w_domain.len);
			ws_list[i].w_domain.len = w_domain.len;

			ws_list[i].status = status;
		}

		pa_dbf.free_result(pa_db, result);
		result = NULL;

		for(i = 0; i < n; i++) {
			subs.watcher_user = ws_list[i].w_user;
			subs.watcher_domain = ws_list[i].w_domain;
			subs.status = ws_list[i].status;
			memset(&subs.reason, 0, sizeof(str));

			if(pres_update_status(&subs, reason, query_cols, query_vals,
					   n_query_cols, &subs_array)
					< 0) {
				LM_ERR("failed to update watcher status\n");
				goto done;
			}
		}

		for(i = 0; i < n; i++) {
			pkg_free(ws_list[i].w_user.s);
			pkg_free(ws_list[i].w_domain.s);
			if(ws_list[i].reason.s)
				pkg_free(ws_list[i].reason.s);
		}
		pkg_free(ws_list);
		ws_list = NULL;

		goto send_notify;
	}

	for(i = 0; i < result->n; i++) {
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);

		status = row_vals[status_col].val.int_val;

		reason.s = (char *)row_vals[reason_col].val.string_val;
		reason.len = reason.s ? strlen(reason.s) : 0;

		w_user.s = (char *)row_vals[w_user_col].val.string_val;
		w_user.len = strlen(w_user.s);

		w_domain.s = (char *)row_vals[w_domain_col].val.string_val;
		w_domain.len = strlen(w_domain.s);

		subs.watcher_user = w_user;
		subs.watcher_domain = w_domain;
		subs.status = status;
		memset(&subs.reason, 0, sizeof(str));

		if(pres_update_status(&subs, reason, query_cols, query_vals,
				   n_query_cols, &subs_array)
				< 0) {
			LM_ERR("failed to update watcher status\n");
			goto done;
		}
	}

	pa_dbf.free_result(pa_db, result);
	result = NULL;

send_notify:

	if(pres_notifier_processes == 0) {
		s = subs_array;

		while(s) {
			if(notify(s, NULL, NULL, 0, 0) < 0) {
				LM_ERR("sending Notify request\n");
				goto done;
			}

			/* delete from database also */
			if(s->status == TERMINATED_STATUS) {
				if(pres_db_delete_status(s) < 0) {
					LM_ERR("failed to delete terminated "
						   "dialog from database\n");
					goto done;
				}
			}

			s = s->next;
		}
	}

	free_subs_list(subs_array, PKG_MEM_TYPE, 0);
	return 0;

done:
	if(result)
		pa_dbf.free_result(pa_db, result);
	free_subs_list(subs_array, PKG_MEM_TYPE, 0);
	if(ws_list) {
		for(i = 0; i < n; i++) {
			if(ws_list[i].w_user.s)
				pkg_free(ws_list[i].w_user.s);
			if(ws_list[i].w_domain.s)
				pkg_free(ws_list[i].w_domain.s);
			if(ws_list[i].reason.s)
				pkg_free(ws_list[i].reason.s);
		}
		pkg_free(ws_list);
	}
	return err_ret;
}

/********************************************************************************/

static int update_pw_dialogs_dbonlymode(subs_t *subs, subs_t **subs_array)
{
	db_key_t query_cols[5], db_cols[3];
	db_val_t query_vals[5], db_vals[3];
	db_key_t result_cols[26];
	int n_query_cols = 0, n_result_cols = 0, n_update_cols = 0;
	int event_col, pres_uri_col, watcher_user_col, watcher_domain_col;
	int r_pres_uri_col, r_to_user_col, r_to_domain_col;
	int r_from_user_col, r_from_domain_col, r_callid_col;
	int r_to_tag_col, r_from_tag_col, r_sockinfo_col;
	int r_event_id_col, r_local_contact_col, r_contact_col;
	int r_record_route_col, r_reason_col;
	int r_event_col, r_local_cseq_col, r_remote_cseq_col;
	int r_status_col, r_version_col;
	int r_expires_col, r_watcher_user_col, r_watcher_domain_col;
	int r_flags_col, r_user_agent_col;
	db1_res_t *result = NULL;
	db_val_t *row_vals;
	db_row_t *rows;
	int nr_rows, loop;
	subs_t s, *cs;
	str ev_sname;

	if(pa_db == NULL) {
		LM_ERR("null database connection\n");
		return (-1);
	}

	if(pa_dbf.use_table(pa_db, &active_watchers_table) < 0) {
		LM_ERR("use table failed\n");
		return (-1);
	}

	query_cols[event_col = n_query_cols] = &str_event_col;
	query_vals[event_col].nul = 0;
	query_vals[event_col].type = DB1_STR;
	query_vals[event_col].val.str_val = subs->event->name;
	n_query_cols++;

	query_cols[pres_uri_col = n_query_cols] = &str_presentity_uri_col;
	query_vals[pres_uri_col].nul = 0;
	query_vals[pres_uri_col].type = DB1_STR;
	query_vals[pres_uri_col].val.str_val = subs->pres_uri;
	n_query_cols++;

	query_cols[watcher_user_col = n_query_cols] = &str_watcher_username_col;
	query_vals[watcher_user_col].nul = 0;
	query_vals[watcher_user_col].type = DB1_STR;
	query_vals[watcher_user_col].val.str_val = subs->watcher_user;
	n_query_cols++;

	query_cols[watcher_domain_col = n_query_cols] = &str_watcher_domain_col;
	query_vals[watcher_domain_col].nul = 0;
	query_vals[watcher_domain_col].type = DB1_STR;
	query_vals[watcher_domain_col].val.str_val = subs->watcher_domain;
	n_query_cols++;


	result_cols[r_to_user_col = n_result_cols++] = &str_to_user_col;
	result_cols[r_to_domain_col = n_result_cols++] = &str_to_domain_col;
	result_cols[r_from_user_col = n_result_cols++] = &str_from_user_col;
	result_cols[r_from_domain_col = n_result_cols++] = &str_from_domain_col;
	result_cols[r_watcher_user_col = n_result_cols++] =
			&str_watcher_username_col;
	result_cols[r_watcher_domain_col = n_result_cols++] =
			&str_watcher_domain_col;
	result_cols[r_callid_col = n_result_cols++] = &str_callid_col;
	result_cols[r_to_tag_col = n_result_cols++] = &str_to_tag_col;
	result_cols[r_from_tag_col = n_result_cols++] = &str_from_tag_col;
	result_cols[r_sockinfo_col = n_result_cols++] = &str_socket_info_col;
	result_cols[r_event_id_col = n_result_cols++] = &str_event_id_col;
	result_cols[r_local_contact_col = n_result_cols++] = &str_local_contact_col;
	result_cols[r_record_route_col = n_result_cols++] = &str_record_route_col;
	result_cols[r_reason_col = n_result_cols++] = &str_reason_col;
	result_cols[r_local_cseq_col = n_result_cols++] = &str_local_cseq_col;
	result_cols[r_version_col = n_result_cols++] = &str_version_col;
	result_cols[r_expires_col = n_result_cols++] = &str_expires_col;
	result_cols[r_event_col = n_result_cols++] = &str_event_col;
	result_cols[r_pres_uri_col = n_result_cols++] = &str_presentity_uri_col;
	result_cols[r_contact_col = n_result_cols++] = &str_contact_col;

	/* these ones are unused for some reason !!! */
	result_cols[r_remote_cseq_col = n_result_cols++] = &str_remote_cseq_col;
	result_cols[r_status_col = n_result_cols++] = &str_status_col;
	/*********************************************/

	result_cols[r_flags_col = n_result_cols++] = &str_flags_col;
	result_cols[r_user_agent_col = n_result_cols++] = &str_user_agent_col;

	if(pa_dbf.query(pa_db, query_cols, 0, query_vals, result_cols, n_query_cols,
			   n_result_cols, 0, &result)
			< 0) {
		LM_ERR("Can't query db\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return (-1);
	}

	if(result == NULL)
		return (-1);

	nr_rows = RES_ROW_N(result);

	LM_DBG("found %d matching dialogs\n", nr_rows);

	if(nr_rows <= 0) {
		pa_dbf.free_result(pa_db, result);
		return 0;
	}

	rows = RES_ROWS(result);
	/* get the results and fill in return data structure */
	for(loop = 0; loop < nr_rows; loop++) {
		row_vals = ROW_VALUES(&rows[loop]);

		memset(&s, 0, sizeof(subs_t));
		s.status = subs->status;

		s.reason.s = subs->reason.s;
		s.reason.len = s.reason.s ? strlen(s.reason.s) : 0; //>>>>>>>>>>

		s.pres_uri.s = (char *)row_vals[r_pres_uri_col].val.string_val;
		s.pres_uri.len = s.pres_uri.s ? strlen(s.pres_uri.s) : 0;

		s.to_user.s = (char *)row_vals[r_to_user_col].val.string_val;
		s.to_user.len = s.to_user.s ? strlen(s.to_user.s) : 0;

		s.to_domain.s = (char *)row_vals[r_to_domain_col].val.string_val;
		s.to_domain.len = s.to_domain.s ? strlen(s.to_domain.s) : 0;

		s.from_user.s = (char *)row_vals[r_from_user_col].val.string_val;
		s.from_user.len = s.from_user.s ? strlen(s.from_user.s) : 0;

		s.from_domain.s = (char *)row_vals[r_from_domain_col].val.string_val;
		s.from_domain.len = s.from_domain.s ? strlen(s.from_domain.s) : 0;

		s.watcher_user.s = (char *)row_vals[r_watcher_user_col].val.string_val;
		s.watcher_user.len = s.watcher_user.s ? strlen(s.watcher_user.s) : 0;

		s.watcher_domain.s =
				(char *)row_vals[r_watcher_domain_col].val.string_val;
		s.watcher_domain.len =
				s.watcher_domain.s ? strlen(s.watcher_domain.s) : 0;

		s.event_id.s = (char *)row_vals[r_event_id_col].val.string_val;
		s.event_id.len = (s.event_id.s) ? strlen(s.event_id.s) : 0;

		s.to_tag.s = (char *)row_vals[r_to_tag_col].val.string_val;
		s.to_tag.len = s.to_tag.s ? strlen(s.to_tag.s) : 0;

		s.from_tag.s = (char *)row_vals[r_from_tag_col].val.string_val;
		s.from_tag.len = s.from_tag.s ? strlen(s.from_tag.s) : 0;

		s.callid.s = (char *)row_vals[r_callid_col].val.string_val;
		s.callid.len = s.callid.s ? strlen(s.callid.s) : 0;

		s.record_route.s = (char *)row_vals[r_record_route_col].val.string_val;
		s.record_route.len = (s.record_route.s) ? strlen(s.record_route.s) : 0;

		s.contact.s = (char *)row_vals[r_contact_col].val.string_val;
		s.contact.len = s.contact.s ? strlen(s.contact.s) : 0;

		s.sockinfo_str.s = (char *)row_vals[r_sockinfo_col].val.string_val;
		s.sockinfo_str.len = s.sockinfo_str.s ? strlen(s.sockinfo_str.s) : 0;

		s.local_contact.s =
				(char *)row_vals[r_local_contact_col].val.string_val;
		s.local_contact.len = s.local_contact.s ? strlen(s.local_contact.s) : 0;

		ev_sname.s = (char *)row_vals[r_event_col].val.string_val;
		ev_sname.len = ev_sname.s ? strlen(ev_sname.s) : 0;

		s.event = contains_event(&ev_sname, NULL);

		if(s.event == NULL) {
			LM_ERR("event not found and set to NULL\n");
		}

		s.local_cseq = row_vals[r_local_cseq_col].val.int_val;

		s.expires = row_vals[r_expires_col].val.int_val;

		if(s.expires > (int)time(NULL) + pres_expires_offset)
			s.expires -= (int)time(NULL);
		else
			s.expires = 0;

		s.version = row_vals[r_version_col].val.int_val;

		s.flags = row_vals[r_flags_col].val.int_val;
		s.user_agent.s = (char *)row_vals[r_user_agent_col].val.string_val;
		s.user_agent.len = (s.user_agent.s) ? strlen(s.user_agent.s) : 0;


		cs = mem_copy_subs(&s, PKG_MEM_TYPE);
		if(cs == NULL) {
			LM_ERR("while copying subs_t structure\n");
			/* tidy up and return */
			pa_dbf.free_result(pa_db, result);
			return (-1);
		}
		cs->local_cseq++;
		cs->next = (*subs_array);
		(*subs_array) = cs;

		printf_subs(cs);
	}

	pa_dbf.free_result(pa_db, result);

	if(pres_notifier_processes == 0 && subs->status == TERMINATED_STATUS) {
		/* delete the records */
		if(pa_dbf.delete(pa_db, query_cols, 0, query_vals, n_query_cols) < 0) {
			LM_ERR("sql delete failed\n");
			return (-1);
		}

		return (0);
	}

	/* otherwise we update the records */
	db_cols[n_update_cols] = &str_status_col;
	db_vals[n_update_cols].type = DB1_INT;
	db_vals[n_update_cols].nul = 0;
	db_vals[n_update_cols].val.int_val = subs->status;
	n_update_cols++;

	db_cols[n_update_cols] = &str_reason_col;
	db_vals[n_update_cols].type = DB1_STR;
	db_vals[n_update_cols].nul = 0;
	db_vals[n_update_cols].val.str_val = subs->reason;
	n_update_cols++;

	db_cols[n_update_cols] = &str_updated_col;
	db_vals[n_update_cols].type = DB1_INT;
	db_vals[n_update_cols].nul = 0;
	if(subs->callid.len == 0 || subs->from_tag.len == 0) {
		db_vals[n_update_cols].val.int_val =
				(int)((kam_rand() / (KAM_RAND_MAX + 1.0))
						* (pres_waitn_time * pres_notifier_poll_rate
								  * pres_notifier_processes));
	} else {
		db_vals[n_update_cols].val.int_val =
				core_case_hash(&subs->callid, &subs->from_tag, 0)
				% (pres_waitn_time * pres_notifier_poll_rate
						  * pres_notifier_processes);
	}
	n_update_cols++;

	if(pa_dbf.update(pa_db, query_cols, 0, query_vals, db_cols, db_vals,
			   n_query_cols, n_update_cols)
			< 0) {
		LM_ERR("DB update failed\n");
		return (-1);
	}

	return (0);
}

/********************************************************************************/

static int update_pw_dialogs(
		subs_t *subs, unsigned int hash_code, subs_t **subs_array)
{
	subs_t *s, *ps, *cs;
	int i = 0;

	LM_DBG("start\n");

	if(pres_subs_dbmode == DB_ONLY) {
		return (update_pw_dialogs_dbonlymode(subs, subs_array));
	}

	lock_get(&subs_htable[hash_code].lock);

	ps = subs_htable[hash_code].entries;

	while(ps && ps->next) {
		s = ps->next;

		if(s->event == subs->event && s->pres_uri.len == subs->pres_uri.len
				&& s->watcher_user.len == subs->watcher_user.len
				&& s->watcher_domain.len == subs->watcher_domain.len
				&& presence_sip_uri_match(&s->pres_uri, &subs->pres_uri) == 0
				&& presence_sip_uri_match(&s->watcher_user, &subs->watcher_user)
						   == 0
				&& presence_sip_uri_match(
						   &s->watcher_domain, &subs->watcher_domain)
						   == 0) {
			i++;
			s->status = subs->status;
			s->reason = subs->reason;
			s->db_flag = UPDATEDB_FLAG;

			cs = mem_copy_subs(s, PKG_MEM_TYPE);
			if(cs == NULL) {
				LM_ERR("copying subs_t structure\n");
				lock_release(&subs_htable[hash_code].lock);
				return -1;
			}
			cs->local_cseq++;
			cs->expires -= (int)time(NULL);
			cs->next = (*subs_array);
			(*subs_array) = cs;
			if(subs->status == TERMINATED_STATUS) {
				ps->next = s->next;
				shm_free(s->contact.s);
				shm_free(s);
				LM_DBG(" deleted terminated dialog from hash table\n");
			} else
				ps = s;

			printf_subs(cs);
		} else
			ps = s;
	}

	LM_DBG("found %d matching dialogs\n", i);
	lock_release(&subs_htable[hash_code].lock);

	return 0;
}

static int w_pres_auth_status(struct sip_msg *_msg, char *_sp1, char *_sp2)
{
	str watcher_uri, presentity_uri;

	if(fixup_get_svalue(_msg, (gparam_t *)_sp1, &watcher_uri) != 0) {
		LM_ERR("invalid watcher uri parameter");
		return -1;
	}

	if(fixup_get_svalue(_msg, (gparam_t *)_sp2, &presentity_uri) != 0) {
		LM_ERR("invalid presentity uri parameter");
		return -1;
	}

	if(watcher_uri.len == 0 || watcher_uri.s == NULL) {
		LM_ERR("missing watcher uri\n");
		return -1;
	}

	if(presentity_uri.len == 0 || presentity_uri.s == NULL) {
		LM_DBG("missing presentity uri\n");
		return -1;
	}

	return pres_auth_status(_msg, watcher_uri, presentity_uri);
}

int ki_pres_auth_status(sip_msg_t *msg, str *watcher_uri, str *presentity_uri)
{
	if(watcher_uri == NULL || presentity_uri == NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	return pres_auth_status(msg, *watcher_uri, *presentity_uri);
}

int pres_auth_status(struct sip_msg *msg, str watcher_uri, str presentity_uri)
{
	str event;
	struct sip_uri uri;
	pres_ev_t *ev;
	str *rules_doc = NULL;
	subs_t subs;
	int res;

	event.s = "presence";
	event.len = 8;

	ev = contains_event(&event, NULL);
	if(ev == NULL) {
		LM_ERR("event is not registered\n");
		return -1;
	}
	if(ev->get_rules_doc == NULL) {
		LM_DBG("event does not require authorization");
		return ACTIVE_STATUS;
	}
	if(parse_uri(presentity_uri.s, presentity_uri.len, &uri) < 0) {
		LM_ERR("failed to parse presentity uri\n");
		return -1;
	}
	res = ev->get_rules_doc(&uri.user, &uri.host, &rules_doc);
	if((res < 0) || (rules_doc == NULL) || (rules_doc->s == NULL)) {
		LM_DBG("no xcap rules doc found for presentity uri\n");
		return PENDING_STATUS;
	}

	if(parse_uri(watcher_uri.s, watcher_uri.len, &uri) < 0) {
		LM_ERR("failed to parse watcher uri\n");
		goto err;
	}

	subs.watcher_user = uri.user;
	subs.watcher_domain = uri.host;
	subs.pres_uri = presentity_uri;
	subs.auth_rules_doc = rules_doc;
	if(ev->get_auth_status(&subs) < 0) {
		LM_ERR("getting status from rules document\n");
		goto err;
	}
	LM_DBG("auth status of watcher <%.*s> on presentity <%.*s> is %d\n",
			watcher_uri.len, watcher_uri.s, presentity_uri.len,
			presentity_uri.s, subs.status);
	pkg_free(rules_doc->s);
	pkg_free(rules_doc);
	if((subs.reason.len == 12)
			&& (strncmp(subs.reason.s, "polite-block", 12) == 0))
		return POLITE_BLOCK_STATUS;
	return subs.status;

err:
	pkg_free(rules_doc->s);
	pkg_free(rules_doc);
	return -1;
}

/**
 * wrapper for pres_refresh_watchers to use in config
 */
static int w_pres_refresh_watchers(
		struct sip_msg *msg, char *puri, char *pevent, char *ptype)
{
	str pres_uri;
	str event;
	int refresh_type;

	if(fixup_get_svalue(msg, (gparam_p)puri, &pres_uri) != 0) {
		LM_ERR("invalid uri parameter");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pevent, &event) != 0) {
		LM_ERR("invalid uri parameter");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)ptype, &refresh_type) != 0) {
		LM_ERR("no type value\n");
		return -1;
	}

	if(refresh_type == 2) {
		LM_ERR("Wrong number of parameters for type 2\n");
		return -1;
	}

	if(pres_refresh_watchers(&pres_uri, &event, refresh_type, NULL, NULL) < 0)
		return -1;

	return 1;
}

static int w_pres_refresh_watchers5(struct sip_msg *msg, char *puri,
		char *pevent, char *ptype, char *furi, char *fname)
{
	str pres_uri, event, file_uri, filename;
	int refresh_type;

	if(fixup_get_svalue(msg, (gparam_p)puri, &pres_uri) != 0) {
		LM_ERR("invalid uri parameter");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pevent, &event) != 0) {
		LM_ERR("invalid event parameter");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)ptype, &refresh_type) != 0) {
		LM_ERR("no type value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)furi, &file_uri) != 0) {
		LM_ERR("invalid file uri parameter");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)fname, &filename) != 0) {
		LM_ERR("invalid filename parameter");
		return -1;
	}

	if(refresh_type != 2) {
		LM_ERR("Wrong number of parameters for type %d\n", refresh_type);
		return -1;
	}

	if(pres_refresh_watchers(
			   &pres_uri, &event, refresh_type, &file_uri, &filename)
			< 0)
		return -1;

	return 1;
}

/**
 * fixup for w_pres_refresh_watchers
 */
static int fixup_refresh_watchers(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	} else if(param_no == 2) {
		return fixup_spve_null(param, 1);
	} else if(param_no == 3) {
		return fixup_igp_null(param, 1);
	} else if(param_no == 4) {
		return fixup_spve_null(param, 1);
	} else if(param_no == 5) {
		return fixup_spve_null(param, 1);
	}

	return 0;
}


/**
 * wrapper for update_watchers_status to use via kemi
 */
static int ki_pres_update_watchers(
		struct sip_msg *msg, str *pres_uri, str *event)
{
	pres_ev_t *ev;
	struct sip_uri uri;
	str *rules_doc = NULL;
	int ret;

	ev = contains_event(event, NULL);
	if(ev == NULL) {
		LM_ERR("event %.*s is not registered\n", event->len, event->s);
		return -1;
	}
	if(ev->get_rules_doc == NULL) {
		LM_DBG("event  %.*s does not provide rules doc API\n", event->len,
				event->s);
		return -1;
	}
	if(parse_uri(pres_uri->s, pres_uri->len, &uri) < 0) {
		LM_ERR("failed to parse presentity uri [%.*s]\n", pres_uri->len,
				pres_uri->s);
		return -1;
	}
	ret = ev->get_rules_doc(&uri.user, &uri.host, &rules_doc);
	if((ret < 0) || (rules_doc == NULL) || (rules_doc->s == NULL)) {
		LM_DBG("no xcap rules doc found for presentity uri [%.*s]\n",
				pres_uri->len, pres_uri->s);
		if(rules_doc != NULL)
			pkg_free(rules_doc);
		return -1;
	}
	ret = 1;
	if(update_watchers_status(pres_uri, ev, rules_doc) < 0) {
		LM_ERR("updating watchers in presence\n");
		ret = -1;
	}

	pkg_free(rules_doc->s);
	pkg_free(rules_doc);

	return ret;
}

/**
 * wrapper for update_watchers_status to use in config
 */
static int w_pres_update_watchers(struct sip_msg *msg, char *puri, char *pevent)
{
	str pres_uri;
	str event;

	if(fixup_get_svalue(msg, (gparam_p)puri, &pres_uri) != 0) {
		LM_ERR("invalid uri parameter");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pevent, &event) != 0) {
		LM_ERR("invalid uri parameter");
		return -1;
	}
	return ki_pres_update_watchers(msg, &pres_uri, &event);
}
/**
 * fixup for w_pres_update_watchers
 */
static int fixup_update_watchers(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	} else if(param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

/*! \brief
 *  rpc cmd: presence.refreshWatchers
 *			\<presentity_uri>
 *			\<event>
 *          \<refresh_type> // can be:  = 0 -> watchers authentification type or
 *									  != 0 -> publish type //
 *		* */
void rpc_presence_refresh_watchers(rpc_t *rpc, void *ctx)
{
	str pres_uri = {0, 0};
	str event = {0, 0};
	str file_uri = {0, 0};
	str filename = {0, 0};
	unsigned int refresh_type;
	int pn;

	LM_DBG("initiation refresh of watchers\n");

	pn = rpc->scan(ctx, "SSu*SS", &pres_uri, &event, &refresh_type, &file_uri,
			&filename);
	if(pn < 3) {
		rpc->fault(ctx, 500, "Not enough parameters");
		return;
	}

	if(pres_uri.s == NULL || pres_uri.len == 0) {
		LM_ERR("empty uri\n");
		rpc->fault(ctx, 500, "Empty presentity URI");
		return;
	}

	if(event.s == NULL || event.len == 0) {
		LM_ERR("empty event parameter\n");
		rpc->fault(ctx, 500, "Empty event parameter");
		return;
	}
	LM_DBG("event '%.*s'\n", event.len, event.s);

	if(refresh_type == 2) {
		if(pn < 5) {
			LM_ERR("empty file uri or name parameters\n");
			rpc->fault(ctx, 500, "No file uri or name parameters");
			return;
		}
		if(file_uri.s == NULL || file_uri.len == 0) {
			LM_ERR("empty file uri parameter\n");
			rpc->fault(ctx, 500, "Empty file uri parameter");
			return;
		}

		if(filename.s == NULL || filename.len == 0) {
			LM_ERR("empty file name parameter\n");
			rpc->fault(ctx, 500, "Empty file name parameter");
			return;
		}
	}

	if(pres_refresh_watchers(&pres_uri, &event, refresh_type,
			   file_uri.len ? &file_uri : NULL, filename.len ? &filename : NULL)
			< 0) {
		rpc->fault(ctx, 500, "Execution failed");
		return;
	}
}

static const char *rpc_presence_refresh_watchers_doc[2] = {
	"Trigger refresh of watchers",
	0
};

/*! \brief
 *  rpc cmd: presence.updateWatchers
 *			\<presentity_uri>
 *			\<event>
 *		* */
void rpc_presence_update_watchers(rpc_t *rpc, void *ctx)
{
	str pres_uri = {0, 0};
	str event = {0, 0};
	int pn;

	LM_DBG("init update of watchers\n");

	pn = rpc->scan(ctx, "SS", &pres_uri, &event);
	if(pn < 2) {
		rpc->fault(ctx, 500, "Not enough parameters");
		return;
	}

	if(pres_uri.s == NULL || pres_uri.len == 0) {
		LM_ERR("empty uri\n");
		rpc->fault(ctx, 500, "Empty presentity URI");
		return;
	}

	if(event.s == NULL || event.len == 0) {
		LM_ERR("empty event parameter\n");
		rpc->fault(ctx, 500, "Empty event parameter");
		return;
	}
	LM_DBG("uri '%.*s' - event '%.*s'\n", pres_uri.len, pres_uri.s,
			event.len, event.s);

	if(ki_pres_update_watchers(NULL, &pres_uri, &event)<0) {
		rpc->fault(ctx, 500, "Processing error");
		return;
	}
}

static const char *rpc_presence_update_watchers_doc[2] = {
	"Trigger update of watchers",
	0
};


void rpc_presence_cleanup(rpc_t *rpc, void *c)
{
	LM_DBG("rpc_presence_cleanup:start\n");

	(void)ps_watchers_db_timer_clean(0, 0);
	(void)ps_presentity_db_timer_clean(0, 0);
	(void)ps_ptable_timer_clean(0, 0);
	(void)timer_db_update(0, 0);

	rpc->rpl_printf(c, "Reload OK");
	return;
}

static const char *rpc_presence_cleanup_doc[3] = {
	"Manually triggers the cleanup functions for the active_watchers, "
	"presentity, and watchers tables.",
	0
};

/*! \brief
 *  rpc cmd: presence.presentity_list
 *			\mode - output attributes control
 *		* */
void rpc_presence_presentity_list(rpc_t *rpc, void *ctx)
{
	str omode = {0, 0};
	int imode = 0;
	int i = 0;
	ps_ptable_t *ptb = NULL;
	ps_presentity_t *ptn = NULL;
	void* th = NULL;
	str pempty = str_init("");

	LM_DBG("listing in memory presentity records\n");

	imode = rpc->scan(ctx, "*S", &omode);
	if(imode < 1) {
		imode = 0;
	} else {
		if(omode.len == 4 && strncmp(omode.s, "full", 4)==0) {
			imode = 1;
		} else {
			imode = 0;
		}
	}
	ptb = ps_ptable_get();
	if(ptb == NULL) {
		return;
	}

	for(i=0; i<ptb->ssize; i++) {
		lock_get(&ptb->slots[i].lock);
		ptn = ptb->slots[i].plist;
		while(ptn!=NULL) {
			/* add record node */
			if (rpc->add(ctx, "{", &th) < 0) {
				rpc->fault(ctx, 500, "Internal error creating rpc");
				lock_release(&ptb->slots[i].lock);
				return;
			}
			/* add common fields */
			if(rpc->struct_add(th, "SSSSSd",
					"user",  &ptn->user,
					"domain", &ptn->domain,
					"event", &ptn->event,
					"etag", &ptn->etag,
					"sender", (ptn->sender.s)?&ptn->sender:&pempty,
					"expires", ptn->expires)<0) {
				rpc->fault(ctx, 500, "Internal error adding item");
				lock_release(&ptb->slots[i].lock);
				return;
			}
			if(imode==1) {
				/* add extra fields */
				if(rpc->struct_add(th, "ddSSd",
						"received_time",  ptn->received_time,
						"priority", ptn->priority,
						"ruid", (ptn->ruid.s)?&ptn->ruid:&pempty,
						"body", (ptn->body.s)?&ptn->body:&pempty,
						"hashid", ptn->hashid)<0) {
					rpc->fault(ctx, 500, "Internal error adding item");
					lock_release(&ptb->slots[i].lock);
					return;
				}
			}
			ptn = ptn->next;
		}
		lock_release(&ptb->slots[i].lock);
	}
	return;
}

static const char *rpc_presence_presentity_list_doc[2] = {
	"Trigger update of watchers",
	0
};

rpc_export_t presence_rpc[] = {
	{"presence.cleanup", rpc_presence_cleanup, rpc_presence_cleanup_doc, 0},
	{"presence.refreshWatchers", rpc_presence_refresh_watchers,
			rpc_presence_refresh_watchers_doc, 0},
	{"presence.updateWatchers", rpc_presence_update_watchers,
			rpc_presence_update_watchers_doc, 0},
	{"presence.presentity_list", rpc_presence_presentity_list,
			rpc_presence_presentity_list_doc, RET_ARRAY},
	{0, 0, 0, 0}
};

static int presence_init_rpc(void)
{
	if(rpc_register_array(presence_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

static int sip_uri_case_sensitive_match(str *s1, str *s2)
{
	if(!s1) {
		LM_ERR("null pointer (s1) in sip_uri_match\n");
		return -1;
	}
	if(!s2) {
		LM_ERR("null pointer (s2) in sip_uri_match\n");
		return -1;
	}
	return strncmp(s1->s, s2->s, s2->len);
}

static int sip_uri_case_insensitive_match(str *s1, str *s2)
{
	if(!s1) {
		LM_ERR("null pointer (s1) in sip_uri_match\n");
		return -1;
	}
	if(!s2) {
		LM_ERR("null pointer (s2) in sip_uri_match\n");
		return -1;
	}
	return strncasecmp(s1->s, s2->s, s2->len);
}

static int fixup_has_subscribers(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	} else if(param_no == 2) {
		return fixup_spve_null(param, 1);
	}

	return 0;
}

static int ki_pres_has_subscribers(sip_msg_t *msg, str *pres_uri, str *wevent)
{
	pres_ev_t *ev;

	ev = contains_event(wevent, NULL);
	if(ev == NULL) {
		LM_ERR("event is not registered\n");
		return -1;
	}

	return get_subscribers_count(msg, *pres_uri, *wevent) > 0 ? 1 : -1;
}

static int w_pres_has_subscribers(sip_msg_t *msg, char *_pres_uri, char *_event)
{
	str presentity_uri, watched_event;

	if(fixup_get_svalue(msg, (gparam_p)_pres_uri, &presentity_uri) != 0) {
		LM_ERR("invalid presentity_uri parameter");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)_event, &watched_event) != 0) {
		LM_ERR("invalid watched_event parameter");
		return -1;
	}

	return ki_pres_has_subscribers(msg, &presentity_uri, &watched_event);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_presence_exports[] = {
	{ str_init("presence"), str_init("handle_publish"),
		SR_KEMIP_INT, ki_handle_publish,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("presence"), str_init("handle_publish_uri"),
		SR_KEMIP_INT, ki_handle_publish_uri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("presence"), str_init("handle_subscribe"),
		SR_KEMIP_INT, handle_subscribe0,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("presence"), str_init("handle_subscribe_uri"),
		SR_KEMIP_INT, handle_subscribe_uri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("presence"), str_init("pres_refresh_watchers"),
		SR_KEMIP_INT, ki_pres_refresh_watchers,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("presence"), str_init("pres_refresh_watchers_file"),
		SR_KEMIP_INT, ki_pres_refresh_watchers_file,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE }
	},
	{ str_init("presence"), str_init("pres_update_watchers"),
		SR_KEMIP_INT, ki_pres_update_watchers,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("presence"), str_init("pres_has_subscribers"),
		SR_KEMIP_INT, ki_pres_has_subscribers,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("presence"), str_init("pres_auth_status"),
		SR_KEMIP_INT, ki_pres_auth_status,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_presence_exports);
	return 0;
}
