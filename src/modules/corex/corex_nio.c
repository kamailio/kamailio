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

#include "corex_nio.h"

/**
 * init nio function
 */
int nio_intercept_init(void)
{
	int route_no;
	pv_spec_t avp_spec;

	route_no=route_get(&event_rt, "network:msg");

	if (route_no==-1)
	{
		LM_ERR("failed to find event_route[network:msg]\n");
		return -1;
	}

	if (event_rt.rlist[route_no]==0)
	{
		LM_ERR("event_route[network:msg] is empty\n");
		return -1;
	}

	nio_route_no=route_no;
	
	if (nio_min_msg_len < 0)
	{
		LM_WARN("min_msg_len is less then zero, setting it to zero");
		nio_min_msg_len = 0;
	}

	if (nio_msg_avp_param.s && nio_msg_avp_param.len > 0) 
	{
		if (pv_parse_spec(&nio_msg_avp_param, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP)
		{
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					nio_msg_avp_param.len, nio_msg_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &nio_msg_avp_name,
					&nio_msg_avp_type)!=0)
		{
			LM_ERR("[%.*s]- invalid AVP definition\n",
					nio_msg_avp_param.len, nio_msg_avp_param.s);
			return -1;
		}
	} else {
		LM_WARN("no AVP defined to store modified message\n");
	}

    /* register network hooks */
    sr_event_register_cb(SREV_NET_DATA_IN, nio_msg_received);
    sr_event_register_cb(SREV_NET_DATA_OUT, nio_msg_sent);
#ifdef USE_TCP
    tcp_set_clone_rcvbuf(1);
#endif
    return 0;
}

/**
 *
 */
int nio_msg_received(void *data)
{
    sip_msg_t msg;
    str *obuf;
    char *nbuf = NULL;
    int_str avp_value;
    struct usr_avp *avp;
    struct run_act_ctx ra_ctx;

    obuf = (str*)data;

    if (obuf->len < nio_min_msg_len) {
        return -1;
    }

    memset(&msg, 0, sizeof(sip_msg_t));
    msg.buf = obuf->s;
    msg.len = obuf->len;

    nio_is_incoming = 1;
    init_run_actions_ctx(&ra_ctx);
    run_actions(&ra_ctx, event_rt.rlist[nio_route_no], &msg);

    if(nio_msg_avp_name.n!=0) {
        avp = NULL;
        avp=search_first_avp(nio_msg_avp_type, nio_msg_avp_name,
            &avp_value, 0);
        if(avp!=NULL && is_avp_str_val(avp)) {
            msg.buf = avp_value.s.s;
            msg.len = avp_value.s.len;
            nbuf = nio_msg_update(&msg, (unsigned int*)&obuf->len);
            if(obuf->len>=BUF_SIZE) {
                LM_ERR("new buffer overflow (%d)\n", obuf->len);
                pkg_free(nbuf);
                return -1;
            }
            memcpy(obuf->s, nbuf, obuf->len);
            obuf->s[obuf->len] = '\0';
        } else {
            LM_WARN("no value set for AVP %.*s, using unmodified message\n",
                nio_msg_avp_param.len, nio_msg_avp_param.s);
        }
    }

    if(nbuf!=NULL)
        pkg_free(nbuf);
    free_sip_msg(&msg);
    return 0;
}

/**
 *
 */
int nio_msg_sent(void *data)
{
    sip_msg_t msg;
    str *obuf;
    int_str avp_value;
    struct usr_avp *avp;
    struct run_act_ctx ra_ctx;

    obuf = (str*)data;

    if (obuf->len < nio_min_msg_len) {
        return -1;
    }

    memset(&msg, 0, sizeof(sip_msg_t));
    msg.buf = obuf->s;
    msg.len = obuf->len;

    nio_is_incoming = 0;
    init_run_actions_ctx(&ra_ctx);
    run_actions(&ra_ctx, event_rt.rlist[nio_route_no], &msg);

    if(nio_msg_avp_name.n!=0) {
        avp = NULL;
        avp=search_first_avp(nio_msg_avp_type, nio_msg_avp_name,
                &avp_value, 0);
        if(avp!=NULL && is_avp_str_val(avp)) {
            msg.buf = avp_value.s.s;
            msg.len = avp_value.s.len;
            obuf->s = nio_msg_update(&msg, (unsigned int*)&obuf->len);
        } else {
            LM_WARN("no value set for AVP %.*s, using unmodified message\n",
                nio_msg_avp_param.len, nio_msg_avp_param.s);
        }
    }

    free_sip_msg(&msg);
    return 0;
}

/**
 *
 */
int nio_check_incoming(void)
{
    return (nio_is_incoming) ? 1 : -1;
}

/**
 *
 */
char* nio_msg_update(sip_msg_t *msg, unsigned int *olen)
{
    struct dest_info dst;

    init_dest_info(&dst);
    dst.proto = PROTO_UDP;
    return build_req_buf_from_sip_req(msg,
            olen, &dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
}

