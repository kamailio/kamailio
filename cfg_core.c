/*
 * Copyright (C) 2007 iptelorg GmbH
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

/** Kamailio core :: core runtime config.
 * @file cfg_core.c
 * @ingroup core
 * Module: @ref core
 *
 * See
 * - @ref ConfigCoreDoc
 * - @ref ConfigEngine
 * - @ref cfg_core.h
 */
/*!
 * \page ConfigCoreDoc Documentation of configuration parser
 * \section ConfigParser Configuration parser
 * Further information
 * - \ref ConfigEngine
 * - \ref cfg_core.h
 * \verbinclude cfg.txt
 *
 */

#include "dprint.h"
#ifdef USE_DST_BLACKLIST
#include "dst_blacklist.h"
#endif
#include "resolve.h"
#ifdef USE_DNS_CACHE
#include "dns_cache.h"
#endif
#if defined PKG_MALLOC || defined SHM_MEM
#include "pt.h"
#endif
#include "msg_translator.h" /* fix_global_req_flags() */
#include "globals.h"
#include "sock_ut.h"
#include "cfg/cfg.h"
#include "cfg_core.h"

struct cfg_group_core default_core_cfg = {
	L_WARN, 	/*!<  print only msg. < L_WARN */
	LOG_DAEMON,	/*!< log_facility -- see syslog(3) */
	L_DBG+1,    /*!< memdbg */
#ifdef USE_DST_BLACKLIST
	/* blacklist */
	0, /*!< dst blacklist is disabled by default */
	DEFAULT_BLST_TIMEOUT,
	DEFAULT_BLST_MAX_MEM,
	0, /* blst_udp_imask */
	0, /* blst_tcp_imask */
	0, /* blst_tls_imask */
	0, /* blst_sctp_imask */
#endif
	/* resolver */
	1,  /*!< dns_try_ipv6 -- on by default */
	0,  /*!< dns_try_naptr -- off by default */
	30,  /*!< udp transport preference (for naptr) */
	20,  /*!< tcp transport preference (for naptr) */
	10,  /*!< tls transport preference (for naptr) */
	20,  /*!< sctp transport preference (for naptr) */
	-1, /*!< dns_retr_time */
	-1, /*!< dns_retr_no */
	-1, /*!< dns_servers_no */
	1,  /*!< dns_search_list */
	1,  /*!< dns_search_fmatch */
	0,  /*!< dns_reinit */
	1,  /*!< dns_naptr_ignore_rfc */
	/* DNS cache */
#ifdef USE_DNS_CACHE
	1,  /*!< use_dns_cache -- on by default */
	0,  /*!< dns_cache_flags */
	0,  /*!< use_dns_failover -- off by default */
	0,  /*!< dns_srv_lb -- off by default */
	DEFAULT_DNS_NEG_CACHE_TTL, /*!< neg. cache ttl */
	DEFAULT_DNS_CACHE_MIN_TTL, /*!< minimum ttl */
	DEFAULT_DNS_CACHE_MAX_TTL, /*!< maximum ttl */
	DEFAULT_DNS_MAX_MEM, /*!< dns_cache_max_mem */
	0, /*!< dns_cache_del_nonexp -- delete only expired entries by default */
	0, /*!< dns_cache_rec_pref -- 0 by default, do not check the existing entries. */
#endif
#ifdef PKG_MALLOC
	0, /*!< mem_dump_pkg */
#endif
#ifdef SHM_MEM
	0, /*!< mem_dump_shm */
#endif
	DEFAULT_MAX_WHILE_LOOPS, /*!< max_while_loops */
	0, /*!< udp_mtu (disabled by default) */
	0, /*!< udp_mtu_try_proto -> default disabled */
	0, /**< udp4_raw (disabled by default) */
	1500, /**< udp4_raw_mtu (1500 by default) */
	-1,  /**< udp4_raw_ttl (auto detect by default) */
	0,  /*!< force_rport */
	L_DBG+1, /*!< memlog */
	3, /*!< mem_summary -flags: 0 off, 1 pkg_status, 2 shm_status,
		4 pkg_sums, 8 shm_sums, 16 short_status */
	0, /*!< mem_safety - 0 disabled */
	0, /*!< mem_join - 0 disabled */
	L_ERR, /*!< corelog */
	L_ERR, /*!< latency log */
	0, /*!< latency limit db */
	0 /*!< latency limit action */
};

void	*core_cfg = &default_core_cfg;


static int check_raw_sock_support(void* cfg_h, str* gname, str* name,
									void** v)
{
	int val;
	
	val = (int)(long)(*v);
#ifndef USE_RAW_SOCKS
	if (val > 0) {
		ERR("no RAW_SOCKS support, please recompile with it enabled\n");
		return -1;
	}
	return 0;
#else /* USE_RAW_SOCKS */
	if (raw_udp4_send_sock < 0) {
		if (val > 0) {
			ERR("could not intialize raw socket on startup, please "
					"restart as root or with CAP_NET_RAW\n");
			return -1;
		} else if (val < 0) {
			/* auto and no socket => disable */
			*v = (void*)(long)0;
		}
	} else if (val < 0) {
		/* auto and socket => enable */
		*v = (void*)(long)1;
	}
	return 0;
#endif /* USE_RAW_SOCKS */
}



static int  udp4_raw_ttl_fixup(void* cfg_h, str* gname, str* name, void** val)
{
	int v;
	v = (int)(long)(*val);
	if (v < 0) {
		if (sendipv4)
			v = sock_get_ttl(sendipv4->socket);
	}
	if (v < 0) {
		/* some error => use a reasonable default */
		v = 63;
	}
	*val = (void*)(long)v;
	return 0;
}



cfg_def_t core_cfg_def[] = {
	{"debug",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"debug level"},
	{"log_facility",	CFG_VAR_INT|CFG_INPUT_STRING,	0, 0, log_facility_fixup, 0,
		"syslog facility, see \"man 3 syslog\""},
	{"memdbg",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"log level for memory debugging messages"},
#ifdef USE_DST_BLACKLIST
	/* blacklist */
	{"use_dst_blacklist",	CFG_VAR_INT,	0, 1, use_dst_blacklist_fixup, 0,
		"enable/disable destination blacklisting"},
	{"dst_blacklist_expire",	CFG_VAR_INT,	0, 0, 0, 0,
		"how much time (in s) a blacklisted destination is kept in the list"},
	{"dst_blacklist_mem",	CFG_VAR_INT,	0, 0, blst_max_mem_fixup, 0,
		"maximum shared memory amount (in KB) used for keeping the blacklisted"
			" destinations"},
	{"dst_blacklist_udp_imask", CFG_VAR_INT, 0, 0, 0, blst_reinit_ign_masks,
		"blacklist event ignore mask for UDP"},
	{"dst_blacklist_tcp_imask", CFG_VAR_INT, 0, 0, 0, blst_reinit_ign_masks,
		"blacklist event ignore mask for TCP"},
	{"dst_blacklist_tls_imask", CFG_VAR_INT, 0, 0, 0, blst_reinit_ign_masks,
		"blacklist event ignore mask for TLS"},
	{"dst_blacklist_sctp_imask", CFG_VAR_INT, 0, 0, 0, blst_reinit_ign_masks,
		"blacklist event ignore mask for SCTP"},
#endif
	/* resolver */
#ifdef USE_DNS_CACHE
	{"dns_try_ipv6",	CFG_VAR_INT,	0, 1, dns_try_ipv6_fixup, fix_dns_flags,
#else
	{"dns_try_ipv6",	CFG_VAR_INT,	0, 1, dns_try_ipv6_fixup, 0,
#endif
		"enable/disable IPv6 DNS lookups"},
#ifdef USE_DNS_CACHE
	{"dns_try_naptr",	CFG_VAR_INT,	0, 1, 0, fix_dns_flags,
#else
	{"dns_try_naptr",	CFG_VAR_INT,	0, 1, 0, 0,
#endif
		"enable/disable NAPTR DNS lookups"},
	{"dns_udp_pref",	CFG_VAR_INT,	0, 0, 0, reinit_proto_prefs,
		"udp protocol preference when doing NAPTR lookups"},
	{"dns_tcp_pref",	CFG_VAR_INT,	0, 0, 0, reinit_proto_prefs,
		"tcp protocol preference when doing NAPTR lookups"},
	{"dns_tls_pref",	CFG_VAR_INT,	0, 0, 0, reinit_proto_prefs,
		"tls protocol preference when doing NAPTR lookups"},
	{"dns_sctp_pref",	CFG_VAR_INT,	0, 0, 0, reinit_proto_prefs,
		"sctp protocol preference when doing NAPTR lookups"},
	{"dns_retr_time",	CFG_VAR_INT,	0, 0, 0, resolv_reinit,
		"time in s before retrying a dns request"},
	{"dns_retr_no",		CFG_VAR_INT,	0, 0, 0, resolv_reinit,
		"number of dns retransmissions before giving up"},
	{"dns_servers_no",	CFG_VAR_INT,	0, 0, 0, resolv_reinit,
		"how many dns servers from the ones defined in "
		"/etc/resolv.conf will be used"},
	{"dns_use_search_list",	CFG_VAR_INT,	0, 1, 0, resolv_reinit,
		"if set to 0, the search list in /etc/resolv.conf is ignored"},
	{"dns_search_full_match",	CFG_VAR_INT,	0, 1, 0, 0,
		"enable/disable domain name checks against the search list "
		"in DNS answers"},
	{"dns_reinit",		CFG_VAR_INT|CFG_INPUT_INT,	1, 1, dns_reinit_fixup,
		resolv_reinit,
		"set to 1 in order to reinitialize the DNS resolver"},
	{"dns_naptr_ignore_rfc",	CFG_VAR_INT,	0, 0, 0, reinit_proto_prefs,
		"ignore the Order field required by RFC 2915"},
	/* DNS cache */
#ifdef USE_DNS_CACHE
	{"use_dns_cache",	CFG_VAR_INT,	0, 1, use_dns_cache_fixup, 0,
		"enable/disable the dns cache"},
	{"dns_cache_flags",	CFG_VAR_INT,	0, 4, 0, fix_dns_flags,
		"dns cache specific resolver flags "
		"(1=ipv4 only, 2=ipv6 only, 4=prefer ipv6"},
	{"use_dns_failover",	CFG_VAR_INT,	0, 1, use_dns_failover_fixup, 0,
		"enable/disable dns failover in case the destination "
		"resolves to multiple ip addresses and/or multiple SRV records "
		"(depends on use_dns_cache)"},
	{"dns_srv_lb",		CFG_VAR_INT,	0, 1, 0, fix_dns_flags,
		"enable/disable load balancing to different srv records "
		"of the same priority based on the srv records weights "
		"(depends on dns_failover)"},
	{"dns_cache_negative_ttl",	CFG_VAR_INT,	0, 0, 0, 0,
		"time to live for negative results (\"not found\") "
		"in seconds. Use 0 to disable"},
	{"dns_cache_min_ttl",	CFG_VAR_INT,	0, 0, 0, 0,
		"minimum accepted time to live for a record, in seconds"},
	{"dns_cache_max_ttl",	CFG_VAR_INT,	0, 0, 0, 0,
		"maximum accepted time to live for a record, in seconds"},
	{"dns_cache_mem",	CFG_VAR_INT,	0, 0, dns_cache_max_mem_fixup, 0,
		"maximum memory used for the dns cache in Kb"},
	{"dns_cache_del_nonexp",	CFG_VAR_INT,	0, 1, 0, 0,
		"allow deletion of non-expired records from the cache when "
		"there is no more space left for new ones"},
	{"dns_cache_rec_pref",	CFG_VAR_INT,	0, 3, 0, 0,
		"DNS cache record preference: "
		" 0 - do not check duplicates"
		" 1 - prefer old records"
		" 2 - prefer new records"
		" 3 - prefer records with longer lifetime"},
#endif
#ifdef PKG_MALLOC
	{"mem_dump_pkg",	CFG_VAR_INT,	0, 0, 0, mem_dump_pkg_cb,
		"dump process memory status, parameter: pid_number"},
#endif
#ifdef SHM_MEM
	{"mem_dump_shm",	CFG_VAR_INT,	0, 0, mem_dump_shm_fixup, 0,
		"dump shared memory status"},
#endif
	{"max_while_loops",	CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"maximum iterations allowed for a while loop" },
	{"udp_mtu",	CFG_VAR_INT|CFG_ATOMIC,	0, 65535, 0, 0,
		"fallback to a congestion controlled protocol if send size"
			" exceeds udp_mtu"},
	{"udp_mtu_try_proto", CFG_VAR_INT, 1, 4, 0, fix_global_req_flags,
		"if send size > udp_mtu use proto (1 udp, 2 tcp, 3 tls, 4 sctp)"},
	{"udp4_raw", CFG_VAR_INT | CFG_ATOMIC, -1, 1, check_raw_sock_support, 0,
		"enable/disable using a raw socket for sending UDP IPV4 packets."
		" Should be  faster on multi-CPU linux running machines."},
	{"udp4_raw_mtu", CFG_VAR_INT | CFG_ATOMIC, 28, 65535, 0, 0,
		"set the MTU used when using raw sockets for udp sending."
		" This  value will be used when deciding whether or not to fragment"
		" the packets."},
	{"udp4_raw_ttl", CFG_VAR_INT | CFG_ATOMIC, -1, 255, udp4_raw_ttl_fixup, 0,
		"set the IP TTL used when using raw sockets for udp sending."
		" -1 will use the same value as for normal udp sockets."},
	{"force_rport",     CFG_VAR_INT, 0, 1,  0, fix_global_req_flags,
		"force rport for all the received messages" },
	{"memlog",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"log level for memory status/summary information"},
	{"mem_summary",	CFG_VAR_INT|CFG_ATOMIC,	0, 31, 0, 0,
		"memory debugging information displayed on exit (flags): "
		" 0 - off,"
		" 1 - dump all the pkg used blocks (status),"
		" 2 - dump all the shm used blocks (status),"
		" 4 - summary of pkg used blocks,"
		" 8 - summary of shm used blocks,"
		" 16 - short status instead of dump" },
	{"mem_safety",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"safety level for memory operations"},
	{"mem_join",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"join free memory fragments"},
	{"corelog",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"log level for non-critical core error messages"},
	{"latency_log",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"log level for latency limits alert messages"},
	{"latency_limit_db",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"limit is ms for alerting on time consuming db commands"},
	{"latency_limit_action",		CFG_VAR_INT|CFG_ATOMIC,	0, 0, 0, 0,
		"limit is ms for alerting on time consuming config actions"},
	{0, 0, 0, 0, 0, 0}
};
