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
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/str.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/resolve.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"
#include "api.h"
#include "ipops_pv.h"
#include "ip_parser.h"
#include "rfc1918_parser.h"
#include "detailed_ip_type.h"

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
int _compare_ips(char*, size_t, enum enum_ip_type, char*, size_t,
		enum enum_ip_type);
int _compare_ips_v4(struct in_addr *ip, char*, size_t);
int _compare_ips_v6(struct in6_addr *ip, char*, size_t);
int _ip_is_in_subnet(char *ip1, size_t len1, enum enum_ip_type ip1_type,
		char *ip2, size_t len2, enum enum_ip_type ip2_type, int netmask);
int _ip_is_in_subnet_v4(struct in_addr *ip, char *net, size_t netlen,
		int netmask);
int _ip_is_in_subnet_v6(struct in6_addr *ip, char *net, size_t netlen,
		int netmask);
int _ip_is_in_subnet_str(void *ip, enum enum_ip_type type, char *s, int slen);
int _ip_is_in_subnet_str_trimmed(void *ip, enum enum_ip_type type, char *b,
		char *e);
static int _detailed_ip_type(unsigned int _type, sip_msg_t* _msg,
		char* _s,  char *_dst);


/*
 * Script functions
 */
static int w_is_ip(sip_msg_t*, char*, char*);
static int w_is_pure_ip(sip_msg_t*, char*, char*);
static int w_is_ipv4(sip_msg_t*, char*, char*);
static int w_is_ipv6(sip_msg_t*, char*, char*);
static int w_is_ipv6_reference(sip_msg_t*, char*, char*);
static int w_ip_type(sip_msg_t*, char*, char*);
static int w_detailed_ipv6_type(sip_msg_t* _msg, char* _s,  char *res);
static int w_detailed_ipv4_type(sip_msg_t* _msg, char* _s,  char *res);
static int w_detailed_ip_type(sip_msg_t* _msg, char* _s,  char *res);
static int w_compare_ips(sip_msg_t*, char*, char*);
static int w_compare_pure_ips(sip_msg_t*, char*, char*);
static int w_is_ip_rfc1918(sip_msg_t*, char*, char*);
static int w_ip_is_in_subnet(sip_msg_t*, char*, char*);
static int w_dns_sys_match_ip(sip_msg_t*, char*, char*);
static int w_dns_int_match_ip(sip_msg_t*, char*, char*);
static int fixup_detailed_ip_type(void** param, int param_no);
static int fixup_free_detailed_ip_type(void** param, int param_no);
static int w_dns_query(sip_msg_t* msg, char* str1, char* str2);
static int w_srv_query(sip_msg_t* msg, char* str1, char* str2);
static int w_naptr_query(sip_msg_t* msg, char* str1, char* str2);
static int mod_init(void);

static pv_export_t mod_pvs[] = {
	{ {"dns", sizeof("dns")-1}, PVT_OTHER, pv_get_dns, 0,
		pv_parse_dns_name, 0, 0, 0 },
	{ {"srvquery", sizeof("srvquery")-1}, PVT_OTHER, pv_get_srv, 0,
		pv_parse_srv_name, 0, 0, 0 },
	{ {"naptrquery", sizeof("naptrquery")-1}, PVT_OTHER, pv_get_naptr, 0,
		pv_parse_naptr_name, 0, 0, 0 },
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
	{ "detailed_ipv4_type", (cmd_function)w_detailed_ipv4_type, 2,
		fixup_detailed_ip_type, fixup_free_detailed_ip_type, ANY_ROUTE },
	{ "detailed_ipv6_type", (cmd_function)w_detailed_ipv6_type, 2,
		fixup_detailed_ip_type, fixup_free_detailed_ip_type, ANY_ROUTE },
	{ "detailed_ip_type", (cmd_function)w_detailed_ip_type, 2,
		fixup_detailed_ip_type, fixup_free_detailed_ip_type, ANY_ROUTE },
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
	{ "naptr_query", (cmd_function)w_naptr_query, 2, fixup_spve_spve, 0,
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
	mod_init,                  /*!< module initialization function */
	(response_function) 0,     /*!< response handling function */
	0,                         /*!< destroy function */
	0                          /*!< per-child init function */
};


static int mod_init(void) {
	/* turn detailed_ip_type relevant structures to netowork byte order
	 * so no need to transform each ip to host order before comparing */
	ipv4ranges_hton();
	ipv6ranges_hton();
	return 0;
}


/* Fixup functions */

/*
 * Fix detailed_ipv6_type param: result (writable pvar).
 */
static int fixup_detailed_ip_type(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 2) {
		if (fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if (((pv_spec_t *) (*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int fixup_free_detailed_ip_type(void** param, int param_no)
{
	if (param_no == 1) {
		//LM_WARN("free function has not been defined for spve\n");
		return 0;
	}

	if (param_no == 2) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Module internal functions
 */

/*! \brief Return 1 if both pure IP's are equal, 0 otherwise. */
int _compare_ips(char *ip1, size_t len1, enum enum_ip_type ip1_type, char *ip2,
		size_t len2, enum enum_ip_type ip2_type)
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
			if (memcmp(in6_addr1.s6_addr, in6_addr2.s6_addr,
						sizeof(in6_addr1.s6_addr)) == 0)
				return 1;
			else
				return 0;
			break;
		default:
			return 0;
			break;
	}
}

int _compare_ips_v4(struct in_addr *ip, char* ip2, size_t len2)
{
	struct in_addr in_addr2;
	char _ip2[INET6_ADDRSTRLEN];

	memcpy(_ip2, ip2, len2);
	_ip2[len2] = '\0';

	if (inet_pton(AF_INET, _ip2, &in_addr2) == 0)  return 0;
	if (ip->s_addr == in_addr2.s_addr) return 1;
	return 0;
}

int _compare_ips_v6(struct in6_addr *ip, char* ip2, size_t len2)
{
	struct in6_addr in6_addr2;
	char _ip2[INET6_ADDRSTRLEN];

	memcpy(_ip2, ip2, len2);
	_ip2[len2] = '\0';

	if (inet_pton(AF_INET6, _ip2, &in6_addr2) != 1)
		return 0;
	if (memcmp(ip->s6_addr, in6_addr2.s6_addr, sizeof(ip->s6_addr)) == 0)
		return 1;
	return 0;
}

/*! \brief Return 1 if IP1 is in the subnet given by IP2 and the netmask,
 * 0 otherwise. */
int _ip_is_in_subnet(char *ip1, size_t len1, enum enum_ip_type ip1_type,
		char *ip2, size_t len2, enum enum_ip_type ip2_type, int netmask)
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
				else if (netmask > (i*8))
					ipv6_mask[i] = ~(0xFF >> (netmask-(i*8)));
				else ipv6_mask[i] = 0x00;
			}
			for (i=0; i<16; i++)  in6_addr1.s6_addr[i] &= ipv6_mask[i];
			if (memcmp(in6_addr1.s6_addr, in6_addr2.s6_addr,
						sizeof(in6_addr1.s6_addr)) == 0)
				return 1;
			else
				return 0;
			break;
		default:
			return 0;
			break;
	}
}

int _ip_is_in_subnet_v4(struct in_addr *ip, char *net, size_t netlen,
		int netmask)
{
	struct in_addr net_addr;
	char _net[INET6_ADDRSTRLEN];
	uint32_t ipv4_mask;

	memcpy(_net, net, netlen);
	_net[netlen] = '\0';

	if (inet_pton(AF_INET, _net, &net_addr) == 0)  return 0;
	if (netmask <0 || netmask > 32)  return 0;

	if (netmask == 32) ipv4_mask = 0xFFFFFFFF;
	else ipv4_mask = htonl(~(0xFFFFFFFF >> netmask));

	if ((ip->s_addr & ipv4_mask) == (net_addr.s_addr & ipv4_mask))
		return 1;
	return 0;
}

int _ip_is_in_subnet_v6(struct in6_addr *ip, char *net, size_t netlen,
		int netmask)
{
	struct in6_addr net_addr;
	char _net[INET6_ADDRSTRLEN];
	uint8_t ipv6_mask[16];
	int i;

	memcpy(_net, net, netlen);
	_net[netlen] = '\0';

	if (inet_pton(AF_INET6, _net, &net_addr) != 1)  return 0;
	if (netmask <0 || netmask > 128)  return 0;
	for (i=0; i<16; i++)
	{
		if (netmask > ((i+1)*8)) ipv6_mask[i] = 0xFF;
		else if (netmask > (i*8))  ipv6_mask[i] = ~(0xFF >> (netmask-(i*8)));
		else ipv6_mask[i] = 0x00;
	}
	for (i=0; i<16; i++)  ip->s6_addr[i] &= ipv6_mask[i];
	for (i=0; i<16; i++)  net_addr.s6_addr[i] &= ipv6_mask[i];
	if (memcmp(ip->s6_addr, net_addr.s6_addr, sizeof(net_addr.s6_addr)) == 0)
		return 1;
	return 0;
}

int _ip_is_in_subnet_str(void *ip, enum enum_ip_type type, char *s, int slen)
{
	enum enum_ip_type ip2_type;
	char *cidr_pos = NULL;
	int netmask = -1;

	cidr_pos = s + slen - 1;
	while (cidr_pos > s)
	{
		if (*cidr_pos == '/')
		{
			slen = (cidr_pos - s);
			netmask = atoi(cidr_pos+1);
			break;
		}
		cidr_pos--;
	}
	switch(ip2_type = ip_parser_execute(s, slen)) {
		case(ip_type_error):
			return -1;
			break;
		case(ip_type_ipv6_reference):
			return -1;
			break;
		default:
			break;
	}

	if(type!=ip2_type) return 0;

	if (netmask == -1)
	{
		switch(type){
			case ip_type_ipv4:
				if (_compare_ips_v4((struct in_addr *)ip, s, slen))
					return 1;
				else
					return -1;
				break;
			case ip_type_ipv6:
				if (_compare_ips_v6((struct in6_addr *)ip, s, slen))
					return 1;
				else
					return -1;
				break;
				break;
			default:
				break;
		}
	}
	else
	{
		switch(type){
			case ip_type_ipv4:
				if (_ip_is_in_subnet_v4((struct in_addr *)ip, s, slen,netmask))
					return 1;
				else
					return -1;
				break;
			case ip_type_ipv6:
				if (_ip_is_in_subnet_v6((struct in6_addr *)ip, s, slen,netmask))
					return 1;
				else
					return -1;
				break;
				break;
			default:
				break;
		}
	}
	return 0;
}

int _ip_is_in_subnet_str_trimmed(void *ip, enum enum_ip_type type, char *b,
		char *e)
{
	while(b<e && *b==' ') b++;
	while(b<e && *(e-1)==' ') e--;
	if(b==e) return 0;
	return _ip_is_in_subnet_str(ip,type,b,e-b);
}

/*
 * Script functions
 */

/*! \brief Return true if the given argument (string or pv) is a valid IPv4,
 * IPv6 or IPv6 reference. */
static int w_is_ip(sip_msg_t* _msg, char* _s, char *_p2)
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

/**
 * 
 */
static int ki_is_ip(sip_msg_t *msg, str *sval)
{
	if (ip_parser_execute(sval->s, sval->len) != ip_type_error)
		return 1;
	else
		return -1;
}

/*! \brief Return true if the given argument (string or pv) is a valid
 * IPv4 or IPv6. */
static int w_is_pure_ip(sip_msg_t* _msg, char* _s, char *_p2)
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

/**
 * 
 */
static int ki_is_pure_ip(sip_msg_t *msg, str *sval)
{
	switch(ip_parser_execute(sval->s, sval->len)) {
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
static int w_is_ipv4(sip_msg_t* _msg, char* _s, char *_p2)
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


/**
 * 
 */
static int ki_is_ip4(sip_msg_t *msg, str *sval)
{
	if (ip_parser_execute(sval->s, sval->len) == ip_type_ipv4)
		return 1;
	else
		return -1;
}

/*! \brief Return true if the given argument (string or pv) is a valid IPv6. */
static int w_is_ipv6(sip_msg_t* _msg, char* _s, char *_p2)
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

/**
 * 
 */
static int ki_is_ip6(sip_msg_t *msg, str *sval)
{
	if (ip_parser_execute(sval->s, sval->len) == ip_type_ipv6)
		return 1;
	else
		return -1;
}

/*! \brief Return true if the given argument (string or pv) is a valid
 * IPv6 reference. */
static int w_is_ipv6_reference(sip_msg_t* _msg, char* _s, char *_p2)
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


/**
 * 
 */
static int ki_is_ip6_reference(sip_msg_t *msg, str *sval)
{
	if (ip_parser_execute(sval->s, sval->len) == ip_type_ipv6_reference)
		return 1;
	else
		return -1;
}

/*! \brief Return the IP type of the given argument (string or pv):
 *  1 = IPv4, 2 = IPv6, 3 = IPv6 refenrece, -1 = invalid IP. */
static int w_ip_type(sip_msg_t* _msg, char* _s, char *_p2)
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

/*! \brief Return the IP type of the given argument (string or pv):
 *  1 = IPv4, 2 = IPv6, 3 = IPv6 refenrece, -1 = invalid IP. */
static int ki_ip_type(sip_msg_t *msg, str *sval)
{
	switch (ip_parser_execute(sval->s, sval->len)) {
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

static int w_detailed_ipv4_type(sip_msg_t* _msg, char* _s,  char *_dst)
{
	return _detailed_ip_type(ip_type_ipv4, _msg, _s, _dst);
}

static int w_detailed_ipv6_type(sip_msg_t* _msg, char* _s,  char *_dst)
{
	return _detailed_ip_type(ip_type_ipv6, _msg, _s, _dst);
}

static int w_detailed_ip_type(sip_msg_t* _msg, char* _s,  char *_dst)
{
	/* `ip_type_error` should read `unknown type` */
	return _detailed_ip_type(ip_type_error, _msg, _s, _dst);
}

static int _detailed_ip_type_helper(unsigned int _type, sip_msg_t* _msg,
		str* sval,  pv_spec_t *dst)
{
	str string;
	pv_value_t val;
	char *res;
	unsigned int assumed_type;

	string = *sval;
	assumed_type = (ip_type_error == _type)?
						ip_parser_execute(string.s, string.len) : _type;

	switch (assumed_type) {
		case ip_type_ipv4:
			if (!ip4_iptype(string, &res)) {
				LM_ERR("bad ip parameter\n");
				return -1;
			}
			break;
		case ip_type_ipv6_reference:
		case ip_type_ipv6:
			/* consider this reference */
			if (string.s[0] == '[') {
				string.s++;
				string.len -= 2;
			}
			if (!ip6_iptype(string, &res)) {
				LM_ERR("bad ip parameter\n");
				return -1;
			}
			break;
		default:
			return -1;
	}

	val.rs.s = res;
	val.rs.len = strlen(res);
	val.flags = PV_VAL_STR;
	dst->setf(_msg, &dst->pvp, (int)EQ_T, &val);
	return 1;
}

static int _detailed_ip_type(unsigned int _type, sip_msg_t* _msg,
		char* _s,  char *_dst)
{
	str string;
	pv_spec_t *dst;

	if (_s == NULL) {
		LM_ERR("bad parameter\n");
		return -2;
	}

	if (fixup_get_svalue(_msg, (gparam_p)_s, &string))
	{
		LM_ERR("cannot print the format for string\n");
		return -3;
	}
	dst = (pv_spec_t *)_dst;

	return _detailed_ip_type_helper(_type, _msg, &string, dst);
}

static int ki_detailed_ip_type_helper(unsigned int _type, sip_msg_t* _msg,
		str* _sval,  str *_dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(_dpv);
	if (dst == NULL) {
		LM_ERR("result pvar is not found: %.*s\n", _dpv->len, _dpv->s);
		return -1;
	}
	if (dst->setf == NULL) {
		LM_ERR("result pvar is not writeble: %.*s\n", _dpv->len, _dpv->s);
		return -1;
	}
	return _detailed_ip_type_helper(_type, _msg, _sval, dst);
}

static int ki_detailed_ipv4_type(sip_msg_t* _msg, str* _sval, str *_dpv)
{
	return ki_detailed_ip_type_helper(ip_type_ipv4, _msg, _sval, _dpv);
}

static int ki_detailed_ipv6_type(sip_msg_t* _msg, str* _sval, str *_dpv)
{
	return ki_detailed_ip_type_helper(ip_type_ipv6, _msg, _sval, _dpv);
}

static int ki_detailed_ip_type(sip_msg_t* _msg, str* _sval, str *_dpv)
{
	/* `ip_type_error` should read `unknown type` */
	return ki_detailed_ip_type_helper(ip_type_error, _msg, _sval, _dpv);
}

/*! \brief Return true if both IP's (string or pv) are equal.
 * This function also allows comparing an IPv6 with an IPv6 reference. */
static int ki_compare_ips(sip_msg_t* _msg, str* _sval1, str* _sval2)
{
	str string1, string2;
	enum enum_ip_type ip1_type, ip2_type;

	string1 = *_sval1;
	string2 = *_sval2;
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

	if (_compare_ips(string1.s, string1.len, ip1_type, string2.s, string2.len,
				ip2_type))
		return 1;
	else
		return -1;
}

/*! \brief Return true if both IP's (string or pv) are equal.
 * This function also allows comparing an IPv6 with an IPv6 reference. */
static int w_compare_ips(sip_msg_t* _msg, char* _s1, char* _s2)
{
	str string1, string2;

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

	return ki_compare_ips(_msg, &string1, &string2);
}

/*! \brief Return true if both pure IP's (string or pv) are equal.
 * IPv6 references not allowed. */
static int ki_compare_pure_ips(sip_msg_t* _msg, str* _sval1, str* _sval2)
{
	str string1, string2;
	enum enum_ip_type ip1_type, ip2_type;

	string1 = *_sval1;
	string2 = *_sval2;
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

	if (_compare_ips(string1.s, string1.len, ip1_type, string2.s, string2.len,
				ip2_type))
		return 1;
	else
		return -1;
}

/*! \brief Return true if both pure IP's (string or pv) are equal.
 * IPv6 references not allowed. */
static int w_compare_pure_ips(sip_msg_t* _msg, char* _s1, char* _s2)
{
	str string1, string2;

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

	return ki_compare_pure_ips(_msg, &string1, &string2);
}

/*! \brief Return true if the first IP (string or pv) is within the subnet
 * defined by the second commma-separated IP list in CIDR notation.
 * IPv6 references not allowed. */
static int ki_ip_is_in_subnet(sip_msg_t* _msg, str* _sval1, str* _sval2)
{
	struct in6_addr ip_addr6;
	struct in_addr ip_addr;
	int ret;
	char ip_addr_str[INET6_ADDRSTRLEN],*b,*e;
	void *ip;
	str string1, string2;
	enum enum_ip_type ip1_type;

	string1 = *_sval1;
	string2 = *_sval2;
	switch(ip1_type = ip_parser_execute(string1.s, string1.len)) {
		case(ip_type_error):
			return -1;
			break;
		case(ip_type_ipv6_reference):
			return -1;
			break;
		case(ip_type_ipv4):
			memcpy(ip_addr_str, string1.s, string1.len);
			ip_addr_str[string1.len] = '\0';
			if(inet_pton(AF_INET, ip_addr_str, &ip_addr) == 0)  return 0;
			ip = &ip_addr;
			break;
		case(ip_type_ipv6):
			memcpy(ip_addr_str, string1.s, string1.len);
			ip_addr_str[string1.len] = '\0';
			if (inet_pton(AF_INET6, ip_addr_str, &ip_addr6) != 1)  return 0;
			ip = &ip_addr6;
			break;
		default:
			return -1;
			break;
	}

	b = string2.s;
	e = strchr(string2.s,',');
	for(; e!= NULL; b = e+1, e = strchr(b,',')) {
		if(b==e) continue;
		if((ret = _ip_is_in_subnet_str_trimmed(ip,ip1_type,b,e))>0) return ret;
	}
	e = string2.s+string2.len;
	ret = _ip_is_in_subnet_str_trimmed(ip,ip1_type,b,e);
	if(ret==0) return -1;
	return ret;
}


/*! \brief Return true if the first IP (string or pv) is within the subnet
 * defined by the second commma-separated IP list in CIDR notation.
 * IPv6 references not allowed. */
static int w_ip_is_in_subnet(sip_msg_t* _msg, char* _s1, char* _s2)
{
	str string1, string2;

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

	return ki_ip_is_in_subnet(_msg, &string1, &string2);
}

/*! \brief Return true if the given argument (string or pv) is a valid
 * RFC 1918 IPv4 (private address). */
static int w_is_ip_rfc1918(sip_msg_t* _msg, char* _s, char *_p2)
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

/*! \brief Return true if the given argument (string or pv) is a valid
 * RFC 1918 IPv4 (private address). */
static int ki_is_ip_rfc1918(sip_msg_t* _msg, str* sval)
{
	if (rfc1918_parser_execute(sval->s, sval->len) == 1)
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

static int ki_dns_sys_match_ip(sip_msg_t *msg, str *vhn, str *vip)
{
	struct addrinfo hints, *res, *p;
	int status;
	ip_addr_t *ipa;
	void *addr;
	str hns;
	str ips;
	struct sockaddr_in *ipv4;
	struct sockaddr_in6 *ipv6;

	hns = *vhn;
	ips = *vip;
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

static int w_dns_sys_match_ip(sip_msg_t *msg, char *hnp, char *ipp)
{
	str hns;
	str ips;

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

	return ki_dns_sys_match_ip(msg, &hns, &ips);
}

static int ki_dns_int_match_ip(sip_msg_t *msg, str *vhn, str *vip)
{
	ip_addr_t *ipa;
	str hns;
	str ips;
	struct hostent* he;
	char ** h;

	hns = *vhn;
	ips = *vip;
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

static int w_dns_int_match_ip(sip_msg_t *msg, char *hnp, char *ipp)
{
	str hns;
	str ips;

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

	return ki_dns_int_match_ip(msg, &hns, &ips);
}

/**
 *
 */
static int w_dns_query(sip_msg_t* msg, char* str1, char* str2)
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
static int ki_dns_query(sip_msg_t* msg, str* naptrname, str* pvid)
{
	return dns_update_pv(naptrname, pvid);
}

/**
 *
 */
static int w_srv_query(sip_msg_t* msg, char* str1, char* str2)
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

/**
 *
 */
static int ki_srv_query(sip_msg_t* msg, str* naptrname, str* pvid)
{
	return srv_update_pv(naptrname, pvid);
}

/**
 *
 */
static int w_naptr_query(sip_msg_t* msg, char* str1, char* str2)
{
	str naptrname;
	str name;

	if(msg==NULL)
	{
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg,(gparam_t*)str1, &naptrname)<0)
	{
		LM_ERR("cannot get the naptrcname\n");
		return -1;
	}
	if(fixup_get_svalue(msg,(gparam_t*)str2, &name)<0)
	{
		LM_ERR("cannot get the pvid name\n");
		return -1;
	}

	return naptr_update_pv(&naptrname, &name);
}

/**
 *
 */
static int ki_naptr_query(sip_msg_t* msg, str* naptrname, str* pvid)
{
	return naptr_update_pv(naptrname, pvid);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_ipops_exports[] = {
	{ str_init("ipops"), str_init("is_ip"),
		SR_KEMIP_INT, ki_is_ip,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("is_pure_ip"),
		SR_KEMIP_INT, ki_is_pure_ip,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("is_ip4"),
		SR_KEMIP_INT, ki_is_ip4,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("is_ip6"),
		SR_KEMIP_INT, ki_is_ip6,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("is_ip6_reference"),
		SR_KEMIP_INT, ki_is_ip6_reference,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("ip_type"),
		SR_KEMIP_INT, ki_ip_type,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("detailed_ipv4_type"),
		SR_KEMIP_INT, ki_detailed_ipv4_type,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("detailed_ipv6_type"),
		SR_KEMIP_INT, ki_detailed_ipv6_type,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("detailed_ip_type"),
		SR_KEMIP_INT, ki_detailed_ip_type,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("compare_ips"),
		SR_KEMIP_INT, ki_compare_ips,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("compare_pure_ips"),
		SR_KEMIP_INT, ki_compare_pure_ips,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("ip_is_in_subnet"),
		SR_KEMIP_INT, ki_ip_is_in_subnet,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("is_ip_rfc1918"),
		SR_KEMIP_INT, ki_is_ip_rfc1918,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("dns_sys_match_ip"),
		SR_KEMIP_INT, ki_dns_sys_match_ip,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("dns_int_match_ip"),
		SR_KEMIP_INT, ki_dns_int_match_ip,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("dns_query"),
		SR_KEMIP_INT, ki_dns_query,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("srv_query"),
		SR_KEMIP_INT, ki_srv_query,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ipops"), str_init("naptr_query"),
		SR_KEMIP_INT, ki_naptr_query,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_ipops_exports);
	return 0;
}
