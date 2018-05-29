/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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

#include "dprint.h"
#include "forward.h"
#include "locking.h"
#include "dset.h"
#include "action.h"
#include "data_lump.h"
#include "data_lump_rpl.h"
#include "strutils.h"
#include "mem/shm.h"
#include "parser/parse_uri.h"
#include "parser/parse_hname2.h"
#include "parser/parse_methods.h"

#include "kemi.h"


#define SR_KEMI_HNAME_SIZE 128

/**
 *
 */
static run_act_ctx_t *_sr_kemi_act_ctx = NULL;

/**
 *
 */
void sr_kemi_act_ctx_set(run_act_ctx_t *ctx)
{
	_sr_kemi_act_ctx = ctx;
}

/**
 *
 */
run_act_ctx_t* sr_kemi_act_ctx_get(void)
{
	return _sr_kemi_act_ctx;
}

/**
 *
 */
static int sr_kemi_core_dbg(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_DBG("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_err(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_ERR("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_info(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_INFO("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_warn(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_WARN("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_notice(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_NOTICE("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_crit(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_CRIT("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_log(sip_msg_t *msg, str *level, str *txt)
{
	if(txt!=NULL && txt->s!=NULL) {
		if(level==NULL || level->s==NULL) {
			LM_ERR("%s", txt->s);
		} else {
			if(strcasecmp(level->s, "dbg")==0) {
				LM_DBG("%s", txt->s);
			} else if(strcasecmp(level->s, "info")==0) {
				LM_INFO("%s", txt->s);
			} else if(strcasecmp(level->s, "warn")==0) {
				LM_WARN("%s", txt->s);
			} else if(strcasecmp(level->s, "crit")==0) {
				LM_CRIT("%s", txt->s);
			} else {
				LM_ERR("%s", txt->s);
			}
		}
	}
	return 0;
}

/**
 *
 */
int sr_kemi_core_drop(sip_msg_t *msg)
{
	if(_sr_kemi_act_ctx==NULL)
		return 0;
	LM_DBG("drop action executed inside embedded interpreter\n");
	_sr_kemi_act_ctx->run_flags |= EXIT_R_F|DROP_R_F;
	return 0;
}

/**
 *
 */
static int sr_kemi_core_is_myself(sip_msg_t *msg, str *uri)
{
	struct sip_uri puri;
	int ret;

	if(uri==NULL || uri->s==NULL) {
		return SR_KEMI_FALSE;
	}
	if(uri->len>4 && (strncmp(uri->s, "sip:", 4)==0
				|| strncmp(uri->s, "sips:", 5)==0)) {
		if(parse_uri(uri->s, uri->len, &puri)!=0) {
			LM_ERR("failed to parse uri [%.*s]\n", uri->len, uri->s);
			return SR_KEMI_FALSE;
		}
		ret = check_self(&puri.host, (puri.port.s)?puri.port_no:0,
				(puri.transport_val.s)?puri.proto:0);
	} else {
		ret = check_self(uri, 0, 0);
	}
	if(ret==1) {
		return SR_KEMI_TRUE;
	}
	return SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_setflag(sip_msg_t *msg, int flag)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	setflag(msg, flag);
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_resetflag(sip_msg_t *msg, int flag)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	resetflag(msg, flag);
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_isflagset(sip_msg_t *msg, int flag)
{
	int ret;

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	ret = isflagset(msg, flag);
	if(ret>0)
		return SR_KEMI_TRUE;
	return SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_setbiflag(sip_msg_t *msg, int flag, int branch)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	setbflag(branch, flag);
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_resetbiflag(sip_msg_t *msg, int flag, int branch)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	resetbflag(branch, flag);
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_isbiflagset(sip_msg_t *msg, int flag, int branch)
{
	int ret;

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	ret = isbflagset(branch, flag);
	if(ret>0)
		return SR_KEMI_TRUE;
	return SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_setbflag(sip_msg_t *msg, int flag)
{
	return sr_kemi_core_setbiflag(msg, flag, 0);
}

/**
 *
 */
static int sr_kemi_core_resetbflag(sip_msg_t *msg, int flag)
{
	return sr_kemi_core_resetbiflag(msg, flag, 0);
}

/**
 *
 */
static int sr_kemi_core_isbflagset(sip_msg_t *msg, int flag)
{
	return sr_kemi_core_isbiflagset(msg, flag, 0);
}

/**
 *
 */
static int sr_kemi_core_setsflag(sip_msg_t *msg, int flag)
{
	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	setsflag(flag);
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_resetsflag(sip_msg_t *msg, int flag)
{
	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	resetsflag(flag);
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_issflagset(sip_msg_t *msg, int flag)
{
	int ret;

	if (!flag_in_range(flag)) {
		LM_ERR("invalid flag parameter %d\n", flag);
		return SR_KEMI_FALSE;
	}

	ret = issflagset(flag);
	if(ret>0)
		return SR_KEMI_TRUE;
	return SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_seturi(sip_msg_t *msg, str *uri)
{
	if(uri==NULL || uri->s==NULL) {
		LM_ERR("invalid uri parameter\n");
		return SR_KEMI_FALSE;
	}

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if(rewrite_uri(msg, uri)<0) {
		LM_ERR("updating r-uri failed\n");
		return SR_KEMI_FALSE;
	}
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_setuser(sip_msg_t *msg, str *user)
{
	struct action  act;
	struct run_act_ctx h;

	if(user==NULL || user->s==NULL) {
		LM_ERR("invalid user parameter\n");
		return SR_KEMI_FALSE;
	}

	if(msg==NULL) {
		LM_WARN("invalid user parameter\n");
		return SR_KEMI_FALSE;
	}

	memset(&act, 0, sizeof(act));
	act.val[0].type = STRING_ST;
	act.val[0].u.string = user->s;
	act.type = SET_USER_T;
	init_run_actions_ctx(&h);
	if (do_action(&h, &act, msg)<0) {
		LM_ERR("do action failed\n");
		return SR_KEMI_FALSE;
	}
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_sethost(sip_msg_t *msg, str *host)
{
	struct action  act;
	struct run_act_ctx h;

	if(host==NULL || host->s==NULL) {
		LM_ERR("invalid host parameter\n");
		return SR_KEMI_FALSE;
	}

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	memset(&act, 0, sizeof(act));
	act.val[0].type = STRING_ST;
	act.val[0].u.string = host->s;
	act.type = SET_HOST_T;
	init_run_actions_ctx(&h);
	if (do_action(&h, &act, msg)<0)
	{
		LM_ERR("do action failed\n");
		return SR_KEMI_FALSE;
	}
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_setdsturi(sip_msg_t *msg, str *uri)
{
	if(uri==NULL || uri->s==NULL) {
		LM_ERR("invalid uri parameter\n");
		return SR_KEMI_FALSE;
	}

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if(set_dst_uri(msg, uri)<0) {
		LM_ERR("setting dst uri failed\n");
		return SR_KEMI_TRUE;
	}
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_resetdsturi(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	reset_dst_uri(msg);
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_isdsturiset(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if(msg->dst_uri.s!=NULL && msg->dst_uri.len>0) {
		return SR_KEMI_TRUE;
	}
	return SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_force_rport(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	msg->msg_flags|=FL_FORCE_RPORT;
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_match_method_id(str *rmethod, str *vmethod, int mid)
{
	char mbuf[SR_KEMI_HNAME_SIZE];
	int i;
	unsigned int method;
	str s;

	if(memchr(vmethod->s, '|', vmethod->len)==NULL) {
		if(rmethod->len!=vmethod->len) {
			return SR_KEMI_FALSE;
		}
		if(strncasecmp(rmethod->s, vmethod->s, vmethod->len)!=0) {
			return SR_KEMI_FALSE;
		}
		return SR_KEMI_TRUE;
	}
	if(vmethod->len>=SR_KEMI_HNAME_SIZE-1) {
		LM_ERR("methods parameter is too long\n");
		return SR_KEMI_FALSE;
	}
	memcpy(mbuf, vmethod->s, vmethod->len);
	mbuf[vmethod->len] = '\0';
	for(i=0; i<vmethod->len; i++) {
		if(mbuf[i]=='|') {
			mbuf[i] = ',';
		}
	}
	s.s = mbuf;
	s.len = vmethod->len;
	if(parse_methods(&s, &method)!=0) {
		LM_ERR("failed to parse methods string [%.*s]\n", s.len, s.s);
		return SR_KEMI_FALSE;
	}
	if((method==METHOD_UNDEF) || (method&METHOD_OTHER)) {
		LM_ERR("unknown method in list [%.*s] - use only standard SIP methods\n",
				s.len, s.s);
		return SR_KEMI_FALSE;
	}
	if((int)method & mid) {
		return SR_KEMI_TRUE;
	}
	return SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_method(sip_msg_t *msg, str *vmethod)
{
	if(msg==NULL || vmethod==NULL || vmethod->s==NULL || vmethod->len<=0) {
		LM_WARN("invalid parameters\n");
		return SR_KEMI_FALSE;
	}

	if(msg->first_line.type==SIP_REQUEST) {
		if(msg->first_line.u.request.method_value==METHOD_OTHER) {
			if(msg->first_line.u.request.method.len!=vmethod->len) {
				return SR_KEMI_FALSE;
			}
			if(strncasecmp(msg->first_line.u.request.method.s, vmethod->s,
						vmethod->len)!=0) {
				return SR_KEMI_FALSE;
			}
			return SR_KEMI_TRUE;
		}
		return sr_kemi_core_match_method_id(&msg->first_line.u.request.method,
				vmethod, msg->first_line.u.request.method_value);
	}

	if(parse_headers(msg, HDR_CSEQ_F, 0)!=0 || msg->cseq==NULL) {
		LM_ERR("cannot parse cseq header\n");
		return SR_KEMI_FALSE;
	}
	if(get_cseq(msg)->method_id==METHOD_OTHER) {
		if(get_cseq(msg)->method.len!=vmethod->len) {
			return SR_KEMI_FALSE;
		}
		if(strncasecmp(get_cseq(msg)->method.s, vmethod->s,
					vmethod->len)!=0) {
			return SR_KEMI_FALSE;
		}
		return SR_KEMI_TRUE;
	}
	return sr_kemi_core_match_method_id(&get_cseq(msg)->method, vmethod,
			get_cseq(msg)->method_id);
}

/**
 *
 */
static int sr_kemi_core_forward_uri(sip_msg_t *msg, str *vuri)
{
	int ret;
	dest_info_t dst;
	sip_uri_t *u;
	sip_uri_t next_hop;

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return -1;
	}

	init_dest_info(&dst);

	if(vuri==NULL || vuri->s==NULL || vuri->len<=0) {
		if (msg->dst_uri.len) {
			ret = parse_uri(msg->dst_uri.s, msg->dst_uri.len, &next_hop);
			u = &next_hop;
		} else {
			ret = parse_sip_msg_uri(msg);
			u = &msg->parsed_uri;
		}
	} else {
		ret = parse_uri(vuri->s, vuri->len, &next_hop);
		u = &next_hop;
	}

	if (ret<0) {
		LM_ERR("forward - bad_uri dropping packet\n");
		return -1;
	}

	dst.proto=u->proto;
	ret=forward_request(msg, &u->host, u->port_no, &dst);
	if (ret>=0) {
		return 1;
	}

	return -1;
}

/**
 *
 */
static int sr_kemi_core_forward(sip_msg_t *msg)
{
	return sr_kemi_core_forward_uri(msg, NULL);
}

/**
 *
 */
static int sr_kemi_core_set_forward_close(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	msg->fwd_send_flags.f |= SND_F_CON_CLOSE;
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_set_forward_no_connect(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	msg->fwd_send_flags.f |= SND_F_FORCE_CON_REUSE;
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_set_reply_close(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	msg->rpl_send_flags.f |= SND_F_CON_CLOSE;
	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_set_reply_no_connect(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	msg->rpl_send_flags.f |= SND_F_FORCE_CON_REUSE;
	return SR_KEMI_TRUE;
}

/**
 *
 */
static sr_kemi_t _sr_kemi_core[] = {
	{ str_init(""), str_init("dbg"),
		SR_KEMIP_NONE, sr_kemi_core_dbg,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("err"),
		SR_KEMIP_NONE, sr_kemi_core_err,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("info"),
		SR_KEMIP_NONE, sr_kemi_core_info,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("warn"),
		SR_KEMIP_NONE, sr_kemi_core_warn,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("notice"),
		SR_KEMIP_NONE, sr_kemi_core_notice,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("crit"),
		SR_KEMIP_NONE, sr_kemi_core_crit,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("log"),
		SR_KEMIP_NONE, sr_kemi_core_log,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("drop"),
		SR_KEMIP_NONE, sr_kemi_core_drop,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_myself"),
		SR_KEMIP_BOOL, sr_kemi_core_is_myself,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("setflag"),
		SR_KEMIP_BOOL, sr_kemi_core_setflag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("resetflag"),
		SR_KEMIP_BOOL, sr_kemi_core_resetflag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("isflagset"),
		SR_KEMIP_BOOL, sr_kemi_core_isflagset,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("setbflag"),
		SR_KEMIP_BOOL, sr_kemi_core_setbflag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("resetbflag"),
		SR_KEMIP_BOOL, sr_kemi_core_resetbflag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("isbflagset"),
		SR_KEMIP_BOOL, sr_kemi_core_isbflagset,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("setbiflag"),
		SR_KEMIP_BOOL, sr_kemi_core_setbiflag,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("resetbiflag"),
		SR_KEMIP_BOOL, sr_kemi_core_resetbiflag,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("isbiflagset"),
		SR_KEMIP_BOOL, sr_kemi_core_isbiflagset,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("setsflag"),
		SR_KEMIP_BOOL, sr_kemi_core_setsflag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("resetsflag"),
		SR_KEMIP_BOOL, sr_kemi_core_resetsflag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("issflagset"),
		SR_KEMIP_BOOL, sr_kemi_core_issflagset,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("seturi"),
		SR_KEMIP_BOOL, sr_kemi_core_seturi,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("setuser"),
		SR_KEMIP_BOOL, sr_kemi_core_setuser,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("sethost"),
		SR_KEMIP_BOOL, sr_kemi_core_sethost,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("setdsturi"),
		SR_KEMIP_BOOL, sr_kemi_core_setdsturi,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("resetdsturi"),
		SR_KEMIP_BOOL, sr_kemi_core_resetdsturi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("isdsturiset"),
		SR_KEMIP_BOOL, sr_kemi_core_isdsturiset,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("force_rport"),
		SR_KEMIP_BOOL, sr_kemi_core_force_rport,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_method"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("forward"),
		SR_KEMIP_INT, sr_kemi_core_forward,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("forward_uri"),
		SR_KEMIP_INT, sr_kemi_core_forward_uri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("set_forward_close"),
		SR_KEMIP_BOOL, sr_kemi_core_set_forward_close,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("set_forward_no_connect"),
		SR_KEMIP_BOOL, sr_kemi_core_set_forward_no_connect,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("set_reply_close"),
		SR_KEMIP_BOOL, sr_kemi_core_set_reply_close,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("set_reply_no_connect"),
		SR_KEMIP_BOOL, sr_kemi_core_set_reply_no_connect,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
static int sr_kemi_hdr_append(sip_msg_t *msg, str *txt)
{
	struct lump* anchor;
	char *hdr;

	if(txt==NULL || txt->s==NULL || msg==NULL)
		return -1;

	LM_DBG("append hf: %.*s\n", txt->len, txt->s);
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return -1;
	}

	hdr = (char*)pkg_malloc(txt->len);
	if(hdr==NULL) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);
	/* anchor after last header */
	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, txt->len, 0) == 0) {
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_append_after(sip_msg_t *msg, str *txt, str *hname)
{
	struct lump* anchor;
	hdr_field_t *hf;
	hdr_field_t hfm;
	char *hdr;
	char hbuf[SR_KEMI_HNAME_SIZE];

	if(txt==NULL || txt->s==NULL || hname==NULL || hname->s==NULL || msg==NULL)
		return -1;

	if(hname->len>SR_KEMI_HNAME_SIZE-4) {
		LM_ERR("header name too long: %d\n", hname->len);
		return -1;
	}
	memcpy(hbuf, hname->s, hname->len);
	hbuf[hname->len] = ':';
	hbuf[hname->len+1] = '\0';

	if (parse_hname2_short(hbuf, hbuf+hname->len+1, &hfm)==0) {
		LM_ERR("error parsing header name [%.*s]\n", hname->len, hname->s);
		return -1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return -1;
	}
	for (hf=msg->headers; hf; hf=hf->next) {
		if (hfm.type!=HDR_OTHER_T && hfm.type!=HDR_ERROR_T) {
			if (hfm.type!=hf->type)
				continue;
		} else {
			if (hf->name.len!=hfm.name.len)
				continue;
			if (cmp_hdrname_str(&hf->name, &hfm.name)!=0)
				continue;
		}
		break;
	}

	hdr = (char*)pkg_malloc(txt->len);
	if(hdr==NULL) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);

	if(hf==0) { /* after last header */
		anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	} else { /* after hf */
		anchor = anchor_lump(msg, hf->name.s + hf->len - msg->buf, 0, 0);
	}

	LM_DBG("append after [%.*s] the hf: [%.*s]\n", hname->len, hname->s,
			txt->len, txt->s);
	if(insert_new_lump_before(anchor, hdr, txt->len, 0) == 0) {
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_remove(sip_msg_t *msg, str *hname)
{
	struct lump* anchor;
	hdr_field_t *hf;
	hdr_field_t hfm;
	char hbuf[SR_KEMI_HNAME_SIZE];

	if(hname==NULL || hname->s==NULL || msg==NULL)
		return -1;

	if(hname->len>SR_KEMI_HNAME_SIZE-4) {
		LM_ERR("header name too long: %d\n", hname->len);
		return -1;
	}
	memcpy(hbuf, hname->s, hname->len);
	hbuf[hname->len] = ':';
	hbuf[hname->len+1] = '\0';

	if (parse_hname2_short(hbuf, hbuf+hname->len+1, &hfm)==0) {
		LM_ERR("error parsing header name [%.*s]\n", hname->len, hname->s);
		return -1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return -1;
	}

	LM_DBG("remove hf: %.*s\n", hname->len, hname->s);
	for (hf=msg->headers; hf; hf=hf->next) {
		if (hfm.type!=HDR_OTHER_T && hfm.type!=HDR_ERROR_T) {
			if (hfm.type!=hf->type)
				continue;
		} else {
			if (hf->name.len!=hname->len)
				continue;
			if(strncasecmp(hf->name.s, hname->s, hname->len)!=0)
				continue;
		}
		anchor=del_lump(msg, hf->name.s - msg->buf, hf->len, 0);
		if (anchor==0) {
			LM_ERR("cannot remove hdr %.*s\n", hname->len, hname->s);
			return -1;
		}
	}
	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_is_present(sip_msg_t *msg, str *hname)
{
	hdr_field_t *hf;
	hdr_field_t hfm;
	char hbuf[SR_KEMI_HNAME_SIZE];

	if(hname==NULL || hname->s==NULL || msg==NULL)
		return -1;

	if(hname->len>SR_KEMI_HNAME_SIZE-4) {
		LM_ERR("header name too long: %d\n", hname->len);
		return -1;
	}
	memcpy(hbuf, hname->s, hname->len);
	hbuf[hname->len] = ':';
	hbuf[hname->len+1] = '\0';

	if (parse_hname2_short(hbuf, hbuf+hname->len+1, &hfm)==0) {
		LM_ERR("error parsing header name [%.*s]\n", hname->len, hname->s);
		return -1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return -1;
	}

	LM_DBG("searching hf: %.*s\n", hname->len, hname->s);
	for (hf=msg->headers; hf; hf=hf->next) {
		if (hfm.type!=HDR_OTHER_T && hfm.type!=HDR_ERROR_T) {
			if (hfm.type!=hf->type)
				continue;
		} else {
			if (hf->name.len!=hname->len)
				continue;
			if(strncasecmp(hf->name.s, hname->s, hname->len)!=0)
				continue;
		}
		return 1;
	}
	return -1;
}

/**
 *
 */
static int sr_kemi_hdr_insert(sip_msg_t *msg, str *txt)
{
	struct lump* anchor;
	char *hdr;

	if(txt==NULL || txt->s==NULL || msg==NULL)
		return -1;

	LM_DBG("insert hf: %.*s\n", txt->len, txt->s);
	hdr = (char*)pkg_malloc(txt->len);
	if(hdr==NULL) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);
	/* anchor before first header */
	anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, txt->len, 0) == 0) {
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_insert_before(sip_msg_t *msg, str *txt, str *hname)
{
	struct lump* anchor;
	hdr_field_t *hf;
	hdr_field_t hfm;
	char *hdr;
	char hbuf[SR_KEMI_HNAME_SIZE];

	if(txt==NULL || txt->s==NULL || hname==NULL || hname->s==NULL || msg==NULL)
		return -1;

	if(hname->len>SR_KEMI_HNAME_SIZE-4) {
		LM_ERR("header name too long: %d\n", hname->len);
		return -1;
	}
	memcpy(hbuf, hname->s, hname->len);
	hbuf[hname->len] = ':';
	hbuf[hname->len+1] = '\0';

	if (parse_hname2_short(hbuf, hbuf+hname->len+1, &hfm)==0) {
		LM_ERR("error parsing header name [%.*s]\n", hname->len, hname->s);
		return -1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return -1;
	}
	for (hf=msg->headers; hf; hf=hf->next) {
		if (hfm.type!=HDR_OTHER_T && hfm.type!=HDR_ERROR_T) {
			if (hfm.type!=hf->type)
				continue;
		} else {
			if (hf->name.len!=hfm.name.len)
				continue;
			if (cmp_hdrname_str(&hf->name, &hfm.name)!=0)
				continue;
		}
		break;
	}

	hdr = (char*)pkg_malloc(txt->len);
	if(hdr==NULL) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);
	if(hf==0) { /* before first header */
		anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, 0);
	} else { /* before hf */
		anchor = anchor_lump(msg, hf->name.s - msg->buf, 0, 0);
	}
	LM_DBG("insert before [%.*s] the hf: %.*s\n", hname->len, hname->s,
			txt->len, txt->s);
	if(insert_new_lump_before(anchor, hdr, txt->len, 0) == 0) {
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_append_to_reply(sip_msg_t *msg, str *txt)
{
	if(txt==NULL || txt->s==NULL || msg==NULL)
		return -1;

	LM_DBG("append to reply: %.*s\n", txt->len, txt->s);

	if(add_lump_rpl(msg, txt->s, txt->len, LUMP_RPL_HDR)==0) {
		LM_ERR("unable to add reply lump\n");
		return -1;
	}

	return 1;
}

/**
 *
 */
static sr_kemi_t _sr_kemi_hdr[] = {
	{ str_init("hdr"), str_init("append"),
		SR_KEMIP_INT, sr_kemi_hdr_append,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("append_after"),
		SR_KEMIP_INT, sr_kemi_hdr_append_after,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("insert"),
		SR_KEMIP_INT, sr_kemi_hdr_insert,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("insert_before"),
		SR_KEMIP_INT, sr_kemi_hdr_insert_before,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("remove"),
		SR_KEMIP_INT, sr_kemi_hdr_remove,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("is_present"),
		SR_KEMIP_INT, sr_kemi_hdr_is_present,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("append_to_reply"),
		SR_KEMIP_INT, sr_kemi_hdr_append_to_reply,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

#define SR_KEMI_MODULES_MAX_SIZE	1024
static int _sr_kemi_modules_size = 0;
static sr_kemi_module_t _sr_kemi_modules[SR_KEMI_MODULES_MAX_SIZE];

/**
 *
 */
int sr_kemi_modules_add(sr_kemi_t *klist)
{
	if(_sr_kemi_modules_size>=SR_KEMI_MODULES_MAX_SIZE) {
		LM_ERR("exceeded max number of modules\n");
		return -1;
	}
	if(_sr_kemi_modules_size==0) {
		LM_DBG("adding core module\n");
		_sr_kemi_modules[_sr_kemi_modules_size].mname = _sr_kemi_core[0].mname;
		_sr_kemi_modules[_sr_kemi_modules_size].kexp = _sr_kemi_core;
		_sr_kemi_modules_size++;
		LM_DBG("adding hdr module\n");
		_sr_kemi_modules[_sr_kemi_modules_size].mname = _sr_kemi_hdr[0].mname;
		_sr_kemi_modules[_sr_kemi_modules_size].kexp = _sr_kemi_hdr;
		_sr_kemi_modules_size++;
	}
	if((_sr_kemi_modules_size>1)
			&& (_sr_kemi_modules[_sr_kemi_modules_size-1].mname.len
					==klist[0].mname.len)
			&& (strncmp(_sr_kemi_modules[_sr_kemi_modules_size-1].mname.s,
					klist[0].mname.s, klist[0].mname.len)==0)) {
		/* handle re-open of the module */
		LM_DBG("updating module: %.*s\n", klist[0].mname.len, klist[0].mname.s);
		_sr_kemi_modules[_sr_kemi_modules_size-1].kexp = klist;
	} else {
		LM_DBG("adding module: %.*s\n", klist[0].mname.len, klist[0].mname.s);
		_sr_kemi_modules[_sr_kemi_modules_size].mname = klist[0].mname;
		_sr_kemi_modules[_sr_kemi_modules_size].kexp = klist;
		_sr_kemi_modules_size++;
	}
	return 0;
}

/**
 *
 */
int sr_kemi_modules_size_get(void)
{
	return _sr_kemi_modules_size;
}

/**
 *
 */
sr_kemi_module_t* sr_kemi_modules_get(void)
{
	return _sr_kemi_modules;
}

/**
 *
 */
sr_kemi_t* sr_kemi_lookup(str *mname, int midx, str *fname)
{
	int i;
	sr_kemi_t *ket;

	if(mname==NULL || mname->len<=0) {
		for(i=0; _sr_kemi_core[i].fname.s!=NULL; i++) {
			ket = &_sr_kemi_core[i];
			if(ket->fname.len==fname->len
					&& strncasecmp(ket->fname.s, fname->s, fname->len)==0) {
				return ket;
			}
		}
	} else {
		if(midx>0 && midx<SR_KEMI_MODULES_MAX_SIZE) {
			for(i=0; _sr_kemi_modules[midx].kexp[i].fname.s!=NULL; i++) {
				ket = &_sr_kemi_modules[midx].kexp[i];
				if(ket->fname.len==fname->len
						&& strncasecmp(ket->fname.s, fname->s, fname->len)==0) {
					return ket;
				}
			}
		}
	}
	return NULL;
}

/**
 *
 */

#define SR_KEMI_ENG_LIST_MAX_SIZE	8
static sr_kemi_eng_t _sr_kemi_eng_list[SR_KEMI_ENG_LIST_MAX_SIZE];
sr_kemi_eng_t *_sr_kemi_eng = NULL;
static int _sr_kemi_eng_list_size=0;

/**
 *
 */
int sr_kemi_eng_register(str *ename, sr_kemi_eng_route_f froute)
{
	int i;

	for(i=0; i<_sr_kemi_eng_list_size; i++) {
		if(_sr_kemi_eng_list[i].ename.len==ename->len
				&& strncasecmp(_sr_kemi_eng_list[i].ename.s, ename->s,
					ename->len)==0) {
			/* found */
			return 1;
		}
	}
	if(_sr_kemi_eng_list_size>=SR_KEMI_ENG_LIST_MAX_SIZE) {
		LM_ERR("too many config routing engines registered\n");
		return -1;
	}
	if(ename->len>=SR_KEMI_BNAME_SIZE) {
		LM_ERR("config routing engine name too long\n");
		return -1;
	}
	strncpy(_sr_kemi_eng_list[_sr_kemi_eng_list_size].bname,
			ename->s, ename->len);
	_sr_kemi_eng_list[_sr_kemi_eng_list_size].ename.s
			= _sr_kemi_eng_list[_sr_kemi_eng_list_size].bname;
	_sr_kemi_eng_list[_sr_kemi_eng_list_size].ename.len = ename->len;
	_sr_kemi_eng_list[_sr_kemi_eng_list_size].ename.s[ename->len] = 0;
	_sr_kemi_eng_list[_sr_kemi_eng_list_size].froute = froute;
	_sr_kemi_eng_list_size++;

	LM_DBG("registered config routing enginge [%.*s]\n",
			ename->len, ename->s);

	return 0;
}

/**
 *
 */
int sr_kemi_eng_set(str *ename, str *cpath)
{
	int i;

	/* skip native and default */
	if(ename->len==6 && strncasecmp(ename->s, "native", 6)==0) {
		return 0;
	}
	if(ename->len==7 && strncasecmp(ename->s, "default", 7)==0) {
		return 0;
	}

	if(sr_kemi_cbname_list_init()<0) {
		return -1;
	}

	for(i=0; i<_sr_kemi_eng_list_size; i++) {
		if(_sr_kemi_eng_list[i].ename.len==ename->len
				&& strncasecmp(_sr_kemi_eng_list[i].ename.s, ename->s,
					ename->len)==0) {
			/* found */
			_sr_kemi_eng = &_sr_kemi_eng_list[i];
			return 0;
		}
	}
	return -1;
}

/**
 *
 */
int sr_kemi_eng_setz(char *ename, char *cpath)
{
	str sname;
	str spath;

	sname.s = ename;
	sname.len = strlen(ename);

	if(cpath!=0) {
		spath.s = cpath;
		spath.len = strlen(cpath);
		return sr_kemi_eng_set(&sname, &spath);
	} else {
		return sr_kemi_eng_set(&sname, NULL);
	}
}

/**
 *
 */
sr_kemi_eng_t* sr_kemi_eng_get(void)
{
	return _sr_kemi_eng;
}

/**
 *
 */
#define KEMI_CBNAME_MAX_LEN	128
#define KEMI_CBNAME_LIST_SIZE	256

typedef struct sr_kemi_cbname {
	str name;
	char bname[KEMI_CBNAME_MAX_LEN];
} sr_kemi_cbname_t;

static gen_lock_t *_sr_kemi_cbname_lock = 0;
static sr_kemi_cbname_t *_sr_kemi_cbname_list = NULL;
static int *_sr_kemi_cbname_list_size = NULL;

/**
 *
 */
int sr_kemi_cbname_list_init(void)
{
	if(_sr_kemi_cbname_list!=NULL) {
		return 0;
	}
	if ( (_sr_kemi_cbname_lock=lock_alloc())==0) {
		LM_CRIT("failed to alloc lock\n");
		return -1;
	}
	if (lock_init(_sr_kemi_cbname_lock)==0 ) {
		LM_CRIT("failed to init lock\n");
		lock_dealloc(_sr_kemi_cbname_lock);
		_sr_kemi_cbname_lock = NULL;
		return -1;
	}
	_sr_kemi_cbname_list_size = shm_malloc(sizeof(int));
	if(_sr_kemi_cbname_list_size==NULL) {
		lock_destroy(_sr_kemi_cbname_lock);
		lock_dealloc(_sr_kemi_cbname_lock);
		LM_ERR("no more shared memory\n");
		return -1;
	}
	*_sr_kemi_cbname_list_size = 0;
	_sr_kemi_cbname_list
			= shm_malloc(KEMI_CBNAME_LIST_SIZE*sizeof(sr_kemi_cbname_t));
	if(_sr_kemi_cbname_list==NULL) {
		LM_ERR("no more shared memory\n");
		shm_free(_sr_kemi_cbname_list_size);
		_sr_kemi_cbname_list_size = NULL;
		lock_destroy(_sr_kemi_cbname_lock);
		lock_dealloc(_sr_kemi_cbname_lock);
		_sr_kemi_cbname_lock = NULL;
		return -1;
	}
	memset(_sr_kemi_cbname_list, 0,
			KEMI_CBNAME_LIST_SIZE*sizeof(sr_kemi_cbname_t));
	return 0;
}

/**
 *
 */
int sr_kemi_cbname_lookup_name(str *name)
{
	int n;
	int i;

	if(_sr_kemi_cbname_list==NULL) {
		return 0;
	}
	if(name->len >= KEMI_CBNAME_MAX_LEN) {
		LM_ERR("callback name is too long [%.*s] (max: %d)\n",
				name->len, name->s, KEMI_CBNAME_MAX_LEN);
		return 0;
	}
	n = *_sr_kemi_cbname_list_size;

	for(i=0; i<n; i++) {
		if(_sr_kemi_cbname_list[i].name.len==name->len
				&& strncmp(_sr_kemi_cbname_list[i].name.s,
						name->s, name->len)==0) {
			return i+1;
		}
	}

	/* not found -- add it */
	lock_get(_sr_kemi_cbname_lock);

	/* check if new callback were indexed meanwhile */
	for(; i<*_sr_kemi_cbname_list_size; i++) {
		if(_sr_kemi_cbname_list[i].name.len==name->len
				&& strncmp(_sr_kemi_cbname_list[i].name.s,
						name->s, name->len)==0) {
			lock_release(_sr_kemi_cbname_lock);
			return i+1;
		}
	}
	if(*_sr_kemi_cbname_list_size>=KEMI_CBNAME_LIST_SIZE) {
		lock_release(_sr_kemi_cbname_lock);
		LM_ERR("no more space to index callbacks\n");
		return 0;
	}
	strncpy(_sr_kemi_cbname_list[i].bname, name->s, name->len);
	_sr_kemi_cbname_list[i].bname[name->len] = '\0';
	_sr_kemi_cbname_list[i].name.s = _sr_kemi_cbname_list[i].bname;
	_sr_kemi_cbname_list[i].name.len = name->len;
	i++;
	*_sr_kemi_cbname_list_size = i;
	lock_release(_sr_kemi_cbname_lock);
	return i;
}

/**
 *
 */
str* sr_kemi_cbname_lookup_idx(int idx)
{
	int n;

	if(_sr_kemi_cbname_list==NULL) {
		return NULL;
	}
	n = *_sr_kemi_cbname_list_size;
	if(idx<1 || idx>n) {
		LM_ERR("index %d is out of range\n", idx);
		return NULL;
	}
	return &_sr_kemi_cbname_list[idx-1].name;
}

/**
 *
 */
typedef struct sr_kemi_param_map {
	int ptype;
	str pname;
} sr_kemi_param_map_t;

/**
 *
 */
static sr_kemi_param_map_t _sr_kemi_param_map[] = {
	{ SR_KEMIP_NONE,   str_init("none") },
	{ SR_KEMIP_INT,    str_init("int") },
	{ SR_KEMIP_STR,    str_init("str") },
	{ SR_KEMIP_BOOL,   str_init("bool") },
	{ SR_KEMIP_INTSTR, str_init("int-str") },
	{ 0, STR_NULL }
};

/**
 *
 */
str *sr_kemi_param_map_get_name(int ptype)
{
	int i;

	for(i=0; _sr_kemi_param_map[i].pname.s!=NULL; i++) {
		if(_sr_kemi_param_map[i].ptype==ptype)
			return &_sr_kemi_param_map[i].pname;
	}
	return NULL;
}

/**
 *
 */
str *sr_kemi_param_map_get_params(int *ptypes)
{
	int i;
	int l;
#define KEMI_PARAM_MAP_SIZE 72
	static char pbuf[KEMI_PARAM_MAP_SIZE];
	static str sret = STR_NULL;
	str *pn;

	pbuf[0] = '\0';
	l = 0;
	for(i = 0; i < SR_KEMI_PARAMS_MAX; i++) {
		if(ptypes[i] == SR_KEMIP_NONE)
			break;
		if(i > 0) {
			l += 2;
			if(l >= KEMI_PARAM_MAP_SIZE - 8) {
				strcat(pbuf, ", ...");
				goto done;
			}
			strcat(pbuf, ", ");
		}
		pn = sr_kemi_param_map_get_name(ptypes[i]);
		if(pn == NULL)
			return NULL;
		l += pn->len;
		if(l >= KEMI_PARAM_MAP_SIZE - 8) {
			strcat(pbuf, ", ...");
			goto done;
		}
		strcat(pbuf, pn->s);
	}
	if(pbuf[0]=='\0') {
		pn = sr_kemi_param_map_get_name(SR_KEMIP_NONE);
		if(pn == NULL)
			return NULL;
		if(pn->len<KEMI_PARAM_MAP_SIZE-1) strncat(pbuf, pn->s, pn->len);
	}
done:
	sret.s = pbuf;
	sret.len = strlen(sret.s);
	return &sret;
}
