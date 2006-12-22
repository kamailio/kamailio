/* $Id$ */
/*
 * =====================================================================================
 * 
 *        Filename:  encode_content_disposition.c
 * 
 *     Description:  [en|de]encodes content disposition
 * 
 *         Version:  1.0
 *         Created:  21/11/05 20:36:19 CET
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
#include "../../parser/parse_disposition.h"
#include "encode_parameters.h"

/*
struct disposition_param { str name; str body; int is_quoted; struct disposition_param *next; };
struct disposition { str type; struct disposition_param *params; };
*/

int encode_content_disposition(char *hdrstart,int hdrlen,struct disposition *body,unsigned char *where)
{
   unsigned char i=3;

   /*where[0] reserved flags for future use*/
   where[1]=(unsigned char)(body->type.s-hdrstart);
   where[2]=(unsigned char)body->type.len;
   i+=encode_parameters(&where[3],(void *)body->params,hdrstart,body,'d');
   return i;
}

int print_encoded_content_disposition(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i=3;/* flags + urilength */
   unsigned char flags=0;

   flags=payload[0];
   printf("%s",prefix);
   for(i=0;i<paylen;i++)
      printf("%s%d%s",i==0?"ENCODED CONTENT-DISPOSITION=[":":",payload[i],i==paylen-1?"]\n":"");
   printf("%sCONTENT DISPOSITION:[%.*s]\n",prefix,payload[2],&hdr[payload[1]]);
   print_encoded_parameters(fd,&payload[3],hdr,paylen-3,prefix);
   return 0;
}


