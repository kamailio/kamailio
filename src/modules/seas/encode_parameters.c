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
 *        Filename:  encode_parameters.c
 * 
 *     Description:  Functions to encode/print parameters
 * 
 *         Version:  1.0
 *         Created:  25/01/06 17:46:04 CET
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
#include "../../str.h"
#include "../../parser/parse_param.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_via.h"
#include "../../parser/parse_disposition.h"
#include "encode_parameters.h"

#define REL_PTR(a,b) (unsigned char)((b)-(a))

/**
 * Returns how many bytes in *where have been used
 *
 * TODO this is little shitty, someone should unify all the param flavours 
 * to a sigle universal type of parameter (to_param,param,disposition_param)
 * the way is done here, at least, we dont have the parameter-hanling code spread all around.
 */
int encode_parameters(unsigned char *where,void *pars,char *hdrstart,void *_body,char to)
{
   struct param *parametro,*params;
   struct to_param *toparam,*toparams;
   struct disposition_param *dparam,*dparams;
   struct via_param *vparam,*vparams;
   struct via_body *vbody;
   struct to_body *tbody;
   char *mylittlepointer,*paramstart; 
   int i,j,paramlen;
   i=0;
   if(!pars)
      return 0;
   if(to=='t'){
      toparams=(struct to_param*)pars;
      tbody=(struct to_body*)_body;
      for(toparam=toparams;toparam;toparam=toparam->next){
	 where[i++]=(unsigned char)(toparam->name.s-hdrstart);
	 if(toparam->value.s)
	    mylittlepointer=toparam->value.s;
	 else
	    if(toparam->next)
	       mylittlepointer=toparam->next->name.s;
	    else
	       mylittlepointer=toparam->name.s+toparam->name.len+1;
         if(mylittlepointer[-1]=='\"')/*check if the parameter was quoted*/
            mylittlepointer--;/*if so, account for quotes*/
	 where[i++]=(unsigned char)(mylittlepointer-hdrstart);
      }
      if((toparam=tbody->last_param)){
	 if(toparam->value.s)
	    mylittlepointer=toparam->value.s+toparam->value.len;
	 else
	    mylittlepointer=toparam->name.s+toparam->name.len;
         if(mylittlepointer[0]=='\"')/*check if the parameter was quoted*/
            mylittlepointer++;/*if so, account for quotes*/
	 where[i++]=(unsigned char)(mylittlepointer-hdrstart+1);
      }
      return i;
   }else if(to=='n'){
      params=(struct param*)pars;
      for(parametro=reverseParameters(params);parametro;parametro=parametro->next){
	 where[i++]=(unsigned char)(parametro->name.s-hdrstart);
	 if(parametro->body.s)
	    mylittlepointer=parametro->body.s;
	 else
	    if(parametro->next)
	       mylittlepointer=parametro->next->name.s;
	    else
	       mylittlepointer=parametro->name.s+parametro->name.len+1;
         if(mylittlepointer[-1]=='\"')/*check if the parameter was quoted*/
            mylittlepointer--;/*if so, account for quotes*/
	 where[i++]=(unsigned char)(mylittlepointer-hdrstart);
      }
      /*look for the last parameter*/
      /*WARNING the ** parameters are in reversed order !!! */
      /*TODO parameter encoding logic should be moved to a specific function...*/
      for(parametro=params;parametro && parametro->next;parametro=parametro->next);
      /*printf("PARAMETRO:%.*s\n",parametro->name.len,parametro->name.s);*/
      if(parametro){
	 if(parametro->body.s)
	    mylittlepointer=parametro->body.s+parametro->body.len;
	 else
	    mylittlepointer=parametro->name.s+parametro->name.len;
         if(mylittlepointer[0]=='\"')/*check if the parameter was quoted*/
            mylittlepointer++;/*if so, account for quotes*/
	 where[i++]=(unsigned char)(mylittlepointer-hdrstart+1);
      }
      return i;
   }else if(to=='d'){
      dparams=(struct disposition_param*)pars;
      for(dparam=dparams;dparam;dparam=dparam->next){
	 where[i++]=(unsigned char)(dparam->name.s-hdrstart);
	 if(dparam->body.s)
	    mylittlepointer=dparam->body.s;
	 else
	    if(dparam->next)
	       mylittlepointer=dparam->next->name.s;
	    else
	       mylittlepointer=dparam->name.s+dparam->name.len+1;
         if(mylittlepointer[-1]=='\"')/*check if the parameter was quoted*/
            mylittlepointer--;/*if so, account for quotes*/
	 where[i++]=(unsigned char)(mylittlepointer-hdrstart);
      }
      /*WARNING the ** parameters are in reversed order !!! */
      /*TODO parameter encoding logic should be moved to a specific function...*/
      for(dparam=dparams;dparam && dparam->next;dparam=dparam->next);
      if(dparam){
	 if(dparam->body.s)
	    mylittlepointer=dparam->body.s+dparam->body.len;
	 else
	    mylittlepointer=dparam->name.s+dparam->name.len;
         if(mylittlepointer[0]=='\"')/*check if the parameter was quoted*/
            mylittlepointer++;/*if so, account for quotes*/
	 where[i++]=(unsigned char)(mylittlepointer-hdrstart+1);
      }
      return i;
   }else if(to=='v'){
      vparams=(struct via_param*)pars;
      vbody=(struct via_body*)_body;
      for(vparam=vparams;vparam;vparam=vparam->next){
	 where[i++]=REL_PTR(hdrstart,vparam->name.s);
	 if(vparam->value.s)
	    mylittlepointer=vparam->value.s;
	 else
	    if(vparam->next)
	       mylittlepointer=vparam->next->name.s;
	    else
	       mylittlepointer=vparam->name.s+vparam->name.len+1;
         if(mylittlepointer[-1]=='\"')/*check if the parameter was quoted*/
            mylittlepointer--;/*if so, account for quotes*/
	 where[i++]=REL_PTR(hdrstart,mylittlepointer);
      }
      if((vparam=vbody->last_param)){
	 if(vparam->value.s)
	    mylittlepointer=vparam->value.s+vparam->value.len;
	 else
	    mylittlepointer=vparam->name.s+vparam->name.len;
         if(mylittlepointer[0]=='\"')/*check if the parameter was quoted*/
            mylittlepointer++;/*if so, account for quotes*/
	 where[i++]=REL_PTR(hdrstart,mylittlepointer+1);
      }
      return i;
   }else if(to=='u'){
      paramlen=*((int*)_body);
      paramstart=(char *)pars;
      j=i=0;
      if(paramstart==0 || paramlen==0)
	 return 0;
      /*the first parameter start index, I suppose paramstart points to the first
       letter of the first parameter: sip:elias@voztele.com;param1=true;param2=false
                                     paramstart points to __^                      
       each parameter is codified with its {param_name_start_idx,[param_value_start_idx|next_param_start_idx]} */
      where[j++]=paramstart-hdrstart;
      while(i<paramlen){
	 i++;
	 if(paramstart[i]==';'){/*no '=' found !*/
	    where[j++]=(unsigned char)(&paramstart[i+1]-hdrstart);
	    where[j++]=(unsigned char)(&paramstart[i+1]-hdrstart);
	 }
	 if(paramstart[i]=='='){/* '=' found, look for the next ';' and let 'i' pointing to it*/
	    where[j++]=(unsigned char)(&paramstart[i+1]-hdrstart);
	    for(;i<paramlen&&paramstart[i]!=';';i++);
	    if(paramstart[i]==';')
	       where[j++]=(unsigned char)(&paramstart[i+1]-hdrstart);
	 }
      }
      where[j++]=(unsigned char)(&paramstart[i+1]-hdrstart);
      if(j%2 == 0)/*this is because maybe the LAST parameter doesn't have an '='*/
	 where[j++]=(unsigned char)(&paramstart[i+1]-hdrstart);
      return j;
   }
   return 0;
}

/**reverses a param_t linked-list
 */
param_t *reverseParameters(param_t *p)
{
   param_t *previous=NULL,*tmp;
   while (p != NULL)
   {
      tmp = p->next;/* store the next */
      p->next = previous;/* next = previous */
      previous = p;/* previous = current */
      p = tmp;/* current = "the next" */
   }
   return previous;
}

/*prints the encoded parameters
 */
int print_encoded_parameters(FILE *fd,unsigned char *payload,char *hdr,int paylen,char *prefix)
{
   int i;
   for(i=0;i<paylen-1;i+=2){
      fprintf(fd,"%s[PARAMETER[%.*s]",prefix,payload[i+1]-payload[i]-1,&hdr[payload[i]]);
      fprintf(fd,"VALUE[%.*s]]\n",(payload[i+2]-payload[i+1])==0?0:(payload[i+2]-payload[i+1]-1),&hdr[payload[i+1]]);
   }
   return 0;
}
