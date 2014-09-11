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
 *        Filename:  encode_digest.c
 * 
 *     Description:  functions to encode/decode/print Digest headers ([proxy,www]-[authenticate,require])
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

#include "../../parser/digest/digest_parser.h"
#include "../../parser/parse_uri.h"
#include "../../parser/digest/digest.h"
#include "encode_digest.h"
#include "xaddress.h"
#include "encode_header.h"
#include "encode_uri.h"

#define HAS_NAME_F		0x01
#define HAS_REALM_F		0x02
#define HAS_NONCE_F		0x04
#define HAS_URI_F		0x08
#define HAS_RESPONSE_F		0x10
#define HAS_ALG_F		0x20
#define HAS_CNONCE_F		0x40
#define HAS_OPAQUE_F		0x80
#define HAS_QoP_F		0x01
#define HAS_NC_F		0x02
/*
 * encodes a digest header body.
 * encoding is:
 * 1: flags
 * HAS_NAME_F		0x01
 * HAS_REALM_F		0x02
 * HAS_NONCE_F		0x04
 * HAS_URI_F		0x08
 * HAS_RESPONSE_F	0x10
 * HAS_ALG_F		0x20
 * HAS_CNONCE_F		0x40
 * HAS_OPAQUE_F		0x80
 * 1: flags
 * HAS_QoP_F		0x01
 * HAS_NC_F		0x02
 * 2: hdr-start based pointer to where the scheme starts + length of the scheme (must be Digest).
 * 
 * for each field present, there are 2 bytes, one pointing the place where it starts, 
 * the next signaling how long this field is. The URI is a special case, and is composed of 1
 * byte telling how long is the URI structure, and then the encoded URI structure.
 */

int encode_digest(char *hdrstart,int hdrlen,dig_cred_t *digest,unsigned char *where)
{
   int i=2,j=0;/* 2*flags */
   unsigned char flags1=0,flags2=0;
   struct sip_uri sipuri;

   if(digest->username.whole.s && digest->username.whole.len){
      flags1|=HAS_NAME_F;
      where[i++]=(unsigned char)(digest->username.whole.s-hdrstart);
      where[i++]=(unsigned char)digest->username.whole.len;
   }
   if(digest->realm.s && digest->realm.len){
      flags1|=HAS_REALM_F;
      where[i++]=(unsigned char)(digest->realm.s-hdrstart);
      where[i++]=(unsigned char)digest->realm.len;
   }
   if(digest->nonce.s && digest->nonce.len){
      flags1|=HAS_NONCE_F;
      where[i++]=(unsigned char)(digest->nonce.s-hdrstart);
      where[i++]=(unsigned char)digest->nonce.len;
   }
   if(digest->uri.s && digest->uri.len){
      memset(&sipuri,0,sizeof(struct sip_uri));
      flags1|=HAS_URI_F;
      if (parse_uri(digest->uri.s, digest->uri.len,&sipuri) < 0 ) {
	 LM_ERR("Bad URI in address\n");
	 return -1;
      }else{
	 if((j=encode_uri2(hdrstart,hdrlen,digest->uri,&sipuri,&where[i+1]))<0){
	    LM_ERR("Error encoding the URI\n");
	    return -1;
	 }else{
	    where[i]=(unsigned char)j;
	    i+=(j+1);
	 }
      }
   }
   if(digest->response.s && digest->response.len){
      flags1|=HAS_RESPONSE_F;
      where[i++]=(unsigned char)(digest->response.s-hdrstart);
      where[i++]=(unsigned char)digest->response.len;
   }
   if(digest->alg.alg_str.s && digest->alg.alg_str.len){
      flags1|=HAS_ALG_F;
      where[i++]=(unsigned char)(digest->alg.alg_str.s-hdrstart);
      where[i++]=(unsigned char)digest->alg.alg_str.len;
   }
   if(digest->cnonce.s && digest->cnonce.len){
      flags1|=HAS_CNONCE_F;
      where[i++]=(unsigned char)(digest->cnonce.s-hdrstart);
      where[i++]=(unsigned char)digest->cnonce.len;
   }
   if(digest->opaque.s && digest->opaque.len){
      flags1|=HAS_OPAQUE_F;
      where[i++]=(unsigned char)(digest->opaque.s-hdrstart);
      where[i++]=(unsigned char)digest->opaque.len;
   }
   if(digest->qop.qop_str.s && digest->qop.qop_str.len){
      flags2|=HAS_QoP_F;
      where[i++]=(unsigned char)(digest->qop.qop_str.s-hdrstart);
      where[i++]=(unsigned char)digest->qop.qop_str.len;
   }
   if(digest->nc.s && digest->nc.len){
      flags2|=HAS_NC_F;
      where[i++]=(unsigned char)(digest->nc.s-hdrstart);
      where[i++]=(unsigned char)digest->nc.len;
   }
   where[0]=flags1;
   where[1]=flags2;
   return i;
}


int print_encoded_digest(FILE *fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i=2;/* flags + flags1 */
   unsigned char flags1,flags2;

   flags1=payload[0];
   flags2=payload[1];
   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"ENCODED DIGEST=[":":",payload[i],i==paylen-1?"]\n":"");
   i=2;
   if(flags1 & HAS_NAME_F){
      fprintf(fd,"%sDIGEST NAME=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags1& HAS_REALM_F){
      fprintf(fd,"%sDIGEST REALM=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags1& HAS_NONCE_F){
      fprintf(fd,"%sDIGEST NONCE=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags1& HAS_URI_F){
      if(print_encoded_uri(fd,&payload[i+1],payload[i],hdr,hdrlen,strcat(prefix,"  "))<0){
	 prefix[strlen(prefix)-2]=0;
	 fprintf(fd,"Error parsing encoded URI\n");
	 return -1;
      }
      i+=payload[i]+1;
   }
   if(flags1& HAS_RESPONSE_F){
      fprintf(fd,"%sDIGEST RESPONSE=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags1& HAS_ALG_F){
      fprintf(fd,"%sDIGEST ALGORITHM=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags1& HAS_CNONCE_F){
      fprintf(fd,"%sDIGEST CNONCE=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags1& HAS_OPAQUE_F){
      fprintf(fd,"%sDIGEST OPAQUE=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags2& HAS_QoP_F){
      fprintf(fd,"%sDIGEST QualityOfProtection=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   if(flags2& HAS_NC_F){
      fprintf(fd,"%sDIGEST NonceCount=[%.*s]\n",prefix,payload[i+1],&hdr[payload[i]]);
      i+=2;
   }
   return 0;
}


/**
 *
 */
int dump_digest_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,FILE* fd,char segregationLevel)
{
   int i=2;/* 2*flags */
   unsigned char flags1=0;

   flags1=payload[0];
   if(!(segregationLevel & ONLY_URIS))
      return dump_standard_hdr_test(hdr,hdrlen,payload,paylen,fd);
   i=2;
   if(flags1 & HAS_NAME_F)
      i+=2;
   if(flags1 & HAS_REALM_F)
      i+=2;
   if(flags1 & HAS_NONCE_F)
      i+=2;
   if(flags1 & HAS_URI_F){
      if(!(segregationLevel & JUNIT) && (segregationLevel & ONLY_URIS))
	 return dump_standard_hdr_test(hdr,hdrlen,&payload[i+1],payload[i],fd);
      if((segregationLevel & JUNIT) && (segregationLevel & ONLY_URIS))
	 return print_uri_junit_tests(hdr,hdrlen,&payload[i+1],payload[i],fd,1,"");
   }
   return 0;
}
