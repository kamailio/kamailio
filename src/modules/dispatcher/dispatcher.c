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
static str dst_avp_param = STR_NULL;
static str grp_avp_param = STR_NULL;
static str cnt_avp_param = STR_NULL;
static str dstid_avp_param = STR_NULL;
static str attrs_avp_param = STR_NULL;
static str sock_avp_param = STR_NULL;
str hash_pvar_param = STR_NULL;

int_str dst_avp_name;
unsigned short dst_avp_type;
int_str grp_avp_name;
unsigned short grp_avp_type;
int_str cnt_avp_name;
unsigned short cnt_avp_type;
int_str dstid_avp_name;
unsigned short dstid_avp_type;
int_str attrs_avp_name;
unsigned short attrs_avp_type;
int_str sock_avp_name;
unsigned short sock_avp_type;

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
int ds_probing_mode  = DS_PROBE_NONE;

static str ds_ping_reply_codes_str= STR_NULL;
static int** ds_ping_reply_codes = NULL;
static int* ds_ping_reply_codes_cnt;

str ds_default_socket       = STR_NULL;
struct socket_info * ds_default_sockinfo = NULL;

int ds_hash_size = 0;
int ds_hash_expire = 7200;
int ds_hash_initexpire = 7200;
int ds_hash_check_interval = 30;
int ds_timer_mode = 0;

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
static int w_ds_next_dst(struct sip_msg*, char*, char*);
static int w_ds_next_domain(struct sip_msg*, char*, char*);
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

static int fixup_ds_is_from_list(void** param, int param_no);
static int fixup_ds_list_exist(void** param,int param_no);

static void destroy(void);

static int ds_warn_fixup(void** param, int param_no);

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
	{"ds_next_dst",      (cmd_function)w_ds_next_dst,      0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_next_domain",   (cmd_function)w_ds_next_domain,   0,
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
	{"ds_load_unset",    (cmd_function)w_ds_load_unset,   0,
		0, 0, ANY_ROUTE},
	{"ds_load_update",   (cmd_function)w_ds_load_update,  0,
		0, 0, ANY_ROUTE},
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
	{"dst_avp",         PARAM_STR, &dst_avp_param},
	{"grp_avp",         PARAM_STR, &grp_avp_param},
	{"cnt_avp",         PARAM_STR, &cnt_avp_param},
	{"dstid_avp",       PARAM_STR, &dstid_avp_param},
	{"attrs_avp",       PARAM_STR, &attrs_avp_param},
	{"sock_avp",        PARAM_STR, &sock_avp_param},
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
	{"outbound_proxy",  PARAM_STR, &ds_outbound_proxy},
	{"ds_default_socket",  PARAM_STR, &ds_default_socket},
	{"ds_timer_mode",      PARAM_INT, &ds_timer_mode},
	{"event_callback",     PARAM_STR, &ds_event_callback},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"dispatcher",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{
	pv_spec_t avp_spec;
	str host;
	int port, proto;

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
	*ds_ping_reply_codes = 0;
	ds_ping_reply_codes_cnt = (int *)shm_malloc(sizeof(int));
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
			LM_WARN("non-local socket <%.*s>\n", ds_default_socket.len,
					ds_default_socket.s);
			return -1;
		}
		LM_INFO("default dispatcher socket set to <%.*s>\n",
				ds_default_socket.len, ds_default_socket.s);
	}

	if(init_data() != 0)
		return -1;

	if(ds_db_url.s) {
		if(init_ds_db() != 0) {
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

	if(dst_avp_param.s && dst_avp_param.len > 0) {
		if(pv_parse_spec(&dst_avp_param, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					dst_avp_param.len, dst_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &dst_avp_name, &dst_avp_type)
				!= 0) {
			LM_ERR("[%.*s]- invalid AVP definition\n", dst_avp_param.len,
					dst_avp_param.s);
			return -1;
		}
	} else {
		dst_avp_name.n = 0;
		dst_avp_type = 0;
	}
	if(grp_avp_param.s && grp_avp_param.len > 0) {
		if(pv_parse_spec(&grp_avp_param, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					grp_avp_param.len, grp_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &grp_avp_name, &grp_avp_type)
				!= 0) {
			LM_ERR("[%.*s]- invalid AVP definition\n", grp_avp_param.len,
					grp_avp_param.s);
			return -1;
		}
	} else {
		grp_avp_name.n = 0;
		grp_avp_type = 0;
	}
	if(cnt_avp_param.s && cnt_avp_param.len > 0) {
		if(pv_parse_spec(&cnt_avp_param, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					cnt_avp_param.len, cnt_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &cnt_avp_name, &cnt_avp_type)
				!= 0) {
			LM_ERR("[%.*s]- invalid AVP definition\n", cnt_avp_param.len,
					cnt_avp_param.s);
			return -1;
		}
	} else {
		cnt_avp_name.n = 0;
		cnt_avp_type = 0;
	}
	if(dstid_avp_param.s && dstid_avp_param.len > 0) {
		if(pv_parse_spec(&dstid_avp_param, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					dstid_avp_param.len, dstid_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &dstid_avp_name, &dstid_avp_type)
				!= 0) {
			LM_ERR("[%.*s]- invalid AVP definition\n", dstid_avp_param.len,
					dstid_avp_param.s);
			return -1;
		}
	} else {
		dstid_avp_name.n = 0;
		dstid_avp_type = 0;
	}

	if(attrs_avp_param.s && attrs_avp_param.len > 0) {
		if(pv_parse_spec(&attrs_avp_param, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					attrs_avp_param.len, attrs_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &attrs_avp_name, &attrs_avp_type)
				!= 0) {
			LM_ERR("[%.*s]- invalid AVP definition\n", attrs_avp_param.len,
					attrs_avp_param.s);
			return -1;
		}
	} else {
		attrs_avp_name.n = 0;
		attrs_avp_type = 0;
	}

	if(sock_avp_param.s && sock_avp_param.len > 0) {
		if(pv_parse_spec(&sock_avp_param, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					sock_avp_param.len, sock_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &sock_avp_name, &sock_avp_type)
				!= 0) {
			LM_ERR("[%.*s]- invalid AVP definition\n", sock_avp_param.len,
					sock_avp_param.s);
			return -1;
		}
	} else {
		sock_avp_name.n = 0;
		sock_avp_type = 0;
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

	if(dstid_avp_param.s && dstid_avp_param.len > 0) {
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
		} else {
			LM_ERR("call load dispatching DSTID_AVP set but no size"
				   " for hash table (see ds_hash_size parameter)\n");
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
	return 0;
}

/*! \brief
 * Initialize children
 */
static int child_init(int rank)
{
	kam_srand((11 + rank) * getpid() * 7);

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
	return w_ds_select_addr(
			msg, set, alg, 0 /* limit number of dst*/, 2 /*set no dst/uri*/);
}

/**
 *
 */
static int w_ds_select_limit(
		struct sip_msg *msg, char *set, char *alg, char *limit)
{
	return w_ds_select_addr(msg, set, alg, limit /* limit number of dst*/,
			2 /*set no dst/uri*/);
}

/**
 *
 */
static int w_ds_select_dst(struct sip_msg *msg, char *set, char *alg)
{
	return w_ds_select_addr(
			msg, set, alg, 0 /* limit number of dst*/, 0 /*set dst uri*/);
}

/**
 *
 */
static int w_ds_select_dst_limit(
		struct sip_msg *msg, char *set, char *alg, char *limit)
{
	return w_ds_select_addr(
			msg, set, alg, limit /* limit number of dst*/, 0 /*set dst uri*/);
}

/**
 *
 */
static int w_ds_select_domain(struct sip_msg *msg, char *set, char *alg)
{
	return w_ds_select_addr(
			msg, set, alg, 0 /* limit number of dst*/, 1 /*set host port*/);
}

/**
 *
 */
static int w_ds_select_domain_limit(
		struct sip_msg *msg, char *set, char *alg, char *limit)
{
	return w_ds_select_addr(
			msg, set, alg, limit /* limit number of dst*/, 1 /*set host port*/);
}

/**
 *
 */
static int w_ds_next_dst(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_next_dst(msg, 0 /*set dst uri*/);
}

/**
 *
 */
static int w_ds_next_domain(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_next_dst(msg, 1 /*set host port*/);
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
	if(!dst_avp_param.s || !grp_avp_param.s || !cnt_avp_param.s
			|| !sock_avp_param.s) {
		LM_ERR("failover functions used, but required AVP parameters"
			   " are NULL -- feature disabled\n");
	}
	return 0;
}

static int ds_reload(sip_msg_t *msg)
{
	if(!ds_db_url.s) {
		if(ds_load_list(dslistfile) != 0)
			LM_ERR("Error reloading from list\n");
		return -1;
	} else {
		if(ds_reload_db() < 0)
			LM_ERR("Error reloading from db\n");
		return -1;
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
		return -1;
	}
	LM_DBG("--- Looking for dispatcher set %d\n", set);
	return ds_list_exist(set);
}

static int ki_ds_list_exists(struct sip_msg *msg, int set)
{
	return ds_list_exist(set);
}

static int fixup_ds_list_exist(void **param, int param_no)
{
	return fixup_igp_null(param, param_no);
	return 0;
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

	/* Validate String: */
	if(cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).s == 0
			|| cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).len
					   <= 0)
		return 0;

	/* parse_params will modify the string pointer of .s, so we need to make a copy. */
	input.s = cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).s;
	input.len =
			cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).len;

	/* Parse the parameters: */
	if(parse_params(&input, CLASS_ANY, 0, &params_list) < 0)
		return -1;

	/* Get the number of entries in the list */
	for(pit = params_list; pit; pit = pit->next) {
		if(pit->name.len == 4 && strncasecmp(pit->name.s, "code", 4) == 0) {
			str2sint(&pit->body, &code);
			if((code >= 100) && (code < 700))
				list_size += 1;
		} else if(pit->name.len == 5
				  && strncasecmp(pit->name.s, "class", 5) == 0) {
			str2sint(&pit->body, &code);
			if((code >= 1) && (code < 7))
				list_size += 100;
		}
	}
	LM_DBG("Should be %d Destinations.\n", list_size);

	if(list_size > 0) {
		/* Allocate Memory for the new list: */
		ds_ping_reply_codes_new = (int *)shm_malloc(list_size * sizeof(int));
		if(ds_ping_reply_codes_new == NULL) {
			free_params(params_list);
			LM_ERR("no more memory\n");
			return -1;
		}

		/* Now create the list of valid reply-codes: */
		for(pit = params_list; pit; pit = pit->next) {
			if(pit->name.len == 4 && strncasecmp(pit->name.s, "code", 4) == 0) {
				str2sint(&pit->body, &code);
				if((code >= 100) && (code < 700))
					ds_ping_reply_codes_new[pos++] = code;
			} else if(pit->name.len == 5
					  && strncasecmp(pit->name.s, "class", 5) == 0) {
				str2sint(&pit->body, &code);
				if((code >= 1) && (code < 7)) {
					/* Add every code from this class, e.g. 100 to 199 */
					for(i = (code * 100); i <= ((code * 100) + 99); i++)
						ds_ping_reply_codes_new[pos++] = i;
				}
			}
		}
	} else {
		ds_ping_reply_codes_new = 0;
	}
	free_params(params_list);

	/* More reply-codes? Change Pointer and then set number of codes. */
	if(list_size > *ds_ping_reply_codes_cnt) {
		// Copy Pointer
		ds_ping_reply_codes_old = *ds_ping_reply_codes;
		*ds_ping_reply_codes = ds_ping_reply_codes_new;
		// Done: Set new Number of entries:
		*ds_ping_reply_codes_cnt = list_size;
		// Free the old memory area:
		if(ds_ping_reply_codes_old)
			shm_free(ds_ping_reply_codes_old);
		/* Less or equal? Set the number of codes first. */
	} else {
		// Done:
		*ds_ping_reply_codes_cnt = list_size;
		// Copy Pointer
		ds_ping_reply_codes_old = *ds_ping_reply_codes;
		*ds_ping_reply_codes = ds_ping_reply_codes_new;
		// Free the old memory area:
		if(ds_ping_reply_codes_old)
			shm_free(ds_ping_reply_codes_old);
	}
	/* Print the list as INFO: */
	for(i = 0; i < *ds_ping_reply_codes_cnt; i++) {
		LM_DBG("Dispatcher: Now accepting Reply-Code %d (%d/%d) as valid\n",
				(*ds_ping_reply_codes)[i], (i + 1), *ds_ping_reply_codes_cnt);
	}
	return 0;
}

int ds_ping_check_rplcode(int code)
{
	int i;

	for(i = 0; i < *ds_ping_reply_codes_cnt; i++) {
		if((*ds_ping_reply_codes)[i] == code)
			return 1;
	}

	return 0;
}

void ds_ping_reply_codes_update(str *gname, str *name)
{
	ds_parse_reply_codes();
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
	return ds_next_dst(msg, 0 /*set dst uri*/);
}

/**
 *
 */
static int ki_ds_next_domain(sip_msg_t *msg)
{
	return ds_next_dst(msg, 1 /*set host port*/);
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
	{ str_init("dispatcher"), str_init("ds_next_dst"),
		SR_KEMIP_INT, ki_ds_next_dst,
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
	return;
}


static const char *dispatcher_rpc_list_doc[2] = {
		"Return the content of dispatcher sets", 0};

/**
 *
 */
int ds_rpc_print_set(ds_set_t *node, rpc_t *rpc, void *ctx, void *rpc_handle)
{
	int i = 0, rc = 0;
	void *rh;
	void *sh;
	void *vh;
	void *wh;
	void *lh; // latency stats handle
	int j;
	char c[3];
	str data = STR_NULL;

	if(!node)
		return 0;

	for(; i < 2; ++i) {
		rc = ds_rpc_print_set(node->next[i], rpc, ctx, rpc_handle);
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

		if(node->dlist[j].attrs.body.s) {
			if(rpc->struct_add(vh, "Ssd{", "URI", &node->dlist[j].uri, "FLAGS",
					   c, "PRIORITY", node->dlist[j].priority, "ATTRS", &wh)
					< 0) {
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				return -1;
			}
			if(rpc->struct_add(wh, "SSdddS", "BODY",
					   &(node->dlist[j].attrs.body), "DUID",
					   (node->dlist[j].attrs.duid.s)
							   ? &(node->dlist[j].attrs.duid)
							   : &data,
					   "MAXLOAD", node->dlist[j].attrs.maxload, "WEIGHT",
					   node->dlist[j].attrs.weight, "RWEIGHT",
					   node->dlist[j].attrs.rweight, "SOCKET",
					   (node->dlist[j].attrs.socket.s)
							   ? &(node->dlist[j].attrs.socket)
							   : &data)
					< 0) {
				rpc->fault(ctx, 500, "Internal error creating attrs struct");
				return -1;
			}
		} else {
			if(rpc->struct_add(vh, "Ssd", "URI", &node->dlist[j].uri, "FLAGS",
					   c, "PRIORITY", node->dlist[j].priority)
					< 0) {
				rpc->fault(ctx, 500, "Internal error creating dest struct");
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

	ds_set_t *ds_list = ds_get_list();
	int ds_list_nr = ds_get_list_nr();

	if(ds_list == NULL || ds_list_nr <= 0) {
		LM_DBG("no destination sets\n");
		rpc->fault(ctx, 500, "No Destination Sets");
		return;
	}

	/* add entry node */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	if(rpc->struct_add(th, "d[", "NRSETS", ds_list_nr, "RECORDS", &ih) < 0) {
		rpc->fault(ctx, 500, "Internal error sets structure");
		return;
	}

	ds_rpc_print_set(ds_list, rpc, ctx, ih);

	return;
}


static const char *dispatcher_rpc_set_state_doc[2] = {
		"Set the state of a destination address", 0};


/*
 * RPC command to set the state of a destination address
 */
static void dispatcher_rpc_set_state(rpc_t *rpc, void *ctx)
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

	if(ds_reinit_state(group, &dest, stval) < 0) {
		rpc->fault(ctx, 500, "State Update Failed");
		return;
	}

	return;
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

/* clang-format off */
rpc_export_t dispatcher_rpc_cmds[] = {
	{"dispatcher.reload", dispatcher_rpc_reload,
		dispatcher_rpc_reload_doc, 0},
	{"dispatcher.list",   dispatcher_rpc_list,
		dispatcher_rpc_list_doc,   0},
	{"dispatcher.set_state",   dispatcher_rpc_set_state,
		dispatcher_rpc_set_state_doc,   0},
	{"dispatcher.ping_active",   dispatcher_rpc_ping_active,
		dispatcher_rpc_ping_active_doc, 0},
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
