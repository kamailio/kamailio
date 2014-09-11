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
#include "../../parser/parse_to.h"
#define MAX_XHDR_LEN 255
#define HAS_DISPLAY_F	0x01
#define HAS_TAG_F	0x02
#define HAS_OTHERPAR_F	0x04
int encode_to_body(char *hdrstart,int hdrlen,struct to_body *body,unsigned char *where);
int print_encoded_to_body(FILE* fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix);
int dump_to_body_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,FILE* fd,char segregationLevel);
