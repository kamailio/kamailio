/**
 * Copyright (C) 2009 SIP-Router.org
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*!
 * \file
 * \brief Kamailio core :: event handling
 * \ingroup core
 * Module: \ref core
 */

#include "dprint.h"
#include "mem/mem.h"
#include "route.h"
#include "events.h"

static sr_event_cb_t _sr_events_list;
static int _sr_events_inited = 0;

typedef struct _sr_core_ert {
	int init_parse_error;
} sr_core_ert_t;

static sr_core_ert_t _sr_core_ert_list;

/**
 *
 */
void sr_core_ert_init(void)
{
	memset(&_sr_core_ert_list, 0, sizeof(sr_core_ert_t));
	/* 0 - is not a valid index in event_route blocks list */
	_sr_core_ert_list.init_parse_error = route_get(&event_rt,
											"core:receive-parse-error");
	if(_sr_core_ert_list.init_parse_error<=0
				|| event_rt.rlist[_sr_core_ert_list.init_parse_error]==NULL) {
		_sr_core_ert_list.init_parse_error = -1;
	} else {
		LM_DBG("event_route[core:receive-parse-error] is defined\n");
	}
}

/**
 *
 */
void sr_core_ert_run(sip_msg_t *msg, int e)
{
	struct run_act_ctx ctx;
	int rtb;

	switch(e) {
		case SR_CORE_ERT_RECEIVE_PARSE_ERROR:
			if(likely(_sr_core_ert_list.init_parse_error<=0))
				return;
			rtb = get_route_type();
			set_route_type(REQUEST_ROUTE);
			init_run_actions_ctx(&ctx);
			run_top_route(event_rt.rlist[_sr_core_ert_list.init_parse_error],
					msg, &ctx);
			set_route_type(rtb);
		break;
	}
}

/**
 *
 */
void sr_event_cb_init(void)
{
	if(_sr_events_inited == 0)
	{
		memset(&_sr_events_list, 0, sizeof(sr_event_cb_t));
		_sr_events_inited = 1;
	}
}

/**
 *
 */
int sr_event_register_cb(int type, sr_event_cb_f f)
{
	int i;

	sr_event_cb_init();
	switch(type) {
		case SREV_NET_DATA_IN:
				for(i=0; i<SREV_CB_LIST_SIZE; i++) {
					if(_sr_events_list.net_data_in[i]==0) {
						_sr_events_list.net_data_in[i] = f;
						break;
					}
				}
				if(i==SREV_CB_LIST_SIZE) return -1;
			break;
		case SREV_NET_DATA_OUT:
				for(i=SREV_CB_LIST_SIZE-1; i>=0; i--) {
					if(_sr_events_list.net_data_out[i]==0) {
						_sr_events_list.net_data_out[i] = f;
						break;
					}
				}
				if(i<0) return -1;
			break;
		case SREV_CORE_STATS:
				if(_sr_events_list.core_stats==0)
					_sr_events_list.core_stats = f;
				else return -1;
			break;
		case SREV_CFG_RUN_ACTION:
				if(_sr_events_list.run_action==0)
					_sr_events_list.run_action = f;
				else return -1;
			break;
		case SREV_PKG_UPDATE_STATS:
				if(_sr_events_list.pkg_update_stats==0)
					_sr_events_list.pkg_update_stats = f;
				else return -1;
			break;
		case SREV_NET_DGRAM_IN:
				if(_sr_events_list.net_dgram_in==0)
					_sr_events_list.net_dgram_in = f;
				else return -1;
			break;
		case SREV_TCP_HTTP_100C:
				if(_sr_events_list.tcp_http_100c==0)
					_sr_events_list.tcp_http_100c = f;
				else return -1;
			break;
		case SREV_TCP_MSRP_FRAME:
				if(_sr_events_list.tcp_msrp_frame==0)
					_sr_events_list.tcp_msrp_frame = f;
				else return -1;
			break;
		case SREV_TCP_WS_FRAME_IN:
				if(_sr_events_list.tcp_ws_frame_in==0)
					_sr_events_list.tcp_ws_frame_in = f;
				else return -1;
			break;
		case SREV_TCP_WS_FRAME_OUT:
				if(_sr_events_list.tcp_ws_frame_out==0)
					_sr_events_list.tcp_ws_frame_out = f;
				else return -1;
			break;
		case SREV_STUN_IN:
				if(_sr_events_list.stun_in==0)
					_sr_events_list.stun_in = f;
				else return -1;
			break;
		case SREV_RCV_NOSIP:
				if(_sr_events_list.rcv_nosip==0)
					_sr_events_list.rcv_nosip = f;
				else return -1;
			break;
		default:
			return -1;
	}
	return 0;
}

/**
 *
 */
int sr_event_exec(int type, void *data)
{
	int ret;
	int i;
#ifdef EXTRA_DEBUG
	str *p;
#endif /* EXTRA_DEBUG */
	switch(type) {
		case SREV_NET_DATA_IN:
				if(unlikely(_sr_events_list.net_data_in[0]!=0))
				{
#ifdef EXTRA_DEBUG
					p = (str*)data;
					LM_DBG("PRE-IN ++++++++++++++++++++++++++++++++\n"
							"%.*s\n+++++\n", p->len, p->s);
#endif /* EXTRA_DEBUG */
					ret = 0;
					for(i=0; i<SREV_CB_LIST_SIZE
							&& _sr_events_list.net_data_in[i]; i++) {
						ret |= _sr_events_list.net_data_in[i](data);
					}
#ifdef EXTRA_DEBUG
					LM_DBG("POST-IN ++++++++++++++++++++++++++++++++\n"
							"%.*s\n+++++\n", p->len, p->s);
#endif /* EXTRA_DEBUG */
					return ret;
				} else return 1;
			break;
		case SREV_NET_DATA_OUT:
				if(unlikely(_sr_events_list.net_data_out[SREV_CB_LIST_SIZE-1]!=0))
				{
#ifdef EXTRA_DEBUG
					p = (str*)data;
					LM_DBG("PRE-OUT ++++++++++++++++++++\n"
							"%.*s\n+++++++++++++++++++\n", p->len, p->s);
#endif /* EXTRA_DEBUG */
					ret = 0;
					for(i=0; i<SREV_CB_LIST_SIZE; i++) {
						if(_sr_events_list.net_data_out[i]) {
							ret |= _sr_events_list.net_data_out[i](data);
						}
					}
#ifdef EXTRA_DEBUG
					LM_DBG("POST-OUT ++++++++++++++++++++\n"
							"%.*s\n+++++++++++++++++++\n", p->len, p->s);
#endif /* EXTRA_DEBUG */
					return ret;
				} else return 1;
			break;
		case SREV_CORE_STATS:
				if(unlikely(_sr_events_list.core_stats!=0))
				{
					ret = _sr_events_list.core_stats(data);
					return ret;
				} else return 1;
			break;
		case SREV_CFG_RUN_ACTION:
				if(unlikely(_sr_events_list.run_action!=0))
				{
					ret = _sr_events_list.run_action(data);
					return ret;
				} else return 1;
		case SREV_PKG_UPDATE_STATS:
				if(unlikely(_sr_events_list.pkg_update_stats!=0))
				{
					ret = _sr_events_list.pkg_update_stats(data);
					return ret;
				} else return 1;
		case SREV_NET_DGRAM_IN:
				if(unlikely(_sr_events_list.net_dgram_in!=0))
				{
					ret = _sr_events_list.net_dgram_in(data);
					return ret;
				} else return 1;
		case SREV_TCP_HTTP_100C:
				if(unlikely(_sr_events_list.tcp_http_100c!=0))
				{
					ret = _sr_events_list.tcp_http_100c(data);
					return ret;
				} else return 1;
		case SREV_TCP_MSRP_FRAME:
				if(unlikely(_sr_events_list.tcp_msrp_frame!=0))
				{
					ret = _sr_events_list.tcp_msrp_frame(data);
					return ret;
				} else return 1;
		case SREV_TCP_WS_FRAME_IN:
				if(unlikely(_sr_events_list.tcp_ws_frame_in!=0))
				{
					ret = _sr_events_list.tcp_ws_frame_in(data);
					return ret;
				} else return 1;
		case SREV_TCP_WS_FRAME_OUT:
				if(unlikely(_sr_events_list.tcp_ws_frame_out!=0))
				{
					ret = _sr_events_list.tcp_ws_frame_out(data);
					return ret;
				} else return 1;
		case SREV_STUN_IN:
				if(unlikely(_sr_events_list.stun_in!=0))
				{
					ret = _sr_events_list.stun_in(data);
					return ret;
				} else return 1;
		case SREV_RCV_NOSIP:
				if(unlikely(_sr_events_list.rcv_nosip!=0))
				{
					ret = _sr_events_list.rcv_nosip(data);
					return ret;
				} else return 1;
		default:
			return -1;
	}
}

/**
 *
 */
int sr_event_enabled(int type)
{
	switch(type) {
		case SREV_NET_DATA_IN:
				return (_sr_events_list.net_data_in[0]!=0)?1:0;
		case SREV_NET_DATA_OUT:
				return (_sr_events_list.net_data_out[SREV_CB_LIST_SIZE-1]!=0)?1:0;
		case SREV_CORE_STATS:
				return (_sr_events_list.core_stats!=0)?1:0;
		case SREV_CFG_RUN_ACTION:
				return (_sr_events_list.run_action!=0)?1:0;
		case SREV_PKG_UPDATE_STATS:
				return (_sr_events_list.pkg_update_stats!=0)?1:0;
		case SREV_NET_DGRAM_IN:
				return (_sr_events_list.net_dgram_in!=0)?1:0;
		case SREV_TCP_HTTP_100C:
				return (_sr_events_list.tcp_http_100c!=0)?1:0;
		case SREV_TCP_MSRP_FRAME:
				return (_sr_events_list.tcp_msrp_frame!=0)?1:0;
		case SREV_TCP_WS_FRAME_IN:
				return (_sr_events_list.tcp_ws_frame_in!=0)?1:0;
		case SREV_TCP_WS_FRAME_OUT:
				return (_sr_events_list.tcp_ws_frame_out!=0)?1:0;
		case SREV_STUN_IN:
				return (_sr_events_list.stun_in!=0)?1:0;
		case SREV_RCV_NOSIP:
				return (_sr_events_list.rcv_nosip!=0)?1:0;
	}
	return 0;
}

