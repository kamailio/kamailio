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
 *        Filename:  encode_contact.c
 * 
 *     Description:  functions to encode/decode/print the contact header
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
#include "../../parser/contact/parse_contact.h"
#include "../../parser/contact/contact.h"
#include "../../parser/parse_uri.h"
#include "encode_contact.h"
#include "encode_uri.h"
#include "xaddress.h"
#include "encode_header.h"
#include "encode_parameters.h"

#define HAS_NAME_F		0x01
#define HAS_Q_F			0x02
#define HAS_EXPIRES_F		0x04
#define HAS_RECEIVED_F		0x08
#define HAS_METHOD_F		0x10
/*
 * encodes a (maybe aggregated) contact header.
 * encoding is:
 * 1: flags
 * 	0x01 this is a star contact (*)
 *[ 
 * 1: number of contacts present
 * N: fore each contact present, the length of the contact structure
 * N*M: the contact structures concatenated
 *]
 */
int encode_contact_body(char *hdr,int hdrlen,contact_body_t *contact_parsed,unsigned char *where)
{
   int i=0,k,contact_offset;
   unsigned char flags=0,tmp[500];
   contact_t *mycontact;

   if(contact_parsed->star){
      flags|=STAR_F;
      where[0]=flags;
      return 1;
   }
   for(contact_offset=0,i=0,mycontact=contact_parsed->contacts;mycontact;mycontact=mycontact->next,i++){
      if((k=encode_contact(hdr,hdrlen,mycontact,&tmp[contact_offset]))<0){
	 LM_ERR("parsing contact number %d\n",i);
	 return -1;
      }
      where[2+i]=k;
      contact_offset+=k;
   }
   where[1]=(unsigned char)i;
   memcpy(&where[2+i],tmp,contact_offset);
   return 2+i+contact_offset;
}

/* Encoder for contacts.
 * Returns the length of the encoded structure in bytes
 * FORMAT (byte meanings):
 * 1: flags
 * 	0x01 :	there is a Display Name
 * 	0x02 :	there is a Q parameter
 * 	0x04 :	there is an EXPIRES parameter
 * 	0x08 :	there is a RECEIVED parameter
 * 	0x10 :	there is a METHOD parameter
 * 1: length of the XURI-encoded uri in bytes.
 * [2]: optionally, 1 HDR-based ptr to the displayname + the length of the name
 * [2]: optionally, 1 HDR-based ptr to the q + the length of the q
 * [2]: optionally, 1 HDR-based ptr to the expires + the length of the expires
 * [2]: optionally, 1 HDR-based ptr to the received param + the length of the param
 * [2]: optionally, 1 HDR-based ptr to the method + the length of the method parameter
 * N: the XURI-encoded URI.
 * [N:] optionally, HDR-based pointers to the different header-parameters
 *
 */
int encode_contact(char *hdrstart,int hdrlen,contact_t *body,unsigned char *where)
{
   int i=2,j=0;/* 1*flags + 1*URI_len*/
   unsigned char flags=0;
   struct sip_uri puri;

   if(body->name.s && body->name.len){
      flags|=HAS_NAME_F;
      where[i++]=(unsigned char)(body->name.s-hdrstart);
      where[i++]=(unsigned char)body->name.len;
   }
   if(body->q){
      flags|=HAS_Q_F;
      where[i++]=(unsigned char)(body->q->name.s-hdrstart);
      where[i++]=(unsigned char)body->q->len;
   }
   if(body->expires){
      flags|=HAS_EXPIRES_F;
      where[i++]=(unsigned char)(body->expires->name.s-hdrstart);
      where[i++]=(unsigned char)body->expires->len;
   }
   if(body->received){
      flags|=HAS_RECEIVED_F;
      where[i++]=(unsigned char)(body->received->name.s-hdrstart);
      where[i++]=(unsigned char)body->received->len;
   }
   if(body->methods){
      flags|=HAS_METHOD_F;
      where[i++]=(unsigned char)(body->methods->name.s-hdrstart);
      where[i++]=(unsigned char)body->methods->len;
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
   /*CAUTION the  parameters are in reversed order !!! */
   /*TODO parameter encoding logic should be moved to a specific function...*/

   i+=encode_parameters(&where[i],body->params,hdrstart,body,'n');
   return i;
}

int print_encoded_contact_body(FILE *fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix)
{
   unsigned char flags, numcontacts;
   int i,offset;

   flags=payload[0];
   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"ENCODED CONTACT BODY:[":":",payload[i],i==paylen-1?"]\n":"");

   if(flags & STAR_F){
      fprintf(fd,"%sSTART CONTACT\n",prefix);
      return 1;
   }
   numcontacts=payload[1];
   if(numcontacts==0){
      LM_ERR("no contacts present?\n");
      return -1;
   }
   for(i=0,offset=2+numcontacts;i<numcontacts;i++){
      print_encoded_contact(fd,hdr,hdrlen,&payload[offset],payload[2+i],strcat(prefix,"  "));
      offset+=payload[2+i];
      prefix[strlen(prefix)-2]=0;
   }
   return 1;
}
int print_encoded_contact(FILE *fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i=2;/* flags + urilength */
   unsigned char flags=0;

   flags=payload[0];
   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"ENCODED CONTACT=[":":",payload[i],i==paylen-1?"]\n":"");
   i=2;
   if(flags & HAS_NAME_F){
      fprintf(fd,"%sCONTACT NAME=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_Q_F){
      fprintf(fd,"%sCONTACT Q=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_EXPIRES_F){
      fprintf(fd,"%sCONTACT EXPIRES=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_RECEIVED_F){
      fprintf(fd,"%sCONTACT RECEIVED=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags & HAS_METHOD_F){
      fprintf(fd,"%sCONTACT METHOD=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(print_encoded_uri(fd,&payload[i],payload[1],hdr,hdrlen,strcat(prefix,"  "))<0){
      prefix[strlen(prefix)-2]=0;
      fprintf(fd,"Error parsing URI\n");
      return -1;
   }
   prefix[strlen(prefix)-2]=0;

   print_encoded_parameters(fd,(unsigned char*)&payload[i+payload[1]],hdr,paylen-i-payload[1],prefix);
   return 0;
}


/**
 *
 */
int dump_contact_body_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,FILE *fd,char segregationLevel,char *prefix)
{
   unsigned char flags, numcontacts;
   int i,offset;

   flags=payload[0];

   if(!segregationLevel){
      return dump_standard_hdr_test(hdr,hdrlen,payload,paylen,fd);
   }
   if(flags & STAR_F){
      return 1;
   }
   numcontacts=payload[1];
   if(numcontacts==0){
      LM_ERR("no contacts present?\n");
      return -1;
   }
   if(segregationLevel & (JUNIT|SEGREGATE|ONLY_URIS)){
      for(i=0,offset=2+numcontacts;i<numcontacts;i++){
	 dump_contact_test(hdr,hdrlen,&payload[offset],payload[2+i],fd,segregationLevel,prefix);
	 offset+=payload[2+i];
      }
   }
   return 1;
}

int dump_contact_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,FILE *fd,char segregationLevel,char *prefix)
{
   int i=2;/* flags + urilength */
   unsigned char flags=0;

   flags=payload[0];
   if((segregationLevel & SEGREGATE)&& !(segregationLevel & ONLY_URIS))
      return dump_standard_hdr_test(hdr,hdrlen,payload,paylen,fd);
   i=2;
   if(flags & HAS_NAME_F)
      i+=2;
   if(flags & HAS_Q_F)
      i+=2;
   if(flags & HAS_EXPIRES_F)
      i+=2;
   if(flags & HAS_RECEIVED_F)
      i+=2;
   if(flags & HAS_METHOD_F)
      i+=2;
   if(!(segregationLevel & JUNIT) && (segregationLevel & ONLY_URIS))
      return dump_standard_hdr_test(hdr,hdrlen,(&payload[i]),(int)payload[1],fd);
   if((segregationLevel & JUNIT) && (segregationLevel & ONLY_URIS))
      return print_uri_junit_tests(hdr,hdrlen,&payload[i],payload[1],fd,1,"");
   if((segregationLevel & JUNIT) && !(segregationLevel & ONLY_URIS)){
      i=2;
      fprintf(fd,"%sgetAddress.getDisplayName=(S)",prefix);
      if(flags & HAS_NAME_F){
	 fprintf(fd,"%.*s\n",payload[i+1],&hdr[payload[i]]);
	 i+=2;
      }else
	 fprintf(fd,"(null)\n");
      fprintf(fd,"%sgetQValue=(F)",prefix);
      if(flags & HAS_Q_F){
	 fprintf(fd,"%.*s\n",payload[i+1],&hdr[payload[i]]);
	 i+=2;
      }else
	 fprintf(fd,"(null)\n");
      fprintf(fd,"%sgetExpires=(I)",prefix);
      if(flags & HAS_EXPIRES_F){
	 fprintf(fd,"%.*s\n",payload[i+1],&hdr[payload[i]]);
	 i+=2;
      }else
	 fprintf(fd,"(null)\n");
      if(flags & HAS_RECEIVED_F){
	 i+=2;
      }
      if(flags & HAS_METHOD_F){
	 i+=2;
      }
      fprintf(fd,"%sgetParameter=(SAVP)",prefix);
      for(i+=payload[1];i<paylen-1;i+=2){
	 printf("%.*s=",payload[i+1]-payload[i]-1,&hdr[payload[i]]);
	 printf("%.*s;",(payload[i+2]-payload[i+1])==0?0:(payload[i+2]-payload[i+1]-1),&hdr[payload[i+1]]);
      }
      fprintf(fd,"\n");
   }
   return 0;
}
