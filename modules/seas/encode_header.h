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


#include "../../parser/hf.h"
#include "../../str.h"
int print_encoded_header(FILE* fd,char *msg,int len,unsigned char *payload,int paylen,char type,char *prefix);
int encode_header(struct sip_msg *msg,struct hdr_field *hdr,unsigned char *payload,int paylen);
int dump_headers_test(char *msg,int msglen,unsigned char *payload,int len,char type,FILE* fd,char segregationLevel);
int dump_standard_hdr_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,FILE* fd);
