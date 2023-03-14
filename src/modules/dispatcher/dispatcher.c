/**
 * dispatcher module - load balancing
 *
 * Copyright (C) 2004-2005 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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
 */

/*! \file
 * \ingroup dispatcher
 * \brief Dispatcher :: Dispatch
 */

/*! \defgroup dispatcher Dispatcher :: Load balancing and failover module
 * 	The dispatcher module implements a set of functions for distributing SIP requests on a
 *	set of servers, but also grouping of server resources.
 *
 *	- The module has an internal API exposed to other modules.
 *	- The module implements a couple of MI functions for managing the list of server resources
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/ut.h"
#include "../../core/route.h"
#include "../../core/timer_proc.h"
#include "../../core/mem/mem.h"
#include "../../core/mod_fix.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "ds_ht.h"
#include "dispatch.h"
#include "config.h"
#include "api.h"

MODULE_VERSION

/* clang-format off */
#define DS_SET_ID_COL			"setid"
#define DS_DEST_URI_COL			"destination"
#define DS_DEST_FLAGS_COL		"flags"
#define DS_DEST_PRIORITY_COL	"priority"
#define DS_DEST_ATTRS_COL		"attrs"
#define DS_TABLE_NAME			"dispatcher"

/** parameters */
char *dslistfile = CFG_DIR"dispatcher.list";
int  ds_force_dst   = 1;
int  ds_flags       = 0;
int  ds_use_default = 0;
str ds_xavp_dst = str_init("_dsdst_");
int ds_xavp_dst_mode = 0;
str ds_xavp_ctx = str_init("_dsctx_");
int ds_xavp_ctx_mode = 0;

str hash_pvar_param = STR_NULL;

str ds_xavp_dst_addr = str_init("uri");
str ds_xavp_dst_grp = str_init("grp");
str ds_xavp_dst_dstid = str_init("dstid");
str ds_xavp_dst_attrs = str_init("attrs");
str ds_xavp_dst_sock = str_init("sock");
str ds_xavp_dst_socket = str_init("socket");
str ds_xavp_dst_sockname = str_init("sockname");

str ds_xavp_ctx_cnt = str_init("cnt");


pv_elem_t * hash_param_model = NULL;

int probing_threshold = 1; /* number of failed requests, before a destination
							* is taken into probing */
int inactive_threshold = 1; /* number of replied requests, before a destination
							 * is taken into back in active state */
str ds_ping_method = str_init("OPTIONS");
str ds_ping_from   = str_init("sip:dispatcher@localhost");
static int ds_ping_interval = 0;
int ds_ping_latency_stats = 0;
int ds_latency_estimator_alpha_i = 900;
float ds_latency_estimator_alpha = 0.9f;
int ds_probing_mode = DS_PROBE_NONE;

static str ds_ping_reply_codes_str= STR_NULL;
static int** ds_ping_reply_codes = NULL;
static int* ds_ping_reply_codes_cnt;

str ds_default_socket = STR_NULL;
str ds_default_sockname = STR_NULL;
struct socket_info * ds_default_sockinfo = NULL;

int ds_hash_size = 0;
int ds_hash_expire = 7200;
int ds_hash_initexpire = 7200;
int ds_hash_check_interval = 30;
int ds_timer_mode = 0;
int ds_attrs_none = 0;
int ds_load_mode = 0;
uint32_t ds_dns_mode = DS_DNS_MODE_INIT;
static int ds_dns_interval = 600;
int ds_dns_ttl = 0;

str ds_outbound_proxy = STR_NULL;

/* tm */
struct tm_binds tmb;

/*db */
str ds_db_url            = STR_NULL;
str ds_set_id_col        = str_init(DS_SET_ID_COL);
str ds_dest_uri_col      = str_init(DS_DEST_URI_COL);
str ds_dest_flags_col    = str_init(DS_DEST_FLAGS_COL);
str ds_dest_priority_col = str_init(DS_DEST_PRIORITY_COL);
str ds_dest_attrs_col    = str_init(DS_DEST_ATTRS_COL);
str ds_table_name        = str_init(DS_TABLE_NAME);

str ds_setid_pvname   = STR_NULL;
pv_spec_t ds_setid_pv;
str ds_attrs_pvname   = STR_NULL;
pv_spec_t ds_attrs_pv;

str ds_event_callback = STR_NULL;
str ds_db_extra_attrs = STR_NULL;
param_t *ds_db_extra_attrs_list = NULL;

static int ds_reload_delta = 5;
static time_t *ds_rpc_reload_time = NULL;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int ds_parse_reply_codes();
static int ds_init_rpc(void);

static int w_ds_select(sip_msg_t*, char*, char*);
static int w_ds_select_limit(sip_msg_t*, char*, char*, char*);
static int w_ds_select_dst(struct sip_msg*, char*, char*);
static int w_ds_select_dst_limit(struct sip_msg*, char*, char*, char*);
static int w_ds_select_domain(struct sip_msg*, char*, char*);
static int w_ds_select_domain_limit(struct sip_msg*, char*, char*, char*);
static int w_ds_select_routes(sip_msg_t*, char*, char*);
static int w_ds_select_routes_limit(sip_msg_t*, char*, char*, char*);
static int w_ds_next_dst(struct sip_msg*, char*, char*);
static int w_ds_next_domain(struct sip_msg*, char*, char*);
static int w_ds_set_dst(struct sip_msg*, char*, char*);
static int w_ds_set_domain(struct sip_msg*, char*, char*);
static int w_ds_mark_dst0(struct sip_msg*, char*, char*);
static int w_ds_mark_dst1(struct sip_msg*, char*, char*);
static int w_ds_load_unset(struct sip_msg*, char*, char*);
static int w_ds_load_update(struct sip_msg*, char*, char*);

static int w_ds_is_from_list0(struct sip_msg*, char*, char*);
static int w_ds_is_from_list1(struct sip_msg*, char*, char*);
static int w_ds_is_from_list2(struct sip_msg*, char*, char*);
static int w_ds_is_from_list3(struct sip_msg*, char*, char*, char*);
static int w_ds_list_exist(struct sip_msg*, char*, char*);
static int w_ds_reload(struct sip_msg* msg, char*, char*);

static int w_ds_is_active(sip_msg_t *msg, char *pset, char *p2);
static int w_ds_is_active_uri(sip_msg_t *msg, char *pset, char *puri);

static int fixup_ds_is_from_list(void** param, int param_no);
static int fixup_ds_list_exist(void** param,int param_no);

static void destroy(void);

static int ds_warn_fixup(void** param, int param_no);

static int pv_get_dsv(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int pv_parse_dsv(pv_spec_p sp, str *in);

static pv_export_t mod_pvs[] = {
	{ {"dsv", (sizeof("dsv")-1)}, PVT_OTHER, pv_get_dsv, 0,
		pv_parse_dsv, 0, 0, 0 },

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"ds_select",    (cmd_function)w_ds_select,            2,
		fixup_igp_igp, 0, ANY_ROUTE},
	{"ds_select",    (cmd_function)w_ds_select_limit,      3,
		fixup_igp_all, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_select_dst",    (cmd_function)w_ds_select_dst,    2,
		fixup_igp_igp, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_select_dst",    (cmd_function)w_ds_select_dst_limit,    3,
		fixup_igp_all, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_select_domain", (cmd_function)w_ds_select_domain, 2,
		fixup_igp_igp, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_select_domain", (cmd_function)w_ds_select_domain_limit, 3,
		fixup_igp_all, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_select_routes", (cmd_function)w_ds_select_routes, 2,
		fixup_spve_spve, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_select_routes", (cmd_function)w_ds_select_routes_limit, 3,
		fixup_spve_spve_igp, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_next_dst",      (cmd_function)w_ds_next_dst,      0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_next_domain",   (cmd_function)w_ds_next_domain,   0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_set_dst",       (cmd_function)w_ds_set_dst,      0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_set_domain",    (cmd_function)w_ds_set_domain,   0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_mark_dst",      (cmd_function)w_ds_mark_dst0,     0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"ds_mark_dst",      (cmd_function)w_ds_mark_dst1,     1,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"ds_is_from_list",  (cmd_function)w_ds_is_from_list0, 0,
		0, 0, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"ds_is_from_list",  (cmd_function)w_ds_is_from_list1, 1,
		fixup_igp_null, 0, ANY_ROUTE},
	{"ds_is_from_list",  (cmd_function)w_ds_is_from_list2, 2,
		fixup_ds_is_from_list, 0, ANY_ROUTE},
	{"ds_is_from_list",  (cmd_function)w_ds_is_from_list3, 3,
		fixup_ds_is_from_list, 0, ANY_ROUTE},
	{"ds_list_exist",  (cmd_function)w_ds_list_exist, 1,
		fixup_ds_list_exist, 0, ANY_ROUTE},
	{"ds_list_exists",  (cmd_function)w_ds_list_exist, 1,
		fixup_ds_list_exist, 0, ANY_ROUTE},
	{"ds_load_unset",    (cmd_function)w_ds_load_unset,   0,
		0, 0, ANY_ROUTE},
	{"ds_load_update",   (cmd_function)w_ds_load_update,  0,
		0, 0, ANY_ROUTE},
	{"ds_is_active",  (cmd_function)w_ds_is_active, 1,
		fixup_igp_null, fixup_free_igp_null, ANY_ROUTE},
	{"ds_is_active",  (cmd_function)w_ds_is_active_uri, 2,
		fixup_igp_spve, fixup_free_igp_spve, ANY_ROUTE},
	{"bind_dispatcher",   (cmd_function)bind_dispatcher,  0,
		0, 0, 0},
	{"ds_reload", (cmd_function)w_ds_reload, 0,
		0, 0, ANY_ROUTE},
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{"list_file",       PARAM_STRING, &dslistfile},
	{"db_url",		    PARAM_STR, &ds_db_url},
	{"table_name", 	    PARAM_STR, &ds_table_name},
	{"setid_col",       PARAM_STR, &ds_set_id_col},
	{"destination_col", PARAM_STR, &ds_dest_uri_col},
	{"flags_col",       PARAM_STR, &ds_dest_flags_col},
	{"priority_col",    PARAM_STR, &ds_dest_priority_col},
	{"attrs_col",       PARAM_STR, &ds_dest_attrs_col},
	{"force_dst",       INT_PARAM, &ds_force_dst},
	{"flags",           INT_PARAM, &ds_flags},
	{"use_default",     INT_PARAM, &ds_use_default},
	{"xavp_dst",        PARAM_STR, &ds_xavp_dst},
	{"xavp_dst_mode",   PARAM_INT, &ds_xavp_dst_mode},
	{"xavp_ctx",        PARAM_STR, &ds_xavp_ctx},
	{"xavp_ctx_mode",   PARAM_INT, &ds_xavp_ctx_mode},
	{"hash_pvar",       PARAM_STR, &hash_pvar_param},
	{"setid_pvname",    PARAM_STR, &ds_setid_pvname},
	{"attrs_pvname",    PARAM_STR, &ds_attrs_pvname},
	{"ds_probing_threshold", INT_PARAM, &probing_threshold},
	{"ds_inactive_threshold", INT_PARAM, &inactive_threshold},
	{"ds_ping_method",     PARAM_STR, &ds_ping_method},
	{"ds_ping_from",       PARAM_STR, &ds_ping_from},
	{"ds_ping_interval",   INT_PARAM, &ds_ping_interval},
	{"ds_ping_latency_stats", INT_PARAM, &ds_ping_latency_stats},
	{"ds_latency_estimator_alpha", INT_PARAM, &ds_latency_estimator_alpha_i},
	{"ds_ping_reply_codes", PARAM_STR, &ds_ping_reply_codes_str},
	{"ds_probing_mode",    INT_PARAM, &ds_probing_mode},
	{"ds_hash_size",       INT_PARAM, &ds_hash_size},
	{"ds_hash_expire",     INT_PARAM, &ds_hash_expire},
	{"ds_hash_initexpire", INT_PARAM, &ds_hash_initexpire},
	{"ds_hash_check_interval", INT_PARAM, &ds_hash_check_interval},
	{"outbound_proxy",     PARAM_STR, &ds_outbound_proxy},
	{"ds_default_socket",  PARAM_STR, &ds_default_socket},
	{"ds_default_sockname",PARAM_STR, &ds_default_sockname},
	{"ds_timer_mode",      PARAM_INT, &ds_timer_mode},
	{"event_callback",     PARAM_STR, &ds_event_callback},
	{"ds_attrs_none",      PARAM_INT, &ds_attrs_none},
	{"ds_db_extra_attrs",  PARAM_STR, &ds_db_extra_attrs},
	{"ds_load_mode",       PARAM_INT, &ds_load_mode},
	{"reload_delta",       PARAM_INT, &ds_reload_delta },
	{"ds_dns_mode",        PARAM_INT, &ds_dns_mode},
	{"ds_dns_interval",    PARAM_INT, &ds_dns_interval},
	{"ds_dns_ttl",         PARAM_INT, &ds_dns_ttl},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"dispatcher",    /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* exported rpc functions */
	mod_pvs,         /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	destroy          /* module destroy function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{
	str host;
	int port, proto;
	param_hooks_t phooks;
	param_t *pit = NULL;

	if(ds_dns_mode & DS_DNS_MODE_TIMER) {
		if(ds_dns_interval<=0) {
			LM_WARN("dns interval parameter not set - using 600\n");
			ds_dns_interval = 600;
		}
		if(sr_wtimer_add(ds_dns_timer, NULL, ds_dns_interval) < 0) {
			return -1;
		}
	}
	if(ds_dns_ttl<0) {
		ds_dns_ttl = 0;
	}
	if(ds_ping_active_init() < 0) {
		return -1;
	}

	if(ds_init_rpc() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(cfg_declare("dispatcher", dispatcher_cfg_def, &default_dispatcher_cfg,
			   cfg_sizeof(dispatcher), &dispatcher_cfg)) {
		LM_ERR("Fail to declare the configuration\n");
		return -1;
	}

	/* Initialize the counter */
	ds_ping_reply_codes = (int **)shm_malloc(sizeof(unsigned int *));
	if(!(ds_ping_reply_codes))
	{
		SHM_MEM_ERROR;
		return -1;
	}
	*ds_ping_reply_codes = 0;
	ds_ping_reply_codes_cnt = (int *)shm_malloc(sizeof(int));
	if(!(ds_ping_reply_codes_cnt))
	{
		shm_free(ds_ping_reply_codes);
		SHM_MEM_ERROR;
		return -1;
	}
	*ds_ping_reply_codes_cnt = 0;
	if(ds_ping_reply_codes_str.s) {
		cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str) =
				ds_ping_reply_codes_str;
		if(ds_parse_reply_codes() < 0) {
			return -1;
		}
	}
	/* copy threshholds to config */
	cfg_get(dispatcher, dispatcher_cfg, probing_threshold) = probing_threshold;
	cfg_get(dispatcher, dispatcher_cfg, inactive_threshold) =
			inactive_threshold;

	if(ds_default_sockname.s && ds_default_sockname.len > 0) {
		ds_default_sockinfo = ksr_get_socket_by_name(&ds_default_sockname);
		if(ds_default_sockinfo == 0) {
			LM_ERR("non-local socket name <%.*s>\n", ds_default_sockname.len,
					ds_default_sockname.s);
			return -1;
		}
		LM_INFO("default dispatcher socket set by name to <%.*s>\n",
				ds_default_sockname.len, ds_default_sockname.s);
	} else {
		if(ds_default_socket.s && ds_default_socket.len > 0) {
			if(parse_phostport(
					ds_default_socket.s, &host.s, &host.len, &port, &proto)
					!= 0) {
				LM_ERR("bad socket <%.*s>\n", ds_default_socket.len,
						ds_default_socket.s);
				return -1;
			}
			ds_default_sockinfo =
					grep_sock_info(&host, (unsigned short)port, proto);
			if(ds_default_sockinfo == 0) {
				LM_ERR("non-local socket <%.*s>\n", ds_default_socket.len,
						ds_default_socket.s);
				return -1;
			}
			LM_INFO("default dispatcher socket set to <%.*s>\n",
					ds_default_socket.len, ds_default_socket.s);
		}
	}

	if(ds_init_data() != 0)
		return -1;

	if(ds_db_url.s) {
		if(ds_db_extra_attrs.s!=NULL && ds_db_extra_attrs.len>2) {
			if(ds_db_extra_attrs.s[ds_db_extra_attrs.len-1]==';') {
				ds_db_extra_attrs.len--;
			}
			if (parse_params(&ds_db_extra_attrs, CLASS_ANY, &phooks,
						&ds_db_extra_attrs_list)<0) {
				LM_ERR("failed to parse extra attrs parameter\n");
				return -1;
			}
			for(pit = ds_db_extra_attrs_list; pit!=NULL; pit=pit->next) {
				if(pit->body.s==NULL || pit->body.len<=0) {
					LM_ERR("invalid db extra attrs parameter\n");
					return -1;
				}
			}
		}
		if(ds_init_db() != 0) {
			LM_ERR("could not initiate a connect to the database\n");
			return -1;
		}
	} else {
		if(ds_load_list(dslistfile) != 0) {
			LM_ERR("no dispatching list loaded from file\n");
			return -1;
		} else {
			LM_DBG("loaded dispatching list\n");
		}
	}

	if(hash_pvar_param.s && *hash_pvar_param.s) {
		if(pv_parse_format(&hash_pvar_param, &hash_param_model) < 0
				|| hash_param_model == NULL) {
			LM_ERR("malformed PV string: %s\n", hash_pvar_param.s);
			return -1;
		}
	} else {
		hash_param_model = NULL;
	}

	if(ds_setid_pvname.s != 0) {
		if(pv_parse_spec(&ds_setid_pvname, &ds_setid_pv) == NULL
				|| !pv_is_w(&ds_setid_pv)) {
			LM_ERR("[%s]- invalid setid_pvname\n", ds_setid_pvname.s);
			return -1;
		}
	}

	if(ds_attrs_pvname.s != 0) {
		if(pv_parse_spec(&ds_attrs_pvname, &ds_attrs_pv) == NULL
				|| !pv_is_w(&ds_attrs_pv)) {
			LM_ERR("[%s]- invalid attrs_pvname\n", ds_attrs_pvname.s);
			return -1;
		}
	}

	if(ds_hash_size > 0) {
		if(ds_hash_load_init(
					1 << ds_hash_size, ds_hash_expire, ds_hash_initexpire)
				< 0)
			return -1;
		if(ds_timer_mode == 1) {
			if(sr_wtimer_add(ds_ht_timer, NULL, ds_hash_check_interval) < 0)
				return -1;
		} else {
			if(register_timer(ds_ht_timer, NULL, ds_hash_check_interval)
					< 0)
				return -1;
		}
	}

	/* Only, if the Probing-Timer is enabled the TM-API needs to be loaded: */
	if(ds_ping_interval > 0) {
		/*****************************************************
		 * TM-Bindings
		 *****************************************************/
		if(load_tm_api(&tmb) == -1) {
			LM_ERR("could not load the TM-functions - disable DS ping\n");
			return -1;
		}
		/*****************************************************
		 * Register the PING-Timer
		 *****************************************************/
		if(ds_timer_mode == 1) {
			if(sr_wtimer_add(ds_check_timer, NULL, ds_ping_interval) < 0)
				return -1;
		} else {
			if(register_timer(ds_check_timer, NULL, ds_ping_interval) < 0)
				return -1;
		}
	}
	if (ds_latency_estimator_alpha_i > 0 && ds_latency_estimator_alpha_i < 1000) {
		ds_latency_estimator_alpha = ds_latency_estimator_alpha_i/1000.0f;
	} else {
		LM_ERR("invalid ds_latency_estimator_alpha must be between 0 and 1000,"
				" using default[%.3f]\n", ds_latency_estimator_alpha);
	}

	ds_rpc_reload_time = shm_malloc(sizeof(time_t));
	if(ds_rpc_reload_time == NULL) {
		shm_free(ds_ping_reply_codes);
		shm_free(ds_ping_reply_codes_cnt);
		SHM_MEM_ERROR;
		return -1;
	}
	*ds_rpc_reload_time = 0;

	return 0;
}

/*! \brief
 * Initialize children
 */
static int child_init(int rank)
{
	return 0;
}

/*! \brief
 * destroy function
 */
static void destroy(void)
{
	ds_destroy_list();
	if(ds_db_url.s)
		ds_disconnect_db();
	ds_hash_load_destroy();
	if(ds_ping_reply_codes)
		shm_free(ds_ping_reply_codes);
	if(ds_ping_reply_codes_cnt)
		shm_free(ds_ping_reply_codes_cnt);
	if(ds_rpc_reload_time!=NULL) {
		shm_free(ds_rpc_reload_time);
		ds_rpc_reload_time = 0;
	}
}

#define GET_VALUE(param_name, param, i_value, s_value, value_flags)        \
	do {                                                                   \
		if(get_is_fparam(&(i_value), &(s_value), msg, (fparam_t *)(param), \
				   &(value_flags))                                         \
				!= 0) {                                                    \
			LM_ERR("no %s value\n", (param_name));                         \
			return -1;                                                     \
		}                                                                  \
	} while(0)

/*! \brief
 * parses string to dispatcher dst flags set
 * returns <0 on failure or int with flag on success.
 */
int ds_parse_flags(char *flag_str, int flag_len)
{
	int flag = 0;
	int i;

	for(i = 0; i < flag_len; i++) {
		if(flag_str[i] == 'a' || flag_str[i] == 'A') {
			flag &= ~(DS_STATES_ALL);
		} else if(flag_str[i] == 'i' || flag_str[i] == 'I') {
			flag |= DS_INACTIVE_DST;
		} else if(flag_str[i] == 'd' || flag_str[i] == 'D') {
			flag |= DS_DISABLED_DST;
		} else if(flag_str[i] == 't' || flag_str[i] == 'T') {
			flag |= DS_TRYING_DST;
		} else if(flag_str[i] == 'p' || flag_str[i] == 'P') {
			flag |= DS_PROBING_DST;
		} else {
			flag = -1;
			break;
		}
	}

	return flag;
}

/**
 *
 */
static int w_ds_select_addr(
		sip_msg_t *msg, char *set, char *alg, char *limit, int mode)
{
	unsigned int algo_flags, set_flags, limit_flags;
	str s_algo = STR_NULL;
	str s_set = STR_NULL;
	str s_limit = STR_NULL;
	int a, s, l;
	if(msg == NULL)
		return -1;

	GET_VALUE("destination set", set, s, s_set, set_flags);
	if(!(set_flags & PARAM_INT)) {
		if(set_flags & PARAM_STR)
			LM_ERR("unable to get destination set from [%.*s]\n", s_set.len,
					s_set.s);
		else
			LM_ERR("unable to get destination set\n");
		return -1;
	}
	GET_VALUE("algorithm", alg, a, s_algo, algo_flags);
	if(!(algo_flags & PARAM_INT)) {
		if(algo_flags & PARAM_STR)
			LM_ERR("unable to get algorithm from [%.*s]\n", s_algo.len,
					s_algo.s);
		else
			LM_ERR("unable to get algorithm\n");
		return -1;
	}

	if(limit) {
		GET_VALUE("limit", limit, l, s_limit, limit_flags);
		if(!(limit_flags & PARAM_INT)) {
			if(limit_flags & PARAM_STR)
				LM_ERR("unable to get dst number limit from [%.*s]\n",
						s_limit.len, s_limit.s);
			else
				LM_ERR("unable to get dst number limit\n");
			return -1;
		}
	} else {
		l = -1; /* will be casted to a rather big unsigned value */
	}

	return ds_select_dst_limit(msg, s, a, (unsigned int)l, mode);
}

/**
 *
 */
static int w_ds_select(struct sip_msg *msg, char *set, char *alg)
{
	return w_ds_select_addr(msg, set, alg, 0 /* limit number of dst*/,
					DS_SETOP_XAVP /*set no dst/uri*/);
}

/**
 *
 */
static int w_ds_select_limit(
		struct sip_msg *msg, char *set, char *alg, char *limit)
{
	return w_ds_select_addr(msg, set, alg, limit /* limit number of dst*/,
			DS_SETOP_XAVP /*set no dst/uri*/);
}

/**
 *
 */
static int w_ds_select_dst(struct sip_msg *msg, char *set, char *alg)
{
	return w_ds_select_addr(msg, set, alg, 0 /* limit number of dst*/,
				DS_SETOP_DSTURI /*set dst uri*/);
}

/**
 *
 */
static int w_ds_select_dst_limit(
		struct sip_msg *msg, char *set, char *alg, char *limit)
{
	return w_ds_select_addr(msg, set, alg, limit /* limit number of dst*/,
			DS_SETOP_DSTURI /*set dst uri*/);
}

/**
 *
 */
static int w_ds_select_domain(struct sip_msg *msg, char *set, char *alg)
{
	return w_ds_select_addr(msg, set, alg, 0 /* limit number of dst*/,
			DS_SETOP_RURI /*set host port*/);
}

/**
 *
 */
static int w_ds_select_domain_limit(
		struct sip_msg *msg, char *set, char *alg, char *limit)
{
	return w_ds_select_addr(msg, set, alg, limit /* limit number of dst*/,
			DS_SETOP_RURI /*set host port*/);
}

/**
 *
 */
static int ki_ds_select_routes_limit(sip_msg_t *msg, str *srules, str *smode,
		int rlimit)
{
	int i;
	int vret;
	int gret;
	sr_xval_t nxval;
	ds_select_state_t vstate;

	memset(&vstate, 0, sizeof(ds_select_state_t));
	vstate.limit = (uint32_t)rlimit;
	if(vstate.limit == 0) {
		LM_DBG("Limit set to 0 - forcing to unlimited\n");
		vstate.limit = 0xffffffff;
	}
	vret = -1;
	gret = -1;
	i = 0;
	while(i<srules->len) {
		vstate.setid = 0;
		for(; i<srules->len; i++) {
			if(srules->s[i]<'0' || srules->s[i]>'9') {
				if(srules->s[i]=='=') {
					i++;
					break;
				} else {
					LM_ERR("invalid character in [%.*s] at [%d]\n",
							srules->len, srules->s, i);
					return -1;
				}
			}
			vstate.setid = (vstate.setid * 10) + (srules->s[i] - '0');
		}
		vstate.alg = 0;
		for(; i<srules->len; i++) {
			if(srules->s[i]<'0' || srules->s[i]>'9') {
				if(srules->s[i]==';') {
					i++;
					break;
				} else {
					LM_ERR("invalid character in [%.*s] at [%d]\n",
							srules->len, srules->s, i);
					return -1;
				}
			}
			vstate.alg = (vstate.alg * 10) + (srules->s[i] - '0');
		}
		LM_DBG("routing with setid=%d alg=%d cnt=%d limit=0x%x (%u)\n",
			vstate.setid, vstate.alg, vstate.cnt, vstate.limit, vstate.limit);

		vstate.umode = DS_SETOP_XAVP;
		/* if no r-uri/d-uri was set already, keep using the update mode
		 * specified by the param, then just add to xavps list */
		if(vstate.emode==0) {
			switch(smode->s[0]) {
				case '0':
				case 'd':
				case 'D':
					vstate.umode = DS_SETOP_DSTURI;
				break;
				case '1':
				case 'r':
				case 'R':
					vstate.umode = DS_SETOP_RURI;
				break;
				case '2':
				case 'x':
				case 'X':
				break;
				default:
					LM_ERR("invalid routing mode parameter: %.*s\n",
							smode->len, smode->s);
					return -1;
			}
		}
		vret = ds_manage_routes(msg, &vstate);
		if(vret<0) {
			LM_DBG("failed to select target destinations from %d=%d [%.*s]\n",
					vstate.setid, vstate.alg, srules->len, srules->s);
			/* continue to try other target groups */
		} else {
			if(vret>0) {
				gret = vret;
			}
		}
	}

	if(gret<0) {
		/* no selection of a target address */
		LM_DBG("failed to select any target destinations from [%.*s]\n",
					srules->len, srules->s);
		/* return last failure code when trying to select target addresses */
		return vret;
	}

	/* add cnt value to xavp */
	if(((ds_xavp_ctx_mode & DS_XAVP_CTX_SKIP_CNT)==0)
			&& (ds_xavp_ctx.len >= 0)) {
		/* add to xavp the number of selected dst records */
		memset(&nxval, 0, sizeof(sr_xval_t));
		nxval.type = SR_XTYPE_LONG;
		nxval.v.l = vstate.cnt;
		if(xavp_add_xavp_value(&ds_xavp_ctx, &ds_xavp_ctx_cnt, &nxval, NULL)==NULL) {
			LM_ERR("failed to add cnt value to xavp\n");
			return -1;
		}
	}

	LM_DBG("selected target destinations: %d\n", vstate.cnt);
	return gret;
}

/**
 *
 */
static int ki_ds_select_routes(sip_msg_t *msg, str *srules, str *smode)
{
	return ki_ds_select_routes_limit(msg, srules, smode, 0);
}

/**
 *
 */
static int w_ds_select_routes(sip_msg_t *msg, char *lrules, char *umode)
{
	return w_ds_select_routes_limit(msg, lrules, umode, 0);
}

/**
 *
 */
static int w_ds_select_routes_limit(sip_msg_t *msg, char *lrules, char *umode,
		char *rlimit)
{
	str vrules;
	str vmode;
	int vlimit;

	if(fixup_get_svalue(msg, (gparam_t*)lrules, &vrules)<0) {
		LM_ERR("failed to get routing rules parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)umode, &vmode)<0) {
		LM_ERR("failed to get update mode parameter\n");
		return -1;
	}
	if(rlimit!=NULL) {
		if(fixup_get_ivalue(msg, (gparam_t*)rlimit, &vlimit)<0) {
			LM_ERR("failed to get limit parameter\n");
			return -1;
		}
	} else {
		vlimit = 0;
	}
	return ki_ds_select_routes_limit(msg, &vrules, &vmode, vlimit);
}

/**
 *
 */
static int w_ds_next_dst(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_update_dst(msg, DS_USE_NEXT, DS_SETOP_DSTURI /*set dst uri*/);
}

/**
 *
 */
static int w_ds_next_domain(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_update_dst(msg, DS_USE_NEXT, DS_SETOP_RURI /*set host port*/);
}

/**
 *
 */
static int w_ds_set_dst(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_update_dst(msg, DS_USE_CRT, DS_SETOP_DSTURI /*set dst uri*/);
}

/**
 *
 */
static int w_ds_set_domain(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_update_dst(msg, DS_USE_CRT, DS_SETOP_RURI /*set host port*/);
}

/**
 *
 */
static int ki_ds_mark_dst(sip_msg_t *msg)
{
	int state;

	state = DS_INACTIVE_DST;
	if(ds_probing_mode == DS_PROBE_ALL)
		state |= DS_PROBING_DST;

	return ds_mark_dst(msg, state);
}

/**
 *
 */
static int w_ds_mark_dst0(struct sip_msg *msg, char *str1, char *str2)
{
	return ki_ds_mark_dst(msg);
}

/**
 *
 */
static int ki_ds_mark_dst_state(sip_msg_t *msg, str *sval)
{
	int state;

	if(sval->s == NULL || sval->len == 0)
		return ki_ds_mark_dst(msg);

	state = ds_parse_flags(sval->s, sval->len);

	if(state < 0) {
		LM_WARN("Failed to parse state flags: %.*s", sval->len, sval->s);
		return -1;
	}

	return ds_mark_dst(msg, state);
}

/**
 *
 */
static int w_ds_mark_dst1(struct sip_msg *msg, char *str1, char *str2)
{
	str sval;

	sval.s = str1;
	sval.len = strlen(str1);

	return ki_ds_mark_dst_state(msg, &sval);
}

/**
 *
 */
static int w_ds_load_unset(struct sip_msg *msg, char *str1, char *str2)
{
	if(ds_load_unset(msg) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_ds_load_update(struct sip_msg *msg, char *str1, char *str2)
{
	if(ds_load_update(msg) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int ds_warn_fixup(void **param, int param_no)
{
	if(ds_xavp_dst.len<=0 || ds_xavp_ctx.len<=0) {
		LM_ERR("failover functions used, but required XAVP parameters"
			   " are NULL -- feature disabled\n");
	}
	return 0;
}

static int ds_reload(sip_msg_t *msg)
{
	if(ds_rpc_reload_time==NULL) {
		LM_ERR("not ready for reload\n");
		return -1;
	}
	if(*ds_rpc_reload_time!=0 && *ds_rpc_reload_time > time(NULL) - ds_reload_delta) {
		LM_ERR("ongoing reload\n");
		return -1;
	}
	*ds_rpc_reload_time = time(NULL);

	if(!ds_db_url.s) {
		if(ds_load_list(dslistfile) != 0) {
			LM_ERR("Error reloading from list\n");
			return -1;
		}
	} else {
		if(ds_reload_db() < 0) {
			LM_ERR("Error reloading from db\n");
			return -1;
		}
	}
	LM_DBG("reloaded dispatcher\n");
	return 1;
}


static int w_ds_reload(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_reload(msg);
}

static int w_ds_is_from_list0(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_is_from_list(msg, -1);
}

static int ki_ds_is_from_lists(sip_msg_t *msg)
{
	return ds_is_from_list(msg, -1);
}

static int w_ds_is_from_list1(struct sip_msg *msg, char *set, char *str2)
{
	int s;
	if(fixup_get_ivalue(msg, (gparam_p)set, &s) != 0) {
		LM_ERR("cannot get set id value\n");
		return -1;
	}
	return ds_is_from_list(msg, s);
}

static int w_ds_is_from_list2(struct sip_msg *msg, char *set, char *mode)
{
	int vset;
	int vmode;

	if(fixup_get_ivalue(msg, (gparam_t *)set, &vset) != 0) {
		LM_ERR("cannot get set id value\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)mode, &vmode) != 0) {
		LM_ERR("cannot get mode value\n");
		return -1;
	}

	return ds_is_addr_from_list(msg, vset, NULL, vmode);
}

static int ki_ds_is_from_list_mode(sip_msg_t *msg, int vset, int vmode)
{
	return ds_is_addr_from_list(msg, vset, NULL, vmode);
}

static int w_ds_is_from_list3(
		struct sip_msg *msg, char *set, char *mode, char *uri)
{
	int vset;
	int vmode;
	str suri;

	if(fixup_get_ivalue(msg, (gparam_t *)set, &vset) != 0) {
		LM_ERR("cannot get set id value\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)mode, &vmode) != 0) {
		LM_ERR("cannot get mode value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)uri, &suri) != 0) {
		LM_ERR("cannot get uri value\n");
		return -1;
	}

	return ds_is_addr_from_list(msg, vset, &suri, vmode);
}

static int ki_ds_is_from_list_uri(sip_msg_t *msg, int vset, int vmode, str *vuri)
{
	return ds_is_addr_from_list(msg, vset, vuri, vmode);
}

static int fixup_ds_is_from_list(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2)
		return fixup_igp_null(param, 1);
	if(param_no == 3)
		return fixup_spve_null(param, 1);
	return 0;
}

/* Check if a given set exist in memory */
static int w_ds_list_exist(struct sip_msg *msg, char *param, char *p2)
{
	int set;

	if(fixup_get_ivalue(msg, (gparam_p)param, &set) != 0) {
		LM_ERR("cannot get set id param value\n");
		return -2;
	}
	return ds_list_exist(set);
}

static int ki_ds_list_exists(struct sip_msg *msg, int set)
{
	return ds_list_exist(set);
}

static int fixup_ds_list_exist(void **param, int param_no)
{
	return fixup_igp_null(param, param_no);
}

static int ki_ds_is_active(sip_msg_t *msg, int set)
{
	return ds_is_active_uri(msg, set, NULL);
}

static int w_ds_is_active(sip_msg_t *msg, char *pset, char *p2)
{
	int vset;

	if(fixup_get_ivalue(msg, (gparam_t *)pset, &vset) != 0) {
		LM_ERR("cannot get set id value\n");
		return -1;
	}

	return ds_is_active_uri(msg, vset, NULL);
}

static int ki_ds_is_active_uri(sip_msg_t *msg, int set, str *uri)
{
	return ds_is_active_uri(msg, set, uri);
}

static int w_ds_is_active_uri(sip_msg_t *msg, char *pset, char *puri)
{
	int vset;
	str suri;

	if(fixup_get_ivalue(msg, (gparam_t *)pset, &vset) != 0) {
		LM_ERR("cannot get set id value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)puri, &suri) != 0) {
		LM_ERR("cannot get uri value\n");
		return -1;
	}

	return ki_ds_is_active_uri(msg, vset, &suri);
}

static int ds_parse_reply_codes()
{
	param_t *params_list = NULL;
	param_t *pit = NULL;
	int list_size = 0;
	int i = 0;
	int pos = 0;
	int code = 0;
	str input = {0, 0};
	int *ds_ping_reply_codes_new = NULL;
	int *ds_ping_reply_codes_old = NULL;

	/* validate input string */
	if(cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).s == 0
			|| cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).len
					   <= 0)
		return 0;

	/* parse_params() updates the string pointer of .s -- make a copy */
	input.s = cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).s;
	input.len =
			cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).len;

	if(parse_params(&input, CLASS_ANY, 0, &params_list) < 0)
		return -1;

	/* get the number of entries in the list */
	for(pit = params_list; pit; pit = pit->next) {
		if(pit->name.len == 4 && strncasecmp(pit->name.s, "code", 4) == 0) {
			str2sint(&pit->body, &code);
			if((code >= 100) && (code < 700))
				list_size += 1;
		} else if(pit->name.len == 5
				  && strncasecmp(pit->name.s, "class", 5) == 0) {
			str2sint(&pit->body, &code);
			if((code >= 1) && (code < 7))
				list_size += 1;
		}
	}
	LM_DBG("expecting %d reply codes and classes\n", list_size);

	if(list_size > 0) {
		/* Allocate Memory for the new list: */
		ds_ping_reply_codes_new = (int *)shm_malloc(list_size * sizeof(int));
		if(ds_ping_reply_codes_new == NULL) {
			free_params(params_list);
			SHM_MEM_ERROR;
			return -1;
		}

		/* Now create the list of valid reply-codes: */
		for(pit = params_list; pit; pit = pit->next) {
			if(pit->name.len == 4 && strncasecmp(pit->name.s, "code", 4) == 0) {
				str2sint(&pit->body, &code);
				if((code >= 100) && (code < 700)) {
					ds_ping_reply_codes_new[pos++] = code;
				}
			} else if(pit->name.len == 5
					  && strncasecmp(pit->name.s, "class", 5) == 0) {
				str2sint(&pit->body, &code);
				if((code >= 1) && (code < 7)) {
					ds_ping_reply_codes_new[pos++] = code;
				}
			}
		}
	} else {
		ds_ping_reply_codes_new = 0;
	}
	free_params(params_list);

	if(list_size > *ds_ping_reply_codes_cnt) {
		/* if more reply-codes -- change pointer and then set number of codes */
		ds_ping_reply_codes_old = *ds_ping_reply_codes;
		*ds_ping_reply_codes = ds_ping_reply_codes_new;
		*ds_ping_reply_codes_cnt = list_size;
		if(ds_ping_reply_codes_old)
			shm_free(ds_ping_reply_codes_old);
	} else {
		/* less or equal reply codea -- set the number of codes first */
		*ds_ping_reply_codes_cnt = list_size;
		ds_ping_reply_codes_old = *ds_ping_reply_codes;
		*ds_ping_reply_codes = ds_ping_reply_codes_new;
		if(ds_ping_reply_codes_old)
			shm_free(ds_ping_reply_codes_old);
	}
	/* Print the list as INFO: */
	for(i = 0; i < *ds_ping_reply_codes_cnt; i++) {
		LM_DBG("accepting reply %s %d (%d/%d) as valid\n",
				((*ds_ping_reply_codes)[i]/10)?"code":"class",
				(*ds_ping_reply_codes)[i], (i + 1), *ds_ping_reply_codes_cnt);
	}
	return 0;
}

int ds_ping_check_rplcode(int code)
{
	int i;

	for(i = 0; i < *ds_ping_reply_codes_cnt; i++) {
		if((*ds_ping_reply_codes)[i] / 10) {
			/* reply code */
			if((*ds_ping_reply_codes)[i] == code) {
				return 1;
			}
		} else {
			/* reply class */
			if((*ds_ping_reply_codes)[i] == code / 100) {
				return 1;
			}
		}
	}

	return 0;
}

void ds_ping_reply_codes_update(str *gname, str *name)
{
	ds_parse_reply_codes();
}

/**
 *
 */
static int pv_get_dsv(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	ds_rctx_t *rctx;

	if(param==NULL) {
		return -1;
	}
	rctx = ds_get_rctx();
	if(rctx==NULL) {
		return pv_get_null(msg, param, res);
	}
	switch(param->pvn.u.isname.name.n)
	{
		case 0:
			return pv_get_sintval(msg, param, res, rctx->code);
		case 1:
			if(rctx->reason.s!=NULL && rctx->reason.len>0) {
				return pv_get_strval(msg, param, res, &rctx->reason);
			}
			return pv_get_null(msg, param, res);
		case 2:
			return pv_get_sintval(msg, param, res, rctx->flags);
		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
static int pv_parse_dsv(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 4:
			if(strncmp(in->s, "code", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 5:
			if(strncmp(in->s, "flags", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		case 6:
			if(strncmp(in->s, "reason", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV key: %.*s\n", in->len, in->s);
	return -1;
}

/* KEMI wrappers */
/**
 *
 */
static int ki_ds_select(sip_msg_t *msg, int set, int alg)
{
	return ds_select_dst_limit(msg, set, alg, 0xffff /* limit number of dst*/,
			2 /*set no dst/uri*/);
}

/**
 *
 */
static int ki_ds_select_limit(sip_msg_t *msg, int set, int alg, int limit)
{
	return ds_select_dst_limit(msg, set, alg, limit /* limit number of dst*/,
			2 /*set no dst/uri*/);
}

/**
 *
 */
static int ki_ds_select_dst(sip_msg_t *msg, int set, int alg)
{
	return ds_select_dst_limit(msg, set, alg, 0xffff /* limit number of dst*/,
			0 /*set dst uri*/);
}

/**
 *
 */
static int ki_ds_select_dst_limit(sip_msg_t *msg, int set, int alg, int limit)
{
	return ds_select_dst_limit(msg, set, alg, limit /* limit number of dst*/,
			0 /*set dst uri*/);
}

/**
 *
 */
static int ki_ds_select_domain(sip_msg_t *msg, int set, int alg)
{
	return ds_select_dst_limit(msg, set, alg, 0xffff /* limit number of dst*/,
			1 /*set host port*/);
}

/**
 *
 */
static int ki_ds_select_domain_limit(sip_msg_t *msg, int set, int alg, int limit)
{
	return ds_select_dst_limit(msg, set, alg, limit /* limit number of dst*/,
			1 /*set host port*/);
}

/**
 *
 */
static int ki_ds_next_dst(sip_msg_t *msg)
{
	return ds_update_dst(msg, DS_USE_NEXT, DS_SETOP_DSTURI /*set dst uri*/);
}

/**
 *
 */
static int ki_ds_next_domain(sip_msg_t *msg)
{
	return ds_update_dst(msg, DS_USE_NEXT, DS_SETOP_RURI /*set host port*/);
}

/**
 *
 */
static int ki_ds_set_dst(sip_msg_t *msg)
{
	return ds_update_dst(msg, DS_USE_CRT, DS_SETOP_DSTURI /*set dst uri*/);
}

/**
 *
 */
static int ki_ds_set_domain(sip_msg_t *msg)
{
	return ds_update_dst(msg, DS_USE_CRT, DS_SETOP_RURI /*set host port*/);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_dispatcher_exports[] = {
	{ str_init("dispatcher"), str_init("ds_select"),
		SR_KEMIP_INT, ki_ds_select,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_select_limit"),
		SR_KEMIP_INT, ki_ds_select_limit,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_select_domain"),
		SR_KEMIP_INT, ki_ds_select_domain,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_select_domain_limit"),
		SR_KEMIP_INT, ki_ds_select_domain_limit,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_next_domain"),
		SR_KEMIP_INT, ki_ds_next_domain,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_set_domain"),
		SR_KEMIP_INT, ki_ds_set_domain,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_select_dst"),
		SR_KEMIP_INT, ki_ds_select_dst,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_select_dst_limit"),
		SR_KEMIP_INT, ki_ds_select_dst_limit,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_select_routes"),
		SR_KEMIP_INT, ki_ds_select_routes,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_select_routes_limit"),
		SR_KEMIP_INT, ki_ds_select_routes_limit,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_next_dst"),
		SR_KEMIP_INT, ki_ds_next_dst,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_set_dst"),
		SR_KEMIP_INT, ki_ds_set_dst,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_mark_dst"),
		SR_KEMIP_INT, ki_ds_mark_dst,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_mark_dst_state"),
		SR_KEMIP_INT, ki_ds_mark_dst_state,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_is_from_lists"),
		SR_KEMIP_INT, ki_ds_is_from_lists,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_is_from_list"),
		SR_KEMIP_INT, ds_is_from_list,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_is_from_list_mode"),
		SR_KEMIP_INT, ki_ds_is_from_list_mode,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_is_from_list_uri"),
		SR_KEMIP_INT, ki_ds_is_from_list_uri,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_load_update"),
		SR_KEMIP_INT, ds_load_update,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_load_unset"),
		SR_KEMIP_INT, ds_load_unset,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_reload"),
		SR_KEMIP_INT, ds_reload,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_list_exists"),
		SR_KEMIP_INT, ki_ds_list_exists,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_is_active"),
		SR_KEMIP_INT, ki_ds_is_active,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dispatcher"), str_init("ds_is_active_uri"),
		SR_KEMIP_INT, ki_ds_is_active_uri,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_dispatcher_exports);
	return 0;
}

/*** RPC implementation ***/

static const char *dispatcher_rpc_reload_doc[2] = {
		"Reload dispatcher destination sets", 0};


/*
 * RPC command to reload dispatcher destination sets
 */
static void dispatcher_rpc_reload(rpc_t *rpc, void *ctx)
{

	if(ds_rpc_reload_time==NULL) {
		LM_ERR("not ready for reload\n");
		rpc->fault(ctx, 500, "Not ready for reload");
		return;
	}
	if(*ds_rpc_reload_time!=0 && *ds_rpc_reload_time > time(NULL) - ds_reload_delta) {
		LM_ERR("ongoing reload\n");
		rpc->fault(ctx, 500, "Ongoing reload");
		return;
	}
	*ds_rpc_reload_time = time(NULL);

	if(!ds_db_url.s) {
		if(ds_load_list(dslistfile) != 0) {
			rpc->fault(ctx, 500, "Reload Failed");
			return;
		}
	} else {
		if(ds_reload_db() < 0) {
			rpc->fault(ctx, 500, "Reload Failed");
			return;
		}
	}
	rpc->rpl_printf(ctx, "Ok. Dispatcher successfully reloaded.");
	return;
}


static const char *dispatcher_rpc_list_doc[2] = {
		"Return the content of dispatcher sets", 0};


#define DS_RPC_PRINT_NORMAL 1
#define DS_RPC_PRINT_SHORT  2
#define DS_RPC_PRINT_FULL   3

/**
 *
 */
int ds_rpc_print_set(ds_set_t *node, rpc_t *rpc, void *ctx, void *rpc_handle,
		int mode)
{
	int i = 0, rc = 0;
	void *rh;
	void *sh;
	void *vh;
	void *wh;
	void *lh;
	void *dh;
	int j;
	char c[3];
	str data = STR_NULL;
	char ipbuf[IP_ADDR_MAX_STRZ_SIZE];

	if(!node)
		return 0;

	for(; i < 2; ++i) {
		rc = ds_rpc_print_set(node->next[i], rpc, ctx, rpc_handle, mode);
		if(rc != 0)
			return rc;
	}

	if(rpc->struct_add(rpc_handle, "{", "SET", &sh) < 0) {
		rpc->fault(ctx, 500, "Internal error set structure");
		return -1;
	}
	if(rpc->struct_add(sh, "d[", "ID", node->id, "TARGETS", &rh) < 0) {
		rpc->fault(ctx, 500, "Internal error creating set id");
		return -1;
	}

	for(j = 0; j < node->nr; j++) {
		if(rpc->struct_add(rh, "{", "DEST", &vh) < 0) {
			rpc->fault(ctx, 500, "Internal error creating dest");
			return -1;
		}

		memset(&c, 0, sizeof(c));
		if(node->dlist[j].flags & DS_INACTIVE_DST)
			c[0] = 'I';
		else if(node->dlist[j].flags & DS_DISABLED_DST)
			c[0] = 'D';
		else if(node->dlist[j].flags & DS_TRYING_DST)
			c[0] = 'T';
		else
			c[0] = 'A';

		if(node->dlist[j].flags & DS_PROBING_DST)
			c[1] = 'P';
		else
			c[1] = 'X';

		if(rpc->struct_add(vh, "Ssd", "URI", &node->dlist[j].uri, "FLAGS",
				   c, "PRIORITY", node->dlist[j].priority)
				< 0) {
			rpc->fault(ctx, 500, "Internal error creating dest struct");
			return -1;
		}

		if(mode == DS_RPC_PRINT_FULL) {
			ipbuf[0] = '\0';
			ip_addr2sbufz(&node->dlist[j].ip_address, ipbuf, IP_ADDR_MAX_STRZ_SIZE);
			if(rpc->struct_add(vh, "Ssddjj", "HOST", &node->dlist[j].host,
						"IPADDR", ipbuf, "PORT", (int)node->dlist[j].port,
						"PROTOID", (int)node->dlist[j].proto,
						"DNSTIME_SEC", (unsigned long)node->dlist[j].dnstime.tv_sec,
						"DNSTIME_USEC", (unsigned long)node->dlist[j].dnstime.tv_usec) < 0) {
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				return -1;
			}
		}

		if(mode != DS_RPC_PRINT_SHORT && node->dlist[j].attrs.body.s!=NULL) {
			if(rpc->struct_add(vh, "{", "ATTRS", &wh) < 0) {
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				return -1;
			}
			if(rpc->struct_add(wh, "SSdddSSS",
						"BODY", &(node->dlist[j].attrs.body),
						"DUID", (node->dlist[j].attrs.duid.s)
									? &(node->dlist[j].attrs.duid) : &data,
						"MAXLOAD", node->dlist[j].attrs.maxload,
						"WEIGHT", node->dlist[j].attrs.weight,
						"RWEIGHT", node->dlist[j].attrs.rweight,
						"SOCKET", (node->dlist[j].attrs.socket.s)
									? &(node->dlist[j].attrs.socket) : &data,
						"SOCKNAME", (node->dlist[j].attrs.sockname.s)
									? &(node->dlist[j].attrs.sockname) : &data,
						"OBPROXY", (node->dlist[j].attrs.obproxy.s)
									? &(node->dlist[j].attrs.obproxy) : &data)
					< 0) {
				rpc->fault(ctx, 500, "Internal error creating attrs struct");
				return -1;
			}
		}
		if (ds_ping_latency_stats) {
			if(rpc->struct_add(vh, "{", "LATENCY", &lh) < 0) {
				rpc->fault(ctx, 500, "Internal error creating dest");
				return -1;
			}
			if (rpc->struct_add(lh, "fffdd", "AVG", node->dlist[j].latency_stats.average,
					  "STD", node->dlist[j].latency_stats.stdev,
					  "EST", node->dlist[j].latency_stats.estimate,
					  "MAX", node->dlist[j].latency_stats.max,
					  "TIMEOUT", node->dlist[j].latency_stats.timeout)
					< 0) {
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				return -1;
			}
		}
		if (ds_hash_size>0) {
			if(rpc->struct_add(vh, "{", "RUNTIME", &dh) < 0) {
				rpc->fault(ctx, 500, "Internal error creating runtime struct");
				return -1;
			}
			if (rpc->struct_add(dh, "d", "DLGLOAD", node->dlist[j].dload) < 0) {
				rpc->fault(ctx, 500, "Internal error creating runtime attrs");
				return -1;
			}
		}
	}

	return 0;
}

/*
 * RPC command to print dispatcher destination sets
 */
static void dispatcher_rpc_list(rpc_t *rpc, void *ctx)
{
	void *th;
	void *ih;
	int n;
	str smode;
	int vmode = DS_RPC_PRINT_NORMAL;

	n = rpc->scan(ctx, "*S", &smode);
	if(n == 1) {
		if(smode.len==5 && strncasecmp(smode.s, "short", 5)==0) {
			vmode = DS_RPC_PRINT_SHORT;
		} else if(smode.len==4 && strncasecmp(smode.s, "full", 4)==0) {
			vmode = DS_RPC_PRINT_FULL;
		}
	}

	ds_set_t *dslist = ds_get_list();
	int dslistnr = ds_get_list_nr();

	if(dslist == NULL || dslistnr <= 0) {
		LM_DBG("no destination sets\n");
		rpc->fault(ctx, 500, "No Destination Sets");
		return;
	}

	/* add entry node */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	if(rpc->struct_add(th, "d[", "NRSETS", dslistnr, "RECORDS", &ih) < 0) {
		rpc->fault(ctx, 500, "Internal error sets structure");
		return;
	}

	ds_rpc_print_set(dslist, rpc, ctx, ih, vmode);

	return;
}


/*
 * RPC command to set the state of a destination address or duid
 */
static void dispatcher_rpc_set_state_helper(rpc_t *rpc, void *ctx, int mattr)
{
	int group;
	str dest;
	str state;
	int stval;

	if(rpc->scan(ctx, ".SdS", &state, &group, &dest) < 3) {
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if(state.len <= 0 || state.s == NULL) {
		LM_ERR("bad state value\n");
		rpc->fault(ctx, 500, "Invalid State Parameter");
		return;
	}

	stval = 0;
	if(state.s[0] == '0' || state.s[0] == 'I' || state.s[0] == 'i') {
		/* set inactive */
		stval |= DS_INACTIVE_DST;
		if((state.len > 1) && (state.s[1] == 'P' || state.s[1] == 'p'))
			stval |= DS_PROBING_DST;
	} else if(state.s[0] == '1' || state.s[0] == 'A' || state.s[0] == 'a') {
		/* set active */
		if((state.len > 1) && (state.s[1] == 'P' || state.s[1] == 'p'))
			stval |= DS_PROBING_DST;
	} else if(state.s[0] == '2' || state.s[0] == 'D' || state.s[0] == 'd') {
		/* set disabled */
		stval |= DS_DISABLED_DST;
	} else if(state.s[0] == '3' || state.s[0] == 'T' || state.s[0] == 't') {
		/* set trying */
		stval |= DS_TRYING_DST;
		if((state.len > 1) && (state.s[1] == 'P' || state.s[1] == 'p'))
			stval |= DS_PROBING_DST;
	} else {
		LM_ERR("unknown state value\n");
		rpc->fault(ctx, 500, "Unknown State Value");
		return;
	}

	if(dest.len == 3 && strncmp(dest.s, "all", 3) == 0) {
		ds_reinit_state_all(group, stval);
	} else {
		if (mattr==1) {
			if(ds_reinit_duid_state(group, &dest, stval) < 0) {
				rpc->fault(ctx, 500, "State Update Failed");
				return;
			}
		} else {
			if(ds_reinit_state(group, &dest, stval) < 0) {
				rpc->fault(ctx, 500, "State Update Failed");
				return;
			}
		}
	}
	rpc->rpl_printf(ctx, "Ok. Dispatcher state updated.");
	return;
}


static const char *dispatcher_rpc_set_state_doc[2] = {
		"Set the state of a destination by address", 0};

/*
 * RPC command to set the state of a destination address
 */
static void dispatcher_rpc_set_state(rpc_t *rpc, void *ctx)
{
	dispatcher_rpc_set_state_helper(rpc, ctx, 0);
}

static const char *dispatcher_rpc_set_duid_state_doc[2] = {
		"Set the state of a destination by duid", 0};

/*
 * RPC command to set the state of a destination duid
 */
static void dispatcher_rpc_set_duid_state(rpc_t *rpc, void *ctx)
{
	dispatcher_rpc_set_state_helper(rpc, ctx, 1);
}

static const char *dispatcher_rpc_ping_active_doc[2] = {
		"Manage setting on/off the pinging (keepalive) of destinations", 0};


/*
 * RPC command to set the state of a destination address
 */
static void dispatcher_rpc_ping_active(rpc_t *rpc, void *ctx)
{
	int state;
	int ostate;
	void *th;

	if(rpc->scan(ctx, "*d", &state) != 1) {
		state = -1;
	}
	ostate = ds_ping_active_get();
	/* add entry node */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	if(state == -1) {
		if(rpc->struct_add(th, "d", "OldPingState", ostate) < 0) {
			rpc->fault(ctx, 500, "Internal error reply structure");
			return;
		}
		return;
	}

	if(ds_ping_active_set(state) < 0) {
		rpc->fault(ctx, 500, "Ping State Update Failed");
		return;
	}
	if(rpc->struct_add(th, "dd", "NewPingState", state, "OldPingState", ostate)
			< 0) {
		rpc->fault(ctx, 500, "Internal error reply structure");
		return;
	}
	return;
}

static const char *dispatcher_rpc_add_doc[2] = {
		"Add a destination address in memory", 0};


/*
 * RPC command to add a destination address to memory
 */
static void dispatcher_rpc_add(rpc_t *rpc, void *ctx)
{
	int group, flags, priority, nparams;
	str dest;
	str attrs = STR_NULL;

	if(ds_rpc_reload_time==NULL) {
		LM_ERR("Not ready for rebuilding destinations list\n");
		rpc->fault(ctx, 500, "Not ready for reload");
		return;
	}
	if(*ds_rpc_reload_time!=0 && *ds_rpc_reload_time > time(NULL) - ds_reload_delta) {
		LM_ERR("ongoing reload\n");
		rpc->fault(ctx, 500, "Ongoing reload");
		return;
	}
	*ds_rpc_reload_time = time(NULL);

	flags = 0;
	priority = 0;

	nparams = rpc->scan(ctx, "dS*ddS", &group, &dest, &flags, &priority, &attrs);
	if(nparams < 2) {
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	} else if (nparams <= 4) {
		attrs.s = 0;
		attrs.len = 0;
	}

	if(ds_add_dst(group, &dest, flags, priority, &attrs) != 0) {
		rpc->fault(ctx, 500, "Adding dispatcher dst failed");
		return;
	}
	rpc->rpl_printf(ctx, "Ok. Dispatcher destination added.");
	return;
}

static const char *dispatcher_rpc_remove_doc[2] = {
		"Remove a destination address from memory", 0};


/*
 * RPC command to remove a destination address from memory
 */
static void dispatcher_rpc_remove(rpc_t *rpc, void *ctx)
{
	int group;
	str dest;

	if(ds_rpc_reload_time==NULL) {
		LM_ERR("Not ready for rebuilding destinations list\n");
		rpc->fault(ctx, 500, "Not ready for reload");
		return;
	}
	if(*ds_rpc_reload_time!=0 && *ds_rpc_reload_time > time(NULL) - ds_reload_delta) {
		LM_ERR("ongoing reload\n");
		rpc->fault(ctx, 500, "Ongoing reload");
		return;
	}
	*ds_rpc_reload_time = time(NULL);

	if(rpc->scan(ctx, "dS", &group, &dest) < 2) {
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}

	if(ds_remove_dst(group, &dest) != 0) {
		rpc->fault(ctx, 500, "Removing dispatcher dst failed");
		return;
	}
	rpc->rpl_printf(ctx, "Ok. Dispatcher destination removed.");
	return;
}

static const char *dispatcher_rpc_hash_doc[2] = {
		"Compute the hash if the values", 0};


/*
 * RPC command to compute the hash of the values
 */
static void dispatcher_rpc_hash(rpc_t *rpc, void *ctx)
{
	int n = 0;
	unsigned int hashid = 0;
	int nslots = 0;
	str val1 = STR_NULL;
	str val2 = STR_NULL;
	void *th;

	n = rpc->scan(ctx, "dS*S", &nslots, &val1, &val2);
	if(n < 2) {
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if(n==2) {
		val2.s = NULL;
		val2.s = 0;
	}

	hashid = ds_get_hash(&val1, &val2);

	/* add entry node */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	if(rpc->struct_add(th, "uu", "hashid", hashid,
				"slot", (nslots>0)?(hashid%nslots):0)
			< 0) {
		rpc->fault(ctx, 500, "Internal error reply structure");
		return;
	}

	return;
}

/* clang-format off */
rpc_export_t dispatcher_rpc_cmds[] = {
	{"dispatcher.reload", dispatcher_rpc_reload,
		dispatcher_rpc_reload_doc, 0},
	{"dispatcher.list",   dispatcher_rpc_list,
		dispatcher_rpc_list_doc,   0},
	{"dispatcher.set_state",   dispatcher_rpc_set_state,
		dispatcher_rpc_set_state_doc,   0},
	{"dispatcher.set_duid_state",   dispatcher_rpc_set_duid_state,
		dispatcher_rpc_set_duid_state_doc,   0},
	{"dispatcher.ping_active",   dispatcher_rpc_ping_active,
		dispatcher_rpc_ping_active_doc, 0},
	{"dispatcher.add",   dispatcher_rpc_add,
		dispatcher_rpc_add_doc, 0},
	{"dispatcher.remove",   dispatcher_rpc_remove,
		dispatcher_rpc_remove_doc, 0},
	{"dispatcher.hash",   dispatcher_rpc_hash,
		dispatcher_rpc_hash_doc, 0},
	{0, 0, 0, 0}
};
/* clang-format on */

/**
 * register RPC commands
 */
static int ds_init_rpc(void)
{
	if(rpc_register_array(dispatcher_rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
