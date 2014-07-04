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
 *        Filename:  encode_route.c
 * 
 *     Description:  functions to encode/decode/print the route header
 * 
 *         Version:  1.0
 *         Created:  20/11/05 04:24:55 CET
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

#include "../../parser/parse_rr.h"
#include "../../parser/parse_uri.h"
#include "encode_route.h"
#include "encode_uri.h"
#include "encode_header.h"
#include "encode_parameters.h"
#include "xaddress.h"


#define HAS_NAME_F		0x01
#define HAS_Q_F			0x02
#define HAS_EXPIRES_F		0x04
#define HAS_RECEIVED_F		0x08
#define HAS_METHOD_F		0x10

/*
 * encodes a (maybe aggregated) route/record-route header.
 * encoding is:
 * 1: flags
 * 1: number of routes present
 * N: fore each route present, the length of the route
 * N*M: the route structures concatenated
 */
int encode_route_body(char *hdr,int hdrlen,rr_t *route_parsed,unsigned char *where)
{
   int i=0,k,route_offset;
   unsigned char tmp[500];
   rr_t *myroute;
   
   for(route_offset=0,i=0,myroute=route_parsed;myroute;myroute=myroute->next,i++){
      if((k=encode_route(hdr,hdrlen,myroute,&tmp[route_offset]))<0){
	 LM_ERR("parsing route number %d\n",i);
	 return -1;
      }
      where[2+i]=(unsigned char)k;
      route_offset+=k;
   }
   where[1]=(unsigned char)i;
   memcpy(&where[2+i],tmp,route_offset);
   return 2+i+route_offset;
}

/* Encoder for route/record-route units (may be inside an aggegated
 * Route/RRoute header, see [rfc3261, 7.3.1, page 30] ).
 * Returns the length of the encoded structure in bytes
 * FORMAT (byte meanings):
 * 1: flags
 * 	0x01 :	there is a Display Name
 * 1: length of the XURI-encoded uri in bytes.
 * [2]: optionally, 1 HDR-based ptr to the displayname + the length of the name
 * N: the XURI-encoded URI.
 * [N:] optionally, HDR-based pointers to the different header-parameters
 *
 */
int encode_route(char *hdrstart,int hdrlen,rr_t *body,unsigned char *where)
{
   int i=2,j=0;/* 1*flags + 1*URI_len*/
   unsigned char flags=0;
   struct sip_uri puri;

   if(body->nameaddr.name.s && body->nameaddr.name.len){
      flags|=HAS_NAME_F;
      where[i++]=(unsigned char)(body->nameaddr.name.s-hdrstart);
      where[i++]=(unsigned char)body->nameaddr.name.len;
   }

   if (parse_uri(body->nameaddr.uri.s, body->nameaddr.uri.len,&puri) < 0 ) {
      LM_ERR("Bad URI in address\n");
	  return -1;
   }else{
      if((j=encode_uri2(hdrstart,hdrlen,body->nameaddr.uri,&puri,&where[i]))<0){
	 LM_ERR("error codifying the URI\n");
	 return -1;
      }else{
	 i+=j;
      }
   }
   where[0]=flags;
   where[1]=(unsigned char)j;
   i+=encode_parameters(&where[i],(void *)body->params,hdrstart,body,'n');
   return i;
}


int print_encoded_route_body(FILE *fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix)
{
   unsigned char numroutes;
   int i,offset;

   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"ENCODED CONTACT BODY:[":":",payload[i],i==paylen-1?"]\n":"");

   numroutes=payload[1];
   if(numroutes==0){
      LM_ERR("no routes present?\n");
      return -1;
   }
   for(i=0,offset=2+numroutes;i<numroutes;i++){
      print_encoded_route(fd,hdr,hdrlen,&payload[offset],payload[2+i],strcat(prefix,"  "));
      offset+=payload[2+i];
      prefix[strlen(prefix)-2]=0;
   }
   return 1;
}

int print_encoded_route(FILE *fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i=2;/* flags + urilength */
   unsigned char flags=0;

   flags=payload[0];
   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"ENCODED ROUTE=[":":",payload[i],i==paylen-1?"]\n":"");
   i=2;
   if(flags & HAS_NAME_F){
      fprintf(fd,"%sNAME=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(print_encoded_uri(fd,&payload[i],payload[1],hdr,hdrlen,strcat(prefix,"  "))<0){
      prefix[strlen(prefix)-2]=0;
      fprintf(fd,"Error parsing URI\n");
      return -1;
   }
   prefix[strlen(prefix)-2]=0;
   print_encoded_parameters(fd,&payload[i+payload[1]],hdr,paylen-i-payload[1],prefix);
   /* for(i+=payload[1];i<paylen-1;i+=2){
      fprintf(fd,"%s[PARAMETER[%.*s]",prefix,payload[i+1]-payload[i]-1,&hdr[payload[i]]);
      fprintf(fd,"VALUE[%.*s]]\n",(payload[i+2]-payload[i+1])==0?0:(payload[i+2]-payload[i+1]-1),&hdr[payload[i+1]]);
   }*/
   return 0;
}


int dump_route_body_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,FILE* fd,char segregationLevel,char *prefix)
{
   unsigned char numroutes;
   int i,offset;

   if(!segregationLevel){
      return dump_standard_hdr_test(hdr,hdrlen,(unsigned char*)payload,paylen,fd);
   }

   numroutes=payload[1];
   if(numroutes==0){
      LM_ERR("no routes present?\n");
      return -1;
   }
   if(segregationLevel & (JUNIT|SEGREGATE|ONLY_URIS)){
      for(i=0,offset=2+numroutes;i<numroutes;i++){
	 dump_route_test(hdr,hdrlen,&payload[offset],payload[2+i],fd,segregationLevel,prefix);
	 offset+=payload[2+i];
      }
   }
   return 1;
}

int dump_route_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,FILE* fd,char segregationLevel,char *prefix)
{
   int i=2;/* flags + urilength */
   unsigned char flags=0;

   if(!(segregationLevel & (ONLY_URIS|JUNIT))){
      return dump_standard_hdr_test(hdr,hdrlen,(unsigned char*)payload,paylen,fd);
   }
   flags=payload[0];
   i=2;
   if(flags & HAS_NAME_F){
      i+=2;
   }
   if((!(segregationLevel & JUNIT)) && (segregationLevel & ONLY_URIS)){
      return dump_standard_hdr_test(hdr,hdrlen,(unsigned char*)(payload+i),payload[1],fd);
   }
   if((segregationLevel & JUNIT) && (segregationLevel & ONLY_URIS)){
      return print_uri_junit_tests(hdr,hdrlen,&payload[i],payload[1],fd,1,"");
   }
   if(segregationLevel & JUNIT){
      i=2;
      fprintf(fd,"%sgetAddress.getDisplayName=(S)",prefix);
      if(flags & HAS_NAME_F){
	 fprintf(fd,"%.*s\n",payload[i+1],&hdr[payload[i]]);
	 i+=2;
      }else
	 fprintf(fd,"(null)\n");
      return print_uri_junit_tests(hdr,hdrlen,&payload[i],payload[1],fd,0,"getAddress.getURI.");
   }
   return 0;
}
