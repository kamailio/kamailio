/* $Id$
 *
 * Copyright (C) 2003 Porta Software Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
 *		 `d' - flags that IP address in SDP should be trusted.
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
 * 2004-03-24	Fix newport for null ip address case (e.g onhold re-INVITE)
* 				(andrei)
 * 
 *
 */

#include "nhelpr_funcs.h"
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

/* Handy macros */
#define	STR2IOVEC(sx, ix)	{(ix).iov_base = (sx).s; (ix).iov_len = (sx).len;}

/* Supported version of the RTP proxy command protocol */
#define	SUP_CPROTOVER	20040107
#define	CPORT		"22222"

/* Anti foot-shooting flags to be set after altering SDP */
#define	SDP_IP_AFS	250
#define	SDP_PORT_AFS	251

static int nat_uac_test_f(struct sip_msg* msg, char* str1, char* str2);
static int fix_nated_contact_f(struct sip_msg *, char *, char *);
static int fix_nated_sdp_f(struct sip_msg *, char *, char *);
static int extract_mediaip(str *, str *, int *);
static int extract_mediaport(str *, str *);
static int alter_mediaip(struct sip_msg *, str *, str *, int, str *, int, int);
static int alter_mediaport(struct sip_msg *, str *, str *, str *, int);
static char *gencookie();
static int rtpp_test(int, int);
static char *send_rtpp_command(struct iovec *, int);
static int unforce_rtp_proxy_f(struct sip_msg *, char *, char *);
static int force_rtp_proxy0_f(struct sip_msg *, char *, char *);
static int force_rtp_proxy1_f(struct sip_msg *, char *, char *);
static int force_rtp_proxy2_f(struct sip_msg *, char *, char *);
static int fix_nated_register_f(struct sip_msg *, char *, char *);
static int add_rcv_param_f(struct sip_msg *, char *, char *);

static void timer(unsigned int, void *);
inline static int fixup_str2int(void**, int);
static int mod_init(void);
static int child_init(int);

static usrloc_api_t ul;

static int cblen = 0;
static int natping_interval = 0;

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

/*
 * If this parameter is set then the natpinger will ping only contacts
 * that have the NAT flag set in user location database
 */
static int ping_nated_only = 0;
static const char sbuf[4] = {0, 0, 0, 0};
static char *rtpproxy_sock = "unix:/var/run/rtpproxy.sock";
static int rtpproxy_disable = 0;
static int rtpproxy_disable_tout = 60;
static int rtpproxy_retr = 5;
static int rtpproxy_tout = 1;
static int umode = 0;
static int controlfd;
static pid_t mypid;
static unsigned int myseqn = 0;
static int_str rcv_avp = {.n = 42};

static cmd_export_t cmds[] = {
	{"fix_nated_contact",  fix_nated_contact_f,    0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"fix_nated_sdp",      fix_nated_sdp_f,        1, fixup_str2int, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"unforce_rtp_proxy",  unforce_rtp_proxy_f,    0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"force_rtp_proxy",    force_rtp_proxy0_f,     0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"force_rtp_proxy",    force_rtp_proxy1_f,     1, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"force_rtp_proxy",    force_rtp_proxy2_f,     2, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"nat_uac_test",       nat_uac_test_f,         1, fixup_str2int, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"fix_nated_register", fix_nated_register_f,   0, 0,             REQUEST_ROUTE },
	{"add_rcv_param",      add_rcv_param_f,        0, 0,             REQUEST_ROUTE },
	{0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"natping_interval",      INT_PARAM, &natping_interval      },
	{"ping_nated_only",       INT_PARAM, &ping_nated_only       },
	{"rtpproxy_sock",         STR_PARAM, &rtpproxy_sock         },
	{"rtpproxy_disable",      INT_PARAM, &rtpproxy_disable      },
	{"rtpproxy_disable_tout", INT_PARAM, &rtpproxy_disable_tout },
	{"rtpproxy_retr",         INT_PARAM, &rtpproxy_retr         },
	{"rtpproxy_tout",         INT_PARAM, &rtpproxy_tout         },
	{"received_avp",          INT_PARAM, &rcv_avp               },
	{0, 0, 0}
};

struct module_exports exports = {
	"nathelper",
	cmds,
	params,
	mod_init,
	0, /* reply processing */
	0, /* destroy function */
	0, /* on_break */
	child_init
};

static int
mod_init(void)
{
	int i;
	char *cp;
	bind_usrloc_t bind_usrloc;
	struct in_addr addr;

	if (natping_interval > 0) {
		bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
		if (!bind_usrloc) {
			LOG(L_ERR, "nathelper: Can't find usrloc module\n");
 			return -1;
 		}

		if (bind_usrloc(&ul) < 0) {
			return -1;
		}

		register_timer(timer, NULL, natping_interval);
	}

	/* Prepare 1918 networks list */
	for (i = 0; nets_1918[i].cnetaddr != NULL; i++) {
		if (inet_aton(nets_1918[i].cnetaddr, &addr) != 1)
			abort();
		nets_1918[i].netaddr = ntohl(addr.s_addr) & nets_1918[i].mask;
	}

	if (rtpproxy_disable == 0) {
		/* Make rtpproxy_sock writable */
		cp = pkg_malloc(strlen(rtpproxy_sock) + 1);
		if (cp == NULL) {
			LOG(L_ERR, "nathelper: Can't allocate memory\n");
			return -1;
		}
		strcpy(cp, rtpproxy_sock);
		rtpproxy_sock = cp;

		if (strncmp(rtpproxy_sock, "udp:", 4) == 0) {
			umode = 1;
			rtpproxy_sock += 4;
		} else if (strncmp(rtpproxy_sock, "udp6:", 5) == 0) {
			umode = 6;
			rtpproxy_sock += 5;
		} else if (strncmp(rtpproxy_sock, "unix:", 5) == 0) {
			umode = 0;
			rtpproxy_sock += 5;
		}
	}

	return 0;
}

static int
child_init(int rank)
{
	int n;
	char *cp;
	struct addrinfo hints, *res;

	if (rtpproxy_disable == 0) {
		if (umode != 0) {
			cp = strrchr(rtpproxy_sock, ':');
			if (cp != NULL) {
				*cp = '\0';
				cp++;
			}
			if (cp == NULL || *cp == '\0')
				cp = CPORT;

			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = 0;
			hints.ai_family = (umode == 6) ? AF_INET6 : AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			if ((n = getaddrinfo(rtpproxy_sock, cp, &hints, &res)) != 0) {
				LOG(L_ERR, "nathelper: getaddrinfo: %s\n", gai_strerror(n));
				return -1;
			}

			controlfd = socket((umode == 6) ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
			if (controlfd == -1) {
				LOG(L_ERR, "nathelper: can't create socket\n");
				freeaddrinfo(res);
				return -1;
			}

			if (connect(controlfd, res->ai_addr, res->ai_addrlen) == -1) {
				LOG(L_ERR, "nathelper: can't connect to a RTP proxy\n");
				close(controlfd);
				freeaddrinfo(res);
				return -1;
			}
			freeaddrinfo(res);
		}

		rtpproxy_disable = rtpp_test(0, 1);
	} else {
		rtpproxy_disable_tout = -1;
	}
	mypid = getpid();

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
 * ser_memmem() returns the location of the first occurrence of data
 * pattern b2 of size len2 in memory block b1 of size len1 or
 * NULL if none is found. Obtained from NetBSD.
 */
static void *
ser_memmem(const void *b1, const void *b2, size_t len1, size_t len2)
{
	/* Initialize search pointer */
	char *sp = (char *) b1;

	/* Initialize pattern pointer */
	char *pp = (char *) b2;

	/* Initialize end of search address space pointer */
	char *eos = sp + len1 - len2;

	/* Sanity check */
	if(!(b1 && b2 && len1 && len2))
		return NULL;

	while (sp <= eos) {
		if (*sp == *pp)
			if (memcmp(sp, pp, len2) == 0)
				return sp;

			sp++;
	}

	return NULL;
}

/*
 * Some helper functions taken verbatim from tm module.
 */

/*
 * Extract tag from To header field of a response
 * assumes the to header is already parsed, so
 * make sure it really is before calling this function
 */
static inline int
get_to_tag(struct sip_msg* _m, str* _tag)
{

	if (!_m->to) {
		LOG(L_ERR, "get_to_tag(): To header field missing\n");
		return -1;
	}

	if (get_to(_m)->tag_value.len) {
		_tag->s = get_to(_m)->tag_value.s;
		_tag->len = get_to(_m)->tag_value.len;
	} else {
		_tag->len = 0;
	}

	return 0;
}


/*
 * Extract tag from From header field of a request
 */
static inline int
get_from_tag(struct sip_msg* _m, str* _tag)
{

	if (parse_from_header(_m) == -1) {
		LOG(L_ERR, "get_from_tag(): Error while parsing From header\n");
		return -1;
	}

	if (get_from(_m)->tag_value.len) {
		_tag->s = get_from(_m)->tag_value.s;
		_tag->len = get_from(_m)->tag_value.len;
	} else {
		_tag->len = 0;
	}

	return 0;
}

/*
 * Extract Call-ID value
 * assumes the callid header is already parsed
 * (so make sure it is, before calling this function or
 *  it might fail even if the message _has_ a callid)
 */
static inline int
get_callid(struct sip_msg* _m, str* _cid)
{

	if (_m->callid == 0) {
		LOG(L_ERR, "get_callid(): Call-ID not found\n");
		return -1;
	}

	_cid->s = _m->callid->body.s;
	_cid->len = _m->callid->body.len;
	trim(_cid);
	return 0;
}

/*
 * Extract URI from the Contact header field
 */
static inline int
get_contact_uri(struct sip_msg* _m, struct sip_uri *uri, contact_t** _c)
{

	if ((parse_headers(_m, HDR_CONTACT, 0) == -1) || !_m->contact)
		return -1;
	if (!_m->contact->parsed && parse_contact(_m->contact) < 0) {
		LOG(L_ERR, "get_contact_uri: Error while parsing Contact body\n");
		return -1;
	}
	*_c = ((contact_body_t*)_m->contact->parsed)->contacts;
	if (*_c == NULL) {
		LOG(L_ERR, "get_contact_uri: Error while parsing Contact body\n");
		return -1;
	}
	if (parse_uri((*_c)->uri.s, (*_c)->uri.len, uri) < 0 || uri->host.len <= 0) {
		LOG(L_ERR, "get_contact_uri: Error while parsing Contact URI\n");
		return -1;
	}
	return 0;
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
	contact_t* c;
	struct lump* anchor;
	struct sip_uri uri;

	if (get_contact_uri(msg, &uri, &c) == -1)
		return -1;
	if (uri.proto != PROTO_UDP && uri.proto != PROTO_NONE)
		return -1;
	if ((c->uri.s < msg->buf) || (c->uri.s > (msg->buf + msg->len))) {
		LOG(L_ERR, "ERROR: you can't call fix_nated_contact twice, "
		    "check your config!\n");
		return -1;
	}
	if (uri.port.len == 0)
		uri.port.s = uri.host.s + uri.host.len;

	offset = c->uri.s - msg->buf;
	anchor = del_lump(msg, offset, c->uri.len, HDR_CONTACT);
	if (anchor == 0)
		return -1;

	cp = ip_addr2a(&msg->rcv.src_ip);
	len = c->uri.len + strlen(cp) + 6 /* :port */ - (uri.port.s + uri.port.len - uri.host.s) + 1;
	buf = pkg_malloc(len);
	if (buf == NULL) {
		LOG(L_ERR, "ERROR: fix_nated_contact: out of memory\n");
		return -1;
	}
	temp[0] = uri.host.s[0];
	temp[1] = c->uri.s[c->uri.len];
	c->uri.s[c->uri.len] = uri.host.s[0] = '\0';
	len1 = snprintf(buf, len, "%s%s:%d%s", c->uri.s, cp, msg->rcv.src_port,
	    uri.port.s + uri.port.len);
	if (len1 < len)
		len = len1;
	uri.host.s[0] = temp[0];
	c->uri.s[c->uri.len] = temp[1];
	if (insert_new_lump_after(anchor, buf, len, HDR_CONTACT) == 0) {
		pkg_free(buf);
		return -1;
	}
	c->uri.s = buf;
	c->uri.len = len;

	return 1;
}

inline static int
fixup_str2int( void** param, int param_no)
{
	unsigned long go_to;
	int err;

	if (param_no == 1) {
		go_to = str2s(*param, strlen(*param), &err);
		if (err == 0) {
			pkg_free(*param);
			*param = (void *)go_to;
			return 0;
		} else {
			LOG(L_ERR, "ERROR: fixup_str2int: bad number <%s>\n",
				(char *)(*param));
			return E_CFG;
		}
	}
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

	tests = (int)(long)str1;

	/* return true if any of the NAT-UAC tests holds */

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
	if ((tests & NAT_UAC_TEST_C_1918) && (contact_1918(msg)>0))
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

	/* no test succeeded */
	return -1;

}

#define	ADD_ADIRECTION	0x01
#define	FIX_MEDIP	0x02

#define	ADIRECTION	"a=direction:active\r\n"
#define	ADIRECTION_LEN	(sizeof(ADIRECTION) - 1)

#define	AOLDMEDIP	"a=oldmediaip:"
#define	AOLDMEDIP_LEN	(sizeof(AOLDMEDIP) - 1)

#define	AOLDMEDIP6	"a=oldmediaip6:"
#define	AOLDMEDIP6_LEN	(sizeof(AOLDMEDIP6) - 1)

#define	AOLDMEDPRT	"a=oldmediaport:"
#define	AOLDMEDPRT_LEN	(sizeof(AOLDMEDPRT) - 1)

#define	ANORTPPROXY	"a=nortpproxy:yes\r\n"
#define	ANORTPPROXY_LEN	(sizeof(ANORTPPROXY) - 1)

static int
fix_nated_sdp_f(struct sip_msg* msg, char* str1, char* str2)
{
	str body, body1, oldip, oldip1, newip;
	int level, pf, pf1;
	char *buf;
	struct lump* anchor;

	level = (int)(long)str1;

	if (extract_body(msg, &body) == -1) {
		LOG(L_ERR,"ERROR: fix_nated_sdp: cannot extract body from msg!\n");
		return -1;
	}

	if (level & ADD_ADIRECTION) {
		msg->msg_flags |= FL_FORCE_ACTIVE;
		anchor = anchor_lump(msg, body.s + body.len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: anchor_lump failed\n");
			return -1;
		}
		buf = pkg_malloc(ADIRECTION_LEN * sizeof(char));
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: out of memory\n");
			return -1;
		}
		memcpy(buf, ADIRECTION, ADIRECTION_LEN);
		if (insert_new_lump_after(anchor, buf, ADIRECTION_LEN, 0) == NULL) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: insert_new_lump_after failed\n");
			pkg_free(buf);
			return -1;
		}
	}

	if (level & FIX_MEDIP) {
		if (extract_mediaip(&body, &oldip, &pf) == -1) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: can't extract media IP from the SDP\n");
			goto finalize;
		}
		if (pf != AF_INET) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: "
			    "not an IPv4 address in SDP\n");
			goto finalize;
		}
		body1.s = oldip.s + oldip.len;
		body1.len = body.s + body.len - body1.s;
		if (extract_mediaip(&body1, &oldip1, &pf1) == -1) {
			oldip1.len = 0;
		}
		if (oldip1.len > 0 && pf != pf1) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: mismatching "
			    "address families in SDP\n");
			return -1;
		}

		newip.s = ip_addr2a(&msg->rcv.src_ip);
		newip.len = strlen(newip.s);
		if (alter_mediaip(msg, &body, &oldip, pf, &newip, pf,
		    1) == -1) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: can't alter media IP");
			return -1;
		}
		if (oldip1.len > 0 && alter_mediaip(msg, &body, &oldip1, pf1,
		    &newip, pf, 0) == -1) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: can't alter media IP");
			return -1;
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
		LOG(L_DBG, "ERROR: extract_mediaip: no `c=' in SDP\n");
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
extract_mediaport(str *body, str *mediaport)
{
	char *cp, *cp1;
	int len;

	cp1 = NULL;
	for (cp = body->s; (len = body->s + body->len - cp) > 0;) {
		cp1 = ser_memmem(cp, "m=", len, 2);
		if (cp1 == NULL || cp1[-1] == '\n' || cp1[-1] == '\r')
			break;
		cp = cp1 + 2;
	}
	if (cp1 == NULL) {
		LOG(L_ERR, "ERROR: extract_mediaport: no `m=' in SDP\n");
		return -1;
	}
	mediaport->s = cp1 + 2;
	mediaport->len = eat_line(mediaport->s, body->s + body->len -
	  mediaport->s) - mediaport->s;
	trim_len(mediaport->len, mediaport->s, *mediaport);

	if (mediaport->len < 7 || memcmp(mediaport->s, "audio", 5) != 0 ||
	  !isspace((int)mediaport->s[5])) {
		LOG(L_ERR, "ERROR: extract_mediaport: can't parse `m=' in SDP\n");
		return -1;
	}
	cp = eat_space_end(mediaport->s + 5, mediaport->s + mediaport->len);
	mediaport->len = eat_token_end(cp, mediaport->s + mediaport->len) - cp;
	mediaport->s = cp;
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
	if (isflagset(msg, SDP_IP_AFS)) {
		LOG(L_ERR, "ERROR: alter_mediaip: you can't rewrite the same "
		  "SDP twice, check your config!\n");
		return -1;
	}

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
		memcpy(buf, omip.s, omip.len);
		memcpy(buf + omip.len, oldip->s, oldip->len);
		memcpy(buf + omip.len + oldip->len, CRLF, CRLF_LEN);
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
	setflag(msg, SDP_IP_AFS);

	if (insert_new_lump_after(anchor, nip.s, nip.len, 0) == 0) {
		LOG(L_ERR, "ERROR: alter_mediaip: insert_new_lump_after failed\n");
		pkg_free(nip.s);
		return -1;
	}
	return 0;
}

static int
alter_mediaport(struct sip_msg *msg, str *body, str *oldport, str *newport,
  int preserve)
{
	char *buf;
	int offset;
	struct lump* anchor;

	/* check that updating mediaport is really necessary */
	if (newport->len == oldport->len &&
	    memcmp(newport->s, oldport->s, newport->len) == 0)
		return 0;

	/*
	 * Since rewriting the same info twice will mess SDP up,
	 * apply simple anti foot shooting measure - put flag on
	 * messages that have been altered and check it when
	 * another request comes.
	 */
	if (isflagset(msg, SDP_PORT_AFS)) {
		LOG(L_ERR, "ERROR: alter_mediaip: you can't rewrite the same "
		  "SDP twice, check your config!\n");
		return -1;
	}

	if (preserve != 0) {
		anchor = anchor_lump(msg, body->s + body->len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaport: anchor_lump failed\n");
			return -1;
		}
		buf = pkg_malloc(AOLDMEDPRT_LEN + oldport->len + CRLF_LEN);
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaport: out of memory\n");
			return -1;
		}
		memcpy(buf, AOLDMEDPRT, AOLDMEDPRT_LEN);
		memcpy(buf + AOLDMEDPRT_LEN, oldport->s, oldport->len);
		memcpy(buf + AOLDMEDPRT_LEN + oldport->len, CRLF, CRLF_LEN);
		if (insert_new_lump_after(anchor, buf,
		    AOLDMEDPRT_LEN + oldport->len + CRLF_LEN, 0) == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaport: insert_new_lump_after failed\n");
			pkg_free(buf);
			return -1;
		}
	}

	buf = pkg_malloc(newport->len);
	if (buf == NULL) {
		LOG(L_ERR, "ERROR: alter_mediaport: out of memory\n");
		return -1;
	}
	offset = oldport->s - msg->buf;
	anchor = del_lump(msg, offset, oldport->len, 0);
	if (anchor == NULL) {
		LOG(L_ERR, "ERROR: alter_mediaport: del_lump failed\n");
		pkg_free(buf);
		return -1;
	}
	memcpy(buf, newport->s, newport->len);
	if (insert_new_lump_after(anchor, buf, newport->len, 0) == 0) {
		LOG(L_ERR, "ERROR: alter_mediaport: insert_new_lump_after failed\n");
		pkg_free(buf);
		return -1;
	}
	setflag(msg, SDP_PORT_AFS);
	return 0;
}

static char *
gencookie()
{
	static char cook[34];

	sprintf(cook, "%d_%u ", (int)mypid, myseqn);
	myseqn++;
	return cook;
}

static int
rtpp_test(int isdisabled, int force)
{
	int rtpp_ver;
	static int recheck_ticks = 0;
	char *cp;
	struct iovec v[2] = {{NULL, 0}, {"V", 1}};

	if (force == 0) {
		if (isdisabled == 0)
			return 0;
		if (recheck_ticks > get_ticks())
			return 1;
	}
	cp = send_rtpp_command(v, 2);
	if (cp == NULL) {
		LOG(L_WARN,"WARNING: rtpp_test: can't get version of "
		    "the RTP proxy\n");
	} else {
		rtpp_ver = atoi(cp);
		if (rtpp_ver == SUP_CPROTOVER) {
			LOG(L_INFO, "rtpp_test: RTP proxy found, support for "
			    "it %senabled\n", force == 0 ? "re-" : "");
			return 0;
		}
		LOG(L_WARN, "WARNING: rtpp_test: unsupported "
		    "version of RTP proxy found: %d supported, "
		    "%d present\n", SUP_CPROTOVER, rtpp_ver);
	}
	LOG(L_WARN, "WARNING: rtpp_test: support for RTP proxy"
	    "has been disabled%s\n",
	    rtpproxy_disable_tout < 0 ? "" : " temporarily");
	if (rtpproxy_disable_tout >= 0)
		recheck_ticks = get_ticks() + rtpproxy_disable_tout;

	return 1;
}

static char *
send_rtpp_command(struct iovec *v, int vcnt)
{
	struct sockaddr_un addr;
	int fd, len, i;
	char *cp;
	static char buf[256];
	struct pollfd fds[1];

	len = 0;
	cp = buf;
	if (umode == 0) {
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_LOCAL;
		strncpy(addr.sun_path, rtpproxy_sock,
		    sizeof(addr.sun_path) - 1);
#ifdef HAVE_SOCKADDR_SA_LEN
		addr.sun_len = strlen(addr.sun_path);
#endif

		fd = socket(AF_LOCAL, SOCK_STREAM, 0);
		if (fd < 0) {
			LOG(L_ERR, "ERROR: send_rtpp_command: can't create socket\n");
			return NULL;
		}
		if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			close(fd);
			LOG(L_ERR, "ERROR: send_rtpp_command: can't connect to RTP proxy\n");
			return NULL;
		}

		do {
			len = writev(fd, v + 1, vcnt - 1);
		} while (len == -1 && errno == EINTR);
		if (len <= 0) {
			close(fd);
			LOG(L_ERR, "ERROR: send_rtpp_command: can't send command to a RTP proxy\n");
			return NULL;
		}
		do {
			len = read(fd, buf, sizeof(buf) - 1);
		} while (len == -1 && errno == EINTR);
		close(fd);
		if (len <= 0) {
			LOG(L_ERR, "ERROR: send_rtpp_command: can't read reply from a RTP proxy\n");
			return NULL;
		}
	} else {
		fds[0].fd = controlfd;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		/* Drain input buffer */
		while ((poll(fds, 1, 0) == 1) &&
		    ((fds[0].revents & POLLIN) != 0)) {
			recv(controlfd, buf, sizeof(buf) - 1, 0);
			fds[0].revents = 0;
		}
		v[0].iov_base = gencookie();
		v[0].iov_len = strlen(v[0].iov_base);
		for (i = 0; i < rtpproxy_retr; i++) {
			do {
				len = writev(controlfd, v, vcnt);
			} while (len == -1 && (errno == EINTR || errno == ENOBUFS));
			if (len <= 0) {
				LOG(L_ERR, "ERROR: send_rtpp_command: "
				    "can't send command to a RTP proxy\n");
				return NULL;
			}
			while ((poll(fds, 1, rtpproxy_tout * 1000) == 1) &&
			    (fds[0].revents & POLLIN) != 0) {
				do {
					len = recv(controlfd, buf, sizeof(buf) - 1, 0);
				} while (len == -1 && errno == EINTR);
				if (len <= 0) {
					LOG(L_ERR, "ERROR: send_rtpp_command: "
					    "can't read reply from a RTP proxy\n");
					return NULL;
				}
				if (len >= (v[0].iov_len - 1) &&
				    memcmp(buf, v[0].iov_base, (v[0].iov_len - 1)) == 0) {
					len -= (v[0].iov_len - 1);
					cp += (v[0].iov_len - 1);
					if (len != 0) {
						len--;
						cp++;
					}
					goto out;
				}
				fds[0].revents = 0;
			}
		}
		if (i == rtpproxy_retr) {
			LOG(L_ERR, "ERROR: send_rtpp_command: "
			    "timeout waiting reply from a RTP proxy\n");
			return NULL;
		}
	}

out:
	cp[len] = '\0';
	return cp;
}

static int
unforce_rtp_proxy_f(struct sip_msg* msg, char* str1, char* str2)
{
	str callid, from_tag, to_tag;
	struct iovec v[1 + 4 + 3] = {{NULL, 0}, {"D", 1}, {" ", 1}, {NULL, 0}, {" ", 1}, {NULL, 0}, {" ", 1}, {NULL, 0}};
						/* 1 */   /* 2 */   /* 3 */    /* 4 */   /* 5 */    /* 6 */   /* 1 */

	rtpproxy_disable = rtpp_test(rtpproxy_disable, 0);
	if (rtpproxy_disable != 0) {
		LOG(L_ERR, "ERROR: unforce_rtp_proxy: support for RTP proxy "
		    "is disabled\n");
		return -1;
	}
	if (get_callid(msg, &callid) == -1 || callid.len == 0) {
		LOG(L_ERR, "ERROR: unforce_rtp_proxy: can't get Call-Id field\n");
		return -1;
	}
	if (get_to_tag(msg, &to_tag) == -1) {
		LOG(L_ERR, "ERROR: unforce_rtp_proxy: can't get To tag\n");
		return -1;
	}
	if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
		LOG(L_ERR, "ERROR: unforce_rtp_proxy: can't get From tag\n");
		return -1;
	}
	STR2IOVEC(callid, v[3]);
	STR2IOVEC(from_tag, v[5]);
	STR2IOVEC(to_tag, v[7]);
	send_rtpp_command(v, (to_tag.len > 0) ? 8 : 6);

	return 1;
}

static int
force_rtp_proxy2_f(struct sip_msg* msg, char* str1, char* str2)
{
	str body, body1, oldport, oldip, oldip1, newport, newip;
	str callid, from_tag, to_tag, tmp;
	int create, port, len, asymmetric, flookup, argc, proxied, real;
	int oidx, pf, pf1, force;
	char opts[16];
	char *cp, *cp1;
	char  *cpend, *next;
	char **ap, *argv[10];
	struct lump* anchor;
	struct iovec v[1 + 6 + 5] = {{NULL, 0}, {NULL, 0}, {" ", 1}, {NULL, 0},
		{" ", 1}, {NULL, 7}, {" ", 1}, {NULL, 1}, {" ", 1}, {NULL, 0},
		{" ", 1}, {NULL, 0}};
						/* 1 */    /* 2 */   /* 3 */    /* 4 */
		/* 5 */    /* 6 */   /* 7 */    /* 8 */   /* 9 */    /* 10 */
		/* 11 */

	v[1].iov_base=opts;
	asymmetric = flookup = force = real = 0;
	oidx = 1;
	for (cp = str1; *cp != '\0'; cp++) {
		switch (*cp) {
		case 'a':
		case 'A':
			opts[oidx++] = 'A';
			asymmetric = 1;
			real = 1;
			break;

		case 'i':
		case 'I':
			opts[oidx++] = 'I';
			break;

		case 'e':
		case 'E':
			opts[oidx++] = 'E';
			break;

		case 'l':
		case 'L':
			flookup = 1;
			break;

		case 'f':
		case 'F':
			force = 1;
			break;

		case 'r':
		case 'R':
			real = 1;
			break;

		default:
			LOG(L_ERR, "ERROR: force_rtp_proxy2: unknown option `%c'\n", *cp);
			return -1;
		}
	}

	rtpproxy_disable = rtpp_test(rtpproxy_disable, 0);
	if (rtpproxy_disable != 0) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: support for RTP proxy "
		    "is disabled\n");
		return -1;
	}

	if (msg->first_line.type == SIP_REQUEST &&
	    msg->first_line.u.request.method_value == METHOD_INVITE) {
		create = 1;
	} else if (msg->first_line.type == SIP_REPLY) {
		create = 0;
	} else {
		return -1;
	}
	/* extract_body will also parse all the headers in the message as
	 * a side effect => don't move get_callid/get_to_tag in front of it
	 * -- andrei */
	if (extract_body(msg, &body) == -1) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't extract body "
		    "from the message\n");
		return -1;
	}
	if (get_callid(msg, &callid) == -1 || callid.len == 0) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't get Call-Id field\n");
		return -1;
	}
	if (get_to_tag(msg, &to_tag) == -1) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't get To tag\n");
		return -1;
	}
	if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't get From tag\n");
		return -1;
	}
	if (flookup != 0) {
		if (create == 0 || to_tag.len == 0)
			return -1;
		create = 0;
		tmp = from_tag;
		from_tag = to_tag;
		to_tag = tmp;
	}
	proxied = 0;
	for (cp = body.s; (len = body.s + body.len - cp) >= ANORTPPROXY_LEN;) {
		cp1 = ser_memmem(cp, ANORTPPROXY, len, ANORTPPROXY_LEN);
		if (cp1 == NULL)
			break;
		if (cp1[-1] == '\n' || cp1[-1] == '\r') {
			proxied = 1;
			break;
		}
		cp = cp1 + ANORTPPROXY_LEN;
	}
	if (proxied != 0 && force == 0)
		return -1;
	if (extract_mediaip(&body, &oldip, &pf) == -1) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't extract media IP "
		    "from the message\n");
		return -1;
	}
	if (asymmetric != 0 || real != 0) {
		newip = oldip;
	} else {
		newip.s = ip_addr2a(&msg->rcv.src_ip);
		newip.len = strlen(newip.s);
	}
	body1.s = oldip.s + oldip.len;
	body1.len = body.s + body.len - body1.s;
	if (extract_mediaip(&body1, &oldip1, &pf1) == -1) {
		oldip1.len = 0;
	}
	if (oldip1.len > 0 && pf != pf1) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: mismatching address "
		    "families in SDP\n");
		return -1;
	}
	if (extract_mediaport(&body, &oldport) == -1) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't extract media port "
		    "from the message\n");
		return -1;
	}
	if (pf == AF_INET6) {
		opts[oidx] = '6';
		oidx++;
	}
	opts[0] = (create == 0) ? 'L' : 'U';
	v[1].iov_len = oidx;
	STR2IOVEC(callid, v[3]);
	STR2IOVEC(newip, v[5]);
	STR2IOVEC(oldport, v[7]);
	STR2IOVEC(from_tag, v[9]);
	STR2IOVEC(to_tag, v[11]);
	cp = send_rtpp_command(v, (to_tag.len > 0) ? 12 : 10);
	if (cp == NULL)
		return -1;
	argc = 0;
	memset(argv, 0, sizeof(argv));
	cpend=cp+strlen(cp);
	next=eat_token_end(cp, cpend);
	for (ap = argv; cp<cpend; cp=next+1, next=eat_token_end(cp, cpend)){
		*next=0;
		if (*cp != '\0') {
			*ap=cp;
			argc++;
			if ((char*)++ap >= ((char*)argv+sizeof(argv)))
				break;
		}
	}
	if (argc < 1)
		return -1;
	port = atoi(argv[0]);
	if (port <= 0 || port > 65535)
		return -1;

	pf1 = (argc >= 3 && argv[2][0] == '6') ? AF_INET6 : AF_INET;

	if (isnulladdr(&oldip, pf)) {
		if (pf1 == AF_INET6) {
			newip.s = "::";
			newip.len = 2;
		} else {
			newip.s = "0.0.0.0";
			newip.len = 7;
		}
	} else {
		newip.s = (argc < 2) ? str2 : argv[1];
		newip.len = strlen(newip.s);
	}
	newport.s=int2str(port, &newport.len); /* beware static buffer */
	
	if (alter_mediaip(msg, &body, &oldip, pf, &newip, pf1, 0) == -1)
		return -1;
	if (oldip1.len > 0 &&
	    alter_mediaip(msg, &body1, &oldip1, pf, &newip, pf1, 0) == -1)
		return -1;
	if (alter_mediaport(msg, &body, &oldport, &newport, 0) == -1)
		return -1;

	if (proxied == 0) {
		cp = pkg_malloc(ANORTPPROXY_LEN * sizeof(char));
		if (cp == NULL) {
			LOG(L_ERR, "ERROR: force_rtp_proxy2: out of memory\n");
			return -1;
		}
		anchor = anchor_lump(msg, body.s + body.len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LOG(L_ERR, "ERROR: force_rtp_proxy2: anchor_lump failed\n");
			pkg_free(cp);
			return -1;
		}
		memcpy(cp, ANORTPPROXY, ANORTPPROXY_LEN);
		if (insert_new_lump_after(anchor, cp, ANORTPPROXY_LEN, 0) == NULL) {
			LOG(L_ERR, "ERROR: force_rtp_proxy2: insert_new_lump_after failed\n");
			pkg_free(cp);
			return -1;
		}
	}

	return 1;
}

static int
force_rtp_proxy1_f(struct sip_msg* msg, char* str1, char* str2)
{
	char *cp;
	char newip[IP_ADDR_MAX_STR_SIZE];

	cp = ip_addr2a(&msg->rcv.dst_ip);
	strcpy(newip, cp);
	return force_rtp_proxy2_f(msg, str1, newip);
}

static int
force_rtp_proxy0_f(struct sip_msg* msg, char* str1, char* str2)
{
	char arg[1] = {'\0'};

	return force_rtp_proxy1_f(msg, arg, NULL);
}

static void
timer(unsigned int ticks, void *param)
{
	int rval;
	void *buf, *cp;
	str c;
	struct sip_uri curi;
	union sockaddr_union to;
	struct hostent* he;
	struct socket_info* send_sock;

	buf = NULL;
	if (cblen > 0) {
		buf = pkg_malloc(cblen);
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: nathelper::timer: out of memory\n");
			return;
		}
	}
	rval = ul.get_all_ucontacts(buf, cblen, (ping_nated_only ? FL_NAT : 0));
	if (rval > 0) {
		if (buf != NULL)
			pkg_free(buf);
		cblen = rval * 2;
		buf = pkg_malloc(cblen);
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: nathelper::timer: out of memory\n");
			return;
		}
		rval = ul.get_all_ucontacts(buf, cblen, (ping_nated_only ? FL_NAT : 0));
		if (rval != 0) {
			pkg_free(buf);
			return;
		}
	}

	if (buf == NULL)
		return;

	cp = buf;
	while (1) {
		memcpy(&(c.len), cp, sizeof(c.len));
		if (c.len == 0)
			break;
		c.s = (char*)cp + sizeof(c.len);
		cp =  (char*)cp + sizeof(c.len) + c.len;
		if (parse_uri(c.s, c.len, &curi) < 0) {
			LOG(L_ERR, "ERROR: nathelper::timer: can't parse contact uri\n");
			continue;
		}
		if (curi.proto != PROTO_UDP && curi.proto != PROTO_NONE)
			continue;
		if (curi.port_no == 0)
			curi.port_no = SIP_PORT;
		he = sip_resolvehost(&curi.host, &curi.port_no, PROTO_UDP);
		if (he == NULL){
			LOG(L_ERR, "ERROR: nathelper::timer: can't resolve_hos\n");
			continue;
		}
		hostent2su(&to, he, 0, curi.port_no);
		send_sock = get_send_socket(&to, PROTO_UDP);
		if (send_sock == NULL) {
			LOG(L_ERR, "ERROR: nathelper::timer: can't get sending socket\n");
			continue;
		}
		udp_send(send_sock, (char *)sbuf, sizeof(sbuf), &to);
	}
	pkg_free(buf);
}


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
	str ip, port;
	int len;
	str proto;

	if (!uri || !m) {
		LOG(L_ERR, "create_rcv_uri: Invalid parameter value\n");
		return -1;
	}

	ip.s = ip_addr2a(&m->rcv.src_ip);
	ip.len = strlen(ip.s);

	port.s = int2str(m->rcv.src_port, &port.len);

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

	len = 4 + ip.len + 1 + port.len;
	if (proto.s) {
		len += TRANSPORT_PARAM_LEN;
		len += proto.len;
	}

	if (len > MAX_URI_SIZE) {
		LOG(L_ERR, "create_rcv_uri: Buffer too small\n");
		return -1;
	}

	p = buf;
	memcpy(p, "sip:", 4);
	p += 4;
	
	memcpy(p, ip.s, ip.len);
	p += ip.len;

	*p++ = ':';
	
	memcpy(p, port.s, port.len);
	p += port.len;

	if (proto.s) {
		memcpy(p, TRANSPORT_PARAM, TRANSPORT_PARAM_LEN);
		p += TRANSPORT_PARAM_LEN;

		memcpy(p, proto.s, proto.len);
		p += proto.len;
	}

	uri->s = buf;
	uri->len = len;

	return 0;
}


/*
 * Add received parameter to Contacts for further
 * forwarding of the REGISTER requuest
 */
static int
add_rcv_param_f(struct sip_msg* msg, char* str1, char* str2)
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
		param = (char*)pkg_malloc(RECEIVED_LEN + uri.len);
		if (!param) {
			LOG(L_ERR, "add_rcv_param: No memory left\n");
			return -1;
		}
		memcpy(param, RECEIVED, RECEIVED_LEN);
		param[RECEIVED_LEN] = '\"';
		memcpy(param + RECEIVED_LEN + 1, uri.s, uri.len);
		param[RECEIVED_LEN + 1 + uri.len] = '\"';

		anchor = anchor_lump(msg, c->name.s + c->len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LOG(L_ERR, "add_rcv_param: anchor_lump failed\n");
			return -1;
		}		

		if (insert_new_lump_after(anchor, param, RECEIVED_LEN + 1 + uri.len + 1, 0) == 0) {
			LOG(L_ERR, "add_rcv_param: insert_new_lump_after failed\n");
			pkg_free(param);
			return -1;
		}

		if (contact_iterator(&c, msg, c) < 0) {
			return -1;
		}
	}

	return 1;
}


/*
 * Create an AVP to be used by registrar with the source IP and port
 * of the REGISTER
 */
static int
fix_nated_register_f(struct sip_msg* msg, char* str1, char* str2)
{
	str uri;
	int_str val;

	if (create_rcv_uri(&uri, msg) < 0) {
		return -1;
	}

	val.s = &uri;

	if (add_avp(AVP_VAL_STR, rcv_avp, val) < 0) {
		LOG(L_ERR, "fix_nated_register: Error while creating AVP\n");
		return -1;
	}

	return 1;
}
