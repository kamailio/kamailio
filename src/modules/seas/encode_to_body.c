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
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../dprint.h"
#include "encode_to_body.h"
#include "encode_uri.h"
#include "encode_header.h"
#include "encode_parameters.h"
#include "xaddress.h"


/* Encoder for From and To headers.
 * Returns the length of the encoded structure in bytes
 * FORMAT (byte meanings):
 * 1: flags
 * 	0x10 :	there is a Display Name
 * 	0x20 :	there is a tag parameter
 * 	0x40 :	there are other parameters
 * 1: length of the XURI-encoded uri in bytes.
 * [2]: optionally, 1 HDR-based ptr to the displayname + the length of the name
 * [2]: optionally, 1 HDR-based ptr to the tag + the length of the tag parameter
 * N: the XURI-encoded URI.
 * [N:] optionally, HDR-based pointers to the different header-parameters
 *
 */
int encode_to_body(char *hdrstart,int hdrlen,struct to_body *body,unsigned char *where)
{
   int i=2,j=0;/* 1*flags + 1*URI_len*/
   unsigned char flags=0;
   struct sip_uri puri;

   if(body->display.s && body->display.len){
      flags|=HAS_DISPLAY_F;
      if(body->display.s[0]=='\"'){
	 body->display.s++;
	 body->display.len-=2;
      }
      where[i++]=(unsigned char)(body->display.s-hdrstart);
      where[i++]=(unsigned char)(body->display.len);
   }
   if(body->tag_value.s && body->tag_value.len){
      flags|=HAS_TAG_F;
      where[i++]=(unsigned char)(body->tag_value.s-hdrstart);
      where[i++]=(unsigned char)body->tag_value.len;
   }
   if (parse_uri(body->uri.s, body->uri.len,&puri) < 0 ) {
      LM_ERR("Bad URI in address\n");
      return -1;
   }else{
      if((j=encode_uri2(hdrstart,hdrlen,body->uri,&puri,&where[i]))<0){
	 LM_ERR("failed to codify the URI\n");
	 return -1;
      }else{
	 i+=j;
      }
   }
   where[0]=flags;
   where[1]=(unsigned char)j;
   i+=encode_parameters(&where[i],(void *)body->param_lst,hdrstart,body,'t');

   return i;
}

int print_encoded_to_body(FILE *fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i=2;/* flags + urilength */
   unsigned char flags=0;

   flags=payload[0];
   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"BODY CODE=[":":",payload[i],i==paylen-1?"]\n":"");
   i=2;
   if(flags & HAS_DISPLAY_F){
      fprintf(fd,"%sDISPLAY NAME=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_TAG_F){
      fprintf(fd,"%sTAG=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(print_encoded_uri(fd,&payload[i],payload[1],hdr,hdrlen,strcat(prefix,"  "))<0){
      fprintf(fd,"Error parsing URI\n");
      prefix[strlen(prefix)-2]=0;
      return -1;
   }
   prefix[strlen(prefix)-2]=0;
   print_encoded_parameters(fd,&payload[i+payload[1]],hdr,paylen-i-payload[1],prefix);
   return 0;
}

/**
 * dumps to FD a NBO int which is the header length, the header,
 * an NBO int which is the payload length, and the payload.
 *
 * hdr is the header,
 * hdrlen is the header length,
 * payload is the payload,
 * paylen is the payload length,
 * fd is the file descriptor to which to dump,
 * segregationLevel is wether only URIS must be dumped or all the header code.
 *
 * return 0 on success, <0 on error
 */
int dump_to_body_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,FILE* fd,char segregationLevel)
{
   int i=2;/* flags + urilength */
   unsigned char flags=0;

   flags=payload[0];
   if(!segregationLevel){
      return dump_standard_hdr_test(hdr,hdrlen,payload,paylen,fd);
   }
   i=2;
   if(flags & HAS_DISPLAY_F){
      i+=2;
   }
   if(flags & HAS_TAG_F){
      i+=2;
   }
   if(!(segregationLevel & JUNIT) && (segregationLevel & ONLY_URIS)){
     return dump_standard_hdr_test(hdr,hdrlen,&payload[i],payload[1],fd);
   }
   if((segregationLevel & JUNIT) && (segregationLevel & ONLY_URIS)){
     return print_uri_junit_tests(hdr,hdrlen,&payload[i],payload[1],fd,1,"");
   }
   return 0;
}
