/*
 * ipops module - IPv4 and Ipv6 operations
 *
 * Copyright (C) 2011 Iñaki Baz Castillo
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 *  2015-03-31: Added srv_query function (rboisvert)
 *  2011-07-29: Added a function to detect RFC1918 private IPv4 addresses (ibc)
 *  2011-04-27: Initial version (ibc)
 */
/*!
 * \file
 * \brief SIP-router ipops :: Module interface
 * \ingroup ipops
 * Copyright (C) 2011 Iñaki Baz Castillo
 * Module: \ref ipops
 */

/*! \defgroup ipops SIP-router ipops Module
 *
 * The ipops module provide IPv4 and IPv6 operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../resolve.h"
#include "api.h"
#include "ipops_pv.h"
#include "ip_parser.h"
#include "rfc1918_parser.h"

MODULE_VERSION


/*
 * Module parameter variables
 */


/*
 * Module core functions
 */


/*
 * Module internal functions
 */
int _compare_ips(char*, size_t, enum enum_ip_type, char*, size_t, enum enum_ip_type);
int _ip_is_in_subnet(char *ip1, size_t len1, enum enum_ip_type ip1_type, char *ip2, size_t len2, enum enum_ip_type ip2_type, int netmask);


/*
 * Script functions
 */
static int w_is_ip(struct sip_msg*, char*);
static int w_is_pure_ip(struct sip_msg*, char*);
static int w_is_ipv4(struct sip_msg*, char*);
static int w_is_ipv6(struct sip_msg*, char*);
static int w_is_ipv6_reference(struct sip_msg*, char*);
static int w_ip_type(struct sip_msg*, char*);
static int w_compare_ips(struct sip_msg*, char*, char*);
static int w_compare_pure_ips(struct sip_msg*, char*, char*);
static int w_is_ip_rfc1918(struct sip_msg*, char*);
static int w_ip_is_in_subnet(struct sip_msg*, char*, char*);
static int w_dns_sys_match_ip(sip_msg_t*, char*, char*);
static int w_dns_int_match_ip(sip_msg_t*, char*, char*);

static int w_dns_query(struct sip_msg* msg, char* str1, char* str2);
static int w_srv_query(struct sip_msg* msg, char* str1, char* str2);

static pv_export_t mod_pvs[] = {
	{ {"dns", sizeof("dns")-1}, PVT_OTHER, pv_get_dns, 0,
		pv_parse_dns_name, 0, 0, 0 },
	{ {"srvquery", sizeof("srvquery")-1}, PVT_OTHER, pv_get_srv, 0,
		pv_parse_srv_name, 0, 0, 0 },
	{ {"HN", sizeof("HN")-1}, PVT_OTHER, pv_get_hn, 0,
		pv_parse_hn_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 * Exported functions
 */
static cmd_export_t cmds[] =
{
	{ "is_ip", (cmd_function)w_is_ip, 1, fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "is_pure_ip", (cmd_function)w_is_pure_ip, 1, fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "is_ipv4", (cmd_function)w_is_ipv4, 1, fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "is_ipv6", (cmd_function)w_is_ipv6, 1, fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "is_ipv6_reference", (cmd_function)w_is_ipv6_reference, 1, fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "ip_type", (cmd_function)w_ip_type, 1, fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "compare_ips", (cmd_function)w_compare_ips, 2, fixup_spve_spve, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "compare_pure_ips", (cmd_function)w_compare_pure_ips, 2, fixup_spve_spve, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "is_ip_rfc1918", (cmd_function)w_is_ip_rfc1918, 1, fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "is_in_subnet", (cmd_function)w_ip_is_in_subnet, 2, fixup_spve_spve, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "dns_sys_match_ip", (cmd_function)w_dns_sys_match_ip, 2, fixup_spve_spve, 0,
		ANY_ROUTE },
	{ "dns_int_match_ip", (cmd_function)w_dns_int_match_ip, 2, fixup_spve_spve, 0,
		ANY_ROUTE },
	{ "dns_query", (cmd_function)w_dns_query, 2, fixup_spve_spve, 0,
		ANY_ROUTE },
	{ "srv_query", (cmd_function)w_srv_query, 2, fixup_spve_spve, 0,
		ANY_ROUTE },
	{ "bind_ipops", (cmd_function)bind_ipops, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0 }
};


/*
 * Module interface
 */
struct module_exports exports = {
	"ipops",                   /*!< module name */
	DEFAULT_DLFLAGS,           /*!< dlopen flags */
	cmds,                      /*!< exported functions */
	0,                         /*!< exported parameters */
	0,                         /*!< exported statistics */
	0,                         /*!< exported MI functions */
	mod_pvs,                   /*!< exported pseudo-variables */
	0,                         /*!< extra processes */
	0,                         /*!< module initialization function */
	(response_function) 0,     /*!< response handling function */
	0,                         /*!< destroy function */
	0                          /*!< per-child init function */
};


/*
 * Module internal functions
 */

/*! \brief Return 1 if both pure IP's are equal, 0 otherwise. */
int _compare_ips(char *ip1, size_t len1, enum enum_ip_type ip1_type, char *ip2, size_t len2, enum enum_ip_type ip2_type)
{
	struct in_addr in_addr1, in_addr2;
	struct in6_addr in6_addr1, in6_addr2;
	char _ip1[INET6_ADDRSTRLEN], _ip2[INET6_ADDRSTRLEN];

	// Not same IP type, return false.
	if (ip1_type != ip2_type)
		return 0;

	memcpy(_ip1, ip1, len1);
	_ip1[len1] = '\0';
	memcpy(_ip2, ip2, len2);
	_ip2[len2] = '\0';

	switch(ip1_type) {
		// Comparing IPv4 with IPv4.
		case(ip_type_ipv4):
			if (inet_pton(AF_INET, _ip1, &in_addr1) == 0)  return 0;
			if (inet_pton(AF_INET, _ip2, &in_addr2) == 0)  return 0;
			if (in_addr1.s_addr == in_addr2.s_addr)
				return 1;
			else
				return 0;
			break;
			// Comparing IPv6 with IPv6.
		case(ip_type_ipv6):
			if (inet_pton(AF_INET6, _ip1, &in6_addr1) != 1)  return 0;
			if (inet_pton(AF_INET6, _ip2, &in6_addr2) != 1)  return 0;
			if (memcmp(in6_addr1.s6_addr, in6_addr2.s6_addr, sizeof(in6_addr1.s6_addr)) == 0)
				return 1;
			else
				return 0;
			break;
		default:
			return 0;
			break;
	}
}

/*! \brief Return 1 if IP1 is in the subnet given by IP2 and the netmask, 0 otherwise. */
int _ip_is_in_subnet(char *ip1, size_t len1, enum enum_ip_type ip1_type, char *ip2, size_t len2, enum enum_ip_type ip2_type, int netmask)
{
	struct in_addr in_addr1, in_addr2;
	struct in6_addr in6_addr1, in6_addr2;
	char _ip1[INET6_ADDRSTRLEN], _ip2[INET6_ADDRSTRLEN];
	uint32_t ipv4_mask;
	uint8_t ipv6_mask[16];
	int i;

	// Not same IP type, return false.
	if (ip1_type != ip2_type)
		return 0;

	memcpy(_ip1, ip1, len1);
	_ip1[len1] = '\0';
	memcpy(_ip2, ip2, len2);
	_ip2[len2] = '\0';

	switch(ip1_type) {
		// Comparing IPv4 with IPv4.
		case(ip_type_ipv4):
			if (inet_pton(AF_INET, _ip1, &in_addr1) == 0)  return 0;
			if (inet_pton(AF_INET, _ip2, &in_addr2) == 0)  return 0;
			if (netmask <0 || netmask > 32)  return 0;
			if (netmask == 32) ipv4_mask = 0xFFFFFFFF;
			else ipv4_mask = htonl(~(0xFFFFFFFF >> netmask));
			if ((in_addr1.s_addr & ipv4_mask) == in_addr2.s_addr)
				return 1;
			else
				return 0;
			break;
			// Comparing IPv6 with IPv6.
		case(ip_type_ipv6):
			if (inet_pton(AF_INET6, _ip1, &in6_addr1) != 1)  return 0;
			if (inet_pton(AF_INET6, _ip2, &in6_addr2) != 1)  return 0;
			if (netmask <0 || netmask > 128)  return 0;
			for (i=0; i<16; i++)
			{
				if (netmask > ((i+1)*8)) ipv6_mask[i] = 0xFF;
				else if (netmask > (i*8))  ipv6_mask[i] = ~(0xFF >> (netmask-(i*8)));
				else ipv6_mask[i] = 0x00;
			}
			for (i=0; i<16; i++)  in6_addr1.s6_addr[i] &= ipv6_mask[i];
			if (memcmp(in6_addr1.s6_addr, in6_addr2.s6_addr, sizeof(in6_addr1.s6_addr)) == 0)
				return 1;
			else
				return 0;
			break;
		default:
			return 0;
			break;
	}
}



/*
 * Script functions
 */

/*! \brief Return true if the given argument (string or pv) is a valid IPv4, IPv6 or IPv6 reference. */
static int w_is_ip(struct sip_msg* _msg, char* _s)
{
	str string;

	if (_s == NULL) {
		LM_ERR("bad parameter\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s, &string))
	{
		LM_ERR("cannot print the format for string\n");
		return -3;
	}

	if (ip_parser_execute(string.s, string.len) != ip_type_error)
		return 1;
	else
		return -1;
}


/*! \brief Return true if the given argument (string or pv) is a valid IPv4 or IPv6. */
static int w_is_pure_ip(struct sip_msg* _msg, char* _s)
{
	str string;

	if (_s == NULL) {
		LM_ERR("bad parameter\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s, &string))
	{
		LM_ERR("cannot print the format for string\n");
		return -3;
	}

	switch(ip_parser_execute(string.s, string.len)) {
		case(ip_type_ipv4):
			return 1;
			break;
		case(ip_type_ipv6):
			return 1;
			break;
		default:
			return -1;
			break;
	}
}


/*! \brief Return true if the given argument (string or pv) is a valid IPv4. */
static int w_is_ipv4(struct sip_msg* _msg, char* _s)
{
	str string;

	if (_s == NULL) {
		LM_ERR("bad parameter\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s, &string))
	{
		LM_ERR("cannot print the format for string\n");
		return -3;
	}

	if (ip_parser_execute(string.s, string.len) == ip_type_ipv4)
		return 1;
	else
		return -1;
}


/*! \brief Return true if the given argument (string or pv) is a valid IPv6. */
static int w_is_ipv6(struct sip_msg* _msg, char* _s)
{
	str string;

	if (_s == NULL) {
		LM_ERR("bad parameter\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s, &string))
	{
		LM_ERR("cannot print the format for string\n");
		return -3;
	}

	if (ip_parser_execute(string.s, string.len) == ip_type_ipv6)
		return 1;
	else
		return -1;
}


/*! \brief Return true if the given argument (string or pv) is a valid IPv6 reference. */
static int w_is_ipv6_reference(struct sip_msg* _msg, char* _s)
{
	str string;

	if (_s == NULL) {
		LM_ERR("bad parameter\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s, &string))
	{
		LM_ERR("cannot print the format for string\n");
		return -3;
	}

	if (ip_parser_execute(string.s, string.len) == ip_type_ipv6_reference)
		return 1;
	else
		return -1;
}


/*! \brief Return the IP type of the given argument (string or pv): 1 = IPv4, 2 = IPv6, 3 = IPv6 refenrece, -1 = invalid IP. */
static int w_ip_type(struct sip_msg* _msg, char* _s)
{
	str string;

	if (_s == NULL) {
		LM_ERR("bad parameter\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s, &string))
	{
		LM_ERR("cannot print the format for string\n");
		return -3;
	}

	switch (ip_parser_execute(string.s, string.len)) {
		case(ip_type_ipv4):
			return 1;
			break;
		case(ip_type_ipv6):
			return 2;
			break;
		case(ip_type_ipv6_reference):
			return 3;
			break;
		default:
			return -1;
			break;
	}
}


/*! \brief Return true if both IP's (string or pv) are equal. This function also allows comparing an IPv6 with an IPv6 reference. */
static int w_compare_ips(struct sip_msg* _msg, char* _s1, char* _s2)
{
	str string1, string2;
	enum enum_ip_type ip1_type, ip2_type;

	if (_s1 == NULL || _s2 == NULL ) {
		LM_ERR("bad parameters\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s1, &string1))
	{
		LM_ERR("cannot print the format for first string\n");
		return -3;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s2, &string2))
	{
		LM_ERR("cannot print the format for second string\n");
		return -3;
	}

	switch(ip1_type = ip_parser_execute(string1.s, string1.len)) {
		case(ip_type_error):
			return -1;
			break;
		case(ip_type_ipv6_reference):
			string1.s += 1;
			string1.len -= 2;
			ip1_type = ip_type_ipv6;
			break;
		default:
			break;
	}
	switch(ip2_type = ip_parser_execute(string2.s, string2.len)) {
		case(ip_type_error):
			return -1;
			break;
		case(ip_type_ipv6_reference):
			string2.s += 1;
			string2.len -= 2;
			ip2_type = ip_type_ipv6;
			break;
		default:
			break;
	}

	if (_compare_ips(string1.s, string1.len, ip1_type, string2.s, string2.len, ip2_type))
		return 1;
	else
		return -1;
}


/*! \brief Return true if both pure IP's (string or pv) are equal. IPv6 references not allowed. */
static int w_compare_pure_ips(struct sip_msg* _msg, char* _s1, char* _s2)
{
	str string1, string2;
	enum enum_ip_type ip1_type, ip2_type;

	if (_s1 == NULL || _s2 == NULL ) {
		LM_ERR("bad parameters\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s1, &string1))
	{
		LM_ERR("cannot print the format for first string\n");
		return -3;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s2, &string2))
	{
		LM_ERR("cannot print the format for second string\n");
		return -3;
	}

	switch(ip1_type = ip_parser_execute(string1.s, string1.len)) {
		case(ip_type_error):
			return -1;
			break;
		case(ip_type_ipv6_reference):
			return -1;
			break;
		default:
			break;
	}
	switch(ip2_type = ip_parser_execute(string2.s, string2.len)) {
		case(ip_type_error):
			return -1;
			break;
		case(ip_type_ipv6_reference):
			return -1;
			break;
		default:
			break;
	}

	if (_compare_ips(string1.s, string1.len, ip1_type, string2.s, string2.len, ip2_type))
		return 1;
	else
		return -1;
}


/*! \brief Return true if the first IP (string or pv) is within the subnet defined by the second IP in CIDR notation. IPv6 references not allowed. */
static int w_ip_is_in_subnet(struct sip_msg* _msg, char* _s1, char* _s2)
{
	str string1, string2;
	enum enum_ip_type ip1_type, ip2_type;
	char *cidr_pos = NULL;
	int netmask = 0;

	if (_s1 == NULL || _s2 == NULL ) {
		LM_ERR("bad parameters\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s1, &string1))
	{
		LM_ERR("cannot print the format for first string\n");
		return -3;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s2, &string2))
	{
		LM_ERR("cannot print the format for second string\n");
		return -3;
	}

	switch(ip1_type = ip_parser_execute(string1.s, string1.len)) {
		case(ip_type_error):
			return -1;
			break;
		case(ip_type_ipv6_reference):
			return -1;
			break;
		default:
			break;
	}
	cidr_pos = string2.s + string2.len - 1;
	while (cidr_pos > string2.s)
	{
		if (*cidr_pos == '/')
		{
			string2.len = (cidr_pos - string2.s);
			netmask = atoi(cidr_pos+1);
			break;
		}
		cidr_pos--;
	}
	switch(ip2_type = ip_parser_execute(string2.s, string2.len)) {
		case(ip_type_error):
			return -1;
			break;
		case(ip_type_ipv6_reference):
			return -1;
			break;
		default:
			break;
	}

	if (netmask == 0)
	{
		if (_compare_ips(string1.s, string1.len, ip1_type, string2.s, string2.len, ip2_type))
			return 1;
		else
			return -1;
	}
	else
	{
		if (_ip_is_in_subnet(string1.s, string1.len, ip1_type, string2.s, string2.len, ip2_type, netmask))
			return 1;
		else
			return -1;
	}
}


/*! \brief Return true if the given argument (string or pv) is a valid RFC 1918 IPv4 (private address). */
static int w_is_ip_rfc1918(struct sip_msg* _msg, char* _s)
{
	str string;

	if (_s == NULL) {
		LM_ERR("bad parameter\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s, &string))
	{
		LM_ERR("cannot print the format for string\n");
		return -3;
	}

	if (rfc1918_parser_execute(string.s, string.len) == 1)
		return 1;
	else
		return -1;
}

static inline ip_addr_t *strtoipX(str *ips)
{
	/* try to figure out INET class */
	if(ips->s[0] == '[' || memchr(ips->s, ':', ips->len)!=NULL)
	{
		/* IPv6 */
		return str2ip6(ips);
	} else {
		/* IPv4 */
		return str2ip(ips);
	}
}

static int w_dns_sys_match_ip(sip_msg_t *msg, char *hnp, char *ipp)
{
	struct addrinfo hints, *res, *p;
	int status;
	ip_addr_t *ipa;
	void *addr;
	str hns;
	str ips;
	struct sockaddr_in *ipv4;
	struct sockaddr_in6 *ipv6;

	if (fixup_get_svalue(msg, (gparam_p)hnp, &hns))
	{
		LM_ERR("cannot evaluate hostname parameter\n");
		return -2;
	}

	if (fixup_get_svalue(msg, (gparam_p)ipp, &ips))
	{
		LM_ERR("cannot evaluate ip address parameter\n");
		return -2;
	}

	ipa = strtoipX(&ips);
	if(ipa==NULL)
	{
		LM_ERR("invalid ip address: %.*s\n", ips.len, ips.s);
		return -3;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /* allow any of AF_INET or AF_INET6 */
	// hints.ai_socktype = SOCK_STREAM;
	hints.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(hns.s, NULL, &hints, &res)) != 0)
	{
		LM_ERR("getaddrinfo: %s\n", gai_strerror(status));
		return -4;
	}

	for(p = res;p != NULL; p = p->ai_next)
	{
		if(p->ai_family==ipa->af)
		{
			if (p->ai_family==AF_INET)
			{
				ipv4 = (struct sockaddr_in *)p->ai_addr;
				addr = &(ipv4->sin_addr);
			} else {
				ipv6 = (struct sockaddr_in6 *)p->ai_addr;
				addr = &(ipv6->sin6_addr);
			}
			if(memcmp(ipa->u.addr, addr, ipa->len)==0)
			{
				/* matched IP */
				freeaddrinfo(res);
				return 1;
			}
		}
	}
	freeaddrinfo(res);

	return -1;
}

static int w_dns_int_match_ip(sip_msg_t *msg, char *hnp, char *ipp)
{
	ip_addr_t *ipa;
	str hns;
	str ips;
	struct hostent* he;
	char ** h;

	if (fixup_get_svalue(msg, (gparam_p)hnp, &hns))
	{
		LM_ERR("cannot evaluate hostname parameter\n");
		return -2;
	}

	if (fixup_get_svalue(msg, (gparam_p)ipp, &ips))
	{
		LM_ERR("cannot evaluate ip address parameter\n");
		return -2;
	}

	ipa = strtoipX(&ips);
	if(ipa==NULL)
	{
		LM_ERR("invalid ip address: %.*s\n", ips.len, ips.s);
		return -3;
	}

	he=resolvehost(hns.s);
	if (he==0) {
		DBG("could not resolve %s\n", hns.s);
		return -4;
	}

	if (he->h_addrtype==ipa->af)
	{
		for(h=he->h_addr_list; (*h); h++)
		{
			if(memcmp(ipa->u.addr, *h, ipa->len)==0)
			{
				/* match */
				return 1;
			}
		}
	}
	/* no match */
	return -1;
}

/**
 *
 */
static int w_dns_query(struct sip_msg* msg, char* str1, char* str2)
{
	str hostname;
	str name;

	if(msg==NULL)
	{
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)str1, &hostname)<0)
	{
		LM_ERR("cannot get the hostname\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)str2, &name)<0)
	{
		LM_ERR("cannot get the pv container name\n");
		return -1;
	}

	return dns_update_pv(&hostname, &name);
}

/**
 *
 */
static int w_srv_query(struct sip_msg* msg, char* str1, char* str2)
{
	str srvcname;
	str name;

	if(msg==NULL)
	{
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)str1, &srvcname)<0)
	{
		LM_ERR("cannot get the srvcname\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)str2, &name)<0)
	{
		LM_ERR("cannot get the pvid name\n");
		return -1;
	}

	return srv_update_pv(&srvcname, &name);
}
