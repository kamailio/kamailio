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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

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
int sipdump_mode = SIPDUMP_MODE_WTEXT;
static str sipdump_event_callback = STR_NULL;
static int sipdump_fage = 0;

static int sipdump_event_route_idx = -1;

static void sipdump_storage_clean(unsigned int ticks, void* param);

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
	{"fage",           PARAM_INT,   &sipdump_fage},
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
	if(!(sipdump_mode & (SIPDUMP_MODE_WTEXT |SIPDUMP_MODE_WPCAP
							| SIPDUMP_MODE_EVROUTE))) {
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

	if(sipdump_mode & (SIPDUMP_MODE_WTEXT|SIPDUMP_MODE_WPCAP)) {
		register_basic_timers(1);
	}

	if(sipdump_fage>0) {
		if(sr_wtimer_add(sipdump_storage_clean, NULL, 600)<0) {
			return -1;
		}
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

	if(!(sipdump_mode & (SIPDUMP_MODE_WTEXT|SIPDUMP_MODE_WPCAP))) {
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

/**
 *
 */
int ki_sipdump_send(sip_msg_t *msg, str *stag)
{
	sipdump_data_t isd;
	sipdump_data_t *osd = NULL;
	char srcip_buf[IP_ADDR_MAX_STRZ_SIZE];

	if(!sipdump_enabled())
		return 1;

	if(!(sipdump_mode & (SIPDUMP_MODE_WTEXT|SIPDUMP_MODE_WPCAP))) {
		LM_WARN("writing to file is disabled - ignoring\n");
		return 1;
	}

	memset(&isd, 0, sizeof(sipdump_data_t));

	gettimeofday(&isd.tv, NULL);
	isd.data.s = msg->buf;
	isd.data.len = msg->len;
	isd.pid = my_pid();
	isd.procno = process_no;
	isd.tag = *stag;
	isd.protoid = msg->rcv.proto;
	isd.afid = msg->rcv.src_ip.af;
	isd.src_ip.len = ip_addr2sbuf(&msg->rcv.src_ip, srcip_buf,
			IP_ADDR_MAX_STRZ_SIZE);
	srcip_buf[isd.src_ip.len] = 0;
	isd.src_ip.s = srcip_buf;
	isd.src_port = msg->rcv.src_port;
	if(msg->rcv.bind_address==NULL
			|| msg->rcv.bind_address->address_str.s==NULL) {
		if(msg->rcv.src_ip.af == AF_INET6) {
			isd.dst_ip.len = 3;
			isd.dst_ip.s = "::2";
		} else {
			isd.dst_ip.len = 7;
			isd.dst_ip.s = "0.0.0.0";
		}
		isd.dst_port = 0;
	} else {
		isd.dst_ip = msg->rcv.bind_address->address_str;
		isd.dst_port = (int)msg->rcv.bind_address->port_no;
	}

	if(sipdump_data_clone(&isd, &osd)<0) {
		LM_ERR("failed to clone sipdump data\n");
		return -1;
	}

	if(sipdump_list_add(osd)<0) {
		LM_ERR("failed to add data to dump queue\n");
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
static sipdump_data_t* sipdump_event_data = NULL;

/**
 *
 */
int sipdump_event_route(sipdump_data_t* sdi)
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
	sipdump_event_data = sdi;

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
	sipdump_event_data = NULL;
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
	sipdump_data_t isd;
	sipdump_data_t *osd = NULL;
	char srcip_buf[IP_ADDR_MAX_STRZ_SIZE];

	if(!sipdump_enabled())
		return 0;

	memset(&isd, 0, sizeof(sipdump_data_t));

	gettimeofday(&isd.tv, NULL);
	isd.data = *((str*)evp->data);
	isd.tag.s = "rcv";
	isd.tag.len = 3;
	isd.pid = my_pid();
	isd.procno = process_no;
	isd.protoid = evp->rcv->proto;
	isd.afid = (evp->rcv->bind_address!=NULL
				&& evp->rcv->bind_address->address.af==AF_INET6)?AF_INET6:AF_INET;
	isd.src_ip.len = ip_addr2sbuf(&evp->rcv->src_ip, srcip_buf,
					IP_ADDR_MAX_STRZ_SIZE);
	srcip_buf[isd.src_ip.len] = '\0';
	isd.src_ip.s = srcip_buf;
	isd.src_port = evp->rcv->src_port;
	if(evp->rcv->bind_address==NULL
			|| evp->rcv->bind_address->address_str.s==NULL) {
		if(isd.afid == AF_INET6) {
			isd.dst_ip.len = 3;
			isd.dst_ip.s = "::2";
		} else {
			isd.dst_ip.len = 7;
			isd.dst_ip.s = "0.0.0.0";
		}
		isd.dst_port = 0;
	} else {
		isd.dst_ip = evp->rcv->bind_address->address_str;
		isd.dst_port = (int)evp->rcv->bind_address->port_no;
	}

	if(sipdump_mode & SIPDUMP_MODE_EVROUTE) {
		if(sipdump_event_route(&isd) == DROP_R_F) {
			/* drop() used in event_route - all done */
			return 0;
		}
	}

	if(!(sipdump_mode & (SIPDUMP_MODE_WTEXT|SIPDUMP_MODE_WPCAP))) {
		return 0;
	}

	if(sipdump_data_clone(&isd, &osd)<0) {
		LM_ERR("failed to close sipdump data\n");
		return -1;
	}

	if(sipdump_list_add(osd)<0) {
		LM_ERR("failed to add data to dump queue\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int sipdump_msg_sent(sr_event_param_t *evp)
{
	sipdump_data_t isd;
	sipdump_data_t *osd = NULL;
	ip_addr_t ip;
	char dstip_buf[IP_ADDR_MAX_STRZ_SIZE];

	if(!sipdump_enabled())
		return 0;

	memset(&isd, 0, sizeof(sipdump_data_t));

	gettimeofday(&isd.tv, NULL);
	isd.data = *((str*)evp->data);
	isd.tag.s = "snd";
	isd.tag.len = 3;
	isd.pid = my_pid();
	isd.procno = process_no;
	isd.protoid = evp->dst->proto;
	isd.afid = evp->dst->send_sock->address.af;

	if(evp->dst->send_sock==NULL || evp->dst->send_sock->address_str.s==NULL) {
		if(evp->dst->send_sock->address.af == AF_INET6) {
			isd.src_ip.len = 3;
			isd.src_ip.s = "::2";
		} else {
			isd.src_ip.len = 7;
			isd.src_ip.s = "0.0.0.0";
		}
		isd.src_port = 0;
	} else {
		isd.src_ip = evp->dst->send_sock->address_str;
		isd.src_port = (int)evp->dst->send_sock->port_no;
	}
	su2ip_addr(&ip, &evp->dst->to);
	isd.dst_ip.len = ip_addr2sbuf(&ip, dstip_buf, IP_ADDR_MAX_STRZ_SIZE);
	dstip_buf[isd.dst_ip.len] = '\0';
	isd.dst_ip.s = dstip_buf;
	isd.dst_port = (int)su_getport(&evp->dst->to);

	if(sipdump_mode & SIPDUMP_MODE_EVROUTE) {
		sipdump_event_route(&isd);
	}

	if(!(sipdump_mode & (SIPDUMP_MODE_WTEXT|SIPDUMP_MODE_WPCAP))) {
		return 0;
	}

	if(sipdump_data_clone(&isd, &osd)<0) {
		LM_ERR("failed to clone sipdump data\n");
		return -1;
	}

	if(sipdump_list_add(osd)<0) {
		LM_ERR("failed to add data to dump queue\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
static void sipdump_storage_clean(unsigned int ticks, void* param)
{
	DIR *dlist = NULL;
	struct stat fstatbuf;
	struct dirent *dentry = NULL;
	str fname = STR_NULL;
	char *cwd = NULL;
	time_t tnow;
	int fcount = 0;

	if(sipdump_folder.s==NULL || sipdump_folder.len<=0
			|| sipdump_fprefix.s==NULL || sipdump_fprefix.len<=0) {
		return;
	}
	cwd = getcwd(NULL, 0);
	if(cwd==NULL) {
		LM_ERR("getcwd failed\n");
		return;
	}
	if ((chdir(sipdump_folder.s)==-1)) {
		LM_ERR("chdir to [%s] failed\n", sipdump_folder.s);
		free(cwd);
		return;
	}

	dlist = opendir(sipdump_folder.s);
	if (dlist==NULL) {
		LM_ERR("unable to read directory [%s]\n", sipdump_folder.s);
		goto done;
	}

	tnow = time(NULL);
	while ((dentry = readdir(dlist))) {
		fname.s = dentry->d_name;
		fname.len = strlen(fname.s);

		/* ignore '.' and '..' */
		if(fname.len==1 && strcmp(fname.s, ".")==0) { continue; }
		if(fname.len==2 && strcmp(fname.s, "..")==0) { continue; }

		if(fname.len<=sipdump_fprefix.len
				|| strncmp(fname.s, sipdump_fprefix.s, sipdump_fprefix.len)!=0) {
			continue;
		}
		if(lstat(fname.s, &fstatbuf) == -1) {
			LM_ERR("stat failed on [%s]\n", fname.s);
			continue;
		}
		if (S_ISREG(fstatbuf.st_mode)) {
			/* check last modification time */
			if ((tnow - sipdump_fage) > fstatbuf.st_mtime) {
				LM_DBG("deleting [%s]\n", fname.s);
				unlink (fname.s);
				fcount++;
			}
		}
	}
	closedir(dlist);
	if(fcount>0) {
		LM_DBG("deleted %d files\n", fcount);
	}

done:
	if((chdir(cwd)==-1)) {
		LM_ERR("chdir to [%s] failed\n", cwd);
		goto done;
	}
	free(cwd);
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

	if (sipdump_event_data==NULL) {
		sr_kemi_xval_null(&_ksr_kemi_sipdump_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_sipdump_xval;
	}
	_ksr_kemi_sipdump_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_sipdump_xval.v.s = sipdump_event_data->data;
	return &_ksr_kemi_sipdump_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_sipdump_get_tag(sip_msg_t *msg)
{
	memset(&_ksr_kemi_sipdump_xval, 0, sizeof(sr_kemi_xval_t));

	if (sipdump_event_data==NULL) {
		sr_kemi_xval_null(&_ksr_kemi_sipdump_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_sipdump_xval;
	}
	_ksr_kemi_sipdump_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_sipdump_xval.v.s = sipdump_event_data->tag;
	return &_ksr_kemi_sipdump_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_sipdump_get_src_ip(sip_msg_t *msg)
{
	memset(&_ksr_kemi_sipdump_xval, 0, sizeof(sr_kemi_xval_t));

	if (sipdump_event_data==NULL) {
		sr_kemi_xval_null(&_ksr_kemi_sipdump_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_sipdump_xval;
	}
	_ksr_kemi_sipdump_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_sipdump_xval.v.s = sipdump_event_data->src_ip;
	return &_ksr_kemi_sipdump_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_sipdump_get_dst_ip(sip_msg_t *msg)
{
	memset(&_ksr_kemi_sipdump_xval, 0, sizeof(sr_kemi_xval_t));

	if (sipdump_event_data==NULL) {
		sr_kemi_xval_null(&_ksr_kemi_sipdump_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_ksr_kemi_sipdump_xval;
	}
	_ksr_kemi_sipdump_xval.vtype = SR_KEMIP_STR;
	_ksr_kemi_sipdump_xval.v.s = sipdump_event_data->dst_ip;
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
			else if(strncmp(in->s, "dst_port", 8)==0)
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
	str saf = str_init("ipv4");
	str sproto = str_init("none");

	if (sipdump_event_data==NULL) {
		return pv_get_null(msg, param, res);
	}

	switch(param->pvn.u.isname.name.n) {
		case 1: /* buf */
			return pv_get_strval(msg, param, res, &sipdump_event_data->data);
		case 2: /* len */
			return pv_get_uintval(msg, param, res, sipdump_event_data->data.len);
		case 3: /* af */
			if(sipdump_event_data->afid==AF_INET6) {
				saf.s = "ipv6";
			}
			return pv_get_strval(msg, param, res, &saf);
		case 4: /* proto */
			get_valid_proto_string(sipdump_event_data->protoid, 0, 0, &sproto);
			return pv_get_strval(msg, param, res, &sproto);
		case 6: /* src_ip*/
			return pv_get_strval(msg, param, res, &sipdump_event_data->src_ip);
		case 7: /* dst_ip*/
			return pv_get_strval(msg, param, res, &sipdump_event_data->dst_ip);
		case 8: /* src_port */
			return pv_get_uintval(msg, param, res, sipdump_event_data->src_port);
		case 9: /* dst_port */
			return pv_get_uintval(msg, param, res, sipdump_event_data->dst_port);
		default:
			/* 0 - tag */
			return pv_get_strval(msg, param, res, &sipdump_event_data->tag);
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
