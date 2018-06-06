/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 */

/*!
 * \file
 * \brief Kamailio topos :: Module interface
 * \ingroup topos
 * Module: \ref topos
 */

/*! \defgroup topoh Kamailio :: Topology stripping
 *
 * This module removes the SIP routing headers that show topology details.
 * The script interpreter gets the SIP messages with full content, so all
 * existing functionality is preserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/events.h"
#include "../../core/dprint.h"
#include "../../core/tcp_options.h"
#include "../../core/ut.h"
#include "../../core/forward.h"
#include "../../core/config.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_from.h"
#include "../../core/timer_proc.h"
#include "../../core/fmsg.h"
#include "../../core/onsend.h"
#include "../../core/kemi.h"

#include "../../lib/srdb1/db.h"
#include "../../lib/srutils/sruid.h"

#include "../../modules/sanity/api.h"

#include "tps_storage.h"
#include "tps_msg.h"
#include "api.h"

MODULE_VERSION


/* Database connection handle */
db1_con_t* _tps_db_handle = NULL;
/* DB functions */
db_func_t _tpsdbf;
/* sruid to get internal uid */
sruid_t _tps_sruid;

/** module parameters */
static str _tps_db_url = str_init(DEFAULT_DB_URL);
int _tps_param_mask_callid = 0;
int _tps_sanity_checks = 0;
str _tps_storage = str_init("db");

extern int _tps_branch_expire;
extern int _tps_dialog_expire;

int _tps_clean_interval = 60;

static int _tps_eventrt_outgoing = -1;
static str _tps_eventrt_callback = STR_NULL;
static str _tps_eventrt_name = str_init("topos:msg-outgoing");

sanity_api_t scb;

int tps_msg_received(sr_event_param_t *evp);
int tps_msg_sent(sr_event_param_t *evp);

static int tps_execute_event_route(sip_msg_t *msg, sr_event_param_t *evp);

/** module functions */
/* Module init function prototype */
static int mod_init(void);
/* Module child-init function prototype */
static int child_init(int rank);
/* Module destroy function prototype */
static void destroy(void);

int bind_topos(topos_api_t *api);

static cmd_export_t cmds[]={
	{"bind_topos",  (cmd_function)bind_topos,  0,
		0, 0, 0},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"storage",			PARAM_STR, &_tps_storage},
	{"db_url",			PARAM_STR, &_tps_db_url},
	{"mask_callid",		PARAM_INT, &_tps_param_mask_callid},
	{"sanity_checks",	PARAM_INT, &_tps_sanity_checks},
	{"branch_expire",	PARAM_INT, &_tps_branch_expire},
	{"dialog_expire",	PARAM_INT, &_tps_dialog_expire},
	{"clean_interval",	PARAM_INT, &_tps_clean_interval},
	{"event_callback",	PARAM_STR, &_tps_eventrt_callback},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"topos",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	destroy,    /* destroy function */
	child_init  /* child initialization function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	_tps_eventrt_outgoing = route_lookup(&event_rt, _tps_eventrt_name.s);
	if(_tps_eventrt_outgoing<0
			|| event_rt.rlist[_tps_eventrt_outgoing]==NULL) {
		_tps_eventrt_outgoing = -1;
	}

	if(faked_msg_init()<0) {
		LM_ERR("failed to init fmsg\n");
		return -1;
	}

	if(_tps_storage.len==2 && strncmp(_tps_storage.s, "db", 2)==0) {
		/* Find a database module */
		if (db_bind_mod(&_tps_db_url, &_tpsdbf)) {
			LM_ERR("unable to bind database module\n");
			return -1;
		}
		if (!DB_CAPABILITY(_tpsdbf, DB_CAP_ALL)) {
			LM_CRIT("database modules does not "
				"provide all functions needed\n");
			return -1;
		}
	} else {
		if(_tps_storage.len!=7 && strncmp(_tps_storage.s, "redis", 5)!=0) {
			LM_ERR("unknown storage type: %.*s\n",
					_tps_storage.len, _tps_storage.s);
			return -1;
		}
	}

	if(_tps_sanity_checks!=0) {
		if(sanity_load_api(&scb)<0) {
			LM_ERR("cannot bind to sanity module\n");
			goto error;
		}
	}
	if(tps_storage_lock_set_init()<0) {
		LM_ERR("failed to initialize locks set\n");
		return -1;
	}

	if(sruid_init(&_tps_sruid, '-', "tpsh", SRUID_INC)<0)
		return -1;

	sr_event_register_cb(SREV_NET_DATA_IN,  tps_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, tps_msg_sent);

#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
#endif

	if(sr_wtimer_add(tps_storage_clean, NULL, _tps_clean_interval)<0)
		return -1;

	return 0;
error:
	return -1;
}

/**
 *
 */
static int child_init(int rank)
{
	if(sruid_init(&_tps_sruid, '-', "tpsh", SRUID_INC)<0)
		return -1;

	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if(_tps_storage.len==2 && strncmp(_tps_storage.s, "db", 2)==0) {
		_tps_db_handle = _tpsdbf.init(&_tps_db_url);
		if (!_tps_db_handle) {
			LM_ERR("unable to connect database\n");
			return -1;
		}
	}
	return 0;

}

/**
 *
 */
static void destroy(void)
{
	if(_tps_storage.len==2 && strncmp(_tps_storage.s, "db", 2)==0) {
		if (_tps_db_handle) {
			_tpsdbf.close(_tps_db_handle);
			_tps_db_handle = 0;
		}
	}
	tps_storage_lock_set_destroy();
}

/**
 *
 */
int tps_prepare_msg(sip_msg_t *msg)
{
	if (parse_msg(msg->buf, msg->len, msg)!=0) {
		LM_DBG("outbuf buffer parsing failed!");
		return 1;
	}

	if(msg->first_line.type==SIP_REQUEST) {
		if(!IS_SIP(msg)) {
			LM_DBG("non sip request message\n");
			return 1;
		}
	} else if(msg->first_line.type!=SIP_REPLY) {
		LM_DBG("non sip message\n");
		return 1;
	}

	if(parse_headers(msg, HDR_VIA2_F, 0)<0) {
		LM_DBG("no via2 has been parsed\n");
	}

	if(parse_headers(msg, HDR_CSEQ_F, 0)!=0 || msg->cseq==NULL) {
		LM_ERR("cannot parse cseq header\n");
		return -1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0)==-1) {
		LM_DBG("parsing headers failed [[%.*s]]\n",
				msg->len, msg->buf);
		return 2;
	}

	if(parse_from_header(msg)<0) {
		LM_ERR("cannot parse FROM header\n");
		return 3;
	}

	if(parse_to_header(msg)<0 || msg->to==NULL) {
		LM_ERR("cannot parse TO header\n");
		return 3;
	}

	if(get_to(msg)==NULL) {
		LM_ERR("cannot get TO header\n");
		return 3;
	}

	return 0;
}

/**
 *
 */
int tps_msg_received(sr_event_param_t *evp)
{
	sip_msg_t msg;
	str *obuf;
	char *nbuf = NULL;
	int dialog;
	int ret;

	obuf = (str*)evp->data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	ret = 0;
	if(tps_prepare_msg(&msg)!=0) {
		goto done;
	}

	if(tps_skip_msg(&msg)) {
		goto done;
	}

	if(msg.first_line.type==SIP_REQUEST) {
		if(_tps_sanity_checks!=0) {
			if(scb.check_defaults(&msg)<1) {
				LM_ERR("sanity checks failed\n");
				goto done;
			}
		}
		dialog = (get_to(&msg)->tag_value.len>0)?1:0;
		if(dialog) {
			/* dialog request */
			tps_request_received(&msg, dialog);
		}
	} else {
		/* reply */
		if(msg.first_line.u.reply.statuscode==100) {
			/* nothing to do - it should be absorbed */
			goto done;
		}
		tps_response_received(&msg);
	}

	nbuf = tps_msg_update(&msg, (unsigned int*)&obuf->len);

	if(nbuf==NULL) {
		LM_ERR("not enough pkg memory for new message\n");
		ret = -1;
		goto done;
	}
	if(obuf->len>=BUF_SIZE) {
		LM_ERR("new buffer overflow (%d)\n", obuf->len);
		ret = -1;
		goto done;
	}
	memcpy(obuf->s, nbuf, obuf->len);
	obuf->s[obuf->len] = '\0';

done:
	if(nbuf!=NULL)
		pkg_free(nbuf);
	free_sip_msg(&msg);
	return ret;
}

/**
 *
 */
int tps_msg_sent(sr_event_param_t *evp)
{
	sip_msg_t msg;
	str *obuf;
	int dialog;
	int local;

	obuf = (str*)evp->data;

	if(tps_execute_event_route(NULL, evp)==1) {
		return 0;
	}

	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(tps_prepare_msg(&msg)!=0) {
		goto done;
	}

	if(tps_skip_msg(&msg)) {
		goto done;
	}

	if(msg.first_line.type==SIP_REQUEST) {
		dialog = (get_to(&msg)->tag_value.len>0)?1:0;

		local = 0;
		if(msg.via2==0) {
			local = 1;
		}

		if(local==1 && dialog==0) {
			if((get_cseq(&msg)->method_id) & (METHOD_OPTIONS|METHOD_NOTIFY)) {
				/* skip local out-of-dialog requests (e.g., keepalive) */
				goto done;
			}
			if(get_cseq(&msg)->method.len==4
					&& strncmp(get_cseq(&msg)->method.s, "KDMQ", 4)==0) {
				goto done;
			}
		}

		tps_request_sent(&msg, dialog, local);
	} else {
		/* reply */
		if(msg.first_line.u.reply.statuscode==100) {
			/* nothing to do - it should be locally generated */
			goto done;
		}
		tps_response_sent(&msg);
	}

	obuf->s = tps_msg_update(&msg, (unsigned int*)&obuf->len);

done:
	free_sip_msg(&msg);
	return 0;
}

/**
 *
 */
int tps_get_dialog_expire(void)
{
	return _tps_dialog_expire;
}

/**
 *
 */
int tps_get_branch_expire(void)
{
	return _tps_branch_expire;
}

/**
 *
 */
static int tps_execute_event_route(sip_msg_t *msg, sr_event_param_t *evp)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb;
	sr_kemi_eng_t *keng = NULL;
	struct onsend_info onsnd_info = {0};

	if(_tps_eventrt_outgoing<0) {
		if(_tps_eventrt_callback.s!=NULL || _tps_eventrt_callback.len>0) {
			keng = sr_kemi_eng_get();
			if(keng==NULL) {
				LM_DBG("event callback (%s) set, but no cfg engine\n",
						_tps_eventrt_callback.s);
				goto done;
			}
		}
	}

	if(_tps_eventrt_outgoing<0 && keng==NULL) {
		return 0;
	}

	LM_DBG("executing event_route[topos:...] (%d)\n",
			_tps_eventrt_outgoing);
	fmsg = faked_msg_next();

	onsnd_info.to = &evp->dst->to;
	onsnd_info.send_sock = evp->dst->send_sock;
	if(msg!=NULL) {
		onsnd_info.buf = msg->buf;
		onsnd_info.len = msg->len;
		onsnd_info.msg = msg;
	} else {
		onsnd_info.buf = fmsg->buf;
		onsnd_info.len = fmsg->len;
		onsnd_info.msg = fmsg;
	}
	p_onsend = &onsnd_info;

	rtb = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	if(_tps_eventrt_outgoing>=0) {
		run_top_route(event_rt.rlist[_tps_eventrt_outgoing], fmsg, &ctx);
	} else {
		if(keng!=NULL) {
			if(keng->froute(fmsg, EVENT_ROUTE,
						&_tps_eventrt_callback, &_tps_eventrt_name)<0) {
				LM_ERR("error running event route kemi callback\n");
				p_onsend=NULL;
				return -1;
			}
		}
	}
	set_route_type(rtb);
	if(ctx.run_flags&DROP_R_F) {
		LM_DBG("exit due to 'drop' in event route\n");
		p_onsend=NULL;
		return 1;
	}

done:
	p_onsend=NULL;
	return 0;
}

/**
 *
 */
int bind_topos(topos_api_t *api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	memset(api, 0, sizeof(topos_api_t));
	api->set_storage_api = tps_set_storage_api;
	api->get_dialog_expire = tps_get_dialog_expire;
	api->get_branch_expire = tps_get_branch_expire;

	return 0;
}