/* $Id$ */
/*
 * =====================================================================================
 * 
 *        Filename:  encode_allow.c
 * 
 *     Description:  [en|de]code allow header
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
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "../../parser/parse_allow.h"
#include "../../parser/msg_parser.h"
#include "encode_allow.h"

char *mismetodos[]={"UNDEF","INVITE","CANCEL","ACK","BYE","INFO","OPTIONS","UPDATE","REGISTER","MESSAGE","SUBSCRIBE","NOTIFY","PRACK","REFER","OTHER"};

/**
 * Encodes allow header.
 *
 * TODO: Does not support the UNDEFINED header type !!!
 */
int encode_allow(char *hdrstart,int hdrlen,unsigned int *bodi,char *where)
{
   unsigned int i;
   memcpy(&i,bodi,4);
   i=htonl(i);
   memcpy(where,&i,4);
   return 4;
}

int print_encoded_allow(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   unsigned int i,j=0,body;

   memcpy(&body,payload,4);
   body=ntohl(body);
   dprintf(fd,"%sMETHODS=",prefix);
   if(body==0)
      dprintf(fd,"UNKNOWN");
   for(i=0;i<32;j=(0x01<<i),i++){
      if(body & (j<15))
	 dprintf(fd,",%s",mismetodos[i]);
   }
   dprintf(fd,"\n");
   return 1;
}
