/*
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*!
 * \file
 * \brief Kamailio statistics handling
 * \ingroup libkcore
 * \author bogdan
 * \author Jeffrey Magder - SOMA Networks
 */


#ifndef _KSTATISTICS_H_
#define _KSTATISTICS_H_

#include "kstats_wrapper.h"


#define NUM_IP_OCTETS 4
#define NUM_IPV6_OCTETS 16


#ifdef STATISTICS
/*! \brief
 * Returns the statistic associated with 'numerical_code' and 'is_a_reply'.
 * Specifically:
 *
 *  - if in_codes is nonzero, then the stat_var for the number of messages 
 *    _received_ with the 'numerical_code' will be returned if it exists.
 *  - otherwise, the stat_var for the number of messages _sent_ with the 
 *    'numerical_code' will be returned, if the stat exists. 
 */
stat_var *get_stat_var_from_num_code(unsigned int numerical_code, int in_codes);

#else
	#define get_stat_var_from_num_code( _n_code, _in_code) NULL
#endif



/*!
 * This function will retrieve a list of all ip addresses and ports that Kamailio
 * is listening on, with respect to the transport protocol specified with
 * 'protocol'. This function only returns IPv4 addresses.
 *
 * The first parameter, ipList, is a pointer to a pointer. It will be assigned a
 * new block of memory holding the IP Addresses and ports being listened to with
 * respect to 'protocol'.  The array maps a 2D array into a 1 dimensional space,
 * and is layed out as follows:
 *
 * The first NUM_IP_OCTETS indices will be the IP address, and the next index
 * the port.  So if NUM_IP_OCTETS is equal to 4 and there are two IP addresses
 * found, then:
 *
 *  - ipList[0] will be the first octet of the first ip address
 *  - ipList[3] will be the last octet of the first ip address.
 *  - iplist[4] will be the port of the first ip address
 *  - 
 *  - iplist[5] will be the first octet of the first ip address, 
 *  - and so on.  
 *
 * The function will return the number of sockets which were found.  This can be
 * used to index into ipList.
 *
 * \note This function assigns a block of memory equal to:
 *
 *            returnedValue * (NUM_IP_OCTETS + 1) * sizeof(int);
 *
 *       Therefore it is CRUCIAL that you free ipList when you are done with its
 *       contents, to avoid a nasty memory leak.
 */
int get_socket_list_from_proto(int **ipList, int protocol);

/*! \brief Function to get a list of all IP addresses and ports in a specific
 * family, like AF_INET or AF_INET6
 *
 * For documentation see \ref get_socket_list_from_proto()
 */
int get_socket_list_from_proto_and_family(int **ipList, int protocol, int family);


/*!
 * Returns the sum of the number of bytes waiting to be consumed on all network
 * interfaces and transports that Kamailio is listening on. 
 *
 * Note: This currently only works on systems supporting the /proc/net/[tcp|udp]
 *       interface.  On other systems, zero will always be returned.  Details of
 *       why this is so can be found in network_stats.c
 */
int get_total_bytes_waiting(void);


#endif
