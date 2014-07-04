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
 *        Filename:  utils.c
 * 
 *     Description:  
 * 
 *         Version:  1.0
 *         Created:  19/01/06 15:50:33 CET
 *        Revision:  none
 *        Compiler:  gcc
 * 
 *          Author:  Elias Baixas (EB), elias@conillera.net
 *         Company:  VozTele.com
 * 
 * =====================================================================================
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <syslog.h>
#include <string.h>

#include "../../parser/msg_parser.h"
#include "../../parser/parse_via.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "encode_header.h"
#include "encode_msg.h"
#include "utils.h"

#define MAX_ERROR 32

static inline int memstr(char *haystack,int hlen,char *needle,int nlen);

int buffered_printer(FILE* infd)
{
   int i,k=0,retval;
   char *missatge=0,*myerror="";
   struct sip_msg msg;
   static char mybuffer[1400];
   static int end=0,last=0;

   while((i=fread(&mybuffer[last],1,1400-last,infd))==1400-last){
      if((end=memstr(mybuffer,last+i,"\n\n\n",3))<0){
	 last+=i;
	 return 0;
      }else{
	 end+=3;
	 while(end<1400 && (mybuffer[end]=='\n' || mybuffer[end]=='.' || mybuffer[end]=='\r'))
	    end++;
	 if((missatge=pkg_malloc(end))==0){
	    myerror="Out of memory !!\n";
	    goto error;
	 }
	 memset(missatge,0,end);
	 memcpy(missatge,mybuffer,end);
	 memset(&msg,0,sizeof(struct sip_msg));
	 msg.buf=missatge;
	 msg.len=end;
	 if(!parse_msg(msg.buf,msg.len,&msg))
	    print_msg_info(stdout,&msg);
	 printf("PARSED:%d,last=%d,end=%d\n",k++,last,end);
	 free_sip_msg(&msg);
	 pkg_free(missatge);
	 memmove(mybuffer,&mybuffer[end],1400-end);
	 last=1400-end;
      }
   }
   retval=0;
   goto exit;
error:
   printf("Error on %s",myerror);
   retval=1;
exit:
   if(missatge)
      pkg_free(missatge);
   return retval;
}

int coded_buffered_printer(FILE* infd)
{
   int i,lastlast;
   char spaces[50];
   static char mybuffer[1500];
   static int size=0,last=0;

   memcpy(spaces," ",2);

   do{
      lastlast=1500-last;
      i=fread(&mybuffer[last],1,lastlast,infd);
      printf("read i=%d\n",i);
      if(i==0)
	 break;
      if(size==0){
	 size=GET_PAY_SIZE(mybuffer);
	 printf("size=%d\n",size);
	 last+=i;
      }
      if(last>=size){
	 printf("should print message: last=%d, size=%d\n",last,size);
	 if(print_encoded_msg(stdout,mybuffer,spaces)<0){
	    printf("Unable to print encoded msg\n");
	    return -1;
	 }
	 if(last>size){
	    memmove(mybuffer,&mybuffer[size],last-size);
	    last=last-size;
	 }else
	    last=0;
	 size=0;
      }
   }while(i>0 && i==lastlast);

   if(i==0)
      return 0;
   else
      return 1;
}

int print_msg_info(FILE* fd,struct sip_msg* msg)
{
   char *payload=0;
   char *prefix=0;
   int retval=-1;
   if((prefix=pkg_malloc(500))==0){
      printf("OUT OF MEMORY !!!\n");
      return -1;
   }
   memset(prefix,0,500);
   strcpy(prefix,"  ");

   if(parse_headers(msg,HDR_EOH_F,0)<0)
      goto error;
   if(!(payload=pkg_malloc(MAX_ENCODED_MSG + MAX_MESSAGE_LEN)))
      goto error;
   if(encode_msg(msg,payload,MAX_ENCODED_MSG + MAX_MESSAGE_LEN)<0){
      printf("Unable to encode msg\n");
      goto error;
   }
   if(print_encoded_msg(fd,payload,prefix)<0){
      printf("Unable to print encoded msg\n");
      pkg_free(payload);
      goto error;
   }
   pkg_free(payload);
   retval =0;
error:
   if(prefix)
      pkg_free(prefix);
   return retval;
}

static inline int memstr(char *haystack,int hlen,char *needle,int nlen)
{
   int i=0;

   if(nlen>hlen)
      return -1;
   while(i<=(hlen-nlen) && (haystack[i]!=needle[0] || memcmp(&haystack[i],needle,nlen)))
      i++;
   if(i>(hlen-nlen))
      return -1;
   else
      return i;
}
