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
 *        Filename:  encode_allow.c
 * 
 *     Description:  [en|de]code allow header
 * 
 *         Version:  1.0
 *         Created:  21/11/05 20:40:25 CET
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
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../parser/parse_allow.h"
#include "../../parser/msg_parser.h"
#include "encode_allow.h"

char *mismetodos[]={"UNDEF","INVITE","CANCEL","ACK","BYE","INFO","OPTIONS","UPDATE","REGISTER","MESSAGE","SUBSCRIBE","NOTIFY","PRACK","REFER","OTHER"};

/**
 * Encodes allow header.
 *
 * TODO: Does not support the UNDEFINED header type !!!
 */
int encode_allow(char *hdrstart,int hdrlen,unsigned int *bodi,char *where)
{
   unsigned int i;
   memcpy(&i,bodi,4);
   i=htonl(i);
   memcpy(where,&i,4);
   return 4;
}

int print_encoded_allow(FILE *fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   unsigned int i,j=0,body;

   memcpy(&body,payload,4);
   body=ntohl(body);
   fprintf(fd,"%sMETHODS=",prefix);
   if(body==0)
      fprintf(fd,"UNKNOWN");
   for(i=0;i<32;j=(0x01<<i),i++){
      if(body & (j<15))
	 fprintf(fd,",%s",mismetodos[i]);
   }
   fprintf(fd,"\n");
   return 1;
}
