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
 *        Filename:  xuri.c
 * 
 *     Description:  first trial to implement xuri
 * 
 *         Version:  1.0
 *         Created:  16/11/05 18:07:24 CET
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
#include <assert.h>
#include <string.h>
#include <netinet/in.h>
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "../../dprint.h"
#include "encode_uri.h"
#include "encode_parameters.h"
#include "encode_header.h"
#include "xaddress.h"

#define REL_PTR(a,b) ((unsigned char)(b-a))

/*The XURI is one of the most important parts in SEAS, as
 * most of the SIP MESSAGE structured headers (that is,
 * headers which have a well-specified body construction)
 * use the URI as the body (ie. via, route, record-route,
 * contact, from, to, RURI)
 *
 * the XURI is a codified structure of flags and pointers
 * that ease the parsing of the URI string.
 *
 * 1: The first byte of the structure, is a
 * HEADER_START-based pointer to the beginning of the URI
 * (including the "sip:").  
 * 1: The next byte is the length of the uri, so URIMAX 
 * is 256 (enough...) 
 * 2: Flags specifying the parts that are present in the URI
 *
 * as follows:
 * 	1: first byte
 * 		SIP_OR_TEL_F	0x01 it is SIP or TEL uri.
 *		SECURE_F	0x02 it is secure or not (SIPS,TELS)
 *		USER_F		0x04 it as a user part (user@host)
 *		PASSWORD_F	0x08 it has a password part
 *		HOST_F		0x10 it has a host part
 *		PORT_F		0x20 it has a port part
 *		PARAMETERS_F	0x40 it has a port part
 *		HEADERS_F	0x80 it has a port part
 *	1: second byte
 *		TRANSPORT_F	0x01 it has other parameters
 *		TTL_F		0x02 it has headers
 *		USER_F		0x04 it has the transport parameter
 *		METHOD_F	0x08 it has the ttl parameter
 *		MADDR_F		0x10 it has the user parameter
 *		LR_F		0x20 it has the method parameter
 *
 * All the following bytes are URI_START-based pointers to
 * the fields that are present in the uri, as specified by
 * the flags. They must appear in the same order shown in
 * the flags, and only appear if the flag was set to 1.
 *
 * the end of the field, will be the place where the
 * following pointer points to, minus one (note that all the
 * fields present in a URI are preceded by 1 character, ie
 * sip[:user][:passwod][@host][:port][;param1=x][;param2=y][?hdr1=a][&hdr2=b]$p
 * it will be necessary to have a pointer at the end,
 * pointing two past the end of the uri, so that the length
 * of the last header can be computed.
 *
 * The reason to have the OTHER and HEADERS flags at the
 * beginning(just after the strictly-uri stuff), is that it
 * will be necessary to know the length of the parameters
 * section and the headers section.  
 *
 * The parameters can
 * appear in an arbitrary order, so they won't be following
 * the convention of transport-ttl-user-method-maddr-lr, so
 * we can't rely on the next pointer to compute the length
 * of the previous pointer field, as the ttl param can 
 * appear before the transport param. so the parameter 
 * pointers must have 2 bytes: pointer+length.
 *
 */
int encode_uri2(char *hdr,int hdrlen,str uri_str, struct sip_uri *uri_parsed,unsigned char *payload)
{
   int i=4,j;/* 1*pointer+1*len+2*flags*/
   unsigned int scheme;
   unsigned char flags1=0,flags2=0,uriptr;

   uriptr=REL_PTR(hdr,uri_str.s);
   if(uri_str.len>255 || uriptr>hdrlen){
      LM_ERR("uri too long, or out of the sip_msg bounds\n");
      return -1;
   }
   payload[0]=uriptr;
   payload[1]=(unsigned char)uri_str.len;
   if(uri_parsed->user.s && uri_parsed->user.len){
      flags1 |= USER_F;
      payload[i++]=REL_PTR(uri_str.s,uri_parsed->user.s);
   }
   if(uri_parsed->passwd.s && uri_parsed->passwd.len){
      flags1 |= PASSWORD_F;
      payload[i++]=REL_PTR(uri_str.s,uri_parsed->passwd.s);
   }
   if(uri_parsed->host.s && uri_parsed->host.len){
      flags1 |= HOST_F;
      payload[i++]=REL_PTR(uri_str.s,uri_parsed->host.s);
   }
   if(uri_parsed->port.s && uri_parsed->port.len){
      flags1 |= PORT_F;
      payload[i++]=REL_PTR(uri_str.s,uri_parsed->port.s);
   }
   if(uri_parsed->params.s && uri_parsed->params.len){
      flags1 |= PARAMETERS_F;
      payload[i++]=REL_PTR(uri_str.s,uri_parsed->params.s);
   }
   if(uri_parsed->headers.s && uri_parsed->headers.len){
      flags1 |= HEADERS_F;
      payload[i++]=REL_PTR(uri_str.s,uri_parsed->headers.s);
   }
   payload[i]=(unsigned char)(uri_str.len+1);
   i++;

   if(uri_parsed->transport.s && uri_parsed->transport.len){
      flags2 |= TRANSPORT_F;
      payload[i]=REL_PTR(uri_str.s,uri_parsed->transport.s);
      payload[i+1]=(unsigned char)(uri_parsed->transport.len);
      i+=2;
   }
   if(uri_parsed->ttl.s && uri_parsed->ttl.len){
      flags2 |= TTL_F;
      payload[i]=REL_PTR(uri_str.s,uri_parsed->ttl.s);
      payload[i+1]=(unsigned char)uri_parsed->ttl.len;
      i+=2;
   }
   if(uri_parsed->user_param.s && uri_parsed->user_param.len){
      flags2 |= USER_F;
      payload[i]=REL_PTR(uri_str.s,uri_parsed->user_param.s);
      payload[i+1]=(unsigned char)uri_parsed->user_param.len;
      i+=2;
   }
   if(uri_parsed->method.s && uri_parsed->method.len){
      flags2 |= METHOD_F;
      payload[i]=REL_PTR(uri_str.s,uri_parsed->method.s);
      payload[i+1]=(unsigned char)uri_parsed->method.len;
      i+=2;
   }
   if(uri_parsed->maddr.s && uri_parsed->maddr.len){
      flags2 |= MADDR_F;
      payload[i]=REL_PTR(uri_str.s,uri_parsed->maddr.s);
      payload[i+1]=(unsigned char)uri_parsed->maddr.len;
      i+=2;
   }
   if(uri_parsed->lr.s && uri_parsed->lr.len){
      flags2 |= LR_F;
      payload[i]=REL_PTR(uri_str.s,uri_parsed->lr.s);
      payload[i+1]=(unsigned char)uri_parsed->lr.len;
      i+=2;
   }
   /*in parse_uri, when there's a user=phone, the type
    * is set to TEL_URI_T, even if there's a sip: in the beginning
    * so lets check it by ourselves:
   switch(uri_parsed->type){
      case SIP_URI_T:
	 flags1 |= SIP_OR_TEL_F;
	 break;
      case SIPS_URI_T:
	 flags1 |= (SIP_OR_TEL_F|SECURE_F);
	 break;
      case TEL_URI_T:
	 break;
      case TELS_URI_T:
	 flags1 |= SECURE_F;
	 break;
      default:
	 return -1;
   }*/
#define SIP_SCH		0x3a706973
#define SIPS_SCH	0x73706973
#define TEL_SCH		0x3a6c6574
#define TELS_SCH	0x736c6574
   scheme=uri_str.s[0]+(uri_str.s[1]<<8)+(uri_str.s[2]<<16)+(uri_str.s[3]<<24);
   scheme|=0x20202020;
   if (scheme==SIP_SCH){
      flags1 |= SIP_OR_TEL_F;
   }else if(scheme==SIPS_SCH){
      if(uri_str.s[4]==':'){
	 flags1 |= (SIP_OR_TEL_F|SECURE_F);
      }else goto error;
   }else if (scheme==TEL_SCH){
      /*nothing*/
   }else if (scheme==TELS_SCH){
      if(uri_str.s[4]==':'){
	 flags1 |= SECURE_F;
      }
   }else goto error;
	
   payload[2]=flags1;
   payload[3]=flags2;
   j=i;
   i+=encode_parameters(&payload[i],uri_parsed->params.s,uri_str.s,&uri_parsed->params.len,'u');
   if(i<j)
      goto error;
   return i;
error:
   return -1;
}

int print_encoded_uri(FILE *fd,unsigned char *payload,int paylen,char *hdrstart,int hdrlen,char *prefix)
{
   int i=4,j=0;/*1*pointer+1*len+2*flags*/
   unsigned char uriidx=0,flags1=0,flags2=0,urilen;
   char *ch_uriptr,*uritype=NULL,*secure=NULL;

   uriidx=payload[0];
   fprintf(fd,"%s",prefix);
   for(j=0;j<paylen;j++)
      fprintf(fd,"%s%d%s",j==0?"ENCODED-URI:[":":",payload[j],j==paylen-1?"]\n":"");
   if(uriidx>hdrlen){
      fprintf(fd,"bad index for start of uri: hdrlen=%d uri_index=%d\n",hdrlen,uriidx);
      return -1;
   }
   ch_uriptr = hdrstart+uriidx;
   urilen=payload[1];
   flags1=payload[2];
   flags2=payload[3];
   fprintf(fd,"%sURI:[%.*s]\n",prefix,urilen,ch_uriptr);
   uritype=flags1&SIP_OR_TEL_F?"SIP":"TEL";
   secure=flags1&SECURE_F?"S":"";
   fprintf(fd,"%s  TYPE:[%s%s]\n",prefix,uritype,secure);
   if(flags1 & USER_F){
      fprintf(fd,"%s  USER:[%.*s]\n",prefix,(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }
   if(flags1 & PASSWORD_F){
      fprintf(fd,"%s  PASSWORD=[%.*s]\n",prefix,(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }
   if(flags1 & HOST_F){
      fprintf(fd,"%s  HOST=[%.*s]\n",prefix,(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }
   if(flags1 & PORT_F){
      fprintf(fd,"%s  PORT=[%.*s]\n",prefix,(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }
   if(flags1 & PARAMETERS_F){
      fprintf(fd,"%s  PARAMETERS=[%.*s]\n",prefix,(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }
   if(flags1 & HEADERS_F){
      fprintf(fd,"%s  HEADERS=[%.*s]\n",prefix,(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }
   ++i;
   if(flags2 & TRANSPORT_F){
      fprintf(fd,"%s  TRANSPORT=[%.*s]\n",prefix,payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }
   if(flags2 & TTL_F){
      fprintf(fd,"%s  TTL_F=[%.*s]\n",prefix,payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }
   if(flags2 & USER_F){
      fprintf(fd,"%s  USER_F=[%.*s]\n",prefix,payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }
   if(flags2 & METHOD_F){
      fprintf(fd,"%s  METHOD_F=[%.*s]\n",prefix,payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }
   if(flags2 & MADDR_F){
      fprintf(fd,"%s  MADDR_F=[%.*s]\n",prefix,payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }
   if(flags2 & LR_F){
      fprintf(fd,"%s  LR_F=[%.*s]\n",prefix,payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }
   print_encoded_parameters(fd,&payload[i],ch_uriptr,paylen-i,prefix);
   return 0;
}


int print_uri_junit_tests(char *hdrstart,int hdrlen,unsigned char *payload,int paylen,FILE *fd,char also_hdr,char *prefix)
{
   int i=4,k=0,m=0;/*1*pointer+1*len+2*flags*/
   unsigned char uriidx=0,flags1=0,flags2=0,urilen;
   char *ch_uriptr,*aux,*aux2,*aux3,*uritype=NULL,*secure=NULL;

   uriidx=payload[0];
   if(uriidx>hdrlen){
      fprintf(fd,"bad index for start of uri: hdrlen=%d uri_index=%d\n",hdrlen,uriidx);
      return -1;
   }

   if(also_hdr)
      dump_standard_hdr_test(hdrstart,hdrlen,payload,paylen,fd);
   ch_uriptr = hdrstart+uriidx;
   urilen=payload[1];
   flags1=payload[2];
   flags2=payload[3];
   fprintf(fd,"%stoString=(S)%.*s\n",prefix,urilen,ch_uriptr);
   uritype=flags1&SIP_OR_TEL_F?"sip":"tel";
   secure=flags1&SECURE_F?"s":"";
   fprintf(fd,"%sgetScheme=(S)%s%s\n",prefix,uritype,secure);
   fprintf(fd,"%sisSecure=(B)%s\n",prefix,flags1&SECURE_F?"true":"false");
   fprintf(fd,"%sisSipURI=(B)%s\n",prefix,"true");
   fprintf(fd,"%sgetUser=(S)",prefix);
   if(flags1 & USER_F){
      fprintf(fd,"%.*s\n",(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }else 
      fprintf(fd,"(null)\n");
   fprintf(fd,"%sgetUserPassword=(S)",prefix);
   if(flags1 & PASSWORD_F){
      fprintf(fd,"%.*s\n",(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }else
      fprintf(fd,"(null)\n");
   fprintf(fd,"%sgetHost=(S)",prefix);
   if(flags1 & HOST_F){
      fprintf(fd,"%.*s\n",(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }else
      fprintf(fd,"(null)\n");/*can't happen*/
   fprintf(fd,"%sgetPort=(I)",prefix);
   if(flags1 & PORT_F){
      fprintf(fd,"%.*s\n",(payload[i+1]-1)-payload[i],&ch_uriptr[payload[i]]);
      ++i;
   }else
      fprintf(fd,"(null)\n");
   /*user=phone;transport=udp*/
   if(flags1 & PARAMETERS_F){
      aux=&ch_uriptr[payload[i]];
      aux2=NULL;
      aux3=aux;
      m=(payload[i+1]-1-payload[i]);
      fprintf(fd,"%sgetParameter=(SAVP)",prefix);/*SVP = Attribute Value Pair*/
      for(k=0;k<=m;k++){
	 if((aux3[k]==';'||(k==m)) && aux2==NULL){/*no parameterValue was found*/
	    fprintf(fd,"%.*s=;",(int)(aux3-aux+k),aux);
	    aux2=NULL;/*resets the parameterValue-start pointer*/
	    aux=aux3+1+k;/*points to the next parameter*/
	 }else 
	    if((aux3[k]==';'||(k==m)) && aux2!=NULL){
	       fprintf(fd,"%.*s=%.*s;",(int)(aux2-aux),aux,(int)(aux3-aux2-1+k),aux2+1);
	       aux2=NULL;
	       aux=aux3+1+k;
	    } else 
	       if(aux3[k]=='='){
		  aux2=aux3+k;
	       }
      }
      fprintf(fd,"\n");
      ++i;
   }
   if(flags1 & HEADERS_F){
      aux=&ch_uriptr[payload[i]];
      aux2=NULL;
      aux3=aux;
      m=(payload[i+1]-1-payload[i]);
      fprintf(fd,"%sgetHeader=(SAVP)",prefix);
      for(k=0;k<=m;k++){
	 if((aux3[k]==';'||(k==m)) && aux2==NULL){/*no parameterValue was found*/
	    fprintf(fd,"%.*s=;",(int)(aux3-aux+k),aux);
	    aux2=NULL;/*resets the parameterValue-start pointer*/
	    aux=aux3+1+k;/*points to the next parameter*/
	 }else 
	    if((aux3[k]==';'||(k==m)) && aux2!=NULL){
	       fprintf(fd,"%.*s=%.*s;",(int)(aux2-aux),aux,(int)(aux3-aux2-1+k),aux2+1);
	       aux2=NULL;
	       aux=aux3+1+k;
	    } else 
	       if(aux3[k]=='='){
		  aux2=aux3+k;
	       }
      }
      fprintf(fd,"\n");
      ++i;
   }
   ++i;
   fprintf(fd,"%sgetTransportParam=(S)",prefix);
   if(flags2 & TRANSPORT_F){
      fprintf(fd,"%.*s\n",payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }else
      fprintf(fd,"(null)\n");
   fprintf(fd,"%sgetTTLparam=(I)",prefix);
   if(flags2 & TTL_F){
      fprintf(fd,"%.*s\n",payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }else
      fprintf(fd,"(null)\n");
   fprintf(fd,"%sgetUserParam=(S)",prefix);
   if(flags2 & USER_F){
      fprintf(fd,"%.*s\n",payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }else
      fprintf(fd,"(null)\n");
   fprintf(fd,"%sgetMethodParam=(S)",prefix);
   if(flags2 & METHOD_F){
      fprintf(fd,"%.*s\n",payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }else
      fprintf(fd,"(null)\n");
   fprintf(fd,"%sgetMAddrParam=(S)",prefix);
   if(flags2 & MADDR_F){
      fprintf(fd,"%.*s\n",payload[i+1],&ch_uriptr[payload[i]]);
      i+=2;
   }else
      fprintf(fd,"(null)\n");
   if(flags2 & LR_F){
      i+=2;
   }
   fprintf(fd,"\n");
   return 0;
}
