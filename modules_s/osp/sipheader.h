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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef OSP_MOD_SIPHEADER_H
#define OSP_MOD_SIPHEADER_H

#include "osp/osp.h"
#include "../../sr_module.h"

#define OSP_HEADER        "P-OSP-Auth-Token: "
#define OSP_HEADER_LEN    strlen(OSP_HEADER)

int  getFromUserpart(    struct sip_msg* msg, char* fromuser, int buffer_size);
int  getToUserpart(      struct sip_msg* msg, char* touser, int buffer_size);
int  addOspHeader(       struct sip_msg* msg, char* token, int  sizeoftoken);
int  getOspHeader(       struct sip_msg* msg, char* token, int* sizeoftoken);
int  getSourceAddress(   struct sip_msg* msg, char* source_address, int buffer_size);
int  getCallId(          struct sip_msg* msg, OSPTCALLID** callid);
int  getRouteParams(     struct sip_msg* msg, char* route_params, int buffer_size);
int  rebuildDestionationUri(str *newuri, char *destination, char *port, char *callednumber);
void getNextHop(struct sip_msg* msg, char* next_hope, int buffer_size);
void copy_from_str_to_buffer(str* from, char* buffer, int buffer_size);

void skipPlus(char* e164);

#endif

