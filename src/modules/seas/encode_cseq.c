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
 *        Filename:  xaddress.c
 * 
 *     Description:  Address manipulation tools
 * 
 *         Version:  1.0
 *         Created:  17/11/05 02:09:44 CET
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
#include "../../mem/mem.h"
#include "../../parser/parse_cseq.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "xaddress.h"/*for SLOG*/

/* Encoder for CSeq header
 * Returns the length of the encoded structure in bytes
 * FORMAT (byte meanings):
 * 1: method_id
 * 4: cseqnum in network byte order
 * 2: HDR-based ptr to the method number + number length
 * 2: HDR-based ptr to the method name + method length
 *
 */
int encode_cseq(char *hdrstart,int hdrlen,struct cseq_body *body,unsigned char *where)
{
   unsigned int cseqnum;
   unsigned char i;

   /*which is the first bit set to 1 ? if i==0, the first bit, 
    * if i==31, the last, if i==32, none*/
   for(i=0;(!(body->method_id & (0x01<<i))) && i<32;i++);
   if(i==32)
      i=0;
   else
      i++;
   where[0]=i;
   if(str2int(&body->number,&cseqnum)<0){
      LM_ERR("str2int(cseq number)\n");
      return -1;
   }
   cseqnum=htonl(cseqnum);
   memcpy(&where[1],&cseqnum,4);/*I use 4 because 3261 says CSEq num must be 32 bits long*/
   where[5]=(unsigned char)(body->number.s-hdrstart);
   where[6]=(unsigned char)(body->number.len);
   where[7]=(unsigned char)(body->method.s-hdrstart);
   where[8]=(unsigned char)(body->method.len);
   return 9;
}

int print_encoded_cseq(FILE *fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   unsigned int cseqnum;
   char *what;

   memcpy(&cseqnum,&payload[1],4);
   cseqnum=ntohl(cseqnum);
   fprintf(fd,"%sCSEQ NUMBER=%d==%.*s\n",prefix,cseqnum,payload[6],&hdr[payload[5]]);
   switch(payload[0]){
      case 0:
	 what="UNDEFINED";
	 break;
      case 1:
	 what="INVITE";
	 break;
      case 2:
	 what="CANCEL";
	 break;
      case 3:
	 what="ACK";
	 break;
      case 4:
	 what="BYE";
	 break;
      case 5:
	 what="INFO";
	 break;
      case 6:
	 what="OPTIONS";
	 break;
      case 7:
	 what="UPDATE";
	 break;
      case 8:
	 what="REGISTER";
	 break;
      case 9:
	 what="MESSAGE";
	 break;
      case 10:
	 what="SUBSCRIBE";
	 break;
      case 11:
	 what="NOTIFY";
	 break;
      case 12:
	 what="PRACK";
	 break;
      case 13:
	 what="REFER";
	 break;
      case 14:
	 what="OTHER";
	 break;
      default:
	 what="UNKNOWN?";
	 break;
   }
   fprintf(fd,"%sCSEQ METHOD=%s==%.*s\n",prefix,what,payload[8],&hdr[payload[7]]);
   return 1;

}
