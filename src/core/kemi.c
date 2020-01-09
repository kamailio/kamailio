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
#include "select_buf.h"
#include "pvar.h"
#include "mem/shm.h"
#include "parser/parse_uri.h"
#include "parser/parse_from.h"
#include "parser/parse_hname2.h"
#include "parser/parse_methods.h"

#include "kemi.h"


#define SR_KEMI_HNAME_SIZE 128

/* names for kemi callback functions */
str kemi_onsend_route_callback = str_init("ksr_onsend_route");
str kemi_reply_route_callback = str_init("ksr_reply_route");
str kemi_event_route_callback = str_init("");

/**
 *
 */
static sr_kemi_xval_t _sr_kemi_xval = {0};

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
			} else if(strcasecmp(level->s, "notice")==0) {
				LM_NOTICE("%s", txt->s);
			} else if(strcasecmp(level->s, "warn")==0) {
				LM_WARN("%s", txt->s);
			} else if(strcasecmp(level->s, "err")==0) {
				LM_ERR("%s", txt->s);
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
int sr_kemi_core_set_drop(sip_msg_t *msg)
{
	if(_sr_kemi_act_ctx==NULL)
		return 0;
	LM_DBG("set drop action executed inside embedded interpreter\n");
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
static int sr_kemi_core_is_myself_ruri(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesn't have a ruri */
		return SR_KEMI_FALSE;

	if (msg->new_uri.s!=NULL)
		return sr_kemi_core_is_myself(msg, &msg->new_uri);
	return sr_kemi_core_is_myself(msg, &msg->first_line.u.request.uri);
}

/**
 *
 */
static int sr_kemi_core_is_myself_furi(sip_msg_t *msg)
{
	to_body_t *xfrom;

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if(parse_from_header(msg)<0) {
		LM_ERR("cannot parse From header\n");
		return SR_KEMI_FALSE;
	}

	if(msg->from==NULL || get_from(msg)==NULL) {
		LM_DBG("no From header\n");
		return SR_KEMI_FALSE;
	}

	xfrom = get_from(msg);

	return sr_kemi_core_is_myself(msg, &xfrom->uri);
}

/**
 *
 */
static int sr_kemi_core_is_myself_turi(sip_msg_t *msg)
{
	to_body_t *xto;

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	if(parse_to_header(msg)<0) {
		LM_ERR("cannot parse To header\n");
		return SR_KEMI_FALSE;
	}

	if(msg->to==NULL || get_to(msg)==NULL) {
		LM_DBG("no To header\n");
		return SR_KEMI_FALSE;
	}

	xto = get_to(msg);

	return sr_kemi_core_is_myself(msg, &xto->uri);
}

/**
 *
 */
static int sr_kemi_core_is_myself_suri(sip_msg_t *msg)
{
	str suri;

	if(get_src_uri(msg, 0, &suri)<0) {
		LM_ERR("cannot src address uri\n");
		return SR_KEMI_FALSE;
	}

	return sr_kemi_core_is_myself(msg, &suri);
}

/**
 *
 */
static int sr_kemi_core_is_myself_srcip(sip_msg_t *msg)
{
	str srcip;
	int ret;

	srcip.s = ip_addr2a(&msg->rcv.src_ip);
	srcip.len = strlen(srcip.s);

	ret = check_self(&srcip, 0, 0);
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
static int sr_kemi_core_add_local_rport(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	msg->msg_flags|=FL_ADD_LOCAL_RPORT;
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
static int sr_kemi_core_is_method_in(sip_msg_t *msg, str *vmethod)
{
	int imethod;
	int i;

	if(msg==NULL || vmethod==NULL || vmethod->s==NULL || vmethod->len<=0) {
		LM_WARN("invalid parameters\n");
		return SR_KEMI_FALSE;
	}

	if(msg->first_line.type==SIP_REQUEST) {
		imethod = msg->first_line.u.request.method_value;
	} else {
		if(parse_headers(msg, HDR_CSEQ_F, 0)!=0 || msg->cseq==NULL) {
			LM_ERR("cannot parse cseq header\n");
			return SR_KEMI_FALSE;
		}
		imethod = get_cseq(msg)->method_id;
	}

	if(imethod==METHOD_OTHER) {
		return SR_KEMI_FALSE;
	}

	for(i=0; i<vmethod->len; i++) {
		switch(vmethod->s[i]) {
			case 'I':
			case 'i':
				if(imethod==METHOD_INVITE) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'A':
			case 'a':
				if(imethod==METHOD_ACK) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'B':
			case 'b':
				if(imethod==METHOD_BYE) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'C':
			case 'c':
				if(imethod==METHOD_CANCEL) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'M':
			case 'm':
				if(imethod==METHOD_MESSAGE) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'R':
			case 'r':
				if(imethod==METHOD_REGISTER) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'P':
			case 'p':
				if(imethod==METHOD_PUBLISH) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'S':
			case 's':
				if(imethod==METHOD_SUBSCRIBE) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'N':
			case 'n':
				if(imethod==METHOD_NOTIFY) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'O':
			case 'o':
				if(imethod==METHOD_OPTIONS) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'F':
			case 'f':
				if(imethod==METHOD_REFER) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'G':
			case 'g':
				if(imethod==METHOD_GET) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'U':
			case 'u':
				if(imethod==METHOD_UPDATE) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'K':
			case 'k':
				if(imethod==METHOD_KDMQ) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'D':
			case 'd':
				if(imethod==METHOD_DELETE) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'T':
			case 't':
				if(imethod==METHOD_POST) {
					return SR_KEMI_TRUE;
				}
			break;
			case 'V':
			case 'v':
				if(imethod==METHOD_PUT) {
					return SR_KEMI_TRUE;
				}
			break;
			default:
				LM_WARN("unsupported method flag: %c\n", vmethod->s[i]);
		}
	}
	return SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_method_type(sip_msg_t *msg, int mtype)
{
	int imethod;

	if(msg==NULL) {
		LM_WARN("invalid parameters\n");
		return SR_KEMI_FALSE;
	}

	if(msg->first_line.type==SIP_REQUEST) {
		imethod = msg->first_line.u.request.method_value;
	} else {
		if(parse_headers(msg, HDR_CSEQ_F, 0)!=0 || msg->cseq==NULL) {
			LM_ERR("cannot parse cseq header\n");
			return SR_KEMI_FALSE;
		}
		imethod = get_cseq(msg)->method_id;
	}

	if(imethod==mtype) {
		return SR_KEMI_TRUE;
	}

	return SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_method_invite(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_INVITE);
}

/**
 *
 */
static int sr_kemi_core_is_method_ack(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_ACK);
}

/**
 *
 */
static int sr_kemi_core_is_method_bye(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_BYE);
}

/**
 *
 */
static int sr_kemi_core_is_method_cancel(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_CANCEL);
}

/**
 *
 */
static int sr_kemi_core_is_method_register(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_REGISTER);
}

/**
 *
 */
static int sr_kemi_core_is_method_options(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_OPTIONS);
}

/**
 *
 */
static int sr_kemi_core_is_method_update(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_UPDATE);
}

/**
 *
 */
static int sr_kemi_core_is_method_subscribe(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_SUBSCRIBE);
}

/**
 *
 */
static int sr_kemi_core_is_method_publish(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_PUBLISH);
}

/**
 *
 */
static int sr_kemi_core_is_method_notify(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_NOTIFY);
}

/**
 *
 */
static int sr_kemi_core_is_method_refer(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_REFER);
}

/**
 *
 */
static int sr_kemi_core_is_method_info(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_INFO);
}

/**
 *
 */
static int sr_kemi_core_is_method_prack(sip_msg_t *msg)
{
	return sr_kemi_core_is_method_type(msg, METHOD_PRACK);
}

/**
 *
 */
static int sr_kemi_core_is_proto_udp(sip_msg_t *msg)
{
	return (msg->rcv.proto == PROTO_UDP)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_proto_tcp(sip_msg_t *msg)
{
	return (msg->rcv.proto == PROTO_TCP)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_proto_tls(sip_msg_t *msg)
{
	return (msg->rcv.proto == PROTO_TLS)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_proto_ws(sip_msg_t *msg)
{
	return (msg->rcv.proto == PROTO_WS)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_proto_wss(sip_msg_t *msg)
{
	return (msg->rcv.proto == PROTO_WSS)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_proto_sctp(sip_msg_t *msg)
{
	return (msg->rcv.proto == PROTO_SCTP)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_af_ipv4(sip_msg_t *msg)
{
	if(msg==NULL || msg->rcv.bind_address==NULL) {
		return SR_KEMI_FALSE;
	}
	return (msg->rcv.bind_address->address.af==AF_INET)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_af_ipv6(sip_msg_t *msg)
{
	if(msg==NULL || msg->rcv.bind_address==NULL) {
		return SR_KEMI_FALSE;
	}
	return (msg->rcv.bind_address->address.af==AF_INET6)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_src_port(sip_msg_t *msg, int vport)
{
	return (vport == (int)msg->rcv.src_port)?SR_KEMI_TRUE:SR_KEMI_FALSE;
}

/**
 *
 */
static int sr_kemi_core_is_dst_port(sip_msg_t *msg, int vport)
{
	if(msg==NULL || msg->rcv.bind_address==NULL) {
		return SR_KEMI_FALSE;
	}
	return (vport == (int)msg->rcv.bind_address->port_no)?SR_KEMI_TRUE:SR_KEMI_FALSE;
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
static int sr_kemi_core_set_advertised_address(sip_msg_t *msg, str *addr)
{
#define SR_ADV_ADDR_SIZE 128
	static char _sr_adv_addr_buf[SR_ADV_ADDR_SIZE];

	if(addr==NULL || addr->s==NULL) {
		LM_ERR("invalid addr parameter\n");
		return SR_KEMI_FALSE;
	}

	if(addr->len>=SR_ADV_ADDR_SIZE) {
		LM_ERR("addr parameter is too large\n");
		return SR_KEMI_FALSE;
	}

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	memcpy(_sr_adv_addr_buf, addr->s, addr->len);
	_sr_adv_addr_buf[addr->len] = '\0';
	msg->set_global_address.s = _sr_adv_addr_buf;
	msg->set_global_address.len = addr->len;

	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_set_advertised_port(sip_msg_t *msg, str *port)
{
#define SR_ADV_PORT_SIZE 8
	static char _sr_adv_port_buf[SR_ADV_PORT_SIZE];

	if(port==NULL || port->s==NULL) {
		LM_ERR("invalid port parameter\n");
		return SR_KEMI_FALSE;
	}

	if(port->len>=SR_ADV_PORT_SIZE) {
		LM_ERR("port parameter is too large\n");
		return SR_KEMI_FALSE;
	}

	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

	memcpy(_sr_adv_port_buf, port->s, port->len);
	_sr_adv_port_buf[port->len] = '\0';
	msg->set_global_port.s = _sr_adv_port_buf;
	msg->set_global_port.len = port->len;

	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_add_tcp_alias(sip_msg_t *msg, int port)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

#ifdef USE_TCP
	if ( msg->rcv.proto==PROTO_TCP
#ifdef USE_TLS
				|| msg->rcv.proto==PROTO_TLS
#endif
			) {
		if (tcpconn_add_alias(msg->rcv.proto_reserved1, port,
							msg->rcv.proto)!=0){
			LM_ERR("adding tcp alias failed\n");
			return SR_KEMI_FALSE;
		}
	}
#endif

	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_core_add_tcp_alias_via(sip_msg_t *msg)
{
	if(msg==NULL) {
		LM_WARN("invalid msg parameter\n");
		return SR_KEMI_FALSE;
	}

#ifdef USE_TCP
	if ( msg->rcv.proto==PROTO_TCP
#ifdef USE_TLS
				|| msg->rcv.proto==PROTO_TLS
#endif
			) {
		if (tcpconn_add_alias(msg->rcv.proto_reserved1, msg->via1->port,
							msg->rcv.proto)!=0){
			LM_ERR("adding tcp alias failed\n");
			return SR_KEMI_FALSE;
		}
	}
#endif
	return SR_KEMI_TRUE;
}


/**
 *
 */
static int sr_kemi_core_get_debug(sip_msg_t *msg)
{
	return get_cfg_debug_level();
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
	{ str_init(""), str_init("set_drop"),
		SR_KEMIP_NONE, sr_kemi_core_set_drop,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_myself"),
		SR_KEMIP_BOOL, sr_kemi_core_is_myself,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_myself_ruri"),
		SR_KEMIP_BOOL, sr_kemi_core_is_myself_ruri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_myself_furi"),
		SR_KEMIP_BOOL, sr_kemi_core_is_myself_furi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_myself_turi"),
		SR_KEMIP_BOOL, sr_kemi_core_is_myself_turi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_myself_suri"),
		SR_KEMIP_BOOL, sr_kemi_core_is_myself_suri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_myself_srcip"),
		SR_KEMIP_BOOL, sr_kemi_core_is_myself_srcip,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	{ str_init(""), str_init("add_local_rport"),
		SR_KEMIP_BOOL, sr_kemi_core_add_local_rport,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_method"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_method_in"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_in,
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
	{ str_init(""), str_init("set_advertised_address"),
		SR_KEMIP_INT, sr_kemi_core_set_advertised_address,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("set_advertised_port"),
		SR_KEMIP_INT, sr_kemi_core_set_advertised_port,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("add_tcp_alias"),
		SR_KEMIP_INT, sr_kemi_core_add_tcp_alias,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("add_tcp_alias_via"),
		SR_KEMIP_INT, sr_kemi_core_add_tcp_alias_via,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_INVITE"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_invite,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_ACK"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_ack,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_BYE"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_bye,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_CANCEL"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_cancel,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_REGISTER"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_register,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_OPTIONS"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_options,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_SUBSCRIBE"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_subscribe,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_PUBLISH"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_publish,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_NOTIFY"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_notify,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_REFER"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_refer,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_INFO"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_info,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_UPDATE"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_update,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_PRACK"),
		SR_KEMIP_BOOL, sr_kemi_core_is_method_prack,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_UDP"),
		SR_KEMIP_BOOL, sr_kemi_core_is_proto_udp,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_TCP"),
		SR_KEMIP_BOOL, sr_kemi_core_is_proto_tcp,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_TLS"),
		SR_KEMIP_BOOL, sr_kemi_core_is_proto_tls,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_WS"),
		SR_KEMIP_BOOL, sr_kemi_core_is_proto_ws,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_WSS"),
		SR_KEMIP_BOOL, sr_kemi_core_is_proto_wss,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_SCTP"),
		SR_KEMIP_BOOL, sr_kemi_core_is_proto_sctp,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_IPv4"),
		SR_KEMIP_BOOL, sr_kemi_core_is_af_ipv4,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_IPv6"),
		SR_KEMIP_BOOL, sr_kemi_core_is_af_ipv6,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_src_port"),
		SR_KEMIP_BOOL, sr_kemi_core_is_src_port,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_dst_port"),
		SR_KEMIP_BOOL, sr_kemi_core_is_dst_port,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("get_debug"),
		SR_KEMIP_INT, sr_kemi_core_get_debug,
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
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);
	/* anchor after last header */
	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if((anchor==NULL)
			|| (insert_new_lump_before(anchor, hdr, txt->len, 0) == 0)) {
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
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);

	if(hf==0) { /* after last header */
		anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	} else { /* after hf */
		anchor = anchor_lump(msg, hf->name.s + hf->len - msg->buf, 0, 0);
	}

	if((anchor==NULL)
			|| (insert_new_lump_before(anchor, hdr, txt->len, 0) == 0)) {
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return -1;
	}
	LM_DBG("appended after [%.*s] the hf: [%.*s]\n", hname->len, hname->s,
			txt->len, txt->s);

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
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);
	/* anchor before first header */
	anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, 0);
	if((anchor==NULL)
			|| (insert_new_lump_before(anchor, hdr, txt->len, 0) == 0)) {
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
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);
	if(hf==0) { /* before first header */
		anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, 0);
	} else { /* before hf */
		anchor = anchor_lump(msg, hf->name.s - msg->buf, 0, 0);
	}
	if((anchor==NULL)
			|| (insert_new_lump_before(anchor, hdr, txt->len, 0) == 0)) {
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return -1;
	}
	LM_DBG("inserted before [%.*s] the hf: %.*s\n", hname->len, hname->s,
			txt->len, txt->s);

	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_rmappend(sip_msg_t *msg, str *hrm, str *hadd)
{
	int ret;

	ret = sr_kemi_hdr_remove(msg, hrm);
	if(ret<0) {
		return ret;
	}
	return sr_kemi_hdr_append(msg, hadd);
}

/**
 *
 */
static int sr_kemi_hdr_rminsert(sip_msg_t *msg, str *hrm, str *hadd)
{
	int ret;

	ret = sr_kemi_hdr_remove(msg, hrm);
	if(ret<0) {
		return ret;
	}
	return sr_kemi_hdr_insert(msg, hadd);
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
static sr_kemi_xval_t* sr_kemi_hdr_get_mode(sip_msg_t *msg, str *hname, int idx,
		int rmode)
{
	char hbuf[256];
	str s;
	hdr_field_t shdr;
	hdr_field_t *ihdr;
#define SR_KEMI_VHDR_SIZE 256
	hdr_field_t *vhdr[SR_KEMI_VHDR_SIZE];
	int n;
	int hmatch;

	memset(&_sr_kemi_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_xval, rmode);
		return &_sr_kemi_xval;
	}
	/* we need to be sure we have parsed all headers */
	if(parse_headers(msg, HDR_EOH_F, 0)<0) {
		LM_ERR("error parsing headers\n");
		sr_kemi_xval_null(&_sr_kemi_xval, rmode);
		return &_sr_kemi_xval;
	}
	if(hname->len>=252) {
		LM_ERR("header name too long\n");
		sr_kemi_xval_null(&_sr_kemi_xval, rmode);
		return &_sr_kemi_xval;
	}

	memcpy(hbuf, hname->s, hname->len);
	hbuf[hname->len] = ':';
	hbuf[hname->len+1] = '\0';
	s.s = hbuf;
	s.len = hname->len + 1;

	if (parse_hname2_short(s.s, s.s + s.len, &shdr)==0) {
		LM_ERR("error parsing header name [%.*s]\n", s.len, s.s);
		sr_kemi_xval_null(&_sr_kemi_xval, rmode);
		return &_sr_kemi_xval;
	}
	n = 0;
	for (ihdr=msg->headers; ihdr; ihdr=ihdr->next) {
		hmatch = 0;
		if (shdr.type!=HDR_OTHER_T && shdr.type!=HDR_ERROR_T) {
			/* find by type */
			if (shdr.type==ihdr->type) {
				hmatch = 1;
			}
		} else {
			/* find by name */
			if (cmp_hdrname_str(&ihdr->name, hname)==0) {
				hmatch = 1;
			}
		}
		if (hmatch == 1) {
			if(idx==n) {
				break;
			} else {
				if(idx<0) {
					vhdr[n] = ihdr;
				}
				n++;
				if(n==SR_KEMI_VHDR_SIZE) {
					LM_DBG("too many headers with name: %.*s\n",
							hname->len, hname->s);
					sr_kemi_xval_null(&_sr_kemi_xval, rmode);
					return &_sr_kemi_xval;
				}
			}
		}
	}
	if(idx>=0) {
		if(ihdr==NULL) {
			sr_kemi_xval_null(&_sr_kemi_xval, rmode);
			return &_sr_kemi_xval;
		}
	} else {
		if(n + idx < 0) {
			sr_kemi_xval_null(&_sr_kemi_xval, rmode);
			return &_sr_kemi_xval;
		}
		ihdr = vhdr[n + idx];
	}

	_sr_kemi_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_xval.v.s = ihdr->body;
	return &_sr_kemi_xval;
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_hdr_get(sip_msg_t *msg, str *hname)
{
	return sr_kemi_hdr_get_mode(msg, hname, 0, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_hdr_gete(sip_msg_t *msg, str *hname)
{
	return sr_kemi_hdr_get_mode(msg, hname, 0, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_hdr_getw(sip_msg_t *msg, str *hname)
{
	return sr_kemi_hdr_get_mode(msg, hname, 0, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_hdr_get_idx(sip_msg_t *msg, str *hname, int idx)
{
	return sr_kemi_hdr_get_mode(msg, hname, idx, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_hdr_gete_idx(sip_msg_t *msg, str *hname, int idx)
{
	return sr_kemi_hdr_get_mode(msg, hname, idx, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_hdr_getw_idx(sip_msg_t *msg, str *hname, int idx)
{
	return sr_kemi_hdr_get_mode(msg, hname, idx, SR_KEMI_XVAL_NULL_PRINT);
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
	{ str_init("hdr"), str_init("rmappend"),
		SR_KEMIP_INT, sr_kemi_hdr_rmappend,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("rminsert"),
		SR_KEMIP_INT, sr_kemi_hdr_rminsert,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
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
	{ str_init("hdr"), str_init("get"),
		SR_KEMIP_XVAL, sr_kemi_hdr_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("gete"),
		SR_KEMIP_XVAL, sr_kemi_hdr_gete,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("getw"),
		SR_KEMIP_XVAL, sr_kemi_hdr_getw,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("get_idx"),
		SR_KEMIP_XVAL, sr_kemi_hdr_get_idx,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("gete_idx"),
		SR_KEMIP_XVAL, sr_kemi_hdr_gete_idx,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("getw_idx"),
		SR_KEMIP_XVAL, sr_kemi_hdr_getw_idx,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};


/**
 *
 */
void sr_kemi_xval_null(sr_kemi_xval_t *xval, int rmode)
{
	if(rmode==SR_KEMI_XVAL_NULL_PRINT) {
		xval->vtype = SR_KEMIP_STR;
		xval->v.s = *pv_get_null_str();
	} else if(rmode==SR_KEMI_XVAL_NULL_EMPTY) {
		xval->vtype = SR_KEMIP_STR;
		xval->v.s = *pv_get_empty_str();
	} else {
		xval->vtype = SR_KEMIP_NULL;
		xval->v.s.s = NULL;
		xval->v.s.len = 0;
	}
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_pv_get_mode(sip_msg_t *msg, str *pvn, int rmode)
{
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;

	memset(&_sr_kemi_xval, 0, sizeof(sr_kemi_xval_t));

	LM_DBG("pv get: %.*s\n", pvn->len, pvn->s);
	pl = pv_locate_name(pvn);
	if(pl != pvn->len) {
		LM_ERR("invalid pv [%.*s] (%d/%d)\n", pvn->len, pvn->s, pl, pvn->len);
		sr_kemi_xval_null(&_sr_kemi_xval, rmode);
		return &_sr_kemi_xval;
	}
	pvs = pv_cache_get(pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", pvn->len, pvn->s);
		sr_kemi_xval_null(&_sr_kemi_xval, rmode);
		return &_sr_kemi_xval;
	}

	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(msg, pvs, &val) != 0) {
		LM_ERR("unable to get pv value for [%.*s]\n", pvn->len, pvn->s);
		sr_kemi_xval_null(&_sr_kemi_xval, rmode);
		return &_sr_kemi_xval;
	}
	if(val.flags&PV_VAL_NULL) {
		sr_kemi_xval_null(&_sr_kemi_xval, rmode);
		return &_sr_kemi_xval;
	}
	if(val.flags&PV_TYPE_INT) {
		_sr_kemi_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_xval.v.n = val.ri;
		return &_sr_kemi_xval;
	}
	_sr_kemi_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_xval.v.s = val.rs;
	return &_sr_kemi_xval;
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_pv_get(sip_msg_t *msg, str *pvn)
{
	return sr_kemi_pv_get_mode(msg, pvn, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_pv_getw(sip_msg_t *msg, str *pvn)
{
	return sr_kemi_pv_get_mode(msg, pvn, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_pv_gete(sip_msg_t *msg, str *pvn)
{
	return sr_kemi_pv_get_mode(msg, pvn, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static void sr_kemi_pv_push_valx (sr_kemi_xval_t *xval, int rmode, int vi, str *vs)
{
	if(rmode==1) {
		xval->vtype = SR_KEMIP_INT;
		xval->v.n = vi;
	} else {
		xval->vtype = SR_KEMIP_STR;
		xval->v.s = *vs;
	}
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_pv_get_valx (sip_msg_t *msg, str *pvn, str *xsval,
		int xival, int rmode)
{
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;

	memset(&_sr_kemi_xval, 0, sizeof(sr_kemi_xval_t));

	LM_DBG("pv get: %.*s\n", pvn->len, pvn->s);
	pl = pv_locate_name(pvn);
	if(pl != pvn->len) {
		LM_ERR("invalid pv [%.*s] (%d/%d)\n", pvn->len, pvn->s, pl, pvn->len);
		sr_kemi_pv_push_valx(&_sr_kemi_xval, rmode, xival, xsval);
		return &_sr_kemi_xval;
	}
	pvs = pv_cache_get(pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", pvn->len, pvn->s);
		sr_kemi_pv_push_valx(&_sr_kemi_xval, rmode, xival, xsval);
		return &_sr_kemi_xval;
	}

	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(msg, pvs, &val) != 0) {
		LM_ERR("unable to get pv value for [%.*s]\n", pvn->len, pvn->s);
		sr_kemi_pv_push_valx(&_sr_kemi_xval, rmode, xival, xsval);
		return &_sr_kemi_xval;
	}
	if(val.flags&PV_VAL_NULL) {
		sr_kemi_pv_push_valx(&_sr_kemi_xval, rmode, xival, xsval);
		return &_sr_kemi_xval;
	}
	if(val.flags&PV_TYPE_INT) {
		_sr_kemi_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_xval.v.n = val.ri;
		return &_sr_kemi_xval;
	}
	_sr_kemi_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_xval.v.s = val.rs;
	return &_sr_kemi_xval;
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_pv_getvs (sip_msg_t *msg, str *pvn, str *xsval)
{
	return sr_kemi_pv_get_valx (msg, pvn, xsval, 0, 0);
}

/**
 *
 */
static sr_kemi_xval_t* sr_kemi_pv_getvn (sip_msg_t *msg, str *pvn, int xival)
{
	return sr_kemi_pv_get_valx (msg, pvn, NULL, xival, 1);
}

/**
 *
 */
static int sr_kemi_pv_seti (sip_msg_t *msg, str *pvn, int ival)
{
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;

	LM_DBG("pv get: %.*s\n", pvn->len, pvn->s);
	pl = pv_locate_name(pvn);
	if(pl != pvn->len) {
		LM_ERR("invalid pv [%.*s] (%d/%d)\n", pvn->len, pvn->s, pl, pvn->len);
		return SR_KEMI_FALSE;
	}
	pvs = pv_cache_get(pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", pvn->len, pvn->s);
		return SR_KEMI_FALSE;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.ri = ival;
	val.flags |= PV_TYPE_INT|PV_VAL_INT;

	if(pv_set_spec_value(msg, pvs, 0, &val)<0) {
		LM_ERR("unable to set pv [%.*s]\n", pvn->len, pvn->s);
		return SR_KEMI_FALSE;
	}

	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_pv_sets (sip_msg_t *msg, str *pvn, str *sval)
{
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;

	LM_DBG("pv set: %.*s\n", pvn->len, pvn->s);
	pl = pv_locate_name(pvn);
	if(pl != pvn->len) {
		LM_ERR("invalid pv [%.*s] (%d/%d)\n", pvn->len, pvn->s, pl, pvn->len);
		return SR_KEMI_FALSE;
	}
	pvs = pv_cache_get(pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", pvn->len, pvn->s);
		return SR_KEMI_FALSE;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.rs = *sval;
	val.flags |= PV_VAL_STR;

	if(pv_set_spec_value(msg, pvs, 0, &val)<0) {
		LM_ERR("unable to set pv [%.*s]\n", pvn->len, pvn->s);
		return SR_KEMI_FALSE;
	}

	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_pv_unset (sip_msg_t *msg, str *pvn)
{
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;

	LM_DBG("pv unset: %.*s\n", pvn->len, pvn->s);
	pl = pv_locate_name(pvn);
	if(pl != pvn->len) {
		LM_ERR("invalid pv [%.*s] (%d/%d)\n", pvn->len, pvn->s, pl, pvn->len);
		return SR_KEMI_FALSE;
	}
	pvs = pv_cache_get(pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", pvn->len, pvn->s);
		return SR_KEMI_FALSE;
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.flags |= PV_VAL_NULL;
	if(pv_set_spec_value(msg, pvs, 0, &val)<0) {
		LM_ERR("unable to unset pv [%.*s]\n", pvn->len, pvn->s);
		return SR_KEMI_FALSE;
	}

	return SR_KEMI_TRUE;
}

/**
 *
 */
static int sr_kemi_pv_is_null (sip_msg_t *msg, str *pvn)
{
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;

	LM_DBG("pv is null test: %.*s\n", pvn->len, pvn->s);
	pl = pv_locate_name(pvn);
	if(pl != pvn->len) {
		LM_ERR("invalid pv [%.*s] (%d/%d)\n", pvn->len, pvn->s, pl, pvn->len);
		return SR_KEMI_TRUE;
	}
	pvs = pv_cache_get(pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", pvn->len, pvn->s);
		return SR_KEMI_TRUE;
	}

	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(msg, pvs, &val) != 0) {
		LM_NOTICE("unable to get pv value for [%.*s]\n", pvn->len, pvn->s);
		return SR_KEMI_TRUE;
	}
	if(val.flags&PV_VAL_NULL) {
		return SR_KEMI_TRUE;
	} else {
		return SR_KEMI_FALSE;
	}
}

/**
 *
 */
static sr_kemi_t _sr_kemi_pv[] = {
	{ str_init("pv"), str_init("get"),
		SR_KEMIP_XVAL, sr_kemi_pv_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("getw"),
		SR_KEMIP_XVAL, sr_kemi_pv_getw,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("gete"),
		SR_KEMIP_XVAL, sr_kemi_pv_gete,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("getvn"),
		SR_KEMIP_XVAL, sr_kemi_pv_getvn,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("getvs"),
		SR_KEMIP_XVAL, sr_kemi_pv_getvs,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("seti"),
		SR_KEMIP_BOOL, sr_kemi_pv_seti,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("sets"),
		SR_KEMIP_BOOL, sr_kemi_pv_sets,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("unset"),
		SR_KEMIP_BOOL, sr_kemi_pv_unset,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("is_null"),
		SR_KEMIP_BOOL, sr_kemi_pv_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
sr_kemi_t* sr_kemi_exports_get_pv(void)
{
	return _sr_kemi_pv;
}

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
		LM_DBG("adding pv module\n");
		_sr_kemi_modules[_sr_kemi_modules_size].mname = _sr_kemi_pv[0].mname;
		_sr_kemi_modules[_sr_kemi_modules_size].kexp = _sr_kemi_pv;
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
		SHM_MEM_ERROR;
		return -1;
	}
	*_sr_kemi_cbname_list_size = 0;
	_sr_kemi_cbname_list
			= shm_malloc(KEMI_CBNAME_LIST_SIZE*sizeof(sr_kemi_cbname_t));
	if(_sr_kemi_cbname_list==NULL) {
		SHM_MEM_ERROR;
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
	{ SR_KEMIP_XVAL,   str_init("xval") },
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

/**
 *
 */
int sr_kemi_route(sr_kemi_eng_t *keng, sip_msg_t *msg, int rtype,
		str *ename, str *edata)
{
	flag_t sfbk;
	int ret;

	sfbk = getsflags();
	setsflagsval(0);
	reset_static_buffer();
	ret = keng->froute(msg, rtype, ename, edata);
	setsflagsval(sfbk);
	return ret;
}

/**
 *
 */
int sr_kemi_ctx_route(sr_kemi_eng_t *keng, run_act_ctx_t *ctx, sip_msg_t *msg,
		int rtype, str *ename, str *edata)
{
	run_act_ctx_t *bctx;
	int ret;

	bctx = sr_kemi_act_ctx_get();
	sr_kemi_act_ctx_set(ctx);
	ret = sr_kemi_route(keng, msg, rtype, ename, edata);
	sr_kemi_act_ctx_set(bctx);
	return ret;
}
