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
 *        Filename:  xcontact.c
 * 
 *     Description:  Contact encoding functions
 * 
 *         Version:  1.0
 *         Created:  19/11/05 14:33:38 CET
 *        Revision:  none
 *        Compiler:  gcc
 * 
 *          Author:  Elias Baixas (EB), elias@conillera.net
 *         Company:  VozTele.com
 * 
 * =====================================================================================
 */
#define _GNU_SOURCE
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/contact/contact.h"
#include "../../parser/digest/digest.h"
#include "../../parser/digest/digest_parser.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_methods.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "encode_to_body.h"
#include "encode_contact.h"
#include "encode_digest.h"
#include "encode_via.h"
#include "encode_cseq.h"
#include "encode_route.h"
#include "encode_content_length.h"
#include "encode_content_type.h"
#include "encode_expires.h"
#include "encode_allow.h"
#include "xaddress.h"
#include "encode_msg.h"
#include "encode_header.h"

#define REL_PTR(a,b) ((unsigned char)(b-a))
#define MAX_CONTACT_LEN 50
#define MAX_XHDR_LEN 255

#define SEGREGATE 0x02
#define ONLY_URIS 0x01
#define HEADER_OFFSET_IDX 0
#define HEADER_LEN_IDX (HEADER_OFFSET_IDX+2)
#define HEADER_NAME_LEN_IDX (HEADER_LEN_IDX+2)
#define HEADER_PAYLOAD_IDX (HEADER_NAME_LEN_IDX+1)

/*
 * This function encodes an arbitrary header into a chunk of bytes,
 * ready to be sent to the Application Server.
 *
 * The header codes start with this encoded-bytes:
 * 2: SIP-MSG-START based pointer to the header (including header name)
 * 2: length of the header
 * 1: length of the header name
 */
int encode_header(struct sip_msg *sipmsg,struct hdr_field *hdr,unsigned char *payload,int paylen)
{
   int len=0;
   unsigned int integer,*methods=0;
   char *hdrstart,*tmp;
   unsigned short int ptr;
   struct to_body *tobody=0;
   struct via_body *viabody=0;
   struct cseq_body *cseqbody=0;
   char *msg,*myerror;
   int mlen;

   msg=sipmsg->buf;
   mlen=sipmsg->len;
   hdrstart = hdr->name.s;
   if(hdrstart-msg<0){
      LM_ERR("header(%.*s) does not belong to sip_msg(hdrstart<msg)\n",
			  hdr->name.len,hdr->name.s);
      return -1;
   }
   ptr=htons((short int)(hdrstart-msg));
   if((hdrstart-msg)>mlen){
      LM_ERR("out of the sip_msg bounds (%d>%d)\n",ntohs(ptr),mlen);
      return -1;
   }
   if(hdr->len>(1<<16)){
      LM_ERR("length of header too long\n");
      return -1;
   }
   memcpy(payload,&ptr,2);
   ptr=htons((short int)(hdr->len));
   memcpy(payload+HEADER_LEN_IDX,&ptr,2);
   payload[HEADER_NAME_LEN_IDX]=(unsigned char)hdr->name.len;
   if(hdr->len>256){
      LM_INFO("header bigger than 256 bytes. Skipping express-encoding\n");
      return 4;/*2 for header offset + 2 for header length*/
   }
   switch(hdr->type){
      case HDR_FROM_T:
      case HDR_TO_T:
      case HDR_REFER_TO_T:
      case HDR_RPID_T:
	 if(!hdr->parsed){
	    if((tobody=pkg_malloc(sizeof(struct to_body)))==0){
	       myerror="Out of memory !!\n";
	       goto error;
	    }
	    memset(tobody,0,sizeof(struct to_body));
	    parse_to(hdr->body.s,hdr->body.s+hdr->body.len+1,tobody);
	    if (tobody->error == PARSE_ERROR) {
	       myerror="bad (REFER,TO,FROM,RPID) header\n";
	       free_to(tobody);
	       return 5;
	       goto error;
	    }
	    hdr->parsed=(struct to_body*)tobody;
	 }else
	    tobody=(struct to_body*)hdr->parsed;
	 if((len=encode_to_body(hdr->name.s,hdr->len,tobody,payload+5))<0){
	    myerror="parsing from or to header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_CONTACT_T:
	 if(!hdr->parsed)
	    if(parse_contact(hdr)<0){
	       myerror="parsing contact\n";
	       goto error;
	    }
	 if((len=encode_contact_body(hdr->name.s,hdr->len,(contact_body_t*)hdr->parsed,payload+5))<0){
	    myerror="encoding contact header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_ROUTE_T:
      case HDR_RECORDROUTE_T:
	 if(!hdr->parsed)
	    if(parse_rr(hdr)<0){
	       myerror="encoding route or recordroute\n";
	       goto error;
	    }
	 if((len=encode_route_body(hdr->name.s,hdr->len,(rr_t*)hdr->parsed,payload+5))<0){
	    myerror="encoding route or recordroute header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_CONTENTLENGTH_T:
	 if(!hdr->parsed){
	    tmp=parse_content_length(hdr->body.s,hdr->body.s+hdr->body.len+1,(int*)&integer);
	    if (tmp==0){
	       myerror="bad content_length header\n";
	       goto error;
	    }
	    hdr->parsed=(void*)(long)integer;
	 }
	 if((len=encode_contentlength(hdr->name.s,hdr->len,(long int)hdr->parsed,(char*)(payload+5)))<0){
	    myerror="encoding content-length header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_VIA_T:
	 if(!hdr->parsed){
	    if((viabody=pkg_malloc(sizeof(struct via_body)))==0){
	       myerror="out of memory\n";
	       goto error;
	    }
	    memset(viabody,0,sizeof(struct via_body));
	    if(parse_via(hdr->body.s,hdr->body.s+hdr->body.len+1,viabody)==0){
	       myerror="encoding via \n";
	       goto error;
	    }
	    hdr->parsed=viabody;
	 }
	 if((len=encode_via_body(hdr->name.s,hdr->len,(struct via_body*)hdr->parsed,payload+5))<0){
	    myerror="encoding via header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_ACCEPT_T:
	 if(!hdr->parsed){
	    if(parse_accept_hdr(sipmsg)<0){
	       return 5;
	    }
	 }
	 if((len=encode_accept(hdr->name.s,hdr->len,(unsigned int*)hdr->parsed,(char*)(payload+5)))<0){
	    myerror="encoding via header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_CONTENTTYPE_T:
	 if(!hdr->parsed){
	    if(parse_content_type_hdr(sipmsg)<0){
	       myerror="encoding content-type header\n";
	       goto error;
	    }
	 }
	 if((len=encode_content_type(hdr->name.s,hdr->len,(unsigned int)(long int)hdr->parsed,(char*)(payload+5)))<0){
	    myerror="encoding via header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_CSEQ_T:
	 if(!hdr->parsed){
	    if((cseqbody=pkg_malloc(sizeof(struct cseq_body)))==0){
	       myerror="out of memory\n";
	       goto error;
	    }
	    memset(cseqbody,0,sizeof(struct cseq_body));
	    if(parse_cseq(hdr->name.s,hdr->body.s+hdr->body.len+1,cseqbody)==0){
	       myerror="encoding cseq header\n";
	       goto error;
	    }
	    hdr->parsed=cseqbody;
	 }
	 if((len=encode_cseq(hdr->name.s,hdr->len,(struct cseq_body*)hdr->parsed,payload+5))<0){
	    myerror="encoding via header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_EXPIRES_T:
	 if(!hdr->parsed){
	    if(parse_expires(hdr)<0){
	       myerror="encoding expires header\n";
	       goto error;
	    }
	 }
	 if((len=encode_expires(hdr->name.s,hdr->len,(exp_body_t *)hdr->parsed,payload+5))<0){
	    myerror="encoding expires header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_ALLOW_T:
	 if(!hdr->parsed){
	    if((methods=pkg_malloc(sizeof(unsigned int)))==0){
	       myerror="out of memory\n";
	       goto error;
	    }
	    *methods=0;
	    if(parse_methods(&hdr->body,methods)!=0){
	       myerror="encoding allow header\n";
	       pkg_free(methods);
	       return 5;
	       /*goto error;*/
	    }
	    hdr->parsed=methods;
	 }
	 if((len=encode_allow(hdr->name.s,hdr->len,(unsigned int*)hdr->parsed,(char*)(payload+5)))<0){
	    myerror="encoding allow header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      case HDR_AUTHORIZATION_T:
      case HDR_PROXYAUTH_T:
	 if(!hdr->parsed){
	    if(parse_credentials(hdr)<0){
	       myerror="encoding a digest header\n";
	       goto error;
	    }
	 }
	 if((len=encode_digest(hdr->name.s,hdr->len,(dig_cred_t*)(&(((auth_body_t*)hdr->parsed)->digest)),payload+5))<0){
	    myerror="encoding allow header\n";
	    goto error;
	 }else{
	    return 5+len;
	 }
	 break;
      default:
	 return 5;
   }
   return 1;
error:
   if(tobody)
      pkg_free(tobody);
   if(cseqbody)
      pkg_free(cseqbody);
   if(viabody)
      free_via_list(viabody);
   if(methods)
      pkg_free(methods);
   LM_ERR("%s",myerror);
   return -1;
}

int print_encoded_header(FILE *fd,char *msg,int msglen,unsigned char *payload,int len,char type,char *prefix)
{
   char *hdr_start_ptr;
   short int start_idx,i;

   memcpy(&start_idx,payload,2);
   start_idx=ntohs(start_idx);

   hdr_start_ptr = &msg[start_idx];
   memcpy(&i,payload+HEADER_LEN_IDX,2);
   i=ntohs(i);

   fprintf(fd,"%sHEADER NAME:[%.*s]\n",prefix,payload[HEADER_NAME_LEN_IDX],hdr_start_ptr);
   fprintf(fd,"%sHEADER:[%.*s]\n",prefix,i-2,hdr_start_ptr);
   fprintf(fd,"%sHEADER CODE=",prefix);
   for(i=0;i<len;i++)
      fprintf(fd,"%s%d%s",i==0?"[":":",payload[i],i==len-1?"]\n":"");
   if(len==4)
      return 1;
   switch(type){
      case HDR_FROM_T:/*from*/
      case HDR_TO_T:/*to*/
      case HDR_REFER_TO_T:/*refer-to*/
      case HDR_RPID_T:/*rpid= remote parte id*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_to_body(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_CONTACT_T:/*contact*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_contact_body(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_ROUTE_T:/*route*/
      case HDR_RECORDROUTE_T:/*record-route*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_route_body(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_CONTENTLENGTH_T:/*contentlength*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_contentlength(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_VIA_T:/*via*/
      case HDR_VIA2_T:/*via*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_via_body(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_ACCEPT_T:/*accept*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_accept(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_CONTENTTYPE_T:/*content-type*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_content_type(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_CSEQ_T:/*CSeq*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_cseq(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_EXPIRES_T:/*expires*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_expires(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_ALLOW_T:/*allow*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_allow(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      case HDR_PROXYAUTH_T:/*proxy-authenticate*/
      case HDR_AUTHORIZATION_T:/*authorization*/
      /*case HDR_PROXYAUTHORIZATION_T:proxy-authorization*/
	 memcpy(&i,payload+HEADER_LEN_IDX,2);
	 i=ntohs(i);
	 print_encoded_digest(fd,hdr_start_ptr,i,&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,strcat(prefix,"  "));
	 prefix[strlen(prefix)-2]=0;
	 break;
      default:
	 return 1;
   }
   return 1;
}

int dump_headers_test(char *msg,int msglen,unsigned char *payload,int len,char type,FILE* fd,char segregationLevel)
{
   char *hdr_start_ptr;
   short int start_idx;

   memcpy(&start_idx,payload,2);
   start_idx=ntohs(start_idx);

   hdr_start_ptr = &msg[start_idx];

   switch(type){
      case 'f':/*from*/
      case 't':/*to*/
      case 'o':/*refer-to*/
      case 'p':/*rpid= remote parte id*/
	 dump_to_body_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd,segregationLevel);
	 break;
      case 'm':/*contact*/
	 dump_contact_body_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd,segregationLevel,"");
	 break;
      case 'r':/*route*/
      case 'R':/*record-route*/
	 dump_route_body_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd,segregationLevel,"");
	 break;
      case 'l':/*contentlength*/
	 dump_standard_hdr_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd);
	 break;
      case 'v':/*via*/
	 dump_via_body_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd,segregationLevel);
	 break;
      case 'A':/*Accept*/
	 dump_standard_hdr_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd);
	 break;
      case 'c':/*content-type*/
	 dump_standard_hdr_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd);
	 break;
      case 'S':/*CSeq*/
	 dump_standard_hdr_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd);
	 break;
      case 'P':/*expires*/
	 dump_standard_hdr_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd);
	 break;
      case 'a':/*allow*/
	 dump_standard_hdr_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd);
	 break;
      case 'h':/*proxy-authenticate*/
      case 'H':/*authorization*/
      case 'z':/*proxy-authorization*/
	 dump_standard_hdr_test(hdr_start_ptr,payload[HEADER_LEN_IDX],&payload[HEADER_PAYLOAD_IDX],
	       len-HEADER_PAYLOAD_IDX,fd);
	 break;
      default:
	 return 1;
   }
   return 1;
}

int dump_standard_hdr_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,FILE* fd)
{
   int i;
   i=htonl(hdrlen);
   fwrite(&i,1,4,fd);
   fwrite(hdr,1,hdrlen,fd);
   i=htonl(paylen);
   fwrite(&i,1,4,fd);
   fwrite(payload,1,paylen,fd);
   fwrite(&theSignal,1,4,fd);
   return 0;
}
