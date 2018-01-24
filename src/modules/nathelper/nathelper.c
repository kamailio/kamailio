/*
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
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
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifndef __USE_BSD
#define __USE_BSD
#endif
#include <netinet/ip.h>
#ifndef __FAVOR_BSD
#define __FAVOR_BSD
#endif
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../core/flags.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/error.h"
#include "../../core/forward.h"
#include "../../core/ip_addr.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parser_f.h"
#include "../../core/parser/parse_methods.h"
#include "../../core/parser/sdp/sdp.h"
#include "../../core/resolve.h"
#include "../../core/timer.h"
#include "../../core/trim.h"
#include "../../core/ut.h"
#include "../../core/pt.h"
#include "../../core/timer_proc.h"
#include "../../core/pvar.h"
#include "../../core/lvalue.h"
#include "../../core/msg_translator.h"
#include "../../core/usr_avp.h"
#include "../../core/socket_info.h"
#include "../../core/mod_fix.h"
#include "../../core/dset.h"
#include "../../core/select.h"
#include "../../core/kemi.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../registrar/sip_msg.h"
#include "../usrloc/usrloc.h"
#include "nathelper.h"
#include "nhelpr_funcs.h"
#include "sip_pinger.h"

MODULE_VERSION

#if !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define PF_LOCAL PF_UNIX
#endif

/* NAT UAC test constants */
#define NAT_UAC_TEST_C_1918 0x01
#define NAT_UAC_TEST_RCVD 0x02
#define NAT_UAC_TEST_V_1918 0x04
#define NAT_UAC_TEST_S_1918 0x08
#define NAT_UAC_TEST_RPORT 0x10
#define NAT_UAC_TEST_O_1918 0x20
#define NAT_UAC_TEST_WS 0x40
#define NAT_UAC_TEST_C_PORT 0x80


#define DEFAULT_NATPING_STATE 1


static int nat_uac_test_f(struct sip_msg *msg, char *str1, char *str2);
static int fix_nated_contact_f(struct sip_msg *, char *, char *);
static int add_contact_alias_0_f(struct sip_msg *, char *, char *);
static int add_contact_alias_3_f(struct sip_msg *, char *, char *, char *);
static int set_contact_alias_f(struct sip_msg *msg, char *str1, char *str2);
static int handle_ruri_alias_f(struct sip_msg *, char *, char *);
static int pv_get_rr_count_f(struct sip_msg *, pv_param_t *, pv_value_t *);
static int pv_get_rr_top_count_f(struct sip_msg *, pv_param_t *, pv_value_t *);
static int fix_nated_sdp_f(struct sip_msg *, char *, char *);
static int is_rfc1918_f(struct sip_msg *, char *, char *);
static int extract_mediaip(str *, str *, int *, char *);
static int alter_mediaip(struct sip_msg *, str *, str *, int, str *, int, int);
static int fix_nated_register_f(struct sip_msg *, char *, char *);
static int fixup_fix_nated_register(void **param, int param_no);
static int fixup_fix_sdp(void **param, int param_no);
static int fixup_add_contact_alias(void **param, int param_no);
static int add_rcv_param_f(struct sip_msg *, char *, char *);
static int nh_sip_reply_received(sip_msg_t *msg);

static void nh_timer(unsigned int, void *);
static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int nathelper_rpc_init(void);

static usrloc_api_t ul;

static int cblen = 0;
static int natping_interval = 0;
struct socket_info *force_socket = 0;


/* clang-format off */
static struct {
	const char *cnetaddr;
	uint32_t netaddr;
	uint32_t mask;
} nets_1918[] = {
	{"10.0.0.0",    0, 0xffffffffu << 24},
	{"172.16.0.0",  0, 0xffffffffu << 20},
	{"192.168.0.0", 0, 0xffffffffu << 16},
	{"100.64.0.0",  0, 0xffffffffu << 22}, /* rfc6598 - cg-nat */
	{NULL, 0, 0}
};
/* clang-format on */

/*
 * If this parameter is set then the natpinger will ping only contacts
 * that have the NAT flag set in user location database
 */
static int ping_nated_only = 0;
static const char sbuf[4] = {0, 0, 0, 0};
static str force_socket_str = STR_NULL;
static int sipping_flag = -1;
static int natping_disable_flag = -1;
static int natping_processes = 1;

static str nortpproxy_str = str_init("a=nortpproxy:yes");

static char *rcv_avp_param = NULL;
static unsigned short rcv_avp_type = 0;
static int_str rcv_avp_name;

static char *natping_socket = NULL;
static int udpping_from_path = 0;
static int sdp_oldmediaip = 1;
static int raw_sock = -1;
static unsigned int raw_ip = 0;
static unsigned short raw_port = 0;
static int nh_keepalive_timeout = 0;
static request_method_t sipping_method_id = 0;
/* filter contacts by server_id */
static int nh_filter_srvid = 0;

/*0-> disabled, 1 ->enabled*/
unsigned int *natping_state = NULL;

/* clang-format off */
static cmd_export_t cmds[] = {
	{"fix_nated_contact",  (cmd_function)fix_nated_contact_f,    0,
		0, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"add_contact_alias",  (cmd_function)add_contact_alias_0_f,  0,
		0, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"add_contact_alias",  (cmd_function)add_contact_alias_3_f,  3,
		fixup_add_contact_alias, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"set_contact_alias",  (cmd_function)set_contact_alias_f,  0,
		0, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"handle_ruri_alias",  (cmd_function)handle_ruri_alias_f,    0,
		0, 0,
		REQUEST_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"fix_nated_sdp",      (cmd_function)fix_nated_sdp_f,        1,
		fixup_fix_sdp,  0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"fix_nated_sdp",      (cmd_function)fix_nated_sdp_f,        2,
		fixup_fix_sdp, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"nat_uac_test",       (cmd_function)nat_uac_test_f,         1,
		fixup_igp_null, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"fix_nated_register", (cmd_function)fix_nated_register_f,   0,
		fixup_fix_nated_register, 0,
		REQUEST_ROUTE },
	{"add_rcv_param",      (cmd_function)add_rcv_param_f,        0,
		0, 0,
		REQUEST_ROUTE },
	{"add_rcv_param",      (cmd_function)add_rcv_param_f,        1,
		fixup_igp_null, 0,
		REQUEST_ROUTE },
	{"is_rfc1918",         (cmd_function)is_rfc1918_f,           1,
		fixup_spve_null, 0,
		ANY_ROUTE },
	{0, 0, 0, 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
	{{"rr_count", (sizeof("rr_count")-1)}, /* number of records routes */
		PVT_CONTEXT, pv_get_rr_count_f, 0, 0, 0, 0, 0},
	{{"rr_top_count", (sizeof("rr_top_count")-1)}, /* number of topmost rrs */
		PVT_CONTEXT, pv_get_rr_top_count_f, 0, 0, 0, 0, 0},
	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"natping_interval",      INT_PARAM, &natping_interval      },
	{"ping_nated_only",       INT_PARAM, &ping_nated_only       },
	{"nortpproxy_str",        PARAM_STR, &nortpproxy_str      },
	{"received_avp",          PARAM_STRING, &rcv_avp_param         },
	{"force_socket",          PARAM_STR, &force_socket_str      },
	{"sipping_from",          PARAM_STR, &sipping_from        },
	{"sipping_method",        PARAM_STR, &sipping_method      },
	{"sipping_bflag",         INT_PARAM, &sipping_flag          },
	{"natping_disable_bflag", INT_PARAM, &natping_disable_flag  },
	{"natping_processes",     INT_PARAM, &natping_processes     },
	{"natping_socket",        PARAM_STRING, &natping_socket        },
	{"keepalive_timeout",     INT_PARAM, &nh_keepalive_timeout  },
	{"udpping_from_path",     INT_PARAM, &udpping_from_path     },
	{"append_sdp_oldmediaip", INT_PARAM, &sdp_oldmediaip        },
	{"filter_server_id",      INT_PARAM, &nh_filter_srvid },

	{0, 0, 0}
};

struct module_exports exports = {
	"nathelper",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,           /* exported statistics */
	0,           /* exported MI functions */
	mod_pvs,     /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,
	nh_sip_reply_received, /* reply processing */
	mod_destroy, /* destroy function */
	child_init
};
/* clang-format on */


static int sel_nathelper(str *res, select_t *s, struct sip_msg *msg)
{
	/* dummy */
	return 0;
}

static int sel_rewrite_contact(str *res, select_t *s, struct sip_msg *msg);

SELECT_F(select_any_nameaddr)

select_row_t sel_declaration[] = {
		{NULL, SEL_PARAM_STR, STR_STATIC_INIT("nathelper"), sel_nathelper,
				SEL_PARAM_EXPECTED},
		{sel_nathelper, SEL_PARAM_STR, STR_STATIC_INIT("rewrite_contact"),
				sel_rewrite_contact, CONSUME_NEXT_INT},

		{sel_rewrite_contact, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"),
				select_any_nameaddr, NESTED | CONSUME_NEXT_STR},

		{NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}};

static int fixup_fix_sdp(void **param, int param_no)
{
	if(param_no == 1) {
		/* flags */
		return fixup_igp_null(param, param_no);
	}
	if(param_no == 2) {
		/* new IP */
		return fixup_spve_all(param, param_no);
	}
	LM_ERR("unexpected param no: %d\n", param_no);
	return -1;
}

static int fixup_fix_nated_register(void **param, int param_no)
{
	if(rcv_avp_name.n == 0) {
		LM_ERR("you must set 'received_avp' parameter. Must be same value as"
			   " parameter 'received_avp' of registrar module\n");
		return -1;
	}
	return 0;
}

static int fixup_add_contact_alias(void **param, int param_no)
{
	if((param_no >= 1) && (param_no <= 3))
		return fixup_spve_null(param, 1);

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static void nathelper_rpc_enable_ping(rpc_t *rpc, void *ctx)
{
	int value = 0;
	if(natping_state == NULL) {
		rpc->fault(ctx, 500, "NATping disabled");
		return;
	}

	if(rpc->scan(ctx, "d", &value) < 1) {
		rpc->fault(ctx, 500, "No parameter");
		return;
	}
	(*natping_state) = value ? 1 : 0;
}

static const char *nathelper_rpc_enable_ping_doc[2] = {
		"Set (enable/disable) nat ping", 0};

rpc_export_t nathelper_rpc[] = {
		{"nathelper.enable_ping", nathelper_rpc_enable_ping,
				nathelper_rpc_enable_ping_doc, 0},
		{0, 0, 0, 0}};

static int nathelper_rpc_init(void)
{
	if(rpc_register_array(nathelper_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

static int init_raw_socket(void)
{
	int on = 1;

	raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if(raw_sock == -1) {
		LM_ERR("cannot create raw socket\n");
		return -1;
	}

	if(setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) == -1) {
		LM_ERR("cannot set socket options\n");
		return -1;
	}

	return raw_sock;
}


static int get_natping_socket(
		char *socket, unsigned int *ip, unsigned short *port)
{
	struct hostent *he;
	str host;
	int lport;
	int lproto;

	if(parse_phostport(socket, &host.s, &host.len, &lport, &lproto) != 0) {
		LM_CRIT("invalid natping_socket parameter <%s>\n", natping_socket);
		return -1;
	}

	if(lproto != PROTO_UDP && lproto != PROTO_NONE) {
		LM_CRIT("natping_socket can be only UDP <%s>\n", natping_socket);
		return 0;
	}
	lproto = PROTO_UDP;
	*port = lport ? (unsigned short)lport : SIP_PORT;

	he = sip_resolvehost(&host, port, (char *)(void *)&lproto);
	if(he == 0) {
		LM_ERR("could not resolve hostname:\"%.*s\"\n", host.len, host.s);
		return -1;
	}
	if(he->h_addrtype != AF_INET) {
		LM_ERR("only ipv4 addresses allowed in natping_socket\n");
		return -1;
	}

	memcpy(ip, he->h_addr_list[0], he->h_length);

	return 0;
}


static int mod_init(void)
{
	int i;
	bind_usrloc_t bind_usrloc;
	struct in_addr addr;
	pv_spec_t avp_spec;
	str s;
	int port, proto;
	str host;

	if(nathelper_rpc_init() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(rcv_avp_param && *rcv_avp_param) {
		s.s = rcv_avp_param;
		s.len = strlen(s.s);
		if(pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", rcv_avp_param);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &rcv_avp_name, &rcv_avp_type)
				!= 0) {
			LM_ERR("[%s]- invalid AVP definition\n", rcv_avp_param);
			return -1;
		}
	} else {
		rcv_avp_name.n = 0;
		rcv_avp_type = 0;
	}

	if(force_socket_str.s && force_socket_str.len > 0) {
		if(parse_phostport(force_socket_str.s, &host.s, &host.len, &port, &proto) == 0) {
			force_socket = grep_sock_info(&host, port, proto);
			if(force_socket == 0) {
				LM_ERR("non-local force_socket <%s>\n", force_socket_str.s);
			}
		}
	}

	/* create raw socket? */
	if((natping_socket && natping_socket[0]) || udpping_from_path) {
		if((!udpping_from_path)
				&& get_natping_socket(natping_socket, &raw_ip, &raw_port) != 0)
			return -1;
		if(init_raw_socket() < 0)
			return -1;
	}

	if(nortpproxy_str.s && nortpproxy_str.len > 0) {
		while(nortpproxy_str.len > 0
				&& (nortpproxy_str.s[nortpproxy_str.len - 1] == '\r'
						   || nortpproxy_str.s[nortpproxy_str.len - 1] == '\n'))
			nortpproxy_str.len--;
	}

	if(natping_interval > 0) {
		bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
		if(!bind_usrloc) {
			LM_ERR("can't find usrloc module\n");
			return -1;
		}

		if(bind_usrloc(&ul) < 0) {
			return -1;
		}

		natping_state = (unsigned int *)shm_malloc(sizeof(unsigned int));
		if(!natping_state) {
			LM_ERR("no shmem left\n");
			return -1;
		}
		*natping_state = DEFAULT_NATPING_STATE;

		if(ping_nated_only && ul.nat_flag == 0) {
			LM_ERR("bad config - ping_nated_only enabled, but no nat bflag"
				   " set in usrloc module\n");
			return -1;
		}
		if(natping_processes < 0) {
			LM_ERR("bad config - natping_processes must be >= 0\n");
			return -1;
		}
		ul.set_max_partition(natping_processes * natping_interval);

		sipping_flag = (sipping_flag == -1) ? 0 : (1 << sipping_flag);
		natping_disable_flag =
				(natping_disable_flag == -1) ? 0 : (1 << natping_disable_flag);

		/* set reply function if SIP natping is enabled */
		if(sipping_flag) {
			if(sipping_from.s == 0 || sipping_from.len <= 0) {
				LM_ERR("SIP ping enabled, but SIP ping FROM is empty!\n");
				return -1;
			}
			if(sipping_method.s == 0 || sipping_method.len <= 0) {
				LM_ERR("SIP ping enabled, but SIP ping method is empty!\n");
				return -1;
			}
			if(nh_keepalive_timeout > 0 && ul.set_keepalive_timeout != NULL) {
				ul.set_keepalive_timeout(nh_keepalive_timeout);
			}

			if(parse_method_name(&sipping_method, &sipping_method_id) < 0) {
				LM_ERR("invalid SIP ping method [%.*s]!\n", sipping_method.len,
						sipping_method.s);
				return -1;
			}
			exports.response_f = sipping_rpl_filter;
			init_sip_ping();
		}

		register_dummy_timers(natping_processes);
	}

	/* Prepare 1918 networks list */
	for(i = 0; nets_1918[i].cnetaddr != NULL; i++) {
		if(inet_aton(nets_1918[i].cnetaddr, &addr) != 1)
			abort();
		nets_1918[i].netaddr = ntohl(addr.s_addr) & nets_1918[i].mask;
	}

	register_select_table(sel_declaration);

	return 0;
}


static int child_init(int rank)
{
	int i;

	if(rank == PROC_MAIN && natping_interval > 0) {
		for(i = 0; i < natping_processes; i++) {
			if(fork_dummy_timer(PROC_TIMER, "TIMER NH", 1 /*socks flag*/,
					   nh_timer, (void *)(unsigned long)i, 1 /*sec*/)
					< 0) {
				LM_ERR("failed to register timer routine as process\n");
				return -1;
				/* error */
			}
		}
	}

	if(rank <= 0 && rank != PROC_TIMER)
		return 0;

	return 0;
}


static void mod_destroy(void)
{
	/*free the shared memory*/
	if(natping_state)
		shm_free(natping_state);
}


static int isnulladdr(str *sx, int pf)
{
	char *cp;

	if(pf == AF_INET6) {
		for(cp = sx->s; cp < sx->s + sx->len; cp++)
			if(*cp != '0' && *cp != ':')
				return 0;
		return 1;
	}
	return (sx->len == 7 && memcmp("0.0.0.0", sx->s, 7) == 0);
}

/*
 * Replaces ip:port pair in the Contact: field with the source address
 * of the packet.
 */
static int fix_nated_contact(struct sip_msg *msg)
{
	int offset, len, len1;
	char *cp, *buf, temp[2];
	contact_t *c;
	struct lump *anchor;
	struct sip_uri uri;
	str hostport;
	str params1 = {0};
	str params2 = {0};

	if(get_contact_uri(msg, &uri, &c) == -1)
		return -1;
	if((c->uri.s < msg->buf) || (c->uri.s > (msg->buf + msg->len))) {
		LM_ERR("you can't call fix_nated_contact twice, "
			   "check your config!\n");
		return -1;
	}

	offset = c->uri.s - msg->buf;
	anchor = del_lump(msg, offset, c->uri.len, HDR_CONTACT_T);
	if(anchor == 0)
		return -1;

	hostport = uri.host;
	if(uri.port.len > 0)
		hostport.len = uri.port.s + uri.port.len - uri.host.s;

	cp = ip_addr2a(&msg->rcv.src_ip);
	len = c->uri.len + strlen(cp) + 6 /* :port */ - hostport.len + 1;
	if(msg->rcv.src_ip.af == AF_INET6)
		len += 2;
	buf = pkg_malloc(len);
	if(buf == NULL) {
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	temp[0] = hostport.s[0];
	temp[1] = c->uri.s[c->uri.len];
	c->uri.s[c->uri.len] = hostport.s[0] = '\0';
	if(uri.maddr.len <= 0) {
		if(msg->rcv.src_ip.af == AF_INET6) {
			len1 = snprintf(buf, len, "%s[%s]:%d%s", c->uri.s, cp,
					msg->rcv.src_port, hostport.s + hostport.len);
		} else {
			len1 = snprintf(buf, len, "%s%s:%d%s", c->uri.s, cp,
					msg->rcv.src_port, hostport.s + hostport.len);
		}
	} else {
		/* skip maddr parameter - makes no sense anymore */
		LM_DBG("removing maddr parameter from contact uri: [%.*s]\n",
				uri.maddr.len, uri.maddr.s);
		params1.s = hostport.s + hostport.len;
		params1.len = uri.maddr.s - params1.s;
		while(params1.len > 0 && (params1.s[params1.len - 1] == ' '
										 || params1.s[params1.len - 1] == '\t'
										 || params1.s[params1.len - 1] == ';'))
			params1.len--;
		params2.s = uri.maddr.s + uri.maddr.len;
		params2.len = c->uri.s + c->uri.len - params2.s;
		if(msg->rcv.src_ip.af == AF_INET6) {
			len1 = snprintf(buf, len, "%s[%s]:%d%.*s%.*s", c->uri.s, cp,
					msg->rcv.src_port, params1.len, params1.s, params2.len,
					params2.s);
		} else {
			len1 = snprintf(buf, len, "%s%s:%d%.*s%.*s", c->uri.s, cp,
					msg->rcv.src_port, params1.len, params1.s, params2.len,
					params2.s);
		}
	}
	if(len1 < len)
		len = len1;
	hostport.s[0] = temp[0];
	c->uri.s[c->uri.len] = temp[1];
	if(insert_new_lump_after(anchor, buf, len, HDR_CONTACT_T) == 0) {
		pkg_free(buf);
		return -1;
	}
	c->uri.s = buf;
	c->uri.len = len;

	return 1;
}

static int fix_nated_contact_f(struct sip_msg *msg, char *str1, char *str2)
{
	return fix_nated_contact(msg);
}

/*
 * Replaces ip:port pair in the Contact: field with the source address
 * of the packet.
 */
static int set_contact_alias(struct sip_msg *msg)
{
	char nbuf[MAX_URI_SIZE];
	str nuri;
	int br;

	int offset, len;
	char *buf;
	contact_t *c;
	struct lump *anchor;
	struct sip_uri uri;

	nuri.s = nbuf;
	nuri.len = MAX_URI_SIZE;
	if(get_contact_uri(msg, &uri, &c) == -1)
		return -1;
	if((c->uri.s < msg->buf) || (c->uri.s > (msg->buf + msg->len))) {
		LM_ERR("you can't update contact twice, check your config!\n");
		return -1;
	}

	if(uri_add_rcv_alias(msg, &c->uri, &nuri) < 0) {
		LM_DBG("cannot add the alias parameter\n");
		return -1;
	}

	br = 1;
	if(c->uri.s[-1] == '<')
		br = 0;


	len = nuri.len + 2 * br;
	buf = pkg_malloc(len + 1);
	if(buf == NULL) {
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	if(br == 1) {
		buf[0] = '<';
		strncpy(buf + 1, nuri.s, nuri.len);
		buf[len - 1] = '>';
	} else {
		strncpy(buf, nuri.s, nuri.len);
	}
	buf[len] = '\0';

	offset = c->uri.s - msg->buf;
	anchor = del_lump(msg, offset, c->uri.len, HDR_CONTACT_T);
	if(anchor == 0) {
		pkg_free(buf);
		return -1;
	}

	if(insert_new_lump_after(anchor, buf, len, HDR_CONTACT_T) == 0) {
		pkg_free(buf);
		return -1;
	}
	c->uri.s = buf + br;
	c->uri.len = len - 2 * br;

	return 1;
}

static int set_contact_alias_f(struct sip_msg *msg, char *str1, char *str2)
{
	return set_contact_alias(msg);
}

#define SALIAS ";alias="
#define SALIAS_LEN (sizeof(SALIAS) - 1)

/*
 * Adds ;alias=ip~port~proto param to contact uri containing received ip,
 * port, and transport proto if contact uri ip and port do not match
 * received ip and port.
 */
static int add_contact_alias_0(struct sip_msg *msg)
{
	int len, param_len, ip_len;
	contact_t *c;
	struct lump *anchor;
	struct sip_uri uri;
	struct ip_addr *ip;
	char *bracket, *lt, *param, *at, *port, *start;

	/* Do nothing if Contact header does not exist */
	if(!msg->contact) {
		if(parse_headers(msg, HDR_CONTACT_F, 0) == -1) {
			LM_ERR("while parsing headers\n");
			return -1;
		}
		if(!msg->contact) {
			LM_DBG("no contact header\n");
			return 2;
		}
	}
	if(get_contact_uri(msg, &uri, &c) == -1) {
		LM_ERR("failed to get contact uri\n");
		return -1;
	}

	/* Compare source ip and port against contact uri */
	if(((ip = str2ip(&(uri.host))) == NULL)
			&& ((ip = str2ip6(&(uri.host))) == NULL)) {
		LM_DBG("contact uri host is not an ip address\n");
	} else {
		if (ip_addr_cmp(ip, &(msg->rcv.src_ip)) &&
                    ((msg->rcv.src_port == uri.port_no) ||
                     ((uri.port.len == 0) && (msg->rcv.src_port == 5060))) &&
                    (uri.proto == msg->rcv.proto)) {
                        LM_DBG("no need to add alias param\n");
                        return 2;
                }
	}

	/* Check if function has been called already */
	if((c->uri.s < msg->buf) || (c->uri.s > (msg->buf + msg->len))) {
		LM_ERR("you can't call add_contact_alias twice, check your config!\n");
		return -1;
	}

	/* Check if Contact URI needs to be enclosed in <>s */
	lt = param = NULL;
	bracket = memchr(msg->contact->body.s, '<', msg->contact->body.len);
	if(bracket == NULL) {
		/* add opening < */
		lt = (char *)pkg_malloc(1);
		if(!lt) {
			LM_ERR("no pkg memory left for lt sign\n");
			goto err;
		}
		*lt = '<';
		anchor = anchor_lump(msg, msg->contact->body.s - msg->buf, 0, 0);
		if(anchor == NULL) {
			LM_ERR("anchor_lump for beginning of contact body failed\n");
			goto err;
		}
		if(insert_new_lump_before(anchor, lt, 1, 0) == 0) {
			LM_ERR("insert_new_lump_before for \"<\" failed\n");
			goto err;
		}
	}

	/* Create  ;alias param */
	param_len = SALIAS_LEN + 1 /* [ */ + IP6_MAX_STR_SIZE
				+ 1 /* ] */ + 1 /* ~ */ + 5 /* port */ + 1 /* ~ */
				+ 1 /* proto */ + 1 /* > */;
	param = (char *)pkg_malloc(param_len);
	if(!param) {
		LM_ERR("no pkg memory left for alias param\n");
		goto err;
	}
	at = param;
	/* ip address */
	append_str(at, SALIAS, SALIAS_LEN);
	if(msg->rcv.src_ip.af == AF_INET6)
		append_chr(at, '[');
	ip_len = ip_addr2sbuf(&(msg->rcv.src_ip), at, param_len - SALIAS_LEN);
	if(ip_len <= 0) {
		LM_ERR("failed to copy source ip\n");
		goto err;
	}
	at = at + ip_len;
	if(msg->rcv.src_ip.af == AF_INET6)
		append_chr(at, ']');
	/* port */
	append_chr(at, '~');
	port = int2str(msg->rcv.src_port, &len);
	append_str(at, port, len);
	/* proto */
	append_chr(at, '~');
	if((msg->rcv.proto < PROTO_UDP) || (msg->rcv.proto > PROTO_WSS)) {
		LM_ERR("invalid transport protocol\n");
		goto err;
	}
	append_chr(at, msg->rcv.proto + '0');
	/* closing > */
	if(bracket == NULL) {
		append_chr(at, '>');
	}
	param_len = at - param;

	/* Add  ;alias param */
	LM_DBG("adding param <%.*s>\n", param_len, param);
	if(uri.port.len > 0) {
		start = uri.port.s + uri.port.len;
	} else {
		start = uri.host.s + uri.host.len;
	}
	anchor = anchor_lump(msg, start - msg->buf, 0, 0);
	if(anchor == NULL) {
		LM_ERR("anchor_lump for ;alias param failed\n");
		goto err;
	}
	if(insert_new_lump_after(anchor, param, param_len, 0) == 0) {
		LM_ERR("insert_new_lump_after for ;alias param failed\n");
		goto err;
	}
	return 1;

err:
	if(lt)
		pkg_free(lt);
	if(param)
		pkg_free(param);
	return -1;
}

static int add_contact_alias_0_f(struct sip_msg *msg, char *str1, char *str2)
{
	return add_contact_alias_0(msg);
}

static int proto_type_to_int(char *proto)
{
	if(strcasecmp(proto, "udp") == 0)
		return PROTO_UDP;
	if(strcasecmp(proto, "tcp") == 0)
		return PROTO_TCP;
	if(strcasecmp(proto, "tls") == 0)
		return PROTO_TLS;
	if(strcasecmp(proto, "sctp") == 0)
		return PROTO_SCTP;
	if(strcasecmp(proto, "ws") == 0)
		return PROTO_WS;
	if(strcasecmp(proto, "wss") == 0)
		return PROTO_WSS;
	return PROTO_OTHER;
}


/*
 * Adds ;alias=ip~port~proto param to contact uri containing ip, port,
 * and encoded proto given as parameters.
 */
static int add_contact_alias_3(
		sip_msg_t *msg, str *ip_str, str *port_str, str *proto_str)
{
	int param_len, proto;
	unsigned int tmp;
	contact_t *c;
	struct lump *anchor;
	struct sip_uri uri;
	char *bracket, *lt, *param, *at, *start;

	/* Do nothing if Contact header does not exist */
	if(!msg->contact) {
		if(parse_headers(msg, HDR_CONTACT_F, 0) == -1) {
			LM_ERR("while parsing headers\n");
			return -1;
		}
		if(!msg->contact) {
			LM_DBG("no contact header\n");
			return 2;
		}
	}
	if(get_contact_uri(msg, &uri, &c) == -1) {
		LM_ERR("failed to get contact uri\n");
		return -1;
	}

	if((str2ip(ip_str) == NULL) && (str2ip6(ip_str) == NULL)) {
		LM_ERR("ip param value %s is not valid IP address\n", ip_str->s);
		return -1;
	}
	if((str2int(port_str, &tmp) == -1) || (tmp == 0) || (tmp > 65535)) {
		LM_ERR("port param value is not valid port\n");
		return -1;
	}
	proto = proto_type_to_int(proto_str->s);
	if(proto == PROTO_OTHER) {
		LM_ERR("proto param value %s is not a known protocol\n", proto_str->s);
		return -1;
	}

	/* Check if function has been called already */
	if((c->uri.s < msg->buf) || (c->uri.s > (msg->buf + msg->len))) {
		LM_ERR("you can't call alias_contact twice, check your config!\n");
		return -1;
	}

	/* Check if Contact URI needs to be enclosed in <>s */
	lt = param = NULL;
	bracket = memchr(msg->contact->body.s, '<', msg->contact->body.len);
	if(bracket == NULL) {
		/* add opening < */
		lt = (char *)pkg_malloc(1);
		if(!lt) {
			LM_ERR("no pkg memory left for lt sign\n");
			goto err;
		}
		*lt = '<';
		anchor = anchor_lump(msg, msg->contact->body.s - msg->buf, 0, 0);
		if(anchor == NULL) {
			LM_ERR("anchor_lump for beginning of contact body failed\n");
			goto err;
		}
		if(insert_new_lump_before(anchor, lt, 1, 0) == 0) {
			LM_ERR("insert_new_lump_before for \"<\" failed\n");
			goto err;
		}
	}

	/* Create  ;alias param */
	param_len = SALIAS_LEN + IP6_MAX_STR_SIZE + 1 /* ~ */ + 5 /* port */
				+ 1 /* ~ */ + 1 /* proto */ + 1 /* closing > */;
	param = (char *)pkg_malloc(param_len);
	if(!param) {
		LM_ERR("no pkg memory left for alias param\n");
		goto err;
	}
	at = param;
	/* ip address */
	append_str(at, SALIAS, SALIAS_LEN);
	append_str(at, ip_str->s, ip_str->len);
	/* port */
	append_chr(at, '~');
	append_str(at, port_str->s, port_str->len);
	/* proto */
	append_chr(at, '~');
	append_chr(at, proto + '0');
	/* closing > */
	if(bracket == NULL) {
		append_chr(at, '>');
	}
	param_len = at - param;

	/* Add  ;alias param */
	LM_DBG("adding param <%.*s>\n", param_len, param);
	if(uri.port.len > 0) {
		start = uri.port.s + uri.port.len;
	} else {
		start = uri.host.s + uri.host.len;
	}
	anchor = anchor_lump(msg, start - msg->buf, 0, 0);
	if(anchor == NULL) {
		LM_ERR("anchor_lump for ;alias param failed\n");
		goto err;
	}
	if(insert_new_lump_after(anchor, param, param_len, 0) == 0) {
		LM_ERR("insert_new_lump_after for ;alias param failed\n");
		goto err;
	}
	return 1;

err:
	if(lt)
		pkg_free(lt);
	if(param)
		pkg_free(param);
	return -1;
}

static int add_contact_alias_3_f(
		sip_msg_t *msg, char *_ip, char *_port, char *_proto)
{
	str ip_str, port_str, proto_str;

	/* Get and check param values */
	if(fixup_get_svalue(msg, (gparam_p)_ip, &ip_str) != 0) {
		LM_ERR("cannot get ip param value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)_port, &port_str) != 0) {
		LM_ERR("cannot get port param value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)_proto, &proto_str) != 0) {
		LM_ERR("cannot get proto param value\n");
		return -1;
	}
	return add_contact_alias_3(msg, &ip_str, &port_str, &proto_str);
}

#define ALIAS "alias="
#define ALIAS_LEN (sizeof(ALIAS) - 1)

/*
 * Checks if r-uri has alias param and if so, removes it and sets $du
 * based on its value.
 */
static int handle_ruri_alias(struct sip_msg *msg)
{
	str uri, proto;
	char buf[MAX_URI_SIZE], *val, *sep, *at, *next, *cur_uri, *rest, *port,
			*trans;
	unsigned int len, rest_len, val_len, alias_len, proto_type, cur_uri_len,
			ip_port_len;

	if(parse_sip_msg_uri(msg) < 0) {
		LM_ERR("while parsing Request-URI\n");
		return -1;
	}
	rest = msg->parsed_uri.sip_params.s;
	rest_len = msg->parsed_uri.sip_params.len;
	if(rest_len == 0) {
		LM_DBG("no params\n");
		return 2;
	}
	while(rest_len >= ALIAS_LEN) {
		if(strncmp(rest, ALIAS, ALIAS_LEN) == 0)
			break;
		sep = memchr(rest, 59 /* ; */, rest_len);
		if(sep == NULL) {
			LM_DBG("no alias param\n");
			return 2;
		} else {
			rest_len = rest_len - (sep - rest + 1);
			rest = sep + 1;
		}
	}

	if(rest_len < ALIAS_LEN) {
		LM_DBG("no alias param\n");
		return 2;
	}

	/* set dst uri based on alias param value */
	val = rest + ALIAS_LEN;
	val_len = rest_len - ALIAS_LEN;
	port = memchr(val, 126 /* ~ */, val_len);
	if(port == NULL) {
		LM_ERR("no '~' in alias param value\n");
		return -1;
	}
	*(port++) = ':';
	trans = memchr(port, 126 /* ~ */, val_len - (port - val));
	if(trans == NULL) {
		LM_ERR("no second '~' in alias param value\n");
		return -1;
	}
	at = &(buf[0]);
	append_str(at, "sip:", 4);
	ip_port_len = trans - val;
	alias_len = SALIAS_LEN + ip_port_len + 2 /* ~n */;
	memcpy(at, val, ip_port_len);
	at = at + ip_port_len;
	trans = trans + 1;
	if((ip_port_len + 2 > val_len) || (*trans == ';') || (*trans == '?')) {
		LM_ERR("no proto in alias param\n");
		return -1;
	}
	proto_type = *trans - 48 /* char 0 */;
	if(proto_type != PROTO_UDP) {
		proto_type_to_str(proto_type, &proto);
		if(proto.len == 0) {
			LM_ERR("unknown proto in alias param\n");
			return -1;
		}
		append_str(at, ";transport=", 11);
		memcpy(at, proto.s, proto.len);
		at = at + proto.len;
	}
	next = trans + 1;
	if((ip_port_len + 2 < val_len) && (*next != ';') && (*next != '?')) {
		LM_ERR("invalid alias param value\n");
		return -1;
	}
	uri.s = &(buf[0]);
	uri.len = at - &(buf[0]);
	LM_DBG("setting dst_uri to <%.*s>\n", uri.len, uri.s);
	if(set_dst_uri(msg, &uri) == -1) {
		LM_ERR("failed to set dst uri\n");
		return -1;
	}

	/* remove alias param */
	if(msg->new_uri.s) {
		cur_uri = msg->new_uri.s;
		cur_uri_len = msg->new_uri.len;
	} else {
		cur_uri = msg->first_line.u.request.uri.s;
		cur_uri_len = msg->first_line.u.request.uri.len;
	}
	at = &(buf[0]);
	len = rest - 1 /* ; */ - cur_uri;
	memcpy(at, cur_uri, len);
	at = at + len;
	len = cur_uri_len - alias_len - len;
	memcpy(at, rest + alias_len - 1, len);
	uri.s = &(buf[0]);
	uri.len = cur_uri_len - alias_len;
	LM_DBG("rewriting r-uri to <%.*s>\n", uri.len, uri.s);
	return rewrite_uri(msg, &uri);
}

static int handle_ruri_alias_f(struct sip_msg *msg, char *str1, char *str2)
{
	return handle_ruri_alias(msg);
}

/*
 * Counts and return the number of record routes in rr headers of the message.
 */
static int pv_get_rr_count_f(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	unsigned int count;
	struct hdr_field *header;
	rr_t *body;

	if(msg == NULL)
		return -1;

	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("while parsing message\n");
		return -1;
	}

	count = 0;
	header = msg->record_route;

	while(header) {
		if(header->type == HDR_RECORDROUTE_T) {
			if(parse_rr(header) == -1) {
				LM_ERR("while parsing rr header\n");
				return -1;
			}
			body = (rr_t *)header->parsed;
			while(body) {
				count++;
				body = body->next;
			}
		}
		header = header->next;
	}

	return pv_get_uintval(msg, param, res, (unsigned int)count);
}

/*
 * Return count of topmost record routes in rr headers of the message.
 */
static int pv_get_rr_top_count_f(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str uri;
	struct sip_uri puri;

	if(msg == NULL)
		return -1;

	if(!msg->record_route && (parse_headers(msg, HDR_RECORDROUTE_F, 0) == -1)) {
		LM_ERR("while parsing Record-Route header\n");
		return -1;
	}

	if(!msg->record_route) {
		return pv_get_uintval(msg, param, res, 0);
	}

	if(parse_rr(msg->record_route) == -1) {
		LM_ERR("while parsing rr header\n");
		return -1;
	}

	uri = ((rr_t *)msg->record_route->parsed)->nameaddr.uri;
	if(parse_uri(uri.s, uri.len, &puri) < 0) {
		LM_ERR("while parsing rr uri\n");
		return -1;
	}

	if(puri.r2.len > 0) {
		return pv_get_uintval(msg, param, res, 2);
	} else {
		return pv_get_uintval(msg, param, res, 1);
	}
}

/*
 * Test if IP address in netaddr belongs to RFC1918 networks
 * netaddr in network byte order
 */
static inline int is1918addr_n(uint32_t netaddr)
{
	int i;
	uint32_t hl;

	hl = ntohl(netaddr);
	for(i = 0; nets_1918[i].cnetaddr != NULL; i++) {
		if((hl & nets_1918[i].mask) == nets_1918[i].netaddr) {
			return 1;
		}
	}
	return 0;
}

/*
 * Test if IP address pointed to by saddr belongs to RFC1918 networks
 */
static inline int is1918addr(str *saddr)
{
	struct in_addr addr;
	int rval;
	char backup;

	rval = -1;
	backup = saddr->s[saddr->len];
	saddr->s[saddr->len] = '\0';
	if(inet_aton(saddr->s, &addr) != 1)
		goto theend;
	rval = is1918addr_n(addr.s_addr);

theend:
	saddr->s[saddr->len] = backup;
	return rval;
}

/*
 * Test if IP address pointed to by ip belongs to RFC1918 networks
 */
static inline int is1918addr_ip(struct ip_addr *ip)
{
	if(ip->af != AF_INET)
		return 0;
	return is1918addr_n(ip->u.addr32[0]);
}

/*
 * test for occurrence of RFC1918 IP address in Contact HF
 */
static int contact_1918(struct sip_msg *msg)
{
	struct sip_uri uri;
	contact_t *c;

	if(get_contact_uri(msg, &uri, &c) == -1)
		return -1;

	return (is1918addr(&(uri.host)) == 1) ? 1 : 0;
}

/*
 * test if source port of signaling is different from
 * port advertised in Contact
 */
static int contact_rport(struct sip_msg *msg)
{
	struct sip_uri uri;
	contact_t *c;

	if(get_contact_uri(msg, &uri, &c) == -1) {
		return -1;
	}

	if(msg->rcv.src_port != (uri.port_no ? uri.port_no : SIP_PORT)) {
		return 1;
	} else {
		return 0;
	}
}

/*
 * test for occurrence of RFC1918 IP address in SDP
 */
static int sdp_1918(struct sip_msg *msg)
{
	str *ip;
	int pf;
	int ret;
	int sdp_session_num, sdp_stream_num;
	sdp_session_cell_t *sdp_session;
	sdp_stream_cell_t *sdp_stream;

	ret = parse_sdp(msg);
	if(ret != 0) {
		if(ret < 0)
			LM_ERR("Unable to parse sdp\n");
		return 0;
	}

	sdp_session_num = 0;
	for(;;) {
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session)
			break;
		sdp_stream_num = 0;
		for(;;) {
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream)
				break;
			if(sdp_stream->ip_addr.s && sdp_stream->ip_addr.len) {
				ip = &(sdp_stream->ip_addr);
				pf = sdp_stream->pf;
			} else {
				ip = &(sdp_session->ip_addr);
				pf = sdp_session->pf;
			}
			if(pf != AF_INET || isnulladdr(ip, pf))
				break;
			if(is1918addr(ip) == 1)
				return 1;
			sdp_stream_num++;
		}
		sdp_session_num++;
	}
	return 0;
}

/*
 * test for occurrence of RFC1918 IP address in top Via
 */
static int via_1918(struct sip_msg *msg)
{

	return (is1918addr(&(msg->via1->host)) == 1) ? 1 : 0;
}

static int nat_uac_test(struct sip_msg *msg, int tests)
{
	/* return true if any of the NAT-UAC tests holds */

	/* test if the source port is different from the port in Via */
	if((tests & NAT_UAC_TEST_RPORT)
			&& (msg->rcv.src_port
					   != (msg->via1->port ? msg->via1->port : SIP_PORT))) {
		return 1;
	}
	/*
	 * test if source address of signaling is different from
	 * address advertised in Via
	 */
	if((tests & NAT_UAC_TEST_RCVD) && received_via_test(msg))
		return 1;
	/*
	 * test for occurrences of RFC1918 addresses in Contact
	 * header field
	 */
	if((tests & NAT_UAC_TEST_C_1918) && (contact_1918(msg) > 0))
		return 1;
	/*
	 * test for occurrences of RFC1918 addresses in SDP body
	 */
	if((tests & NAT_UAC_TEST_S_1918) && sdp_1918(msg))
		return 1;
	/*
	 * test for occurrences of RFC1918 addresses top Via
	 */
	if((tests & NAT_UAC_TEST_V_1918) && via_1918(msg))
		return 1;

	/*
	 * test for occurrences of RFC1918 addresses in source address
	 */
	if((tests & NAT_UAC_TEST_O_1918) && is1918addr_ip(&msg->rcv.src_ip))
		return 1;

	/*
	 * test prototype to check whether the message arrived on a WebSocket
	 */
	if((tests & NAT_UAC_TEST_WS)
			&& (msg->rcv.proto == PROTO_WS || msg->rcv.proto == PROTO_WSS))
		return 1;

	/*
	 * test if source port of signaling is different from
	 * port advertised in Contact
	 */
	if((tests & NAT_UAC_TEST_C_PORT) && (contact_rport(msg) > 0))
		return 1;

	/* no test succeeded */
	return -1;
}

static int nat_uac_test_f(struct sip_msg *msg, char *str1, char *str2)
{
	int tflags = 0;
	if(fixup_get_ivalue(msg, (gparam_t*)str1, &tflags)<0) {
		LM_ERR("failed to get the value for flags parameter\n");
		return -1;
	}
	return nat_uac_test(msg, tflags);
}

static int is_rfc1918(struct sip_msg *msg, str *address)
{
	return (is1918addr(address) == 1) ? 1 : -1;
}


static int is_rfc1918_f(struct sip_msg *msg, char *str1, char *str2)
{
	str address;

	if(fixup_get_svalue(msg, (gparam_p)str1, &address) != 0
			|| address.len == 0) {
		LM_ERR("invalid address parameter\n");
		return -2;
	}

	return is_rfc1918(msg, &address);
}

#define ADD_ADIRECTION 0x01
#define FIX_MEDIP 0x02
#define ADD_ANORTPPROXY 0x04
#define FIX_ORGIP 0x08

#define ADIRECTION "a=direction:active"
#define ADIRECTION_LEN (sizeof(ADIRECTION) - 1)

#define AOLDMEDIP "a=oldmediaip:"
#define AOLDMEDIP_LEN (sizeof(AOLDMEDIP) - 1)

#define AOLDMEDIP6 "a=oldmediaip6:"
#define AOLDMEDIP6_LEN (sizeof(AOLDMEDIP6) - 1)

#define AOLDMEDPRT "a=oldmediaport:"
#define AOLDMEDPRT_LEN (sizeof(AOLDMEDPRT) - 1)


/* replace ip addresses in SDP and return umber of replacements */
static inline int replace_sdp_ip(
		struct sip_msg *msg, str *org_body, char *line, str *ip)
{
	str body1, oldip, newip;
	str body = *org_body;
	unsigned hasreplaced = 0;
	int pf, pf1 = 0;
	str body2;
	char *bodylimit = body.s + body.len;
	int ret;
	int count = 0;

	/* Iterate all lines and replace ips in them. */
	if(!ip) {
		newip.s = ip_addr2a(&msg->rcv.src_ip);
		newip.len = strlen(newip.s);
	} else {
		newip = *ip;
	}
	body1 = body;
	for(;;) {
		if(extract_mediaip(&body1, &oldip, &pf, line) == -1)
			break;
		if(pf != AF_INET) {
			LM_ERR("not an IPv4 address in '%s' SDP\n", line);
			return -1;
		}
		if(!pf1)
			pf1 = pf;
		else if(pf != pf1) {
			LM_ERR("mismatching address families in '%s' SDP\n", line);
			return -1;
		}
		body2.s = oldip.s + oldip.len;
		body2.len = bodylimit - body2.s;
		ret = alter_mediaip(
				msg, &body1, &oldip, pf, &newip, pf, sdp_oldmediaip);
		if(ret == -1) {
			LM_ERR("can't alter '%s' IP\n", line);
			return -1;
		}
		count += ret;
		hasreplaced = 1;
		body1 = body2;
	}
	if(!hasreplaced) {
		LM_ERR("can't extract '%s' IP from the SDP\n", line);
		return -1;
	}

	return count;
}

static int fix_nated_sdp_f(struct sip_msg *msg, char *str1, char *str2)
{
	str body;
	str ip;
	int level, rest_len;
	char *buf, *m_start, *m_end;
	struct lump *anchor;
	int ret;
	int count = 0;

	if(fixup_get_ivalue(msg, (gparam_t *)str1, &level) != 0) {
		LM_ERR("failed to get value for first parameter\n");
		return -1;
	}
	if(str2 && fixup_get_svalue(msg, (gparam_t *)str2, &ip) != 0) {
		LM_ERR("failed to get value for second parameter\n");
		return -1;
	}

	if(extract_body(msg, &body) == -1) {
		LM_ERR("cannot extract body from msg!\n");
		return -1;
	}

	if(level & (ADD_ADIRECTION | ADD_ANORTPPROXY)) {

		msg->msg_flags |= FL_FORCE_ACTIVE;

		if(level & ADD_ADIRECTION) {
			m_start = ser_memmem(body.s, "\r\nm=", body.len, 4);
			while(m_start != NULL) {
				m_start += 4;
				rest_len = body.len - (m_start - body.s);
				m_start = m_end = ser_memmem(m_start, "\r\nm=", rest_len, 4);
				if(!m_end)
					m_end = body.s + body.len; /* just before the final \r\n */
				anchor = anchor_lump(msg, m_end - msg->buf, 0, 0);
				if(anchor == NULL) {
					LM_ERR("anchor_lump failed\n");
					return -1;
				}
				buf = pkg_malloc((ADIRECTION_LEN + CRLF_LEN) * sizeof(char));
				if(buf == NULL) {
					LM_ERR("out of pkg memory\n");
					return -1;
				}
				memcpy(buf, CRLF, CRLF_LEN);
				memcpy(buf + CRLF_LEN, ADIRECTION, ADIRECTION_LEN);
				if(insert_new_lump_after(
						   anchor, buf, ADIRECTION_LEN + CRLF_LEN, 0)
						== NULL) {
					LM_ERR("insert_new_lump_after failed\n");
					pkg_free(buf);
					return -1;
				}
			}
		}

		if((level & ADD_ANORTPPROXY) && nortpproxy_str.len) {
			anchor = anchor_lump(msg, body.s + body.len - msg->buf, 0, 0);
			if(anchor == NULL) {
				LM_ERR("anchor_lump failed\n");
				return -1;
			}
			buf = pkg_malloc((nortpproxy_str.len + CRLF_LEN) * sizeof(char));
			if(buf == NULL) {
				LM_ERR("out of pkg memory\n");
				return -1;
			}
			memcpy(buf, CRLF, CRLF_LEN);
			memcpy(buf + CRLF_LEN, nortpproxy_str.s, nortpproxy_str.len);
			if(insert_new_lump_after(
					   anchor, buf, nortpproxy_str.len + CRLF_LEN, 0)
					== NULL) {
				LM_ERR("insert_new_lump_after failed\n");
				pkg_free(buf);
				return -1;
			}
		}
	}

	if(level & FIX_MEDIP) {
		/* Iterate all c= and replace ips in them. */
		ret = replace_sdp_ip(msg, &body, "c=", str2 ? &ip : 0);
		if(ret == -1)
			return -1;
		count += ret;
	}

	if(level & FIX_ORGIP) {
		/* Iterate all o= and replace ips in them. */
		ret = replace_sdp_ip(msg, &body, "o=", str2 ? &ip : 0);
		if(ret == -1)
			return -1;
		count += ret;
	}

	return count > 0 ? 1 : 2;
}

static int extract_mediaip(str *body, str *mediaip, int *pf, char *line)
{
	char *cp, *cp1;
	int len, nextisip;

	cp1 = NULL;
	for(cp = body->s; (len = body->s + body->len - cp) > 0;) {
		cp1 = ser_memmem(cp, line, len, 2);
		if(cp1 == NULL || cp1[-1] == '\n' || cp1[-1] == '\r')
			break;
		cp = cp1 + 2;
	}
	if(cp1 == NULL)
		return -1;

	mediaip->s = cp1 + 2;
	mediaip->len =
			eat_line(mediaip->s, body->s + body->len - mediaip->s) - mediaip->s;
	trim_len(mediaip->len, mediaip->s, *mediaip);

	nextisip = 0;
	for(cp = mediaip->s; cp < mediaip->s + mediaip->len;) {
		len = eat_token_end(cp, mediaip->s + mediaip->len) - cp;
		if(nextisip == 1) {
			mediaip->s = cp;
			mediaip->len = len;
			nextisip++;
			break;
		}
		if(len == 3 && memcmp(cp, "IP", 2) == 0) {
			switch(cp[2]) {
				case '4':
					nextisip = 1;
					*pf = AF_INET;
					break;

				case '6':
					nextisip = 1;
					*pf = AF_INET6;
					break;

				default:
					break;
			}
		}
		cp = eat_space_end(cp + len, mediaip->s + mediaip->len);
	}
	if(nextisip != 2 || mediaip->len == 0) {
		LM_ERR("no `IP[4|6]' in `%s' field\n", line);
		return -1;
	}
	return 1;
}

static int alter_mediaip(struct sip_msg *msg, str *body, str *oldip, int oldpf,
		str *newip, int newpf, int preserve)
{
	char *buf;
	int offset;
	struct lump *anchor;
	str omip, nip, oip;

	/* check that updating mediaip is really necessary */
	if(oldpf == newpf && isnulladdr(oldip, oldpf))
		return 0;
	if(newip->len == oldip->len && memcmp(newip->s, oldip->s, newip->len) == 0)
		return 0;

	if(preserve != 0) {
		anchor = anchor_lump(msg, body->s + body->len - msg->buf, 0, 0);
		if(anchor == NULL) {
			LM_ERR("anchor_lump failed\n");
			return -1;
		}
		if(oldpf == AF_INET6) {
			omip.s = AOLDMEDIP6;
			omip.len = AOLDMEDIP6_LEN;
		} else {
			omip.s = AOLDMEDIP;
			omip.len = AOLDMEDIP_LEN;
		}
		buf = pkg_malloc(omip.len + oldip->len + CRLF_LEN);
		if(buf == NULL) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(buf, CRLF, CRLF_LEN);
		memcpy(buf + CRLF_LEN, omip.s, omip.len);
		memcpy(buf + CRLF_LEN + omip.len, oldip->s, oldip->len);
		if(insert_new_lump_after(
				   anchor, buf, omip.len + oldip->len + CRLF_LEN, 0)
				== NULL) {
			LM_ERR("insert_new_lump_after failed\n");
			pkg_free(buf);
			return -1;
		}
	}

	if(oldpf == newpf) {
		nip.len = newip->len;
		nip.s = pkg_malloc(nip.len);
		if(nip.s == NULL) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(nip.s, newip->s, newip->len);
	} else {
		nip.len = newip->len + 2;
		nip.s = pkg_malloc(nip.len);
		if(nip.s == NULL) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(nip.s + 2, newip->s, newip->len);
		nip.s[0] = (newpf == AF_INET6) ? '6' : '4';
		nip.s[1] = ' ';
	}

	oip = *oldip;
	if(oldpf != newpf) {
		do {
			oip.s--;
			oip.len++;
		} while(*oip.s != '6' && *oip.s != '4');
	}
	offset = oip.s - msg->buf;
	anchor = del_lump(msg, offset, oip.len, 0);
	if(anchor == NULL) {
		LM_ERR("del_lump failed\n");
		pkg_free(nip.s);
		return -1;
	}

	if(insert_new_lump_after(anchor, nip.s, nip.len, 0) == 0) {
		LM_ERR("insert_new_lump_after failed\n");
		pkg_free(nip.s);
		return -1;
	}
	return 1;
}


static u_short raw_checksum(unsigned char *buffer, int len)
{
	u_long sum = 0;

	while(len > 1) {
		sum += *buffer << 8;
		buffer++;
		sum += *buffer;
		buffer++;
		len -= 2;
	}
	if(len) {
		sum += *buffer << 8;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum = (sum >> 16) + (sum);

	return (u_short)~sum;
}


static int send_raw(const char *buf, int buf_len, union sockaddr_union *to,
		const unsigned int s_ip, const unsigned int s_port)
{
	struct ip *ip;
	struct udphdr *udp;
	unsigned char packet[50];
	int len = sizeof(struct ip) + sizeof(struct udphdr) + buf_len;

	if(len > sizeof(packet)) {
		LM_ERR("payload too big\n");
		return -1;
	}

	ip = (struct ip *)packet;
	udp = (struct udphdr *)(packet + sizeof(struct ip));
	memcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), buf, buf_len);

	ip->ip_v = 4;
	ip->ip_hl = sizeof(struct ip) / 4; // no options
	ip->ip_tos = 0;
	ip->ip_len = htons(len);
	ip->ip_id = 23;
	ip->ip_off = 0;
	ip->ip_ttl = 69;
	ip->ip_p = 17;
	ip->ip_src.s_addr = s_ip;
	ip->ip_dst.s_addr = to->sin.sin_addr.s_addr;

	ip->ip_sum = raw_checksum((unsigned char *)ip, sizeof(struct ip));

	udp->uh_sport = htons(s_port);
	udp->uh_dport = to->sin.sin_port;
	udp->uh_ulen = htons((unsigned short)sizeof(struct udphdr) + buf_len);
	udp->uh_sum = 0;

	return sendto(raw_sock, packet, len, 0, (struct sockaddr *)to,
			sizeof(struct sockaddr_in));
}

/**
 * quick function to extract ip:port from path
 */
static char *extract_last_path_ip(str path)
{
	/* used for raw UDP ping which works only on IPv4 */
	static char ip[24];
	char *start = NULL, *end = NULL, *p;
	int i;
	int path_depth = 0;
	int max_path_depth;

	max_path_depth = udpping_from_path - 1;

	if(!path.len || !path.s)
		return NULL;

	p = path.s;
	for(i = 0; i < path.len; i++) {
		if(!strncmp("<sip:", p, 5) && i < path.len - 4) {
			start = p + 5;

			end = NULL;
		}
		if((*p == ';' || *p == '>') && !end) {
			end = p;
			if(max_path_depth) {
				path_depth++;
				if(path_depth >= max_path_depth) {
					break;
				}
			}
		}
		p++;
	}
	if(start && end) {
		int len = end - start;
		if(len > sizeof(ip) - 1) {
			return NULL;
		}
		memcpy(ip, start, len);
		ip[len] = '\0';
		return (char *)ip;
	} else {
		return NULL;
	}
}


static void nh_timer(unsigned int ticks, void *timer_idx)
{
	static unsigned int iteration = 0;
	int rval;
	void *buf, *cp;
	str c;
	str recv;
	str *dst_uri;
	str opt;
	str path;
	str ruid;
	unsigned int aorhash;
	struct sip_uri curi;
	struct hostent *he;
	struct socket_info *send_sock;
	unsigned int flags;
	char proto;
	struct dest_info dst;
	char *path_ip_str = NULL;
	unsigned int path_ip = 0;
	unsigned short path_port = 0;
	int options = 0;

	if((*natping_state) == 0)
		goto done;

	buf = NULL;
	if(cblen > 0) {
		buf = pkg_malloc(cblen);
		if(buf == NULL) {
			LM_ERR("out of pkg memory\n");
			goto done;
		}
	}
	if(nh_filter_srvid)
		options |= GAU_OPT_SERVER_ID;
	rval = ul.get_all_ucontacts(buf, cblen, (ping_nated_only ? ul.nat_flag : 0),
			((unsigned int)(unsigned long)timer_idx) * natping_interval
					+ iteration,
			natping_processes * natping_interval, options);
	if(rval < 0) {
		LM_ERR("failed to fetch contacts\n");
		goto done;
	}
	if(rval > 0) {
		if(buf != NULL)
			pkg_free(buf);
		cblen = rval * 2;
		buf = pkg_malloc(cblen);
		if(buf == NULL) {
			LM_ERR("out of pkg memory\n");
			goto done;
		}
		rval = ul.get_all_ucontacts(buf, cblen,
				(ping_nated_only ? ul.nat_flag : 0),
				((unsigned int)(unsigned long)timer_idx) * natping_interval
						+ iteration,
				natping_processes * natping_interval, options);
		if(rval != 0) {
			pkg_free(buf);
			goto done;
		}
	}

	if(buf == NULL)
		goto done;

	cp = buf;
	while(1) {
		memcpy(&(c.len), cp, sizeof(c.len));
		if(c.len == 0)
			break;
		c.s = (char *)cp + sizeof(c.len);
		cp = (char *)cp + sizeof(c.len) + c.len;
		memcpy(&(recv.len), cp, sizeof(recv.len));
		recv.s = (char *)cp + sizeof(recv.len);
		cp = (char *)cp + sizeof(recv.len) + recv.len;
		memcpy(&send_sock, cp, sizeof(send_sock));
		cp = (char *)cp + sizeof(send_sock);
		memcpy(&flags, cp, sizeof(flags));
		cp = (char *)cp + sizeof(flags);
		memcpy(&(path.len), cp, sizeof(path.len));
		path.s = path.len ? ((char *)cp + sizeof(path.len)) : NULL;
		cp = (char *)cp + sizeof(path.len) + path.len;
		memcpy(&(ruid.len), cp, sizeof(ruid.len));
		ruid.s = ruid.len ? ((char *)cp + sizeof(ruid.len)) : NULL;
		cp = (char *)cp + sizeof(ruid.len) + ruid.len;
		memcpy(&aorhash, cp, sizeof(aorhash));
		cp = (char *)cp + sizeof(aorhash);

		if((flags & natping_disable_flag)) /* always 0 if natping_disable_flag not set */
			continue;

		if(recv.len > 0)
			dst_uri = &recv;
		else
			dst_uri = &c;

		/* determin the destination */
		if(path.len && (flags & sipping_flag) != 0) {
			/* send to first URI in path */
			if(get_path_dst_uri(&path, &opt) < 0) {
				LM_ERR("failed to get dst_uri for Path\n");
				continue;
			}
			/* send to the contact/received */
			if(parse_uri(opt.s, opt.len, &curi) < 0) {
				LM_ERR("can't parse contact dst_uri\n");
				continue;
			}
		} else if(path.len && udpping_from_path) {
			path_ip_str = extract_last_path_ip(path);
			if(path_ip_str == NULL) {
				LM_ERR("ERROR:nathelper:nh_timer: unable to parse path from "
					   "location\n");
				continue;
			}
			if(get_natping_socket(path_ip_str, &path_ip, &path_port)) {
				LM_ERR("could not parse path host for udpping_from_path\n");
				continue;
			}
			if(parse_uri(dst_uri->s, dst_uri->len, &curi) < 0) {
				LM_ERR("can't parse contact/received uri\n");
				continue;
			}
		} else {
			/* send to the contact/received */
			if(parse_uri(dst_uri->s, dst_uri->len, &curi) < 0) {
				LM_ERR("can't parse contact/received uri\n");
				continue;
			}
		}
		if(curi.proto != PROTO_UDP && curi.proto != PROTO_NONE)
			continue;
		if(curi.port_no == 0)
			curi.port_no = SIP_PORT;
		proto = curi.proto;
		/* we sholud get rid of this resolve (to ofen and to slow); for the
		 * moment we are lucky since the curi is an IP -bogdan */
		he = sip_resolvehost(&curi.host, &curi.port_no, &proto);
		if(he == NULL) {
			LM_ERR("can't resolve_host\n");
			continue;
		}
		init_dest_info(&dst);
		hostent2su(&dst.to, he, 0, curi.port_no);

		if(force_socket) {
			send_sock = force_socket;
		}
		if(send_sock == 0) {
			send_sock = get_send_socket(0, &dst.to, PROTO_UDP);
		}
		if(send_sock == NULL) {
			LM_ERR("can't get sending socket\n");
			continue;
		}
		dst.proto = PROTO_UDP;
		dst.send_sock = send_sock;

		if((flags & sipping_flag) != 0
				&& (opt.s = build_sipping(
							&c, send_sock, &path, &ruid, aorhash, &opt.len))
						   != 0) {
			if(udp_send(&dst, opt.s, opt.len) < 0) {
				LM_ERR("sip udp_send failed\n");
			}
		} else if(raw_ip) {
			if(send_raw((char *)sbuf, sizeof(sbuf), &dst.to, raw_ip, raw_port)
					< 0) {
				LM_ERR("send_raw failed\n");
			}
		} else if(udpping_from_path) {
			if(send_raw((char *)sbuf, sizeof(sbuf), &dst.to, path_ip, path_port)
					< 0) {
				LM_ERR("send_raw from path failed\n");
			}
		} else {
			if(udp_send(&dst, (char *)sbuf, sizeof(sbuf)) < 0) {
				LM_ERR("udp_send failed\n");
			}
		}
	}
	pkg_free(buf);
done:
	iteration++;
	if(iteration == natping_interval)
		iteration = 0;
}


/*
 * Create received SIP uri that will be either
 * passed to registrar in an AVP or apended
 * to Contact header field as a parameter
 */
static int create_rcv_uri(str *uri, struct sip_msg *m)
{
	return get_src_uri(m, 0, uri);
}


/*
 * Add received parameter to Contacts for further
 * forwarding of the REGISTER requuest
 */
static int ki_add_rcv_param(sip_msg_t *msg, int upos)
{
	contact_t *c;
	struct lump *anchor;
	char *param;
	str uri;
	int hdr_param;

	hdr_param = (upos)?0:1;

	if(create_rcv_uri(&uri, msg) < 0) {
		return -1;
	}

	if(contact_iterator(&c, msg, 0) < 0) {
		return -1;
	}

	while(c) {
		param = (char *)pkg_malloc(RECEIVED_LEN + 2 + uri.len);
		if(!param) {
			LM_ERR("no pkg memory left\n");
			return -1;
		}
		memcpy(param, RECEIVED, RECEIVED_LEN);
		param[RECEIVED_LEN] = '\"';
		memcpy(param + RECEIVED_LEN + 1, uri.s, uri.len);
		param[RECEIVED_LEN + 1 + uri.len] = '\"';

		if(hdr_param) {
			/* add the param as header param */
			anchor = anchor_lump(msg, c->name.s + c->len - msg->buf, 0, 0);
		} else {
			/* add the param as uri param */
			anchor = anchor_lump(msg, c->uri.s + c->uri.len - msg->buf, 0, 0);
		}
		if(anchor == NULL) {
			LM_ERR("anchor_lump failed\n");
			pkg_free(param);
			return -1;
		}

		if(insert_new_lump_after(
				   anchor, param, RECEIVED_LEN + 1 + uri.len + 1, 0)
				== 0) {
			LM_ERR("insert_new_lump_after failed\n");
			pkg_free(param);
			return -1;
		}

		if(contact_iterator(&c, msg, c) < 0) {
			return -1;
		}
	}

	return 1;
}

/*
 * Add received parameter to Contacts for further
 * forwarding of the REGISTER requuest
 */
static int add_rcv_param_f(struct sip_msg *msg, char *str1, char *str2)
{
	int hdr_param = 0;

	if(str1) {
		if(fixup_get_ivalue(msg, (gparam_t*)str1, &hdr_param)<0) {
			LM_ERR("failed to get flags parameter\n");
			return -1;
		}
	}
	return ki_add_rcv_param(msg, hdr_param);
}

/*
 * Create an AVP to be used by registrar with the source IP and port
 * of the REGISTER
 */
static int fix_nated_register(struct sip_msg *msg)
{
	str uri;
	int_str val;

	if(rcv_avp_name.n == 0)
		return 1;

	if(create_rcv_uri(&uri, msg) < 0) {
		return -1;
	}

	val.s = uri;

	if(add_avp(AVP_VAL_STR | rcv_avp_type, rcv_avp_name, val) < 0) {
		LM_ERR("failed to create AVP\n");
		return -1;
	}

	return 1;
}

static int fix_nated_register_f(struct sip_msg *msg, char *str1, char *str2)
{
	return fix_nated_register(msg);
}

/**
 * handle SIP replies
 */
static int nh_sip_reply_received(sip_msg_t *msg)
{
	to_body_t *fb;
	str ruid;
	str ah;
	unsigned int aorhash;
	char *p;

	if(nh_keepalive_timeout <= 0)
		return 1;
	if(msg->cseq == NULL && ((parse_headers(msg, HDR_CSEQ_F, 0) == -1)
									|| (msg->cseq == NULL))) {
		LM_ERR("no CSEQ header\n");
		goto done;
	}
	if(sipping_method_id != METHOD_UNDEF && sipping_method_id != METHOD_OTHER) {
		if(get_cseq(msg)->method_id != sipping_method_id)
			goto done;
	} else {
		if(sipping_method_id == METHOD_OTHER) {
			if(get_cseq(msg)->method.len != sipping_method.len)
				goto done;
			if(strncmp(get_cseq(msg)->method.s, sipping_method.s,
					   sipping_method.len)
					!= 0)
				goto done;
		} else {
			goto done;
		}
	}
	/* there must be no second via */
	if(!(parse_headers(msg, HDR_VIA2_F, 0) == -1 || (msg->via2 == 0)
			   || (msg->via2->error != PARSE_OK)))
		goto done;

	/* from uri check */
	if((parse_from_header(msg)) < 0) {
		LM_ERR("cannot parse From header\n");
		goto done;
	}

	fb = get_from(msg);
	if(fb->uri.len != sipping_from.len
			|| strncmp(fb->uri.s, sipping_from.s, sipping_from.len) != 0)
		goto done;

	/* from-tag is: ruid-aorhash-counter */
	if(fb->tag_value.len <= 0)
		goto done;

	LM_DBG("checking nathelper keepalive reply [%.*s]\n", fb->tag_value.len,
			fb->tag_value.s);

	/* skip counter */
	p = q_memrchr(fb->tag_value.s, '-', fb->tag_value.len);
	if(p == NULL) {
		LM_DBG("from tag format mismatch [%.*s]\n", fb->tag_value.len,
				fb->tag_value.s);
		goto done;
	}
	/* aor hash */
	ah.len = p - fb->tag_value.s;
	aorhash = 0;
	p = q_memrchr(fb->tag_value.s, '-', ah.len);
	if(p == NULL) {
		LM_DBG("from tag format mismatch [%.*s]!\n", fb->tag_value.len,
				fb->tag_value.s);
		goto done;
	}
	ah.s = p + 1;
	ah.len = fb->tag_value.s + ah.len - ah.s;

	LM_DBG("aor hash string is [%.*s] (%d)\n", ah.len, ah.s, ah.len);

	if(ah.len <= 0 || reverse_hex2int(ah.s, ah.len, &aorhash) < 0) {
		LM_DBG("cannot get aor hash in [%.*s]\n", fb->tag_value.len,
				fb->tag_value.s);
		goto done;
	}
	LM_DBG("aor hash is [%u] string [%.*s]\n", aorhash, ah.len, ah.s);

	ruid.s = fb->tag_value.s;
	ruid.len = ah.s - ruid.s - 1;

	if(ruid.len <= 0) {
		LM_DBG("cannot get ruid in [%.*s]\n", fb->tag_value.len,
				fb->tag_value.s);
		goto done;
	}

	LM_DBG("reply for keepalive of [%.*s:%u]\n", ruid.len, ruid.s, aorhash);

	ul.refresh_keepalive(aorhash, &ruid);
done:
	/* let the core handle further the reply */
	return 1;
}

static int sel_rewrite_contact(str *res, select_t *s, struct sip_msg *msg)
{
	static char buf[500];
	contact_t *c;
	int n, def_port_fl, len;
	char *cp;
	str hostport;
	struct sip_uri uri;

	res->len = 0;
	n = s->params[2].v.i;
	if(n <= 0) {
		LM_ERR("rewrite contact[%d] - zero or negative index not supported\n",
				n);
		return -1;
	}
	c = 0;
	do {
		if(contact_iterator(&c, msg, c) < 0 || !c)
			return -1;
		n--;
	} while(n > 0);

	if(parse_uri(c->uri.s, c->uri.len, &uri) < 0 || uri.host.len <= 0) {
		LM_ERR("rewrite contact[%d] - error while parsing Contact URI\n",
				s->params[2].v.i);
		return -1;
	}
	len = c->len - uri.host.len;
	if(uri.port.len > 0)
		len -= uri.port.len;
	def_port_fl =
			(msg->rcv.proto == PROTO_TLS && msg->rcv.src_port == SIPS_PORT)
			|| (msg->rcv.proto != PROTO_TLS && msg->rcv.src_port == SIP_PORT);
	if(!def_port_fl)
		len += 1 /*:*/ + 5 /*port*/;
	if(len > sizeof(buf)) {
		LM_ERR("rewrite contact[%d] - contact too long\n", s->params[2].v.i);
		return -1;
	}
	hostport = uri.host;
	if(uri.port.len > 0)
		hostport.len = uri.port.s + uri.port.len - uri.host.s;

	res->s = buf;
	res->len = hostport.s - c->name.s;
	memcpy(buf, c->name.s, res->len);
	cp = ip_addr2a(&msg->rcv.src_ip);
	if(def_port_fl) {
		res->len += snprintf(buf + res->len, sizeof(buf) - res->len, "%s", cp);
	} else {
		res->len += snprintf(buf + res->len, sizeof(buf) - res->len, "%s:%d",
				cp, msg->rcv.src_port);
	}
	memcpy(buf + res->len, hostport.s + hostport.len,
			c->len - (hostport.s + hostport.len - c->name.s));
	res->len += c->len - (hostport.s + hostport.len - c->name.s);

	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_nathelper_exports[] = {
	{ str_init("nathelper"), str_init("nat_uac_test"),
		SR_KEMIP_INT, nat_uac_test,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nathelper"), str_init("fix_nated_contact"),
		SR_KEMIP_INT, fix_nated_contact,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nathelper"), str_init("fix_nated_register"),
		SR_KEMIP_INT, fix_nated_register,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nathelper"), str_init("set_contact_alias"),
		SR_KEMIP_INT, set_contact_alias,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nathelper"), str_init("handle_ruri_alias"),
		SR_KEMIP_INT, handle_ruri_alias,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nathelper"), str_init("is_rfc1918"),
		SR_KEMIP_INT, is_rfc1918,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nathelper"), str_init("add_contact_alias"),
		SR_KEMIP_INT, add_contact_alias_0,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nathelper"), str_init("add_contact_alias_addr"),
		SR_KEMIP_INT, add_contact_alias_3,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nathelper"), str_init("add_rcv_param"),
		SR_KEMIP_INT, ki_add_rcv_param,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_nathelper_exports);
	return 0;
}
