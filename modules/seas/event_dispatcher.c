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

#include <sys/types.h>/*setsockopt,bind,accept,fork,pid_t*/
#include <sys/socket.h>/*setsockopt,bind,accept,listen*/
#include <netinet/tcp.h>/*TCP_NODELAY*/
#include <string.h>/*strcmp,memset*/
#include <errno.h>/*errno*/
#include <unistd.h>/*close(),read(),pipe,fork,pid_t*/
#include <sys/poll.h>/*poll*/
#include <signal.h>/*signal*/
#include <time.h>/*time*/
#include <string.h>/*memcmp*/
#include <sys/types.h>/*waitpid*/
#include <sys/wait.h>/*waitpid*/

#include "../../ip_addr.h" /*sockaddr_union, ip_addr*/
#include "../../hashes.h" /*T_TABLE_POWER*/
#include "../../mem/mem.h" /*pkg_malloc*/
#include "../../mem/shm_mem.h" /*shm_malloc*/
#include "../../dprint.h" /*LM_**/
#include "../../locking.h"
#include "../../cfg/cfg_struct.h"

#include "seas.h"
#include "ha.h"
#include "cluster.h"
#include "seas_action.h"
#include "statistics.h"
#include "event_dispatcher.h"

#define PING_OVER_FACTOR 2
#define MAX_WRITE_TRIES 10

char *action_names[]={"NONE",
     "PROVISIONAL_REPLY",
     "FINAL_REPLY",
     "REPLY_FIN_DLG",
     "UAC_REQ",
     "AC_RES_FAIL",
     "STATELESS_MSG",
     "AC_CANCEL",
     "JAIN_PONG"};


struct unc_as unc_as_t[2*MAX_UNC_AS_NR];

/*this is for the Action Dispatcher Process */
struct as_entry *my_as;
extern int process_no;
extern int sig_flag;

static int process_event_reply(as_p as);
static int handle_as_data(int fd);
static inline int print_sock_info(char *buffer,int wheremax,int *idx,struct socket_info *s,enum sip_protos type);
static inline int send_sockinfo(int fd);
static inline int add_new_as(int event_idx,int action_idx,struct as_entry *as);
static int dispatch_relay();
static int new_as_connect(int fd,char which);
static inline int read_name(int sock,char *dst,int dstlen);
static int handle_unc_as_data(int fd);
static int open_server_sockets(struct ip_addr *address,unsigned short port,int *fd);



/** Main loop for the Event Dispatcher process.
 * 
 */
int dispatcher_main_loop(void)
{
   struct pollfd poll_fds[3+MAX_AS_NR],*poll_tmp;
   int clean_index,i,j,k,fd,poll_events=0,socks[2],chld_status;
   int as_nr,unc_as_nr;
   pid_t chld;
   struct timeval last_ping,now;
   struct as_entry *as;

   sig_flag=0;
   is_dispatcher=1;
   as_nr=0;

   timerclear(&last_ping);
   timerclear(&now);
   signal(SIGCHLD,seas_sighandler);
   signal(SIGTERM,seas_sighandler);
   signal(SIGUSR1,seas_sighandler);
   signal(SIGINT, seas_sighandler);
   signal(SIGKILL,seas_sighandler);

   strcpy(whoami,"Seas Event Dispatcher process");
   /*I set process_no to -1 because otherwise, the logging process confuses this process with another from SER
    * (see LM_*() and dprint() and my_pid())*/
   process_no = -1;

   if(open_server_sockets(seas_listen_ip,seas_listen_port,socks)==-1){
      LM_ERR("unable to open server sockets on dispatcher\n");
      return -1;
   }
   for(i=0;i<2;i++){
      poll_fds[i].fd=socks[i];
      poll_fds[i].revents=0;
      poll_fds[i].events=POLLIN;
   }
   poll_fds[2].fd=read_pipe;
   poll_fds[2].revents=0;
   poll_fds[2].events=POLLIN;/*pollhup ?*/

   poll_events=0;
   unc_as_nr=0;

   if(use_ha)
      spawn_pinger();

   while(1){
		/* update the local config framework structures */
		cfg_update();

      if(sig_flag==SIGCHLD){
	 while ((chld=waitpid( -1, &chld_status, WNOHANG ))>0) {
	    if (WIFEXITED(chld_status)){
	       LM_INFO("child process %d exited normally, status=%d\n",
				   chld,WEXITSTATUS(chld_status));
	    }else if (WIFSIGNALED(chld_status)) {
	       LM_INFO("child process %d exited by a signal %d\n",
				   chld,WTERMSIG(chld_status));
	    }else if (WIFSTOPPED(chld_status)) 
	       LM_INFO("child process %d stopped by a signal %d\n",
				   chld,WSTOPSIG(chld_status));
	    for (as=as_list;as;as=as->next) {
	       if(as->type!=AS_TYPE)
		  continue;
	       if(as->u.as.action_pid==chld){
		  for(i=0;i<as_nr && ((poll_fds[3+i].fd)!=(as->u.as.event_fd));i++)
		     ;
		  if(i==as_nr){
		     LM_ERR("Either the pinger has died or BUG found..\n");
		     continue;
		  }
		  /*overwrite the obsolete 'i' position with the next position*/
		  for(j=3+i;j<(as_nr+unc_as_nr+3-1);i++){
		     poll_fds[j].fd=poll_fds[j+1].fd;
		     poll_fds[j].events=poll_fds[j+1].events;
		     poll_fds[j].revents=poll_fds[j+1].revents;
		  }
		  close(as->u.as.event_fd);/*close the socket fd*/
		  if (as->u.as.ev_buffer.s) {
		     pkg_free(as->u.as.ev_buffer.s);
		     as->u.as.ev_buffer.s=(char *)0;
		     as->u.as.ev_buffer.len=0;
		  }
		  as->u.as.event_fd=as->u.as.action_fd=-1;
		  as->connected=0;
		  destroy_pingtable(&as->u.as.jain_pings);
		  destroy_pingtable(&as->u.as.servlet_pings);
		  as_nr--;
		  LM_WARN("client [%.*s] leaving (Action Dispatcher Process died !)\n",
				  as->name.len,as->name.s);
		  break;
	       }/*if(action_pid==chld)*/
	    }/*for(as=as_list;as;as=as->next)*/
	 }/*while(waitpid(-1)>0)*/
      }else if (sig_flag) {
	 LM_WARN("received signal != sigchld(%d)\n",sig_flag);
	  }
      sig_flag=0;
      clean_index=0;
      LM_INFO("polling [2 ServSock] [1 pipe] [%d App Servers]"
			  " [%d Uncomplete AS]\n",as_nr,unc_as_nr);
      poll_events = poll(poll_fds,3+unc_as_nr+as_nr,-1);
      if (poll_events == -1) {
	 if(errno==EINTR){
	    /*handle the case a child has died.
	     * It will be done in the next iteration in if(seas_sigchld_received)*/
	    continue;
	 }
	 if(errno==EBADF){
	    LM_ERR("invalid file descriptor passed to poll (%s)\n",
				strerror(errno));
	    return -1;/*??*/
	 }
	 /* errors */
	 LM_ERR("poll'ing:%s\n",strerror(errno));
	 poll_events=0;
	 continue;
      } else if (poll_events == 0) {/*timeout*/
	 continue;
      } else {/*there are events !*/
	 /*handle connections from server sockets*/
	 for(i=0;i<2;i++){
	    if(poll_fds[i].revents)
	       poll_events--;
	    if(poll_fds[i].revents & POLLIN){
	       poll_fds[i].revents &= (~POLLIN);
	       if((fd=new_as_connect(socks[i],i==0?'e':'a'))>=0){
		  poll_tmp=&poll_fds[3+as_nr+unc_as_nr];
		  poll_tmp->fd=fd;
		  poll_tmp->events=POLLIN|POLLHUP;
		  unc_as_nr++;
		  LM_DBG("Have new %s client\n",i==0?"event":"action");
	       }else{
		  LM_ERR("accepting connection from AS\n");
	       }
	    }
	 }
	 /*handle data from pipe*/
	 if(poll_fds[2].revents & POLLIN){
	    poll_fds[2].revents &= (~POLLIN);
	    poll_events--;
	    if(dispatch_relay()<0){
	       LM_ERR("dispatch_relay returned -1"
		     "should clean-up table\n");
	    }
	 }
	 /*now handle receive data from completed AS*/
	 clean_index=0;
	 LM_DBG("Scanning data from %d AS\n",as_nr);
	 for(i=0;(i<as_nr) && poll_events;i++){
	    clean_index=0;
	    poll_tmp=&poll_fds[3+i];
	    if(poll_tmp->revents)
	       poll_events--;
	    if(poll_tmp->revents & POLLIN){
	       LM_DBG("POLLIN found in AS #%i\n",i);
	       poll_tmp->revents &= (~POLLIN);
	       switch(handle_as_data(poll_tmp->fd)){
		  case -2:/*read returned 0 bytes, an AS client is leaving*/
		     clean_index=1;
		     break;
		  case -1:/*shouldnt happen*/
		     LM_ERR("reading from AS socket\n");
		     break;
		  case 0:/* event_response received and processed*/
		     break;
		  default:
		     LM_WARN("unknown return type from handle_as_data\n");
	       }
	    }
	    if(clean_index || (poll_tmp->revents & POLLHUP)){
	       LM_DBG("POLHUP or read==0 found in %i AS \n",i);
	       clean_index=0;
	       poll_tmp->revents = 0;
	       for(as=as_list;as;as=as->next){
		  if(as->type==CLUSTER_TYPE)
		     continue;
		  if(as->connected && (as->u.as.event_fd == poll_tmp->fd)){
		     close(poll_tmp->fd);/*close the socket fd*/
		     /*TODO  we should send a signal to the Action Dispatcher !!!*/
		     as->connected=0;
		     as_nr--;
		     /*overwrite the obsolete 'i' position with the next position*/
		     for(k=i;k<(as_nr+unc_as_nr);k++){
			j=3+k;
			poll_fds[j].fd=poll_fds[j+1].fd;
			poll_fds[j].events=poll_fds[j+1].events;
			poll_fds[j].revents=poll_fds[j+1].revents;
		     }
		     --i;
		     LM_WARN("client %.*s leaving !!!\n",as->name.len,as->name.s);
		     break;
		  }
	       }
	       if (!as) {
		  LM_ERR("the leaving client was not found in the as_list\n");
		   }
	    }
	 }
	 /*now handle data sent from uncompleted AS*/
	 LM_DBG("Scanning data from %d uncomplete AS \n",unc_as_nr);
	 clean_index=0;
	 for(i=0;i<unc_as_nr && poll_events;i++){
	    poll_tmp=&poll_fds[3+as_nr+i];
	    if(poll_tmp->revents)
	       poll_events--;
	    if(poll_tmp->revents & POLLIN){
	       LM_DBG("POLLIN found in %d uncomplete AS \n",i);
	       poll_tmp->revents &= (~POLLIN);
	       fd=handle_unc_as_data(poll_tmp->fd);
	       if(fd>0){
		  /* there's a new AS, push the uncomplete poll_fds up and set the AS */
		  for(k=i;k>0;k--){
		     j=3+as_nr+k;
		     poll_fds[j].fd=poll_fds[j-1].fd;
		     poll_fds[j].events=poll_fds[j-1].events;
		     poll_fds[j].revents=poll_fds[j-1].revents;
		  }
		  poll_fds[3+as_nr].fd=fd;
		  poll_fds[3+as_nr].events=POLLIN|POLLHUP;
		  poll_fds[3+as_nr].revents=0;
		  as_nr++;/*not very sure if this is thread-safe*/
		  unc_as_nr--;
	       }else if(fd<=0){/* pull the upper set of uncomplete AS down and take this one out*/
		  poll_tmp->revents=0;
		  for(k=i;k<(unc_as_nr-1);k++){
		     j=3+as_nr+k;
		     poll_fds[j].fd=poll_fds[j+1].fd;
		     poll_fds[j].events=poll_fds[j+1].events;
		     poll_fds[j].revents=poll_fds[j+1].revents;
		  }
		  unc_as_nr--;
		  /** we decrement i so that pulling down the upper part of the unc_as array so that
		   * it doesn't affect our for loop */
		  i--;
	       }
	    }
	    if(poll_tmp->revents & POLLHUP){
	       LM_DBG("POLLHUP found in %d uncomplete AS \n",i);
	       close(poll_tmp->fd);
	       for(k=i;k<(unc_as_nr-1);k++){
		  j=3+as_nr+k;
		  poll_fds[j].fd=poll_fds[j+1].fd;
		  poll_fds[j].events=poll_fds[j+1].events;
		  poll_fds[j].revents=poll_fds[j+1].revents;
	       }
	       unc_as_nr--;
	       i--;
	       poll_tmp->revents = 0;
	    }
	 }/*for*/
      }/*else ...(poll_events>0)*/
   }/*while(1)*/
}


/**
 * opens the server socket, which attends (accepts) the clients, that is: 
 * params:
 * address:
 * 	address to which to listen
 * port:
 * 	base port to which to listen. then port+1 will be the socket
 * 	for action's delivery.
 * fds:
 * 	in fd[0] the action socket will be put.
 * 	in fd[1] the event socket will be put.
 *
 * returns 0 on exit, <0 on fail
 *
 */
static int open_server_sockets(struct ip_addr *address,unsigned short port,int *fd)
{

   /*using sockaddr_union enables ipv6..*/
   union sockaddr_union su;
   int i,optval;

   fd[0]=fd[1]=-1;

   if(address->af!=AF_INET && address->af!=AF_INET6){
      LM_ERR("Only ip and ipv6 allowed socket types\n");
      return -1;
   }

   for(i=0;i<2;i++){
      if(init_su(&su,address,port+i)<0){
	 LM_ERR("unable to init sockaddr_union\n");
	 return -1;
      }
      if((fd[i]=socket(AF2PF(su.s.sa_family), SOCK_STREAM, 0))==-1){
	 LM_ERR("trying to open server %s socket (%s)\n",i==0?"event":"action",strerror(errno));
	 goto error;
      }
      optval=1;
      if (setsockopt(fd[i], SOL_SOCKET, SO_REUSEADDR, (void*)&optval, sizeof(optval))==-1) {
	 LM_ERR("setsockopt (%s)\n",strerror(errno));
	 goto error;
      }
      if ((bind(fd[i],(struct sockaddr *)&(su.s),sizeof(struct sockaddr_in)))==-1){
	 LM_ERR( "bind (%s)\n",strerror(errno));
	 goto error;
      }
      if (listen(fd[i], 10)==-1){
	 LM_ERR( "listen (%s)\n",strerror(errno));
	 goto error;
      }
   }
   return 0;

error:
   for(i=0;i<2;i++)
      if(fd[i]!=-1){
	 close(fd[i]);
	 fd[i]=-1;
      }
   return -1;
}


union helper{
   as_msg_p ptr;
   char bytes[sizeof(as_msg_p)];
};

/**
 * Sends event 
 *
 * returns
 * 	 0  OK
 * 	-1 couldn't read the event from the pipe
 * 	-2 couldn't send the event
 * TODO this should be FAR more generic... for example, there might be events
 * which are not related to any transaction (finish event, or error event...)
 * we should separate event-specific handling in different functions...
 */
static int dispatch_relay(void)
{
   int i,j,retval,tries;
   union helper thepointer;

   i=j=0;
   retval=0;
read_again:
   i=read(read_pipe,thepointer.bytes+j,sizeof(as_msg_p)-j);
   if(i<0){
      if(errno==EINTR){
	 goto read_again;
      }else{
	 LM_ERR("Dispatcher Process received unknown error"
			 " reading from pipe (%s)\n",strerror(errno));
	 retval=-1;
	 goto error;
      }
   }else if(i==0){
	 LM_ERR("Dispatcher Process "
	       "received 0 while reading from pipe\n");
	 goto error;
   }else{
      j+=i;
      if(j<sizeof(as_msg_p))
	 goto read_again;
   }

   if (!thepointer.ptr) {
      LM_ERR("Received Corrupted pointer to event !!\n");
      retval=0;
      goto error;
   }
   /*the message*/
   if(use_stats && thepointer.ptr->transaction)
      event_stat(thepointer.ptr->transaction);
   if(thepointer.ptr->as == NULL || !thepointer.ptr->as->connected || thepointer.ptr->as->type==CLUSTER_TYPE){
      LM_WARN("tryied to send an event to an App Server"
			  " that is scheduled to die!!\n");
      retval=-2;
      goto error;
   }
   j=0;
   tries=0;
write_again:
   i=write(thepointer.ptr->as->u.as.event_fd,thepointer.ptr->msg+j,thepointer.ptr->len-j);
   if(i==-1){
      switch(errno){
	 case EINTR:
	    if(!thepointer.ptr->as->connected){
	       LM_WARN("tryied to send an event to an App Server"
				   " that is scheduled to die!!\n");
	       retval=-2;
	       goto error;
	    }
	    goto write_again;
	 case EPIPE:
	    LM_ERR("AS [%.*s] closed "
		  "the socket !\n",thepointer.ptr->as->u.as.name.len,thepointer.ptr->as->u.as.name.s);
	    retval=-2;
	    goto error;
	 default:
	    LM_ERR("unknown error while trying to write to AS socket(%s)\n",
				strerror(errno));
	    retval=-2;
	    goto error;
      }
   }else if(i>0){
      j+=i;
      if(j<thepointer.ptr->len)
	 goto write_again;
   }else if(i==0){
      if (tries++ > MAX_WRITE_TRIES) { 
	 LM_ERR("MAX WRITE TRIES !!!\n");
	 goto error;
      }else
	 goto write_again;
   }
   LM_DBG("Event relaied to %.*s AS\n",thepointer.ptr->as->u.as.name.len,
		   thepointer.ptr->as->u.as.name.s);
   LM_DBG("Event type %s \n",action_names[thepointer.ptr->type]);
   retval=0;
error:
   if(thepointer.ptr){
      if(thepointer.ptr->msg)
	 shm_free(thepointer.ptr->msg);
      shm_free(thepointer.ptr);
   }
   return retval;
}

/**
 * receives 2 indexes in unc_as_t which correspond one to
 * the events socket and the other to the actions socket
 *
 * returns
 * 	0 on success
 * 	-1 on error
 */
static inline int add_new_as(int event_idx,int action_idx,struct as_entry *as)
{
   struct unc_as *ev,*ac;
   int j;
   as_p the_as=0;
   struct as_entry *tmp;

   ev=&unc_as_t[event_idx];
   ac=&unc_as_t[action_idx];

   the_as=&(as->u.as);

   the_as->action_fd=ac->fd;
   the_as->event_fd=ev->fd;
   the_as->name.len = strlen(ev->name);
   if(use_ha){
      if(jain_ping_timeout){
	 if (0>init_pingtable(&the_as->jain_pings,jain_ping_timeout,(jain_ping_timeout/jain_ping_period+1)*PING_OVER_FACTOR)){
	    LM_ERR("Unable to init jain pinging table...\n");
	    goto error;
	 }
      }
      if(servlet_ping_timeout){
	 if (0>init_pingtable(&the_as->servlet_pings,servlet_ping_timeout,(servlet_ping_timeout/servlet_ping_period+1)*PING_OVER_FACTOR)){
	    LM_ERR("Unable to init servlet pinging table...\n");
	    goto error;
	 }
      }
   }
   /*TODO attention, this is pkg_malloc because only the Event_Dispatcher process 
    * has to use it !!*/
   if(!(the_as->ev_buffer.s = pkg_malloc(AS_BUF_SIZE))){
      LM_ERR("unable to alloc pkg mem for the event buffer\n");
      goto error;
   }
   the_as->ev_buffer.len=0;
   as->connected=1;
   the_as->action_pid=0;
   for(tmp=as_list;tmp;tmp=tmp->next){
      if(tmp->type==AS_TYPE)
	 continue;
      for (j=0;j<tmp->u.cs.num;j++) {
	 if (tmp->u.cs.as_names[j].len == the_as->name.len && 
	       !memcmp(tmp->u.cs.as_names[j].s,the_as->name.s,the_as->name.len)) { 
	    if(tmp->u.cs.num==tmp->u.cs.registered){
	       LM_ERR("AS %.*s belongs to cluster %.*s which is already completed\n",
		     the_as->name.len,the_as->name.s,tmp->name.len,tmp->name.s);
	       break;
	    }
	    tmp->u.cs.registered++;
	    break;
	 }
      }
   }
   if(0>spawn_action_dispatcher(as)){
      LM_ERR("Unable to spawn Action Dispatcher for as %s\n",ev->name);
      goto error;
   }
   if(send_sockinfo(the_as->event_fd)==-1){
      LM_ERR("Unable to send socket info to as %s\n",ev->name);
      goto error;
   }
   return 0;
error:
   if(the_as->ev_buffer.s){
      pkg_free(the_as->ev_buffer.s);
      the_as->ev_buffer.s=(char*)0;
   }
   if(the_as->action_pid)
      kill(the_as->action_pid,SIGTERM);
   if(jain_ping_timeout)
      destroy_pingtable(&the_as->jain_pings);
   if(servlet_ping_timeout)
      destroy_pingtable(&the_as->servlet_pings);
   return -1;
}






/**prints available sockets in SER to the App Server.
 * format is:
 * 1: transport identifier (u for UDP, t for TCP, s for TLS)
 * 1: length of socket name (sip.voztele.com or whatever)
 * N: name
 * 1: length of IP address (192.168.1.2)
 * N: ip address in ascii
 * 2: port nubmer in NBO
 *
 * returns
 * 	-1 on error
 * 	0  on success
 */
static inline int send_sockinfo(int fd)
{
   struct socket_info *s;
   unsigned char i;
   char buffer[300];
   int k=0,j;
   buffer[k++]=16;/*This used to be T_TABLE_POWER in Kamailio 1.0.1, now its hardcoded in config.h*/
   for(i=0,s=udp_listen;s;s=s->next,i++);
#ifdef USE_TCP
   for(s=tcp_listen;s;s=s->next,i++);
#endif
#ifdef USE_TLS
   for(s=tls_listen;s;s=s->next,i++);
#endif
   if(i==0){
      LM_ERR("no udp|tcp|tls sockets ?!!\n");
      return -1;
   }
   buffer[k++]=i;
   for(s=udp_listen;s;s=s->next){
      if(print_sock_info(buffer,300,&k,s,PROTO_UDP)==-1)
	 return -1;
   }
#ifdef USE_TCP
   for(s=tcp_listen;s;s=s->next){
      if(print_sock_info(buffer,300,&k,s,PROTO_TCP)==-1)
	 return -1;
   }
#endif
#ifdef USE_TLS
   for(s=tls_listen;s;s=s->next){
      if(print_sock_info(buffer,300,&k,s,PROTO_TLS)==-1)
	 return -1;
   }
#endif
write_again:
   j=write(fd,buffer,k);
   if(j==-1){
      if(errno==EINTR)
	 goto write_again;
      else
	 return -1;
   }
   return 0;
}

/* prints sock info into the byte array where
 * returns 0 on success, -1 on err
 * the message sent is as follows:
 * 1: protocol type (0=NONE,1=UDP, 2=TCP, 3=TLS)
 * 1: name length
 * N: name
 * 1: address string length
 * N: address
 * 2: NBO unsigned shor int port number
 *
 * TODO buffer overflow risk
 */
static inline int print_sock_info(char *buffer,int wheremax,int *idx,struct socket_info *s,enum sip_protos type)
{
   int k;
   unsigned char i;
   unsigned short int j;
   if((wheremax-*idx)<49)/*31*name+17*ipv6+2*port+1*type*/
      return -1;
   k=*idx;
   buffer[k++]=(char)type;
   if((i=(unsigned char)s->name.len)>30){
      LM_ERR("name too long\n");
      return -1;
   }
   buffer[k++]=i;
   memcpy(&buffer[k],s->name.s,i);
   k+=i;
   i=(unsigned char)s->address_str.len;
   buffer[k++]=i;
   memcpy(&buffer[k],s->address_str.s,i);
   k+=i;
   j=htons(s->port_no);
   memcpy(&buffer[k],&j,2);
   k+=2;
   *idx=k;
   return 0;
}

/**
 * Handles data from an AppServer. First searches in the AS table which was the AS
 * that sent the data (we dont already know it because this comes from a poll_fd
 * struct). When the one is found, it calls process_event_reply, which in turn
 * looks if there's a complete event in the buffer, and if there is, processes it.
 *
 * returns
 * 	-1 on error
 * 	-2 on read()==0 (the socket has been closed by the other end)
 *  	0 on success
 */
static int handle_as_data(int fd)
{
   int j,k;
   struct as_entry *as;
   for(as=as_list;as;as=as->next)
      if(as->type == AS_TYPE && as->connected && (as->u.as.event_fd==fd))
	 break;
   if(!as){
      LM_ERR("AS not found\n");
      return -1;
   }
   k=AS_BUF_SIZE-(as->u.as.ev_buffer.len);
again:
   if((j=read(fd,as->u.as.ev_buffer.s+as->u.as.ev_buffer.len,k))<0){
      LM_ERR("reading data for as %.*s\n",as->name.len,as->name.s);
      if(errno==EINTR)
	 goto again;
      else
	 return -1;
   }else if(j==0){
      LM_ERR("AS client leaving (%.*s)\n",as->name.len,as->name.s);
      return -2;
   }
   as->u.as.ev_buffer.len+=j;
   LM_DBG("read %d bytes from AS (total = %d)\n",j,as->u.as.ev_buffer.len);
   if(as->u.as.ev_buffer.len>10)
      process_event_reply(&as->u.as);
   return 0;
}

/**
 * This function processess the Application Server buffer. We do buffered
 * processing because it increases performance quite a bit. Any message
 * sent from the AS comes with the first 2 bytes as an NBO unsigned short int 
 * which says the length of the following message (header and payload).
 * This way, we avoid multiple small reads() to the socket, which (as we know), consumes
 * far more processor because of the kernel read(2) system call. The drawback
 * is the added complexity of mantaining a buffer, the bytes read, and looking
 * if there is a complete message already prepared.
 *
 * Actions are supposed to be small, that's why BUF_SIZE is 2000 bytes length. 
 * Most of the actions will be that size or less. That is why the 4 bytes telling the
 * length of the Action payload are included in its size. This way you can use a fixed size
 * buffer to receive the Actions and not need to be pkb_malloc'ing for each new event.
 * If there is a particular bigger packet, for example one carrying a picture (a JPG can
 * easily surpass the 2000 byte limit) then a pkg_malloc will be required. This is left TODO
 *
 * returns
 * 	-1 on error (packet too big)
 * 	0  on success
 */
static int process_event_reply(as_p as)
{
   unsigned int ev_len;
   unsigned char processor_id,type;
   unsigned int flags;

   ev_len=(as->ev_buffer.s[0]<<24)|(as->ev_buffer.s[1]<<16)|(as->ev_buffer.s[2]<<8)|((as->ev_buffer.s[3])&0xFF);
   type=as->ev_buffer.s[4];
   processor_id=as->ev_buffer.s[5];
   flags=(as->ev_buffer.s[6]<<24)|(as->ev_buffer.s[7]<<16)|(as->ev_buffer.s[8]<<8)|((as->ev_buffer.s[9])&0xFF);
 
   /*if ev_len > BUF_SIZE then a flag should be put on the AS so that the whole length
    * of the action is skipped, until a mechanism for handling big packets is implemented*/
   if(ev_len>AS_BUF_SIZE){
      LM_WARN("Packet too big (%d)!!! should be skipped"
			  " and an error returned!\n",ev_len);
      return -1;
   }
   if((as->ev_buffer.len<ev_len) || as->ev_buffer.len<4)
      return 0;

   while (as->ev_buffer.len>=ev_len) {
      switch(type){
         case BIND_AC:
            LM_DBG("Processing a BIND action from AS (length=%d): %.*s\n",
                  ev_len,as->name.len,as->name.s);
            process_bind_action(as,processor_id,flags,&as->ev_buffer.s[10],ev_len-10);
            break;
         case UNBIND_AC:
            LM_DBG("Processing a UNBIND action from AS (length=%d): %.*s\n",
                  ev_len,as->name.len,as->name.s);
            process_unbind_action(as,processor_id,flags,&as->ev_buffer.s[10],ev_len-10);
            break;
         default:
            LM_DBG("Unknown action type %d (len=%d,proc=%d,flags=%d)\n",type,ev_len,(int)processor_id,flags);
            return 0;
      }
      memmove(as->ev_buffer.s,&(as->ev_buffer.s[ev_len]),(as->ev_buffer.len)-ev_len);
      (as->ev_buffer.len)-=ev_len;
      if(as->ev_buffer.len>10){
         ev_len=(as->ev_buffer.s[0]<<24)|(as->ev_buffer.s[1]<<16)|(as->ev_buffer.s[2]<<8)|((as->ev_buffer.s[3])&0xFF);
         type=as->ev_buffer.s[4];
         processor_id=as->ev_buffer.s[5];
         flags=(as->ev_buffer.s[6]<<24)|(as->ev_buffer.s[7]<<16)|(as->ev_buffer.s[8]<<8)|((as->ev_buffer.s[9])&0xFF);
      }else{
         return 0;
      }
   }

   return 0;
}


/**
 * processes a BIND event type from the AS.
 * Bind events follow this form:
 * 1:Address Family
 * 1:address length in bytes (16 for ipv6, 4 for ipv4) in NETWORK BYTE ORDER (fortunately, ip_addr struct stores it in NBO)
 * [16|4]:the IP address
 * 1:protocol used (UDP,TCP or TLS);
 * 2:NBO port
 *
 */
int process_bind_action(as_p as,unsigned char processor_id,unsigned int flags,char *payload,int len)
{
   struct socket_info *si,*xxx_listen;
   struct ip_addr my_addr;
   int i,k,proto;
   unsigned short port;
   char buffer[300],*proto_s;
   k=0;
   *buffer=0;
   proto_s="NONE";
   for(i=0;i<MAX_BINDS;i++){
      if(as->bound_processor[i]==0)
	 break;
   }
   if(i==MAX_BINDS){
      LM_ERR("No more bindings allowed. Ignoring bind request for processor %d\n",processor_id);
      return -1;
   }
   memset(&my_addr,0,sizeof(struct ip_addr));
   my_addr.af=payload[k++];
   my_addr.len=payload[k++];
   memcpy(my_addr.u.addr,payload+k,my_addr.len);
   k+=my_addr.len;
   proto=payload[k++];
   memcpy(&port,payload+k,2);
   k+=2;
   port=ntohs(port);
   print_ip_buf(&my_addr,buffer,300);
   switch(proto){
      case PROTO_UDP:
	 proto_s="UDP";
	 xxx_listen=udp_listen;
	 break;
#ifdef USE_TCP
      case PROTO_TCP:
	 proto_s="TCP";
	 xxx_listen=tcp_listen;
	 break;
#endif
#ifdef USE_TLS
      case PROTO_TLS:
	 proto_s="TLS";
	 xxx_listen=tls_listen;
	 break;
#endif
      default:
	 goto error;
   }
   for(si=xxx_listen;si;si=si->next){
      if(my_addr.af==si->address.af && 
	    my_addr.len==si->address.len && 
	    !memcmp(si->address.u.addr,my_addr.u.addr,my_addr.len) && 
	    port == si->port_no){
	 as->binds[i]=si;
	 as->bound_processor[i]=processor_id;
	 as->num_binds++;
	 LM_DBG("AS processor with id: %d bound to %s %s %d\n",processor_id,proto_s,buffer,port);
	 return 0;
      }
   }
error:
   LM_ERR("Cannot bind to %s %s %d !!!\n",proto_s,buffer,port);
   return -1;
}

/**
 * processes a UNBIND event type from the AS.
 * Bind events follow this form:
 * 1:processor_id
 *
 */
int process_unbind_action(as_p as,unsigned char processor_id,unsigned int flags,char *payload,int len)
{
   int i;

   for(i=0;i<as->num_binds;i++){
      if(as->bound_processor[i] == processor_id)
	 break;
   }
   if(i==MAX_BINDS){
      LM_ERR("tried to unbind a processor which is not registered (id=%d)!\n",processor_id);
      return 0;
   }
   as->bound_processor[i]=0;
   as->num_binds--;
   LM_DBG("AS processor un-bound with id: %d\n",processor_id);
   return 0;
}

/**
 * params:
 *   the filedes where the data was received.
 * returns:
 *   0 if this fd should be taken out of the poll_fd array bcause it already has AS name.
 *   fd if an AS was completed (returns the fd of the events socket)
 *   -1 if there was an error
 *   -2 if client disconnected and should be closed and taken outside the poll_fd array
 */
static int handle_unc_as_data(int fd)
{
   int i,j,k,len;
   char *name1;
   struct as_entry *as;
   /*first, we see if the data to read is from any of the uncompleted as's*/
   for(i=0;i<2*MAX_UNC_AS_NR ;i++)
      if(unc_as_t[i].valid && unc_as_t[i].fd==fd)
	 break;
   if(i==2*MAX_UNC_AS_NR){
      LM_ERR("has received an fd which is not in uncompleted AS array\n");
      return -1;
   }
   if(unc_as_t[i].flags & HAS_NAME){/*shouldn't happen, if it has a name, it shouldnt be in fdset[]*/
      LM_WARN("this shouldn't happen\n");
      return 0;/*already have a name, please take me out the uncompleted AS array*/
   }
   LM_DBG("Reading client name\n");

   if(-1==(len=read_name(fd,unc_as_t[i].name,MAX_AS_NAME))){
      /*this guy should be disconnected, it sent an AS_NAME too long*/
      LM_ERR("Bad name passed from fd\n");
      unc_as_t[i].valid=0;
      unc_as_t[i].flags=0;
      return -2;
   }else if(len==-2){
      LM_WARN("client disconnected\n");
      return -2;
   }
   name1=unc_as_t[i].name;

   /* Check the name isn't already taken */
   for(as=as_list;as;as=as->next){
      if(as->name.len==len && !memcmp(name1,as->name.s,len)){
	 if(as->connected){
	    LM_WARN("AppServer trying to connect with a name already taken (%.*s)\n",len,name1);
	    unc_as_t[i].valid=0;
	    unc_as_t[i].flags=0;
	    return -2;
	 }
	 break;
      }
   }
   if (!as) {
      LM_ERR("a client tried to connect which is not declared in config. script(%.*s)\n",len,name1);
      unc_as_t[i].valid=0;
      unc_as_t[i].flags=0;
      return -2;
   }
   unc_as_t[i].flags |= HAS_NAME;
   /* the loop's upper bound, 
    * if 'i' is in the lower part, then look for an unc_as in the upper part*/
   k=(i>=MAX_UNC_AS_NR?MAX_UNC_AS_NR:2*MAX_UNC_AS_NR);
   /* the loop's lower bound */
   for(j=(i>=MAX_UNC_AS_NR?0:MAX_UNC_AS_NR);j<k;j++)
      if(unc_as_t[j].valid && 
	    (unc_as_t[j].flags & HAS_NAME) && 
	    !strcmp(unc_as_t[i].name,unc_as_t[j].name))
	 break;
   LM_INFO("Fantastic, we have a new client: %s\n",unc_as_t[i].name);
   if(j==k)/* the unc_as peer's socket hasn't been found, just take this one out of fdset because it already has its name */
      return 0;/*take me out from fdset[]*/
   LM_INFO("EUREKA, we have a new completed AS: %s\n",unc_as_t[i].name);
   /* EUREKA ! we have a sweet pair of AS sockets, with the same name !!*/
   if(add_new_as(i<j?i:j,i<j?j:i,as)==-1){
      close(unc_as_t[j].fd);
      close(unc_as_t[i].fd);
      unc_as_t[j].valid=unc_as_t[i].valid=0;
      unc_as_t[j].flags=unc_as_t[i].flags=0;
      return -1;
   }
   unc_as_t[j].valid=unc_as_t[i].valid=0;
   unc_as_t[j].flags=unc_as_t[i].flags=0;
   return unc_as_t[i<j?i:j].fd;
}


/* This bloated function reads the name of the AS from the
 * socket and copies it into dst, then returns strlen(dst)
 * or -1 if error.
 */
static inline int read_name(int sock,char *dst,int dstlen)
{
   int n,namelen;
   namelen=0;
try_again1:
   if((n=read(sock,&namelen,1))<0){
      if(errno==EINTR)
	 goto try_again1;
      else{
	 LM_ERR("trying to read length from fd=%d (%s)\n",sock,strerror(errno));
	 return -1;
      }
   }else if(n==0){
      LM_WARN("uncomplete AS has disconnected before giving its name\n");
      return -2;
   }
   if(namelen>dstlen || namelen==0){
      LM_ERR("name too long to fit in dst (%d > %d)\n",namelen,dstlen);
      return -1;
   }
try_again2:
   if((n=read(sock,dst,namelen))<0){
      if(errno==EINTR)
	 goto try_again2;
      else{
	 LM_ERR("trying to read %d chars into %p from fd=%d (%s)\n",namelen,dst,sock,strerror(errno));
	 return -1;
      }
   }else if(n==0){
      LM_WARN("uncomplete AS has disconnected before giving its name\n");
      return -2;
   }
   dst[namelen]=0;
   return namelen;
}

/* handle new App Server connect. 
 * params:
 * fd:
 * 	fd on which to accept.
 * which:
 * 	if the fd is the event one, which='e', if is action, which='a'
 *
 * TODO: not very reliable, because if someone connects() to one of the serversockets 
 * but not to the other one, then synchronization would be lost, and any subsequent connect
 * attempts would fail (remember, we receive a connect in event[] and wait for a connect in action)
 * the point is, the connects must allways come in pairs, if one comes alone, we lost sync.
 * we should put kind of timeout in connects or something...
 */
static int new_as_connect(int fd,char which)
{
   union sockaddr_union su;
   int sock,i,flags;
   socklen_t su_len;

   su_len = sizeof(union sockaddr_union);
   sock=-1;
again:
   sock=accept(fd, &su.s, &su_len);
   if(sock==-1){
      if(errno==EINTR){
	 goto again;
      }else{
		 LM_ERR("while accepting connection: %s\n", strerror(errno));
	 return -1;
      }
   }
   switch(which){
      case 'e':
	 for(i=0;i<MAX_UNC_AS_NR && unc_as_t[i].valid;i++);
	 if(i==MAX_UNC_AS_NR){
	    LM_WARN("no more uncomplete connections allowed\n");
	    goto error;
	 }
	 unc_as_t[i].fd=sock;
	 unc_as_t[i].valid=1;
	 unc_as_t[i].flags=HAS_FD;
	 memcpy(&unc_as_t[i].su,&su,su_len);
	 break;
      case 'a':
	 for(i=MAX_UNC_AS_NR;(i<(2*MAX_UNC_AS_NR)) && unc_as_t[i].valid;i++);
	 if(i==2*MAX_UNC_AS_NR){
	    LM_WARN("no more uncomplete connections allowed\n");
	    goto error;
	 }
	 unc_as_t[i].fd=sock;
	 unc_as_t[i].valid=1;
	 unc_as_t[i].flags=HAS_FD;
	 memcpy(&unc_as_t[i].su,&su,su_len);
	 break;
      default:
	 break;
   }
   flags=1;
   if ((setsockopt(sock, IPPROTO_TCP , TCP_NODELAY,&flags, sizeof(flags))<0) ){
      LM_WARN("could not disable Nagle: %s\n",
	    strerror(errno));
   }

   return sock;
error:
   if(sock!=-1)
      close(sock);
   return -1;
}

int spawn_action_dispatcher(struct as_entry *the_as)
{
   pid_t pid;
   pid=fork();
   if(pid<0){
      LM_ERR("unable to fork an action dispatcher for %.*s\n",the_as->name.len,the_as->name.s);
      return -1;
   }
   if(pid==0){/*child*/
      my_as = the_as;
      is_dispatcher=0;
      dispatch_actions();
      exit(0);
   }else{
      the_as->u.as.action_pid=pid;
   }
   return 0;
}

