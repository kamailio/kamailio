/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/pt.h"
#include "../../core/timer_proc.h"
#include "../../core/mod_fix.h"
#include "../../core/fmsg.h"
#include "../../core/events.h"
#include "../../core/kemi.h"

#include "sipdump_write.h"

MODULE_VERSION

static int sipdump_enable = 0;
int sipdump_rotate = 7200;
static int sipdump_wait = 100;
static str sipdump_folder = str_init("/tmp");
static str sipdump_fprefix = str_init("kamailio-sipdump-");
static int sipdump_mode = SIPDUMP_MODE_WFILE;
static str sipdump_event_callback = STR_NULL;

static int sipdump_event_route_idx = -1;

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_sipdump_send(sip_msg_t *msg, char *ptag, char *str2);

int sipdump_msg_received(sr_event_param_t *evp);
int sipdump_msg_sent(sr_event_param_t *evp);

int pv_parse_sipdump_name(pv_spec_t *sp, str *in);
int pv_get_sipdump(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);

/* clang-format off */
static cmd_export_t cmds[]={
	{"sipdump_send", (cmd_function)w_sipdump_send, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"enable",         PARAM_INT,   &sipdump_enable},
	{"wait",           PARAM_INT,   &sipdump_wait},
	{"rotate",         PARAM_INT,   &sipdump_rotate},
	{"folder",         PARAM_STR,   &sipdump_folder},
	{"fprefix",        PARAM_STR,   &sipdump_fprefix},
	{"mode",           PARAM_INT,   &sipdump_mode},
	{"event_callback", PARAM_STR,   &sipdump_event_callback},

	{0, 0, 0}
};

static pv_export_t mod_pvs[] = {

	{ {"sipdump", (sizeof("sipdump")-1)}, PVT_OTHER, pv_get_sipdump, 0,
		pv_parse_sipdump_name, 0, 0, 0 },

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
	"sipdump",      /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,           /* exported functions */
	params,         /* exported parameters */
	0,              /* exported rpc functions */
	mod_pvs,        /* exported pseudo-variables */
	0,              /* response handling function */
	mod_init,       /* module init function */
	child_init,     /* per child init function */
	mod_destroy     /* module destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	if(!(sipdump_mode & (SIPDUMP_MODE_WFILE | SIPDUMP_MODE_EVROUTE))) {
		LM_ERR("invalid mode parameter\n");
		return -1;
	}

	if(sipdump_rpc_init()<0) {
		LM_ERR("failed to register rpc commands\n");
		return -1;
	}

	if(sipdump_file_init(&sipdump_folder, &sipdump_fprefix) < 0) {
		LM_ERR("cannot initialize storage file\n");
		return -1;
	}

	if(sipdump_list_init(sipdump_enable) < 0) {
		LM_ERR("cannot initialize internal structure\n");
		return -1;
	}

	if(sipdump_mode & SIPDUMP_MODE_EVROUTE) {
		sipdump_event_route_idx = route_lookup(&event_rt, "sipdump:msg");
		if (sipdump_event_route_idx>=0 && event_rt.rlist[sipdump_event_route_idx]==0) {
			sipdump_event_route_idx = -1; /* disable */
		}
		if(faked_msg_init() <0) {
			LM_ERR("cannot initialize faked msg structure\n");
			return -1;
		}
	}

	if(sipdump_mode & SIPDUMP_MODE_WFILE) {
		register_basic_timers(1);
	}

	sr_event_register_cb(SREV_NET_DATA_IN, sipdump_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, sipdump_msg_sent);

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{

	if(rank != PROC_MAIN)
		return 0;

	if(!(sipdump_mode & SIPDUMP_MODE_WFILE)) {
		return 0;
	}

	if(fork_basic_utimer(PROC_TIMER, "SIPDUMP WRITE TIMER", 1 /*socks flag*/,
			   sipdump_timer_exec, NULL, sipdump_wait /*usec*/)
				< 0) {
		LM_ERR("failed to register timer routine as process\n");
		return -1; /* error */
	}

	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	sipdump_list_destroy();
}

#define SIPDUMP_WBUF_SIZE 65536
static char _sipdump_wbuf[SIPDUMP_WBUF_SIZE];

typedef struct sipdump_info {
	str tag;
	str buf;
	str af;
	str proto;
	str src_ip;
	int src_port;
	str dst_ip;
	int dst_port;
} sipdump_info_t;

/**
 *
 */
int sipdump_buffer_write(sipdump_info_t *sdi, str *obuf)
{
	struct timeval tv;
	struct tm ti;
	char t_buf[26] = {0};

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &ti);
	obuf->len = snprintf(_sipdump_wbuf, SIPDUMP_WBUF_SIZE,
		"====================\n"
		"tag: %.*s\n"
		"pid: %d\n"
		"process: %d\n"
		"time: %lu.%06lu\n"
		"date: %s"
		"proto: %.*s %.*s\n"
		"srcip: %.*s\n"
		"srcport: %d\n"
		"dstip: %.*s\n"
		"dstport: %d\n"
		"~~~~~~~~~~~~~~~~~~~~\n"
		"%.*s"
		"||||||||||||||||||||\n",
		sdi->tag.len, sdi->tag.s,
		my_pid(),
		process_no,
		(unsigned long)tv.tv_sec, (unsigned long)tv.tv_usec,
		asctime_r(&ti, t_buf),
		sdi->proto.len, sdi->proto.s, sdi->af.len, sdi->af.s,
		sdi->src_ip.len, sdi->src_ip.s, sdi->src_port,
		sdi->dst_ip.len, sdi->dst_ip.s, sdi->dst_port,
		sdi->buf.len, sdi->buf.s
	);
	obuf->s = _sipdump_wbuf;

	return 0;
}

/**
 *
 */
int ki_sipdump_send(sip_msg_t *msg, str *stag)
{
	str wdata;
	sipdump_info_t sdi;
	char srcip_buf[IP_ADDR_MAX_STRZ_SIZE];

	if(!sipdump_enabled())
		return 1;

	if(!(sipdump_mode & SIPDUMP_MODE_WFILE)) {
		LM_WARN("writing to file is disabled - ignoring\n");
		return 1;
	}

	memset(&sdi, 0, sizeof(sipdump_info_t));

	sdi.buf.s = msg->buf;
	sdi.buf.len = msg->len;
	sdi.tag = *stag;
	sdi.src_ip.len = ip_addr2sbufz(&msg->rcv.src_ip, srcip_buf,
			IP_ADDR_MAX_STRZ_SIZE);
	sdi.src_ip.s = srcip_buf;
	sdi.src_port = msg->rcv.src_port;
	if(msg->rcv.bind_address==NULL
			|| msg->rcv.bind_address->address_str.s==NULL) {
		sdi.dst_ip.len = 7;
		sdi.dst_ip.s = "0.0.0.0";
		sdi.dst_port = 0;
	} else {
		sdi.dst_ip = msg->rcv.bind_address->address_str;
		sdi.dst_port = (int)msg->rcv.bind_address->port_no;
	}

	sdi.af.len = 4;
	if(msg->rcv.bind_address!=NULL
			&& msg->rcv.bind_address->address.af==AF_INET6) {
		sdi.af.s = "ipv6";
	} else {
		sdi.af.s = "ipv4";
	}
	sdi.proto.s = "none";
	sdi.proto.len = 4;
	get_valid_proto_string(msg->rcv.proto, 0, 0, &sdi.proto);

	if(sipdump_buffer_write(&sdi, &wdata)<0) {
		LM_ERR("failed to write to buffer\n");
		return -1;
	}

	if(sipdump_list_add(&wdata)<0) {
		LM_ERR("failed to add data to write list\n");
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_sipdump_send(sip_msg_t *msg, char *ptag, char *str2)
{
	str stag;

	if(!sipdump_enabled())
		return 1;

	if(fixup_get_svalue(msg, (gparam_t*)ptag, &stag)<0) {
		LM_ERR("failed to get tag parameter\n");
		return -1;
	}
	return ki_sipdump_send(msg, &stag);
}

/**
 *
 */
static sipdump_info_t* sipdump_event_info = NULL;

/**
 *
 */
int sipdump_event_route(sipdump_info_t* sdi)
{
	int backup_rt;
	run_act_ctx_t ctx;
	run_act_ctx_t *bctx;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("sipdump:msg");
	sip_msg_t *fmsg = NULL;

	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	fmsg = faked_msg_next();
	sipdump_event_info = sdi;

	if(sipdump_event_route_idx>=0) {
		run_top_route(event_rt.rlist[sipdump_event_route_idx], fmsg, 0);
	} else {
		keng = sr_kemi_eng_get();
		if (keng!=NULL) {
			bctx = sr_kemi_act_ctx_get();
			sr_kemi_act_ctx_set(&ctx);
			(void)sr_kemi_route(keng, fmsg, EVENT_ROUTE,
						&sipdump_event_callback, &evname);
			sr_kemi_act_ctx_set(bctx);
		}
	}
	sipdump_event_info = NULL;
	set_route_type(backup_rt);
	if(ctx.run_flags & DROP_R_F) {
		return DROP_R_F;
	}
	return RETURN_R_F;
}

/**
 *
 */
int sipdump_msg_received(sr_event_param_t *evp)
{
	str wdata;
	sipdump_info_t sdi;
	char srcip_buf[IP_ADDR_MAX_STRZ_SIZE];

	if(!sipdump_enabled())
		return 0;

	memset(&sdi, 0, sizeof(sipdump_info_t));

	sdi.buf = *((str*)evp->data);
	sdi.tag.s = "rcv";
	sdi.tag.len = 3;
	sdi.src_ip.len = ip_addr2sbufz(&evp->rcv->src_ip, srcip_buf,
					IP_ADDR_MAX_STRZ_SIZE);
	sdi.src_ip.s = srcip_buf;
	sdi.src_port = evp->rcv->src_port;
	if(evp->rcv->bind_address==NULL
			|| evp->rcv->bind_address->address_str.s==NULL) {
		sdi.dst_ip.len = 7;
		sdi.dst_ip.s = "0.0.0.0";
		sdi.dst_port = 0;
	} else {
		sdi.dst_ip = evp->rcv->bind_address->address_str;
		sdi.dst_port = (int)evp->rcv->bind_address->port_no;
	}
	sdi.af.len = 4;
	if(evp->rcv->bind_address!=NULL
			&& evp->rcv->bind_address->address.af==AF_INET6) {
		sdi.af.s = "ipv6";
	} else {
		sdi.af.s = "ipv4";
	}
	sdi.proto.s = "none";
	sdi.proto.len = 4;
	get_valid_proto_string(evp->rcv->proto, 0, 0, &sdi.proto);

	if(sipdump_mode & SIPDUMP_MODE_EVROUTE) {
		if(sipdump_event_route(&sdi) == DROP_R_F) {
			/* drop() used in event_route - all done */
			return 0;
		}
	}

	if(!(sipdump_mode & SIPDUMP_MODE_WFILE)) {
		return 0;
	}

	if(sipdump_buffer_write(&sdi, &wdata)<0) {
		LM_ERR("failed to write to buffer\n");
		return -1;
	}

	if(sipdump_list_add(&wdata)<0) {
		LM_ERR("failed to add data to write list\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int sipdump_msg_sent(sr_event_param_t *evp)
{
	str wdata;
	sipdump_info_t sdi;
	ip_addr_t ip;
	char dstip_buf[IP_ADDR_MAX_STRZ_SIZE];

	if(!sipdump_enabled())
		return 0;

	memset(&sdi, 0, sizeof(sipdump_info_t));

	sdi.buf = *((str*)evp->data);
	sdi.tag.s = "snd";
	sdi.tag.len = 3;

	if(evp->dst->send_sock==NULL || evp->dst->send_sock->address_str.s==NULL) {
		sdi.src_ip.len = 7;
		sdi.src_ip.s = "0.0.0.0";
		sdi.src_port = 0;
	} else {
		sdi.src_ip = evp->dst->send_sock->address_str;
		sdi.src_port = (int)evp->dst->send_sock->port_no;
	}
	su2ip_addr(&ip, &evp->dst->to);
	sdi.dst_ip.len = ip_addr2sbufz(&ip, dstip_buf, IP_ADDR_MAX_STRZ_SIZE);
	sdi.dst_ip.s = dstip_buf;
	sdi.dst_port = (int)su_getport(&evp->dst->to);

	sdi.af.len = 4;
	if(evp->dst->send_sock->address.af==AF_INET6) {
		sdi.af.s = "ipv6";
	} else {
		sdi.af.s = "ipv4";
	}
	sdi.proto.s = "none";
	sdi.proto.len = 4;
	get_valid_proto_string(evp->dst->proto, 0, 0, &sdi.proto);

	if(sipdump_mode & SIPDUMP_MODE_EVROUTE) {
		sipdump_event_route(&sdi);
	}

	if(!(sipdump_mode & SIPDUMP_MODE_WFILE)) {
		return 0;
	}

	if(sipdump_buffer_write(&sdi, &wdata)<0) {
		LM_ERR("failed to write to buffer\n");
		return -1;
	}

	if(sipdump_list_add(&wdata)<0) {
		LM_ERR("failed to add data to write list\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
static sr_kemi_xval_t _ksr_kemi_sipdump_xval = {0};

/**
 *
 */
static sr_kemi_xval_t* ki_sipdump_get_buf(sip_msg_t *msg)
{
	memset(&_ksr_kemi_sipdump_xval, 0, sizeof(sr_kemi_xval_t));

	if (sipdump_event_info==NULL) {
		sr_kemi_xval_null(&_ksr_kemi_sipdump_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_sipdump_xval;
	}
	_ksr_kemi_sipdump_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_sipdump_xval.v.s = sipdump_event_info->buf;
	return &_ksr_kemi_sipdump_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_sipdump_get_tag(sip_msg_t *msg)
{
	memset(&_ksr_kemi_sipdump_xval, 0, sizeof(sr_kemi_xval_t));

	if (sipdump_event_info==NULL) {
		sr_kemi_xval_null(&_ksr_kemi_sipdump_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_sipdump_xval;
	}
	_ksr_kemi_sipdump_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_sipdump_xval.v.s = sipdump_event_info->tag;
	return &_ksr_kemi_sipdump_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_sipdump_get_src_ip(sip_msg_t *msg)
{
	memset(&_ksr_kemi_sipdump_xval, 0, sizeof(sr_kemi_xval_t));

	if (sipdump_event_info==NULL) {
		sr_kemi_xval_null(&_ksr_kemi_sipdump_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_sipdump_xval;
	}
	_ksr_kemi_sipdump_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_sipdump_xval.v.s = sipdump_event_info->src_ip;
	return &_ksr_kemi_sipdump_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_sipdump_get_dst_ip(sip_msg_t *msg)
{
	memset(&_ksr_kemi_sipdump_xval, 0, sizeof(sr_kemi_xval_t));

	if (sipdump_event_info==NULL) {
		sr_kemi_xval_null(&_ksr_kemi_sipdump_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_sipdump_xval;
	}
	_ksr_kemi_sipdump_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_sipdump_xval.v.s = sipdump_event_info->dst_ip;
	return &_ksr_kemi_sipdump_xval;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_sipdump_exports[] = {
	{ str_init("sipdump"), str_init("send"),
		SR_KEMIP_INT, ki_sipdump_send,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sipdump"), str_init("get_buf"),
		SR_KEMIP_XVAL, ki_sipdump_get_buf,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sipdump"), str_init("get_tag"),
		SR_KEMIP_XVAL, ki_sipdump_get_tag,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sipdump"), str_init("get_src_ip"),
		SR_KEMIP_XVAL, ki_sipdump_get_src_ip,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sipdump"), str_init("get_dst_ip"),
		SR_KEMIP_XVAL, ki_sipdump_get_dst_ip,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int pv_parse_sipdump_name(pv_spec_t *sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 2:
			if(strncmp(in->s, "af", 2)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		case 3:
			if(strncmp(in->s, "buf", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "len", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "tag", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 5:
			if(strncmp(in->s, "proto", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else goto error;
		break;
		case 6:
			if(strncmp(in->s, "sproto", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "src_ip", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else if(strncmp(in->s, "dst_ip", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
			else goto error;
		break;
		case 8:
			if(strncmp(in->s, "src_port", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
			if(strncmp(in->s, "dst_port", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 9;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV snd name %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
int pv_get_sipdump(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res)
{
	if (sipdump_event_info==NULL) {
		return pv_get_null(msg, param, res);
	}

	switch(param->pvn.u.isname.name.n) {
		case 1: /* buf */
			return pv_get_strval(msg, param, res, &sipdump_event_info->buf);
		case 2: /* len */
			return pv_get_uintval(msg, param, res, sipdump_event_info->buf.len);
		case 3: /* af */
			return pv_get_strval(msg, param, res, &sipdump_event_info->af);
		case 4: /* proto */
			return pv_get_strval(msg, param, res, &sipdump_event_info->proto);
		case 6: /* src_ip*/
			return pv_get_strval(msg, param, res, &sipdump_event_info->src_ip);
		case 7: /* dst_ip*/
			return pv_get_strval(msg, param, res, &sipdump_event_info->dst_ip);
		case 8: /* src_port */
			return pv_get_uintval(msg, param, res, sipdump_event_info->src_port);
		case 9: /* dst_port */
			return pv_get_uintval(msg, param, res, sipdump_event_info->dst_port);
		default:
			/* 0 - tag */
			return pv_get_strval(msg, param, res, &sipdump_event_info->tag);
	}
}

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_sipdump_exports);
	return 0;
}
