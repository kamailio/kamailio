/**
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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
#include <stdarg.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/forward.h"
#include "../../core/resolve.h"
#include "../../core/udp_server.h"
#include "../../core/kemi.h"

MODULE_VERSION

static int _lc_log_udp = 0;
static struct dest_info _lc_udp_dst = {0};

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_log_udp(struct sip_msg* msg, char* txt, char* p2);

void _lc_core_log_udp(int lpriority, const char *format, ...);

static cmd_export_t cmds[]={
	{"log_udp", (cmd_function)w_log_udp, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{0, 0, 0}
};

struct module_exports exports = {
	"log_custom",   /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,           /* exported functions */
	params,         /* exported parameters */
	0,              /* exported RPC functions */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy    	/* destroy function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	return 0;
}

/**
 * @brief Initialize module children
 */
static int child_init(int rank)
{
	if(rank!=PROC_INIT)
		return 0;

	_lc_udp_dst.proto = PROTO_UDP;
	_lc_udp_dst.send_sock=get_send_socket(NULL, &_lc_udp_dst.to, PROTO_UDP);
	if (_lc_udp_dst.send_sock==0) {
		_lc_udp_dst.send_sock = get_out_socket(&_lc_udp_dst.to, PROTO_UDP);
		if (_lc_udp_dst.send_sock==0) {
			LM_ERR("failed to get send socket\n");
			return -1;
		}
	}
	LM_DBG("setting udp-send custom logging function\n");
	km_log_func_set(&_lc_core_log_udp);
	_lc_log_udp = 1;



	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 *
 */
static int w_log_udp(struct sip_msg* msg, char* txt, char* p2)
{
	str stxt;
	int ret;

	if(_lc_log_udp==0)
		return 1;

	if(fixup_get_svalue(msg, (gparam_t*)txt, &stxt)!=0) {
		LM_ERR("unable to get text parameter\n");
		return -1;
	}

	ret=udp_send(&_lc_udp_dst, stxt.s, stxt.len);

	if(ret==0) return 1;

	return ret;
}

#define LC_LOG_MSG_MAX_SIZE	16384
void _lc_core_log_udp(int lpriority, const char *format, ...)
{
	va_list arglist;
	char obuf[LC_LOG_MSG_MAX_SIZE];
	int n;

	va_start(arglist, format);

	n = 0;
	n += snprintf(obuf + n, LC_LOG_MSG_MAX_SIZE - n, "(%d) ", my_pid());
	n += vsnprintf(obuf + n, LC_LOG_MSG_MAX_SIZE - n, format, arglist);
	va_end(arglist);
	if(udp_send(&_lc_udp_dst, obuf, n)!=0) {
		LM_DBG("udp send returned non zero\n");
	}
}

int ki_log_udp(sip_msg_t *msg, str *txt)
{
	int ret;

	if(_lc_log_udp==0)
		return 1;

	ret=udp_send(&_lc_udp_dst, txt->s, txt->len);

	if(ret==0) return 1;

	return ret;

}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_log_custom_exports[] = {
	{ str_init("log_custom"), str_init("log_udp"),
		SR_KEMIP_INT, ki_log_udp,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	str dest = {0};
	int ret = 0;
	struct sip_uri next_hop, *u;
	char *p;

	if(_km_log_engine_type==0 || _km_log_engine_data==0)
		return 0;


	if(strcasecmp(_km_log_engine_type, "udp")!=0)
		return 0;

	dest.s = _km_log_engine_data;
	dest.len = strlen(dest.s);

	init_dest_info(&_lc_udp_dst);

	u = &next_hop;
	u->port_no = 5060;
	u->host = dest;
	p = dest.s;
	/* detect ipv6 */
	p = memchr(p, ']', dest.len);
	if (p) p++;
	else p = dest.s;
	p = memchr(p, ':', dest.len - (p - dest.s));
	if (p) {
		u->host.len = p - dest.s;
		p++;
		u->port_no = str2s(p, dest.len - (p - dest.s), NULL);
	}

	ret = sip_hostport2su(&_lc_udp_dst.to, &u->host, u->port_no,
			&_lc_udp_dst.proto);
	if(ret!=0) {
		LM_ERR("failed to resolve [%.*s]\n", u->host.len,
				ZSW(u->host.s));
		return -1;
	}

	sr_kemi_modules_add(sr_kemi_log_custom_exports);
	return 0;
}
