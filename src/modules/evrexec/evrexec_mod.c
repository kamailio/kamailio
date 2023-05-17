/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/route.h"
#include "../../core/receive.h"
#include "../../core/action.h"
#include "../../core/pt.h"
#include "../../core/ut.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/parser/parse_param.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"


MODULE_VERSION

typedef struct evrexec_task
{
	str ename;
	int rtid;
	str sockaddr;
	int sockfd;
	unsigned int wait;
	unsigned int workers;
	struct evrexec_task *next;
} evrexec_task_t;

evrexec_task_t *_evrexec_list = NULL;

typedef struct evrexec_info
{
	str data;
	str srcip;
	str srcport;
	int srcportno;
} evrexec_info_t;

static evrexec_info_t _evrexec_info = {0};

/** module functions */
static int mod_init(void);
static int child_init(int);

int evrexec_param(modparam_t type, void *val);
void evrexec_process_start(evrexec_task_t *it, int idx);
void evrexec_process_socket(evrexec_task_t *it, int idx);

static int pv_get_evr(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int pv_parse_evr_name(pv_spec_p sp, str *in);

static param_export_t params[] = {
		{"exec", PARAM_STRING | USE_FUNC_PARAM, (void *)evrexec_param},
		{0, 0, 0}};

static pv_export_t mod_pvs[] = {
		{{"evr", (sizeof("evr") - 1)}, PVT_OTHER, pv_get_evr, 0,
				pv_parse_evr_name, 0, 0, 0},

		{{0, 0}, 0, 0, 0, 0, 0, 0, 0}};

/** module exports */
struct module_exports exports = {
		"evrexec",		 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		0,				 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* RPC method exports */
		mod_pvs,		 /* exported pseudo-variables */
		0,				 /* response handling function */
		mod_init,		 /* module initialization function */
		child_init,		 /* per-child init function */
		0				 /* module destroy function */
};

static rpc_export_t evr_rpc_methods[];

/**
 * init module function
 */
static int mod_init(void)
{
	evrexec_task_t *it;

	if(rpc_register_array(evr_rpc_methods) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(_evrexec_list == NULL)
		return 0;

	/* init faked sip msg */
	if(faked_msg_init() < 0) {
		LM_ERR("failed to init evrexec local sip msg\n");
		return -1;
	}

	/* register additional processes */
	it = _evrexec_list;
	while(it) {
		register_procs(it->workers);
		it = it->next;
	}

	return 0;
}

/**
 *
 */
static int child_init(int rank)
{
	evrexec_task_t *it;
	int i;
	int pid;
	char si_desc[MAX_PT_DESC];

	if(_evrexec_list == NULL)
		return 0;

	if(rank != PROC_MAIN)
		return 0;

	it = _evrexec_list;
	while(it) {
		for(i = 0; i < it->workers; i++) {
			if(it->sockaddr.len > 0) {
				snprintf(si_desc, MAX_PT_DESC,
						"EVREXEC child=%d exec=%.*s socket=%.*s", i,
						it->ename.len, it->ename.s, it->sockaddr.len,
						it->sockaddr.s);
			} else {
				snprintf(si_desc, MAX_PT_DESC, "EVREXEC child=%d exec=%.*s", i,
						it->ename.len, it->ename.s);
			}
			pid = fork_process(PROC_RPC, si_desc, 1);
			if(pid < 0)
				return -1; /* error */
			if(pid == 0) {
				/* child */
				/* initialize the config framework */
				if(cfg_child_init())
					return -1;

				if(it->sockaddr.len > 0) {
					evrexec_process_socket(it, i);
				} else {
					evrexec_process_start(it, i);
				}
			}
		}
		it = it->next;
	}

	return 0;
}

/**
 *
 */
void evrexec_process_start(evrexec_task_t *it, int idx)
{
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng = NULL;
	str sidx = STR_NULL;

	if(it != NULL) {
		fmsg = faked_msg_next();
		set_route_type(LOCAL_ROUTE);
		if(it->wait > 0)
			sleep_us(it->wait);
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			if(it->rtid >= 0 && event_rt.rlist[it->rtid] != NULL) {
				run_top_route(event_rt.rlist[it->rtid], fmsg, 0);
			} else {
				LM_WARN("empty event route block [%.*s]\n", it->ename.len,
						it->ename.s);
			}
		} else {
			sidx.s = int2str(idx, &sidx.len);
			if(sr_kemi_route(keng, fmsg, EVENT_ROUTE, &it->ename, &sidx) < 0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
	}
	/* avoid exiting the process */
	while(1) {
		sleep(3600);
	}
}

/**
 *
 */
void evrexec_process_socket(evrexec_task_t *it, int idx)
{
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng = NULL;
	char hostval[64];
	char portval[6];
	struct addrinfo hints;
	int ret;
	struct addrinfo *res = 0;
	sr_phostp_t phostp;
	char rcvbuf[BUF_SIZE];
	struct sockaddr_storage src_addr;
	socklen_t src_addr_len;
	ssize_t count;
	char srchostval[NI_MAXHOST];
	char srcportval[NI_MAXSERV];

	if(parse_protohostport(&it->sockaddr, &phostp) < 0 || phostp.port == 0
			|| phostp.host.len > 62 || phostp.sport.len > 5) {
		LM_ERR("failed to parse or invalid local socket address: %.*s\n",
				it->sockaddr.len, it->sockaddr.s);
		return;
	}
	memcpy(hostval, phostp.host.s, phostp.host.len);
	hostval[phostp.host.len] = '\0';
	memcpy(portval, phostp.sport.s, phostp.sport.len);
	portval[phostp.sport.len] = '\0';

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	ret = getaddrinfo(hostval, portval, &hints, &res);
	if(ret != 0) {
		LM_ERR("failed to resolve local socket address: %.*s (ret: %d)",
				it->sockaddr.len, it->sockaddr.s, ret);
		return;
	}

	it->sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(it->sockfd == -1) {
		LM_ERR("failed to create socket - address: %.*s (%d/%s)\n",
				it->sockaddr.len, it->sockaddr.s, errno, strerror(errno));
		freeaddrinfo(res);
		return;
	}
	if(bind(it->sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		LM_ERR("failed to bind socket - address: %.*s (%d/%s)\n",
				it->sockaddr.len, it->sockaddr.s, errno, strerror(errno));
		close(it->sockfd);
		freeaddrinfo(res);
		return;
	}
	freeaddrinfo(res);

	while(1) {
		src_addr_len = sizeof(src_addr);
		count = recvfrom(it->sockfd, rcvbuf, sizeof(rcvbuf) - 1, 0,
				(struct sockaddr *)&src_addr, &src_addr_len);
		if(count == -1) {
			LM_ERR("failed to receive on socket - address: %.*s (%d/%s)\n",
					it->sockaddr.len, it->sockaddr.s, errno, strerror(errno));
			continue;
		} else if(count == sizeof(rcvbuf) - 1) {
			LM_WARN("datagram too large for buffer - truncated\n");
		}
		rcvbuf[count] = '\0';

		ret = getnameinfo((struct sockaddr *)&src_addr, src_addr_len,
				srchostval, sizeof(srchostval), srcportval, sizeof(srcportval),
				NI_NUMERICHOST | NI_NUMERICSERV);
		if(ret == 0) {
			LM_DBG("received data from %s port %s\n", srchostval, srcportval);
			_evrexec_info.srcip.s = srchostval;
			_evrexec_info.srcip.len = strlen(_evrexec_info.srcip.s);
			_evrexec_info.srcport.s = srcportval;
			_evrexec_info.srcport.len = strlen(_evrexec_info.srcport.s);
			str2sint(&_evrexec_info.srcport, &_evrexec_info.srcportno);
		}

		_evrexec_info.data.s = rcvbuf;
		_evrexec_info.data.len = (int)count;

		fmsg = faked_msg_next();
		set_route_type(LOCAL_ROUTE);
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			if(it->rtid >= 0 && event_rt.rlist[it->rtid] != NULL) {
				run_top_route(event_rt.rlist[it->rtid], fmsg, 0);
			} else {
				LM_WARN("empty event route block [%.*s]\n", it->ename.len,
						it->ename.s);
			}
		} else {
			if(sr_kemi_route(keng, fmsg, EVENT_ROUTE, &it->ename, &it->sockaddr)
					< 0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
		memset(&_evrexec_info, 0, sizeof(evrexec_info_t));
	}
	/* avoid exiting the process */
	while(1) {
		sleep(3600);
	}
}

/**
 *
 */
int evrexec_param(modparam_t type, void *val)
{
	param_t *params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	evrexec_task_t *it;
	evrexec_task_t tmp;
	sr_kemi_eng_t *keng = NULL;
	str s;

	if(val == NULL)
		return -1;
	s.s = (char *)val;
	s.len = strlen(s.s);
	if(s.s[s.len - 1] == ';')
		s.len--;
	if(parse_params(&s, CLASS_ANY, &phooks, &params_list) < 0)
		return -1;
	memset(&tmp, 0, sizeof(evrexec_task_t));
	for(pit = params_list; pit; pit = pit->next) {
		if(pit->name.len == 4 && strncasecmp(pit->name.s, "name", 4) == 0) {
			tmp.ename = pit->body;
		} else if(pit->name.len == 4
				  && strncasecmp(pit->name.s, "wait", 4) == 0) {
			if(tmp.wait == 0) {
				if(str2int(&pit->body, &tmp.wait) < 0) {
					LM_ERR("invalid wait: %.*s\n", pit->body.len, pit->body.s);
					return -1;
				}
			}
		} else if(pit->name.len == 7
				  && strncasecmp(pit->name.s, "workers", 7) == 0) {
			if(tmp.workers == 0) {
				if(str2int(&pit->body, &tmp.workers) < 0) {
					LM_ERR("invalid workers: %.*s\n", pit->body.len,
							pit->body.s);
					return -1;
				}
			}
		} else if(pit->name.len == 8
				  && strncasecmp(pit->name.s, "sockaddr", 8) == 0) {
			tmp.sockaddr = pit->body;

		} else {
			LM_ERR("invalid attribute: %.*s=%.*s\n", pit->name.len, pit->name.s,
					pit->body.len, pit->body.s);
			return -1;
		}
	}
	if(tmp.ename.s == NULL || tmp.ename.len <= 0) {
		LM_ERR("missing or invalid name attribute\n");
		free_params(params_list);
		return -1;
	}
	if(tmp.sockaddr.len > 0) {
		if(tmp.sockaddr.len < 6) {
			LM_ERR("invalid sockaddr: %.*s\n", tmp.sockaddr.len,
					tmp.sockaddr.s);
			free_params(params_list);
			return -1;
		}
		if(strncmp(tmp.sockaddr.s, "udp:", 4) != 0) {
			LM_ERR("unsupported sockaddr: %.*s\n", tmp.sockaddr.len,
					tmp.sockaddr.s);
			free_params(params_list);
			return -1;
		}
	}
	/* set '\0' at the end of route name */
	tmp.ename.s[tmp.ename.len] = '\0';
	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		tmp.rtid = route_get(&event_rt, tmp.ename.s);
		if(tmp.rtid == -1) {
			LM_ERR("event route not found: %.*s\n", tmp.ename.len, tmp.ename.s);
			free_params(params_list);
			return -1;
		}
	} else {
		tmp.rtid = -1;
	}

	it = (evrexec_task_t *)pkg_malloc(sizeof(evrexec_task_t));
	if(it == 0) {
		LM_ERR("no more pkg memory\n");
		free_params(params_list);
		return -1;
	}
	memcpy(it, &tmp, sizeof(evrexec_task_t));
	it->sockfd = -1;
	if(it->workers == 0)
		it->workers = 1;
	if(it->sockaddr.len > 0)
		it->workers = 1;
	it->next = _evrexec_list;
	_evrexec_list = it;
	free_params(params_list);
	return 0;
}

/**
 *
 */
static int pv_get_evr(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	if(param == NULL || _evrexec_info.data.s == NULL) {
		return pv_get_null(msg, param, res);
	}

	switch(param->pvn.u.isname.name.n) {
		case 0: /* data */
			return pv_get_strval(msg, param, res, &_evrexec_info.data);
		case 1: /* srcpip */
			if(_evrexec_info.srcip.s == NULL) {
				return pv_get_null(msg, param, res);
			}
			return pv_get_strval(msg, param, res, &_evrexec_info.srcip);
		case 2: /* srcport */
			if(_evrexec_info.srcport.s == NULL) {
				return pv_get_null(msg, param, res);
			}
			return pv_get_strval(msg, param, res, &_evrexec_info.srcport);
		case 3: /* srcportno */
			return pv_get_sintval(msg, param, res, _evrexec_info.srcportno);

		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
static int pv_parse_evr_name(pv_spec_p sp, str *in)
{
	if(sp == NULL || in == NULL || in->len <= 0)
		return -1;

	switch(in->len) {
		case 4:
			if(strncmp(in->s, "data", 4) == 0) {
				sp->pvp.pvn.u.isname.name.n = 0;
			} else {
				goto error;
			}
			break;
		case 5:
			if(strncmp(in->s, "srcip", 5) == 0) {
				sp->pvp.pvn.u.isname.name.n = 1;
			} else {
				goto error;
			}
			break;
		case 7:
			if(strncmp(in->s, "srcport", 7) == 0) {
				sp->pvp.pvn.u.isname.name.n = 2;
			} else {
				goto error;
			}
			break;
		case 9:
			if(strncmp(in->s, "srcportno", 9) == 0) {
				sp->pvp.pvn.u.isname.name.n = 3;
			} else {
				goto error;
			}
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV evr key: %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
const char *rpc_evr_run_doc[2] = {"Execute event_route block", 0};

/**
 *
 */
void rpc_evr_run(rpc_t *rpc, void *c)
{
	str evr_name = STR_NULL;
	str evr_data = STR_NULL;
	int ret = 0;
	int evr_id = -1;
	sr_kemi_eng_t *keng = NULL;
	sip_msg_t *fmsg = NULL;
	int rtbk = 0;
	char evr_buf[2];

	ret = rpc->scan(c, "s*s", &evr_name.s, &evr_data.s);
	if(ret < 1) {
		LM_ERR("failed getting the parameters");
		rpc->fault(c, 500, "Invalid parameters");
		return;
	}
	evr_name.len = strlen(evr_name.s);
	if(ret < 2) {
		evr_buf[0] = '\0';
		evr_data.s = evr_buf;
		evr_data.len = 0;
	} else {
		evr_data.len = strlen(evr_data.s);
	}

	_evrexec_info.data = evr_data;
	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		evr_id = route_lookup(&event_rt, evr_name.s);
		if(evr_id == -1) {
			memset(&_evrexec_info, 0, sizeof(evrexec_info_t));
			LM_ERR("event route not found: %.*s\n", evr_name.len, evr_name.s);
			rpc->fault(c, 500, "Event route not found");
			return;
		}
	} else {
		evr_id = -1;
	}

	fmsg = faked_msg_next();
	rtbk = get_route_type();
	set_route_type(LOCAL_ROUTE);

	if(evr_id >= 0) {
		if(event_rt.rlist[evr_id] != NULL) {
			run_top_route(event_rt.rlist[evr_id], fmsg, 0);
		} else {
			LM_WARN("empty event route block [%.*s]\n", evr_name.len,
					evr_name.s);
		}
	} else {
		if(keng != NULL
				&& sr_kemi_route(keng, fmsg, EVENT_ROUTE, &evr_name, &evr_data)
						   < 0) {
			LM_ERR("error running event route kemi callback\n");
		}
	}
	set_route_type(rtbk);
	memset(&_evrexec_info, 0, sizeof(evrexec_info_t));
}

/**
 *
 */
static rpc_export_t evr_rpc_methods[] = {
		{"evrexec.run", rpc_evr_run, rpc_evr_run_doc, 0}, {0, 0, 0, 0}};
