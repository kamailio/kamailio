/* $Id$
 *
 * Ser module, it implements the following commands:
 * fix_nated_contact() - replaces host:port in Contact field with host:port
 *			 we received this message from
 * fix_nated_sdp() - replaces IP address in the SDP with IP address
 *		     and/or adds direction=active option to the SDP
 * force_rtp_proxy() - rewrite IP address and UDP port in the SDP
 *		       body in such a way that RTP traffic visits
 *		       RTP proxy running on the same machine as a
 *		       ser itself
 *
 * Beware, those functions will only work correctly if the UA supports
 * symmetric signalling and media (not all do)!!!
 *
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
 * 2004-02-210	force_rtp_proxy now accepts option argument, which
 *		consists of string of chars, each of them turns "on"
 *		some feature, currently supported ones are:
 *
 *		 `a' - flags that UA from which message is received
 *		       doesn't support symmetric RTP;
 *		 `l' - force "lookup", that is, only rewrite SDP when
 *		       corresponding session is already exists in the
 *		       RTP proxy. Only makes sense for SIP requests,
 *		       replies are always processed in "lookup" mode;
 *		 'i' - flags that message is received from UA in the
 *		       LAN. Only makes sense when RTP proxy is rinning
 *		       in the bridge mode.
 *
 *		force_rtp_proxy can now be invoked without any argumens,
 *		as previously, with one argument - in this case argument
 *		is treated as option string and with two arguments, in
 *		which case 1st argument is option string and the 2nd
 *		one is IP address which have to be inserted into
 *		SDP (IP address on which RTP proxy listens).
 *
 */

#include "nhelpr_funcs.h"
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
#define	ISNULLADDR(sx)		((sx).len == 7 && memcmp("0.0.0.0", (sx).s, 7) == 0)

/* Supported version of the RTP proxy command protocol */
#define	SUP_CPROTOVER	20040107
#define	CPORT		"22222"

static int nat_uac_test_f(struct sip_msg* msg, char* str1, char* str2);
static int fix_nated_contact_f(struct sip_msg *, char *, char *);
static int fix_nated_sdp_f(struct sip_msg *, char *, char *);
static int extract_mediaip(str *, str *);
static int extract_mediaport(str *, str *);
static int alter_mediaip(struct sip_msg *, str *, str *, str *, int);
static int alter_mediaport(struct sip_msg *, str *, str *, str *, int);
static char *gencookie();
static char *send_rtpp_command(struct iovec *, int);
static int unforce_rtp_proxy_f(struct sip_msg *, char *, char *);
static int force_rtp_proxy0_f(struct sip_msg *, char *, char *);
static int force_rtp_proxy1_f(struct sip_msg *, char *, char *);
static int force_rtp_proxy2_f(struct sip_msg *, char *, char *);

static void timer(unsigned int, void *);
inline static int fixup_str2int(void**, int);
static int mod_init(void);
static int child_init(int);

static usrloc_api_t ul;

static int cblen = 0;
static int natping_interval = 0;

static struct {
	const char *cnetaddr;
	u_int32_t netaddr;
	u_int32_t mask;
} nets_1918[] = {
	{"10.0.0.0",    0, 0xfffffffful << 24},
	{"172.16.0.0",  0, 0xfffffffful << 20},
	{"192.168.0.0", 0, 0xfffffffful << 16},
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
static int rtpproxy_retr = 5;
static int rtpproxy_tout = 1;
static int umode = 0;
static int controlfd;
static pid_t mypid;
static unsigned int myseqn = 0;

static cmd_export_t cmds[]={
	{"fix_nated_contact", fix_nated_contact_f,    0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"fix_nated_sdp",     fix_nated_sdp_f,        1, fixup_str2int, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"unforce_rtp_proxy", unforce_rtp_proxy_f,    0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"force_rtp_proxy",   force_rtp_proxy0_f,     0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"force_rtp_proxy",   force_rtp_proxy1_f,     1, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"force_rtp_proxy",   force_rtp_proxy2_f,     2, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
	{"nat_uac_test",      nat_uac_test_f,         1, fixup_str2int, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"natping_interval", INT_PARAM, &natping_interval },
	{"ping_nated_only",  INT_PARAM, &ping_nated_only  },
	{"rtpproxy_sock",    STR_PARAM, &rtpproxy_sock    },
	{"rtpproxy_disable", INT_PARAM, &rtpproxy_disable },
	{"rtpproxy_retr",    INT_PARAM, &rtpproxy_disable },
	{"rtpproxy_tout",    INT_PARAM, &rtpproxy_disable },
	{0, 0, 0}
};

struct module_exports exports={
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
		/* Make rtpproxy_sock writeable */
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
	int rtpp_ver, n;
	char *cp;
	struct iovec v[2] = {{NULL, 0}, {"V", 1}};
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

		cp = send_rtpp_command(v, 2);
		if (cp == NULL) {
			LOG(L_WARN,"WARNING: nathelper: can't get version of "
			    "the RTP proxy\n");
			rtpproxy_disable = 1;
		} else {
			rtpp_ver = atoi(cp);
			if (rtpp_ver != SUP_CPROTOVER) {
				LOG(L_WARN, "WARNING: nathelper: unsupported "
				    "version of RTP proxy found: %d supported, "
				    "%d present\n", SUP_CPROTOVER, rtpp_ver);
				rtpproxy_disable = 1;
			}
		}
		if (rtpproxy_disable != 0)
			LOG(L_WARN, "WARNING: nathelper: support for RTP proxy"
			    "has been disabled\n");
	}
	mypid = getpid();

	return 0;
}

/*
 * ser_memmem() returns the location of the first occurence of data
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

	/* Intialize end of search address space pointer */
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
	u_int32_t netaddr;
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
 * test for occurence of RFC1918 IP address in Contact HF
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
 * test for occurence of RFC1918 IP address in SDP
 */
static int
sdp_1918(struct sip_msg* msg)
{
	str body, ip;

	if (extract_body(msg, &body) == -1) {
		LOG(L_ERR,"ERROR: sdp_1918: cannot extract body from msg!\n");
		return 0;
	}
	if (extract_mediaip(&body, &ip) == -1) {
		LOG(L_ERR, "ERROR: sdp_1918: can't extract media IP from the SDP\n");
		return 0;
	}
	if (ISNULLADDR(ip))
		return 0;

	return (is1918addr(&ip) == 1) ? 1 : 0;
}

/*
 * test for occurence of RFC1918 IP address in top Via
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
	 * test for occurences of RFC1918 addresses in Contact
	 * header field
	 */
	if ((tests & NAT_UAC_TEST_C_1918) && contact_1918(msg))
		return 1;
	/*
	 * test for occurences of RFC1918 addresses in SDP body
	 */
	if ((tests & NAT_UAC_TEST_S_1918) && sdp_1918(msg))
		return 1;
	/*
	 * test for occurences of RFC1918 addresses top Via
	 */
	if ((tests & NAT_UAC_TEST_V_1918) && via_1918(msg))
		return 1;

	/* no test succeeded */
	return -1;

}

#define	ADD_ADIRECTION	0x01
#define	FIX_MEDIAIP	0x02

#define	ADIRECTION	"a=direction:active\r\n"
#define	ADIRECTION_LEN	(sizeof(ADIRECTION) - 1)

#define	AOLDMEDIAIP	"a=oldmediaip:"
#define	AOLDMEDIAIP_LEN	(sizeof(AOLDMEDIAIP) - 1)

#define	AOLDMEDIPRT	"a=oldmediaport:"
#define	AOLDMEDIPRT_LEN	(sizeof(AOLDMEDIPRT) - 1)

#define	ANORTPPROXY	"a=nortpproxy:yes\r\n"
#define	ANORTPPROXY_LEN	(sizeof(ANORTPPROXY) - 1)

static int
fix_nated_sdp_f(struct sip_msg* msg, char* str1, char* str2)
{
	str body, body1, oldip, oldip1, newip;
	int level;
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

	if (level & FIX_MEDIAIP) {
		if (extract_mediaip(&body, &oldip) == -1) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: can't extract media IP from the SDP\n");
			goto finalise;
		}
		body1.s = oldip.s + oldip.len;
		body1.len = body.s + body.len - body1.s;
		if (extract_mediaip(&body1, &oldip1) == -1) {
			oldip1.len = 0;
		}

		newip.s = ip_addr2a(&msg->rcv.src_ip);
		newip.len = strlen(newip.s);
		if (alter_mediaip(msg, &body, &oldip, &newip, 1) == -1) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: can't alter media IP");
			return -1;
		}
		if (oldip1.len > 0 && alter_mediaip(msg, &body, &oldip1, &newip, 0) == -1) {
			LOG(L_ERR, "ERROR: fix_nated_sdp: can't alter media IP");
			return -1;
		}
	}

finalise:
	return 1;
}

static int
extract_mediaip(str *body, str *mediaip)
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
		if (len == 3 && memcmp(cp, "IP4", 3) == 0)
			nextisip = 1;
		cp = eat_space_end(cp + len, mediaip->s + mediaip->len);
	}
	if (nextisip != 2 || mediaip->len == 0) {
		LOG(L_ERR, "ERROR: extract_mediaip: no `IP4' in `c=' field\n");
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
alter_mediaip(struct sip_msg *msg, str *body, str *oldip, str *newip,
  int preserve)
{
	char *buf;
	int offset;
	struct lump* anchor;

	/* check that updating mediaip is really necessary */
	if (ISNULLADDR(*oldip))
		return 0;
	if (newip->len == oldip->len &&
	    memcmp(newip->s, oldip->s, newip->len) == 0)
		return 0;

	if (preserve != 0) {
		anchor = anchor_lump(msg, body->s + body->len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaip: anchor_lump failed\n");
			return -1;
		}
		buf = pkg_malloc(AOLDMEDIAIP_LEN + oldip->len + CRLF_LEN);
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaip: out of memory\n");
			return -1;
		}
		memcpy(buf, AOLDMEDIAIP, AOLDMEDIAIP_LEN);
		memcpy(buf + AOLDMEDIAIP_LEN, oldip->s, oldip->len);
		memcpy(buf + AOLDMEDIAIP_LEN + oldip->len, CRLF, CRLF_LEN);
		if (insert_new_lump_after(anchor, buf,
		    AOLDMEDIAIP_LEN + oldip->len + CRLF_LEN, 0) == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaip: insert_new_lump_after failed\n");
			pkg_free(buf);
			return -1;
		}
	}

	buf = pkg_malloc(newip->len);
	if (buf == NULL) {
		LOG(L_ERR, "ERROR: alter_mediaip: out of memory\n");
		return -1;
	}
	offset = oldip->s - msg->buf;
	anchor = del_lump(msg, offset, oldip->len, 0);
	if (anchor == NULL) {
		LOG(L_ERR, "ERROR: alter_mediaip: del_lump failed\n");
		pkg_free(buf);
		return -1;
	}
	memcpy(buf, newip->s, newip->len);
	if (insert_new_lump_after(anchor, buf, newip->len, 0) == 0) {
		LOG(L_ERR, "ERROR: alter_mediaip: insert_new_lump_after failed\n");
		pkg_free(buf);
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

	if (preserve != 0) {
		anchor = anchor_lump(msg, body->s + body->len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaport: anchor_lump failed\n");
			return -1;
		}
		buf = pkg_malloc(AOLDMEDIPRT_LEN + oldport->len + CRLF_LEN);
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: alter_mediaport: out of memory\n");
			return -1;
		}
		memcpy(buf, AOLDMEDIPRT, AOLDMEDIPRT_LEN);
		memcpy(buf + AOLDMEDIPRT_LEN, oldport->s, oldport->len);
		memcpy(buf + AOLDMEDIPRT_LEN + oldport->len, CRLF, CRLF_LEN);
		if (insert_new_lump_after(anchor, buf,
		    AOLDMEDIPRT_LEN + oldport->len + CRLF_LEN, 0) == NULL) {
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
	return 0;
}

static char *
gencookie()
{
	static char cook[34];

	sprintf(cook, "%d_%u ", mypid, myseqn);
	myseqn++;
	return cook;
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
	int create, port, len, asymmetric, flookup, argc;
	int oidx;
	char buf[16], opts[16];
	char *cp, *cp1;
	char **ap, *argv[10];
	struct lump* anchor;
	struct iovec v[1 + 6 + 5] = {{NULL, 0}, {opts, 0}, {" ", 1}, {NULL, 0}, {" ", 1}, {NULL, 7}, {" ", 1}, {NULL, 1}, {" ", 1}, {NULL, 0}, {" ", 1}, {NULL, 0}};
						/* 1 */    /* 2 */   /* 3 */    /* 4 */   /* 5 */    /* 6 */   /* 7 */    /* 8 */   /* 9 */    /* 10 */  /* 11 */

	asymmetric = flookup = 0;
	oidx = 1;
	for (cp = str1; *cp != '\0'; cp++) {
		switch (*cp) {
		case 'a':
		case 'A':
			opts[oidx++] = 'A';
			asymmetric = 1;
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

		default:
			LOG(L_ERR, "ERROR: force_rtp_proxy2: unknown option `%c'\n", *cp);
			return -1;
		}
	}

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
	if (extract_body(msg, &body) == -1) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't extract body "
		    "from the message\n");
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
	for (cp = body.s; (len = body.s + body.len - cp) >= ANORTPPROXY_LEN;) {
		cp1 = ser_memmem(cp, ANORTPPROXY, len, ANORTPPROXY_LEN);
		if (cp1 == NULL)
			break;
		if (cp1[-1] == '\n' || cp1[-1] == '\r')
			return -1;
		cp = cp1 + ANORTPPROXY_LEN;
	}
	if (extract_mediaip(&body, &oldip) == -1) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't extract media IP "
		    "from the message\n");
		return -1;
	}
	if (asymmetric != 0) {
		newip = oldip;
	} else {
		newip.s = ip_addr2a(&msg->rcv.src_ip);
		newip.len = strlen(newip.s);
	}
	body1.s = oldip.s + oldip.len;
	body1.len = body.s + body.len - body1.s;
	if (extract_mediaip(&body1, &oldip1) == -1) {
		oldip1.len = 0;
	}
	if (extract_mediaport(&body, &oldport) == -1) {
		LOG(L_ERR, "ERROR: force_rtp_proxy2: can't extract media port "
		    "from the message\n");
		return -1;
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
	for (ap = argv; (*ap = strsep(&cp, "\r\n\t ")) != NULL;)
		if (**ap != '\0') {
			argc++;
			if (++ap >= &argv[10])
				break;
		}
	if (argc < 1)
		return -1;
	port = atoi(argv[0]);
	if (port <= 0 || port > 65535)
		return -1;

	newport.s = buf;
	newport.len = sprintf(buf, "%d", port);
	newip.s = (argc < 2) ? str2 : argv[1];
	newip.len = strlen(newip.s);

	if (alter_mediaip(msg, &body, &oldip, &newip, 0) == -1)
		return -1;
	if (oldip1.len > 0 &&
	    alter_mediaip(msg, &body1, &oldip1, &newip, 0) == -1)
		return -1;
	if (alter_mediaport(msg, &body, &oldport, &newport, 0) == -1)
		return -1;

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

	return 1;
}

static int
force_rtp_proxy1_f(struct sip_msg* msg, char* str1, char* str2)
{
	char *cp, *newip;
	int len;

	cp = ip_addr2a(&msg->rcv.dst_ip);
	len = strlen(cp);
	newip = alloca(len + 1);
	memcpy(newip, cp, len + 1);
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
