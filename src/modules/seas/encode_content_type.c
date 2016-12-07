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
 *        Filename:  encode_content_type.c
 * 
 *     Description:  [en|de]code content type
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
#include <netinet/in.h>
#include <string.h>
#include "../../parser/parse_content.h"
#include "encode_content_type.h"

int encode_content_type(char *hdrstart,int hdrlen,unsigned int bodi,char *where)
{
   return encode_mime_type(hdrstart,hdrlen,bodi,where);
}

int print_encoded_content_type(FILE* fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   unsigned int type;
   memcpy(&type,payload,sizeof(unsigned int));
   return print_encoded_mime_type(fd,hdr,hdrlen,&type,paylen,prefix);
}

int encode_accept(char *hdrstart,int hdrlen,unsigned int *bodi,char *where)
{
   int i;

   for(i=0;bodi[i]!=0;i++){
      encode_mime_type(hdrstart,hdrlen,bodi[i],&where[1+i*sizeof(unsigned int)]);
   }
   where[0]=(unsigned char)i;

   return 1+i*sizeof(unsigned int);
}

int print_encoded_accept(FILE* fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i;
   unsigned int type;
   for(i=0;i<payload[0];i++){
      memcpy(&type,&payload[1+i*sizeof(unsigned int)],sizeof(unsigned int));
      print_encoded_mime_type(fd,hdr,hdrlen,&type,4,prefix);
   }
   return 1;

}

int encode_mime_type(char *hdrstart,int hdrlen,unsigned int bodi,char *where)
{
   unsigned int type;

   type=htonl(bodi);
   memcpy(where,&type,sizeof(unsigned int));

   return sizeof(int);
}

int print_encoded_mime_type(FILE *fd,char *hdr,int hdrlen,unsigned int* payload,int paylen,char *prefix)
{
   unsigned int type;
   char *chtype,*chsubtype;

   type=ntohl(*payload);

   switch(type>>16){
      case TYPE_TEXT:
	 chtype="text";
	 break;
      case TYPE_MESSAGE:
	 chtype="message";
	 break;
      case TYPE_APPLICATION:
	 chtype="application";
	 break;
      case TYPE_MULTIPART:
	 chtype="multipart";
	 break;
      case TYPE_ALL:
	 chtype="all";
	 break;
      case TYPE_UNKNOWN:
	 chtype="unknown";
	 break;
      default:
	chtype="(didn't know this type existed)";
	break;
   }

   switch(type&0xFF){
      case SUBTYPE_PLAIN:
	 chsubtype="SUBTYPE_PLAIN";
	 break;
      case SUBTYPE_CPIM:
	 chsubtype="SUBTYPE_CPIM";
	 break;
      case SUBTYPE_SDP:
	 chsubtype="SUBTYPE_SDP";
	 break;
      case SUBTYPE_CPLXML:
	 chsubtype="SUBTYPE_CPLXML";
	 break;
      case SUBTYPE_PIDFXML:
	 chsubtype="SUBTYPE_PIDFXML";
	 break;
      case SUBTYPE_RLMIXML:
	 chsubtype="SUBTYPE_RLMIXML";
	 break;
      case SUBTYPE_RELATED:
	 chsubtype="SUBTYPE_RELATED";
	 break;
      case SUBTYPE_LPIDFXML:
	 chsubtype="SUBTYPE_LPIDFXML";
	 break;
      case SUBTYPE_XPIDFXML:
	 chsubtype="SUBTYPE_XPIDFXML";
	 break;
      case SUBTYPE_WATCHERINFOXML:
	 chsubtype="SUBTYPE_WATCHERINFOXML";
	 break;
      case SUBTYPE_EXTERNAL_BODY:
	 chsubtype="SUBTYPE_EXTERNAL_BODY";
	 break;
      case SUBTYPE_XML_MSRTC_PIDF:
	 chsubtype="SUBTYPE_XML_MSRTC_PIDF";
	 break;
      case SUBTYPE_ALL:
	 chsubtype="SUBTYPE_ALL";
	 break;
      case SUBTYPE_UNKNOWN:
	 chsubtype="SUBTYPE_UNKNOWN";
	 break;
      default:
	 chsubtype="(didnt know this subtype existed)";
   }

   fprintf(fd,"%sTYPE:[%s]\n",prefix,chtype);
   fprintf(fd,"%sSUBTYPE:[%s]\n",prefix,chsubtype);
   return 0;
}
