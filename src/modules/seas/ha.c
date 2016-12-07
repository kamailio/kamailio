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


#include <stdlib.h>/*atoi*/
#include <time.h>/*gettimeofday*/
#include <poll.h>/*poll*/
#include "ha.h"
#include "seas.h"
#include "../../mem/mem.h" /*pkg_malloc*/
#include "../../mem/shm_mem.h" /*shm_malloc*/

/** if any of these global ping vars is set to 0, then
 * this kind of ping is DISABLED
 */
char *jain_ping_config=0;
int jain_ping_period=0;
int jain_pings_lost=0;
int jain_ping_timeout=0;

char *servlet_ping_config=0;
int servlet_ping_period=0;
int servlet_pings_lost=0;
int servlet_ping_timeout=0;

int use_ha=0;
pid_t pinger_pid;

static inline int parse_ping(char * string,int *ping_period,int *pings_lost,int *ping_timeout);
static inline int send_ping(struct as_entry *the_as,struct timeval *now);
/**
 * returns:
 * 0 if no High Availability
 * 1 if High Availability
 * -1 if config error
 */
int prepare_ha(void)
{
   use_ha=0;
   if(!(jain_ping_config || servlet_ping_config)){
      jain_pings_lost=servlet_pings_lost=0;
      return 0;
   }
   if(parse_ping(jain_ping_config,&jain_ping_period,&jain_pings_lost,&jain_ping_timeout)<0)
      goto error;
   if(parse_ping(servlet_ping_config,&servlet_ping_period,&servlet_pings_lost,&servlet_ping_timeout)<0) 
      goto error;
   LM_DBG("jain: pinging period :%d max pings lost:%d ping timeout:%d\n",
		   jain_ping_period,jain_pings_lost,jain_ping_timeout);
   LM_DBG("servlet: pinging period:%d max pings lost:%d ping timeout:%d\n",
		   servlet_ping_period,servlet_pings_lost,servlet_ping_timeout);
   use_ha=1;
   return 1;
error:
   return -1;
}

int print_pingtable(struct ha *ta,int idx,int lock)
{
   int i;
   if(lock)
      lock_get(ta->mutex);
   for(i=0;i<ta->size;i++){
      if((ta->begin+ta->count)>ta->size){
	 if ((i<ta->begin && i<((ta->begin+ta->count)%ta->size)) || (i>=ta->begin && i<(ta->begin+ta->count)))
	    fprintf(stderr,"*");
	 else
	    fprintf(stderr,"=");
      }else{
	 if (i>=ta->begin && i<(ta->begin+ta->count))
	    fprintf(stderr,"*");
	 else
	    fprintf(stderr,"=");
      }
   }
   if(lock)
      lock_release(ta->mutex);
   fprintf(stderr,"\n");
   for(i=0;i<ta->size;i++)
      if(i==idx)
	 fprintf(stderr,"-");
      else
	 fprintf(stderr,"%d",i);
   fprintf(stderr,"\n");
   return 0;
}

/**
 * Parses the PING configuration string. Its format is 
 * "ping_period:pings_lost:ping_timeout"
 * ping_period : time between pings
 * pings_lost: number of lost pings before failure
 * ping_timeout: time to consider a ping failed
 *
 * returns 
 * 0 if config is not set
 * -1 if config is malformed (unable to parse);
 *  1 if config is successfully set
 */
static inline int parse_ping(char * string,int *ping_period,int *pings_lost,int *ping_timeout)
{
   char *ping_period_s,*pings_lost_s,*ping_timeout_s;
   ping_period_s=pings_lost_s=ping_timeout_s=(char *)0;

   if(string==0 || *string==0){
      *ping_period=0;
      *pings_lost=0;
      *ping_timeout=0;
      return 0;
   }

   if (((*string)<'0')||((*string)>'9')) {
      LM_ERR("malformed ping config string. Unparseable :[%s]\n",string);
      return -1;
   }
   ping_period_s=string;
   while(*string){
      if(*string == ':'){
	 *string=0;
	 if (!pings_lost_s && (*(string+1))) {
	    pings_lost_s=string+1;
	 }else if (!ping_timeout_s && (*(string+1))) {
	    ping_timeout_s=string+1;
	 }else{
	    LM_ERR("malformed ping config string. Unparseable :[%s]\n",string);
	    return -1;
	 }
      }
      string++;
   }
   if (!(ping_period_s && pings_lost_s && ping_timeout_s)) {
      LM_ERR("malformed ping config string. Unparseable :[%s]\n",string);
      return -1;
   }
   *ping_period =atoi(ping_period_s);
   *pings_lost=atoi(pings_lost_s);
   *ping_timeout=atoi(ping_timeout_s);
   if (*ping_period<=0 || *pings_lost<=0 || *ping_timeout<=0) {
      return -1;
   }
   return 1;
}

/**
 * we spawn a pinger process.
 *
 * some day we could spawn a pinger-thread instead of a process..
 *
 * returns:
 * 	-1 on error;
 */
int spawn_pinger(void)
{
   int n,next_jain,next_servlet,timeout;
   struct timeval now,last_jain,last_servlet;
   struct as_entry *as;

   if ((pinger_pid=fork())<0) {
      LM_ERR("forking failed!\n");
      goto error;
   }else if(pinger_pid>0){
      return 0;
   }
   strcpy(whoami,"Pinger Process\n");
   is_dispatcher=0;
   my_as=0;
   /* child */
   if(jain_ping_period && servlet_ping_period){
      next_jain=next_servlet=0;
   }else if(jain_ping_period){
      next_servlet=INT_MAX;
      next_jain=0;
   }else if(servlet_ping_period){
      next_jain=INT_MAX;
      next_servlet=0;
   }else{
      next_jain=next_servlet=INT_MAX;
   }

   gettimeofday(&last_jain,NULL);
   memcpy(&last_servlet,&last_jain,sizeof(struct timeval));

   while(1){
      gettimeofday(&now,NULL);
      if(next_jain!=INT_MAX){
	 next_jain=jain_ping_period-((now.tv_sec-last_jain.tv_sec)*1000 + (now.tv_usec-last_jain.tv_usec)/1000);
      }
      if(next_servlet!=INT_MAX){
	 next_servlet=servlet_ping_period-((now.tv_sec-last_servlet.tv_sec)*1000 + (now.tv_usec-last_servlet.tv_usec)/1000);
      }

      timeout=next_jain<next_servlet?next_jain:next_servlet;

      if ((n=poll(NULL,0,timeout<0?0:timeout))<0) {
	 LM_ERR("poll returned %d\n",n);
	 goto error;
      }else if(n==0){/*timeout*/
	 gettimeofday(&now,NULL);
	 if (jain_ping_period && ((now.tv_sec-last_jain.tv_sec)*1000 + (now.tv_usec-last_jain.tv_usec)/1000)>=jain_ping_period) {
	    gettimeofday(&last_jain,NULL);
	    for(as=as_list;as;as=as->next){
	       if(as->type == AS_TYPE && as->connected){
		  send_ping(as,&now);
	       }
	    }
	 }
	 if (servlet_ping_period && ((now.tv_sec-last_servlet.tv_sec)*1000 + (now.tv_usec-last_servlet.tv_usec)/1000)>=servlet_ping_period) {
	    gettimeofday(&last_servlet,NULL);
	    for(as=as_list;as;as=as->next){
	       if(as->type==AS_TYPE && as->connected){
		  send_ping(as,&now);
	       }
	    }
	 }
      }else{/*impossible..*/
	 LM_ERR("bug:poll returned %d\n",n);
	 goto error;
      }
   }
   return 0;
error:
   return -1;
}

/**
 * sends a ping to the app-server, and uses now as sent time
 *
 * returns
 * 	0 on success
 * 	-1 on error
 */
static inline int send_ping(struct as_entry *the_as,struct timeval *now)
{
   char *the_ping;
   as_msg_p aping;
   int pinglen,retval;
   unsigned int seqno;
   struct ping *pingu;

   aping=(as_msg_p)0;
   the_ping=(char *)0;
   retval=0;
   if (!(aping=shm_malloc(sizeof(as_msg_t)))) {
      LM_ERR("out of shm_mem for ping event\n");
      retval=-1;
      goto error;
   }
   if (!(the_ping=create_ping_event(&pinglen,0,&seqno))) {
      LM_ERR("Unable to create ping event\n");
      retval=-1;
      goto error;
   }
   aping->as=the_as;
   aping->msg=the_ping;
   aping->len=pinglen;
   
   lock_get(the_as->u.as.jain_pings.mutex);
   {
      if(the_as->u.as.jain_pings.count==the_as->u.as.jain_pings.size){
	 LM_ERR("Cant send ping because the pingtable is full (%d pings)\n",\
	       the_as->u.as.jain_pings.count);
	 retval=0;
	 lock_release(the_as->u.as.jain_pings.mutex);
	 goto error;
      }else{
	 pingu=the_as->u.as.jain_pings.pings+the_as->u.as.jain_pings.end;
	 the_as->u.as.jain_pings.end=(the_as->u.as.jain_pings.end+1)%the_as->u.as.jain_pings.size;
	 the_as->u.as.jain_pings.count++;
      }
      memcpy(&pingu->sent,now,sizeof(struct timeval));
      pingu->id=seqno;
   }
   lock_release(the_as->u.as.jain_pings.mutex);
again:
   if(0>write(write_pipe,&aping,sizeof(as_msg_p))){
      if(errno==EINTR){
	 goto again;
      }else{
	 LM_ERR("error sending ping\n");
	 goto error;
      }
   }
   return 0;
error:
   if(aping)
      shm_free(aping);
   if(the_ping)
      shm_free(the_ping);
   return retval;
}

/**
 * Initializes the high availability (ha) structure
 *
 * returns
 * 	0 on success
 * 	-1 on error
 */
int init_pingtable(struct ha *table,int timeout,int maxpings)
{
   if(maxpings<=0)
      maxpings=1;
   table->begin=0;
   table->end=0;
   table->timed_out_pings=0;
   table->size=maxpings;
   table->timeout=timeout;

   if (!(table->mutex=lock_alloc())){
      LM_ERR("Unable to allocate a lock for the ping table\n");
      goto error;
   }else 
      lock_init(table->mutex);
   LM_ERR("alloc'ing %d bytes for %d pings\n",(int)(maxpings*sizeof(struct ping)),maxpings);
   if (0==(table->pings=shm_malloc(maxpings*sizeof(struct ping)))){
      LM_ERR("Unable to shm_malloc %d bytes for %d pings\n",(int)(maxpings*sizeof(struct ping)),maxpings);
      goto error;
   }else{
      memset(table->pings,0,(maxpings*sizeof(struct ping)));
   }
   return 0;
error:
   destroy_pingtable(table);
   return -1;
}

void destroy_pingtable(struct ha *table)
{
   if(table->mutex){
      lock_dealloc(table->mutex);
      table->mutex=0;
   }
   if(table->pings){
      shm_free(table->pings);
      table->pings=0;
   }
}

/**
 * event_length(4) UNSIGNED INT includes the length 4 bytes itself
 * type(1), 
 * processor_id(1), 0 means nobody, 0xFF means everybody, 0<N<0xFF means processor with id=N
 * flags(4), 
 * ping_num(4), 
 *
 * NOT REENTRANT (uses static local var to store ping seqno.)
 * 
 * returns
 * 	0 on error
 * 	pointer to the buffer on success
 *
 */
char * create_ping_event(int *evt_len,int flags,unsigned int *seqno)
{
   unsigned int i,k;
   char *buffer;
   static unsigned int ping_seqno=0;

   if (!(buffer=shm_malloc(4+1+1+4+4))) {
      LM_ERR("out of shm for ping\n");
      return 0;
   }
   *evt_len=(4+1+1+4+4);
   ping_seqno++;
   *seqno=ping_seqno;
   i=htonl(14);
   memcpy(buffer,&i,4);
   k=4;
   /*type*/
   buffer[k++]=(unsigned char)PING_AC;
   /*processor_id*/
   buffer[k++]=(unsigned char)0xFF;
   /*flags*/
   flags=htonl(flags);
   memcpy(buffer+k,&flags,4);
   k+=4;
   /*ping sequence number*/
   i=htonl(ping_seqno);
   memcpy(buffer+k,&i,4);
   k+=4;
   return buffer;
}
