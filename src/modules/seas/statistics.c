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

#include <stdlib.h>
#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h> /* superset of previous */
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#include "statistics.h"
#include "seas.h" /*SLOG*/
#include "../../mem/shm_mem.h"
#include "../../resolve.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../locking.h"
#define STATS_PAY 101

struct statstable*  seas_stats_table;
int stats_fd;
char use_stats=0;
pid_t pid;

static void sig_handler(int signo)
{
   switch(signo){
      case SIGTERM:
	 LM_ERR("stats process caught SIGTERM, shutting down..\n");
	 close(stats_fd);
	 destroy_seas_stats_table();
	 exit(0);
      default:
	 LM_DBG("caught signal %d\n",signo);
   }
   LM_WARN("statistics process:caught signal (%d)\n",signo);
}

struct statstable* init_seas_stats_table(void)
{
   /*allocs the table*/
   seas_stats_table= (struct statstable*)shm_malloc( sizeof( struct statstable ) );
   if (!seas_stats_table) {
      LM_ERR("no shmem for stats table (%d bytes)\n",(int)sizeof(struct statstable));
      return 0;
   }
   memset(seas_stats_table, 0, sizeof(struct statstable) );
   if(0==(seas_stats_table->mutex=lock_alloc())){
      LM_ERR("couldn't alloc mutex (get_lock_t)\n");
      shm_free(seas_stats_table);
      return 0;
   }
   lock_init(seas_stats_table->mutex);
   return seas_stats_table;
}

void destroy_seas_stats_table(void)
{
   /*deallocs the table*/
   if(seas_stats_table){
      lock_destroy(seas_stats_table->mutex);
      shm_free(seas_stats_table);
      seas_stats_table=(struct statstable *)0;
   }
}

/** This will be called from within w_as_relay()
 *
 * TODO handle locking ?
 */
void as_relay_stat(struct cell *t)
{
   struct statscell *s;
   struct totag_elem *to;
   if(t==0)
      return;
   if(t->fwded_totags != 0){
      LM_DBG("seas:as_relay_stat() unable to put a payload "
			  "in fwded_totags because it is being used !!\n");
      return;
   }
   if(!(s=shm_malloc(sizeof(struct statscell)))){
      return;
   }
   if(!(to=shm_malloc(sizeof(struct totag_elem)))){
      shm_free(s);
      return;
   }
   memset(s,0,sizeof(struct statscell));
   gettimeofday(&(s->u.uas.as_relay),NULL);
   s->type=UAS_T;
   to->tag.len=0;
   to->tag.s=(char *)s;
   to->next=0;
   to->acked=STATS_PAY;
   t->fwded_totags=to;
   lock_get(seas_stats_table->mutex);
   (seas_stats_table->started_transactions)++;
   lock_release(seas_stats_table->mutex);
}

/** this will be called from the SEAS event dispatcher
 * when it writes the event to the socket
 *
 * Parameters: a cell OR its hash_index and its label
 *
 * TODO handle locking/mutexing ?
 */
void event_stat(struct cell *t)
{
   struct statscell *s;
   struct totag_elem *to;
   if(t==0){
      /*seas_f.tmb.t_lookup_ident(&t,hash_index,label); BAD bcos it refcounts,
	   * and there's no way to simply unrefcount from outside TM*/
      return;
   }
   if(t->fwded_totags == 0){
      LM_DBG("seas:event_stat() unabe to set the event_stat timeval:"
			  " no payload found at cell!! (fwded_totags=0)\n");
      return;
   }
   /*esto da un CORE DUMP cuando hay mucha carga.. warning*/
   to=t->fwded_totags;
   while(to){
      if(to->acked==STATS_PAY){
	 s=(struct statscell *)to->tag.s;
	 gettimeofday(&(s->u.uas.event_sent),NULL);
	 return;
      }else
	 to=to->next;
   }
   return;
}

/** param i is in milliseconds*/
static inline int assignIndex(int i)
{
   return (i/100)>14?14:(i/100);
}

/** this will be called from the SEAS action dispatcher
 * when it receives the action from the socket
 */
void action_stat(struct cell *t)
{
   unsigned int seas_dispatch;
   //unsigned int as_delay;
   struct timeval *t1,*t2;
   //struct timeval *t3;
   struct statscell *s;
   struct totag_elem *to;
   if(t==0)
      return;
   if(t->fwded_totags == 0){
      LM_DBG("seas:event_stat() unable to set the event_stat timeval:"
			  " no payload found at cell!! (fwded_totags=0)\n");
      return;
   }
   to=t->fwded_totags;
   while(to){
      if(to->acked==STATS_PAY){
	 s=(struct statscell *)to->tag.s;
	 gettimeofday(&(s->u.uas.action_recvd),NULL);
	 break;
      }else
	 to=to->next;
   }
   /**no statistics found**/
   if(to==0)
      return;
   t1=&(s->u.uas.as_relay);
   t2=&(s->u.uas.event_sent);
   //t3=&(s->u.uas.action_recvd);
   seas_dispatch = (t2->tv_sec - t1->tv_sec)*1000 + (t2->tv_usec-t1->tv_usec)/1000;
   //as_delay = (t3->tv_sec - t2->tv_sec)*1000 + (t3->tv_usec-t2->tv_usec)/1000;

   lock_get(seas_stats_table->mutex);
   {
      seas_stats_table->dispatch[assignIndex(seas_dispatch)]++;
      seas_stats_table->event[assignIndex(seas_dispatch)]++;
      (seas_stats_table->finished_transactions)++;
   }
   lock_release(seas_stats_table->mutex);
}


/**
 * stats socket sould be an IP_address:port or unix://path/to_file
 * TODO handling unix sockets and IPv6 !!
 *
 * returns 
 * 0 if no stats
 * 1 if stats properly started
 * -1 if error
 */
int start_stats_server(char *stats_socket)
{
   char *p,*port;
   unsigned short stats_port;
   struct hostent *he;
   /*use sockaddr_storage ??*/
   struct sockaddr_in su;
   int optval;

   use_stats=0;
   port=(char *)0;
   he=(struct hostent *)0;
   stats_fd=-1;
   p=stats_socket;

   if(p==0 || *p==0)
      return 0;

   if(!init_seas_stats_table()){
      LM_ERR("unable to init stats table, disabling statistics\n");
      return -1;
   }
   while(*p){
      if(*p == ':'){
	 *p=0;
	 port=p+1;
	 break;
      }
   }
   if(!(he=resolvehost(stats_socket)))
      goto error;
   if(port==(char*)0 || *port==0)
      stats_port=5088;
   else if(!(stats_port=str2s(port,strlen(port),0))){
	 LM_ERR("invalid port %s\n",port);
	 goto error;
      }
   if((stats_fd=socket(he->h_addrtype, SOCK_STREAM, 0))==-1){
      LM_ERR("trying to open server socket (%s)\n",strerror(errno));
      goto error;
   }
   optval=1;
   if (setsockopt(stats_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&optval, sizeof(optval))==-1) {
      LM_ERR("setsockopt (%s)\n",strerror(errno));
      goto error;
   }
   su.sin_family = he->h_addrtype;
   su.sin_port=htons(stats_port);
   memcpy(&su.sin_addr,he->h_addr_list[0],4);
   if((bind(stats_fd,(struct sockaddr*)&su,sizeof(struct sockaddr_in)))==-1){
      LM_ERR( "bind (%s)\n",strerror(errno));
      goto error;
   }
   if(listen(stats_fd, 10)==-1){
      LM_ERR( "listen (%s)\n",strerror(errno));
      goto error;
   }
   if(!(pid=fork())){/*child*/
      signal(SIGTERM,sig_handler);
      serve_stats(stats_fd);
      printf("statistics Server Process exits !!\n");
      exit(0);
   }else if(pid>0){/*parent*/
      close(stats_fd);
   }else{/*error*/
      LM_ERR("failed to create stats server process\n");
      goto error;
   }
   use_stats=1;
   return 1;
error:
   if(stats_fd!=-1)
      close(stats_fd);
   destroy_seas_stats_table();
   return -1;
}

/**
 * stats socket sould be an IP_address:port or unix://path/to_file
 * TODO handling unix sockets and IPv6 !!
 *
 * returns 
 * 0 if no stats
 * 1 if stats properly started
 * -1 if error
 */
int stop_stats_server(void)
{
   if(pid)
      kill(SIGTERM,pid);
   return 0;
}

void serve_stats(int fd)
{
   union sockaddr_union su;
   int sock,i,retrn;
   socklen_t su_len;
   char f;
   /* we install our signal handler..*/
   signal(SIGTERM,sig_handler);
   signal(SIGHUP,sig_handler);
   signal(SIGPIPE,sig_handler);
   signal(SIGQUIT,sig_handler);
   signal(SIGINT,sig_handler);
   signal(SIGCHLD,sig_handler);

   while(1){
      su_len = sizeof(union sockaddr_union);
      sock=-1;
      sock=accept(fd, &su.s, &su_len);
      if(sock==-1){
	 if(errno==EINTR){
	    continue;
	 }else{
	    LM_ERR("failed to accept connection: %s\n", strerror(errno));
	    return ;
	 }
      }
      while(0!=(i=read(sock,&f,1))){
	 if(i==-1){
	    if(errno==EINTR){
	       continue;
	    }else{
	       LM_ERR("unknown error reading from socket\n"); 
	       close(sock);
	       /** and continue accept()'ing*/
	       break;
	    }
	 }
	 retrn=print_stats_info(f,sock);
	 if(retrn==-1){
	    /**simple error happened, dont worry*/
	       LM_ERR("printing statisticss \n");
	       continue;
	 }else if(retrn==-2){
	    /**let's go to the outer loop, and receive more Statistics clients*/
	    LM_ERR("statistics client left\n");
	    close(sock);
	    break;
	 }
      }
   }
}

/**
 * (from snprintf manual)
 * "The  functions  snprintf()  and vsnprintf() do not write more than size bytes (including the trailing '\\0').  If the output was truncated due to
 * this limit then the return value is the number of characters (not including the trailing '\\0') which would have been written to the final string
 * if  enough space had been available. Thus, a return value of size or more means that the output was truncated."
 */
int print_stats_info(int f,int sock)
{
#define STATS_BUF_SIZE  400
   int j,k,writen;
   char buf[STATS_BUF_SIZE];

   writen=0;
   if(0>(k=snprintf(buf,STATS_BUF_SIZE, "Timings:      0-1   1-2   2-3   3-4   4-5   5-6   6-7   7-8   8-9   9-10  10-11 11-12 12-13 13-14 14+\n"))){
      goto error;
   }else{
      if(k>STATS_BUF_SIZE){
	 j=STATS_BUF_SIZE;
	 goto send;
      }
      j=k;
   }
   lock_get(seas_stats_table->mutex);
   if(0>(k=snprintf(&buf[j],STATS_BUF_SIZE-j,"UAS:dispatch: %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d\n",\
	       seas_stats_table->dispatch[0],seas_stats_table->dispatch[1],seas_stats_table->dispatch[2],seas_stats_table->dispatch[3],seas_stats_table->dispatch[4]\
	       ,seas_stats_table->dispatch[5],seas_stats_table->dispatch[6],seas_stats_table->dispatch[7],seas_stats_table->dispatch[8],seas_stats_table->dispatch[9],\
	       seas_stats_table->dispatch[10],seas_stats_table->dispatch[11],seas_stats_table->dispatch[12],seas_stats_table->dispatch[13],seas_stats_table->dispatch[14]))){
      goto error;
   }else{
      if(k>(STATS_BUF_SIZE-j)){
	 j=STATS_BUF_SIZE;
	 goto send;
      }
      j+=k;
   }
   if(0>(k=snprintf(&buf[j],STATS_BUF_SIZE-j,"UAS:event:    %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d %-5d\n",\
	       seas_stats_table->event[0],seas_stats_table->event[1],seas_stats_table->event[2],seas_stats_table->event[3],seas_stats_table->event[4]\
	       ,seas_stats_table->event[5],seas_stats_table->event[6],seas_stats_table->event[7],seas_stats_table->event[8],seas_stats_table->event[9],\
	       seas_stats_table->event[10],seas_stats_table->event[11],seas_stats_table->event[12],seas_stats_table->event[13],seas_stats_table->event[14]))){
      goto error;
   }else{
      if(k>STATS_BUF_SIZE-j){
	 j=STATS_BUF_SIZE;
	 goto send;
      }
      j+=k;
   }
   if(0>(k=snprintf(&buf[j],STATS_BUF_SIZE-j,"Started Transactions: %d\nTerminated Transactions:%d\nReceived replies:%d\nReceived:%d\n",\
	       seas_stats_table->started_transactions,seas_stats_table->finished_transactions,seas_stats_table->received_replies,seas_stats_table->received))){
      goto error;
   }else{
      if(k>STATS_BUF_SIZE-j){
	 j=STATS_BUF_SIZE;
	 goto send;
      }
      j+=k;
   }
send:
   lock_release(seas_stats_table->mutex);
again:/*mutex is released*/
   k=write(sock,buf,j);
   if(k<0){
      switch(errno){
	 case EINTR:
	    goto again;
	 case EPIPE:
	    return -2;
      }
   }
   writen+=k;
   if(writen<j)
      goto again;
   return writen;
error:/*mutex is locked*/
   lock_release(seas_stats_table->mutex);
   return -1;
}

void stats_reply(void)
{
   lock_get(seas_stats_table->mutex);
   seas_stats_table->received_replies++;
   lock_release(seas_stats_table->mutex);
}


