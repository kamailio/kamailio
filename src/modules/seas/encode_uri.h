/* $Id$
 *
 * Copyright (C) 2006-2007 VozTelecom Sistemas S.L
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
 */

#include "../../str.h"
#include "../../parser/msg_parser.h"

#define SIP_OR_TEL_F	0x01
#define SECURE_F	0x02
#define USER_F		0x04
#define PASSWORD_F	0x08
#define HOST_F		0x10
#define PORT_F		0x20
#define PARAMETERS_F	0x40
#define HEADERS_F	0x80

#define TRANSPORT_F	0x01
#define TTL_F		0x02
#define USER_F		0x04
#define METHOD_F	0x08
#define MADDR_F		0x10
#define LR_F		0x20

#define MAX_XURI_LEN	23

int encode_uri2(char *hdr,int hdrlen,str uri_str, struct sip_uri *uri_parsed,unsigned char *where);
int print_encoded_uri(FILE* fd,unsigned char *payload,int paylen,char *hdr,int hdrlen,char *prefix);
int print_uri_junit_tests(char *hdrstart,int hdrlen,unsigned char *payload,int paylen,FILE* fd,char also_hdr,char *prefix);
