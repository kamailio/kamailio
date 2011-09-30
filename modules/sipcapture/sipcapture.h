/*
 * $Id$
 *
 * hep related structure
 *
 * Copyright (C) 2011 Alexandr Dubovikov (QSC AG) (alexandr.dubovikov@gmail.com)
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


struct hep_hdr{
    u_int8_t hp_v;            /* version */
    u_int8_t hp_l;            /* length */
    u_int8_t hp_f;            /* family */
    u_int8_t hp_p;            /* protocol */
    u_int16_t hp_sport;       /* source port */
    u_int16_t hp_dport;       /* destination port */
};

struct hep_iphdr{
        struct in_addr hp_src;
        struct in_addr hp_dst;      /* source and dest address */
};

#ifdef USE_IPV6
struct hep_ip6hdr {
        struct in6_addr hp6_src;        /* source address */
        struct in6_addr hp6_dst;        /* destination address */
};
#endif

/* Make it independed */
/* Copied from linux/filter.h */
struct my_sock_filter      /* Filter block */
{
        __u16   code;   /* Actual filter code */
        __u8    jt;     /* Jump true */
        __u8    jf;     /* Jump false */
        __u32   k;      /* Generic multiuse field */
};

struct my_sock_fprog       /* Required for SO_ATTACH_FILTER. */
{
        unsigned short          len;    /* Number of filter blocks */
        struct my_sock_filter __user *filter;
};

#ifndef BPF_JUMP
#define BPF_JUMP(code, k, jt, jf) { (unsigned short)(code), jt, jf, k }
#endif

/* tcpdump -s 0 udp and portrange 5060-5090 -dd */
static struct my_sock_filter BPF_code[] = {
		{ 0x28, 0, 0, 0x0000000c },
		{ 0x15, 0, 7, 0x000086dd },
		{ 0x30, 0, 0, 0x00000014 },
		{ 0x15, 0, 18, 0x00000011 },
		{ 0x28, 0, 0, 0x00000036 },
		{ 0x35, 0, 1, 0x000013c4 },
		{ 0x25, 0, 14, 0x000013e2 },
		{ 0x28, 0, 0, 0x00000038 },
		{ 0x35, 11, 13, 0x000013c4 },
		{ 0x15, 0, 12, 0x00000800 },
		{ 0x30, 0, 0, 0x00000017 },
		{ 0x15, 0, 10, 0x00000011 },
		{ 0x28, 0, 0, 0x00000014 },
		{ 0x45, 8, 0, 0x00001fff },
		{ 0xb1, 0, 0, 0x0000000e },
		{ 0x48, 0, 0, 0x0000000e },
		{ 0x35, 0, 1, 0x000013c4 },
		{ 0x25, 0, 3, 0x000013e2 },
		{ 0x48, 0, 0, 0x00000010 },
		{ 0x35, 0, 2, 0x000013c4 },
		{ 0x25, 1, 0, 0x000013e2 },
		{ 0x6, 0, 0, 0x0000ffff },
		{ 0x6, 0, 0, 0x00000000 },
};


static struct my_sock_fprog Filter = {
           .len = sizeof(BPF_code) / sizeof(BPF_code[0]),
           .filter = (struct my_sock_filter *) BPF_code,
};
