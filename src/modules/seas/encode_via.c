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
 *        Filename:  encode_via.c
 * 
 *     Description:  functions to encode VIA headers
 * 
 *         Version:  1.0
 *         Created:  21/11/05 02:30:50 CET
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

#include "../../parser/parse_via.h"
#include "../../parser/parse_uri.h"
#include "encode_via.h"
#include "encode_uri.h"
#include "xaddress.h"
#include "encode_header.h"
#include "encode_parameters.h"

#define HAS_PARAMS_F		0x01
#define HAS_BRANCH_F		0x02
#define HAS_RECEIVED_F		0x04
#define HAS_RPORT_F		0x08
#define HAS_I_F			0x10
#define HAS_ALIAS_F		0x20
#define HAS_PORT_F		0x40

#define REL_PTR(a,b) (unsigned char)((b)-(a))

/*
 * encodes a (maybe aggregated) via header.
 * encoding is:
 * 1: flags
 * 1: number of vias present
 * N: fore each via present, the length of the via
 * N*M: the via structures concatenated
 */
int encode_via_body(char *hdr,int hdrlen,struct via_body *via_parsed,unsigned char *where)
{
   int i=0,k,via_offset;
   unsigned char tmp[500];
   struct via_body *myvia;

   if(via_parsed)
      for(via_offset=0,i=0,myvia=via_parsed;myvia;myvia=myvia->next,i++){
	 if((k=encode_via(hdr,hdrlen,myvia,&tmp[via_offset]))<0){
	    LM_ERR("failed to parse via number %d\n",i);
	    return -1;
	 }
	 where[2+i]=(unsigned char)k;
	 via_offset+=k;
      }
   else
      return -1;
   where[1]=(unsigned char)i;/*how may vias there are*/
   memcpy(&where[2+i],tmp,via_offset);
   return 2+i+via_offset;
}

/* Encoder for vias.
 * Returns the length of the encoded structure in bytes
 * FORMAT (byte meanings):
 * 1: flags
 *
 */
int encode_via(char *hdrstart,int hdrlen,struct via_body *body,unsigned char *where)
{
   int i;/* 1*flags + 1*hostport_len*/
   unsigned char flags=0;

   where[1]=REL_PTR(hdrstart,body->name.s);
   where[2]=REL_PTR(hdrstart,body->version.s);
   where[3]=REL_PTR(hdrstart,body->transport.s);
   where[4]=REL_PTR(hdrstart,body->transport.s+body->transport.len+1);
   where[5]=REL_PTR(hdrstart,body->host.s);
   if(body->port_str.s && body->port_str.len){
      flags|=HAS_PORT_F;
      where[6]=REL_PTR(hdrstart,body->port_str.s);
      where[7]=REL_PTR(hdrstart,body->port_str.s+body->port_str.len+1);
      i=8;
   }else{
      where[6]=REL_PTR(hdrstart,body->host.s+body->host.len+1);
      i=7;
   }
   if(body->params.s && body->params.len){
      flags|=HAS_PARAMS_F;
      where[i++]=REL_PTR(hdrstart,body->params.s);
      where[i++]=(unsigned char)body->params.len;
   }
   if(body->branch && body->branch->value.s && body->branch->value.len){
      flags|=HAS_BRANCH_F;
      where[i++]=REL_PTR(hdrstart,body->branch->value.s);
      where[i++]=(unsigned char)body->branch->value.len;
   }
   if(body->received && body->received->value.s && body->received->value.len){
      flags|=HAS_RECEIVED_F;
      where[i++]=REL_PTR(hdrstart,body->received->value.s);
      where[i++]=(unsigned char)body->received->value.len;
   }
   if(body->rport && body->rport->value.s && body->rport->value.len){
      flags|=HAS_RPORT_F;
      where[i++]=REL_PTR(hdrstart,body->rport->value.s);
      where[i++]=(unsigned char)body->rport->value.len;
   }
   if(body->i && body->i->value.s && body->i->value.len){
      flags|=HAS_I_F;
      where[i++]=REL_PTR(hdrstart,body->i->value.s);
      where[i++]=(unsigned char)body->i->value.len;
   }
   if(body->alias && body->alias->value.s && body->alias->value.len){
      flags|=HAS_ALIAS_F;
      where[i++]=REL_PTR(hdrstart,body->alias->value.s);
      where[i++]=(unsigned char)body->alias->value.len;
   }

   where[0]=flags;
   i+=encode_parameters(&where[i],body->param_lst,hdrstart,(void *)body,'v');
   return i;
}

int print_encoded_via_body(FILE *fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix)
{
   unsigned char numvias;
   int i,offset;

   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"ENCODED VIA BODY:[":":",payload[i],i==paylen-1?"]\n":"");
   numvias=payload[1];
   fprintf(fd,"%s%d","NUMBER OF VIAS:",numvias);
   if(numvias==0){
      LM_ERR("no vias present?\n");
      return -1;
   }
   for(i=0,offset=2+numvias;i<numvias;i++){
      print_encoded_via(fd,hdr,hdrlen,&payload[offset],payload[2+i],strcat(prefix,"  "));
      offset+=payload[2+i];
      prefix[strlen(prefix)-2]=0;
   }
   return 1;
}

int print_encoded_via(FILE *fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i=2;/* flags + urilength */
   unsigned char flags=0;

   flags=payload[0];
   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"ENCODED VIA=[":":",payload[i],i==paylen-1?"]\n":"");

   fprintf(fd,"%sPROT=[%.*s]\n",prefix,payload[2]-payload[1]-1,&hdr[payload[1]]);
   fprintf(fd,"%sVER=[%.*s]\n",prefix,payload[3]-payload[2]-1,&hdr[payload[2]]);
   fprintf(fd,"%sTRANSP=[%.*s]\n",prefix,payload[4]-payload[3]-1,&hdr[payload[3]]);

   fprintf(fd,"%sHOST=[%.*s]\n",prefix,payload[6]-payload[5]-1,&hdr[payload[5]]);
   if(flags & HAS_PORT_F){
      fprintf(fd,"%sPORT=[%.*s]\n",prefix,payload[7]-payload[6]-1,&hdr[payload[6]]);
      i=8;
   }else
      i=7;
   if(flags & HAS_PARAMS_F){
      fprintf(fd,"%sPARAMS=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_BRANCH_F){
      fprintf(fd,"%sBRANCH=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_RECEIVED_F){
      fprintf(fd,"%sRECEIVED=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_RPORT_F){
      fprintf(fd,"%sRPORT=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_I_F){
      fprintf(fd,"%sI=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_ALIAS_F){
      fprintf(fd,"%sALIAS=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   for(;i<paylen-1;i+=2){
      fprintf(fd,"%s[PARAMETER[%.*s]",prefix,payload[i+1]-payload[i]-1,&hdr[payload[i]]);
      fprintf(fd,"VALUE[%.*s]]\n",(payload[i+2]-payload[i+1])==0?0:(payload[i+2]-payload[i+1]-1),&hdr[payload[i+1]]);
   }
   return 0;
}

int dump_via_body_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,FILE* fd,char segregationLevel)
{
   unsigned char numvias;
   int i,offset;

   if(!segregationLevel){
      return dump_standard_hdr_test(hdr,hdrlen,payload,paylen,fd);
   }

   numvias=payload[1];
   if(numvias==0){
      LM_ERR("no vias present?\n");
      return -1;
   }
   if(segregationLevel & SEGREGATE)
      for(i=0,offset=2+numvias;i<numvias;i++){
	 dump_standard_hdr_test(hdr,hdrlen,&payload[offset],payload[2+i],fd);
	 offset+=payload[2+i];
      }
   return 1;
}
