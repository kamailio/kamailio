/*
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
 *
 */

/*!
 * \file
 * \brief Kamailio core ::
 * \ingroup core
 * Module: \ref core
 */


#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "receive.h"
#include "globals.h"
#include "dprint.h"
#include "route.h"
#include "parser/msg_parser.h"
#include "forward.h"
#include "action.h"
#include "mem/mem.h"
#include "stats.h"
#include "ip_addr.h"
#include "script_cb.h"
#include "nonsip_hooks.h"
#include "dset.h"
#include "usr_avp.h"
#ifdef WITH_XAVP
#include "xavp.h"
#endif
#include "select_buf.h"

#include "tcp_server.h"  /* for tcpconn_add_alias */
#include "tcp_options.h" /* for access to tcp_accept_aliases*/
#include "cfg/cfg.h"
#include "core_stats.h"
#include "kemi.h"

#ifdef DEBUG_DMALLOC
#include <mem/dmalloc.h>
#endif

int _sr_ip_free_bind = 0;

unsigned int msg_no = 0;
/* address preset vars */
str default_global_address = {0, 0};
str default_global_port = {0, 0};
str default_via_address = {0, 0};
str default_via_port = {0, 0};

/**
 * increment msg_no and return the new value
 */
unsigned int inc_msg_no(void)
{
	return ++msg_no;
}

/**
 *
 */
int sip_check_fline(char *buf, unsigned int len)
{
	char *p;
	int m;

	m = 0;
	for(p = buf; p < buf + len; p++) {
		/* first check if is a reply - starts with SIP/2.0 */
		if(m == 0) {
			if(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
				continue;
			if(buf + len - p < 10)
				return -1;
			if(strncmp(p, "SIP/2.0 ", 8) == 0) {
				LM_DBG("first line indicates a SIP reply\n");
				return 0;
			}
			m = 1;
		} else {
			/* check if a request - before end of first line is SIP/2.0 */
			if(*p != '\r' && *p != '\n')
				continue;
			if(p - 10 >= buf) {
				if(strncmp(p - 8, " SIP/2.0", 8) == 0) {
					LM_DBG("first line indicates a SIP request\n");
					return 0;
				}
			}
			return -1;
		}
	}
	return -1;
}

/** Receive message
 *  WARNING: buf must be 0 terminated (buf[len]=0) or some things might
 * break (e.g.: modules/textops)
 */
int receive_msg(char *buf, unsigned int len, struct receive_info *rcv_info)
{
	struct sip_msg *msg;
	struct run_act_ctx ctx;
	struct run_act_ctx *bctx;
	int ret;
#ifdef STATS
	int skipped = 1;
	int stats_on = 1;
#else
	int stats_on = 0;
#endif
	struct timeval tvb, tve;
	struct timezone tz;
	unsigned int diff = 0;
	str inb;
	sr_net_info_t netinfo;
	sr_kemi_eng_t *keng = NULL;
	sr_event_param_t evp = {0};

	if(sr_event_enabled(SREV_NET_DATA_RECV)) {
		if(sip_check_fline(buf, len) == 0) {
			memset(&netinfo, 0, sizeof(sr_net_info_t));
			netinfo.data.s = buf;
			netinfo.data.len = len;
			netinfo.rcv = rcv_info;
			evp.data = (void *)&netinfo;
			sr_event_exec(SREV_NET_DATA_RECV, &evp);
		}
	}

	inb.s = buf;
	inb.len = len;
	evp.data = (void *)&inb;
	evp.rcv = rcv_info;
	sr_event_exec(SREV_NET_DATA_IN, &evp);
	len = inb.len;

	msg = pkg_malloc(sizeof(struct sip_msg));
	if(msg == 0) {
		LM_ERR("no mem for sip_msg\n");
		goto error00;
	}
	msg_no++;
	/* number of vias parsed -- good for diagnostic info in replies */
	via_cnt = 0;

	memset(msg, 0, sizeof(struct sip_msg)); /* init everything to 0 */
	/* fill in msg */
	msg->buf = buf;
	msg->len = len;
	/* zero termination (termination of orig message bellow not that
	 * useful as most of the work is done with scratch-pad; -jiri  */
	/* buf[len]=0; */ /* WARNING: zero term removed! */
	msg->rcv = *rcv_info;
	msg->id = msg_no;
	msg->pid = my_pid();
	msg->set_global_address = default_global_address;
	msg->set_global_port = default_global_port;

	if(likely(sr_msg_time == 1))
		msg_set_time(msg);

	if(parse_msg(buf, len, msg) != 0) {
		evp.data = (void *)msg;
		if((ret = sr_event_exec(SREV_RCV_NOSIP, &evp)) < NONSIP_MSG_DROP) {
			LOG(cfg_get(core, core_cfg, corelog),
					"core parsing of SIP message failed (%s:%d/%d)\n",
					ip_addr2a(&msg->rcv.src_ip), (int)msg->rcv.src_port,
					(int)msg->rcv.proto);
			sr_core_ert_run(msg, SR_CORE_ERT_RECEIVE_PARSE_ERROR);
		} else if(ret == NONSIP_MSG_DROP)
			goto error02;
	}

	if(parse_headers(msg, HDR_FROM_F | HDR_TO_F | HDR_CALLID_F | HDR_CSEQ_F, 0)
			< 0) {
		LM_WARN("parsing relevant headers failed\n");
	}
	LM_DBG("--- received sip message - %s - call-id: [%.*s] - cseq: [%.*s]\n",
			(msg->first_line.type == SIP_REQUEST) ? "request" : "reply",
			(msg->callid && msg->callid->body.s) ? msg->callid->body.len : 0,
			(msg->callid && msg->callid->body.s) ? msg->callid->body.s : "",
			(msg->cseq && msg->cseq->body.s) ? msg->cseq->body.len : 0,
			(msg->cseq && msg->cseq->body.s) ? msg->cseq->body.s : "");

	/* set log prefix */
	log_prefix_set(msg);

	/* ... clear branches from previous message */
	clear_branches();

	if(msg->first_line.type == SIP_REQUEST) {
		ruri_mark_new(); /* ruri is usable for forking (not consumed yet) */
		if(!IS_SIP(msg)) {
			if((ret = nonsip_msg_run_hooks(msg)) != NONSIP_MSG_ACCEPT) {
				if(unlikely(ret == NONSIP_MSG_ERROR))
					goto error03;
				goto end; /* drop the message */
			}
		}
		/* sanity checks */
		if((msg->via1 == 0) || (msg->via1->error != PARSE_OK)) {
			/* no via, send back error ? */
			LM_ERR("no via found in request\n");
			STATS_BAD_MSG();
			goto error02;
		}
/* check if necessary to add receive?->moved to forward_req */
/* check for the alias stuff */
#ifdef USE_TCP
		if(msg->via1->alias && cfg_get(tcp, tcp_cfg, accept_aliases)
				&& (((rcv_info->proto == PROTO_TCP) && !tcp_disable)
#ifdef USE_TLS
						   || ((rcv_info->proto == PROTO_TLS) && !tls_disable)
#endif
								   )) {
			if(tcpconn_add_alias(rcv_info->proto_reserved1, msg->via1->port,
					   rcv_info->proto)
					!= 0) {
				LM_ERR("tcp alias failed\n");
				/* continue */
			}
		}
#endif

		/*	skip: */
		LM_DBG("preparing to run routing scripts...\n");
		if(is_printable(cfg_get(core, core_cfg, latency_cfg_log))
				|| stats_on == 1) {
			gettimeofday(&tvb, &tz);
		}
		/* execute pre-script callbacks, if any; -jiri */
		/* if some of the callbacks said not to continue with
		 * script processing, don't do so
		 * if we are here basic sanity checks are already done
		 * (like presence of at least one via), so you can count
		 * on via1 being parsed in a pre-script callback --andrei
		*/
		if(exec_pre_script_cb(msg, REQUEST_CB_TYPE) == 0) {
			STATS_REQ_FWD_DROP();
			goto end; /* drop the request */
		}

		set_route_type(REQUEST_ROUTE);
		/* exec the routing script */
		if(unlikely(main_rt.rlist[DEFAULT_RT] == NULL)) {
			keng = sr_kemi_eng_get();
			if(keng == NULL) {
				LM_ERR("no config routing engine registered\n");
				goto error_req;
			}
			if(keng->froute(msg, REQUEST_ROUTE, NULL, NULL) < 0) {
				LM_NOTICE("negative return code from engine function\n");
			}
		} else {
			if(run_top_route(main_rt.rlist[DEFAULT_RT], msg, 0) < 0) {
				LM_WARN("error while trying script\n");
				goto error_req;
			}
		}

		if(is_printable(cfg_get(core, core_cfg, latency_cfg_log))
				|| stats_on == 1) {
			gettimeofday(&tve, &tz);
			diff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
			LOG(cfg_get(core, core_cfg, latency_cfg_log),
					"request-route executed in: %d usec\n", diff);
#ifdef STATS
			stats->processed_requests++;
			stats->acc_req_time += diff;
			STATS_RX_REQUEST(msg->first_line.u.request.method_value);
#endif
		}

		/* execute post request-script callbacks */
		exec_post_script_cb(msg, REQUEST_CB_TYPE);
	} else if(msg->first_line.type == SIP_REPLY) {
		/* sanity checks */
		if((msg->via1 == 0) || (msg->via1->error != PARSE_OK)) {
			/* no via, send back error ? */
			LM_ERR("no via found in reply\n");
			STATS_BAD_RPL();
			goto error02;
		}

		if(is_printable(cfg_get(core, core_cfg, latency_cfg_log))
				|| stats_on == 1) {
			gettimeofday(&tvb, &tz);
		}
#ifdef STATS
		STATS_RX_RESPONSE(msg->first_line.u.reply.statuscode / 100);
#endif

		/* execute pre-script callbacks, if any; -jiri */
		/* if some of the callbacks said not to continue with
		 * script processing, don't do so
		 * if we are here basic sanity checks are already done
		 * (like presence of at least one via), so you can count
		 * on via1 being parsed in a pre-script callback --andrei
		*/
		if(exec_pre_script_cb(msg, ONREPLY_CB_TYPE) == 0) {
			STATS_RPL_FWD_DROP();
			goto end; /* drop the reply */
		}

		/* exec the onreply routing script */
		keng = sr_kemi_eng_get();
		if(onreply_rt.rlist[DEFAULT_RT] != NULL || keng != NULL) {
			set_route_type(CORE_ONREPLY_ROUTE);
			ret = 1;
			if(unlikely(keng != NULL)) {
				bctx = sr_kemi_act_ctx_get();
				init_run_actions_ctx(&ctx);
				sr_kemi_act_ctx_set(&ctx);
				ret = keng->froute(msg, CORE_ONREPLY_ROUTE, NULL, NULL);
				sr_kemi_act_ctx_set(bctx);
			} else {
				ret = run_top_route(onreply_rt.rlist[DEFAULT_RT], msg, &ctx);
			}
#ifndef NO_ONREPLY_ROUTE_ERROR
			if(unlikely(ret < 0)) {
				LM_WARN("error while trying onreply script\n");
				goto error_rpl;
			} else
#endif /* NO_ONREPLY_ROUTE_ERROR */
					if(unlikely(ret == 0 || (ctx.run_flags & DROP_R_F))) {
				STATS_RPL_FWD_DROP();
				goto skip_send_reply; /* drop the message, no error */
			}
		}
		/* send the msg */
		forward_reply(msg);
	skip_send_reply:
		if(is_printable(cfg_get(core, core_cfg, latency_cfg_log))
				|| stats_on == 1) {
			gettimeofday(&tve, &tz);
			diff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
			LOG(cfg_get(core, core_cfg, latency_cfg_log),
					"reply-route executed in: %d usec\n", diff);
#ifdef STATS
			stats->processed_responses++;
			stats->acc_res_time += diff;
#endif
		}

		/* execute post reply-script callbacks */
		exec_post_script_cb(msg, ONREPLY_CB_TYPE);
	}

end:
#ifdef STATS
	skipped = 0;
#endif
	ksr_msg_env_reset();
	LM_DBG("cleaning up\n");
	free_sip_msg(msg);
	pkg_free(msg);
#ifdef STATS
	if(skipped)
		STATS_RX_DROPS;
#endif
	/* reset log prefix */
	log_prefix_set(NULL);
	return 0;

#ifndef NO_ONREPLY_ROUTE_ERROR
error_rpl:
	/* execute post reply-script callbacks */
	exec_post_script_cb(msg, ONREPLY_CB_TYPE);
	goto error02;
#endif /* NO_ONREPLY_ROUTE_ERROR */
error_req:
	LM_DBG("error:...\n");
	/* execute post request-script callbacks */
	exec_post_script_cb(msg, REQUEST_CB_TYPE);
error03:
error02:
	free_sip_msg(msg);
	pkg_free(msg);
error00:
	ksr_msg_env_reset();
	STATS_RX_DROPS;
	/* reset log prefix */
	log_prefix_set(NULL);
	return -1;
}

/**
 * clean up msg environment, such as avp and xavp lists
 */
void ksr_msg_env_reset(void)
{
	reset_avps();
#ifdef WITH_XAVP
	xavp_reset_list();
#endif
}
