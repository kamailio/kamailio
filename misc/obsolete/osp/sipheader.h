/*
 * ser osp module. 
 *
 * This module enables ser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
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

#ifndef _OSP_MOD_SIPHEADER_H_
#define _OSP_MOD_SIPHEADER_H_

#include <osp/osp.h>
#include "../../parser/msg_parser.h"

#define OSP_TOKEN_HEADER    "P-OSP-Auth-Token: "
#define OSP_HEADER_SIZE     strlen(OSP_TOKEN_HEADER)

void ospCopyStrToBuffer(str* source, char* buffer, int buffersize);
int ospGetFromUserpart(struct sip_msg* msg, char* fromuser, int buffersize);
int ospGetRpidUserpart(struct sip_msg* msg, char* fromuser, int buffersize);
int ospGetToUserpart(struct sip_msg* msg, char* touser, int buffersize);
int ospGetUriUserpart(struct sip_msg* msg, char* touser, int buffersize);
int ospAddOspHeader(struct sip_msg* msg, unsigned char* token, unsigned int tokensize);
int ospGetOspHeader(struct sip_msg* msg, unsigned char* token, unsigned int* tokensize);
int ospGetSourceAddress(struct sip_msg* msg, char* sourceaddress, int buffersize);
int ospGetCallId(struct sip_msg* msg, OSPTCALLID** callid);
int ospGetRouteParameters(struct sip_msg* msg, char* routeparams, int buffersize);
int ospRebuildDestionationUri(str* newuri, char* called, char* dest, char* port, int format);
void ospGetNextHop(struct sip_msg* msg, char* nexthop, int buffersize);
int ospGetDirection(struct sip_msg* msg);

#endif /* _OSP_MOD_SIPHEADER_H_ */

