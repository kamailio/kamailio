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
#include "ip_addr.h"
#include "script_cb.h"
#include "nonsip_hooks.h"
#include "dset.h"
#include "fmsg.h"
#include "usr_avp.h"
#include "xavp.h"
#include "select_buf.h"
#include "locking.h"

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

int ksr_route_locks_size = 0;
static rec_lock_set_t* ksr_route_locks_set = NULL;

int ksr_route_locks_set_init(void)
{
	if(ksr_route_locks_set!=NULL || ksr_route_locks_size<=0)
		return 0;

	ksr_route_locks_set = rec_lock_set_alloc(ksr_route_locks_size);
	if(ksr_route_locks_set==NULL) {
		LM_ERR("failed to allocate route locks set\n");
		return -1;
	}
	if(rec_lock_set_init(ksr_route_locks_set)==NULL) {
		LM_ERR("failed to init route locks set\n");
		return -1;
	}
	return 0;
}

void ksr_route_locks_set_destroy(void)
{
	if(ksr_route_locks_set==NULL)
		return;

	rec_lock_set_destroy(ksr_route_locks_set);
	rec_lock_set_dealloc(ksr_route_locks_set);
	ksr_route_locks_set = NULL;
}

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

/**
 *
 */
static sr_net_info_t *ksr_evrt_rcvnetinfo = NULL;
int ksr_evrt_received_mode = 0;
str kemi_received_route_callback = STR_NULL;

/**
 *
 */
sr_net_info_t *ksr_evrt_rcvnetinfo_get(void)
{
	return ksr_evrt_rcvnetinfo;
}

/**
 *
 */
int ksr_evrt_received(char *buf, unsigned int len, receive_info_t *rcv_info)
{
	sr_kemi_eng_t *keng = NULL;
	sr_net_info_t netinfo;
	int ret = 0;
	int rt = -1;
	run_act_ctx_t ra_ctx;
	run_act_ctx_t *bctx = NULL;
	sip_msg_t *fmsg = NULL;
	str evname = str_init("core:msg-received");

	if(len==0 || rcv_info==NULL || buf==NULL) {
		LM_ERR("required parameters are not available\n");
		return -1;
	}

	if(kemi_received_route_callback.len>0) {
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			LM_DBG("kemi enabled with no core:msg-receive event route callback\n");
			return 0;
		}
	} else {
		rt = route_lookup(&event_rt, evname.s);
		if (rt < 0 || event_rt.rlist[rt] == NULL) {
			LM_DBG("event route core:msg-received not defined\n");
			return 0;
		}
	}
	memset(&netinfo, 0, sizeof(sr_net_info_t));
	netinfo.data.s = buf;
	netinfo.data.len = len;
	netinfo.rcv = rcv_info;

	ksr_evrt_rcvnetinfo = &netinfo;
	set_route_type(REQUEST_ROUTE);
	fmsg = faked_msg_get_next();
	init_run_actions_ctx(&ra_ctx);
	if(keng) {
		bctx = sr_kemi_act_ctx_get();
		sr_kemi_act_ctx_set(&ra_ctx);
		ret=sr_kemi_route(keng, fmsg, REQUEST_ROUTE,
				&kemi_received_route_callback, NULL);
		sr_kemi_act_ctx_set(bctx);
	} else {
		ret=run_actions(&ra_ctx, event_rt.rlist[rt], fmsg);
	}
	if(ra_ctx.run_flags&DROP_R_F) {
		LM_DBG("dropping received message\n");
		ret = -1;
	}
	ksr_evrt_rcvnetinfo = NULL;

	return ret;
}


static int ksr_evrt_pre_routing_idx = -1;
str kemi_pre_routing_callback = STR_NULL;


/**
 *
 */
int ksr_evrt_pre_routing(sip_msg_t *msg)
{
	int ret = 0;
	int rt = -1;
	run_act_ctx_t ra_ctx;
	run_act_ctx_t *bctx = NULL;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("core:pre-routing");
	recv_flags_t brflags;

	if(msg->rcv.rflags & RECV_F_INTERNAL) {
		DBG("skip internal routed message\n");
		return 0;
	}

	if(kemi_pre_routing_callback.len>0) {
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			LM_DBG("kemi enabled with no core:pre-routing event route callback\n");
			return 0;
		}
	} else {
		if(ksr_evrt_pre_routing_idx == -1) {
			rt = route_lookup(&event_rt, evname.s);
			if (rt < 0 || event_rt.rlist[rt] == NULL) {
				ksr_evrt_pre_routing_idx = -2;
			}
		} else {
			rt = ksr_evrt_pre_routing_idx;
		}
		if (rt < 0 || event_rt.rlist[rt] == NULL) {
			LM_DBG("event route core:pre-routing not defined\n");
			return 0;
		}
	}

	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ra_ctx);
	brflags = msg->rcv.rflags;
	msg->rcv.rflags |= RECV_F_PREROUTING;
	if(keng) {
		bctx = sr_kemi_act_ctx_get();
		sr_kemi_act_ctx_set(&ra_ctx);
		ret=sr_kemi_route(keng, msg, REQUEST_ROUTE,
				&kemi_pre_routing_callback, &evname);
		sr_kemi_act_ctx_set(bctx);
	} else {
		ret=run_actions(&ra_ctx, event_rt.rlist[rt], msg);
	}
	msg->rcv.rflags = brflags;
	if(ra_ctx.run_flags&DROP_R_F) {
		LM_DBG("drop was used\n");
		return 1;
	}
	LM_DBG("execution returned %d\n", ret);

	return 0;
}

/** Receive message
 *  WARNING: buf must be 0 terminated (buf[len]=0) or some things might
 * break (e.g.: modules/textops)
 */
int receive_msg(char *buf, unsigned int len, receive_info_t *rcv_info)
{
	struct sip_msg *msg = NULL;
	struct run_act_ctx ctx;
	struct run_act_ctx *bctx = NULL;
	int ret = -1;
	struct timeval tvb = {0}, tve = {0};
	unsigned int diff = 0;
	str inb = STR_NULL;
	sr_net_info_t netinfo = {0};
	sr_kemi_eng_t *keng = NULL;
	sr_event_param_t evp = {0};
	unsigned int cidlockidx = 0;
	unsigned int cidlockset = 0;
	int errsipmsg = 0;
	int exectime = 0;

	if(rcv_info->bind_address==NULL) {
		LM_ERR("critical - incoming message without local socket [%.*s ...]\n",
				(len>128)?128:len, buf);
		return -1;
	}

	if(ksr_evrt_received_mode!=0) {
		if(ksr_evrt_received(buf, len, rcv_info)<0) {
			LM_DBG("dropping the received message\n");
			goto error00;
		}
	}
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
	if(unlikely(msg == 0)) {
		PKG_MEM_ERROR;
		goto error00;
	}
	msg_no++;
	/* number of vias parsed -- good for diagnostic info in replies */
	via_cnt = 0;

	memset(msg, 0, sizeof(struct sip_msg)); /* init everything to 0 */
	/* fill in msg */
	msg->buf = buf;
	msg->len = len;
	/* zero termination (termination of orig message below not that
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
		errsipmsg = 1;
		evp.data = (void *)msg;
		if((ret = sr_event_exec(SREV_RCV_NOSIP, &evp)) < NONSIP_MSG_DROP) {
			LM_DBG("attempt of nonsip message processing failed\n");
		} else if(ret == NONSIP_MSG_DROP) {
			LM_DBG("nonsip message processing completed\n");
			goto end;
		}
	}
	if(errsipmsg==1) {
		LOG(cfg_get(core, core_cfg, sip_parser_log),
				"core parsing of SIP message failed (%s:%d/%d)\n",
				ip_addr2a(&msg->rcv.src_ip), (int)msg->rcv.src_port,
				(int)msg->rcv.proto);
		sr_core_ert_run(msg, SR_CORE_ERT_RECEIVE_PARSE_ERROR);
		goto error02;
	}

	if(unlikely(parse_headers(msg, HDR_FROM_F | HDR_TO_F | HDR_CALLID_F | HDR_CSEQ_F, 0)
			< 0)) {
		LOG(cfg_get(core, core_cfg, sip_parser_log),
				"parsing relevant headers failed\n");
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

	ret = ksr_evrt_pre_routing(msg);
	if(ret<0) {
		goto error02;
	}
	if(ret == 1) {
		/* finished */
		goto end;
	}

	if(unlikely(ksr_route_locks_set!=NULL && msg->callid && msg->callid->body.s
			&& msg->callid->body.len >0)) {
		cidlockidx = get_hash1_raw(msg->callid->body.s, msg->callid->body.len);
		cidlockidx = cidlockidx % ksr_route_locks_set->size;
		cidlockset = 1;
	}


	if(is_printable(cfg_get(core, core_cfg, latency_cfg_log))) {
		exectime = 1;
	}

	if(msg->first_line.type == SIP_REQUEST) {
		ruri_mark_new(); /* ruri is usable for forking (not consumed yet) */
		if(!IS_SIP(msg)) {
			LM_DBG("handling non-sip request message\n");
			if((ret = nonsip_msg_run_hooks(msg)) != NONSIP_MSG_ACCEPT) {
				if(unlikely(ret == NONSIP_MSG_ERROR)) {
					LM_DBG("failed handling non-sip request message\n");
					goto error03;
				}
				LM_DBG("finished handling non-sip request message\n");
				goto end; /* drop the message */
			}
		}
		/* sanity checks */
		if(unlikely((msg->via1 == 0) || (msg->via1->error != PARSE_OK))) {
			/* no via, send back error ? */
			LOG(cfg_get(core, core_cfg, sip_parser_log),
					"no via found in request\n");
			STATS_BAD_MSG();
			goto error02;
		}
		if(unlikely((msg->callid == 0) || (msg->cseq == 0) || (msg->from == 0)
					|| (msg->to == 0))) {
			/* no required headers -- send back error ? */
			LOG(cfg_get(core, core_cfg, sip_parser_log),
					"required headers not found in request\n");
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
		if(exectime) {
			gettimeofday(&tvb, NULL);
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
				LM_ERR("no request_route {...} and no other config routing"
						" engine registered\n");
				goto error_req;
			}
			if(unlikely(cidlockset)) {
				rec_lock_set_get(ksr_route_locks_set, cidlockidx);
				if(sr_kemi_route(keng, msg, REQUEST_ROUTE, NULL, NULL) < 0)
					LM_NOTICE("negative return code from engine function\n");
				rec_lock_set_release(ksr_route_locks_set, cidlockidx);
			} else {
				if(sr_kemi_route(keng, msg, REQUEST_ROUTE, NULL, NULL) < 0)
					LM_NOTICE("negative return code from engine function\n");
			}
		} else {
			if(unlikely(cidlockset)) {
				rec_lock_set_get(ksr_route_locks_set, cidlockidx);
				if(run_top_route(main_rt.rlist[DEFAULT_RT], msg, 0) < 0) {
					rec_lock_set_release(ksr_route_locks_set, cidlockidx);
					LM_WARN("error while trying script\n");
					goto error_req;
				}
				rec_lock_set_release(ksr_route_locks_set, cidlockidx);
			} else {
				if(run_top_route(main_rt.rlist[DEFAULT_RT], msg, 0) < 0) {
					LM_WARN("error while trying script\n");
					goto error_req;
				}
			}
		}

		if(exectime) {
			gettimeofday(&tve, NULL);
			diff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
			if (cfg_get(core, core_cfg, latency_limit_cfg) == 0
					|| cfg_get(core, core_cfg, latency_limit_cfg) <= diff) {
				LOG(cfg_get(core, core_cfg, latency_cfg_log),
						"request-route executed in: %d usec\n", diff);
			}
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
		if(unlikely((msg->callid == 0) || (msg->cseq == 0) || (msg->from == 0)
					|| (msg->to == 0))) {
			/* no required headers -- send back error ? */
			LOG(cfg_get(core, core_cfg, sip_parser_log),
					"required headers not found in reply\n");
			STATS_BAD_RPL();
			goto error02;
		}

		if(exectime) {
			gettimeofday(&tvb, NULL);
		}

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
		if(kemi_reply_route_callback.len>0) {
			keng = sr_kemi_eng_get();
		}
		if(onreply_rt.rlist[DEFAULT_RT] != NULL || keng != NULL) {
			set_route_type(CORE_ONREPLY_ROUTE);
			ret = 1;
			if(unlikely(keng != NULL)) {
				bctx = sr_kemi_act_ctx_get();
				init_run_actions_ctx(&ctx);
				sr_kemi_act_ctx_set(&ctx);
				if(unlikely(cidlockset)) {
					rec_lock_set_get(ksr_route_locks_set, cidlockidx);
					ret = sr_kemi_route(keng, msg, CORE_ONREPLY_ROUTE, NULL, NULL);
					rec_lock_set_release(ksr_route_locks_set, cidlockidx);
				} else {
					ret = sr_kemi_route(keng, msg, CORE_ONREPLY_ROUTE, NULL, NULL);
				}
				sr_kemi_act_ctx_set(bctx);
			} else {
				if(unlikely(cidlockset)) {
					rec_lock_set_get(ksr_route_locks_set, cidlockidx);
					ret = run_top_route(onreply_rt.rlist[DEFAULT_RT], msg, &ctx);
					rec_lock_set_release(ksr_route_locks_set, cidlockidx);
				} else  {
					ret = run_top_route(onreply_rt.rlist[DEFAULT_RT], msg, &ctx);
				}
			}
#ifndef NO_ONREPLY_ROUTE_ERROR
			if(unlikely(ret < 0)) {
				LM_WARN("error while trying onreply script\n");
				goto error_rpl;
			} else
#endif /* NO_ONREPLY_ROUTE_ERROR */
				if(unlikely(ret == 0 || (ctx.run_flags & DROP_R_F))) {
					STATS_RPL_FWD_DROP();
					LM_DBG("drop flag set - skip forwarding the reply\n");
					goto skip_send_reply; /* drop the message, no error */
				}
		}
		/* send the msg */
		forward_reply(msg);
	skip_send_reply:
		if(exectime) {
			gettimeofday(&tve, NULL);
			diff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
			if (cfg_get(core, core_cfg, latency_limit_cfg) == 0
					|| cfg_get(core, core_cfg, latency_limit_cfg) <= diff) {
				LOG(cfg_get(core, core_cfg, latency_cfg_log),
						"reply-route executed in: %d usec\n", diff);
			}
		}

		/* execute post reply-script callbacks */
		exec_post_script_cb(msg, ONREPLY_CB_TYPE);
	}

end:
	ksr_msg_env_reset();
	LM_DBG("cleaning up\n");
	free_sip_msg(msg);
	pkg_free(msg);
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
	/* reset log prefix */
	log_prefix_set(NULL);
	return -1;
}

/**
 * clean up msg environment, such as avp, xavp and xavu lists
 */
void ksr_msg_env_reset(void)
{
	reset_avps();
	xavp_reset_list();
	xavu_reset_list();
	xavi_reset_list();
}
