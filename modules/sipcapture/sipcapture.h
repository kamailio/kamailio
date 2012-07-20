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

#ifdef __OS_solaris
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
#define IPPROTO_IPIP IPPROTO_ENCAP /* Solaris IPIP protocol has name ENCAP */
#endif

struct _sipcapture_object {
	str method;
	str reply_reason;
	str ruri;
	str ruri_user;
	str from_user;
	str from_tag;
	str to_user;
	str to_tag;
	str pid_user;
	str contact_user;
	str auth_user;
	str callid;
	str callid_aleg;
	str via_1;
	str via_1_branch;
	str cseq;
	str diversion;
	str reason;
	str content_type;
	str authorization;
	str user_agent;
	str source_ip;
	int source_port;
	str destination_ip;
	int destination_port;
	str contact_ip;
	int contact_port;
	str originator_ip;
	int originator_port;
	int proto;
	int family;
	str rtp_stat;
	int type;
        long long tmstamp;
	str node;	
	str msg;	
#ifdef STATISTICS
	stat_var *stat;
#endif
};

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

struct hep_timehdr{
   u_int32_t tv_sec;         /* seconds */
   u_int32_t tv_usec;        /* useconds */
   u_int16_t captid;          /* Capture ID node */
};

#ifdef USE_IPV6
struct hep_ip6hdr {
        struct in6_addr hp6_src;        /* source address */
        struct in6_addr hp6_dst;        /* destination address */
};
#endif
