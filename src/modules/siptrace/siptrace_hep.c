/*
 * siptrace module - helper module to trace sip messages
 *
 * Copyright (C) 2017 kamailio.org
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/pvar.h"
#include "../../core/proxy.h"
#include "../../core/forward.h"
#include "../../core/resolve.h"
#include "../../core/socket_info.h"
#include "../../core/parser/parse_uri.h"

#include "../../modules/sipcapture/hep.h"

#include "siptrace_hep.h"

extern int hep_version;
extern int hep_capture_id;
extern int hep_vendor_id;
extern str hep_auth_key_str;
extern str trace_send_sock_str;
extern sip_uri_t *trace_send_sock_uri;
extern str trace_send_sock_name_str;
extern socket_info_t *trace_send_sock_info;
extern str trace_dup_uri_str;
extern sip_uri_t *trace_dup_uri;

/**
 *
 */
int trace_send_hep_duplicate(str *body, str *from, str *to,
		struct dest_info *dst2, str *correlation_id_str)
{
	switch(hep_version) {
		case 1:
		case 2:
			return trace_send_hep2_duplicate(body, from, to, dst2);
		case 3:
			return trace_send_hep3_duplicate(
					body, from, to, dst2, correlation_id_str);
		default:
			LM_ERR("Unsupported HEP version\n");
			return -1;
	}
}

/**
 *
 */
int trace_send_hep3_duplicate(str *body, str *from, str *to,
		struct dest_info *dst2, str *correlation_id_str)
{
	struct socket_info *si;
	void *buffer = NULL;
	unsigned int len, proto;
	struct dest_info dst;
	struct dest_info *dst_fin = NULL;
	struct proxy_l *p = NULL;
	union sockaddr_union from_su;
	union sockaddr_union to_su;
	struct timeval tvb;
	struct timezone tz;

	gettimeofday(&tvb, &tz);

	if(pipport2su(from->s, &from_su, &proto) == -1
			|| (pipport2su(to->s, &to_su, &proto) == -1))
		goto error;

	if(from_su.s.sa_family != to_su.s.sa_family) {
		LM_ERR("interworking detected ?\n");
		goto error;
	}

	len = sizeof(struct hep_ctrl);		   // header
	len += sizeof(struct hep_chunk_uint8); // proto_family
	len += sizeof(struct hep_chunk_uint8); // proto_id
	if(from_su.s.sa_family == AF_INET6) {
		len += sizeof(struct hep_chunk_ip6); // src IPv6 address
		len += sizeof(struct hep_chunk_ip6); // dst IPv6 address
	} else {
		len += sizeof(struct hep_chunk_ip4); // src IPv4 address
		len += sizeof(struct hep_chunk_ip4); // dst IPv4 address
	}
	len += sizeof(struct hep_chunk_uint16); // source port
	len += sizeof(struct hep_chunk_uint16); // destination port
	len += sizeof(struct hep_chunk_uint32); // timestamp
	len += sizeof(struct hep_chunk_uint32); // timestamp us
	len += sizeof(struct hep_chunk_uint8);  // proto_type (SIP)
	len += sizeof(struct hep_chunk_uint32); // capture ID
	len += sizeof(struct hep_chunk);		// payload

	if(hep_auth_key_str.s && hep_auth_key_str.len > 0) {
		len += sizeof(struct hep_chunk) + hep_auth_key_str.len;
	}

	if(correlation_id_str) {
		if(correlation_id_str->len > 0) {
			len += sizeof(struct hep_chunk) + correlation_id_str->len;
		}
	}

	len += body->len;

	if(unlikely(len > BUF_SIZE)) {
		goto error;
	}

	buffer = (void *)pkg_malloc(len);
	if(!buffer) {
		LM_ERR("out of memory\n");
		goto error;
	}

	HEP3_PACK_INIT(buffer);
	HEP3_PACK_CHUNK_UINT8(0, 0x0001, from_su.s.sa_family);
	HEP3_PACK_CHUNK_UINT8(0, 0x0002, proto);
	if(from_su.s.sa_family == AF_INET) {
		HEP3_PACK_CHUNK_UINT32_NBO(0, 0x0003, from_su.sin.sin_addr.s_addr);
		HEP3_PACK_CHUNK_UINT32_NBO(0, 0x0004, to_su.sin.sin_addr.s_addr);
		HEP3_PACK_CHUNK_UINT16_NBO(0, 0x0007, htons(from_su.sin.sin_port));
		HEP3_PACK_CHUNK_UINT16_NBO(0, 0x0008, htons(to_su.sin.sin_port));
	} else if(from_su.s.sa_family == AF_INET6) {
		HEP3_PACK_CHUNK_IP6(0, 0x0005, &from_su.sin6.sin6_addr);
		HEP3_PACK_CHUNK_IP6(0, 0x0006, &to_su.sin6.sin6_addr);
		HEP3_PACK_CHUNK_UINT16_NBO(0, 0x0007, htons(from_su.sin6.sin6_port));
		HEP3_PACK_CHUNK_UINT16_NBO(0, 0x0008, htons(to_su.sin6.sin6_port));
	} else {
		LM_ERR("unknown address family [%u]\n", from_su.s.sa_family);
		goto error;
	}

	HEP3_PACK_CHUNK_UINT32(0, 0x0009, tvb.tv_sec);
	HEP3_PACK_CHUNK_UINT32(0, 0x000a, tvb.tv_usec);
	HEP3_PACK_CHUNK_UINT8(0, 0x000b, 0x01); /* protocol type: SIP */
	HEP3_PACK_CHUNK_UINT32(0, 0x000c, hep_capture_id);

	if(correlation_id_str) {
		if(correlation_id_str->len > 0) {
			HEP3_PACK_CHUNK_DATA(
					0, 0x0011, correlation_id_str->s, correlation_id_str->len);
		}
	}
	if(hep_auth_key_str.s && hep_auth_key_str.len > 0) {
		HEP3_PACK_CHUNK_DATA(0, 0x000e, hep_auth_key_str.s, hep_auth_key_str.len);
	}
	HEP3_PACK_CHUNK_DATA(0, 0x000f, body->s, body->len);
	HEP3_PACK_FINALIZE(buffer, &len);

	if(!dst2) {
		init_dest_info(&dst);
		dst.proto = trace_dup_uri->proto;
		p = mk_proxy(&trace_dup_uri->host, trace_dup_uri->port_no, dst.proto);
		if(p == 0) {
			LM_ERR("bad host name in uri\n");
			goto error;
		}

		hostent2su(
				&dst.to, &p->host, p->addr_idx, (p->port) ? p->port : SIP_PORT);
		LM_DBG("setting up the socket_info\n");
		dst_fin = &dst;
	} else {
		dst_fin = dst2;
	}

	si = NULL;
	if(trace_send_sock_name_str.s) {
		LM_DBG("send sock name activated - find the sock info\n");
		if(trace_send_sock_info) {
			si = trace_send_sock_info;
		} else {
			si = ksr_get_socket_by_name(&trace_send_sock_name_str);
		}
	} else if(trace_send_sock_str.s) {
		LM_DBG("send sock addr activated - find the sock_info\n");
		if(trace_send_sock_info) {
			si = trace_send_sock_info;
		} else {
			si = grep_sock_info(&trace_send_sock_uri->host,
					trace_send_sock_uri->port_no,
					trace_send_sock_uri->proto);
		}
	}
	if(trace_send_sock_name_str.s || trace_send_sock_str.s) {
		if(!si) {
			LM_WARN("cannot find send socket info\n");
		} else {
			LM_DBG("found send socket: [%.*s] [%.*s]\n", si->name.len,
					si->name.s, si->address_str.len, si->address_str.s);
			dst_fin->send_sock = si;
		}
	}

	if(dst_fin->send_sock == 0) {
		dst_fin->send_sock = get_send_socket(0, &dst_fin->to, dst_fin->proto);
		if(dst_fin->send_sock == 0) {
			LM_ERR("can't forward to af %d, proto %d no corresponding"
				   " listening socket\n",
					dst_fin->to.s.sa_family, dst_fin->proto);
			goto error;
		}
	}

	if(msg_send_buffer(dst_fin, buffer, len, 1) < 0) {
		LM_ERR("cannot send hep duplicate message\n");
		goto error;
	}

	if(p) {
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}
	pkg_free(buffer);
	return 0;
error:
	if(p) {
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}
	if(buffer)
		pkg_free(buffer);
	return -1;
}

/**
 *
 */
int trace_send_hep2_duplicate(
		str *body, str *from, str *to, struct dest_info *dst2)
{
	struct dest_info dst;
	struct socket_info *si;
	struct dest_info *dst_fin = NULL;
	struct proxy_l *p = NULL /* make gcc happy */;
	void *buffer = NULL;
	union sockaddr_union from_su;
	union sockaddr_union to_su;
	unsigned int len, buflen, proto;
	struct hep_hdr hdr;
	struct hep_iphdr hep_ipheader;
	struct hep_timehdr hep_time;
	struct timeval tvb;
	struct timezone tz;

	struct hep_ip6hdr hep_ip6header;

	if(body->s == NULL || body->len <= 0)
		return -1;

	if(trace_dup_uri_str.s == 0 || trace_dup_uri == NULL)
		return 0;


	gettimeofday(&tvb, &tz);


	/* message length */
	len = body->len + sizeof(struct hep_ip6hdr) + sizeof(struct hep_hdr)
		  + sizeof(struct hep_timehdr);
	;


	/* The packet is too big for us */
	if(unlikely(len > BUF_SIZE)) {
		goto error;
	}

	/* Convert proto:ip:port to sockaddress union SRC IP */
	if(pipport2su(from->s, &from_su, &proto) == -1
			|| (pipport2su(to->s, &to_su, &proto) == -1))
		goto error;

	/* check if from and to are in the same family*/
	if(from_su.s.sa_family != to_su.s.sa_family) {
		LM_ERR("interworking detected ?\n");
		goto error;
	}

	if(!dst2) {
		init_dest_info(&dst);
		/* create a temporary proxy*/
		dst.proto = trace_dup_uri->proto;
		p = mk_proxy(&trace_dup_uri->host, trace_dup_uri->port_no, dst.proto);
		if(p == 0) {
			LM_ERR("bad host name in uri\n");
			goto error;
		}

		hostent2su(
				&dst.to, &p->host, p->addr_idx, (p->port) ? p->port : SIP_PORT);
		LM_DBG("setting up the socket_info\n");
		dst_fin = &dst;
	} else {
		dst_fin = dst2;
	}

	si = NULL;
	if(trace_send_sock_name_str.s) {
		LM_DBG("send sock name activated - find the sock info\n");
		if(trace_send_sock_info) {
			si = trace_send_sock_info;
		} else {
			si = ksr_get_socket_by_name(&trace_send_sock_name_str);
		}
	} else if(trace_send_sock_str.s) {
		LM_DBG("send sock addr activated - find the sock info\n");
		if(trace_send_sock_info) {
			si = trace_send_sock_info;
		} else {
			si = grep_sock_info(&trace_send_sock_uri->host,
					trace_send_sock_uri->port_no,
					trace_send_sock_uri->proto);
		}
	}
	if(trace_send_sock_name_str.s || trace_send_sock_str.s) {
		if(!si) {
			LM_WARN("cannot find send socket info\n");
		} else {
			LM_DBG("found send socket: [%.*s] [%.*s]\n", si->name.len,
					si->name.s, si->address_str.len, si->address_str.s);
			dst_fin->send_sock = si;
		}
	}

	if(dst_fin->send_sock == 0) {
		dst_fin->send_sock = get_send_socket(0, &dst_fin->to, dst_fin->proto);
		if(dst_fin->send_sock == 0) {
			LM_ERR("can't forward to af %d, proto %d no corresponding"
				   " listening socket\n",
					dst_fin->to.s.sa_family, dst_fin->proto);
			goto error;
		}
	}

	/* Version && proto && length */
	hdr.hp_l = sizeof(struct hep_hdr);
	hdr.hp_v = hep_version;
	hdr.hp_p = proto;

	/* AND the last */
	if(from_su.s.sa_family == AF_INET) {
		/* prepare the hep headers */

		hdr.hp_f = AF_INET;
		hdr.hp_sport = htons(from_su.sin.sin_port);
		hdr.hp_dport = htons(to_su.sin.sin_port);

		hep_ipheader.hp_src = from_su.sin.sin_addr;
		hep_ipheader.hp_dst = to_su.sin.sin_addr;

		len = sizeof(struct hep_iphdr);
	} else if(from_su.s.sa_family == AF_INET6) {
		/* prepare the hep6 headers */

		hdr.hp_f = AF_INET6;

		hdr.hp_sport = htons(from_su.sin6.sin6_port);
		hdr.hp_dport = htons(to_su.sin6.sin6_port);

		hep_ip6header.hp6_src = from_su.sin6.sin6_addr;
		hep_ip6header.hp6_dst = to_su.sin6.sin6_addr;

		len = sizeof(struct hep_ip6hdr);
	} else {
		LM_ERR("Unsupported protocol family\n");
		goto error;
		;
	}

	hdr.hp_l += len;
	if(hep_version == 2) {
		len += sizeof(struct hep_timehdr);
	}
	len += sizeof(struct hep_hdr) + body->len;
	buffer = (void *)pkg_malloc(len + 1);
	if(buffer == 0) {
		LM_ERR("out of memory\n");
		goto error;
	}

	/* Copy job */
	memset(buffer, '\0', len + 1);

	/* copy hep_hdr */
	memcpy((void *)buffer, &hdr, sizeof(struct hep_hdr));
	buflen = sizeof(struct hep_hdr);

	/* hep_ip_hdr */
	if(from_su.s.sa_family == AF_INET) {
		memcpy((void *)buffer + buflen, &hep_ipheader,
				sizeof(struct hep_iphdr));
		buflen += sizeof(struct hep_iphdr);
	} else {
		memcpy((void *)buffer + buflen, &hep_ip6header,
				sizeof(struct hep_ip6hdr));
		buflen += sizeof(struct hep_ip6hdr);
	}

	if(hep_version == 2) {

		hep_time.tv_sec = to_le(tvb.tv_sec);
		hep_time.tv_usec = to_le(tvb.tv_usec);
		hep_time.captid = hep_capture_id;

		memcpy((void *)buffer + buflen, &hep_time, sizeof(struct hep_timehdr));
		buflen += sizeof(struct hep_timehdr);
	}

	/* PAYLOAD */
	memcpy((void *)(buffer + buflen), (void *)body->s, body->len);
	buflen += body->len;

	if(msg_send_buffer(dst_fin, buffer, buflen, 1) < 0) {
		LM_ERR("cannot send hep duplicate message\n");
		goto error;
	}

	if(p) {
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}
	pkg_free(buffer);
	return 0;
error:
	if(p) {
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}
	if(buffer)
		pkg_free(buffer);
	return -1;
}

/*!
 * \brief Convert a STR [proto:]ip[:port] into socket address.
 * [proto:]ip[:port]
 * \param pipport (udp:127.0.0.1:5060 or tcp:2001:0DB8:AC10:FE01:5060)
 * \param tmp_su target structure
 * \param proto uint protocol type
 * \return success / unsuccess
 */
int pipport2su(char *pipport, union sockaddr_union *tmp_su, unsigned int *proto)
{
	unsigned int port_no, cutlen = 4;
	struct ip_addr *ip;
	char *p, *host_s;
	str port_str, host_uri;
	unsigned len = 0;
	char tmp_piport[256];

	/*parse protocol */
	if(strncmp(pipport, "udp:", 4) == 0)
		*proto = IPPROTO_UDP;
	else if(strncmp(pipport, "tcp:", 4) == 0)
		*proto = IPPROTO_TCP;
	else if(strncmp(pipport, "tls:", 4) == 0)
		*proto = IPPROTO_IDP; /* fake proto type */
	else if(strncmp(pipport, "ws:", 3) == 0)
		cutlen = 3, *proto = IPPROTO_IDP; /* fake proto type */
	else if(strncmp(pipport, "wss:", 4) == 0)
		*proto = IPPROTO_IDP; /* fake proto type */
#ifdef USE_SCTP
	else if(strncmp(pipport, "sctp:", 5) == 0)
		cutlen = 5, *proto = IPPROTO_SCTP;
#endif
	else if(strncmp(pipport, "any:", 4) == 0)
		*proto = IPPROTO_UDP;
	else {
		LM_ERR("bad protocol %s\n", pipport);
		return -1;
	}

	if((len = strlen(pipport)) >= 256) {
		LM_ERR("too big pipport\n");
		goto error;
	}

	/* our tmp string */
	strncpy(tmp_piport, pipport, len + 1);

	len = 0;

	/*separate proto and host */
	p = tmp_piport + cutlen;
	if((*(p)) == '\0') {
		LM_ERR("malformed ip address\n");
		goto error;
	}
	host_s = p;

	if((p = strrchr(p + 1, ':')) == 0) {
		LM_DBG("no port specified\n");
		port_no = 0;
	} else {
		/*the address contains a port number*/
		*p = '\0';
		p++;
		port_str.s = p;
		port_str.len = strlen(p);
		LM_DBG("the port string is %s\n", p);
		if(str2int(&port_str, &port_no) != 0) {
			LM_ERR("there is not a valid number port\n");
			goto error;
		}
		*p = '\0';
	}

	/* now IPv6 address has no brackets. It should be fixed! */
	if(host_s[0] == '[') {
		len = strlen(host_s + 1) - 1;
		if(host_s[len + 1] != ']') {
			LM_ERR("bracket not closed\n");
			goto error;
		}
		memmove(host_s, host_s + 1, len);
		host_s[len] = '\0';
	}

	host_uri.s = host_s;
	host_uri.len = strlen(host_s);

	/* check if it's an ip address */
	if(((ip = str2ip(&host_uri)) != 0) || ((ip = str2ip6(&host_uri)) != 0)) {
		ip_addr2su(tmp_su, ip, ntohs(port_no));
		return 0;
	}

error:
	return -1;
}

/**
 *
 */
int hlog(struct sip_msg *msg, str *correlationid, str *message)
{
	char *buf;
	size_t len;
	struct timeval tvb;
	struct timezone tz;
	struct dest_info dst;
	struct proxy_l *p = NULL;
	struct socket_info *si;

	if(!correlationid) {
		if(msg->callid == NULL && ((parse_headers(msg, HDR_CALLID_F, 0) == -1)
										  || (msg->callid == NULL))) {
			LM_ERR("cannot parse Call-Id header\n");
			return -1;
		}
		correlationid = &(msg->callid->body);
	}

	len = sizeof(hep_ctrl_t)
		  + sizeof(hep_chunk_uint8_t)  /* ip protocol family */
		  + sizeof(hep_chunk_uint8_t)  /* ip protocol id */
		  + sizeof(hep_chunk_t) + 16   /* src address (enough space for ipv6) */
		  + sizeof(hep_chunk_t) + 16   /* dst address (ditto) */
		  + sizeof(hep_chunk_uint16_t) /* src port */
		  + sizeof(hep_chunk_uint16_t) /* dst port */
		  + sizeof(hep_chunk_uint32_t) /* timestamp */
		  + sizeof(hep_chunk_uint32_t) /* timestamp micro */
		  + sizeof(hep_chunk_uint32_t) /* capture id */
		  + sizeof(hep_chunk_uint8_t)  /* protocol type */
		  + sizeof(hep_chunk_t) + correlationid->len + sizeof(hep_chunk_t)
		  + message->len;

	if(hep_auth_key_str.len) {
		len += sizeof(hep_chunk_t) + hep_auth_key_str.len;
	}

	buf = pkg_malloc(len);

	if(!buf) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}

	gettimeofday(&tvb, &tz);

	init_dest_info(&dst);
	dst.proto = trace_dup_uri->proto;
	p = mk_proxy(&trace_dup_uri->host, trace_dup_uri->port_no, dst.proto);
	if(p == 0) {
		LM_ERR("bad host name in uri\n");
		goto error;
	}

	hostent2su(&dst.to, &p->host, p->addr_idx, (p->port) ? p->port : SIP_PORT);
	LM_DBG("setting up the socket_info\n");

	free_proxy(p); /* frees only p content, not p itself */
	pkg_free(p);

	si = NULL;
	if(trace_send_sock_name_str.s) {
		LM_DBG("send sock name activated - find the sock info\n");
		if(trace_send_sock_info) {
			si = trace_send_sock_info;
		} else {
			si = ksr_get_socket_by_name(&trace_send_sock_name_str);
		}
	} else if(trace_send_sock_str.s) {
		LM_DBG("send sock addr activated - find the sock info\n");
		if(trace_send_sock_info) {
			si = trace_send_sock_info;
		} else {
			si = grep_sock_info(&trace_send_sock_uri->host,
					trace_send_sock_uri->port_no,
					trace_send_sock_uri->proto);
		}
	}
	if(trace_send_sock_name_str.s || trace_send_sock_str.s) {
		if(!si) {
			LM_WARN("cannot find send socket info\n");
		} else {
			LM_DBG("found send socket: [%.*s] [%.*s]\n", si->name.len,
					si->name.s, si->address_str.len, si->address_str.s);
			dst.send_sock = si;
		}
	}

	if(dst.send_sock == 0) {
		dst.send_sock = get_send_socket(0, &dst.to, dst.proto);
		if(dst.send_sock == 0) {
			LM_ERR("can't forward to af %d, proto %d no corresponding"
				   " listening socket\n",
					dst.to.s.sa_family, dst.proto);
			goto error;
		}
	}

	HEP3_PACK_INIT(buf);
	HEP3_PACK_CHUNK_UINT8(0, 0x0001, dst.send_sock->address.af);
	HEP3_PACK_CHUNK_UINT8(0, 0x0002, 0x11);
	if(dst.send_sock->address.af == AF_INET) {
		HEP3_PACK_CHUNK_UINT32_NBO(
				0, 0x0003, dst.send_sock->address.u.addr32[0]);
		HEP3_PACK_CHUNK_UINT32_NBO(0, 0x0004, dst.to.sin.sin_addr.s_addr);
		HEP3_PACK_CHUNK_UINT16_NBO(0, 0x0008, dst.to.sin.sin_port);
	} else if(dst.send_sock->address.af == AF_INET6) {
		HEP3_PACK_CHUNK_IP6(0, 0x0005, dst.send_sock->address.u.addr);
		HEP3_PACK_CHUNK_IP6(0, 0x0006, &dst.to.sin6.sin6_addr);
		HEP3_PACK_CHUNK_UINT16_NBO(0, 0x0008, dst.to.sin6.sin6_port);
	} else {
		LM_ERR("unknown address family [%u]\n", dst.send_sock->address.af);
		goto error;
	}
	HEP3_PACK_CHUNK_UINT16(0, 0x0007, dst.send_sock->port_no);

	HEP3_PACK_CHUNK_UINT32(0, 0x0009, tvb.tv_sec);
	HEP3_PACK_CHUNK_UINT32(0, 0x000a, tvb.tv_usec);
	HEP3_PACK_CHUNK_UINT8(0, 0x000b, 0x64); /* protocol type: log */
	HEP3_PACK_CHUNK_UINT32(0, 0x000c, hep_capture_id);
	HEP3_PACK_CHUNK_DATA(0, 0x0011, correlationid->s, correlationid->len);
	if(hep_auth_key_str.s && hep_auth_key_str.len > 0) {
		HEP3_PACK_CHUNK_DATA(0, 0x000e, hep_auth_key_str.s, hep_auth_key_str.len);
	}
	HEP3_PACK_CHUNK_DATA(0, 0x000f, message->s, message->len);
	HEP3_PACK_FINALIZE(buf, &len);

	if(msg_send_buffer(&dst, buf, len, 1) < 0) {
		LM_ERR("cannot send hep log\n");
		goto error;
	}

	pkg_free(buf);
	return 1;

error:
	pkg_free(buf);
	return -1;
}
