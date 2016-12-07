/* $Id$
 *
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * ---------
 * 2003-10-09	nat_uac_test introduced (jiri)
 *
 * 2003-11-06   nat_uac_test permitted from onreply_route (jiri)
 *
 * 2003-12-01   unforce_rtp_proxy introduced (sobomax)
 *
 * 2004-01-07	RTP proxy support updated to support new version of the
 *		RTP proxy (20040107).
 *
 *		force_rtp_proxy() now inserts a special flag
 *		into the SDP body to indicate that this session already
 *		proxied and ignores sessions with such flag.
 *
 *		Added run-time check for version of command protocol
 *		supported by the RTP proxy.
 *
 * 2004-01-16   Integrated slightly modified patch from Tristan Colgate,
 *		force_rtp_proxy function with IP as a parameter (janakj)
 *
 * 2004-01-28	nat_uac_test extended to allow testing SDP body (sobomax)
 *
 *		nat_uac_test extended to allow testing top Via (sobomax)
 *
 * 2004-02-21	force_rtp_proxy now accepts option argument, which
 *		consists of string of chars, each of them turns "on"
 *		some feature, currently supported ones are:
 *
 *		 `a' - flags that UA from which message is received
 *		       doesn't support symmetric RTP;
 *		 `l' - force "lookup", that is, only rewrite SDP when
 *		       corresponding session is already exists in the
 *		       RTP proxy. Only makes sense for SIP requests,
 *		       replies are always processed in "lookup" mode;
 *		 `i' - flags that message is received from UA in the
 *		       LAN. Only makes sense when RTP proxy is running
 *		       in the bridge mode.
 *
 *		force_rtp_proxy can now be invoked without any arguments,
 *		as previously, with one argument - in this case argument
 *		is treated as option string and with two arguments, in
 *		which case 1st argument is option string and the 2nd
 *		one is IP address which have to be inserted into
 *		SDP (IP address on which RTP proxy listens).
 *
 * 2004-03-12	Added support for IPv6 addresses in SDPs. Particularly,
 *		force_rtp_proxy now can work with IPv6-aware RTP proxy,
 *		replacing IPv4 address in SDP with IPv6 one and vice versa.
 *		This allows creating full-fledged IPv4<->IPv6 gateway.
 *		See 4to6.cfg file for example.
 *
 *		Two new options added into force_rtp_proxy:
 *
 *		 `f' - instructs nathelper to ignore marks inserted
 *		       by another nathelper in transit to indicate
 *		       that the session is already goes through another
 *		       proxy. Allows creating chain of proxies.
 *		 `r' - flags that IP address in SDP should be trusted.
 *		       Without this flag, nathelper ignores address in the
 *		       SDP and uses source address of the SIP message
 *		       as media address which is passed to the RTP proxy.
 *
 *		Protocol between nathelper and RTP proxy in bridge
 *		mode has been slightly changed. Now RTP proxy expects SER
 *		to provide 2 flags when creating or updating session
 *		to indicate direction of this session. Each of those
 *		flags can be either `e' or `i'. For example `ei' means
 *		that we received INVITE from UA on the "external" network
 *		network and will send it to the UA on "internal" one.
 *		Also possible `ie' (internal->external), `ii'
 *		(internal->internal) and `ee' (external->external). See
 *		example file alg.cfg for details.
 *
 * 2004-03-15	If the rtp proxy test failed (wrong version or not started)
 *		retry test from time to time, when some *rtpproxy* function
 *		is invoked. Minimum interval between retries can be
 *		configured via rtpproxy_disable_tout module parameter (default
 *		is 60 seconds). Setting it to -1 will disable periodic
 *		rechecks completely, setting it to 0 will force checks
 *		for each *rtpproxy* function call. (andrei)
 *
 * 2004-03-22	Fix assignment of rtpproxy_retr and rtpproxy_tout module
 *		parameters.
 *
 * 2004-03-22	Fix get_body position (should be called before get_callid)
 * 				(andrei)
 *
 * 2004-03-24	Fix newport for null ip address case (e.g onhold re-INVITE)
 * 				(andrei)
 *
 * 2004-09-30	added received port != via port test (andrei)
 *
 * 2004-10-10   force_socket option introduced (jiri)
 *
 * 2005-02-24	Added support for using more than one rtp proxy, in which
 *		case traffic will be distributed evenly among them. In addition,
 *		each such proxy can be assigned a weight, which will specify
 *		which share of the traffic should be placed to this particular
 *		proxy.
 *
 *		Introduce failover mechanism, so that if SER detects that one
 *		of many proxies is no longer available it temporarily decreases
 *		its weight to 0, so that no traffic will be assigned to it.
 *		Such "disabled" proxies are periodically checked to see if they
 *		are back to normal in which case respective weight is restored
 *		resulting in traffic being sent to that proxy again.
 *
 *		Those features can be enabled by specifying more than one "URI"
 *		in the rtpproxy_sock parameter, optionally followed by the weight,
 *		which if absent is assumed to be 1, for example:
 *
 *		rtpproxy_sock="unix:/foo/bar=4 udp:1.2.3.4:3456=3 udp:5.6.7.8:5432=1"
 *
 * 2005-02-25	Force for pinging the socket returned by USRLOC (bogdan)
 *
 * 2005-03-22	Support for multiple media streams added (netch)
 *
 * 2005-04-27	Support for doing natpinging using real SIP requests added.
 *		Requires tm module for doing its work. Old method (sending UDP
 *		with 4 zero bytes can be selected by specifying natping_method="null".
 *
 * 2005-12-23	Support for selecting particular RTP proxy node has been added.
 *		In force_rtp_proxy() it can be done via new N modifier, followed
 *		by the index (starting at 0) of the node in the rtpproxy_sock
 *		parameter. For example, in the example above force_rtp_proxy("N1") will
 *		will select node udp:1.2.3.4:3456. In unforce_rtp_proxy(), the same
 *		can be done by specifying index as an argument directly, i.e.
 *		unforce_rtp_proxy(1).
 *
 *		Since nathelper is not transaction or call stateful, care should be
 *		taken to ensure that force_rtp_proxy() in request path matches
 *		force_rtp_proxy() in reply path, that is the same node is selected.
 *
 * 2006-06-10	Select nathepler.rewrite_contact
 *		pingcontact function (tma)
 *
 * 2007-04-23	Do NAT pinging in the separate dedicated process. It provides much
 *		better scalability than doing it in the main one.
 *
 * 2007-04-23	New function start_recording() allowing to start recording RTP
 *		session in the RTP proxy.
 *
 * 2007-08-28	natping_crlf option was introduced (jiri)
 *
 * 2007-09-12	Added stateless sip natping (andrei)
 *
 * 2007-12-10	IP address in the session header is now updated along with
 *		address(es) in media description (andrei).
 *
 *		force_rtp_proxy() now accepts zNNN parameter and passes it to the
 *		RTP proxy.
 *
 *		New functions rtpproxy_offer() and rtpproxy_answer() to allow
 *		using RTP proxy in the cases when SDP offer is in 2xx and SDP
 *		answer in ACK.
 *
 */

#include "nhelpr_funcs.h"
#include "nathelper.h"
#include "../../flags.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../forward.h"
#include "../../mem/mem.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parser_f.h"
#include "../../resolve.h"
#include "../../timer.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../registrar/sip_msg.h"
#include "../../msg_translator.h"
#include "../usrloc/usrloc.h"
#include "../../usr_avp.h"
#include "../../socket_info.h"
#include "../../select.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
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

MODULE_VERSION

#if !defined(AF_LOCAL)
#define	AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define	PF_LOCAL PF_UNIX
#endif

/* NAT UAC test constants */
#define	NAT_UAC_TEST_C_1918	0x01
#define	NAT_UAC_TEST_RCVD	0x02
#define	NAT_UAC_TEST_V_1918	0x04
#define	NAT_UAC_TEST_S_1918	0x08
#define	NAT_UAC_TEST_RPORT	0x10
#define	NAT_UAC_TEST_C_PORT	0x20


static int nat_uac_test_f(struct sip_msg* msg, char* str1, char* str2);
static int fix_nated_contact_f(struct sip_msg *, char *, char *);
static int fix_nated_sdp_f(struct sip_msg *, char *, char *);
static int extract_mediaip(str *, str *, int *);
static int alter_mediaip(struct sip_msg *, str *, str *, int, str *, int, int);
static int fix_nated_register_f(struct sip_msg *, char *, char *);
static int fixup_ping_contact(void **param, int param_no);
static int ping_contact_f(struct sip_msg* msg, char* str1, char* str2);

static int mod_init(void);
static void mod_cleanup(void);
static int child_init(int);

struct socket_info* force_socket = 0;


static struct {
	const char *cnetaddr;
	uint32_t netaddr;
	uint32_t mask;
} nets_1918[] = {
	{"10.0.0.0",    0, 0xffffffffu << 24},
	{"172.16.0.0",  0, 0xffffffffu << 20},
	{"192.168.0.0", 0, 0xffffffffu << 16},
	{NULL, 0, 0}
};

static char *force_socket_str = 0;

static cmd_export_t cmds[] = {
	{"fix_nated_contact",  fix_nated_contact_f,    0, NULL,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"fix_nated_sdp",      fix_nated_sdp_f,        1, fixup_int_1, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"nat_uac_test",       nat_uac_test_f,         1, fixup_int_1, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"fix_nated_register", fix_nated_register_f,   0, NULL,             REQUEST_ROUTE },
	{"ping_contact",       ping_contact_f,         1, fixup_ping_contact,  REQUEST_ROUTE },
	{0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"natping_interval",      PARAM_INT,    &natping_interval      },
	{"natping_crlf",      	  PARAM_INT,    &natping_crlf	       },
	{"natping_method",        PARAM_STRING, &natping_method        },
	{"natping_stateful",      PARAM_INT,    &natping_stateful      },
	{"ping_nated_only",       PARAM_INT,    &ping_nated_only       },
	{"force_socket",          PARAM_STRING, &force_socket_str      },
	{0, 0, 0}
};

struct module_exports exports = {
	"nathelper",
	cmds,
	0,       /* RPC methods */
	params,
	mod_init,
	intercept_ping_reply, /* reply processing */
	mod_cleanup, /* destroy function */
	0, /* on_break */
	child_init
};

static int
sel_nathelper(str* res, select_t* s, struct sip_msg* msg)
{

	/* dummy */
	return 0;
}

static int sel_rewrite_contact(str* res, select_t* s, struct sip_msg* msg);

SELECT_F(select_any_nameaddr)

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("nathelper"), sel_nathelper, SEL_PARAM_EXPECTED},
	{ sel_nathelper, SEL_PARAM_STR, STR_STATIC_INIT("rewrite_contact"), sel_rewrite_contact, CONSUME_NEXT_INT },

	{ sel_rewrite_contact, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR},

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

static int
mod_init(void)
{
	int i;
	struct in_addr addr;
	str socket_str;

	if (force_socket_str != NULL) {
		socket_str.s = force_socket_str;
		socket_str.len = strlen(socket_str.s);
		force_socket = grep_sock_info(&socket_str, 0, 0);
	}

	if (natpinger_init() < 0) {
		LOG(L_ERR, "nathelper: natpinger_init() failed\n");
		return -1;
	}

	/* Prepare 1918 networks list */
	for (i = 0; nets_1918[i].cnetaddr != NULL; i++) {
		if (inet_aton(nets_1918[i].cnetaddr, &addr) != 1)
			abort();
		nets_1918[i].netaddr = ntohl(addr.s_addr) & nets_1918[i].mask;
	}

	register_select_table(sel_declaration);
	return 0;
}

static void
mod_cleanup(void)
{

	natpinger_cleanup();
}

static int
child_init(int rank)
{
	if (natpinger_child_init(rank) < 0)
		return -1;

	return 0;
}

static int
isnulladdr(str *sx, int pf)
{
	char *cp;

	if (pf == AF_INET6) {
		for(cp = sx->s; cp < sx->s + sx->len; cp++)
			if (*cp != '0' && *cp != ':')
				return 0;
		return 1;
	}
	return (sx->len == 7 && memcmp("0.0.0.0", sx->s, 7) == 0);
}

/*
 * Replaces ip:port pair in the Contact: field with the source address
 * of the packet.
 */
static int
fix_nated_contact_f(struct sip_msg* msg, char* str1, char* str2)
{
	int offset, len, len1;
	char *cp, *buf, temp[2];
	contact_t *c;
	struct lump *anchor;
	struct sip_uri uri;
	str hostport;

	if (get_contact_uri(msg, &uri, &c) == -1)
		return -1;
	/* for UAs behind NAT we have to hope that they will reuse the
	 * TCP connection, otherwise they are lost any way. So this check
	 * does not make too much sense.
	if (uri.proto != PROTO_UDP && uri.proto != PROTO_NONE)
		return -1;
	*/
	if ((c->uri.s < msg->buf) || (c->uri.s > (msg->buf + msg->len))) {
		LOG(L_ERR, "ERROR: you can't call fix_nated_contact twice, "
		    "check your config!\n");
		return -1;
	}

	offset = c->uri.s - msg->buf;
	anchor = del_lump(msg, offset, c->uri.len, HDR_CONTACT_T);
	if (anchor == 0)
		return -1;

	hostport = uri.host;
	if (uri.port.len > 0)
		hostport.len = uri.port.s + uri.port.len - uri.host.s;

	cp = ip_addr2a(&msg->rcv.src_ip);
	len = c->uri.len + strlen(cp) + 6 /* :port */ - hostport.len + 1;
	buf = pkg_malloc(len);
	if (buf == NULL) {
		LOG(L_ERR, "ERROR: fix_nated_contact: out of memory\n");
		return -1;
	}
	temp[0] = hostport.s[0];
	temp[1] = c->uri.s[c->uri.len];
	c->uri.s[c->uri.len] = hostport.s[0] = '\0';
	len1 = snprintf(buf, len, "%s%s:%d%s", c->uri.s, cp, msg->rcv.src_port,
	    hostport.s + hostport.len);
	if (len1 < len)
		len = len1;
	hostport.s[0] = temp[0];
	c->uri.s[c->uri.len] = temp[1];
	if (insert_new_lump_after(anchor, buf, len, HDR_CONTACT_T) == 0) {
		pkg_free(buf);
		return -1;
	}
	c->uri.s = buf;
	c->uri.len = len;

	return 1;
}

static int
sel_rewrite_contact(str* res, select_t* s, struct sip_msg* msg)
{
	static char buf[500];
	contact_t* c;
	int n, def_port_fl, len;
	char *cp;
	str hostport;
	struct sip_uri uri;

	res->len = 0;
	n = s->params[2].v.i;
	if (n <= 0) {
		LOG(L_ERR, "ERROR: rewrite_contact[%d]: zero or negative index not supported\n", n);
		return -1;
	}
	c = 0;
	do {
		if (contact_iterator(&c, msg, c) < 0 || !c)
			return -1;
		n--;
	} while (n > 0);

	if (parse_uri(c->uri.s, c->uri.len, &uri) < 0 || uri.host.len <= 0) {
		LOG(L_ERR, "rewrite_contact[%d]: Error while parsing Contact URI\n", s->params[2].v.i);
		return -1;
	}
	len = c->len - uri.host.len;
	if (uri.port.len > 0)
		len -= uri.port.len;
	def_port_fl = (msg->rcv.proto == PROTO_TLS && msg->rcv.src_port == SIPS_PORT) || (msg->rcv.proto != PROTO_TLS && msg->rcv.src_port == SIP_PORT);
	if (!def_port_fl)
		len += 1/*:*/+5/*port*/;
	if (len > sizeof(buf)) {
		LOG(L_ERR, "ERROR: rewrite_contact[%d]: contact too long\n", s->params[2].v.i);
		return -1;
	}
	hostport = uri.host;
	if (uri.port.len > 0)
		hostport.len = uri.port.s + uri.port.len - uri.host.s;

	res->s = buf;
	res->len = hostport.s - c->name.s;
	memcpy(buf, c->name.s, res->len);
	cp = ip_addr2a(&msg->rcv.src_ip);
	if (def_port_fl) {
		res->len+= snprintf(buf+res->len, sizeof(buf)-res->len, "%s", cp);
	} else {
		res->len+= snprintf(buf+res->len, sizeof(buf)-res->len, "%s:%d", cp, msg->rcv.src_port);
	}
	memcpy(buf+res->len, hostport.s+hostport.len, c->len-(hostport.s+hostport.len-c->name.s));
	res->len+= c->len-(hostport.s+hostport.len-c->name.s);

	return 0;
}

/*
 * Test if IP address pointed to by saddr belongs to RFC1918 networks
 */
static inline int
is1918addr(str *saddr)
{
	struct in_addr addr;
	uint32_t netaddr;
	int i, rval;
	char backup;

	rval = -1;
	backup = saddr->s[saddr->len];
	saddr->s[saddr->len] = '\0';
	if (inet_aton(saddr->s, &addr) != 1)
		goto theend;
	netaddr = ntohl(addr.s_addr);
	for (i = 0; nets_1918[i].cnetaddr != NULL; i++) {
		if ((netaddr & nets_1918[i].mask) == nets_1918[i].netaddr) {
			rval = 1;
			goto theend;
		}
	}
	rval = 0;

theend:
	saddr->s[saddr->len] = backup;
	return rval;
}

/*
 * test for occurrence of RFC1918 IP address in Contact HF
 */
static int
contact_1918(struct sip_msg* msg)
{
	struct sip_uri uri;
	contact_t* c;

	if (get_contact_uri(msg, &uri, &c) == -1)
		return -1;

	return (is1918addr(&(uri.host)) == 1) ? 1 : 0;
}

static int
contact_rport(struct sip_msg* msg)
{
	struct sip_uri uri;
	contact_t* c;

	if (get_contact_uri(msg, &uri, &c) == -1) {
		return -1;
	}

	if (msg->rcv.src_port != (uri.port_no ? uri.port_no : SIP_PORT)) {
		return 1;
	} else {
		return 0;
	}
}

/*
 * test for occurrence of RFC1918 IP address in SDP
 */
static int
sdp_1918(struct sip_msg* msg)
{
	str body, ip;
	int pf;

	if (extract_body(msg, &body) == -1) {
		LOG(L_ERR,"ERROR: sdp_1918: cannot extract body from msg!\n");
		return 0;
	}
	if (extract_mediaip(&body, &ip, &pf) == -1) {
		LOG(L_ERR, "ERROR: sdp_1918: can't extract media IP from the SDP\n");
		return 0;
	}
	if (pf != AF_INET || isnulladdr(&ip, pf))
		return 0;

	return (is1918addr(&ip) == 1) ? 1 : 0;
}

/*
 * test for occurrence of RFC1918 IP address in top Via
 */
static int
via_1918(struct sip_msg* msg)
{

	return (is1918addr(&(msg->via1->host)) == 1) ? 1 : 0;
}

static int
nat_uac_test_f(struct sip_msg* msg, char* str1, char* str2)
{
	int tests;

	if (get_int_fparam(&tests, msg, (fparam_t*) str1) < 0)
		return -1;

	/* return true if any of the NAT-UAC tests holds */

	/* test if the source port is different from the port in Via */
	if ((tests & NAT_UAC_TEST_RPORT) &&
	    (msg->rcv.src_port != (msg->via1->port ? msg->via1->port : SIP_PORT))) {
		return 1;
	}
	/*
	 * test if source address of signaling is different from
	 * address advertised in Via
	 */
	if ((tests & NAT_UAC_TEST_RCVD) && received_test(msg))
		return 1;
	/*
	 * test for occurrences of RFC1918 addresses in Contact
	 * header field
	 */
	if ((tests & NAT_UAC_TEST_C_1918) && (contact_1918(msg) > 0))
		return 1;
	/*
	 * test for occurrences of RFC1918 addresses in SDP body
	 */
	if ((tests & NAT_UAC_TEST_S_1918) && sdp_1918(msg))
		return 1;
	/*
	 * test for occurrences of RFC1918 addresses top Via
	 */
	if ((tests & NAT_UAC_TEST_V_1918) && via_1918(msg))
		return 1;
	/*
	 * test if source port of signaling is different from
	 * port advertised in Contact
	 */
	if ((tests & NAT_UAC_TEST_C_PORT) && (contact_rport(msg) > 0))
		return 1;

	/* no test succeeded */
	return -1;

}

#define	ADD_ADIRECTION	0x01
#define	FIX_MEDIP	0x02
#define	ADD_ANORTPPROXY	0x04
#define ADD_ADIRECTIONP 0x08

#define	ADIRECTION	"a=direction:active"
#define	ADIRECTION_LEN	(sizeof(ADIRECTION) - 1)

#define ADIRECTIONP     "a=direction:passive"
#define ADIRECTIONP_LEN (sizeof(ADIRECTIONP) - 1)

#define	AOLDMEDIP	"a=oldmediaip:"
#define	AOLDMEDIP_LEN	(sizeof(AOLDMEDIP) - 1)

#define	AOLDMEDIP6	"a=oldmediaip6:"
#define	AOLDMEDIP6_LEN	(sizeof(AOLDMEDIP6) - 1)

#define	AOLDMEDPRT	"a=oldmediaport:"
#define	AOLDMEDPRT_LEN	(sizeof(AOLDMEDPRT) - 1)

#define	ANORTPPROXY	"a=nortpproxy:yes"
#define	ANORTPPROXY_LEN	(sizeof(ANORTPPROXY) - 1)

static int
fix_nated_sdp_f(struct sip_msg* msg, char* str1, char* str2)
{
	str body, body1, oldip, newip;
	int level, pf;
	char *buf;
	struct lump* anchor;

	if (get_int_fparam(&level, msg, (fparam_t*) str1) < 0)
		return -1;
	
	if (extract_body(msg, &body) == -1) {
		LOG(L_ERR,"ERROR: fix_nated_sdp: cannot extract body from msg!\n");
		return -1;
	}

	if (level & (ADD_ADIRECTION | ADD_ANORTPPROXY | ADD_ADIRECTIONP)) {
		msg->msg_flags |= FL_FORCE_ACTIVE;
		anchor = anchor_lump(msg, body.s + body.len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: anchor_lump failed\n");
			return -1;
		}
		if (level & ADD_ADIRECTION) {
			buf = pkg_malloc((ADIRECTION_LEN + CRLF_LEN) * sizeof(char));
			if (buf == NULL) {
				LOG(L_ERR, "ERROR: fix_nated_sdp: out of memory\n");
				return -1;
			}
			memcpy(buf, CRLF, CRLF_LEN);
			memcpy(buf + CRLF_LEN, ADIRECTION, ADIRECTION_LEN);
			if (insert_new_lump_after(anchor, buf, ADIRECTION_LEN + CRLF_LEN, 0) == NULL) {
				LOG(L_ERR, "ERROR: fix_nated_sdp: insert_new_lump_after failed\n");
				pkg_free(buf);
				return -1;
			}
		}
		if (level & ADD_ADIRECTIONP) {
			buf = pkg_malloc((ADIRECTIONP_LEN + CRLF_LEN) * sizeof(char));
			if (buf == NULL) {
				LOG(L_ERR, "ERROR: fix_nated_sdp: out of memory\n");
				return -1;
			}
			memcpy(buf, CRLF, CRLF_LEN);
			memcpy(buf + CRLF_LEN, ADIRECTIONP, ADIRECTIONP_LEN);
			if (insert_new_lump_after(anchor, buf, ADIRECTIONP_LEN + CRLF_LEN, 0) == NULL) {
				LOG(L_ERR, "ERROR: fix_nated_sdp: insert_new_lump_after failed\n");
				pkg_free(buf);
				return -1;
			}
		}
		if (level & ADD_ANORTPPROXY) {
			buf = pkg_malloc((ANORTPPROXY_LEN + CRLF_LEN) * sizeof(char));
			if (buf == NULL) {
				LOG(L_ERR, "ERROR: fix_nated_sdp: out of memory\n");
				return -1;
			}
			memcpy(buf, CRLF, CRLF_LEN);
			memcpy(buf + CRLF_LEN, ANORTPPROXY, ANORTPPROXY_LEN);
			if (insert_new_lump_after(anchor, buf, ANORTPPROXY_LEN + CRLF_LEN, 0) == NULL) {
				LOG(L_ERR, "ERROR: fix_nated_sdp: insert_new_lump_after failed\n");
				pkg_free(buf);
				return -1;
			}
		}
	}

	if (level & FIX_MEDIP) {
 		/* Iterate all c= and replace ips in them. */
 		unsigned hasreplaced = 0;
 		int pf1 = 0;
 		str body2;
 		char *bodylimit = body.s + body.len;
 		newip.s = ip_addr2a(&msg->rcv.src_ip);
 		newip.len = strlen(newip.s);
 		body1 = body;
 		for(;;) {
 			if (extract_mediaip(&body1, &oldip, &pf) == -1)
 				break;
 			if (pf != AF_INET) {
 				LOG(L_ERR, "ERROR: fix_nated_sdp: "
 				    "not an IPv4 address in SDP\n");
 				goto finalize;
 			}
 			if (!pf1)
 				pf1 = pf;
 			else if (pf != pf1) {
 				LOG(L_ERR, "ERROR: fix_nated_sdp: mismatching "
 				    "address families in SDP\n");
 				return -1;
 			}
 			body2.s = oldip.s + oldip.len;
 			body2.len = bodylimit - body2.s;
 			if (alter_mediaip(msg, &body1, &oldip, pf, &newip, pf,
 			    1) == -1)
 			{
 				LOG(L_ERR, "ERROR: fix_nated_sdp: can't alter media IP\n");
 				return -1;
 			}
 			hasreplaced = 1;
 			body1 = body2;
 		}
 		if (!hasreplaced) {
 			LOG(L_ERR, "ERROR: fix_nated_sdp: can't extract media IP from the SDP\n");
 			goto finalize;
 		}
	}

finalize:
	return 1;
}

static int
extract_mediaip(str *body, str *mediaip, int *pf)
{
	char *cp, *cp1;
	int len, nextisip;

	cp1 = NULL;
	for (cp = body->s; (len = body->s + body->len - cp) > 0;) {
		cp1 = ser_memmem(cp, "c=", len, 2);
		if (cp1 == NULL || cp1[-1] == '\n' || cp1[-1] == '\r')
			break;
		cp = cp1 + 2;
	}
	if (cp1 == NULL) {
		LOG(L_ERR, "ERROR: extract_mediaip: no `c=' in SDP\n");
		return -1;
	}
	mediaip->s = cp1 + 2;
	mediaip->len = eat_line(mediaip->s, body->s + body->len - mediaip->s) - mediaip->s;
	trim_len(mediaip->len, mediaip->s, *mediaip);

	nextisip = 0;
	for (cp = mediaip->s; cp < mediaip->s + mediaip->len;) {
		len = eat_token_end(cp, mediaip->s + mediaip->len) - cp;
		if (nextisip == 1) {
			mediaip->s = cp;
			mediaip->len = len;
			nextisip++;
			break;
		}
		if (len == 3 && memcmp(cp, "IP", 2) == 0) {
			switch (cp[2]) {
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
	if (nextisip != 2 || mediaip->len == 0) {
		LOG(L_ERR, "ERROR: extract_mediaip: "
		    "no `IP[4|6]' in `c=' field\n");
		return -1;
	}
	return 1;
}

static int
alter_mediaip(struct sip_msg *msg, str *body, str *oldip, int oldpf,
  str *newip, int newpf, int preserve)
{
	char *buf;
	int offset;
	struct lump* anchor;
	str omip, nip, oip;

	/* check that updating mediaip is really necessary */
	if (oldpf == newpf && isnulladdr(oldip, oldpf))
		return 0;
	if (newip->len == oldip->len &&
	    memcmp(newip->s, oldip->s, newip->len) == 0)
		return 0;

	/*
	 * Since rewriting the same info twice will mess SDP up,
	 * apply simple anti foot shooting measure - put flag on
	 * messages that have been altered and check it when
	 * another request comes.
	 */
#if 0
	/* disabled:
	 *  - alter_mediaip is called twice if 2 c= lines are present
	 *    in the sdp (and we want to allow it)
	 *  - the message flags are propagated in the on_reply_route
	 *  => if we set the flags for the request they will be seen for the
	 *    reply too, but we don't want that
	 *  --andrei
	 */
	if (msg->msg_flags & FL_SDP_IP_AFS) {
		LOG(L_ERR, "ERROR: alter_mediaip: you can't rewrite the same "
		  "SDP twice, check your config!\n");
		return -1;
	}
#endif

	if (preserve != 0) {
		anchor = anchor_lump(msg, body->s + body->len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaip: anchor_lump failed\n");
			return -1;
		}
		if (oldpf == AF_INET6) {
			omip.s = AOLDMEDIP6;
			omip.len = AOLDMEDIP6_LEN;
		} else {
			omip.s = AOLDMEDIP;
			omip.len = AOLDMEDIP_LEN;
		}
		buf = pkg_malloc(omip.len + oldip->len + CRLF_LEN);
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaip: out of memory\n");
			return -1;
		}
		memcpy(buf, CRLF, CRLF_LEN);
		memcpy(buf + CRLF_LEN, omip.s, omip.len);
		memcpy(buf + CRLF_LEN + omip.len, oldip->s, oldip->len);
		if (insert_new_lump_after(anchor, buf,
		    omip.len + oldip->len + CRLF_LEN, 0) == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaip: insert_new_lump_after failed\n");
			pkg_free(buf);
			return -1;
		}
	}

	if (oldpf == newpf) {
		nip.len = newip->len;
		nip.s = pkg_malloc(nip.len);
		if (nip.s == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaip: out of memory\n");
			return -1;
		}
		memcpy(nip.s, newip->s, newip->len);
	} else {
		nip.len = newip->len + 2;
		nip.s = pkg_malloc(nip.len);
		if (nip.s == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaip: out of memory\n");
			return -1;
		}
		memcpy(nip.s + 2, newip->s, newip->len);
		nip.s[0] = (newpf == AF_INET6) ? '6' : '4';
		nip.s[1] = ' ';
	}

	oip = *oldip;
	if (oldpf != newpf) {
		do {
			oip.s--;
			oip.len++;
		} while (*oip.s != '6' && *oip.s != '4');
	}
	offset = oip.s - msg->buf;
	anchor = del_lump(msg, offset, oip.len, 0);
	if (anchor == NULL) {
		LOG(L_ERR, "ERROR: alter_mediaip: del_lump failed\n");
		pkg_free(nip.s);
		return -1;
	}

#if 0
	msg->msg_flags |= FL_SDP_IP_AFS;
#endif

	if (insert_new_lump_after(anchor, nip.s, nip.len, 0) == 0) {
		LOG(L_ERR, "ERROR: alter_mediaip: insert_new_lump_after failed\n");
		pkg_free(nip.s);
		return -1;
	}
	return 0;
}

/*
 * Auxiliary for some functions.
 * Returns pointer to first character of found line, or NULL if no such line.
 */

#define DSTIP_PARAM ";dstip="
#define DSTIP_PARAM_LEN (sizeof(DSTIP_PARAM) - 1)

#define DSTPORT_PARAM ";dstport="
#define DSTPORT_PARAM_LEN (sizeof(DSTPORT_PARAM) - 1)


/*
 * Create received SIP uri that will be either
 * passed to registrar in an AVP or apended
 * to Contact header field as a parameter
 */
static int
create_rcv_uri(str* uri, struct sip_msg* m)
{
	static char buf[MAX_URI_SIZE];
	char* p;
	str src_ip, src_port, dst_ip, dst_port;
	int len;
	str proto;

	if (!uri || !m) {
		LOG(L_ERR, "create_rcv_uri: Invalid parameter value\n");
		return -1;
	}

	src_ip.s = ip_addr2a(&m->rcv.src_ip);
	src_ip.len = strlen(src_ip.s);
	src_port.s = int2str(m->rcv.src_port, &src_port.len);

	dst_ip = m->rcv.bind_address->address_str;
	dst_port = m->rcv.bind_address->port_no_str;

	switch(m->rcv.proto) {
	case PROTO_NONE:
	case PROTO_UDP:
		proto.s = 0; /* Do not add transport parameter, UDP is default */
		proto.len = 0;
		break;

	case PROTO_TCP:
		proto.s = "TCP";
		proto.len = 3;
		break;

	case PROTO_TLS:
		proto.s = "TLS";
		proto.len = 3;
		break;

	case PROTO_SCTP:
		proto.s = "SCTP";
		proto.len = 4;
		break;

	default:
		LOG(L_ERR, "BUG: create_rcv_uri: Unknown transport protocol\n");
		return -1;
	}

	len = 4 + src_ip.len + 1 + src_port.len;
	if (proto.s) {
		len += TRANSPORT_PARAM_LEN;
		len += proto.len;
	}

	len += DSTIP_PARAM_LEN + dst_ip.len;
	len += DSTPORT_PARAM_LEN + dst_port.len;

	if (m->rcv.src_ip.af == AF_INET6) {
		len += 2;
	}

	if (len > MAX_URI_SIZE) {
		LOG(L_ERR, "create_rcv_uri: Buffer too small\n");
		return -1;
	}

	p = buf;
	/* as transport=tls is deprecated shouldnt this be sips in case of TLS? */
	memcpy(p, "sip:", 4);
	p += 4;

	if (m->rcv.src_ip.af == AF_INET6)
		*p++ = '[';
	memcpy(p, src_ip.s, src_ip.len);
	p += src_ip.len;
	if (m->rcv.src_ip.af == AF_INET6)
		*p++ = ']';

	*p++ = ':';

	memcpy(p, src_port.s, src_port.len);
	p += src_port.len;

	if (proto.s) {
		memcpy(p, TRANSPORT_PARAM, TRANSPORT_PARAM_LEN);
		p += TRANSPORT_PARAM_LEN;

		memcpy(p, proto.s, proto.len);
		p += proto.len;
	}

	memcpy(p, DSTIP_PARAM, DSTIP_PARAM_LEN);
	p += DSTIP_PARAM_LEN;
	memcpy(p, dst_ip.s, dst_ip.len);
	p += dst_ip.len;

	memcpy(p, DSTPORT_PARAM, DSTPORT_PARAM_LEN);
	p += DSTPORT_PARAM_LEN;
	memcpy(p, dst_port.s, dst_port.len);
	p += dst_port.len;

	uri->s = buf;
	uri->len = len;

	return 0;
}


/*
 * Create an AVP to be used by registrar with the source IP and port
 * of the REGISTER
 */
static int
fix_nated_register_f(struct sip_msg* msg, char* str1, char* str2)
{
	contact_t* c;
	struct lump* anchor;
	char* param;
	str uri;

	if (create_rcv_uri(&uri, msg) < 0) {
		return -1;
	}

	if (contact_iterator(&c, msg, 0) < 0) {
		return -1;
	}

	while(c) {
		param = (char*)pkg_malloc(RECEIVED_LEN + 2 + uri.len);
		if (!param) {
			ERR("No memory left\n");
			return -1;
		}
		memcpy(param, RECEIVED, RECEIVED_LEN);
		param[RECEIVED_LEN] = '\"';
		memcpy(param + RECEIVED_LEN + 1, uri.s, uri.len);
		param[RECEIVED_LEN + 1 + uri.len] = '\"';

		anchor = anchor_lump(msg, c->name.s + c->len - msg->buf, 0, 0);
		if (anchor == NULL) {
			ERR("anchor_lump failed\n");
			return -1;
		}

		if (insert_new_lump_after(anchor, param, RECEIVED_LEN + 1 + uri.len + 1, 0) == 0) {
			ERR("insert_new_lump_after failed\n");
			pkg_free(param);
			return -1;
		}

		if (contact_iterator(&c, msg, c) < 0) {
			return -1;
		}
	}

	return 1;
}

static int
fixup_ping_contact(void **param, int param_no)
{
	int ret;

	if (param_no == 1) {
		ret = fix_param(FPARAM_AVP, param);
		if (ret <= 0) return ret;
		if (fix_param(FPARAM_STR, param) != 0) return -1;
	}
	return 0;
}

static int
ping_contact_f(struct sip_msg *msg, char *str1, char *str2)
{
	struct dest_info dst;
	str s;
	avp_t *avp;
	avp_value_t val;

	switch (((fparam_t *)str1)->type) {
		case FPARAM_AVP:
			if (!(avp = search_first_avp(((fparam_t *)str1)->v.avp.flags,
			    ((fparam_t *)str1)->v.avp.name, &val, 0))) {
				return -1;
			} else {
				if ((avp->flags & AVP_VAL_STR) == 0)
					return -1;
				s = val.s;
			}
			break;
		case FPARAM_STRING:
			s = ((fparam_t *)str1)->v.str;
			break;
	        default:
        	        ERR("BUG: Invalid parameter value in ping_contact\n");
                	return -1;
        }
	init_dest_info(&dst);
	return natping_contact(s, &dst);
}
