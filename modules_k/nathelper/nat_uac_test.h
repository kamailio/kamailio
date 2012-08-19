/* $Id$
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef NAT_UAC_TEST_H
#define NAT_UAC_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "../../msg_translator.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/contact.h"
#include "../../parser/sdp/sdp.h"

#include "nhelpr_funcs.h"

/* NAT UAC test constants */
#define NAT_UAC_TEST_C_1918     0x01
#define NAT_UAC_TEST_RCVD       0x02
#define NAT_UAC_TEST_V_1918     0x04
#define NAT_UAC_TEST_S_1918     0x08
#define NAT_UAC_TEST_RPORT      0x10
#define NAT_UAC_TEST_O_1918     0x20
#define NAT_UAC_TEST_WS         0x40

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
 * Test if IP address in netaddr belongs to RFC1918 networks
 * netaddr in network byte order
 */
static inline int
is1918addr_n(uint32_t netaddr)
{
	int i;
	uint32_t hl;

	hl = ntohl(netaddr);
	for (i = 0; nets_1918[i].cnetaddr != NULL; i++) {
		if ((hl & nets_1918[i].mask) == nets_1918[i].netaddr) {
			return 1;
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
	int rval;
	char backup;

	rval = -1;
	backup = saddr->s[saddr->len];
	saddr->s[saddr->len] = '\0';
	if (inet_aton(saddr->s, &addr) != 1)
		goto theend;
	rval = is1918addr_n(addr.s_addr);

theend:
	saddr->s[saddr->len] = backup;
	return rval;
}

static inline int
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
 * test for occurrence of RFC1918 IP address in Contact HF
 */
static inline int
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
static inline int
sdp_1918(struct sip_msg* msg)
{
	str *ip;
	int pf;
	int sdp_session_num, sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;

	if(0 != parse_sdp(msg)) {
		LM_ERR("Unable to parse sdp\n");
		return 0;
	}

	sdp_session_num = 0;
	for(;;) {
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;) {
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;
			if (sdp_stream->ip_addr.s && sdp_stream->ip_addr.len) {
				ip = &(sdp_stream->ip_addr);
				pf = sdp_stream->pf;
			} else {
				ip = &(sdp_session->ip_addr);
				pf = sdp_session->pf;
			}
			if (pf != AF_INET || isnulladdr(ip, pf))
				break;
			if (is1918addr(ip) == 1)
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
static int
via_1918(struct sip_msg* msg)
{

	return (is1918addr(&(msg->via1->host)) == 1) ? 1 : 0;
}

/*
 * Test if IP address pointed to by ip belongs to RFC1918 networks
 */
static inline int
is1918addr_ip(struct ip_addr *ip)
{
	if (ip->af != AF_INET)
		return 0;
	return is1918addr_n(ip->u.addr32[0]);
}

static inline int
nat_uac_test_f(struct sip_msg* msg, char* str1, char* str2)
{
	int tests;

	tests = (int)(long)str1;

	/* return true if any of the NAT-UAC tests holds */

	/* test if the source port is different from the port in Via */
	if ((tests & NAT_UAC_TEST_RPORT) &&
		 (msg->rcv.src_port!=(msg->via1->port?msg->via1->port:SIP_PORT)) ){
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

	/*
	 * test for occurrences of RFC1918 addresses in source address
	 */
	if ((tests & NAT_UAC_TEST_O_1918) && is1918addr_ip(&msg->rcv.src_ip))
		return 1;

	/*
 	 * tests prototype to check whether the message arrived on a WebSocket
 	 */
	if ((tests & NAT_UAC_TEST_WS)
		&& (msg->rcv.proto == PROTO_WS || msg->rcv.proto == PROTO_WSS))
		return 1;

	/* no test succeeded */
	return -1;

}
#endif /* NAT_UAC_TEST_H */
