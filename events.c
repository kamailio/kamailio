/**
 * $Id$
 *
 * Copyright (C) 2009 SIP-Router.org
 *
 * This file is part of Extensible SIP Router, a free SIP server.
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
 * \brief SIP-router core :: 
 * \ingroup core
 * Module: \ref core
 */

#include "dprint.h"
#include "mem/mem.h"
#include "events.h"

static sr_event_cb_t _sr_events_list;
static int _sr_events_inited = 0;

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
	sr_event_cb_init();
	switch(type) {
		case SREV_NET_DATA_IN:
				if(_sr_events_list.net_data_in==0)
					_sr_events_list.net_data_in = f;
				else return -1;
			break;
		case SREV_NET_DATA_OUT:
				if(_sr_events_list.net_data_out==0)
					_sr_events_list.net_data_out = f;
				else return -1;
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
		case SREV_PKG_SET_USED:
				if(_sr_events_list.pkg_set_used==0)
					_sr_events_list.pkg_set_used = f;
				else return -1;
			break;
		case SREV_PKG_SET_REAL_USED:
				if(_sr_events_list.pkg_set_real_used==0)
					_sr_events_list.pkg_set_real_used = f;
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
#ifdef EXTRA_DEBUG
	str *p;
#endif /* EXTRA_DEBUG */
	switch(type) {
		case SREV_NET_DATA_IN:
				if(unlikely(_sr_events_list.net_data_in!=0))
				{
#ifdef EXTRA_DEBUG
					p = (str*)data;
					LM_DBG("PRE-IN ++++++++++++++++++++++++++++++++\n"
							"%.*s\n+++++\n", p->len, p->s);
#endif /* EXTRA_DEBUG */
					ret = _sr_events_list.net_data_in(data);
#ifdef EXTRA_DEBUG
					LM_DBG("POST-IN ++++++++++++++++++++++++++++++++\n"
							"%.*s\n+++++\n", p->len, p->s);
#endif /* EXTRA_DEBUG */
					return ret;
				} else return 1;
			break;
		case SREV_NET_DATA_OUT:
				if(unlikely(_sr_events_list.net_data_out!=0))
				{
#ifdef EXTRA_DEBUG
					p = (str*)data;
					LM_DBG("PRE-OUT ++++++++++++++++++++\n"
							"%.*s\n+++++++++++++++++++\n", p->len, p->s);
#endif /* EXTRA_DEBUG */
					ret = _sr_events_list.net_data_out(data);
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
		case SREV_PKG_SET_USED:
				if(unlikely(_sr_events_list.pkg_set_used!=0))
				{
					ret = _sr_events_list.pkg_set_used(data);
					return ret;
				} else return 1;
		case SREV_PKG_SET_REAL_USED:
				if(unlikely(_sr_events_list.pkg_set_real_used!=0))
				{
					ret = _sr_events_list.pkg_set_real_used(data);
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
				return (_sr_events_list.net_data_in!=0)?1:0;
		case SREV_NET_DATA_OUT:
				return (_sr_events_list.net_data_out!=0)?1:0;
		case SREV_CORE_STATS:
				return (_sr_events_list.core_stats!=0)?1:0;
		case SREV_CFG_RUN_ACTION:
				return (_sr_events_list.run_action!=0)?1:0;
		case SREV_PKG_SET_USED:
				return (_sr_events_list.pkg_set_used!=0)?1:0;
		case SREV_PKG_SET_REAL_USED:
				return (_sr_events_list.pkg_set_real_used!=0)?1:0;
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
	}
	return 0;
}

