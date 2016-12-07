/*
 * $Id$
 *
 * Functions that process IPOPS message 
 *
 * Copyright (C) 2012 Hugh Waite (crocodile-rcs.com)
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

#include "api.h"
#include "ip_parser.h"

extern int _compare_ips(char*, size_t, enum enum_ip_type, char*, size_t, enum enum_ip_type);
extern int _ip_is_in_subnet(char *ip1, size_t len1, enum enum_ip_type ip1_type, char *ip2, size_t len2, enum enum_ip_type ip2_type, int netmask);
/**
 *
 */
int ipopsapi_compare_ips(const str *const ip1, const str *const ip2)
{
  str string1 = *ip1;
  str string2 = *ip2;
  enum enum_ip_type ip1_type, ip2_type;
  
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

/**
 *
 */
int ipopsapi_ip_is_in_subnet(const str *const ip1, const str *const ip2)
{
  str string1 = *ip1;
  str string2 = *ip2;
  enum enum_ip_type ip1_type, ip2_type;
  char *cidr_pos = NULL;
  int netmask = 0;
  
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
    if (*cidr_pos == '/') break;
    cidr_pos--;
  }
  if (cidr_pos == string2.s) return -1;
  string2.len = (cidr_pos - string2.s);
  netmask = atoi(cidr_pos+1);
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
  
  if (_ip_is_in_subnet(string1.s, string1.len, ip1_type, string2.s, string2.len, ip2_type, netmask))
    return 1;
  else
    return -1;
}

/**
 *
 */
int ipopsapi_is_ip(const str * const ip)
{
  if (ip_parser_execute(ip->s, ip->len) != ip_type_error)
    return 1;
  else
    return -1;
}

/**
 *
 */
int bind_ipops(ipops_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->compare_ips     = ipopsapi_compare_ips;
	api->ip_is_in_subnet = ipopsapi_ip_is_in_subnet;
	api->is_ip           = ipopsapi_is_ip;

	return 0;
}
