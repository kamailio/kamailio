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


#include <string.h>

#include "cluster.h"
#include "seas.h"
#include "../../dprint.h"
#include "../../mem/mem.h"

char *cluster_cfg;

#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
	(_p)++;}


/**
 * Parses the PING configuration string. Its format is 
 * "ping_period:pings_lost:ping_timeout"
 * ping_period : time between pings
 * pings_lost: number of lost pings before failure
 * ping_timeout: time to consider a ping failed
 *
 * returns 
 * 0 if no clusters present
 * -1 if config is malformed (unable to parse);
 *  1 if config is successfully set
 */
int parse_cluster_cfg(void)
{
   char *p,*start;
   int n,k;
   struct as_entry **entry,*tmp,*tmp2;

   if((p=cluster_cfg)==0 || *cluster_cfg==0){
      return 0;
   }
   entry=&as_list;

   while (*p)
   {
      eat_spaces(p);
      /*get cluster name*/
      start = p;
      while (*p!=' ' && *p!='\t' && *p!='[' && *p!=0)
	 p++;
      if ( p==start || *p==0 ){
	 LM_ERR("cluster names must only contain alphanumeric chars\n");
	 goto error;
      }
      if (!((*entry)=(struct as_entry*)shm_malloc(sizeof(struct as_entry)))) {
	 LM_ERR("Out of shm mem for as_entry\n");
	 goto error;
      }
      memset(*entry,0,sizeof(struct as_entry));
      if (!((*entry)->name.s=shm_malloc(p-start))) {
	 LM_ERR("Out of shm malloc for cluster name\n");
	 goto error;
      }
      memcpy((*entry)->name.s, start, p-start);
      (*entry)->name.len=p-start;
      (*entry)->connected=0;
      (*entry)->type=CLUSTER_TYPE;
      (*entry)->u.cs.name=(*entry)->name;
      /*get as names*/
      eat_spaces(p);
      if (*p!='['){
	 LM_ERR("Malformed cluster cfg string %s\n",cluster_cfg);
	 goto error;
      }
      p++;
      n=0;
      while (*p!=']')
      {
	 eat_spaces(p);
	 start = p;
	 while(*p!=' ' && *p!='\t' && *p!=']' && *p!=',' && *p!=0)
	    p++;
	 if ( p==start || *p==0 )
	    goto error;
	 if (!((*entry)->u.cs.as_names[n].s=shm_malloc(p-start))) {
	    LM_ERR("Out of shm_mem for AS name in cluster\n");
	    goto error;
	 }
	 (*entry)->u.cs.as_names[n].len=p-start;
	 memcpy((*entry)->u.cs.as_names[n].s,start,p-start);
	 n++;
	 if(n>=MAX_AS_PER_CLUSTER){
	    LM_ERR("too many AS per cluster\n");
	    goto error;
	 }
	 eat_spaces(p);
	 if (*p==',') {
	    p++;
	    eat_spaces(p);
	 }
      }
      p++;
      (*entry)->u.cs.num=n;
      /* end of element */
      eat_spaces(p);
      if (*p==',')
	 p++;
      eat_spaces(p);
      entry=&((*entry)->next);
   }
   for (tmp=as_list;tmp->next;tmp=tmp->next){
      LM_DBG("%.*s\n",tmp->name.len,tmp->name.s);
   }
   LM_DBG("%.*s\n",tmp->name.len,tmp->name.s);
   entry=&(tmp->next);
   for(tmp=as_list;tmp;tmp=tmp->next){
      if (tmp->type!=CLUSTER_TYPE) 
	 continue;
      LM_DBG("cluster:[%.*s]\n",tmp->name.len,tmp->name.s);
      for(k=0;k<tmp->u.cs.num;k++){
	 LM_DBG("\tAS:[%.*s]\n",tmp->u.cs.as_names[k].len,tmp->u.cs.as_names[k].s);
	 for (tmp2=as_list;tmp2;tmp2=tmp2->next) {
	    if (tmp2->type== AS_TYPE && tmp->u.cs.as_names[k].len == tmp2->name.len && 
		  !memcmp(tmp->u.cs.as_names[k].s,tmp2->name.s,tmp2->name.len)) {
	       tmp->u.cs.servers[k]=&tmp2->u.as;
	       break;
	    }
	 }
	 if(tmp2)
	    continue;
	 if (!((*entry)=shm_malloc(sizeof(struct as_entry)))) {
	    LM_ERR("Out of shm mem \n");
	    goto error;
	 }
	 memset(*entry,0,sizeof(struct as_entry));
	 (*entry)->type=AS_TYPE;
	 if (!((*entry)->name.s=shm_malloc(tmp->u.cs.as_names[k].len))) {
	    LM_ERR("out of shm mem\n");
	    goto error;
	 }
	 memcpy((*entry)->name.s,tmp->u.cs.as_names[k].s,tmp->u.cs.as_names[k].len);
	 (*entry)->name.len=tmp->u.cs.as_names[k].len;
	 (*entry)->u.as.name=(*entry)->name;
	 tmp->u.cs.servers[k]=&(*entry)->u.as;
	 entry=&((*entry)->next);
      }
   }
   for(tmp=as_list;tmp;tmp=tmp->next){
      LM_DBG("%.*s %s",tmp->name.len,tmp->name.s,tmp->next?"":"\n");
   }
   return 1;
error:
   tmp=as_list;
   while(tmp){
      for(k=0;k<tmp->u.cs.num;k++){
	 if(tmp->u.cs.as_names[k].s)
	    shm_free(tmp->u.cs.as_names[k].s);
      }
      if(tmp->name.s)
	 shm_free(tmp->name.s);
      tmp2=tmp;
      tmp=tmp->next;
      shm_free(tmp2);
   }
   as_list=(struct as_entry *)0;
   return -1;
}
