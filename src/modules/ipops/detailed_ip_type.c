/*
 * Functions that operate on IP addresses
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#if defined (__OS_freebsd)
#include <sys/socket.h>
#endif
#include <string.h>
#include <unistd.h>
#include "../../core/dprint.h"
#include "../../core/trim.h"
#include "../../core/str.h"
#include "detailed_ip_type.h"

static ip4_node IPv4ranges[IPv4RANGES_SIZE] = {
        { 0xffffffff,  "BROADCAST",  0xffffffff },  // 255.255.255.255/32
        { 0xcb007100,  "TEST-NET",   0xffffff00 },  // 203.0.113/24
        { 0xc6336400,  "TEST-NET",   0xffffff00 },  // 198.51.100/24
        { 0xc0586300,  "6TO4-RELAY", 0xffffff00 },  // 192.88.99.0/24
        { 0xc0000200,  "TEST-NET",   0xffffff00 },  // 192.0.2/24
        { 0xc0000000,  "RESERVED",   0xffffff00 },  // 192.0.0/24
        { 0xc0000000,  "6TO4-RELAY", 0xfffffff8 },  // 192.0.0.0/29
        { 0xc0a80000,  "PRIVATE",    0xffff0000 },  // 192.168/16
        { 0xa9fe0000,  "LINK-LOCAL", 0xffff0000 },  // 169.254/16
        { 0xc6120000,  "RESERVED",   0xfffe0000 },  // 198.18/15
        { 0xac100000,  "PRIVATE",    0xfffe0000 },  // 172.16/12
        { 0x64400000,  "SHARED",     0xffc00000 },  // 100.64/10
        { 0x7f000000,  "LOOPBACK",   0xff000000 },  // 127.0/8
        { 0x0a000000,  "PRIVATE",    0xff000000 },  // 10/8
        { 0x0,         "PRIVATE",    0xff000000 },  // 0/8
        { 0xf0000000,  "RESERVED",   0xf0000000 },  // 240/4
        { 0xe0000000,  "MULTICAST",  0xf0000000 }   // 224/4
};

static  ip6_node IPv6ranges[IPv6RANGES_SIZE] = {
    { {0x00000000, 0x00000000, 0x00000000, 0x00000000} , "UNSPECIFIED",         {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF} },  //::/128
    { {0x00000000, 0x00000000, 0x00000000, 0x00000001} , "LOOPBACK",            {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF} },  //::1/128
    { {0x00000000, 0x00000000, 0x0000FFFF, 0x00000000} , "IPV4MAP",             {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000} },  //::FFFF:0:0/96
    { {0x01000000, 0x00000000, 0x00000000, 0x00000000} , "DISCARD",             {0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000} },  //0100::/64
    { {0x20010002, 0x00000000, 0x00000000, 0x00000000} , "BMWG",                {0xFFFFFFFF, 0xFFFF0000, 0x00000000, 0x00000000} },  //2001:0002::/48
    { {0x20010000, 0x00000000, 0x00000000, 0x00000000} , "TEREDO",              {0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000} },  //2001::/32
    { {0x20010DB8, 0x00000000, 0x00000000, 0x00000000} , "DOCUMENTATION",       {0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000} },  //2001:DB8::/32
    { {0x20010010, 0x00000000, 0x00000000, 0x00000000} , "ORCHID",              {0xFFFFFFF0, 0x00000000, 0x00000000, 0x00000000} },  //2001:10::/28
    { {0x20020000, 0x00000000, 0x00000000, 0x00000000} , "6TO4",                {0xFFFF0000, 0x00000000, 0x00000000, 0x00000000} },  //2002::/16
    { {0xFEC00000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xFFC00000, 0x00000000, 0x00000000, 0x00000000} },  //FEC0::/10
    { {0xFE800000, 0x00000000, 0x00000000, 0x00000000} , "LINK-LOCAL-UNICAST",  {0xFFc00000, 0x00000000, 0x00000000, 0x00000000} },  //FE80::/10
    { {0xFE000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xFF800000, 0x00000000, 0x00000000, 0x00000000} },  //FE00::/9
    { {0x00000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xFF000000, 0x00000000, 0x00000000, 0x00000000} },  //::/8
    { {0x01000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xFF000000, 0x00000000, 0x00000000, 0x00000000} },  //0100::/8
    { {0xFF000000, 0x00000000, 0x00000000, 0x00000000} , "MULTICAST",           {0xFF000000, 0x00000000, 0x00000000, 0x00000000} },  //FF00::/8
    { {0x02000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xFE000000, 0x00000000, 0x00000000, 0x00000000} },  //0200::/7
    { {0xFC000000, 0x00000000, 0x00000000, 0x00000000} , "UNIQUE-LOCAL-UNICAST",{0xFE000000, 0x00000000, 0x00000000, 0x00000000} },  //FC00::/7
    { {0x04000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xFC000000, 0x00000000, 0x00000000, 0x00000000} },  //400::/6
    { {0xF8000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xFC000000, 0x00000000, 0x00000000, 0x00000000} },  //F800::/6
    { {0x08000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xF8000000, 0x00000000, 0x00000000, 0x00000000} },  //0800::/5
    { {0xF0000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xF8000000, 0x00000000, 0x00000000, 0x00000000} },  //F000::/5
    { {0xE0000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xF0000000, 0x00000000, 0x00000000, 0x00000000} },  //E000::/4
    { {0x10000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xF0000000, 0x00000000, 0x00000000, 0x00000000} },  //1000::/4
    { {0xC0000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xE0000000, 0x00000000, 0x00000000, 0x00000000} },  //C000::/3
    { {0x20000000, 0x00000000, 0x00000000, 0x00000000} , "GLOBAL-UNICAST",      {0xE0000000, 0x00000000, 0x00000000, 0x00000000} },  //2000::/3
    { {0x40000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xE0000000, 0x00000000, 0x00000000, 0x00000000} },  //4000::/3
    { {0x60000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xE0000000, 0x00000000, 0x00000000, 0x00000000} },  //6000::/3
    { {0x80000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xE0000000, 0x00000000, 0x00000000, 0x00000000} },  //8000::/3
    { {0xA0000000, 0x00000000, 0x00000000, 0x00000000} , "RESERVED",            {0xE0000000, 0x00000000, 0x00000000, 0x00000000} }   //A000::/3

};


int ip6_iptype(str string_ip, char **res)
{
  uint32_t in6_addr[4];
  char in6_string[INET6_ADDRSTRLEN];
  int i;

  trim(&string_ip);
  if (string_ip.len >= INET6_ADDRSTRLEN)
        return 0;

  memcpy(in6_string, string_ip.s, string_ip.len);
  in6_string[string_ip.len] = '\0';

  if (inet_pton(AF_INET6, in6_string, in6_addr) != 1)  return 0;

  for (i = 0; i < IPv6RANGES_SIZE; i++) {
      if (((in6_addr[0] & IPv6ranges[i].sub_mask[0]) == IPv6ranges[i].value[0]) &&
          ((in6_addr[1] & IPv6ranges[i].sub_mask[1]) == IPv6ranges[i].value[1]) &&
          ((in6_addr[2] & IPv6ranges[i].sub_mask[2]) == IPv6ranges[i].value[2]) &&
          ((in6_addr[3] & IPv6ranges[i].sub_mask[3]) == IPv6ranges[i].value[3])) {
          *res = IPv6ranges[i].ip_type;
          return 1;
      }
  }
  /* the ip must be in the interval, else there is some problem */
  return 0;
}

int ip4_iptype(str string_ip, char **res)
{
  uint32_t in4_addr;
  char in4_string[INET_ADDRSTRLEN];
  int i;

  trim(&string_ip);
  if (string_ip.len >= INET_ADDRSTRLEN)
      return 0;

  memcpy(in4_string, string_ip.s, string_ip.len);
  in4_string[string_ip.len] = '\0';

  if (inet_pton(AF_INET, in4_string, &in4_addr) != 1)  return 0;

  *res = "PUBLIC";
  for (i = 0; i < IPv4RANGES_SIZE; i++) {
      if ( (in4_addr & IPv4ranges[i].sub_mask) == IPv4ranges[i].value ) {
          *res = IPv4ranges[i].ip_type;
          return 1;
      }
  }
  return 1;
}

void ipv4ranges_hton() {
    int pos;
    uint32_t tmp;

    for (pos=0; pos < IPv4RANGES_SIZE; pos++) {
        tmp = IPv4ranges[pos].value;
        IPv4ranges[pos].value = ntohl(tmp);
        tmp = IPv4ranges[pos].sub_mask;
        IPv4ranges[pos].sub_mask = ntohl(tmp);
    }
}

void ipv6ranges_hton() {
    int pos;
    uint32_t tmp;

    for (pos=0; pos < IPv6RANGES_SIZE; pos++) {
        tmp = IPv6ranges[pos].value[0];
        IPv6ranges[pos].value[0] = ntohl(tmp);
        tmp = IPv6ranges[pos].value[1];
        IPv6ranges[pos].value[1] = ntohl(tmp);
        tmp = IPv6ranges[pos].value[2];
        IPv6ranges[pos].value[2] = ntohl(tmp);
        tmp = IPv6ranges[pos].value[3];
        IPv6ranges[pos].value[3] = ntohl(tmp);

        tmp = IPv6ranges[pos].sub_mask[0];
        IPv6ranges[pos].sub_mask[0] = ntohl(tmp);
        tmp = IPv6ranges[pos].sub_mask[1];
        IPv6ranges[pos].sub_mask[1] = ntohl(tmp);
        tmp = IPv6ranges[pos].sub_mask[2];
        IPv6ranges[pos].sub_mask[2] = ntohl(tmp);
        tmp = IPv6ranges[pos].sub_mask[3];
        IPv6ranges[pos].sub_mask[3] = ntohl(tmp);
    }
}
