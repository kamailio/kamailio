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
 *        Filename:  main.c
 * 
 *     Description:  functions to encode a message
 * 
 *         Version:  1.0
 *         Created:  14/11/05 13:42:53 CET
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "../../parser/msg_parser.h"
#include "../../parser/parse_via.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "encode_header.h"
#include "encode_uri.h"
#include "encode_msg.h"
#include "xaddress.h"

unsigned int theSignal = 0xAA55AA55;/*which is: 10101010-01010101-10101010-01010101*/

char get_header_code(struct hdr_field *hf)
{
   switch(hf->type){
      case HDR_CALLID_T:
	 return 'i';
      case HDR_CONTACT_T:
	 return 'm';
      case HDR_CONTENTLENGTH_T:
	 return 'l';
      case HDR_CONTENTTYPE_T:
	 return 'c';
      case HDR_FROM_T:
	 return 'f';
      case HDR_SUBJECT_T:
	 return 's';
      case HDR_SUPPORTED_T:
	 return 'k';
      case HDR_TO_T:
	 return 't';
      case HDR_VIA_T:
	 return 'v';
      case HDR_ROUTE_T:
	 return 'r';
      case HDR_RECORDROUTE_T:
	 return 'R';
      case HDR_ALLOW_T:
	 return 'a';
      case HDR_ACCEPT_T:
	 return 'A';
      case HDR_CSEQ_T:
	 return 'S';
      case HDR_REFER_TO_T:
	 return 'o';
      case HDR_RPID_T:
	 return 'p';
      case HDR_EXPIRES_T:
	 return 'P';
      case HDR_AUTHORIZATION_T:
	 return 'H';
      case HDR_PROXYAUTH_T:
	 return 'z';
      default:
	 return 'x';
   }
   return 'x';
}

/* This function extracts meta-info from the sip_msg structure and
 * formats it so that it can be used to rapidly access the message structured
 * parts.
 *
 * RETURNS: LENGTH of structure on success, <0 if failure
 * if there was failure, you dont need to pkg_free the payload (it is done inside).
 * if there was success, you __NEED_TO_PKG_FREE_THE_PAYLOAD__ from the calling function.
 *
 * The encoded meta-info is composed by 3 sections:
 *
 * MSG_META_INFO:
 * 2: short int in network-byte-order, if <100, the msg is a REQUEST and the int
 * is the code of the METHOD. if >100, it is a RESPONSE and the int is the code
 * of the response.
 * 2: short int in NBO: payload-start based pointer (index) to where the SIP MSG starts.
 * 2: short int in NBO: the sip-message length
 * 2: METHOD or CODE string SIP-START-based pointer and length
 * 2: R-URI or REASON PHRASE string SIP-START-based pointer and length
 * 2: VERSION string SIP-START-based pointer and length
 * 2: short int in NBO: start of the content of the SIP message
 * [1+N]: in case this is a request, the length of the encoded-uri and the encoded-uri
 * 1: how many present headers have been found.
 *
 * MSG_HEADERS_INDEX:
 * N*3: groups of 3 bytes, each one describing a header struct: the first byte
 * is a letter that corresponds to a header type, the second and third bytes are a NBO
 * inidex to where this struct begins within the HEADERS_META_INFO section.
 *
 * HEADERS_META_INFO:
 * M: all the codified headers meta info structs one after another
 *
 * SIP_MSG:
 * the SIP message as it has been received.
 *
 * The length of the structure, will be ((short*)payload)[1] + ((short*)payload)[2]
 *
 * TODO: msg->parsed_uri msg->parsed_orig_uri_ok, msg->first_line->u.request.uri
 * buggy and little bit fuzzy
 */
int encode_msg(struct sip_msg *msg,char *payload,int len)
{
   int i,j,k,u,request;
   unsigned short int h;
   struct hdr_field* hf;
   struct msg_start* ms;
   struct sip_uri miuri;
   char *myerror=NULL;
   ptrdiff_t diff;

   if(len < MAX_ENCODED_MSG + MAX_MESSAGE_LEN)
      return -1;
   if(parse_headers(msg,HDR_EOH_F,0)<0){
      myerror="in parse_headers";
      goto error;
   }
   memset(payload,0,len);
   ms=&msg->first_line;
	if(ms->type == SIP_REQUEST)
		request=1;
	else if(ms->type == SIP_REPLY)
		request=0;
	else{
		myerror="message is neither request nor response";
		goto error;
	}
	if(request) {
		for(h=0;h<32;j=(0x01<<h),h++)
			if(j & ms->u.request.method_value)
				break;
	} else {
		h=(unsigned short)(ms->u.reply.statuscode);
	}
   if(h==32){/*statuscode wont be 32...*/
      myerror="unknown message type\n";
      goto error;
   }
   h=htons(h);
   /*first goes the message code type*/
   memcpy(payload,&h,2);
   h=htons((unsigned short int)msg->len);
   /*then goes the message start idx, but we'll put it later*/
   /*then goes the message length (we hope it to be less than 65535 bytes...)*/
   memcpy(&payload[MSG_LEN_IDX],&h,2);
   /*then goes the content start index (starting from SIP MSG START)*/
   if(0>(diff=(get_body(msg)-(msg->buf)))){
      myerror="body starts before the message (uh ?)";
      goto error;
   }else
      h=htons((unsigned short int)diff);
   memcpy(payload+CONTENT_IDX,&h,2);
   payload[METHOD_CODE_IDX]=(unsigned char)(request?
	 (ms->u.request.method.s-msg->buf):
	 (ms->u.reply.status.s-msg->buf));
   payload[METHOD_CODE_IDX+1]=(unsigned char)(request?
	 (ms->u.request.method.len):
	 (ms->u.reply.status.len));
   payload[URI_REASON_IDX]=(unsigned char)(request?
	 (ms->u.request.uri.s-msg->buf):
	 (ms->u.reply.reason.s-msg->buf));
   payload[URI_REASON_IDX+1]=(unsigned char)(request?
	 (ms->u.request.uri.len):
	 (ms->u.reply.reason.len));
   payload[VERSION_IDX]=(unsigned char)(request?
	 (ms->u.request.version.s-msg->buf):
	 (ms->u.reply.version.s-msg->buf));
   if(request){
      if (parse_uri(ms->u.request.uri.s,ms->u.request.uri.len, &miuri)<0){
	 LM_ERR("<%.*s>\n",ms->u.request.uri.len,ms->u.request.uri.s);
	 myerror="while parsing the R-URI";
	 goto error;
      }
      if(0>(j=encode_uri2(msg->buf,
		  ms->u.request.method.s-msg->buf+ms->len,
		  ms->u.request.uri,&miuri,
		  (unsigned char*)&payload[REQUEST_URI_IDX+1])))
      {
	    myerror="ENCODE_MSG: ERROR while encoding the R-URI";
	    goto error;
      }
      payload[REQUEST_URI_IDX]=(unsigned char)j;
      k=REQUEST_URI_IDX+1+j;
   }else
      k=REQUEST_URI_IDX;
   u=k;
   k++;
   for(i=0,hf=msg->headers;hf;hf=hf->next,i++);
   i++;/*we do as if there was an extra header, that marks the end of
	 the previous header in the headers hashtable(read below)*/
   j=k+3*i;
   for(i=0,hf=msg->headers;hf;hf=hf->next,k+=3){
      payload[k]=(unsigned char)(hf->type & 0xFF);
      h=htons(j);
      /*now goes a payload-based-ptr to where the header-code starts*/
      memcpy(&payload[k+1],&h,2);
      /*TODO fix this... fixed with k-=3?*/
      if(0>(i=encode_header(msg,hf,(unsigned char*)(payload+j),MAX_ENCODED_MSG+MAX_MESSAGE_LEN-j))){
	 LM_ERR("encoding header %.*s\n",hf->name.len,hf->name.s);
	 goto error;
	 k-=3;
	 continue;
      }
      j+=(unsigned short int)i;
   }
   /*now goes the number of headers that have been found, right after the meta-msg-section*/
   payload[u]=(unsigned char)((k-u-1)/3);
   j=htons(j);
   /*now copy the number of bytes that the headers-meta-section has occupied,right afther
    * headers-meta-section(the array with ['v',[2:where],'r',[2:where],'R',[2:where],...]
    * this is to know where the LAST header ends, since the length of each header-struct
    * is calculated substracting the nextHeaderStart - presentHeaderStart 
    * the k+1 is because payload[k] is usually the letter*/
   memcpy(&payload[k+1],&j,2);
   k+=3;
   j=ntohs(j);
   /*now we copy the headers-meta-section after the msg-headers-meta-section*/
   /*memcpy(&payload[k],payload2,j);*/
   /*j+=k;*/
   /*pkg_free(payload2);*/
   /*now we copy the actual message after the headers-meta-section*/
   memcpy(&payload[j],msg->buf,msg->len);
   LM_DBG("msglen = %d,msg starts at %d\n",msg->len,j);
   j=htons(j);
   /*now we copy at the beginning, the index to where the actual message starts*/
   memcpy(&payload[MSG_START_IDX],&j,2);
   return GET_PAY_SIZE( payload );
error:
   LM_ERR("%s\n",myerror);
   return -1;
}

int decode_msg(struct sip_msg *msg,char *code, unsigned int len)
{
   unsigned short int h;
   char *myerror=NULL;

   memcpy(&h,&code[2],2);
   h=ntohs(h);
   /*TODO use shorcuts in meta-info header.*/

   msg->buf=&code[h];
   memcpy(&h,&code[4],2);
   h=ntohs(h);
   msg->len=h;
   if(parse_headers(msg,HDR_EOH_F,0)<0){
      myerror="in parse_headers";
      goto error;
   }
error:
   LM_ERR("(%s)\n",myerror);
   return -1;
}

int print_encoded_msg(FILE* fd,char *code,char *prefix)
{
   unsigned short int i,j,k,l,m,msglen;
   char r,*msg;
   unsigned char *payload;
   
   payload=(unsigned char*)code;
   memcpy(&i,code,2);
   memcpy(&j,&code[MSG_START_IDX],2);
   memcpy(&msglen,&code[MSG_LEN_IDX],2);
   i=ntohs(i);
   j=ntohs(j);
   msglen=ntohs(msglen);
   for(k=0;k<j;k++)
      fprintf(fd,"%s%d%s",k==0?"ENCODED-MSG:[":":",payload[k],k==j-1?"]\n":"");
   msg=(char*)&payload[j];
   fprintf(fd,"MESSAGE:\n[%.*s]\n",msglen,msg);
   r=(i<100)?1:0;
   if(r){
      fprintf(fd,"%sREQUEST CODE=%d==%.*s,URI=%.*s,VERSION=%*.s\n",prefix,i,
	    payload[METHOD_CODE_IDX+1],&msg[payload[METHOD_CODE_IDX]],
	    payload[URI_REASON_IDX+1],&msg[payload[URI_REASON_IDX]],
	    payload[VERSION_IDX+1],&msg[payload[VERSION_IDX]]);
      print_encoded_uri(fd,&payload[REQUEST_URI_IDX+1],payload[REQUEST_URI_IDX],msg,50,strcat(prefix,"  "));
      prefix[strlen(prefix)-2]=0;
      i=REQUEST_URI_IDX+1+payload[REQUEST_URI_IDX];
   }else{
      fprintf(fd,"%sRESPONSE CODE=%d==%.*s,REASON=%.*s,VERSION=%.*s\n",prefix,i,
	    payload[METHOD_CODE_IDX+1],&msg[payload[METHOD_CODE_IDX]],
	    payload[URI_REASON_IDX+1],&msg[payload[URI_REASON_IDX]],
	    payload[VERSION_IDX+1],&msg[payload[VERSION_IDX]]);
      i=REQUEST_URI_IDX;
   }
   k=((payload[CONTENT_IDX]<<8)|payload[CONTENT_IDX+1]);
   j=msglen-k;
   fprintf(fd,"%sMESSAGE CONTENT:%.*s\n",prefix,j,&msg[k]);
   j=payload[i];
   fprintf(fd,"%sHEADERS PRESENT(%d):",prefix,j);
   i++;
   for(k=i;k<i+(j*3);k+=3)
      fprintf(fd,"%c%d%c",k==i?'[':',',payload[k],k==(i+3*j-3)?']':' ');
   fprintf(fd,"\n");
   for(k=i;k<i+(j*3);k+=3){
      memcpy(&l,&payload[k+1],2);
      memcpy(&m,&payload[k+4],2);
      l=ntohs(l);
      m=ntohs(m);
      print_encoded_header(fd,msg,msglen,&payload[l],m-l,payload[k],prefix);
   }
   return 1;
}

/*
 * Function to generate testing file, where we dump entire encoded-messages
 * preceded by a network-byte-order short int that says how long is the message,
 * or just encoded-headers. The last integer, is a flag set of which headers
 * must be dumped
 */

int dump_msg_test(char *code,FILE* fd,char header,char segregationLevel)
{
   unsigned short int i,j,l,m,msglen;
   int k;
   char r,*msg;
   unsigned char *payload;
   payload=(unsigned char*)code;
   memcpy(&i,code,2);/*the CODE of the request/response*/
   memcpy(&j,&code[MSG_START_IDX],2);/*where the MSG starts*/
   memcpy(&msglen,&code[MSG_LEN_IDX],2);/*how long the MSG is*/
   i=ntohs(i);
   j=ntohs(j);
   msglen=ntohs(msglen);
   if(header==0){
      fwrite(code,1,j+msglen,fd);
      fwrite(&theSignal,1,4,fd);
      return 0;
   }
   msg=(char*)&payload[j];
   r=(i<100)?1:0;
   if(r){
      if(segregationLevel & ALSO_RURI){
	 if(!(segregationLevel & JUNIT)){ 
	    
	    k=htonl(payload[REQUEST_URI_IDX+1]+payload[REQUEST_URI_IDX+2]);
	    fwrite(&k,1,4,fd);
	    fwrite(msg,1,ntohl(k),fd);
	    k=htonl((long)payload[REQUEST_URI_IDX]);
	    fwrite(&k,1,4,fd);
	    fwrite(&payload[REQUEST_URI_IDX+1],1,payload[REQUEST_URI_IDX],fd);
	    fwrite(&theSignal,1,4,fd);
	 }else
	    print_uri_junit_tests(msg,payload[REQUEST_URI_IDX+1]+payload[REQUEST_URI_IDX+2]
		  ,&payload[REQUEST_URI_IDX+1],payload[REQUEST_URI_IDX],fd,1,"");
      }
      i=REQUEST_URI_IDX+1+payload[REQUEST_URI_IDX];
   }else{
      i=REQUEST_URI_IDX;
   }
   j=payload[i];
   i++;
   for(k=i;k<i+(j*3);k+=3){
      memcpy(&l,&payload[k+1],2);
      memcpy(&m,&payload[k+4],2);
      l=ntohs(l);
      m=ntohs(m);
      if(header==(char)payload[k] ||
	    (header=='U' &&
	     (payload[k]=='f' ||
	      payload[k]=='t' ||
	      payload[k]=='m' ||
	      payload[k]=='o' ||
	      payload[k]=='p')))
	 dump_headers_test(msg,msglen,&payload[i+(j*3)+l+3],m-l,payload[k],fd,segregationLevel);
   }
   return 1;
}


/*
 * Function to generate testing file, where we dump entire encoded-messages
 * preceded by a network-byte-order short int that says how long is the message,
 * or just encoded-headers. The last integer, is a flag set of which headers
 * must be dumped
 */

int print_msg_junit_test(char *code,FILE* fd,char header,char segregationLevel)
{
   unsigned short int i,j,l,m,msglen;
   int k;
   char r,*msg;
   unsigned char *payload;
   payload=(unsigned char*)code;
   memcpy(&i,code,2);/*the CODE of the request/response*/
   memcpy(&j,&code[MSG_START_IDX],2);/*where the MSG starts*/
   memcpy(&msglen,&code[MSG_LEN_IDX],2);/*how long the MSG is*/
   i=ntohs(i);
   j=ntohs(j);
   msglen=ntohs(msglen);
   if(header==0){
      fwrite(code,1,j+msglen,fd);
      fwrite(&theSignal,1,4,fd);
      return 0;
   }
   msg=(char*)&payload[j];
   r=(i<100)?1:0;
   if(r){
      if(segregationLevel & ALSO_RURI){
	 k=htonl(50);
	 fwrite(&k,1,4,fd);
	 fwrite(msg,1,50,fd);
	 k=htonl((long)payload[REQUEST_URI_IDX]);
	 fwrite(&k,1,4,fd);
	 fwrite(&payload[REQUEST_URI_IDX+1],1,payload[REQUEST_URI_IDX],fd);
	 fwrite(&theSignal,1,4,fd);
      }
      i=REQUEST_URI_IDX+1+payload[REQUEST_URI_IDX];
   }else{
      i=REQUEST_URI_IDX;
   }
   j=payload[i];
   i++;
   for(k=i;k<i+(j*3);k+=3){
      memcpy(&l,&payload[k+1],2);
      memcpy(&m,&payload[k+4],2);
      l=ntohs(l);
      m=ntohs(m);
      if(header==(char)payload[k] ||
	    (header=='U' &&
	     (payload[k]=='f' ||
	      payload[k]=='t' ||
	      payload[k]=='m' ||
	      payload[k]=='o' ||
	      payload[k]=='p')))
	 dump_headers_test(msg,msglen,&payload[i+(j*3)+l+3],m-l,payload[k],fd,segregationLevel);
   }
   return 1;
}
