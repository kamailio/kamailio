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

/*
 * =====================================================================================
 * 
 *        Filename:  encode_expires.c
 * 
 *     Description:  functions to [en|de]code expires header
 * 
 *         Version:  1.0
 *         Created:  22/11/05 00:05:54 CET
 *        Revision:  none
 *        Compiler:  gcc
 * 
 *          Author:  Elias Baixas (EB), elias@conillera.net
 *         Company:  VozTele.com
 * 
 * =====================================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include "../../parser/parse_expires.h"

/*
 * Encodes expires headers (content = delta-seconds)
 * 4: network-byte-order value
 * 2: hdr-based pointer to begin of value string + value string length
 */
int encode_expires(char *hdrstart,int hdrlen,exp_body_t *body,unsigned char *where)
{
   int i;

   i=htonl(body->val);
   memcpy(where,&i,4);
   where[4]=(unsigned char)(body->text.s-hdrstart);
   where[5]=(unsigned char)(body->text.len);
   return 6;
}

int print_encoded_expires(FILE *fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i;
   memcpy(&i,payload,4);
   i=ntohl(i);
   fprintf(fd,"%sEXPIRES VALUE=%d==%.*s\n",prefix,i,payload[5],&hdr[payload[4]]);
   return 1;
}


