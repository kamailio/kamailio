/**
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
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
#include <arpa/inet.h>

#include "../../core/ip_addr.h"
#include "../../core/resolve.h"

#include "sipdump_write.h"

extern int sipdump_mode;

/* structures related to PCAP headers imported from open source Asterisk project
 * function to write to PCAP file adapted for internal structures
 * source: res_pjsip_logger.c - License GPLv2 -  Copyright (C) Digium, Inc.*/

/* PCAP Header */
struct pcap_header
{
	uint32_t magic_number;	/* PCAP file format magic number */
	uint16_t version_major; /* Major version number of the file format */
	uint16_t version_minor; /* Minor version number of the file format */
	int32_t thiszone;		/* GMT to local correction */
	uint32_t sigfigs;		/* Accuracy of timestamps */
	uint32_t snaplen; /* The maximum size that can be recorded in the file */
	uint32_t network; /* Type of packets held within the file */
};

/* PCAP Packet Record Header */
struct pcap_record_header
{
	uint32_t ts_sec;   /* When the record was created */
	uint32_t ts_usec;  /* When the record was created */
	uint32_t incl_len; /* Length of packet as saved in the file */
	uint32_t orig_len; /* Length of packet as sent over network */
};

/* PCAP Ethernet Header */
struct pcap_ethernet_header
{
	uint8_t dst[6]; /* Destination MAC address */
	uint8_t src[6]; /* Source MAC address */
	uint16_t type;	/*!The type of packet contained within */
} __attribute__((__packed__));

/*! PCAP IPv4 Header */
struct pcap_ipv4_header
{
	uint8_t ver_ihl; /* IP header version and other bits */
	uint8_t ip_tos;	 /*  Type of service details */
	uint16_t ip_len; /* Total length of the packet (including IPv4 header) */
	uint16_t ip_id;	 /* Identification value */
	uint16_t ip_off; /* Fragment offset */
	uint8_t ip_ttl;	 /* Time to live for the packet */
	uint8_t ip_protocol; /* Protocol of the data held within the packet (always UDP) */
	uint16_t ip_sum; /* Checksum (not calculated for our purposes */
	uint32_t ip_src; /* Source IP address */
	uint32_t ip_dst; /* Destination IP address */
};

/* PCAP IPv6 Header */
struct pcap_ipv6_header
{
	union
	{
		struct ip6_hdrctl
		{
			uint32_t ip6_un1_flow; /* Version, traffic class, flow label */
			uint16_t
					ip6_un1_plen; /* Length of the packet (not including IPv6 header) */
			uint8_t ip6_un1_nxt;  /* Next header field */
			uint8_t ip6_un1_hlim; /* Hop Limit */
		} ip6_un1;
		uint8_t ip6_un2_vfc; /* Version, traffic class */
	} ip6_ctlun;
	struct in6_addr ip6_src; /* Source IP address */
	struct in6_addr ip6_dst; /* Destination IP address */
};

/* PCAP UDP Header */
struct pcap_udp_header
{
	uint16_t src;	   /* Source IP port */
	uint16_t dst;	   /* Destination IP port */
	uint16_t length;   /* Length of the UDP header plus UDP packet */
	uint16_t checksum; /* Packet checksum, left uncalculated for our purposes */
};

void sipdump_init_pcap(FILE *fs)
{
	struct pcap_header v_pcap_header = {
			.magic_number = 0xa1b2c3d4,
			.version_major = 2,
			.version_minor = 4,
			.snaplen = 65535,
			.network =
					1, /* use ethernet to combine IPv4 and IPv6 in same pcap */
	};

	LM_DBG("writing the pcap file header\n");
	if(fwrite(&v_pcap_header, sizeof(struct pcap_header), 1, fs) != 1) {
		LM_ERR("failed to write the pcap file header\n");
	} else {
		fflush(fs);
	}
}

static char *_sipdump_pcap_data_buf = NULL;

void sipdump_write_pcap(FILE *fs, sipdump_data_t *spd)
{
	str data = str_init("");
	str mval = str_init("");
	str sproto = str_init("none");
	char *p = NULL;

	struct pcap_record_header v_pcap_record_header = {
			.ts_sec = 0,
			.ts_usec = 0,
	};
	struct pcap_ethernet_header v_pcap_ethernet_header = {
			.type = 0,
	};
	struct pcap_ipv4_header v_pcap_ipv4_header = {
			.ver_ihl = 0x45, /* IPv4 + 20 bytes of header */
			.ip_ttl = 128,	 /* put a TTL of 128 to keep Wireshark less blue */
	};
	struct pcap_ipv6_header v_pcap_ipv6_header = {
			.ip6_ctlun.ip6_un2_vfc = 0x60,
	};
	void *pcap_ip_header;
	size_t pcap_ip_header_len;
	struct pcap_udp_header v_pcap_udp_header;
	struct in_addr ip4addr;
	struct in6_addr ip6addr;

	if(fs == NULL || spd == NULL) {
		return;
	}

	v_pcap_record_header.ts_sec = spd->tv.tv_sec;
	v_pcap_record_header.ts_usec = spd->tv.tv_usec;

	data = spd->data;
	if((sipdump_mode & SIPDUMP_MODE_WPCAPEX)
			&& (spd->data.len < BUF_SIZE - 256)) {
		if(_sipdump_pcap_data_buf == NULL) {
			_sipdump_pcap_data_buf = (char *)malloc(BUF_SIZE);
		}
		if(_sipdump_pcap_data_buf != NULL) {
			data.s = _sipdump_pcap_data_buf;
			data.len = 0;
			mval.s = q_memchr(spd->data.s, '\n', spd->data.len);
			p = data.s;
			if(mval.s != NULL) {
				data.len = mval.s - spd->data.s + 1;
				memcpy(p, spd->data.s, data.len);
				p += data.len;
				get_valid_proto_string(spd->protoid, 0, 0, &sproto);
				mval.len = snprintf(p, BUF_SIZE - (data.len + 1),
						"P-KSR-SIPDump: %.*s pid=%d pno=%d\r\n", sproto.len,
						sproto.s, spd->pid, spd->procno);
				if(mval.len < 0 || mval.len >= BUF_SIZE - (data.len + 1)) {
					data = spd->data;
				} else {
					data.len += mval.len;
					p += mval.len;
					mval.s += 1;
					memcpy(p, mval.s, spd->data.s + spd->data.len - mval.s);
					data.len += spd->data.s + spd->data.len - mval.s;
				}
			}
		}
	}
	/* always store UDP */
	v_pcap_udp_header.src = ntohs(spd->src_port);
	v_pcap_udp_header.dst = ntohs(spd->dst_port);
	v_pcap_udp_header.length = ntohs(sizeof(struct pcap_udp_header) + data.len);
	v_pcap_udp_header.checksum = 0;

	/* IP header */
	if(spd->afid == AF_INET6) {
		LM_DBG("ipv6 = %s -> %s\n", spd->src_ip.s, spd->dst_ip.s);
		v_pcap_ethernet_header.type = htons(0x86DD); /* IPv6 packet */
		pcap_ip_header = &v_pcap_ipv6_header;
		pcap_ip_header_len = sizeof(struct pcap_ipv6_header);
		if(inet_pton(AF_INET6, spd->src_ip.s, &ip6addr) != 1) {
			LM_ERR("failed to parse IPv6 address %s\n", spd->src_ip.s);
			return;
		}
		memcpy(&v_pcap_ipv6_header.ip6_src, &ip6addr, sizeof(struct in6_addr));
		if(inet_pton(AF_INET6, spd->dst_ip.s, &ip6addr) != 1) {
			LM_ERR("failed to parse IPv6 address %s\n", spd->dst_ip.s);
			return;
		}
		memcpy(&v_pcap_ipv6_header.ip6_dst, &ip6addr, sizeof(struct in6_addr));
		v_pcap_ipv6_header.ip6_ctlun.ip6_un1.ip6_un1_plen =
				htons(sizeof(struct pcap_udp_header) + data.len);
		v_pcap_ipv6_header.ip6_ctlun.ip6_un1.ip6_un1_nxt = IPPROTO_UDP;
	} else {
		LM_DBG("ipv4 = %s -> %s\n", spd->src_ip.s, spd->dst_ip.s);
		v_pcap_ethernet_header.type = htons(0x0800); /* IPv4 packet */
		pcap_ip_header = &v_pcap_ipv4_header;
		pcap_ip_header_len = sizeof(struct pcap_ipv4_header);
		if(inet_pton(AF_INET, spd->src_ip.s, &ip4addr) != 1) {
			LM_ERR("failed to parse IPv4 address %s\n", spd->src_ip.s);
			return;
		}
		memcpy(&v_pcap_ipv4_header.ip_src, &ip4addr, sizeof(uint32_t));
		if(inet_pton(AF_INET, spd->dst_ip.s, &ip4addr) != 1) {
			LM_ERR("failed to parse IPv4 address %s\n", spd->dst_ip.s);
			return;
		}
		memcpy(&v_pcap_ipv4_header.ip_dst, &ip4addr, sizeof(uint32_t));
		v_pcap_ipv4_header.ip_len =
				htons(sizeof(struct pcap_udp_header)
						+ sizeof(struct pcap_ipv4_header) + data.len);
		v_pcap_ipv4_header.ip_protocol = IPPROTO_UDP; /* UDP */
	}

	/* add up all the sizes for this record */
	v_pcap_record_header.orig_len = sizeof(struct pcap_ethernet_header)
									+ pcap_ip_header_len
									+ sizeof(struct pcap_udp_header) + data.len;
	v_pcap_record_header.incl_len = v_pcap_record_header.orig_len;

	if(fwrite(&v_pcap_record_header, sizeof(struct pcap_record_header), 1, fs)
			!= 1) {
		LM_ERR("writing PCAP header failed: %s\n", strerror(errno));
	}
	if(fwrite(&v_pcap_ethernet_header, sizeof(struct pcap_ethernet_header), 1,
			   fs)
			!= 1) {
		LM_ERR("writing ethernet header to pcap failed: %s\n", strerror(errno));
	}
	if(fwrite(pcap_ip_header, pcap_ip_header_len, 1, fs) != 1) {
		LM_ERR("writing IP header to pcap failed: %s\n", strerror(errno));
	}
	if(fwrite(&v_pcap_udp_header, sizeof(struct pcap_udp_header), 1, fs) != 1) {
		LM_ERR("writing UDP header to pcap failed: %s\n", strerror(errno));
	}
	if(fwrite(data.s, data.len, 1, fs) != 1) {
		LM_ERR("writing UDP payload to pcap failed: %s\n", strerror(errno));
	}
	fflush(fs);
}
