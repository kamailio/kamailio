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

#include <string.h>/*memset*/
#include <errno.h>/*errno*/
#include <unistd.h>/*close(),pipe,fork,pid_t*/
#include <sys/wait.h>/*wait*/
#include <signal.h>/*SIGINT,etc*/

#include "../../sr_module.h"
#include "../../ip_addr.h" /*ip_addr,hostent2ip_addr*/
#include "../../tags.h" /*init_tags*/
#include "../../socket_info.h" /*get_first_socket()*/
#include "../../resolve.h" /*resolvehost*/
#include "../../mem/mem.h" /*pkg_malloc*/
#include "../../mem/shm_mem.h" /*shm_malloc*/
#include "../../dprint.h" /*LM_**/
#include "../../error.h" /*ser_error*/
#include "../../modules/tm/tm_load.h" /*load_tm_api*/
#include "../../modules/tm/h_table.h" /*cell*/
#include "../../modules/tm/t_lookup.h" /*T_UNDEFINED*/
#include "../../cfg/cfg_struct.h"

#include "encode_msg.h" /*encode_msg*/

#include "seas.h"
#include "seas_action.h"
#include "event_dispatcher.h"
#include "statistics.h"/*pstart_stats_server*/
#include "ha.h"
#include "cluster.h"

MODULE_VERSION


/* Exported Functions */
static int w_as_relay_t(struct sip_msg *msg, char *as_name, char *foo);
static int w_as_relay_sl(struct sip_msg *msg, char *as_name, char *foo);

/* Local functions */
static int seas_init(void);
static int seas_child_init(int rank);
static int seas_exit();
static int fixup_as_relay(void** param, int param_no);

/*utility functions*/
static void seas_init_tags();
static inline int is_e2e_ack(struct cell *t,struct sip_msg *msg);

char seas_tags[TOTAG_VALUE_LEN+1];
char *seas_tag_suffix;
char whoami[MAX_WHOAMI_LEN];
int is_dispatcher=0;
extern int sig_flag;

static char *seas_listen_socket=0;
static char *seas_stats_socket=0;

struct ip_addr *seas_listen_ip=0;
unsigned short seas_listen_port=0;

struct as_entry *as_table=0;

struct as_entry *as_list=0;

int write_pipe=0;
int read_pipe=0;

struct seas_functions seas_f;

static cmd_export_t cmds[]=
{
	{"as_relay_t",   (cmd_function)w_as_relay_t,  1,  fixup_as_relay,
			0, REQUEST_ROUTE},
	{"as_relay_sl",  (cmd_function)w_as_relay_sl, 1,  fixup_as_relay,
			0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[]=
{
   {"listen_sockets",PARAM_STRING, &seas_listen_socket},
   {"stats_socket",  PARAM_STRING, &seas_stats_socket},
   {"jain_ping",     PARAM_STRING, &jain_ping_config},
   {"servlet_ping",  PARAM_STRING, &servlet_ping_config},
   {"clusters",      PARAM_STRING, &cluster_cfg},
   {0,0,0}
};

static proc_export_t seas_procs[] = {
	{"SEAS",  0,  0,  0,  1 },
	{0,0,0,0,0}
};

struct module_exports exports= 
{
   "seas",
   DEFAULT_DLFLAGS,
   cmds,        /* exported commands */
   params,      /* exported parameters */
   0,           /* exported statistics */
   0,           /* exported mi commands */
   0,           /* exported module-items (pseudo variables) */
   seas_procs,  /* extra processes */
   seas_init,   /* module initialization function */
   0,           /* response function */
   (destroy_function) seas_exit,   /* module exit function */
   (child_init_function) seas_child_init  /* per-child init function */
};

static int fixup_as_relay(void** param, int param_no)
{
   int len;
   char *parameter;
   struct as_entry **entry,*tmp;

   parameter=(char *)(*param);

   if (param_no!=1)
      return 0;
   len=strlen(parameter);
   
   for (entry=&as_list;*entry;entry=&((*entry)->next)) {
      if (len== (*entry)->name.len && 
	    !memcmp((*entry)->name.s,parameter,len)) {
	 pkg_free(*param);
	 *param=*entry;
	 return 1;
      }
   }
   if (!(*entry)) {
      if (!(*entry=(struct as_entry *)shm_malloc(sizeof(struct as_entry)))) {
	 LM_ERR("no more shm_mem\n");
	 goto error;
      }
      memset(*entry,0,sizeof(struct as_entry));
      if(!((*entry)->name.s=shm_malloc(len))){
	 LM_ERR("no more share mem\n");
	 goto error;
      }
      (*entry)->name.len=len;
      memcpy((*entry)->name.s,parameter,len);
      (*entry)->u.as.name=(*entry)->name;
      (*entry)->u.as.event_fd=(*entry)->u.as.action_fd=-1;
      (*entry)->type=AS_TYPE;
      pkg_free(*param);
      *param=*entry;
   }
   for (tmp=as_list;tmp;tmp=tmp->next)
      LM_DBG("%.*s\n",tmp->name.len,tmp->name.s);
   return 1;
error:
   return -1;
}

/**
 * Sets up signal handlers
 */
void seas_sighandler(int signo)
{
   struct as_entry *as;
   if(is_dispatcher)
      sig_flag=signo;
   switch(signo){
      case SIGPIPE:
	 if(is_dispatcher)
	    return;
	 LM_INFO("%s exiting\n",whoami);
	 if(my_as->u.as.ac_buffer.s){
	    pkg_free(my_as->u.as.ac_buffer.s);
	    my_as->u.as.ac_buffer.s=0;
	 }
	 if(my_as->u.as.action_fd!=-1){
	    close(my_as->u.as.action_fd);
	    my_as->u.as.action_fd=-1;
	 }
	 exit(0);
	 break;
      case SIGCHLD:
	 LM_INFO("Child stopped or terminated\n");
	 break;
      case SIGUSR1:
      case SIGUSR2:
	 LM_DBG("Memory status (pkg):\n");
#ifdef PKG_MALLOC
	 pkg_status();
#endif
	 break;
      case SIGINT:
      case SIGTERM:
	 LM_INFO("INFO: signal %d received\n",signo);
#ifdef PKG_MALLOC
	 pkg_status();
#endif
	 if(is_dispatcher){
	    for (as=as_list;as;as=as->next) {
	       if(as->type==AS_TYPE && as->connected)
		  kill(as->u.as.action_pid,signo);
	    }
	    while(wait(0) > 0);
	    exit(0);
	 }else{
	    LM_INFO("%s exiting\n",whoami);
	    if(my_as && my_as->u.as.ac_buffer.s)
	       pkg_free(my_as->u.as.ac_buffer.s);
	    if(my_as && my_as->u.as.action_fd!=-1)
	       close(my_as->u.as.action_fd);
	    exit(0);
	 }
	 break;
   }
}

/**
 * wrapper for the AS transaction-stateful relay script function.
 *
 */
static int w_as_relay_t(struct sip_msg *msg, char *entry, char *foo)
{
   as_msg_p my_as_ev;
   int new_tran,ret=0,len;
   char *buffer,processor_id;
   struct cell *mycel;
   struct as_entry *as;
   char *msg100="Your call is important to us";
   char *msg500="Server Internal Error!";

   buffer=(char*)0;
   my_as_ev=(as_msg_p)0;

   /**
    * returns <0 on error
    * 1 if (new transaction was created) or if (ACK for locally replied 200 with totag) or if (ACK for code>=300)
    * 0 if it was a retransmission 
    */
   new_tran = seas_f.tmb.t_newtran(msg);
   if(new_tran<0) {
      ret = (ser_error==E_BAD_VIA && reply_to_via) ? 0 : new_tran;
      goto done;
   }
   /*retransmission: script processing should be stopped*/
   if (new_tran==0 && !(msg->REQ_METHOD==METHOD_ACK)){
      ret = 0;
      goto done;
   }
   /*new transaction created, let's pass it to an APP SERVER*/
   if (msg->REQ_METHOD==METHOD_INVITE )
   {
      LM_DBG("new INVITE\n");
      if(!seas_f.tmb.t_reply(msg,100,msg100)){
	 LM_DBG("t_reply (100)\n");
	 goto error;
      }
   }
   as=(struct as_entry *)entry;
   if(!as->connected){
      LM_ERR("app server %.*s not connected\n",as->name.len,as->name.s);
      goto error;
   }
   if(as->type==AS_TYPE){
      if((processor_id=get_processor_id(&msg->rcv,&(as->u.as)))<0){
	 LM_ERR("no processor found for packet dst %s:%d\n",ip_addr2a(&msg->rcv.dst_ip),msg->rcv.dst_port);
	 goto error;
      }
   }else if(as->type==CLUSTER_TYPE){
      LM_ERR("clustering not fully implemented\n");
      return 0;
   }else{
      LM_ERR("unknown type of as (neither cluster nor as)\n");
      return -1;
   }
   LM_DBG("as found ! (%.*s) processor id = %d\n",as->name.len,as->name.s,processor_id);
   if(new_tran==1 && msg->REQ_METHOD==METHOD_ACK){
      LM_DBG("message handled in transaction callbacks. skipping\n");
      ret = 0;
      goto done;
      /* core should forward statelessly (says t_newtran)
      LM_DBG("forwarding statelessly !!!\n");
      if(!(buffer=create_as_event_sl(msg,processor_id,&len,0))){
	 LM_ERR("create_as_event_sl() unable to create event code\n");
	 goto error;
      }
      */
   }else if(!(buffer=create_as_event_t(seas_f.tmb.t_gett(),msg,processor_id,&len,0))){
      LM_ERR("unable to create event code\n");
      goto error;
   }
   if(!(my_as_ev=shm_malloc(sizeof(as_msg_t)))){
      LM_ERR("Out of shared mem!\n");
      goto error;
   }
   my_as_ev->msg = buffer;
   my_as_ev->as = as;
   my_as_ev->type = T_REQ_IN;
   my_as_ev->len = len;
   my_as_ev->transaction=seas_f.tmb.t_gett(); /*does not refcount*/
   if(use_stats && new_tran>0)
      as_relay_stat(seas_f.tmb.t_gett());
again:
   ret=write(write_pipe,&my_as_ev,sizeof(as_msg_p));
   if(ret==-1){
      if(errno==EINTR)
	 goto again;
      else if(errno==EPIPE){
	 LM_ERR("SEAS Event Dispatcher has closed the pipe. Invalidating it !\n");
	 goto error;
	 /** TODO handle this correctly !!!*/
      }
   }
   /* seas_f.tmb.t_setkr(REQ_FWDED); */
   ret=0;
done:
   return ret;
error:
   mycel=seas_f.tmb.t_gett();
   if(mycel && mycel!=T_UNDEFINED){
      if(!seas_f.tmb.t_reply(msg,500,msg500)){
	 LM_ERR("t_reply (500)\n");
      }
   }
   if(my_as_ev)
      shm_free(my_as_ev);
   if(buffer)
      shm_free(buffer);
   return ret;
}


/**
 * wrapper for the AS stateless relay script function.
 *
 */
static int w_as_relay_sl(struct sip_msg *msg, char *as_name, char *foo)
{
   as_msg_p my_as_ev=0;
   int ret=0,len;
   char *buffer=0,processor_id;
   struct as_entry *as;

   as=(struct as_entry *)as_name;

   if(as->type==AS_TYPE){
      if((processor_id=get_processor_id(&msg->rcv,&(as->u.as)))<0){
	 LM_ERR("no processor found for packet with dst port:%d\n",msg->rcv.dst_port);
	 goto error;
      }
   }else if (as->type==CLUSTER_TYPE) {
      LM_ERR("clustering not fully implemented\n");
      goto error;
   }else{
      LM_ERR("unknown type of as\n");
      goto error;
   }

   LM_DBG("as found ! (%.*s) processor id = %d\n",as->name.len,as->name.s,processor_id);
   if(!(buffer=create_as_event_sl(msg,processor_id,&len,0))){
      LM_ERR("unable to create event code\n");
      goto error;
   }
   if(!(my_as_ev=shm_malloc(sizeof(as_msg_t))))
      goto error;
   my_as_ev->msg = buffer;
   my_as_ev->as = as;
   my_as_ev->type = SL_REQ_IN;
   my_as_ev->len = len;
   my_as_ev->transaction=seas_f.tmb.t_gett(); /*does not refcount*/
   if(use_stats)
      as_relay_stat(seas_f.tmb.t_gett());
again:
   ret=write(write_pipe,&my_as_ev,sizeof(as_msg_p));
   if(ret==-1){
      if(errno==EINTR)
	 goto again;
      else if(errno==EPIPE){
	 LM_ERR("SEAS Event Dispatcher has closed the pipe. Invalidating it !\n");
	 return -2;
	 /** TODO handle this correctly !!!*/
      }
   }
   //this shouln't be here, because it will remove the transaction from memory, but
   //if transaction isn't unref'ed iw will be released anyway at t_unref if kr (killreason)==0
   // a wait timer will be put to run with WT_TIME_OUT (5 seconds, within which the AS should respond)      
   // this is a bug !!! I think this is why we lose calls at high load !!
   //t_release(msg, 0, 0);
   /* seas_f.tmb.t_setkr(REQ_FWDED); */

   ret=0;
   return ret;
error:
   if(my_as_ev)
      shm_free(my_as_ev);
   if(buffer)
      shm_free(buffer);
   return ret;
}

/**
 * creates an as_event in shared memory and returns its address or NULL if error.
 * event_length(4) UNSIGNED INT includes the length 4 bytes itself
 * type(1), 
 * flags(4),
 * transport(1).
 * src_ip_len(1), 
 * src_ip(4 or 16), 
 * dst_ip_len(1), 
 * dst_ip(4 or 16), 
 * src_port(2), 
 * dst_port(2), 
 * hash index(4), 
 * label(4), 
 * [cancelled hash_index,label]
 *
 */
char * create_as_event_t(struct cell *t,struct sip_msg *msg,char processor_id,int *evt_len,int flags)
{
   unsigned int i,hash_index,label;
   unsigned short int port;
   unsigned int k,len;
   char *buffer=NULL;
   struct cell *originalT;

   originalT=0;

   if(!(buffer=shm_malloc(ENCODED_MSG_SIZE))){
      LM_ERR("Out Of Memory !!\n");
      return 0;
   }
   *evt_len=0;
   if(t){
      hash_index=t->hash_index;
      label=t->label;
   }else{
      /**seas_f.tmb.t_get_trans_ident(msg,&hash_index,&label); this is bad, because it ref-counts !!!*/
      LM_ERR("no transaction provided...\n");
      goto error;
   }

   k=4;
   /*type*/
   buffer[k++]=(unsigned char)T_REQ_IN;
   /*processor_id*/
   buffer[k++]=(unsigned char)processor_id;
   /*flags*/
   if(is_e2e_ack(t,msg)){
      flags|=E2E_ACK;
   }else if(msg->REQ_METHOD==METHOD_CANCEL){
      LM_DBG("new CANCEL\n");
      originalT=seas_f.tmb.t_lookup_original(msg);
      if(!originalT || originalT==T_UNDEFINED){
	 /** we dont even pass the unknown CANCEL to JAIN*/
	 LM_WARN("CANCEL does not match any existing transaction!!\n");
	 goto error;
      }else{
	 flags|=CANCEL_FOUND;
	 //seas_f.tmb.unref_cell(originalT);
      }
      LM_DBG("Cancelling transaction !!\n");
   }
   flags=htonl(flags);
   memcpy(buffer+k,&flags,4);
   k+=4;
   /*protocol should be UDP,TCP,TLS or whatever*/
   buffer[k++]=(unsigned char)msg->rcv.proto;
   /*src ip len + src ip*/
   len=msg->rcv.src_ip.len;
   buffer[k++]=(unsigned char)len;
   memcpy(buffer+k,&(msg->rcv.src_ip.u),len);
   k+=len;
   /*dst ip len + dst ip*/
   len=msg->rcv.dst_ip.len;
   buffer[k++]=(unsigned char)len;
   memcpy(buffer+k,&(msg->rcv.dst_ip.u),len);
   k+=len;
   /*src port */
   port=htons(msg->rcv.src_port);
   memcpy(buffer+k,&port,2);
   k+=2;
   /*dst port */
   port=htons(msg->rcv.dst_port);
   memcpy(buffer+k,&port,2);
   k+=2;
   /*hash_index*/
   i=htonl(hash_index);
   memcpy(buffer+k,&i,4);
   k+=4;
   /*label (is the collision slot in the hash-table)*/
   i=htonl(label);
   memcpy(buffer+k,&i,4);
   k+=4;
   if(msg->REQ_METHOD==METHOD_CANCEL && originalT){
      LM_DBG("Cancelled transaction: Hash_Index=%d, Label=%d\n",originalT->hash_index,originalT->label);
      /*hash_index*/
      i=htonl(originalT->hash_index);
      memcpy(buffer+k,&i,4);
      k+=4;
      /*label (is the collision slot in the hash-table)*/
      i=htonl(originalT->label);
      memcpy(buffer+k,&i,4);
      k+=4;
   }

   /*length of event (hdr+payload-4), copied at the beginning*/
   if(encode_msg(msg,buffer+k,ENCODED_MSG_SIZE-k)<0){
      LM_ERR("Unable to encode msg\n");
      goto error;
   }
   i = GET_PAY_SIZE(buffer+k);
   k+=i;
   *evt_len=k;
   k=htonl(k);
   memcpy(buffer,&k,4);
   return buffer;
error:
   if(buffer)
      shm_free(buffer);
   return 0;
}


/**
 * creates an as_event in shared memory and returns its address or NULL if error.
 * event_length(4) UNSIGNED INT includes the length 4 bytes itself
 * type(1), 
 * processor_id(4), 
 * flags(4),
 * transport(1).
 * src_ip_len(1), 
 * src_ip(4 or 16), 
 * dst_ip_len(1), 
 * dst_ip(4 or 16), 
 * src_port(2), 
 * dst_port(2), 
 *
 */
char * create_as_event_sl(struct sip_msg *msg,char processor_id,int *evt_len,int flags)
{
   unsigned int i;
   unsigned short int port;
   unsigned int k,len;
   char *buffer=NULL;

   if(!(buffer=shm_malloc(ENCODED_MSG_SIZE))){
      LM_ERR("create_as_event_t Out Of Memory !!\n");
      return 0;
   }
   *evt_len=0;

   /*leave 4 bytes for event length*/
   k=4;
   /*type*/
   buffer[k++]=(unsigned char)SL_REQ_IN;
   /*processor_id*/
   buffer[k++]=(unsigned char)processor_id;
   /*flags*/
   flags=htonl(flags);
   memcpy(buffer+k,&flags,4);
   k+=4;
   /*protocol should be UDP,TCP,TLS or whatever*/
   buffer[k++]=(unsigned char)msg->rcv.proto;
   /*src ip len + src ip*/
   len=msg->rcv.src_ip.len;
   buffer[k++]=(unsigned char)len;
   memcpy(buffer+k,&(msg->rcv.src_ip.u),len);
   k+=len;
   /*dst ip len + dst ip*/
   len=msg->rcv.dst_ip.len;
   buffer[k++]=(unsigned char)len;
   memcpy(buffer+k,&(msg->rcv.dst_ip.u),len);
   k+=len;
   /*src port */
   port=htons(msg->rcv.src_port);
   memcpy(buffer+k,&port,2);
   k+=2;
   /*dst port */
   port=htons(msg->rcv.dst_port);
   memcpy(buffer+k,&port,2);
   k+=2;
   /*length of event (hdr+payload-4), copied at the beginning*/
   if(encode_msg(msg,buffer+k,ENCODED_MSG_SIZE-k)<0){
      LM_ERR("Unable to encode msg\n");
      goto error;
   }
   i = GET_PAY_SIZE(buffer+k);
   k+=i;
   *evt_len=k;
   k=htonl(k);
   memcpy(buffer,&k,4);
   return buffer;
error:
   if(buffer)
      shm_free(buffer);
   return 0;
}


static inline int is_e2e_ack(struct cell *t,struct sip_msg *msg)
{
   if(msg->REQ_METHOD != METHOD_ACK)
      return 0;
   if (t->uas.status<300)
      return 1;
   return 0;
}

/** Initializes seas module. It first parses the listen_sockets parameter
 * which has the form "ip_address[:port]", creates the pipe to
 * communicate with the dispatcher.
 */
static int seas_init(void)
{
   char *p,*port;
   struct hostent *he;
   struct socket_info *si;
   int c_pipe[2],mierr,i;
   /** Populate seas_functions*/
   if (load_tm_api(&seas_f.tmb)!=0) {
      LM_ERR( "can't load TM API\n");
      return -1;
   }
   if(!(seas_f.t_check_orig_trans = find_export("t_check_trans", 0, 0))){
      LM_ERR( "Seas requires transaction module (t_check_trans not found)\n");
      return -1;
   }
   /** Populate seas_functions*/
   c_pipe[0]=c_pipe[1]=-1;
   p=seas_listen_socket;
   port=(char *)0;
   seas_listen_port=5080;
   /*if the seas_listen_socket configuration string is empty, use default values*/
   if(p==NULL || *p==0){
      si=get_first_socket();
      seas_listen_ip=&si->address;
   } else {/*if config string is not empty, then try to find host first, and maybe port..*/
      while(*p){
	 if(*p == ':'){
	    *p=0;
	    port=p+1;
	    break;
	 }
	 p++;
      }
      if(!(he=resolvehost(seas_listen_socket)))
	 goto error;
      if(!(seas_listen_ip=pkg_malloc(sizeof(struct ip_addr))))
	 goto error;
      hostent2ip_addr(seas_listen_ip, he, 0);
      if(port!=(char *)0 && (seas_listen_port=str2s(port,strlen(port),&mierr))==0){
	 LM_ERR("invalid port %s \n",port);
	 goto error;
      }
   }
   memset(unc_as_t,0,2*MAX_UNC_AS_NR*sizeof(struct unc_as));//useless because unc_as_t is in bss?
   if (pipe(c_pipe)==-1) {
      LM_ERR("cannot create pipe!\n");
      goto error;
   }
   read_pipe=c_pipe[0];
   write_pipe=c_pipe[1];
   seas_init_tags();
   if(0>start_stats_server(seas_stats_socket))
      goto error;
   if(0>prepare_ha())
      goto error;
   if(0>parse_cluster_cfg())
      goto error;
   register_procs(1);
	/* add child to update local config framework structures */
	cfg_register_child(1);

   return 0;
error:
   for(i=0;i<2;i++)
      if(c_pipe[i]!=-1)
	 close(c_pipe[i]);
   if(seas_listen_ip!=0)
      pkg_free(seas_listen_ip);
   if(use_stats)
      stop_stats_server();
   return -1;
}


/**Initializes SEAS to-tags
*/
static void seas_init_tags(void)
{
   init_tags(seas_tags, &seas_tag_suffix,"VozTele-Seas/tags",'-');
   LM_DBG("seas_init_tags, seas_tags=%s\n",seas_tags);
}

/**
 * This function initializes each one of the processes spawn by the server.
 * the rank is 1 only when the main process is being initialized, so in that
 * case the function spawns the SEAS process to handle as_events triggered
 * from the other SER processes (executing the script).
 * the new process created, then goes into dispatcher_main_loop(), where
 * it reads() the pipe waiting for events produced by other SER processes.
 */
static int seas_child_init(int rank)
{
   int pid;

   /* only the child 1 will execute this */
   if (rank != PROC_MAIN){
      /* only dispatcher needs to read from the pipe, so close reading fd*/
      /*close(read_pipe);*/
      return 0;
   }
   if ((pid=fork_process(PROC_NOCHLDINIT,"SEAS",0))<0) {
      LM_ERR("forking failed\n");
      return -1;
   }
   if (!pid) {
      /*dispatcher child. we leave writing end open so that new childs spawned
       * by event dispatcher can also write to pipe.. */

		/* initialize the config framework */
		if (cfg_child_init())
			return -1;

      /* close(write_pipe); */
      return dispatcher_main_loop();
   }
   return 0;
}

/* this should close the sockets open to any of the application servers, and
 * send them an EOF event or something that signals that SER is beeing shutdown,
 * so they could do their cleanup, etc.
 */
static int seas_exit(void)
{
   if( seas_listen_ip!=NULL && seas_listen_ip!=&(get_first_socket()->address))
      pkg_free(seas_listen_ip);
   return 0;
}

/**
 * search within a given AS, if any of the registered processors is bound
 * to the receive_info structure passed. If there is one, it returns its 
 * identifier (number between 0 and 128), otherwise it returns -1;
 */
char get_processor_id(struct receive_info *rcv,as_p as)
{
   int i;
   for(i=0;i<MAX_BINDS;i++){
      if(as->bound_processor[i]!=0 &&
	    (rcv->dst_ip.len == as->binds[i]->address.len) &&
	    (rcv->dst_ip.af==as->binds[i]->address.af) &&
	    (!memcmp(rcv->dst_ip.u.addr,as->binds[i]->address.u.addr,rcv->dst_ip.len))/* &&
										       (rcv->dst_port==as->binds[i].dst_port) &&
										       (rcv->proto==as->binds[i].proto)*/)
	 return as->bound_processor[i];
   }
   return -1;
}
