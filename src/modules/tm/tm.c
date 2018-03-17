/*
 * TM (transaction management) module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/** TM :: Module API (core).
 * @file
 * @ingroup tm
 */

/**
 * @defgroup tm TM :: Transaction stateful proxy support
 *
 * The TM module enables stateful processing of SIP transactions. The main use
 * of stateful logic, which is costly in terms of memory and CPU, is some
 * services that inherently need state.
 *
 * For example, transaction-based accounting (module acc) needs to process
 * transaction state as opposed to individual messages, and any kinds of
 * forking must be implemented statefully. Other use of stateful processing
 * is it trading CPU caused by retransmission processing for memory.
 * That however only makes sense if CPU consumption per request is huge.
 * For example, if you want to avoid costly DNS resolution for every
 * retransmission of a request to an unresolvable destination, use stateful
 * mode. Then, only the initial message burdens server by DNS queries,
 * subsequent retransmissions will be dropped and will not result in more
 * processes blocked by DNS resolution. The price is more memory consumption
 * and higher processing latency.
 */


#include "defs.h"


#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/ut.h"
#include "../../core/script_cb.h"
#include "../../core/usr_avp.h"
#include "../../core/mem/mem.h"
#include "../../core/route_struct.h"
#include "../../core/route.h"
#include "../../core/cfg/cfg.h"
#include "../../core/globals.h"
#include "../../core/timer_ticks.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "config.h"
#include "sip_msg.h"
#include "h_table.h"
#include "t_hooks.h"
#include "tm_load.h"
#include "ut.h"
#include "t_reply.h"
#include "uac.h"
#include "t_fwd.h"
#include "t_lookup.h"
#include "t_stats.h"
#include "callid.h"
#include "t_cancel.h"
#include "t_fifo.h"
#include "timer.h"
#include "t_msgbuilder.h"
#include "select.h"
#include "t_serial.h"
#include "rpc_uac.h"

MODULE_VERSION

/* fixup functions */
static int fixup_hostport2proxy(void** param, int param_no);
static int fixup_proto_hostport2proxy(void** param, int param_no);
static int fixup_on_failure(void** param, int param_no);
static int fixup_on_branch_failure(void** param, int param_no);
static int fixup_on_reply(void** param, int param_no);
static int fixup_on_branch(void** param, int param_no);
static int fixup_t_reply(void** param, int param_no);
static int fixup_on_sl_reply(modparam_t type, void* val);
static int fixup_t_relay_to(void** param, int param_no);
static int fixup_t_is_set(void** param, int param_no);

/* init functions */
static int mod_init(void);
static int child_init(int rank);


/* exported functions */
static int w_t_check(struct sip_msg* msg, char* str, char* str2);
static int w_t_lookup_cancel(struct sip_msg* msg, char* str, char* str2);
static int w_t_reply(struct sip_msg* msg, char* str, char* str2);
static int w_t_release(struct sip_msg* msg, char* str, char* str2);
static int w_t_retransmit_reply(struct sip_msg* p_msg, char* foo, char* bar );
static int w_t_newtran(struct sip_msg* p_msg, char* foo, char* bar );
static int w_t_relay( struct sip_msg  *p_msg , char *_foo, char *_bar);
static int w_t_relay2( struct sip_msg  *p_msg , char *proxy, char*);
static int w_t_relay_to_udp( struct sip_msg  *p_msg , char *proxy, char *);
static int w_t_relay_to_udp_uri( struct sip_msg  *p_msg , char*, char*);
#ifdef USE_TCP
static int w_t_relay_to_tcp( struct sip_msg  *p_msg , char *proxy, char *);
static int w_t_relay_to_tcp_uri( struct sip_msg  *p_msg , char*, char*);
#endif
#ifdef USE_TLS
static int w_t_relay_to_tls( struct sip_msg  *p_msg , char *proxy, char *);
static int w_t_relay_to_tls_uri( struct sip_msg  *p_msg , char*, char*);
#endif
#ifdef USE_SCTP
static int w_t_relay_to_sctp( struct sip_msg  *p_msg , char *proxy, char *);
static int w_t_relay_to_sctp_uri( struct sip_msg*, char*, char*);
#endif
static int w_t_relay_to_avp(struct sip_msg* msg, char* str,char*);
static int w_t_relay_to(struct sip_msg* msg, char* str,char*);
static int w_t_replicate_uri( struct sip_msg  *p_msg ,
		char *uri,       /* sip uri as string or variable */
		char *_foo       /* nothing expected */ );
static int w_t_replicate( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ );
static int w_t_replicate_udp( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ );
#ifdef USE_TCP
static int w_t_replicate_tcp( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ );
#endif
#ifdef USE_TLS
static int w_t_replicate_tls( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ );
#endif
#ifdef USE_SCTP
static int w_t_replicate_sctp( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ );
#endif
static int w_t_replicate_to(struct sip_msg* msg, char* str,char*);
static int w_t_forward_nonack(struct sip_msg* msg, char* str, char* );
static int w_t_forward_nonack_uri(struct sip_msg* msg, char* str,char*);
static int w_t_forward_nonack_udp(struct sip_msg* msg, char* str,char*);
#ifdef USE_TCP
static int w_t_forward_nonack_tcp(struct sip_msg*, char* str,char*);
#endif
#ifdef USE_TLS
static int w_t_forward_nonack_tls(struct sip_msg*, char* str,char*);
#endif
#ifdef USE_SCTP
static int w_t_forward_nonack_sctp(struct sip_msg*, char* str,char*);
#endif
static int w_t_forward_nonack_to(struct sip_msg* msg, char* str,char*);
static int w_t_relay_cancel(struct sip_msg *p_msg, char *_foo,
		char *_bar);
static int w_t_on_failure(struct sip_msg* msg, char *go_to, char *foo);
static int w_t_on_branch_failure(struct sip_msg* msg, char *go_to,
		char *foo);
static int w_t_on_branch(struct sip_msg* msg, char *go_to, char *foo);
static int w_t_on_reply(struct sip_msg* msg, char *go_to, char *foo );
static int t_check_status(struct sip_msg* msg, char *match, char *foo);
static int t_set_fr_inv(struct sip_msg* msg, char* fr_inv, char* foo);
static int t_set_fr_all(struct sip_msg* msg, char* fr_inv, char* fr);
static int w_t_reset_fr(struct sip_msg* msg, char* foo, char* bar);
static int w_t_set_retr(struct sip_msg* msg, char* retr_t1, char* retr_t2);
static int w_t_reset_retr(struct sip_msg* msg, char* foo, char* bar);
static int w_t_set_max_lifetime(struct sip_msg* msg, char* inv, char* noninv);
static int w_t_reset_max_lifetime(struct sip_msg* msg, char* foo, char* bar);
static int w_t_set_auto_inv_100(struct sip_msg* msg, char* on_off, char* foo);
static int w_t_set_disable_6xx(struct sip_msg* msg, char* on_off, char* foo);
static int w_t_set_disable_failover(struct sip_msg* msg, char* on_off, char* f);
#ifdef CANCEL_REASON_SUPPORT
static int w_t_set_no_e2e_cancel_reason(struct sip_msg* msg, char* on_off,
		char* f);
#endif /* CANCEL_REASON_SUPPORT */
static int w_t_set_disable_internal_reply(struct sip_msg* msg, char* on_off,
		char* f);
static int w_t_branch_timeout(struct sip_msg* msg, char*, char*);
static int w_t_branch_replied(struct sip_msg* msg, char*, char*);
static int w_t_any_timeout(struct sip_msg* msg, char*, char*);
static int w_t_any_replied(struct sip_msg* msg, char*, char*);
static int w_t_is_canceled(struct sip_msg* msg, char*, char*);
static int w_t_is_expired(struct sip_msg* msg, char*, char*);
static int w_t_is_retr_async_reply(struct sip_msg* msg, char*, char*);
static int w_t_grep_status(struct sip_msg* msg, char*, char*);
static int w_t_drop_replies(struct sip_msg* msg, char* foo, char* bar);
static int w_t_save_lumps(struct sip_msg* msg, char* foo, char* bar);
static int w_t_check_trans(struct sip_msg* msg, char* foo, char* bar);
static int w_t_is_set(struct sip_msg* msg, char* target, char* bar);
static int w_t_use_uac_headers(sip_msg_t* msg, char* foo, char* bar);
static int w_t_uac_send(sip_msg_t* msg, char* pmethod, char* pruri,
		char* pnexthop, char* psock, char *phdrs, char* pbody);


/* by default the fr timers avps are not set, so that the avps won't be
 * searched for nothing each time a new transaction is created */
static char *fr_timer_param = 0 /*FR_TIMER_AVP*/;
static char *fr_inv_timer_param = 0 /*FR_INV_TIMER_AVP*/;

str contacts_avp = {0, 0};
str contact_flows_avp = {0, 0};
str ulattrs_xavp_name = {NULL, 0};

int tm_remap_503_500 = 1;

int tm_failure_exec_mode = 0;

int tm_dns_reuse_rcv_socket = 0;

static rpc_export_t tm_rpc[];

str tm_event_callback = STR_NULL;

static int fixup_t_check_status(void** param, int param_no);

static cmd_export_t cmds[]={
	{"t_newtran",          w_t_newtran,             0, 0,
		REQUEST_ROUTE},
	{"t_lookup_request",   w_t_check,               0, 0,
		REQUEST_ROUTE},
	{"t_lookup_cancel",    w_t_lookup_cancel,       0, 0,
		REQUEST_ROUTE},
	{"t_lookup_cancel",    w_t_lookup_cancel,       1, fixup_int_1,
		REQUEST_ROUTE},
	{"t_reply",              w_t_reply,               2, fixup_t_reply,
		REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"t_retransmit_reply", w_t_retransmit_reply,    0, 0,
		REQUEST_ROUTE},
	{"t_release",          w_t_release,             0, 0,
		REQUEST_ROUTE},
	{"t_relay_to_udp",       w_t_relay_to_udp,        2, fixup_hostport2proxy,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"t_relay_to_udp",       w_t_relay_to_udp_uri,    0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
#ifdef USE_TCP
	{"t_relay_to_tcp",       w_t_relay_to_tcp,        2, fixup_hostport2proxy,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"t_relay_to_tcp",       w_t_relay_to_tcp_uri,    0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
#endif
#ifdef USE_TLS
	{"t_relay_to_tls",       w_t_relay_to_tls,        2, fixup_hostport2proxy,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"t_relay_to_tls",       w_t_relay_to_tls_uri,    0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
#endif
#ifdef USE_SCTP
	{"t_relay_to_sctp",       w_t_relay_to_sctp,       2, fixup_hostport2proxy,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"t_relay_to_sctp",       w_t_relay_to_sctp_uri,    0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
#endif
	{"t_replicate",        w_t_replicate_uri,       0, 0,
		REQUEST_ROUTE},
	{"t_replicate",        w_t_replicate_uri,       1, fixup_spve_null,
		REQUEST_ROUTE},
	{"t_replicate",        w_t_replicate,           2, fixup_hostport2proxy,
		REQUEST_ROUTE},
	{"t_replicate_udp",    w_t_replicate_udp,       2, fixup_hostport2proxy,
		REQUEST_ROUTE},
#ifdef USE_TCP
	{"t_replicate_tcp",    w_t_replicate_tcp,       2, fixup_hostport2proxy,
		REQUEST_ROUTE},
#endif
#ifdef USE_TLS
	{"t_replicate_tls",    w_t_replicate_tls,       2, fixup_hostport2proxy,
		REQUEST_ROUTE},
#endif
#ifdef USE_SCTP
	{"t_replicate_sctp",    w_t_replicate_sctp,     2, fixup_hostport2proxy,
		REQUEST_ROUTE},
#endif
	{"t_replicate_to", w_t_replicate_to,  		2, fixup_proto_hostport2proxy,
		REQUEST_ROUTE},
	{"t_relay",              w_t_relay,               0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_relay",              w_t_relay2,              2, fixup_hostport2proxy,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_relay_to_avp", w_t_relay_to_avp,  		2, fixup_proto_hostport2proxy,
		REQUEST_ROUTE},
	{"t_relay_to",			w_t_relay_to,           0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_relay_to",			w_t_relay_to,           1, fixup_t_relay_to,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_relay_to",			w_t_relay_to,           2, fixup_t_relay_to,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_forward_nonack",     w_t_forward_nonack,      2, fixup_hostport2proxy,
		REQUEST_ROUTE},
	{"t_forward_nonack_uri", w_t_forward_nonack_uri,  0, 0,
		REQUEST_ROUTE},
	{"t_forward_nonack_udp", w_t_forward_nonack_udp,  2, fixup_hostport2proxy,
		REQUEST_ROUTE},
#ifdef USE_TCP
	{"t_forward_nonack_tcp", w_t_forward_nonack_tcp,  2, fixup_hostport2proxy,
		REQUEST_ROUTE},
#endif
#ifdef USE_TLS
	{"t_forward_nonack_tls", w_t_forward_nonack_tls,  2, fixup_hostport2proxy,
		REQUEST_ROUTE},
#endif
#ifdef USE_SCTP
	{"t_forward_nonack_sctp", w_t_forward_nonack_sctp, 2, fixup_hostport2proxy,
		REQUEST_ROUTE},
#endif
	{"t_forward_nonack_to", w_t_forward_nonack_to,  2, fixup_proto_hostport2proxy,
		REQUEST_ROUTE},
	{"t_relay_cancel",     w_t_relay_cancel,        0, 0,
		REQUEST_ROUTE},
	{"t_on_failure",       w_t_on_failure,         1, fixup_on_failure,
		REQUEST_ROUTE | FAILURE_ROUTE | TM_ONREPLY_ROUTE | BRANCH_ROUTE },
	{"t_on_branch_failure",w_t_on_branch_failure,  1, fixup_on_branch_failure,
		REQUEST_ROUTE | FAILURE_ROUTE | TM_ONREPLY_ROUTE | BRANCH_ROUTE },
	{"t_on_reply",         w_t_on_reply,            1, fixup_on_reply,
		REQUEST_ROUTE | FAILURE_ROUTE | TM_ONREPLY_ROUTE | BRANCH_ROUTE },
	{"t_on_branch",       w_t_on_branch,         1, fixup_on_branch,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_check_status",     t_check_status,          1, fixup_t_check_status,
		REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
	{"t_write_req",       t_write_req,              2, fixup_t_write,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_write_unix",      t_write_unix,             2, fixup_t_write,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_set_fr",          t_set_fr_inv,             1, fixup_var_int_1,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_set_fr",          t_set_fr_all,             2, fixup_var_int_12,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_reset_fr",        w_t_reset_fr,             0, 0,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_set_retr",        w_t_set_retr,               2, fixup_var_int_12,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_reset_retr",      w_t_reset_retr,           0, 0,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_set_max_lifetime", w_t_set_max_lifetime,      2, fixup_var_int_12,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_reset_max_lifetime", w_t_reset_max_lifetime, 0, 0,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_set_auto_inv_100", w_t_set_auto_inv_100,     1, fixup_var_int_1,
		REQUEST_ROUTE},
	{"t_set_disable_6xx", w_t_set_disable_6xx,       1, fixup_var_int_1,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_set_disable_failover", w_t_set_disable_failover, 1, fixup_var_int_1,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
#ifdef CANCEL_REASON_SUPPORT
	{"t_set_no_e2e_cancel_reason", w_t_set_no_e2e_cancel_reason, 1,
		fixup_var_int_1,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	/* alias for t_set_no_e2e_cancel_reason */
	{"t_disable_e2e_cancel_reason", w_t_set_no_e2e_cancel_reason, 1,
		fixup_var_int_1,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
#endif /* CANCEL_REASON_SUPPORT */
	{"t_set_disable_internal_reply", w_t_set_disable_internal_reply, 1,
		fixup_var_int_1,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_branch_timeout",  w_t_branch_timeout,       0, 0,
		FAILURE_ROUTE|EVENT_ROUTE},
	{"t_branch_replied",  w_t_branch_replied,       0, 0,
		FAILURE_ROUTE|EVENT_ROUTE},
	{"t_any_timeout",     w_t_any_timeout,          0, 0,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_any_replied",     w_t_any_replied,          0, 0,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_is_canceled",     w_t_is_canceled,          0, 0,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_is_retr_async_reply",     w_t_is_retr_async_reply,     0, 0,
		TM_ONREPLY_ROUTE},
	{"t_is_expired",      w_t_is_expired,           0, 0,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_grep_status",     w_t_grep_status,          1, fixup_var_int_1,
		REQUEST_ROUTE|TM_ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"t_drop_replies",    w_t_drop_replies,         0, 0,
		FAILURE_ROUTE},
	{"t_drop_replies",    w_t_drop_replies,         1, 0,
		FAILURE_ROUTE},
	{"t_save_lumps",      w_t_save_lumps,           0, 0,
		REQUEST_ROUTE},
	{"t_check_trans",	  w_t_check_trans,			0, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE },
	{"t_is_set",	      w_t_is_set,				1, fixup_t_is_set,
		ANY_ROUTE },
	{"t_use_uac_headers",  w_t_use_uac_headers,		0, 0,
		ANY_ROUTE },
	{"t_uac_send", (cmd_function)w_t_uac_send, 6, fixup_spve_all,
		ANY_ROUTE },

	{"t_load_contacts", t_load_contacts,            0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{"t_next_contacts", t_next_contacts,            0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{"t_next_contact_flow", t_next_contact_flow,            0, 0,
		REQUEST_ROUTE },

	/* not applicable from the script */
	{"load_tm",            (cmd_function)load_tm,           NO_SCRIPT,   0, 0},
	{"load_xtm",           (cmd_function)load_xtm,          NO_SCRIPT,   0, 0},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"ruri_matching",       PARAM_INT, &default_tm_cfg.ruri_matching         },
	{"via1_matching",       PARAM_INT, &default_tm_cfg.via1_matching         },
	{"callid_matching",     PARAM_INT, &default_tm_cfg.callid_matching       },
	{"fr_timer",            PARAM_INT, &default_tm_cfg.fr_timeout            },
	{"fr_inv_timer",        PARAM_INT, &default_tm_cfg.fr_inv_timeout        },
	{"wt_timer",            PARAM_INT, &default_tm_cfg.wait_timeout          },
	{"delete_timer",        PARAM_INT, &default_tm_cfg.delete_timeout        },
	{"retr_timer1",         PARAM_INT, &default_tm_cfg.rt_t1_timeout_ms      },
	{"retr_timer2"  ,       PARAM_INT, &default_tm_cfg.rt_t2_timeout_ms      },
	{"max_inv_lifetime",    PARAM_INT, &default_tm_cfg.tm_max_inv_lifetime   },
	{"max_noninv_lifetime", PARAM_INT, &default_tm_cfg.tm_max_noninv_lifetime},
	{"noisy_ctimer",        PARAM_INT, &default_tm_cfg.noisy_ctimer          },
	{"auto_inv_100",        PARAM_INT, &default_tm_cfg.tm_auto_inv_100       },
	{"auto_inv_100_reason", PARAM_STRING, &default_tm_cfg.tm_auto_inv_100_r  },
	{"unix_tx_timeout",     PARAM_INT, &default_tm_cfg.tm_unix_tx_timeout    },
	{"restart_fr_on_each_reply", PARAM_INT,
		&default_tm_cfg.restart_fr_on_each_reply},
	{"fr_timer_avp",        PARAM_STRING, &fr_timer_param                    },
	{"fr_inv_timer_avp",    PARAM_STRING, &fr_inv_timer_param                },
	{"tw_append",           PARAM_STRING|PARAM_USE_FUNC,
		(void*)parse_tw_append   },
	{"pass_provisional_replies", PARAM_INT,
		&default_tm_cfg.pass_provisional_replies },
	{"aggregate_challenges", PARAM_INT, &default_tm_cfg.tm_aggregate_auth    },
	{"unmatched_cancel",    PARAM_INT, &default_tm_cfg.unmatched_cancel      },
	{"default_code",        PARAM_INT, &default_tm_cfg.default_code          },
	{"default_reason",      PARAM_STRING, &default_tm_cfg.default_reason     },
	{"reparse_invite",      PARAM_INT, &default_tm_cfg.reparse_invite        },
	{"ac_extra_hdrs",       PARAM_STR, &default_tm_cfg.ac_extra_hdrs         },
	{"blst_503",            PARAM_INT, &default_tm_cfg.tm_blst_503           },
	{"blst_503_def_timeout",PARAM_INT, &default_tm_cfg.tm_blst_503_default   },
	{"blst_503_min_timeout",PARAM_INT, &default_tm_cfg.tm_blst_503_min       },
	{"blst_503_max_timeout",PARAM_INT, &default_tm_cfg.tm_blst_503_max       },
	{"blst_methods_add",    PARAM_INT, &default_tm_cfg.tm_blst_methods_add   },
	{"blst_methods_lookup", PARAM_INT, &default_tm_cfg.tm_blst_methods_lookup},
	{"cancel_b_method",     PARAM_INT, &default_tm_cfg.cancel_b_flags},
	{"reparse_on_dns_failover", PARAM_INT,
		&default_tm_cfg.reparse_on_dns_failover},
	{"on_sl_reply",         PARAM_STRING|PARAM_USE_FUNC, fixup_on_sl_reply   },
	{"contacts_avp",        PARAM_STR, &contacts_avp                },
	{"contact_flows_avp",   PARAM_STR, &contact_flows_avp           },
	{"disable_6xx_block",   PARAM_INT, &default_tm_cfg.disable_6xx           },
	{"local_ack_mode",      PARAM_INT, &default_tm_cfg.local_ack_mode        },
	{"failure_reply_mode",  PARAM_INT, &failure_reply_mode                   },
	{"faked_reply_prio",    PARAM_INT, &faked_reply_prio                     },
	{"remap_503_500",       PARAM_INT, &tm_remap_503_500                     },
	{"failure_exec_mode",   PARAM_INT, &tm_failure_exec_mode                 },
	{"dns_reuse_rcv_socket",PARAM_INT, &tm_dns_reuse_rcv_socket              },
#ifdef CANCEL_REASON_SUPPORT
	{"local_cancel_reason", PARAM_INT, &default_tm_cfg.local_cancel_reason   },
	{"e2e_cancel_reason",   PARAM_INT, &default_tm_cfg.e2e_cancel_reason     },
#endif /* CANCEL_REASON_SUPPORT */
	{"xavp_contact",        PARAM_STR, &ulattrs_xavp_name                    },
	{"event_callback",      PARAM_STR, &tm_event_callback                    },
	{0,0,0}
};

#ifdef STATIC_TM
struct module_exports tm_exports = {
#else
struct module_exports exports= {
#endif
	"tm",
	/* -------- exported functions ----------- */
	cmds,
	tm_rpc,    /* RPC methods */
	/* ------------ exported variables ---------- */
	params,

	mod_init, /* module initialization function */
	(response_function) reply_received,
	(destroy_function) tm_shutdown,
	0, /* w_onbreak, */
	child_init /* per-child init function */
};



/* helper for fixup_on_* */
static int fixup_routes(char* r_type, struct route_list* rt, void** param)
{
	int i;

	i=route_get(rt, (char*)*param);
	if (i==-1){
		LM_ERR("route_get failed\n");
		return E_UNSPEC;
	}
	if (r_type && rt->rlist[i]==0){
		LM_WARN("%s(\"%s\"): empty/non existing route\n",
				r_type, (char*)*param);
	}
	*param=(void*)(long)i;
	return 0;
}

static int fixup_t_reply(void** param, int param_no)
{
	if (param_no == 1) {
		if (fixup_var_int_12(param, 1) != 0) return -1;
	} else if (param_no == 2) {
		return fixup_var_str_12(param, 2);
	}
	return 0;
}

static int fixup_on_failure(void** param, int param_no)
{
	if (param_no==1){
		if(strlen((char*)*param)<=1
				&& (*(char*)(*param)==0 || *(char*)(*param)=='0')) {
			*param = (void*)0;
			return 0;
		}
		return fixup_routes("t_on_failure", &failure_rt, param);
	}
	return 0;
}

#define BRANCH_FAILURE_ROUTE_PREFIX "tm:branch-failure"
static int fixup_on_branch_failure(void** param, int param_no)
{
	char *full_route_name = NULL;
	int len;
	int ret = 0;
	if (param_no==1){
		if((len = strlen((char*)*param))<=1
				&& (*(char*)(*param)==0 || *(char*)(*param)=='0')) {
			*param = (void*)0;
			return 0;
		}
		len += strlen(BRANCH_FAILURE_ROUTE_PREFIX) + 1;
		if ((full_route_name = pkg_malloc(len+1)) == NULL)
		{
			LM_ERR("No memory left in branch_failure fixup\n");
			return -1;
		}
		sprintf(full_route_name, "%s:%s", BRANCH_FAILURE_ROUTE_PREFIX, (char*)*param);
		*param=(void*)full_route_name;
		ret = fixup_routes("t_on_branch_failure", &event_rt, param);
		pkg_free(full_route_name);
	}
	return ret;
}


static int fixup_on_reply(void** param, int param_no)
{
	if (param_no==1){
		if(strlen((char*)*param)<=1
				&& (*(char*)(*param)==0 || *(char*)(*param)=='0')) {
			*param = (void*)0;
			return 0;
		}
		return fixup_routes("t_on_reply", &onreply_rt, param);
	}
	return 0;
}



static int fixup_on_branch(void** param, int param_no)
{
	if (param_no==1){
		if(strlen((char*)*param)<=1
				&& (*(char*)(*param)==0 || *(char*)(*param)=='0')) {
			*param = (void*)0;
			return 0;
		}
		return fixup_routes("t_on_branch", &branch_rt, param);
	}
	return 0;
}

static int fixup_on_sl_reply(modparam_t type, void* val)
{
	if ((type & PARAM_STRING) == 0) {
		LM_ERR("not a string parameter\n");
		return -1;
	}

	if (fixup_routes(0, &onreply_rt, &val))
		return -1;

	goto_on_sl_reply = (int)(long)val;
	return 0;
}



/* (char *hostname, char *port_nr) ==> (struct proxy_l *, -)  */
static int fixup_hostport2proxy(void** param, int param_no)
{
	unsigned int port;
	char *host;
	int err;
	struct proxy_l *proxy;
	action_u_t *a;
	str s;

	DBG("TM module: fixup_hostport2proxy(%s, %d)\n", (char*)*param, param_no);
	if (param_no==1){
		return 0;
	} else if (param_no==2) {
		a = fixup_get_param(param, param_no, 1);
		host= a->u.string;
		port=str2s(*param, strlen(*param), &err);
		if (err!=0) {
			LM_ERR("bad port number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
		s.s = host;
		s.len = strlen(host);
		proxy=mk_proxy(&s, port, 0); /* FIXME: udp or tcp? */
		if (proxy==0) {
			LM_ERR("bad host name in URI <%s>\n",
					host );
			return E_BAD_ADDRESS;
		}
		/* success -- fix the first parameter to proxy now ! */

		a->u.data=proxy;
		return 0;
	} else {
		LM_ERR("called with parameter number different than {1,2}\n");
		return E_BUG;
	}
}

/* (char *$proto, char *$host:port) ==> (fparam, fparam)  */
static int fixup_proto_hostport2proxy(void** param, int param_no) {
	int ret;

	ret = fix_param(FPARAM_AVP, param);
	if (ret <= 0) return ret;
	if (fix_param(FPARAM_STRING, param) != 0) return -1;
	return 0;
}


static int fixup_t_check_status(void** param, int param_no)
{
	int ret;

	ret = fix_param(FPARAM_AVP, param);
	if (ret <= 0) return ret;

	ret = fix_param(FPARAM_SELECT, param);
	if (ret <= 0) return ret;

	if (fix_param(FPARAM_REGEX, param) != 0) return -1;
	return 0;
}


/***************************** init functions *****************************/
static int w_t_unref( struct sip_msg *foo, unsigned int flags, void *bar)
{
	return t_unref(foo);
}


static int script_init( struct sip_msg *foo, unsigned int flags, void *bar)
{
	/* we primarily reset all private memory here to make sure
	 * private values left over from previous message will
	 * not be used again */

	/* make sure the new message will not inherit previous
	   message's t_on_failure value
	   */
	t_on_failure( 0 );
	t_on_branch_failure(0);
	t_on_reply(0);
	t_on_branch(0);
	/* reset the kr status */
	reset_kr();
	/* set request mode so that multiple-mode actions know
	 * how to behave */
	set_route_type(REQUEST_ROUTE);
	lumps_are_cloned = 0;
	return 1;
}


static int mod_init(void)
{
	DBG( "TM - (sizeof cell=%ld, sip_msg=%ld) initializing...\n",
			(long)sizeof(struct cell), (long)sizeof(struct sip_msg));

	/* checking if we have sufficient bitmap capacity for given
	 * maximum number of  branches */
	if (sr_dst_max_branches+1>31) {
		LM_CRIT("Too many max UACs for UAC branch_bm_t bitmap: %d\n",
				sr_dst_max_branches );
		return -1;
	}

	if (init_callid() < 0) {
		LM_CRIT("Error while initializing Call-ID generator\n");
		return -1;
	}

	/* building the hash table*/
	if (!init_hash_table()) {
		LM_ERR("initializing hash_table failed\n");
		return -1;
	}

	/* init static hidden values */
	init_t();

	if (tm_init_selects()==-1) {
		LM_ERR("select init failed\n");
		return -1;
	}

	/* the default timer values must be fixed-up before
	 * declaring the configuration (Miklos) */
	if (tm_init_timers()==-1) {
		LM_ERR("timer init failed\n");
		return -1;
	}

	/* the cancel branch flags must be fixed before declaring the
	 * configuration */
	if (cancel_b_flags_get(&default_tm_cfg.cancel_b_flags,
				default_tm_cfg.cancel_b_flags)<0){
		LM_ERR("bad cancel branch method\n");
		return -1;
	}

#ifdef USE_DNS_FAILOVER
	if (default_tm_cfg.reparse_on_dns_failover && mhomed) {
		LM_WARN("reparse_on_dns_failover is enabled on a"
				" multihomed host -- check the readme of tm module!\n");
	}
#endif

	/* declare the configuration */
	if (cfg_declare("tm", tm_cfg_def, &default_tm_cfg, cfg_sizeof(tm),
				&tm_cfg)) {
		LM_ERR("failed to declare the configuration\n");
		return -1;
	}

	/* First tm_stat initialization function only allocates the top level stat
	 * structure in shared memory, the initialization will complete in child
	 * init with init_tm_stats_child when the final value of
	 * estimated_process_count is known
	 */
	if (init_tm_stats() < 0) {
		LM_CRIT("failed to init stats\n");
		return -1;
	}

	if (uac_init()==-1) {
		LM_ERR("uac_init failed\n");
		return -1;
	}

	if (init_tmcb_lists()!=1) {
		LM_CRIT("failed to init tmcb lists\n");
		return -1;
	}

	tm_init_tags();
	init_twrite_lines();
	if (init_twrite_sock() < 0) {
		LM_ERR("Unable to create socket\n");
		return -1;
	}

	/* register post-script clean-up function */
	if (register_script_cb( w_t_unref, POST_SCRIPT_CB|REQUEST_CB, 0)<0 ) {
		LM_ERR("failed to register POST request callback\n");
		return -1;
	}
	if (register_script_cb( script_init, PRE_SCRIPT_CB|REQUEST_CB , 0)<0 ) {
		LM_ERR("failed to register PRE request callback\n");
		return -1;
	}

	if (init_avp_params(fr_timer_param, fr_inv_timer_param) < 0) {
		LM_ERR("failed to process AVP params\n");
		return -1;
	}
	if ((contacts_avp.len > 0) && (contact_flows_avp.len == 0)) {
		LM_ERR("contact_flows_avp param has not been defined\n");
		return -1;
	}

#ifdef WITH_EVENT_LOCAL_REQUEST
	goto_on_local_req=route_lookup(&event_rt, "tm:local-request");
	if (goto_on_local_req>=0 && event_rt.rlist[goto_on_local_req]==0)
		goto_on_local_req=-1; /* disable */
	if (goto_on_local_req>=0 || tm_event_callback.len>0)
		set_child_rpc_sip_mode();
#endif /* WITH_EVENT_LOCAL_REQUEST */
	if (goto_on_sl_reply && onreply_rt.rlist[goto_on_sl_reply]==0)
		LM_WARN("empty/non existing on_sl_reply route\n");

#ifdef WITH_TM_CTX
	tm_ctx_init();
#endif
	tm_init = 1;
	return 0;
}

static int child_init(int rank)
{
	if (rank == PROC_INIT) {
		/* we must init stats when rank==PROC_INIT: after mod_init we know
		 * the exact number of processes and we must init the shared structure
		 * before any other process is starting (or else some new process
		 * might try to use the stats before the stats array is allocated) */
		if (init_tm_stats_child() < 0) {
			LM_ERR("Error while initializing tm statistics structures\n");
			return -1;
		}
	}else if (child_init_callid(rank) < 0) {
		/* don't init callid for PROC_INIT*/
		LM_ERR("Error while initializing Call-ID generator\n");
		return -2;
	}
	return 0;
}


/**************************** wrapper functions ***************************/
static int t_check_status(struct sip_msg* msg, char *p1, char *foo)
{
	regmatch_t pmatch;
	struct cell *t;
	char *status, *s = NULL;
	char backup;
	int lowest_status, n, ret;
	fparam_t* fp;
	regex_t* re0 = NULL;
	regex_t* re = NULL;
	str tmp;

	fp = (fparam_t*)p1;
	t = 0;
	/* first get the transaction */
	if (t_check(msg, 0 ) == -1) return -1;
	if ((t = get_t()) == 0) {
		LM_ERR("cannot check status for a reply"
				" which has no T-state established\n");
		goto error;
	}
	backup = 0;

	switch(fp->type) {
		case FPARAM_REGEX:
			re = fp->v.regex;
			break;

		default:
			/* AVP or select, get the value and compile the regex */
			if (get_str_fparam(&tmp, msg, fp) < 0) goto error;
			s = pkg_malloc(tmp.len + 1);
			if (s == NULL) {
				LM_ERR("Out of memory\n");
				goto error;
			}
			memcpy(s, tmp.s, tmp.len);
			s[tmp.len] = '\0';

			if ((re0 = pkg_malloc(sizeof(regex_t))) == 0) {
				LM_ERR("No memory left\n");
				goto error;
			}

			if (regcomp(re0, s, REG_EXTENDED|REG_ICASE|REG_NEWLINE)) {
				LM_ERR("Bad regular expression '%s'\n", s);
				pkg_free(re0);
				re0 = NULL;
				goto error;
			}
			re = re0;
			break;
	}

	switch(get_route_type()) {
		case REQUEST_ROUTE:
			/* use the status of the last sent reply */
			status = int2str( t->uas.status, 0);
			break;

		case TM_ONREPLY_ROUTE:
		case CORE_ONREPLY_ROUTE:
			/* use the status of the current reply */
			status = msg->first_line.u.reply.status.s;
			backup = status[msg->first_line.u.reply.status.len];
			status[msg->first_line.u.reply.status.len] = 0;
			break;

		case FAILURE_ROUTE:
			/* use the status of the winning reply */
			ret = t_pick_branch( -1, 0, t, &lowest_status);
			if (ret == -1) {
				/* t_pick_branch() retuns error also when there are only
				 * blind UACs. Let us give it another chance including the
				 * blind branches. */
				LM_DBG("t_pick_branch returned error,"
						" trying t_pick_branch_blind\n");
				ret = t_pick_branch_blind(t, &lowest_status);
			}
			if (ret < 0) {
				LM_CRIT("BUG: t_pick_branch failed to get"
						" a final response in FAILURE_ROUTE\n");
				goto error;
			}
			status = int2str( lowest_status , 0);
			break;
		case BRANCH_FAILURE_ROUTE:
			status = int2str(t->uac[get_t_branch()].last_received, 0);
			break;
		default:
			LM_ERR("unsupported route type %d\n",
					get_route_type());
			goto error;
	}

	LM_DBG("checked status is <%s>\n",status);
	/* do the checking */
	n = regexec(re, status, 1, &pmatch, 0);

	if (backup) status[msg->first_line.u.reply.status.len] = backup;
	if (s) pkg_free(s);
	if (re0) {
		regfree(re0);
		pkg_free(re0);
	}

	if (unlikely(t && is_route_type(CORE_ONREPLY_ROUTE))){
		/* t_check() above has the side effect of setting T and
		 * REFerencing T => we must unref and unset it.  */
		UNREF( t );
		set_t(T_UNDEFINED, T_BR_UNDEFINED);
	}
	if (n!=0) return -1;
	return 1;

error:
	if (unlikely(t && is_route_type(CORE_ONREPLY_ROUTE))){
		/* t_check() above has the side effect of setting T and
		 * REFerencing T => we must unref and unset it.  */
		UNREF( t );
		set_t(T_UNDEFINED, T_BR_UNDEFINED);
	}
	if (s) pkg_free(s);
	if (re0) {
		regfree(re0);
		pkg_free(re0);
	}
	return -1;
}

static int ki_t_check_status(sip_msg_t* msg, str *sexp)
{
	regmatch_t pmatch;
	struct cell *t;
	char *status, *s = NULL;
	char backup;
	int lowest_status, n, ret;
	regex_t re;

	/* first get the transaction */
	if (t_check(msg, 0 ) == -1) return -1;

	backup = 0;
	if ((t = get_t()) == 0) {
		LM_ERR("cannot check status for a reply"
				" which has no T-state established\n");
		goto error0;
	}

	memset(&re, 0, sizeof(regex_t));
	if (regcomp(&re, sexp->s, REG_EXTENDED|REG_ICASE|REG_NEWLINE)) {
		LM_ERR("Bad regular expression '%s'\n", s);
		goto error0;
	}

	switch(get_route_type()) {
		case REQUEST_ROUTE:
			/* use the status of the last sent reply */
			status = int2str(t->uas.status, 0);
			break;

		case TM_ONREPLY_ROUTE:
		case CORE_ONREPLY_ROUTE:
			/* use the status of the current reply */
			status = msg->first_line.u.reply.status.s;
			backup = status[msg->first_line.u.reply.status.len];
			status[msg->first_line.u.reply.status.len] = 0;
			break;

		case FAILURE_ROUTE:
			/* use the status of the winning reply */
			ret = t_pick_branch( -1, 0, t, &lowest_status);
			if (ret == -1) {
				/* t_pick_branch() retuns error also when there are only
				 * blind UACs. Let us give it another chance including the
				 * blind branches. */
				LM_DBG("t_pick_branch returned error,"
						" trying t_pick_branch_blind\n");
				ret = t_pick_branch_blind(t, &lowest_status);
			}
			if (ret < 0) {
				LM_CRIT("BUG: t_pick_branch failed to get"
						" a final response in FAILURE_ROUTE\n");
				goto error;
			}
			status = int2str(lowest_status, 0);
			break;
		case BRANCH_FAILURE_ROUTE:
			status = int2str(t->uac[get_t_branch()].last_received, 0);
			break;
		default:
			LM_ERR("unsupported route type %d\n",
					get_route_type());
			goto error;
	}

	LM_DBG("checked status is <%s>\n",status);
	/* do the checking */
	n = regexec(&re, status, 1, &pmatch, 0);

	if (backup) status[msg->first_line.u.reply.status.len] = backup;
	regfree(&re);

	if (unlikely(t && is_route_type(CORE_ONREPLY_ROUTE))){
		/* t_check() above has the side effect of setting T and
		 * REFerencing T => we must unref and unset it.  */
		UNREF( t );
		set_t(T_UNDEFINED, T_BR_UNDEFINED);
	}
	if (n!=0) return -1;
	return 1;

error:
	regfree(&re);
error0:
	if (unlikely(t && is_route_type(CORE_ONREPLY_ROUTE))){
		/* t_check() above has the side effect of setting T and
		 * REFerencing T => we must unref and unset it.  */
		UNREF( t );
		set_t(T_UNDEFINED, T_BR_UNDEFINED);
	}

	return -1;
}

static int w_t_check(struct sip_msg* msg, char* str, char* str2)
{
	return (t_check_msg( msg , 0  )==1) ? 1 : -1;
}

static int ki_t_lookup_request(struct sip_msg* msg)
{
	return (t_check_msg( msg , 0  )==1) ? 1 : -1;
}

static int ki_t_lookup_cancel_flags(sip_msg_t* msg, int flags)
{
	struct cell *ret;
	if (msg->REQ_METHOD==METHOD_CANCEL) {
		ret = t_lookupOriginalT( msg );
		LM_DBG("lookup_original: t_lookupOriginalT returned: %p\n", ret);
		if (ret != T_NULL_CELL) {
			/* If the parameter is set to 1, overwrite the message flags of
			 * the CANCEL with the flags of the INVITE */
			if (flags)
				msg->flags = ret->uas.request->flags;

			/* The cell is reffed by t_lookupOriginalT, but T is not set.
			   So we must unref it before returning. */
			UNREF(ret);
			return 1;
		}
	} else {
		LM_WARN("script error - t_lookup_cancel() called for non-CANCEL request\n");
	}
	return -1;
}

static int ki_t_lookup_cancel(sip_msg_t* msg)
{
	return ki_t_lookup_cancel_flags(msg, 0);
}

static int w_t_lookup_cancel(struct sip_msg* msg, char* str, char* str2)
{
	int i = 0;

	if(str) {
		if(get_int_fparam(&i, msg, (fparam_t*)str)<0) return -1;
	}
	return ki_t_lookup_cancel_flags(msg, i);
}

inline static int str2proto(char *s, int len) {
	if (len == 3 && !strncasecmp(s, "udp", 3))
		return PROTO_UDP;
	else if (len == 3 && !strncasecmp(s, "tcp", 3))  /* tcp&tls checks will be
														passed in getproto() */
		return PROTO_TCP;
	else if (len == 3 && !strncasecmp(s, "tls", 3))
		return PROTO_TLS;
	else if (len == 4 && !strncasecmp(s, "sctp", 4))
		return PROTO_SCTP;
	else if (len == 2 && !strncasecmp(s, "ws", 2))
		return PROTO_WS;
	else if (len == 3 && !strncasecmp(s, "wss", 3)) {
		LM_WARN("\"wss\" used somewhere...\n");
		return PROTO_WS;
	} else
		return PROTO_NONE;
}

inline static struct proxy_l* t_protoaddr2proxy(char *proto_par, char *addr_par) {
	struct proxy_l *proxy = 0;
	avp_t* avp;
	avp_value_t val;
	int proto, port, err;
	str s;
	char *c;

	switch(((fparam_t *)proto_par)->type) {
		case FPARAM_AVP:
			if (!(avp = search_first_avp(((fparam_t *)proto_par)->v.avp.flags,
							((fparam_t *)proto_par)->v.avp.name, &val, 0))) {
				proto = PROTO_NONE;
			} else {
				if (avp->flags & AVP_VAL_STR) {
					proto = str2proto(val.s.s, val.s.len);
				}
				else {
					proto = val.n;
				}
			}
			break;

		case FPARAM_INT:
			proto = ((fparam_t *)proto_par)->v.i;
			break;
		case FPARAM_STRING:
			proto = str2proto( ((fparam_t *)proto_par)->v.asciiz,
					strlen(((fparam_t *)proto_par)->v.asciiz));
			break;
		default:
			LM_ERR("Invalid proto parameter value in t_protoaddr2proxy\n");
			return 0;
	}


	switch(((fparam_t *)addr_par)->type) {
		case FPARAM_AVP:
			if (!(avp = search_first_avp(((fparam_t *)addr_par)->v.avp.flags,
							((fparam_t *)addr_par)->v.avp.name, &val, 0))) {
				s.len = 0;
			} else {
				if ((avp->flags & AVP_VAL_STR) == 0) {
					LM_ERR("avp <%.*s> value is not string\n",
							((fparam_t *)addr_par)->v.avp.name.s.len,
							((fparam_t *)addr_par)->v.avp.name.s.s);
					return 0;
				}
				s = val.s;
			}
			break;

		case FPARAM_STRING:
			s.s = ((fparam_t *) addr_par)->v.asciiz;
			s.len = strlen(s.s);
			break;

		default:
			LM_ERR("Invalid addr parameter value in t_protoaddr2proxy\n");
			return 0;
	}

	port = 5060;
	if (s.len) {
		c = memchr(s.s, ':', s.len);
		if (c) {
			port = str2s(c+1, s.len-(c-s.s+1), &err);
			if (err!=0) {
				LM_ERR("bad port number <%.*s>\n", s.len, s.s);
				return 0;
			}
			s.len = c-s.s;
		}
	}
	if (!s.len) {
		LM_ERR("host name is empty\n");
		return 0;
	}
	proxy=mk_proxy(&s, port, proto);
	if (proxy==0) {
		LM_ERR("bad host name in URI <%.*s>\n", s.len, s.s );
		return 0;
	}
	return proxy;
}

static int _w_t_forward_nonack(struct sip_msg* msg, struct proxy_l* proxy,
		int proto)
{
	struct cell *t;
	if (t_check( msg , 0 )==-1) {
		LM_ERR("can't forward when no transaction was set up\n");
		return -1;
	}
	t=get_t();
	if ( t && t!=T_UNDEFINED ) {
		if (msg->REQ_METHOD==METHOD_ACK) {
			LM_WARN("you don't really want to fwd hop-by-hop ACK\n");
			return -1;
		}
		return t_forward_nonack(t, msg, proxy, proto );
	} else {
		LM_DBG("no transaction found\n");
		return -1;
	}
}


static int w_t_forward_nonack( struct sip_msg* msg, char* proxy,
		char* foo)
{
	return _w_t_forward_nonack(msg, ( struct proxy_l *) proxy, PROTO_NONE);
}


static int w_t_forward_nonack_uri(struct sip_msg* msg, char *foo,
		char *bar)
{
	return _w_t_forward_nonack(msg, 0, PROTO_NONE);
}


static int w_t_forward_nonack_udp( struct sip_msg* msg, char* proxy,
		char* foo)
{
	return _w_t_forward_nonack(msg, ( struct proxy_l *) proxy, PROTO_UDP);
}


#ifdef USE_TCP
static int w_t_forward_nonack_tcp( struct sip_msg* msg, char* proxy,
		char* foo)
{
	return _w_t_forward_nonack(msg, ( struct proxy_l *) proxy, PROTO_TCP);
}
#endif


#ifdef USE_TLS
static int w_t_forward_nonack_tls( struct sip_msg* msg, char* proxy,
		char* foo)
{
	return _w_t_forward_nonack(msg, ( struct proxy_l *) proxy, PROTO_TLS);
}
#endif


#ifdef USE_SCTP
static int w_t_forward_nonack_sctp( struct sip_msg* msg, char* proxy,
		char* foo)
{
	return _w_t_forward_nonack(msg, ( struct proxy_l *) proxy, PROTO_SCTP);
}
#endif


static int w_t_forward_nonack_to( struct sip_msg  *p_msg ,
		char *proto_par,
		char *addr_par   )
{
	struct proxy_l *proxy;
	int r = -1;
	proxy = t_protoaddr2proxy(proto_par, addr_par);
	if (proxy) {
		r = _w_t_forward_nonack(p_msg, proxy, proxy->proto);
		free_proxy(proxy);
		pkg_free(proxy);
	}
	return r;
}


static int w_t_reply(struct sip_msg* msg, char* p1, char* p2)
{
	struct cell *t;
	int code, ret = -1;
	str reason;
	char* r;

	if (msg->REQ_METHOD==METHOD_ACK) {
		LM_DBG("ACKs are not replied\n");
		return -1;
	}
	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if (!t) {
		LM_ERR("cannot send a t_reply to a message"
				" for which no T-state has been established\n");
		return -1;
	}

	if (get_int_fparam(&code, msg, (fparam_t*)p1) < 0) {
		code = cfg_get(tm, tm_cfg, default_code);
	}

	if (get_str_fparam(&reason, msg, (fparam_t*)p2) < 0) {
		r = cfg_get(tm, tm_cfg, default_reason);
	} else {
		r = as_asciiz(&reason);
		if (r == NULL) r = cfg_get(tm, tm_cfg, default_reason);
	}

	/* if called from reply_route, make sure that unsafe version
	 * is called; we are already in a mutex and another mutex in
	 * the safe version would lead to a deadlock
	 */

	t->flags |= T_ADMIN_REPLY;
	if (is_route_type(FAILURE_ROUTE)) {
		LM_DBG("t_reply_unsafe called from w_t_reply\n");
		ret = t_reply_unsafe(t, msg, code, r);
	} else if (is_route_type(REQUEST_ROUTE)) {
		ret = t_reply( t, msg, code, r);
	} else if (is_route_type(ONREPLY_ROUTE)) {
		if (likely(t->uas.request)){
			if (is_route_type(CORE_ONREPLY_ROUTE))
				ret=t_reply(t, t->uas.request, code, r);
			else
				ret=t_reply_unsafe(t, t->uas.request, code, r);
		}else
			ret=-1;
		/* t_check() above has the side effect of setting T and
		 * REFerencing T => we must unref and unset it.
		 * Note: this is needed only in the CORE_ONREPLY_ROUTE and not also in
		 * the TM_ONREPLY_ROUTE.
		 */
		if (is_route_type(CORE_ONREPLY_ROUTE)) {
			UNREF( t );
			set_t(T_UNDEFINED, T_BR_UNDEFINED);
		}
	} else {
		LM_CRIT("w_t_reply entered in unsupported mode\n");
		ret = -1;
	}

	if (r && (r != cfg_get(tm, tm_cfg, default_reason))) pkg_free(r);
	return ret;
}


static int t_release(sip_msg_t* msg)
{
	struct cell *t;
	int ret;

	if(get_route_type()!=REQUEST_ROUTE)
	{
		LM_INFO("invalid usage - not in request route\n");
		return -1;
	}

	if (t_check( msg  , 0  )==-1) return -1;
	t=get_t();
	if ( t && t!=T_UNDEFINED ) {
		ret = t_release_transaction( t );
		t_unref(msg);
		return ret;
	}
	return 1;
}

static int w_t_release(struct sip_msg* msg, char* str, char* str2)
{
	return t_release(msg);
}

static int ki_t_retransmit_reply(struct sip_msg* p_msg)
{
	struct cell *t;

	if (t_check( p_msg  , 0 )==-1)
		return 1;
	t=get_t();
	if (t) {
		if (p_msg->REQ_METHOD==METHOD_ACK) {
			LM_WARN("ACKs transmit_replies not replied\n");
			return -1;
		}
		return t_retransmit_reply( t );
	} else
		return -1;
}

static int w_t_retransmit_reply( struct sip_msg* p_msg, char* foo, char* bar)
{
	return ki_t_retransmit_reply(p_msg);
}

static int w_t_newtran( struct sip_msg* p_msg, char* foo, char* bar )
{
	/* t_newtran returns 0 on error (negative value means
	 * 'transaction exists' */
	int ret;
	ret = t_newtran( p_msg );
	if (ret==E_SCRIPT) {
		LM_NOTICE("transaction already in process %p\n", get_t() );
	}
	return ret;
}


static int w_t_on_failure( struct sip_msg* msg, char *go_to, char *foo)
{
	t_on_failure( (unsigned int )(long) go_to );
	return 1;
}


static int w_t_on_branch_failure( struct sip_msg* msg, char *go_to, char *foo)
{
	t_on_branch_failure( (unsigned int )(long) go_to );
	return 1;
}


static int w_t_on_branch( struct sip_msg* msg, char *go_to, char *foo)
{
	t_on_branch( (unsigned int )(long) go_to );
	return 1;
}


static int w_t_on_reply( struct sip_msg* msg, char *go_to, char *foo )
{
	t_on_reply( (unsigned int )(long) go_to );
	return 1;
}


static int t_is_set(sip_msg_t* msg, str *target)
{
	int r;
	tm_cell_t *t = NULL;

	r = 0;
	t = get_t();
	if (t==T_UNDEFINED) t = NULL;

	switch(target->s[0]) {
		case 'b':
			if(t==NULL)
				r = get_on_branch();
			else
				r = t->on_branch;
			break;
		case 'f':
			if(t==NULL)
				r = get_on_failure();
			else
				r = t->on_failure;
			break;
		case 'o':
			if(t==NULL)
				r = get_on_reply();
			else
				r = t->on_reply;
			break;
	}
	if(r) return 1;
	return -1;
}

static int w_t_is_set(struct sip_msg* msg, char *target, char *foo)
{
	str s = STR_NULL;

	s.s = target;
	return t_is_set(msg, &s);
}

static int fixup_t_is_set(void** param, int param_no)
{
	int len;
	if (param_no==1) {
		len = strlen((char*)*param);
		if((len==13 && strncmp((char*)*param, "failure_route", 13)==0)
				|| (len==13 && strncmp((char*)*param, "onreply_route", 13)==0)
				|| (len==12 && strncmp((char*)*param, "branch_route", 12)==0)) {
			return 0;
		}

		LM_ERR("invalid parameter value: %s\n", (char*)*param);
		return 1;
	}
	return 0;
}

static int _w_t_relay_to(struct sip_msg  *p_msg ,
		struct proxy_l *proxy, int force_proto)
{
	struct cell *t;
	int res;

	if (is_route_type(FAILURE_ROUTE|BRANCH_FAILURE_ROUTE)) {
		t=get_t();
		if (!t || t==T_UNDEFINED) {
			LM_CRIT("undefined T\n");
			return -1;
		}
		res = t_forward_nonack(t, p_msg, proxy, force_proto);
		if (res <= 0) {
			if (res != E_CFG) {
				LM_ERR("t_forward_noack failed\n");
				/* let us save the error code, we might need it later
				 * when the failure_route has finished (Miklos) */
			}
			tm_error=ser_error;
			return -1;
		}
		return 1;
	}
	if (is_route_type(REQUEST_ROUTE))
		return t_relay_to( p_msg, proxy, force_proto,
				0 /* no replication */ );
	LM_CRIT("unsupported route type: %d\n", get_route_type());
	return 0;
}


static int w_t_relay_to_udp( struct sip_msg  *p_msg ,
		char *proxy,/* struct proxy_l * expected */
		char *_foo       /* nothing expected */ )
{
	return _w_t_relay_to( p_msg, ( struct proxy_l *) proxy, PROTO_UDP);
}

/* forward to uri, but force udp as transport */
static int w_t_relay_to_udp_uri( struct sip_msg  *p_msg ,
		char *_foo, char *_bar   )
{
	return _w_t_relay_to(p_msg, (struct proxy_l *)0, PROTO_UDP);
}


#ifdef USE_TCP
static int w_t_relay_to_tcp( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l* */
		char *_foo       /* nothing expected */ )
{
	return _w_t_relay_to( p_msg, ( struct proxy_l *) proxy, PROTO_TCP);
}

/* forward to uri, but force tcp as transport */
static int w_t_relay_to_tcp_uri( struct sip_msg  *p_msg ,
		char *_foo, char *_bar   )
{
	return _w_t_relay_to(p_msg, (struct proxy_l *)0, PROTO_TCP);
}
#endif


#ifdef USE_TLS
static int w_t_relay_to_tls( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l* expected */
		char *_foo       /* nothing expected */ )
{
	return _w_t_relay_to( p_msg, ( struct proxy_l *) proxy, PROTO_TLS);
}

/* forward to uri, but force tls as transport */
static int w_t_relay_to_tls_uri( struct sip_msg  *p_msg ,
		char *_foo, char *_bar   )
{
	return _w_t_relay_to(p_msg, (struct proxy_l *)0, PROTO_TLS);
}
#endif


#ifdef USE_SCTP
static int w_t_relay_to_sctp( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l* */
		char *_foo       /* nothing expected */ )
{
	return _w_t_relay_to( p_msg, ( struct proxy_l *) proxy, PROTO_SCTP);
}

/* forward to uri, but force tcp as transport */
static int w_t_relay_to_sctp_uri( struct sip_msg  *p_msg ,
		char *_foo, char *_bar   )
{
	return _w_t_relay_to(p_msg, (struct proxy_l *)0, PROTO_SCTP);
}
#endif


static int w_t_relay_to_avp( struct sip_msg  *p_msg ,
		char *proto_par,
		char *addr_par   )
{
	struct proxy_l *proxy;
	int r = -1;

	proxy = t_protoaddr2proxy(proto_par, addr_par);
	if (proxy) {
		r = _w_t_relay_to(p_msg, proxy, PROTO_NONE);
		free_proxy(proxy);
		pkg_free(proxy);
	}
	return r;
}

int t_replicate_uri(struct sip_msg *msg, str *suri)
{
	struct proxy_l *proxy = NULL;
	struct sip_uri turi;
	int r = -1;

	if (suri != NULL && suri->s != NULL && suri->len > 0)
	{
		memset(&turi, 0, sizeof(struct sip_uri));
		if(parse_uri(suri->s, suri->len, &turi)!=0)
		{
			LM_ERR("bad replicate SIP address!\n");
			return -1;
		}

		proxy=mk_proxy(&turi.host, turi.port_no, turi.proto);
		if (proxy==0) {
			LM_ERR("cannot create proxy from URI <%.*s>\n",
					suri->len, suri->s );
			return -1;
		}

		r = t_replicate(msg, proxy, proxy->proto);
		free_proxy(proxy);
		pkg_free(proxy);
	} else {
		r = t_replicate(msg, NULL, 0);
	}
	return r;
}

static int w_t_replicate_uri(struct sip_msg  *msg ,
		char *uri,       /* sip uri as string or variable */
		char *_foo       /* nothing expected */ )
{
	str suri;

	if(uri==NULL)
		return t_replicate_uri(msg, NULL);

	if(fixup_get_svalue(msg, (gparam_p)uri, &suri)!=0)
	{
		LM_ERR("invalid replicate uri parameter");
		return -1;
	}
	return t_replicate_uri(msg, &suri);
}

static int w_t_replicate( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, p_msg->rcv.proto );
}

static int w_t_replicate_udp( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_UDP );
}


#ifdef USE_TCP
static int w_t_replicate_tcp( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_TCP );
}
#endif


#ifdef USE_TLS
static int w_t_replicate_tls( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_TLS );
}
#endif


#ifdef USE_SCTP
static int w_t_replicate_sctp( struct sip_msg  *p_msg ,
		char *proxy, /* struct proxy_l *proxy expected */
		char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_SCTP );
}
#endif


static int w_t_replicate_to( struct sip_msg  *p_msg ,
		char *proto_par,
		char *addr_par   )
{
	struct proxy_l *proxy;
	int r = -1;
	proxy = t_protoaddr2proxy(proto_par, addr_par);
	if (proxy) {
		r = t_replicate(p_msg, proxy, proxy->proto);
		free_proxy(proxy);
		pkg_free(proxy);
	}
	return r;
}

static int w_t_relay( struct sip_msg  *p_msg ,
		char *_foo, char *_bar)
{
	return _w_t_relay_to(p_msg, (struct proxy_l *)0, PROTO_NONE);
}


/* like t_relay but use the specified destination and port and the same proto
 * as the received msg */
static int w_t_relay2( struct sip_msg  *p_msg , char *proxy,
		char *_foo)
{
	return _w_t_relay_to(p_msg, (struct proxy_l*) proxy, p_msg->rcv.proto);
}


/* relays CANCEL at the beginning of the script */
static int w_t_relay_cancel( struct sip_msg  *p_msg ,
		char *_foo, char *_bar)
{
	if (p_msg->REQ_METHOD!=METHOD_CANCEL)
		return 1;

	/* it makes no sense to use this function without reparse_invite=1 */
	if (!cfg_get(tm, tm_cfg, reparse_invite))
		LM_WARN("probably used with wrong configuration,"
				" check the readme for details\n");

	return t_relay_cancel(p_msg);
}

/* set fr_inv_timeout & or fr_timeout; 0 means: use the default value */
static int t_set_fr_all(struct sip_msg* msg, char* p1, char* p2)
{
	int fr, fr_inv;

	if (get_int_fparam(&fr_inv, msg, (fparam_t*)p1) < 0) return -1;
	if (p2) {
		if (get_int_fparam(&fr, msg, (fparam_t*)p2) < 0) return -1;
	} else {
		fr = 0;
	}

	return t_set_fr(msg, fr_inv, fr);
}

static int t_set_fr_inv(struct sip_msg* msg, char* fr_inv, char* foo)
{
	return t_set_fr_all(msg, fr_inv, (char*)0);
}

static int ki_t_set_fr(struct sip_msg* msg, int fr_inv, int fr)
{
	return t_set_fr(msg, fr_inv, fr);
}

static int ki_t_set_fr_inv(struct sip_msg* msg, int fr_inv)
{
	return t_set_fr(msg, fr_inv, 0);
}

/* reset fr_timer and fr_inv_timer to the default values */
static int w_t_reset_fr(struct sip_msg* msg, char* foo, char* bar)
{
	return t_reset_fr();
}

static int ki_t_reset_fr(struct sip_msg* msg)
{
	return t_reset_fr();
}

static int ki_t_set_retr(sip_msg_t* msg, int t1, int t2)
{
#ifdef TM_DIFF_RT_TIMEOUT
	return t_set_retr(msg, t1, t2);
#else
	LM_ERR("support for changing retransmission intervals on "
			"the fly not compiled in (re-compile tm with"
			" -DTM_DIFF_RT_TIMEOUT)\n");
	return -1;
#endif
}

/* set retr. intervals per transaction; 0 means: use the default value */
static int w_t_set_retr(struct sip_msg* msg, char* p1, char* p2)
{
	int t1, t2;

	if (get_int_fparam(&t1, msg, (fparam_t*)p1) < 0) return -1;
	if (p2) {
		if (get_int_fparam(&t2, msg, (fparam_t*)p2) < 0) return -1;
	} else {
		t2 = 0;
	}
	return ki_t_set_retr(msg, t1, t2);
}

/* reset retr. t1 and t2 to the default values */
int ki_t_reset_retr(sip_msg_t* msg)
{
#ifdef TM_DIFF_RT_TIMEOUT
	return t_reset_retr();
#else
	LM_ERR("support for changing retransmission intervals on "
			"the fly not compiled in (re-compile tm with"
			" -DTM_DIFF_RT_TIMEOUT)\n");
	return -1;
#endif
}

int w_t_reset_retr(struct sip_msg* msg, char* foo, char* bar)
{
	return ki_t_reset_retr(msg);
}

/* set maximum transaction lifetime for inv & noninv */
static int w_t_set_max_lifetime(struct sip_msg* msg, char* p1, char* p2)
{
	int t1, t2;

	if (get_int_fparam(&t1, msg, (fparam_t*)p1) < 0) return -1;
	if (p2) {
		if (get_int_fparam(&t2, msg, (fparam_t*)p2) < 0) return -1;
	} else {
		t2 = 0;
	}
	return t_set_max_lifetime(msg, t1, t2);
}

static int ki_t_set_max_lifetime(sip_msg_t* msg, int t1, int t2)
{
	return t_set_max_lifetime(msg, t1, t2);
}

/* reset maximum invite/non-invite lifetime to the default value */
int w_t_reset_max_lifetime(struct sip_msg* msg, char* foo, char* bar)
{
	return t_reset_max_lifetime();
}

int ki_t_reset_max_lifetime(sip_msg_t* msg)
{
	return t_reset_max_lifetime();
}


/**
 * helper macro, builds a function for setting a cell flag from the script.
 * e.g. W_T_SET_FLAG_GEN_FUNC(t_set_foo, T_FOO) =>
 * static int t_set_foo(struct sip_msg* msg, char*, char* )
 * that will expect fparam as first param and will set or reset T_FOO
 * in the current or next to be created transaction. */
#define T_SET_FLAG_GEN_FUNC(fname, T_FLAG_NAME) \
	static int fname(sip_msg_t* msg, int state) \
{ \
	struct cell* t; \
	unsigned int set_flags; \
	unsigned int reset_flags; \
	\
	t=get_t(); \
	/* in REPLY_ROUTE and FAILURE_ROUTE T will be set to current transaction; \
	 * in REQUEST_ROUTE T will be set only if the transaction was already  \
	 * created; if not -> use the static variables */ \
	if (!t || t==T_UNDEFINED ){ \
		set_flags=get_msgid_val(user_cell_set_flags, msg->id, int); \
		reset_flags=get_msgid_val(user_cell_reset_flags, msg->id, int); \
		if (state){ \
			/* set */ \
			set_flags|= T_FLAG_NAME; \
			reset_flags&=~T_FLAG_NAME; \
		}else{ \
			/* reset */ \
			set_flags&=~T_FLAG_NAME; \
			reset_flags|=T_FLAG_NAME; \
		} \
		set_msgid_val(user_cell_set_flags, msg->id, int, set_flags); \
		set_msgid_val(user_cell_reset_flags, msg->id, int, reset_flags); \
	}else{ \
		if (state) \
		t->flags|=T_FLAG_NAME; \
		else \
		t->flags&=~T_FLAG_NAME; \
	} \
	return 1; \
}

#define W_T_SET_FLAG_GEN_FUNC(fname, T_FLAG_NAME) \
	static int w_##fname(sip_msg_t* msg, char* p1, char* p2) \
{ \
	int state; \
	if (get_int_fparam(&state, msg, (fparam_t*)p1) < 0) return -1; \
	return fname(msg, state); \
}

/* set automatically sending 100 replies on/off for the current or
 * next to be created transaction */
T_SET_FLAG_GEN_FUNC(t_set_auto_inv_100, T_AUTO_INV_100)

W_T_SET_FLAG_GEN_FUNC(t_set_auto_inv_100, T_AUTO_INV_100)


/* set 6xx handling for the current or next to be created transaction */
T_SET_FLAG_GEN_FUNC(t_set_disable_6xx, T_DISABLE_6xx)

W_T_SET_FLAG_GEN_FUNC(t_set_disable_6xx, T_DISABLE_6xx)


/* disable dns failover for the current transaction */
T_SET_FLAG_GEN_FUNC(t_set_disable_failover, T_DISABLE_FAILOVER)

W_T_SET_FLAG_GEN_FUNC(t_set_disable_failover, T_DISABLE_FAILOVER)


#ifdef CANCEL_REASON_SUPPORT
/* disable/enable e2e cancel reason copy for the current transaction */
T_SET_FLAG_GEN_FUNC(t_set_no_e2e_cancel_reason, T_NO_E2E_CANCEL_REASON)

W_T_SET_FLAG_GEN_FUNC(t_set_no_e2e_cancel_reason, T_NO_E2E_CANCEL_REASON)
#endif /* CANCEL_REASON_SUPPORT */


/* disable internal negative reply for the current transaction */
T_SET_FLAG_GEN_FUNC(t_set_disable_internal_reply, T_DISABLE_INTERNAL_REPLY)

W_T_SET_FLAG_GEN_FUNC(t_set_disable_internal_reply, T_DISABLE_INTERNAL_REPLY)


/* FAILURE_ROUTE and BRANCH_FAILURE_ROUTE only,
 * returns true if the choosed "failure" branch failed because of a timeout,
 * -1 otherwise */
int t_branch_timeout(sip_msg_t* msg)
{
	switch(get_route_type()) {
		case FAILURE_ROUTE:
		case BRANCH_FAILURE_ROUTE:
			return (msg->msg_flags & FL_TIMEOUT)?1:-1;
		default:
			LM_ERR("unsupported route type %d\n", get_route_type());
	}
	return -1;
}


/* script function, FAILURE_ROUTE and BRANCH_FAILURE_ROUTE only,
 * returns true if the choosed "failure" branch failed because of a timeout,
 * -1 otherwise */
int w_t_branch_timeout(sip_msg_t* msg, char* foo, char* bar)
{
	return t_branch_timeout(msg);
}

/* FAILURE_ROUTE and BRANCH_FAILURE_ROUTE only,
 * returns true if the choosed "failure" branch ever received a reply,
 * -1 otherwise */
int t_branch_replied(sip_msg_t* msg)
{
	switch(get_route_type()) {
		case FAILURE_ROUTE:
		case BRANCH_FAILURE_ROUTE:
			return (msg->msg_flags & FL_REPLIED)?1:-1;
		default:
			LM_ERR("unsupported route type %d\n", get_route_type());
	}
	return -1;
}


/* script function, FAILURE_ROUTE and BRANCH_FAILURE_ROUTE only,
 * returns true if the choosed "failure" branch ever received a reply,
 * -1 otherwise */
int w_t_branch_replied(sip_msg_t* msg, char* foo, char* bar)
{
	return t_branch_replied(msg);
}

/* script function, returns: 1 if the transaction was canceled, -1 if not */
int t_is_canceled(struct sip_msg* msg)
{
	struct cell *t;
	int ret;

	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if ((t==0) || (t==T_UNDEFINED)){
		LM_ERR("cannot check a message"
				" for which no T-state has been established\n");
		ret=-1;
	}else{
		ret=(t->flags & T_CANCELED)?1:-1;
	}
	return ret;
}

static int w_t_is_canceled(sip_msg_t* msg, char* foo, char* bar)
{
	return t_is_canceled(msg);
}

/* returns: 1 if the transaction is currently suspended, -1 if not */
int t_is_retr_async_reply(sip_msg_t* msg)
{
	struct cell *t;
	int ret;

	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if ((t==0) || (t==T_UNDEFINED)){
		LM_ERR("cannot check a message"
				" for which no T-state has been established\n");
		ret=-1;
	}else{
		LM_DBG("TRANSACTION FLAGS IS %d\n", t->flags);
		ret=(t->flags & T_ASYNC_SUSPENDED)?1:-1;
	}
	return ret;
}

/* script function, returns: 1 if the transaction is currently suspended,
 * -1 if not */
static int w_t_is_retr_async_reply(sip_msg_t* msg, char* foo, char* bar)
{
	return t_is_retr_async_reply(msg);
}

/* returns: 1 if the transaction lifetime interval has already elapsed, -1 if not */
int t_is_expired(sip_msg_t* msg)
{
	struct cell *t;
	int ret;

	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if ((t==0) || (t==T_UNDEFINED)){
		LM_ERR("cannot check a message"
				" for which no T-state has been established\n");
		ret=-1;
	}else{
		ret=(TICKS_GT(t->end_of_life, get_ticks_raw()))?-1:1;
	}
	return ret;
}

/* script function, returns: 1 if the transaction lifetime interval
 * has already elapsed, -1 if not */
int w_t_is_expired(sip_msg_t* msg, char* foo, char* bar)
{
	return t_is_expired(msg);
}

/* returns: 1 if any of the branches did timeout, -1 if not */
int t_any_timeout(sip_msg_t* msg)
{
	struct cell *t;
	int r;

	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if ((t==0) || (t==T_UNDEFINED)){
		LM_ERR("cannot check a message"
				" for which no T-state has been established\n");
		return -1;
	}else{
		for (r=0; r<t->nr_of_outgoings; r++){
			if (t->uac[r].request.flags & F_RB_TIMEOUT)
				return 1;
		}
	}
	return -1;
}


/* script function, returns: 1 if any of the branches did timeout, -1 if not */
int w_t_any_timeout(sip_msg_t* msg, char* foo, char* bar)
{
	return t_any_timeout(msg);
}

/* returns: 1 if any of the branches received at leat one
 * reply, -1 if not */
int t_any_replied(sip_msg_t* msg)
{
	struct cell *t;
	int r;

	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if ((t==0) || (t==T_UNDEFINED)){
		LM_ERR("cannot check a message"
				" for which no T-state has been established\n");
		return -1;
	}else{
		for (r=0; r<t->nr_of_outgoings; r++){
			if (t->uac[r].request.flags & F_RB_REPLIED)
				return 1;
		}
	}
	return -1;
}


/* script function, returns: 1 if any of the branches received at leat one
 * reply, -1 if not */
int w_t_any_replied(sip_msg_t* msg, char* foo, char* bar)
{
	return t_any_replied(msg);
}


/* returns: 1 if any of the branches received the
 *  reply code "status" */
int t_grep_status(sip_msg_t* msg, int code)
{
	struct cell *t;
	int r;

	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if ((t==0) || (t==T_UNDEFINED)){
		LM_ERR("cannot check a message"
				" for which no T-state has been established\n");
		return -1;
	}else{
		for (r=0; r<t->nr_of_outgoings; r++){
			if ((t->uac[r].last_received==code)  &&
					(t->uac[r].request.flags & F_RB_REPLIED))
				return 1;
		}
	}
	return -1;
}


/* script function, returns: 1 if any of the branches received the
 *  reply code "status" */
int w_t_grep_status(struct sip_msg* msg, char* status, char* bar)
{
	int code;

	if (get_int_fparam(&code, msg, (fparam_t*)status) < 0) return -1;

	return t_grep_status(msg, code);
}

/* drop all the existing replies in failure_route to make sure
 * that none of them is picked up again */
static int t_drop_replies_helper(sip_msg_t* msg, char* mode)
{
	if(mode==NULL)
		t_drop_replies(1);
	else if(*mode=='n')
		t_drop_replies(0);
	else if(*mode=='l')
		t_drop_replies(2);
	else
		t_drop_replies(1);
	return 1;
}

static int w_t_drop_replies(struct sip_msg* msg, char* mode, char* bar)
{
	return t_drop_replies_helper(msg, mode);
}

static int ki_t_drop_replies(sip_msg_t* msg, str* mode)
{
	return t_drop_replies_helper(msg, (mode)?mode->s:NULL);
}

static int ki_t_drop_replies_all(sip_msg_t* msg)
{
	return t_drop_replies_helper(msg, NULL);
}

/* save the message lumps after t_newtran() but before t_relay() */
static int ki_t_save_lumps(sip_msg_t* msg)
{
	struct cell *t;

	if (is_route_type(REQUEST_ROUTE)) {
		t=get_t();
		if (!t || t==T_UNDEFINED) {
			LM_ERR("transaction has not been created yet\n");
			return -1;
		}

		if (save_msg_lumps(t->uas.request, msg)) {
			LM_ERR("failed to save the message lumps\n");
			return -1;
		}
	} /* else nothing to do, the lumps have already been saved */
	return 1;
}

static int w_t_save_lumps(struct sip_msg* msg, char* foo, char* bar)
{
	return ki_t_save_lumps(msg);
}

/* wrapper function needed after changes in w_t_reply */
int w_t_reply_wrp(struct sip_msg *m, unsigned int code, char *txt)
{
	fparam_t c;
	fparam_t r;

	c.type = FPARAM_INT;
	c.orig = NULL; /* ? */
	c.v.i = code;

	r.type = FPARAM_STRING;
	r.orig = NULL; /* ? */
	r.v.asciiz = txt;

	return w_t_reply(m, (char *)&c, (char*)&r);
}



/** script function, check if a msg is assoc. to a transaction.
 * @return -1 (not), 1 (reply, e2e ack or cancel for an existing transaction),
 *          0 (request retransmission, ack to negative reply or ack to local
 *           transaction)
 * Note: the e2e ack matching works only for local e2e acks or for
 *       transactions with E2EACK* callbacks installed (but even in this
 *       case matching E2EACKs on proxied transaction is not entirely
 *       reliable: if the ACK  is delayed the proxied transaction might
 *       be already deleted when it reaches the proxy (wait_timeout))
 */
int t_check_trans(struct sip_msg* msg)
{
	struct cell* t;
	int branch;
	int ret;

	/* already processing a T */
	if(is_route_type(FAILURE_ROUTE)
			|| is_route_type(BRANCH_ROUTE)
			|| is_route_type(BRANCH_FAILURE_ROUTE)
			|| is_route_type(TM_ONREPLY_ROUTE)) {
		return 1;
	}

	if (msg->first_line.type==SIP_REPLY) {
		branch = 0;
		ret = (t_check_msg( msg , &branch)==1) ? 1 : -1;
		tm_ctx_set_branch_index(branch);
		return ret;
	} else if (msg->REQ_METHOD==METHOD_CANCEL) {
		return w_t_lookup_cancel(msg, 0, 0);
	} else {
		switch(t_check_msg(msg, 0)){
			case -2: /* possible e2e ack */
				return 1;
			case 1: /* found */
				t=get_t();
				if (msg->REQ_METHOD==METHOD_ACK){
					/* ack to neg. reply  or ack to local trans.
					 * => process it and end the script */
					/* FIXME: there's no way to distinguish here
					 * between acks to local trans. and neg. acks */
					if (unlikely(has_tran_tmcbs(t, TMCB_ACK_NEG_IN)))
						run_trans_callbacks(TMCB_ACK_NEG_IN, t, msg,
								0, msg->REQ_METHOD);
					t_release_transaction(t);
				} else {
					/* is a retransmission */
					if (unlikely(has_tran_tmcbs(t, TMCB_REQ_RETR_IN)))
						run_trans_callbacks(TMCB_REQ_RETR_IN, t, msg,
								0, msg->REQ_METHOD);
					t_retransmit_reply(t);
				}
				/* no need for UNREF(t); set_t(0) - the end-of-script
				 * t_unref callback will take care of them */
				return 0; /* exit from the script */
		}
		/* not found or error */
	}
	return -1;
}

static int w_t_check_trans(struct sip_msg* msg, char* foo, char* bar)
{
	return t_check_trans(msg);
}

static int hexatoi(str *s, unsigned int* result)
{
	int i, xv, fact;

	/* more than 32bit hexa? */
	if (s->len>8)
		return -1;

	*result = 0;
	fact = 1;

	for(i=s->len-1; i>=0 ;i--)
	{
		xv = hex2int(s->s[i]);
		if(xv<0)
			return -1;

		*result += (xv * fact);
		fact *= 16;
	}
	return 0;
}

static int fixup_t_relay_to(void** param, int param_no)
{

	int port;
	int proto;
	unsigned int flags;
	struct proxy_l *proxy;
	action_u_t *a;
	str s;
	str host;

	s.s = (char*)*param;
	s.len = strlen(s.s);
	LM_DBG("fixing (%s, %d)\n", s.s, param_no);
	if (param_no==1){
		a = fixup_get_param(param, param_no, 2);
		if(a==NULL)
		{
			LM_CRIT("server error for parameter <%s>\n",s.s);
			return E_UNSPEC;
		}
		if(a->u.string!=NULL) {
			/* second parameter set, first should be proxy addr */
			if (parse_phostport(s.s, &host.s, &host.len, &port, &proto)!=0){
				LM_CRIT("invalid proxy addr parameter <%s>\n",s.s);
				return E_UNSPEC;
			}

			proxy = mk_proxy(&host, port, proto);
			if (proxy==0) {
				LM_ERR("failed to build proxy structure for <%.*s>\n",
						host.len, host.s );
				return E_UNSPEC;
			}
			*(param)=proxy;
			return 0;
		} else {
			/* no second parameter, then is proxy addr or flags */
			flags = 0;
			if (s.len>2 && s.s[0]=='0' && s.s[1]=='x') {
				s.s += 2;
				s.len -= 2;
				if(hexatoi(&s, &flags)<0)
				{
					LM_CRIT("invalid hexa flags <%s>\n", s.s);
					return E_UNSPEC;
				}
				a->u.data = (void*)(unsigned long int)flags;
				*(param)= 0;
				return 0;
			} else {
				if(str2int(&s, &flags)==0)
				{
					a->u.data = (void*)(unsigned long int)flags;
					*(param)= 0;
					return 0;
				} else {
					/* try proxy */
					if (parse_phostport(s.s, &host.s, &host.len,
								&port, &proto)!=0){
						LM_CRIT("invalid proxy addr parameter <%s>\n",s.s);
						return E_UNSPEC;
					}

					proxy = mk_proxy(&host, port, proto);
					if (proxy==0) {
						LM_ERR("failed to build proxy structure for <%.*s>\n",
								host.len, host.s );
						return E_UNSPEC;
					}
					*(param)=proxy;
					return 0;
				}
			}
		}
	} else if (param_no==2) {
		/* flags */
		flags = 0;
		if (s.len>2 && s.s[0]=='0' && s.s[1]=='x') {
			s.s += 2;
			s.len -= 2;
			if(hexatoi(&s, &flags)<0)
			{
				LM_CRIT("invalid hexa flags <%s>\n", s.s);
				return E_UNSPEC;
			}
			*(param) = (void*)(unsigned long int)flags;
			return 0;
		} else {
			if(str2int(&s, &flags)==0)
			{
				*(param) = (void*)(unsigned long int)flags;
				return 0;
			} else {
				LM_CRIT("invalid flags <%s>\n", s.s);
				return E_UNSPEC;
			}
		}
	} else {
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_BUG;
	}
}


static int w_t_relay_to(struct sip_msg *msg, char *proxy, char *flags)
{
	unsigned int fl;
	struct proxy_l *px;
	fparam_t param;

	fl = (unsigned int)(long)(void*)flags;
	px = (struct proxy_l*)proxy;

	if(flags!=0)
	{
		memset(&param, 0, sizeof(fparam_t));
		param.type = FPARAM_INT;
		/* no auto 100 trying */
		if(fl&1) {
			param.v.i = 0;
			w_t_set_auto_inv_100(msg, (char*)(&param), 0);
		}
		/* no auto negative reply */
		if(fl&2) {
			param.v.i = 1;
			w_t_set_disable_internal_reply(msg, (char*)(&param), 0);
		}
		/* no dns failover */
		if(fl&4) {
			param.v.i = 1;
			w_t_set_disable_failover(msg, (char*)(&param), 0);
		}
	}
	return _w_t_relay_to(msg, px, PROTO_NONE);
}


static int ki_t_use_uac_headers(sip_msg_t* msg)
{
	tm_cell_t *t;

	t=get_t();
	if (t!=NULL && t!=T_UNDEFINED) {
		t->uas.request->msg_flags |= FL_USE_UAC_FROM|FL_USE_UAC_TO;
	}
	msg->msg_flags |= FL_USE_UAC_FROM|FL_USE_UAC_TO;

	return 1;
}

static int w_t_use_uac_headers(sip_msg_t* msg, char* foo, char* bar)
{
	return ki_t_use_uac_headers(msg);
}

static int w_t_uac_send(sip_msg_t* msg, char* pmethod, char* pruri,
		char* pnexthop, char* psock, char *phdrs, char* pbody)
{
	str method = STR_NULL;
	str ruri = STR_NULL;
	str nexthop = STR_NULL;
	str send_socket = STR_NULL;
	str headers = STR_NULL;
	str body = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pmethod, &method)!=0) {
		LM_ERR("invalid method parameter");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pruri, &ruri)!=0) {
		LM_ERR("invalid ruri parameter");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pnexthop, &nexthop)!=0) {
		LM_ERR("invalid nexthop parameter");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)psock, &send_socket)!=0) {
		LM_ERR("invalid send socket parameter");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)phdrs, &headers)!=0) {
		LM_ERR("invalid headers parameter");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pbody, &body)!=0) {
		LM_ERR("invalid body parameter");
		return -1;
	}

	if(t_uac_send(&method, &ruri, &nexthop, &send_socket, &headers, &body)<0) {
		return -1;
	}
	return 1;
}

static int ki_t_uac_send(sip_msg_t* msg, str* method, str* ruri,
		str* nexthop, str* ssock, str *hdrs, str* body)
{
	if(t_uac_send(method, ruri, nexthop, ssock, hdrs, body)<0) {
		return -1;
	}
	return 1;
}

/* rpc docs */

static const char* rpc_cancel_doc[2] = {
	"Cancel a pending transaction",
	0
};

static const char* rpc_reply_doc[2] = {
	"Reply transaction",
	0
};

static const char* rpc_reply_callid_doc[2] = {
	"Reply transaction by call-id",
	0
};

static const char* tm_rpc_stats_doc[2] = {
	"Print transaction statistics.",
	0
};

static const char* tm_rpc_hash_stats_doc[2] = {
	"Prints hash table statistics (can be used only if tm is compiled"
		" with -DTM_HASH_STATS).",
	0
};

static const char* rpc_t_uac_start_doc[2] = {
	"starts a tm uac using  a list of string parameters: method, ruri, dst_uri"
		", send_sock, headers (CRLF separated) and body (optional)",
	0
};

static const char* rpc_t_uac_wait_doc[2] = {
	"starts a tm uac and waits for the final reply, using a list of string "
		"parameters: method, ruri, dst_uri send_sock, headers (CRLF separated)"
		" and body (optional)",
	0
};

static const char* tm_rpc_list_doc[2] = {
	"List transactions.",
	0
};

static const char* tm_rpc_clean_doc[2] = {
	"Clean expired (lifetime exceeded) transactions.",
	0
};


/* rpc exports */
static rpc_export_t tm_rpc[] = {
	{"tm.cancel", rpc_cancel,   rpc_cancel_doc,   0},
	{"tm.reply",  rpc_reply,    rpc_reply_doc,    0},
	{"tm.reply_callid", rpc_reply_callid,   rpc_reply_callid_doc,   0},
	{"tm.stats",  tm_rpc_stats, tm_rpc_stats_doc, 0},
	{"tm.hash_stats",  tm_rpc_hash_stats, tm_rpc_hash_stats_doc, 0},
	{"tm.t_uac_start", rpc_t_uac_start, rpc_t_uac_start_doc, 0 },
	{"tm.t_uac_wait",  rpc_t_uac_wait,  rpc_t_uac_wait_doc, RET_ARRAY},
	{"tm.list",  tm_rpc_list,  tm_rpc_list_doc, RET_ARRAY},
	{"tm.clean", tm_rpc_clean,  tm_rpc_clean_doc, 0},
	{0, 0, 0, 0}
};

/**
 *
 */
static int ki_t_on_failure(sip_msg_t *msg, str *rname)
{
	int ridx;
	sr_kemi_eng_t *keng;

	if(rname==NULL || rname->s==NULL || rname->len<=0 || rname->s[0]=='\0') {
		ridx = 0;
	} else {
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			ridx = route_get(&failure_rt, rname->s);
		} else {
			ridx = sr_kemi_cbname_lookup_name(rname);
		}
	}
	if(ridx<0) { ridx = 0; }

	t_on_failure(ridx);
	return 1;
}

/**
 *
 */
static int ki_t_on_branch_failure(sip_msg_t *msg, str *rname)
{
	int ridx;
	sr_kemi_eng_t *keng;

	if(rname==NULL || rname->s==NULL || rname->len<=0 || rname->s[0]=='\0') {
		ridx = 0;
	} else {
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			ridx = route_get(&event_rt, rname->s);
		} else {
			ridx = sr_kemi_cbname_lookup_name(rname);
		}
	}
	if(ridx<0) { ridx = 0; }

	t_on_branch_failure(ridx);
	return 1;
}


/**
 *
 */
static int ki_t_on_branch(sip_msg_t *msg, str *rname)
{
	int ridx;
	sr_kemi_eng_t *keng;

	if(rname==NULL || rname->s==NULL || rname->len<=0 || rname->s[0]=='\0') {
		ridx = 0;
	} else {
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			ridx = route_get(&branch_rt, rname->s);
		} else {
			ridx = sr_kemi_cbname_lookup_name(rname);
		}
	}
	if(ridx<0) { ridx = 0; }

	t_on_branch(ridx);
	return 1;
}

/**
 *
 */
static int ki_t_on_reply(sip_msg_t *msg, str *rname)
{
	int ridx;
	sr_kemi_eng_t *keng;

	if(rname==NULL || rname->s==NULL || rname->len<=0 || rname->s[0]=='\0') {
		ridx = 0;
	} else {
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			ridx = route_get(&onreply_rt, rname->s);
		} else {
			ridx = sr_kemi_cbname_lookup_name(rname);
		}
	}
	if(ridx<0) { ridx = 0; }

	t_on_reply(ridx);
	return 1;
}

/**
 *
 */
static int ki_t_relay(sip_msg_t *msg)
{
	return _w_t_relay_to(msg, (struct proxy_l *)0, PROTO_NONE);
}

/**
 *
 */
static int ki_t_reply(sip_msg_t *msg, int code, str *reason)
{
	return w_t_reply_wrp(msg, (unsigned int)code, reason->s);
}

/**
 *
 */
static sr_kemi_t tm_kemi_exports[] = {
	{ str_init("tm"), str_init("t_relay"),
		SR_KEMIP_INT, ki_t_relay,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_on_branch"),
		SR_KEMIP_INT, ki_t_on_branch,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_on_failure"),
		SR_KEMIP_INT, ki_t_on_failure,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_on_branch_failure"),
		SR_KEMIP_INT, ki_t_on_branch_failure,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_on_reply"),
		SR_KEMIP_INT, ki_t_on_reply,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_reply"),
		SR_KEMIP_INT, ki_t_reply,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_check_trans"),
		SR_KEMIP_INT, t_check_trans,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_is_canceled"),
		SR_KEMIP_INT, t_is_canceled,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_newtran"),
		SR_KEMIP_INT, t_newtran,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_release"),
		SR_KEMIP_INT, t_release,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_replicate"),
		SR_KEMIP_INT, t_replicate_uri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_is_set"),
		SR_KEMIP_INT, t_is_set,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_lookup_request"),
		SR_KEMIP_INT, ki_t_lookup_request,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_lookup_cancel"),
		SR_KEMIP_INT, ki_t_lookup_cancel,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_lookup_cancel_flags"),
		SR_KEMIP_INT, ki_t_lookup_cancel_flags,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_retransmit_reply"),
		SR_KEMIP_INT, ki_t_retransmit_reply,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_fr_inv"),
		SR_KEMIP_INT, ki_t_set_fr_inv,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_fr"),
		SR_KEMIP_INT, ki_t_set_fr,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_reset_fr"),
		SR_KEMIP_INT, ki_t_reset_fr,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_max_lifetime"),
		SR_KEMIP_INT, ki_t_set_max_lifetime,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_reset_max_lifetime"),
		SR_KEMIP_INT, ki_t_reset_max_lifetime,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_retr"),
		SR_KEMIP_INT, ki_t_set_retr,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_reset_retr"),
		SR_KEMIP_INT, ki_t_reset_retr,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_uac_send"),
		SR_KEMIP_INT, ki_t_uac_send,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR }
	},
	{ str_init("tm"), str_init("t_load_contacts"),
		SR_KEMIP_INT, ki_t_load_contacts,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_next_contacts"),
		SR_KEMIP_INT, ki_t_next_contacts,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_next_contact_flow"),
		SR_KEMIP_INT, ki_t_next_contact_flow,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_drop_replies_all"),
		SR_KEMIP_INT, ki_t_drop_replies_all,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_drop_replies"),
		SR_KEMIP_INT, ki_t_drop_replies,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_use_uac_headers"),
		SR_KEMIP_INT, ki_t_use_uac_headers,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_save_lumps"),
		SR_KEMIP_INT, ki_t_save_lumps,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_is_expired"),
		SR_KEMIP_INT, t_is_expired,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_check_status"),
		SR_KEMIP_INT, ki_t_check_status,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_grep_status"),
		SR_KEMIP_INT, t_grep_status,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_is_retr_async_reply"),
		SR_KEMIP_INT, t_is_retr_async_reply,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_any_replied"),
		SR_KEMIP_INT, t_any_replied,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_any_timeout"),
		SR_KEMIP_INT, t_any_timeout,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_branch_replied"),
		SR_KEMIP_INT, t_branch_replied,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_branch_timeout"),
		SR_KEMIP_INT, t_branch_timeout,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_auto_inv_100"),
		SR_KEMIP_INT, t_set_auto_inv_100,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_disable_6xx"),
		SR_KEMIP_INT, t_set_disable_6xx,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_disable_failover"),
		SR_KEMIP_INT, t_set_disable_failover,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_no_e2e_cancel_reason"),
		SR_KEMIP_INT, t_set_no_e2e_cancel_reason,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tm"), str_init("t_set_disable_internal_reply"),
		SR_KEMIP_INT, t_set_disable_internal_reply,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},


	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};


int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(tm_kemi_exports);
	return 0;
}
