/* $Id$ */
/*
 * =====================================================================================
 * 
 *        Filename:  encode_expires.c
 * 
 *     Description:  functions to [en|de]code expires header
 * 
 *         Version:  1.0
 *         Created:  22/11/05 00:05:54 CET
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
#include "../../parser/parse_expires.h"

/*
 * Encodes expires headers (content = delta-seconds)
 * 4: network-byte-order value
 * 2: hdr-based pointer to begin of value string + value string length
 */
int encode_expires(char *hdrstart,int hdrlen,exp_body_t *body,unsigned char *where)
{
   int i;

   i=htonl(body->val);
   memcpy(where,&i,4);
   where[4]=(unsigned char)(body->text.s-hdrstart);
   where[5]=(unsigned char)(body->text.len);
   return 6;
}

int print_encoded_expires(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i;
   memcpy(&i,payload,4);
   i=ntohl(i);
   dprintf(fd,"%sEXPIRES VALUE=%d==%.*s\n",prefix,i,payload[5],&hdr[payload[4]]);
   return 1;
}


