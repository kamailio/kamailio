/*
 * Sdp mangler module
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* History:
 * --------
 *  2003-04-07 first version.  
 */


#ifndef IP_HELPER_H
#define IP_HELPER_H

unsigned int net_address (unsigned int address, unsigned int mask);
int same_net (unsigned int ip, unsigned int address, unsigned int mask);
void ip2str (unsigned int address, char **rr);
int is_positive_number (char *str);
int parse_ip_address (char *c, unsigned int *address);
int parse_ip_netmask (char *c, char **ip, unsigned int *mask);
unsigned int make_mask (unsigned int length);


#endif
