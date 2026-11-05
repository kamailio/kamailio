/**
 * Copyright 2015 (C) Orange
 * <camille.oudot@orange.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include "../../core/dprint.h"
#include "../../core/tcp_conn.h"
#include "../../core/globals.h"
#include "../../core/timer.h"
#include "../../core/route.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"
#include "../../core/sr_module.h"

#include "tcpops.h"


extern str tcpops_event_callback;

static str tcpops_evrt_closed = str_init("tcp:closed");
static str tcpops_evrt_timeout = str_init("tcp:timeout");
static str tcpops_evrt_reset = str_init("tcp:reset");

/* globally enabled by default */
int tcp_closed_event = 1;

int tcp_closed_routes[_TCP_CLOSED_REASON_MAX] = {
		[0 ... sizeof(tcp_closed_routes) / sizeof(*tcp_closed_routes) - 1] =
				-1};

/**
 *
 */
void tcpops_init_evroutes(void)
{
	if(tcpops_event_callback.len > 0) {
		return;
	}

	/* get event routes */
	tcp_closed_routes[TCP_CLOSED_EOF] =
			route_get(&event_rt, tcpops_evrt_closed.s);
	tcp_closed_routes[TCP_CLOSED_TIMEOUT] =
			route_get(&event_rt, tcpops_evrt_timeout.s);
	tcp_closed_routes[TCP_CLOSED_RESET] =
			route_get(&event_rt, tcpops_evrt_reset.s);
}

static void tcpops_tcp_closed_run_route(tcp_closed_event_info_t *tev)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng = NULL;
	str *evname;

	LM_DBG("event reason id: %d\n", tev->reason);
	if(tcpops_event_callback.len > 0) {
		rt = -1;
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			LM_DBG("even callback set, but no kemi engine\n");
			return;
		}
	} else {
		rt = tcp_closed_routes[tev->reason];
		if(rt == -1) {
			LM_DBG("event reason id: %d - event route not set\n", tev->reason);
			return;
		}
	}

	if(faked_msg_init() < 0) {
		LM_ERR("faked_msg_init() failed\n");
		return;
	}
	fmsg = faked_msg_next();
	fmsg->rcv = tev->con->rcv;

	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	if(keng == NULL) {
		if(rt >= 0) {
			run_top_route(event_rt.rlist[rt], fmsg, 0);
		} else {
			LM_DBG("no event route block to execute\n");
		}
	} else {
		if(tev->reason == TCP_CLOSED_TIMEOUT) {
			evname = &tcpops_evrt_timeout;
		} else if(tev->reason == TCP_CLOSED_RESET) {
			evname = &tcpops_evrt_reset;
		} else {
			evname = &tcpops_evrt_closed;
		}
		if(sr_kemi_route(
				   keng, fmsg, EVENT_ROUTE, &tcpops_event_callback, evname)
				< 0) {
			LM_ERR("error running event route kemi callback [%.*s - %.*s]\n",
					tcpops_event_callback.len, tcpops_event_callback.s,
					evname->len, evname->s);
		}
	}
	set_route_type(backup_rt);
}

int tcpops_handle_tcp_closed(sr_event_param_t *evp)
{
	tcp_closed_event_info_t *tev = (tcp_closed_event_info_t *)evp->data;

	if(tev == NULL || tev->con == NULL) {
		LM_WARN("received bad TCP closed event\n");
		return -1;
	}
	LM_DBG("received TCP closed event\n");

	/* run event route if tcp_closed_event == 1 or if the
	 * F_CONN_CLOSE_EV flag is explicitly set */
	if(tcp_closed_event == 1 || (tev->con->flags & F_CONN_CLOSE_EV)) {
		tcpops_tcp_closed_run_route(tev);
	}

	return 0;
}
