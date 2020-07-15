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

#include "../../core/ip_addr.h"
#include "../../core/resolve.h"

#include "sipdump_write.h"

/* structures related to PCAP headers imported from open source Asterisk project
 * fuction to write to PCAP file adapted for internal structures
 * source: res_pjsip_logger.c - License GPLv2 -  Copyright (C) Digium, Inc.*/

/* PCAP Header */
struct pcap_header {
	uint32_t magic_number; 	/* PCAP file format magic number */
	uint16_t version_major;	/* Major version number of the file format */
	uint16_t version_minor;	/* Minor version number of the file format */
	int32_t thiszone;	/* GMT to local correction */
	uint32_t sigfigs;	/* Accuracy of timestamps */
	uint32_t snaplen;	/* The maximum size that can be recorded in the file */
	uint32_t network;	/* Type of packets held within the file */
};

/* PCAP Packet Record Header */
struct pcap_record_header {
	uint32_t ts_sec;	/* When the record was created */
	uint32_t ts_usec;	/* When the record was created */
	uint32_t incl_len;	/* Length of packet as saved in the file */
	uint32_t orig_len;	/* Length of packet as sent over network */
};

/* PCAP Ethernet Header */
struct pcap_ethernet_header {
	uint8_t dst[6];	/* Destination MAC address */
	uint8_t src[6];	/* Source MAC address */
	uint16_t type;	/*!The type of packet contained within */
} __attribute__((__packed__));

/*! PCAP IPv4 Header */
struct pcap_ipv4_header {
	uint8_t ver_ihl;	/* IP header version and other bits */
	uint8_t ip_tos;		/*  Type of service details */
	uint16_t ip_len;	/* Total length of the packet (including IPv4 header) */
	uint16_t ip_id;		/* Identification value */
	uint16_t ip_off;	/* Fragment offset */
	uint8_t ip_ttl;		/* Time to live for the packet */
	uint8_t ip_protocol;	/* Protocol of the data held within the packet (always UDP) */
	uint16_t ip_sum;	/* Checksum (not calculated for our purposes */
	uint32_t ip_src;	/* Source IP address */
	uint32_t ip_dst;	/* Destination IP address */
};

/* PCAP IPv6 Header */
struct pcap_ipv6_header {
   union {
      struct ip6_hdrctl {
         uint32_t ip6_un1_flow; /* Version, traffic class, flow label */
         uint16_t ip6_un1_plen; /* Length of the packet (not including IPv6 header) */
         uint8_t ip6_un1_nxt; 	/* Next header field */
         uint8_t ip6_un1_hlim;	/* Hop Limit */
      } ip6_un1;
      uint8_t ip6_un2_vfc;	/* Version, traffic class */
   } ip6_ctlun;
   struct in6_addr ip6_src; /* Source IP address */
   struct in6_addr ip6_dst; /* Destination IP address */
};

/* PCAP UDP Header */
struct pcap_udp_header {
	uint16_t src;		/* Source IP port */
	uint16_t dst;		/* Destination IP port */
	uint16_t length;	/* Length of the UDP header plus UDP packet */
	uint16_t checksum;	/* Packet checksum, left uncalculated for our purposes */
};

void sipdump_init_pcap(FILE *fs)
{
	struct pcap_header v_pcap_header = {
		.magic_number = 0xa1b2c3d4,
		.version_major = 2,
		.version_minor = 4,
		.snaplen = 65535,
		.network = 1, /* use ethernet to combine IPv4 and IPv6 in same pcap */
	};

	LM_DBG("writing the pcap file header\n");
	if(fwrite(&v_pcap_header, sizeof(struct pcap_header), 1, fs) != 1) {
		LM_ERR("failed to write the pcap file header\n");
	}
}

void sipdump_write_pcap(FILE *fs, sipdump_data_t *spd)
{
	struct pcap_record_header v_pcap_record_header = {
		.ts_sec = spd->tv.tv_sec,
		.ts_usec = spd->tv.tv_usec,
	};
	struct pcap_ethernet_header v_pcap_ethernet_header = {
		.type = 0,
	};
	struct pcap_ipv4_header v_pcap_ipv4_header = {
		.ver_ihl = 0x45, /* IPv4 + 20 bytes of header */
		.ip_ttl = 128, /* We always put a TTL of 128 to keep Wireshark less blue */
	};
	struct pcap_ipv6_header v_pcap_ipv6_header = {
		.ip6_ctlun.ip6_un2_vfc = 0x60,
	};
	void *pcap_ip_header;
	size_t pcap_ip_header_len;
	struct pcap_udp_header v_pcap_udp_header;
	ip_addr_t src_ip;
	ip_addr_t dst_ip;

	if(fs == NULL || spd == NULL) {
		return;
	}

	/* always store UDP */
	v_pcap_udp_header.src = ntohs(spd->src_port);
	v_pcap_udp_header.dst = ntohs(spd->dst_port);
	v_pcap_udp_header.length = ntohs(sizeof(struct pcap_udp_header) + spd->data.len);

	/* IP header */
	if (spd->afid == AF_INET6) {
		v_pcap_ethernet_header.type = htons(0x86DD); /* IPv6 packet */
		pcap_ip_header = &v_pcap_ipv6_header;
		pcap_ip_header_len = sizeof(struct pcap_ipv6_header);
		str2ip6buf(&spd->src_ip, &src_ip);
		memcpy(&v_pcap_ipv6_header.ip6_src, src_ip.u.addr16, src_ip.len);
		str2ip6buf(&spd->dst_ip, &dst_ip);
		memcpy(&v_pcap_ipv6_header.ip6_dst, dst_ip.u.addr16, dst_ip.len);
		v_pcap_ipv6_header.ip6_ctlun.ip6_un1.ip6_un1_plen = htons(sizeof(struct pcap_udp_header)
					+ spd->data.len);
		v_pcap_ipv6_header.ip6_ctlun.ip6_un1.ip6_un1_nxt = IPPROTO_UDP;
	} else {
		v_pcap_ethernet_header.type = htons(0x0800); /* IPv4 packet */
		pcap_ip_header = &v_pcap_ipv4_header;
		pcap_ip_header_len = sizeof(struct pcap_ipv4_header);
		str2ipbuf(&spd->src_ip, &src_ip);
		memcpy(&v_pcap_ipv6_header.ip6_src, src_ip.u.addr, src_ip.len);
		str2ipbuf(&spd->dst_ip, &dst_ip);
		memcpy(&v_pcap_ipv6_header.ip6_dst, dst_ip.u.addr, dst_ip.len);
		v_pcap_ipv4_header.ip_len = htons(sizeof(struct pcap_udp_header)
					+ sizeof(struct pcap_ipv4_header) + spd->data.len);
		v_pcap_ipv4_header.ip_protocol = IPPROTO_UDP; /* UDP */
	}

	/* add up all the sizes for this record */
	v_pcap_record_header.orig_len = sizeof(struct pcap_ethernet_header) + pcap_ip_header_len
			+ sizeof(struct pcap_udp_header) + spd->data.len;
	v_pcap_record_header.incl_len = v_pcap_record_header.orig_len;

	if (fwrite(&v_pcap_record_header, sizeof(struct pcap_record_header), 1, fs) != 1) {
		LM_ERR("writing PCAP header failed: %s\n", strerror(errno));
	}
	if (fwrite(&v_pcap_ethernet_header, sizeof(struct pcap_ethernet_header), 1, fs) != 1) {
		LM_ERR("writing ethernet header to pcap failed: %s\n", strerror(errno));
	}
	if (fwrite(pcap_ip_header, pcap_ip_header_len, 1, fs) != 1) {
		LM_ERR("writing IP header to pcap failed: %s\n", strerror(errno));
	}
	if (fwrite(&v_pcap_udp_header, sizeof(struct pcap_udp_header), 1, fs) != 1) {
		LM_ERR("writing UDP header to pcap failed: %s\n", strerror(errno));
	}
	if (fwrite(spd->data.s, spd->data.len, 1, fs) != 1) {
		LM_ERR("writing UDP payload to pcap failed: %s\n", strerror(errno));
	}
}