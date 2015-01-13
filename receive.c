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

#include "tcp_server.h" /* for tcpconn_add_alias */
#include "tcp_options.h" /* for access to tcp_accept_aliases*/
#include "cfg/cfg.h"
#include "core_stats.h"

#ifdef DEBUG_DMALLOC
#include <mem/dmalloc.h>
#endif

unsigned int msg_no=0;
/* address preset vars */
str default_global_address={0,0};
str default_global_port={0,0};
str default_via_address={0,0};
str default_via_port={0,0};

/**
 * increment msg_no and return the new value
 */
unsigned int inc_msg_no(void)
{
	return ++msg_no;
}


/** Receive message
 *  WARNING: buf must be 0 terminated (buf[len]=0) or some things might 
 * break (e.g.: modules/textops)
 */
int receive_msg(char* buf, unsigned int len, struct receive_info* rcv_info) 
{
	struct sip_msg* msg;
	struct run_act_ctx ctx;
	int ret;
#ifdef STATS
	int skipped = 1;
	struct timeval tvb, tve;	
	struct timezone tz;
	unsigned int diff;
#endif
	str inb;

	inb.s = buf;
	inb.len = len;
	sr_event_exec(SREV_NET_DATA_IN, (void*)&inb);
	len = inb.len;

	msg=pkg_malloc(sizeof(struct sip_msg));
	if (msg==0) {
		LM_ERR("no mem for sip_msg\n");
		goto error00;
	}
	msg_no++;
	/* number of vias parsed -- good for diagnostic info in replies */
	via_cnt=0;

	memset(msg,0, sizeof(struct sip_msg)); /* init everything to 0 */
	/* fill in msg */
	msg->buf=buf;
	msg->len=len;
	/* zero termination (termination of orig message bellow not that
	   useful as most of the work is done with scratch-pad; -jiri  */
	/* buf[len]=0; */ /* WARNING: zero term removed! */
	msg->rcv=*rcv_info;
	msg->id=msg_no;
	msg->pid=my_pid();
	msg->set_global_address=default_global_address;
	msg->set_global_port=default_global_port;
	
	if(likely(sr_msg_time==1)) msg_set_time(msg);

	if (parse_msg(buf,len, msg)!=0){
		if(sr_event_exec(SREV_RCV_NOSIP, (void*)msg)!=0) {
			LOG(cfg_get(core, core_cfg, corelog),
				"core parsing of SIP message failed (%s:%d/%d)\n",
				ip_addr2a(&msg->rcv.src_ip), (int)msg->rcv.src_port,
				(int)msg->rcv.proto);
			sr_core_ert_run(msg, SR_CORE_ERT_RECEIVE_PARSE_ERROR);
		}
		goto error02;
	}
	LM_DBG("After parse_msg...\n");

	/* set log prefix */
	log_prefix_set(msg);

	/* ... clear branches from previous message */
	clear_branches();

	if (msg->first_line.type==SIP_REQUEST){
		ruri_mark_new(); /* ruri is usable for forking (not consumed yet) */
		if (!IS_SIP(msg)){
			if ((ret=nonsip_msg_run_hooks(msg))!=NONSIP_MSG_ACCEPT){
				if (unlikely(ret==NONSIP_MSG_ERROR))
					goto error03;
				goto end; /* drop the message */
			}
		}
		/* sanity checks */
		if ((msg->via1==0) || (msg->via1->error!=PARSE_OK)){
			/* no via, send back error ? */
			LM_ERR("no via found in request\n");
			STATS_BAD_MSG();
			goto error02;
		}
		/* check if necessary to add receive?->moved to forward_req */
		/* check for the alias stuff */
#ifdef USE_TCP
		if (msg->via1->alias && cfg_get(tcp, tcp_cfg, accept_aliases) && 
				(((rcv_info->proto==PROTO_TCP) && !tcp_disable)
#ifdef USE_TLS
					|| ((rcv_info->proto==PROTO_TLS) && !tls_disable)
#endif
				)
			){
			if (tcpconn_add_alias(rcv_info->proto_reserved1, msg->via1->port,
									rcv_info->proto)!=0){
				LM_ERR("tcp alias failed\n");
				/* continue */
			}
		}
#endif

	/*	skip: */
		LM_DBG("preparing to run routing scripts...\n");
#ifdef  STATS
		gettimeofday( & tvb, &tz );
#endif
		/* execute pre-script callbacks, if any; -jiri */
		/* if some of the callbacks said not to continue with
		   script processing, don't do so
		   if we are here basic sanity checks are already done
		   (like presence of at least one via), so you can count
		   on via1 being parsed in a pre-script callback --andrei
		*/
		if (exec_pre_script_cb(msg, REQUEST_CB_TYPE)==0 )
		{
			STATS_REQ_FWD_DROP();
			goto end; /* drop the request */
		}

		set_route_type(REQUEST_ROUTE);
		/* exec the routing script */
		if (run_top_route(main_rt.rlist[DEFAULT_RT], msg, 0)<0){
			LM_WARN("error while trying script\n");
			goto error_req;
		}

#ifdef STATS
		gettimeofday( & tve, &tz );
		diff = (tve.tv_sec-tvb.tv_sec)*1000000+(tve.tv_usec-tvb.tv_usec);
		stats->processed_requests++;
		stats->acc_req_time += diff;
		LM_DBG("successfully ran routing scripts...(%d usec)\n", diff);
		STATS_RX_REQUEST( msg->first_line.u.request.method_value );
#endif

		/* execute post request-script callbacks */
		exec_post_script_cb(msg, REQUEST_CB_TYPE);
	}else if (msg->first_line.type==SIP_REPLY){
		/* sanity checks */
		if ((msg->via1==0) || (msg->via1->error!=PARSE_OK)){
			/* no via, send back error ? */
			LM_ERR("no via found in reply\n");
			STATS_BAD_RPL();
			goto error02;
		}

#ifdef STATS
		gettimeofday( & tvb, &tz );
		STATS_RX_RESPONSE ( msg->first_line.u.reply.statuscode / 100 );
#endif
		
		/* execute pre-script callbacks, if any; -jiri */
		/* if some of the callbacks said not to continue with
		   script processing, don't do so
		   if we are here basic sanity checks are already done
		   (like presence of at least one via), so you can count
		   on via1 being parsed in a pre-script callback --andrei
		*/
		if (exec_pre_script_cb(msg, ONREPLY_CB_TYPE)==0 )
		{
			STATS_RPL_FWD_DROP();
			goto end; /* drop the reply */
		}

		/* exec the onreply routing script */
		if (onreply_rt.rlist[DEFAULT_RT]){
			set_route_type(CORE_ONREPLY_ROUTE);
			ret=run_top_route(onreply_rt.rlist[DEFAULT_RT], msg, &ctx);
#ifndef NO_ONREPLY_ROUTE_ERROR
			if (unlikely(ret<0)){
				LM_WARN("error while trying onreply script\n");
				goto error_rpl;
			}else
#endif /* NO_ONREPLY_ROUTE_ERROR */
			if (unlikely(ret==0 || (ctx.run_flags&DROP_R_F))){
				STATS_RPL_FWD_DROP();
				goto skip_send_reply; /* drop the message, no error */
			}
		}
		/* send the msg */
		forward_reply(msg);
	skip_send_reply:
#ifdef STATS
		gettimeofday( & tve, &tz );
		diff = (tve.tv_sec-tvb.tv_sec)*1000000+(tve.tv_usec-tvb.tv_usec);
		stats->processed_responses++;
		stats->acc_res_time+=diff;
		LM_DBG("successfully ran reply processing...(%d usec)\n", diff);
#endif

		/* execute post reply-script callbacks */
		exec_post_script_cb(msg, ONREPLY_CB_TYPE);
	}

end:
#ifdef STATS
	skipped = 0;
#endif
	/* free possible loaded avps -bogdan */
	reset_avps();
#ifdef WITH_XAVP
	xavp_reset_list();
#endif
	LM_DBG("cleaning up\n");
	free_sip_msg(msg);
	pkg_free(msg);
#ifdef STATS
	if (skipped) STATS_RX_DROPS;
#endif
	/* reset log prefix */
	log_prefix_set(NULL);
	return 0;

#ifndef NO_ONREPLY_ROUTE_ERROR
error_rpl:
	/* execute post reply-script callbacks */
	exec_post_script_cb(msg, ONREPLY_CB_TYPE);
	reset_avps();
#ifdef WITH_XAVP
	xavp_reset_list();
#endif
	goto error02;
#endif /* NO_ONREPLY_ROUTE_ERROR */
error_req:
	LM_DBG("error:...\n");
	/* execute post request-script callbacks */
	exec_post_script_cb(msg, REQUEST_CB_TYPE);
error03:
	/* free possible loaded avps -bogdan */
	reset_avps();
#ifdef WITH_XAVP
	xavp_reset_list();
#endif
error02:
	free_sip_msg(msg);
	pkg_free(msg);
error00:
	STATS_RX_DROPS;
	/* reset log prefix */
	log_prefix_set(NULL);
	return -1;
}

