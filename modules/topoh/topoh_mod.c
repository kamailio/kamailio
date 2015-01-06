/*
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
 * \brief Kamailio topoh :: Module interface
 * \ingroup topoh
 * Module: \ref topoh
 */

/*! \defgroup topoh Kamailio :: Topology hiding
 *
 * This module hides the SIP routing headers that show topology details.
 * It it is not affected by the server being transaction stateless or
 * stateful. The script interpreter gets the SIP messages decoded, so all
 * existing functionality is preserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "../../events.h"
#include "../../dprint.h"
#include "../../tcp_options.h"
#include "../../ut.h"
#include "../../forward.h"
#include "../../config.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"

#include "../../modules/sanity/api.h"

#include "th_mask.h"
#include "th_msg.h"

MODULE_VERSION


/** module parameters */
str _th_key = str_init("aL9.n8~Hm]Z");
str th_cookie_name = str_init("TH"); /* lost parameter? */
str th_cookie_value = {0, 0};        /* lost parameter? */
str th_ip = str_init("127.0.0.8");
str th_uparam_name = str_init("line");
str th_uparam_prefix = str_init("sr-");
str th_vparam_name = str_init("branch");
str th_vparam_prefix = str_init("z9hG4bKsr-");

str th_callid_prefix = str_init("!!:");
str th_via_prefix = {0, 0};
str th_uri_prefix = {0, 0};

int th_param_mask_callid = 0;

int th_sanity_checks = 0;
sanity_api_t scb;
int th_mask_addr_myself = 0;

int th_msg_received(void *data);
int th_msg_sent(void *data);

/** module functions */
static int mod_init(void);

static param_export_t params[]={
	{"mask_key",		PARAM_STR, &_th_key},
	{"mask_ip",			PARAM_STR, &th_ip},
	{"mask_callid",		PARAM_INT, &th_param_mask_callid},
	{"uparam_name",		PARAM_STR, &th_uparam_name},
	{"uparam_prefix",	PARAM_STR, &th_uparam_prefix},
	{"vparam_name",		PARAM_STR, &th_vparam_name},
	{"vparam_prefix",	PARAM_STR, &th_vparam_prefix},
	{"callid_prefix",	PARAM_STR, &th_callid_prefix},
	{"sanity_checks",	PARAM_INT, &th_sanity_checks},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"topoh",
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	0,
	0           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	sip_uri_t puri;
	char buri[MAX_URI_SIZE];

	if(th_sanity_checks!=0)
	{
		if(sanity_load_api(&scb)<0)
		{
			LM_ERR("cannot bind to sanity module\n");
			goto error;
		}
	}
	if(th_ip.len<=0)
	{
		LM_ERR("mask IP parameter is invalid\n");
		goto error;
	}

	if(th_ip.len + 32 >= MAX_URI_SIZE) {
		LM_ERR("mask address is too long\n");
		goto error;
	}
	memcpy(buri, "sip:", 4);
	memcpy(buri+4, th_ip.s, th_ip.len);
	buri[th_ip.len+8] = '\0';

	if(parse_uri(buri, th_ip.len+4, &puri)<0) {
		LM_ERR("mask uri is invalid\n");
		goto error;
	}
	if(check_self(&puri.host, puri.port_no, 0)==1)
	{
		th_mask_addr_myself = 1;
		LM_INFO("mask address matches myself [%.*s]\n",
				th_ip.len, th_ip.s);
	}

	/* 'SIP/2.0/UDP ' + ip + ';' + param + '=' + prefix (+ '\0') */
	th_via_prefix.len = 12 + th_ip.len + 1 + th_vparam_name.len + 1
		+ th_vparam_prefix.len;
	th_via_prefix.s = (char*)pkg_malloc(th_via_prefix.len+1);
	if(th_via_prefix.s==NULL)
	{
		LM_ERR("via prefix parameter is invalid\n");
		goto error;
	}
	/* 'sip:' + ip + ';' + param + '=' + prefix (+ '\0') */
	th_uri_prefix.len = 4 + th_ip.len + 1 + th_uparam_name.len + 1
		+ th_uparam_prefix.len;
	th_uri_prefix.s = (char*)pkg_malloc(th_uri_prefix.len+1);
	if(th_uri_prefix.s==NULL)
	{
		LM_ERR("uri prefix parameter is invalid\n");
		goto error;
	}
	/* build via prefix */
	memcpy(th_via_prefix.s, "SIP/2.0/UDP ", 12);
	memcpy(th_via_prefix.s+12, th_ip.s, th_ip.len);
	th_via_prefix.s[12+th_ip.len] = ';';
	memcpy(th_via_prefix.s+12+th_ip.len+1, th_vparam_name.s,
			th_vparam_name.len);
	th_via_prefix.s[12+th_ip.len+1+th_vparam_name.len] = '=';
	memcpy(th_via_prefix.s+12+th_ip.len+1+th_vparam_name.len+1,
			th_vparam_prefix.s, th_vparam_prefix.len);
	th_via_prefix.s[th_via_prefix.len] = '\0';
	LM_DBG("VIA prefix: [%s]\n", th_via_prefix.s);
	/* build uri prefix */
	memcpy(th_uri_prefix.s, "sip:", 4);
	memcpy(th_uri_prefix.s+4, th_ip.s, th_ip.len);
	th_uri_prefix.s[4+th_ip.len] = ';';
	memcpy(th_uri_prefix.s+4+th_ip.len+1, th_uparam_name.s, th_uparam_name.len);
	th_uri_prefix.s[4+th_ip.len+1+th_uparam_name.len] = '=';
	memcpy(th_uri_prefix.s+4+th_ip.len+1+th_uparam_name.len+1,
			th_uparam_prefix.s, th_uparam_prefix.len);
	th_uri_prefix.s[th_uri_prefix.len] = '\0';
	LM_DBG("URI prefix: [%s]\n", th_uri_prefix.s);

	th_mask_init();
	sr_event_register_cb(SREV_NET_DATA_IN, th_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, th_msg_sent);
#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
#endif
	return 0;
error:
	return -1;
}

/**
 *
 */
int th_prepare_msg(sip_msg_t *msg)
{
	if (parse_msg(msg->buf, msg->len, msg)!=0)
	{
		LM_DBG("outbuf buffer parsing failed!");
		return 1;
	}

	if(msg->first_line.type==SIP_REQUEST)
	{
		if(!IS_SIP(msg))
		{
			LM_DBG("non sip request message\n");
			return 1;
		}
	} else if(msg->first_line.type!=SIP_REPLY) {
		LM_DBG("non sip message\n");
		return 1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0)==-1)
	{
		LM_DBG("parsing headers failed");
		return 2;
	}

	/* force 2nd via parsing here - it helps checking it later */
	if (parse_headers(msg, HDR_VIA2_F, 0)==-1
		|| (msg->via2==0) || (msg->via2->error!=PARSE_OK))
	{
		LM_DBG("no second via in this message \n");
	}

	if(parse_from_header(msg)<0)
	{
		LM_ERR("cannot parse FROM header\n");
		return 3;
	}

	if(parse_to_header(msg)<0 || msg->to==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		return 3;
	}

	if(get_to(msg)==NULL)
	{
		LM_ERR("cannot get TO header\n");
		return 3;
	}

	return 0;
}

/**
 *
 */
int th_msg_received(void *data)
{
	sip_msg_t msg;
	str *obuf;
	char *nbuf = NULL;
	int direction;
	int dialog;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(th_prepare_msg(&msg)!=0)
	{
		goto done;
	}

	if(th_skip_msg(&msg))
	{
		goto done;
	}

	direction = 0;
	th_cookie_value.s = "xx";
	th_cookie_value.len = 2;
	if(msg.first_line.type==SIP_REQUEST)
	{
		if(th_sanity_checks!=0)
		{
			if(scb.check_defaults(&msg)<1)
			{
				LM_ERR("sanity checks failed\n");
				goto done;
			}
		}
		dialog = (get_to(&msg)->tag_value.len>0)?1:0;
		if(dialog)
		{
			direction = th_route_direction(&msg);
			if(direction<0)
			{
				LM_ERR("not able to detect direction\n");
				goto done;
			}
			th_cookie_value.s = (direction==0)?"dc":"uc";
		} else {
			th_cookie_value.s = "di";
		}
		if(dialog)
		{
			/* dialog request */
			th_unmask_ruri(&msg);
			th_unmask_route(&msg);
			th_unmask_refer_to(&msg);
			if(direction==1)
			{
				th_unmask_callid(&msg);
			}
		}
	} else {
		/* reply */
		if(msg.via2==0)
		{
			/* one Via in received reply -- it is for local generated request
			 * - nothing to unhide unless is CANCEL/ACK */
			if((get_cseq(&msg)->method_id)&(METHOD_CANCEL))
				goto done;
		}

		th_unmask_via(&msg, &th_cookie_value);
		th_flip_record_route(&msg, 0);
		if(th_cookie_value.s[0]=='u')
		{
			th_cookie_value.s = "dc";
		} else {
			th_cookie_value.s = "uc";
			th_unmask_callid(&msg);
		}
		th_cookie_value.len = 2;
	}

	LM_DBG("adding cookie: %.*s\n", th_cookie_value.len, th_cookie_value.s);

	th_add_cookie(&msg);
	nbuf = th_msg_update(&msg, (unsigned int*)&obuf->len);

	if(obuf->len>=BUF_SIZE)
	{
		LM_ERR("new buffer overflow (%d)\n", obuf->len);
		pkg_free(nbuf);
		return -1;
	}
	memcpy(obuf->s, nbuf, obuf->len);
	obuf->s[obuf->len] = '\0';

done:
	if(nbuf!=NULL)
		pkg_free(nbuf);
	free_sip_msg(&msg);
	return 0;
}

/**
 *
 */
int th_msg_sent(void *data)
{
	sip_msg_t msg;
	str *obuf;
	int direction;
	int dialog;
	int local;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(th_prepare_msg(&msg)!=0)
	{
		goto done;
	}

	if(th_skip_msg(&msg))
	{
		goto done;
	}

	th_cookie_value.s = th_get_cookie(&msg, &th_cookie_value.len);
	LM_DBG("the COOKIE is [%.*s]\n", th_cookie_value.len, th_cookie_value.s);
	if(th_cookie_value.s[0]!='x')
		th_del_cookie(&msg);
	if(msg.first_line.type==SIP_REQUEST)
	{
		direction = (th_cookie_value.s[0]=='u')?1:0; /* upstream/downstram */
		dialog = (get_to(&msg)->tag_value.len>0)?1:0;

		if(msg.via2==0) {
			local = 1;
			if(direction==0 && th_cookie_value.s[1]=='l') {
				/* downstream local request (e.g., dlg bye) */
				local = 2;
			}
		} else {
			/* more than one Via, but no received th cookie */
			local = (th_cookie_value.s[0]!='d' && th_cookie_value.s[0]!='u')?1:0;
		}
		/* local generated requests */
		if(local)
		{
			/* ACK and CANCEL go downstream */
			if(get_cseq(&msg)->method_id==METHOD_ACK
					|| get_cseq(&msg)->method_id==METHOD_CANCEL
					|| local==2)
			{
				th_mask_callid(&msg);
				goto ready;
			} else {
				/* should be for upstream */
				goto done;
			}
		}
		th_mask_via(&msg);
		th_mask_contact(&msg);
		th_mask_record_route(&msg);
		if(dialog)
		{
			/* dialog request */
			if(direction==0)
			{
				/* downstream */
				th_mask_callid(&msg);
			}
		} else {
			/* initial request */
			th_update_hdr_replaces(&msg);
			th_mask_callid(&msg);
		}
	} else {
		/* reply */
		if(th_cookie_value.s[th_cookie_value.len-1]=='x')
		{
			/* ?!?! - we should have a cookie in any reply case */
			goto done;
		}
		if(th_cookie_value.s[th_cookie_value.len-1]=='v')
		{
			/* reply generated locally - direction was set by request */
			if(th_cookie_value.s[0]=='u')
			{
				th_mask_callid(&msg);
			}
		} else {
			th_flip_record_route(&msg, 1);
			th_mask_contact(&msg);
			if(th_cookie_value.s[0]=='d')
			{
				th_mask_callid(&msg);
			}
		}
	}

ready:
	obuf->s = th_msg_update(&msg, (unsigned int*)&obuf->len);

done:
	free_sip_msg(&msg);
	return 0;
}

