/* $Id$ */
/*
 * =====================================================================================
 * 
 *        Filename:  encode_content_length.c
 * 
 *     Description:  Function to encode content-length headers.
 * 
 *         Version:  1.0
 *         Created:  21/11/05 02:02:58 CET
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

/*
 * Encodes a content-length header.
 * encoding is as follows:
 * 1: length of the payload.
 * N: Network-Byte-Ordered(little endian) of the
 * multibyte number represeting the length (now, it is
 * a long integer)
 */
int encode_contentlength(char *hdr,int hdrlen,long int len,char *where)
{
   long int longint;

   longint = htonl(len);
   where[0]=sizeof(long int);
   memcpy(&where[1],&longint,sizeof(long int));
   return 1+sizeof(long int);

}

int print_encoded_contentlength(int fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix)
{
   long int content_length;
   int i;

   memcpy(&content_length,&payload[1],payload[0]);
   content_length=ntohl(content_length);

   dprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      dprintf(fd,"%s%d%s",i==0?"ENCODED CONTENT LENGTH BODY:[":":",payload[i],i==paylen-1?"]\n":"");
   dprintf(fd,"%s  CONTENT LENGTH=[%d]\n",prefix,(int)content_length);
   return 1;
}

