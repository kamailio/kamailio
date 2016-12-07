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
 *
 * History:
 * -------
 * 2008-04-04 added support for local and remote dispaly name in TM dialogs
 *            (by Andrei Pisau <andrei at voice-system dot ro> )
 *
 */

#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <signal.h>
#include <poll.h>
#include <assert.h>/*assert*/

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../pt.h"/*process_count*/
#include "../../ip_addr.h"
#include "../../tags.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../parser/hf.h"
#include "../../parser/parse_fline.h"
#include "../../parser/parser_f.h"/*find_not_quoted*/
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_rr.h"/*parse_rr*/
#include "../../parser/parse_via.h"/*parse_via*/
#include "../../parser/parse_param.h"/*parse_params*/
#include "../../parser/parse_uri.h" /*parse_uri*/
#include "../../parser/msg_parser.h"
#include "encode_msg.h"
#include "../../modules/tm/t_lookup.h"
#include "../../modules/tm/h_table.h"
#include "../../modules/tm/dlg.h"
#include "seas.h"
#include "statistics.h"
#include "seas_action.h"
#include "seas_error.h"
#include "ha.h"

#define MAX_HEADER 1024

#define SPIRAL_HDR "X-WeSIP-SPIRAL: true"
#define SPIRAL_HDR_LEN (sizeof(SPIRAL_HDR)-1)

#define RECORD_ROUTE "Record-Route: "
#define RECORD_ROUTE_LEN (sizeof(RECORD_ROUTE)-1)

#define VIA "Via: "
#define VIA_LEN (sizeof(VIA)-1)

extern char *seas_tag_suffix;
extern char seas_tags[];
pid_t my_parent;
extern int fifo_pid;

static inline struct sip_msg *parse_ac_msg(hdr_flags_t flags,char *start,int len);
static inline void free_sip_msg_lite(struct sip_msg *my_msg);
static inline int calculate_hooks(dlg_t* _d);

static inline int process_input(int fd);
static inline int process_pings(struct ha *the_table);
static inline int ac_jain_pong(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len);
int process_pong(struct ha *the_table,unsigned int seqno);
int print_local_uri(as_p as,char processor_id,char *where,int len);
void uas_e2e_ack_cb(struct cell* t, int type,struct tmcb_params *rcvd_params);


int dispatch_actions(void)
{
   int fd,n,ret,timeout,elapsed_ms;
   static int ktimeout;
   struct pollfd fds[1];
   struct timeval last,now;

   /* now the process_no is set, I delete the pt (process_table) global var,
	* because it confuses LM_*() */
   pt=0;
   fd=my_as->u.as.action_fd;
   fds[0].fd=fd;
   fds[0].events=POLLIN|POLLHUP;
   fds[0].revents=0;
   my_parent=getppid();
   snprintf(whoami,MAX_WHOAMI_LEN,"[%.*s] Action dispatcher",my_as->name.len,my_as->name.s);
   if(jain_ping_timeout && servlet_ping_timeout)
      ktimeout=jain_ping_timeout<servlet_ping_timeout?jain_ping_timeout:servlet_ping_timeout;
   else if(jain_ping_timeout)
      ktimeout=jain_ping_timeout;
   else if(servlet_ping_timeout)
      ktimeout=servlet_ping_timeout;
   /*ac_buffer is pkg_malloc because only this process (action dispatcher) will use it*/
   if((my_as->u.as.ac_buffer.s = pkg_malloc(AS_BUF_SIZE))==0){
      LM_ERR("no more pkg mem\n");
      return -1;
   }
   my_as->u.as.ac_buffer.len=0;
   if(use_ha){
      timeout=ktimeout;
      while(1){
	 gettimeofday(&last,NULL);
	 print_pingtable(&my_as->u.as.jain_pings,-1,1);
	 if(0>(n=poll(fds,1,timeout))){
	    if(errno==EINTR){
	       gettimeofday(&last,NULL);
	       continue;
	    }else if(errno==EBADF){
	       LM_ERR("EBADF !!\n");
	    }else{
	       LM_ERR("on poll\n");
	    }
	 }else if(n==0){/*timeout*/
	    if (0>(ret=process_pings(&my_as->u.as.jain_pings))) {
	       return ret;
	    }
	    timeout=ktimeout;
	 }else{ /*events*/
	    if (0>(ret=process_input(fd))) {
	       return ret;
	    }
	    gettimeofday(&now,NULL);
	    elapsed_ms=((now.tv_sec-last.tv_sec)*1000)+((now.tv_usec-last.tv_usec)/1000);
	    if(elapsed_ms<timeout){
	       timeout-=elapsed_ms;
	    }else{
	       if(0>(ret=process_pings(&my_as->u.as.jain_pings))){
		  return ret;
	       }
	       timeout=ktimeout;
	    }
	 }
	 fds[0].events=POLLIN|POLLHUP;
	 fds[0].revents=0;
      }
   }else{
      do{
	 ret=process_input(fd);
      }while(ret>=0);
   }

   return 0;
}

static inline int process_input(int fd)
{
   int j,k;

   k=AS_BUF_SIZE-(my_as->u.as.ac_buffer.len);
again:
   if(0>(j=read(fd,my_as->u.as.ac_buffer.s+my_as->u.as.ac_buffer.len,k))){
      if(errno==EINTR)
	 goto again;
      LM_ERR("reading data for as %.*s (%s)\n",my_as->name.len,my_as->name.s,strerror(errno));
      return -1;
   }else if(j==0){
      pkg_free(my_as->u.as.ac_buffer.s);
      close(fd);
      LM_ERR("read 0 bytes from AS:%.*s\n",my_as->name.len,my_as->name.s);
      /** we return, so we will exit, so our parent (Event Dispatcher) will receive a sigchld and know
       * it should tear down the corresponding AS
       * what still is not clear is what will happen to events that were put in the pipe...
       */
      return -2;
   }
   (my_as->u.as.ac_buffer.len)+=j;
   LM_DBG("read %d bytes from AS action socket (total = %d)\n",j,my_as->u.as.ac_buffer.len);
   if(use_stats)
      receivedplus();
   if(my_as->u.as.ac_buffer.len>=10){
      process_action(&my_as->u.as);
      LM_DBG("(Action dispatched,buffer.len=%d)\n",my_as->u.as.ac_buffer.len);
   }
   return 0;
}

/**
 * The ha structure (high availability) uses a circular (ring) buffer. A linked
 * list could be used, but it would involve a lot of shm_malloc/free, and this
 * would involve a lot of shm-lock_get/release, which would interfere a lot
 * with all the SER processes. With a this ring buffer, the lock_get/release only
 * involve the SEAS processes.
 * This function scans the ping structures in the buffer, computing the elapsed time
 * from when the ping was sent, so if the ping has timed out, it increases the 
 * timed_out_pings counter. All the timed-out pings are removed from the buffer (the
 * begin index is incremented). Because the pings are added always at the end
 * of the buffer, they will always be ordered in increasing time, so when we find one ping
 * that has not timed out, the following pings will neither be.
 *
 */
static inline int process_pings(struct ha *the_table)
{
   int i,k,elapsed;
   struct ping *tmp;
   struct timeval now;

   tmp=NULL;
   gettimeofday(&now,NULL);
   if(the_table->count==0)
      return 0;
   lock_get(the_table->mutex);
   {
      print_pingtable(the_table,-1,0);
      for(i=0;i<the_table->count;i++){
	 k=(the_table->begin+i)%the_table->size;
	 tmp=the_table->pings+k;
	 elapsed=(now.tv_sec-tmp->sent.tv_sec)*1000+(now.tv_usec-tmp->sent.tv_usec)/1000;
	 if(elapsed>the_table->timeout){
	    LM_DBG("ping timed out %d\n",tmp->id);
	    the_table->timed_out_pings++;
	 }else{
	    the_table->begin=k;
	    the_table->count-=i;
	    break;
	 }
      }
   }
   lock_release(the_table->mutex);
   return 0;
}

/* Because TransactionModule organizes statistics based on process_no, 
 * and process_no are only assigned to SER processes (not to Action dispatchers like us ;)
 * we have to simulate we are the FIFO process, so TM thinks that the transactions WE put
 * are put by the fifo process...
static inline void set_process_no()
{
   int pcnt,i;

   pcnt=process_count();
   for(i=0;i<pcnt;i++){
      if(pt[i].pid==fifo_pid){
	 process_no=i;
	 LM_DBG("Setting fake process_no to %d (fifo pid=%d)\n",i,fifo_pid);
	 return;
      }
   }
   for(i=0;i<pcnt;i++){
      if(!memcmp(pt[i].desc,"unix domain socket server",26)){
	 process_no=i;
	 LM_DBG("Setting fake process_no to %d\n",i);
	 return;
      }
   }
   LM_ERR("unable to fake process_no\n");
}
*/

/**Processes the actions received from the socket.
 * returns
 * 	-1 on error
 * 	0 on success
 */
int process_action(as_p the_as)
{
   unsigned int ac_len;
   unsigned char processor_id,type;
   unsigned int flags;

   ac_len=(the_as->ac_buffer.s[0]<<24)|(the_as->ac_buffer.s[1]<<16)|(the_as->ac_buffer.s[2]<<8)|((the_as->ac_buffer.s[3])&0xFF);
   type=the_as->ac_buffer.s[4];
   processor_id=the_as->ac_buffer.s[5];
   flags=(the_as->ac_buffer.s[6]<<24)|(the_as->ac_buffer.s[7]<<16)|(the_as->ac_buffer.s[8]<<8)|((the_as->ac_buffer.s[9])&0xFF);
   /*yeah, it comes in network byte order*/
   /*if ac_len > BUF_SIZE then a flag should be put on the AS so that the whole length
    * of the action is skipped, until a mechanism for handling big packets is implemented*/
   if(use_stats)
      stats_reply();
   if(ac_len>AS_BUF_SIZE){
      LM_WARN("action too big (%d)!!! should be skipped and"
			  " an error returned!\n",ac_len);
      return -1;
   }
   while (the_as->ac_buffer.len>=ac_len) {
      LM_DBG("Processing action %d bytes long\n",ac_len);
      switch(type){
	 case REPLY_PROV:
	 case REPLY_FIN:
	    LM_DBG("Processing a REPLY action from AS (length=%d): %.*s\n",
				ac_len,the_as->name.len,the_as->name.s);
	    ac_reply(the_as,processor_id,flags,the_as->ac_buffer.s+10,ac_len-10);
	    break;
	 case UAC_REQ:
	    LM_DBG("Processing an UAC REQUEST action from AS (length=%d): %.*s\n",
				ac_len,the_as->name.len,the_as->name.s);
	    ac_uac_req(the_as,processor_id,flags,the_as->ac_buffer.s+10,ac_len-10);
	    break;
	 case AC_CANCEL:
	    LM_DBG("Processing a CANCEL REQUEST action from AS (length=%d): %.*s\n",
				ac_len,the_as->name.len,the_as->name.s);
	    ac_cancel(the_as,processor_id,flags,the_as->ac_buffer.s+10,ac_len-10);
	    break;
	 case SL_MSG:
	    LM_DBG("Processing a STATELESS MESSAGE action from AS (length=%d): %.*s\n",
				ac_len,the_as->name.len,the_as->name.s);
	    ac_sl_msg(the_as,processor_id,flags,the_as->ac_buffer.s+10,ac_len-10);
	    break;
	 case JAIN_PONG:
	    LM_DBG("Processing a PONG\n");
	    ac_jain_pong(the_as,processor_id,flags,the_as->ac_buffer.s+10,ac_len-10);
	    break;
	 default:
	    LM_DBG("Processing a UNKNOWN TYPE action from AS (length=%d): %.*s\n",
				ac_len,the_as->name.len,the_as->name.s);
	    break;
      }
      memmove(the_as->ac_buffer.s,the_as->ac_buffer.s+ac_len,(the_as->ac_buffer.len)-ac_len);
      (the_as->ac_buffer.len)-=ac_len;
      if(the_as->ac_buffer.len>10){
	 ac_len=(the_as->ac_buffer.s[0]<<24)|(the_as->ac_buffer.s[1]<<16)|(the_as->ac_buffer.s[2]<<8)|((the_as->ac_buffer.s[3])&0xFF);
         type=the_as->ac_buffer.s[4];
         processor_id=the_as->ac_buffer.s[5];
         flags=(the_as->ac_buffer.s[6]<<24)|(the_as->ac_buffer.s[7]<<16)|(the_as->ac_buffer.s[8]<<8)|((the_as->ac_buffer.s[9])&0xFF);
      }else{
	 return 0;
      }
   }
   return 0;
}

static inline int ac_jain_pong(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len)
{
   unsigned int seqno;
   int k;
   k=0;
   net2hostL(seqno,action,k);
   process_pong(&the_as->jain_pings,seqno);
   return 0;
}

int process_pong(struct ha *the_table,unsigned int seqno)
{
   int i,k,elapsed;
   struct ping *tmp;
   struct timeval now;

   gettimeofday(&now,NULL);
   tmp=NULL;
   if(the_table->count==0)
      return 0;
   lock_get(the_table->mutex);
   print_pingtable(the_table,-1,0);
   for(i=0;i<the_table->count;i++){
      k=(the_table->begin+i)%the_table->size;
      tmp=the_table->pings+k;
      if(tmp->id == seqno){
	 elapsed=(now.tv_sec-tmp->sent.tv_sec)*1000+(now.tv_usec-tmp->sent.tv_usec)/1000;
	 LM_DBG("Ping-Pong delay: %d (timeout was:%d)\n",elapsed,the_table->timeout);
	 if(elapsed>the_table->timeout){
	    /*if this ping has timed out, all the more-ancient pings will also be
	     * timed out*/
	    the_table->timed_out_pings+=i;
	 }/*anyway, when we find a ping in the table, we remove all the pings that are more
	    ancient (if there are any..)*/
	 the_table->count-=(i+1);
	 the_table->begin=(k+1)%the_table->size;
	 break;
      }
   }
   lock_release(the_table->mutex);
   return 0;
}


/**
 * ac_cancel:
 * @param the_as Application Server structure which sent this action
 * @param action action payload
 * @param len the length of the payload
 * 
 * This function cancels a previously initiated UAC Transaction.
 * it receives the HashIndex and Label of the cell being cancelled
 * and invokes t_cancel_uac from the transactionModule API which 
 * cancels the transaction.
 *
 * Returns: 
 * */

int ac_cancel(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len)
{
   unsigned int ret,cancelled_hashIdx,cancelled_label,i;
   struct sip_msg *my_msg;
   struct as_uac_param *the_param;
   struct cell* t_invite;
   int k,retval,uac_id;
   str headers,body;

   body.s=headers.s=NULL;
   my_msg=NULL;
   the_param=NULL;
   i=k=0;

   net2hostL(uac_id,action,k);

   net2hostL(cancelled_hashIdx,action,k);
   net2hostL(cancelled_label,action,k);

   if(!(headers.s=pkg_malloc(MAX_HEADER))){
      LM_ERR("Out of Memory!!");
      goto error;
   }
   headers.len=0;
   if(!(my_msg=pkg_malloc(sizeof(struct sip_msg)))){
      LM_ERR("out of memory!\n");
      goto error;
   }
   memset(my_msg,0,sizeof(struct sip_msg));
   my_msg->buf=action+k;
   my_msg->len=len-k;
   LM_DBG("Action UAC Message: uac_id:%d processor_id=%d, message:[%.*s]\n",
		   uac_id,processor_id,len-4,&action[4]);
   if(parse_msg(action+k,len-k,my_msg)<0){
      LM_ERR("parsing sip_msg");
      goto error;
   }
   if(my_msg->first_line.type==SIP_REPLY){
      LM_ERR("trying to create a UAC with a SIP response!!\n");
      goto error;
   }
   if(parse_headers(my_msg,HDR_EOH_F,0)==-1){
      LM_ERR("parsing headers\n");
      goto error;
   }
   if(0>(headers.len=extract_allowed_headers(my_msg,1,-1,HDR_CONTENTLENGTH_F|HDR_ROUTE_F|HDR_TO_F|HDR_FROM_F|HDR_CALLID_F|HDR_CSEQ_F,headers.s,MAX_HEADER))) {
      LM_ERR("Unable to extract allowed headers!!\n");
      goto error;
   }
   if(flags & SPIRAL_FLAG){
      memcpy(headers.s+headers.len,SPIRAL_HDR CRLF,SPIRAL_HDR_LEN + CRLF_LEN);
      headers.len+=SPIRAL_HDR_LEN+CRLF_LEN;
      /*headers.s[headers.len]=0;
      fake_uri.s=pkg_malloc(200);
      fake_uri.len=print_local_uri(the_as,processor_id,fake_uri.s,200);

      if(fake_uri.len<0){
	 SLM_ERR("printing local uri\n");
	 goto error;
      }
      my_dlg->hooks.next_hop=&fake_uri;*/
   }

   headers.s[headers.len]=0;

   /*let's get the body*/
   i=(unsigned int)get_content_length(my_msg);
   if(i!=0){
      if(!(body.s=pkg_malloc(i))){
	 LM_ERR("Out of Memory!");
	 goto error;
      }
      memcpy(body.s,get_body(my_msg),i);
      body.len=i;
      LM_DBG("Trying to construct a Sip Request with: body:%d[%s]"
			  " headers:%d[%s]\n", body.len,body.s,headers.len,headers.s);
   }else{
      body.s=NULL;
      body.len=0;
   }

   if(!(the_param=shm_malloc(sizeof(struct as_uac_param)))){
      LM_ERR("no more share memory\n");
      goto error;
   }

	if(seas_f.tmb.t_lookup_ident(&t_invite,cancelled_hashIdx,cancelled_label)<0){
		LM_ERR("failed to t_lookup_ident hash_idx=%d,"
			"label=%d\n", cancelled_hashIdx,cancelled_label);
		goto error;
	}
	seas_f.tmb.unref_cell(t_invite);

   the_param->who=my_as;
   the_param->uac_id=uac_id;
   the_param->processor_id=processor_id;
   the_param->destroy_cb_set=0;
   
   /* registers TMCB_RESPONSE_IN|TMCB_LOCAL_COMPLETED tm callbacks */
   ret=seas_f.tmb.t_cancel_uac(&headers,&body,cancelled_hashIdx,cancelled_label,uac_cb,(void*)the_param);
   if (ret == 0) {
      LM_ERR( "t_cancel_uac failed\n");
      as_action_fail_resp(uac_id,SE_CANCEL,SE_CANCEL_MSG,SE_CANCEL_MSG_LEN);
      goto error;
   }else{
      the_param->label=ret;
   }

	seas_f.tmb.unref_cell(t_invite);
   retval=0;
   goto exit;
error:
   retval = -1;
   if(the_param)
      shm_free(the_param);
exit:
   if(headers.s)
      pkg_free(headers.s);
   if(body.s)
      pkg_free(headers.s);
   if(my_msg){
      if(my_msg->headers)
	 free_hdr_field_lst(my_msg->headers);
      pkg_free(my_msg);
   }
   return retval;
}

int recordroute_diff(struct sip_msg *req,struct sip_msg *resp)
{
   struct hdr_field *hf;
   rr_t *rr1;
   int i,j,k;
   i=j=k=0;
   /* count how many record-route bodies come in the response*/
   /* this does not work, I think because of siblings
   for(hf=resp->record_route;hf;hf=hf->sibling,j=0){
   */
   for(hf=resp->headers;hf;hf=hf->next,j=0){
      if(hf->type != HDR_RECORDROUTE_T)
	 continue;
      if(!hf->parsed){
	 if(0>parse_rr(hf))
	    goto error;
	 j=1;
      }
      for(rr1=hf->parsed;rr1;rr1=rr1->next){
	 i++;
      }
      if(j){
	 free_rr((rr_t**)(void*)&hf->parsed);
	 hf->parsed=NULL;
      }
   }
   /*
   for(hf=req->record_route;hf;hf=hf->sibling,j=0){
      */
   for(hf=req->headers;hf;hf=hf->next,j=0){
      if(hf->type != HDR_RECORDROUTE_T)
	 continue;
      if(!hf->parsed){
	 if(0>parse_rr(hf))
	    goto error;
	 j=1;
      }
      for(rr1=hf->parsed;rr1;rr1=rr1->next){
	 k++;
      }
      if(j){
	 free_rr((rr_t**)(void*)&hf->parsed);
	 hf->parsed=NULL;
      }
   }
   return i-k;
error:
   return -1;
}

int via_diff(struct sip_msg *req,struct sip_msg *resp)
{
   struct hdr_field *hf;
   struct via_body *vb;
   int i,j,k;

   i=j=k=0;
   /* count how many via bodies come in the response*/
   for(hf=resp->h_via1;hf;hf=next_sibling_hdr(hf)){
      if(!hf->parsed){
	 if((vb=pkg_malloc(sizeof(struct via_body)))==0){
	    LM_ERR("Out of mem in via_diff!!\n");
	    return -1;
	 }
	 memset(vb,0,sizeof(struct via_body));
	 if(parse_via(hf->body.s,hf->body.s+hf->body.len+1,vb)==0){
	    LM_ERR("Unable to parse via in via_diff!\n");
	    pkg_free(vb);
	    return -1;
	 }
	 hf->parsed=vb;
	 j=1;
      }
      for(vb=hf->parsed;vb;vb=vb->next){
	 i++;
      }
      if(j){
	 free_via_list((struct via_body*)hf->parsed);
	 hf->parsed=NULL;
	 j=0;
      }
   }
   j=0;
   /* count how many via bodies were in the orig. request*/
   for(hf=req->h_via1;hf;hf=next_sibling_hdr(hf)){
      if(!hf->parsed){
	 if((vb=pkg_malloc(sizeof(struct via_body)))==0){
	    goto error;
	 }
	 memset(vb,0,sizeof(struct via_body));
	 if(parse_via(hf->body.s,hf->body.s+hf->body.len+1,vb)==0){
	    goto error;
	 }
	 hf->parsed=vb;
	 j=1;
      }
      for(vb=hf->parsed;vb;vb=vb->next){
	 k++;
      }
      if(j){
	 free_via_list((struct via_body*)hf->parsed);
	 hf->parsed=NULL;
	 j=0;
      }
   }
   return i-k;
error:
   return -1;
}

static void param_free(void *param)
{
   shm_free(param);
}
/**
 * ac_reply: UAS transaction Reply action. It replies to an incoming request with a response.
 * @param the_as The App Server that sent this action.
 * @param action action
 * @param len length
 *
 * function description
 *
 * Returns: what
 */
int ac_reply(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len)
{
   unsigned int hash_index,label,contentlength;
   struct cell *c=NULL;
   struct sip_msg *my_msg;
   struct to_body *tb;
   str new_header,body,totag;
   char *ttag;
   int i,k,retval;
   static char headers[MAX_HEADER];
   struct as_uac_param *the_param;

   contentlength=0;
   ttag=NULL;
   my_msg=NULL;
   i=k=0;
   the_param=NULL;

   net2hostL(hash_index,action,k);
   net2hostL(label,action,k);

   if(seas_f.tmb.t_lookup_ident(&c,hash_index,label)<0){
      LM_ERR("Failed to t_lookup_ident hash_idx=%d,label=%d\n",hash_index,label);
      goto error;
   }
   if(use_stats)
      action_stat(c);
   if(c->uas.status>=200){
      LM_ERR("ac_reply: trying to reply to a \"%.*s\" transaction"
	    "that is already in completed state\n",REQ_LINE(c->uas.request).method.len,REQ_LINE(c->uas.request).method.s);
      goto error;
   }
   if (!(my_msg=parse_ac_msg(HDR_EOH_F,action+k,len-k))) {
      LM_ERR("Failed to parse_ac_msg hash_idx=%d,label=%d\n",hash_index,label);
      goto error;
   }
   tb=(struct to_body*)my_msg->to->parsed;
   if(tb->tag_value.s && tb->tag_value.len){
      totag=tb->tag_value;
   }else{
      totag.s=NULL;
      totag.len=0;
      /*if(!(ttag=pkg_malloc(TOTAG_VALUE_LEN))){
	 LM_ERR("Out of memory !!!\n");
	 goto error;
      }
      totag.s=ttag;
      calc_crc_suffix(c->uas.request,seas_tag_suffix);
      LM_DBG("seas_tags = %.*s\n",TOTAG_VALUE_LEN,seas_tags);
      memcpy(totag.s,seas_tags,TOTAG_VALUE_LEN);
      totag.len=TOTAG_VALUE_LEN;*/
   }
   LM_DBG("Using totag=[%.*s]\n",totag.len,totag.s);
   if(my_msg->content_length)
      contentlength=(unsigned int)(long)my_msg->content_length->parsed;
   if(0>(i=recordroute_diff(c->uas.request,my_msg))){/*not likely..*/
      LM_DBG("Seems that request had more RecordRoutes than response...\n");
      /* This prevents host->proxy->host from working. TODO review recordroute_diff code.
      goto error; */
   }else
      LM_DBG("Recordroute Diff = %d\n",i);

   if(0>(i=extract_allowed_headers(my_msg,0,i,HDR_VIA_F|HDR_TO_F|HDR_FROM_F|HDR_CSEQ_F|HDR_CALLID_F|HDR_CONTENTLENGTH_F,headers,MAX_HEADER))){
      LM_ERR("ac_reply() filtering headers !\n");
      goto error;
   }
   headers[i]=0;
   new_header.s=headers;
   new_header.len=i;

   /* If it is INVITE and response is success (>=200 && <300), we mark it as local so that
    * SER does NOT retransmit the final response (by default, SER retransmit local UAS final
    * responses...*/
   if(is_invite(c) && my_msg->first_line.u.reply.statuscode>=200 && my_msg->first_line.u.reply.statuscode<300){
      c->flags |= T_IS_LOCAL_FLAG;
      if(!(the_param=shm_malloc(sizeof(struct as_uac_param)))){
         LM_ERR("no more share memory\n");
         goto error;
      }
      the_param->processor_id=processor_id;
      the_param->who=my_as;
      the_param->destroy_cb_set=0;
      if (seas_f.tmb.register_tmcb( 0, c, TMCB_E2EACK_IN,uas_e2e_ack_cb, the_param,&param_free)<=0) {
         LM_ERR("cannot register additional callbacks\n");
         goto error;
      }
   }
   /*WARNING casting unsigned int to int*/
   body.len=contentlength;
   body.s=get_body(my_msg);

   LM_DBG("Trying to construct a SipReply with: ReasonPhrase:[%.*s] body:[%.*s] headers:[%.*s] totag:[%.*s]\n",\
	 my_msg->first_line.u.reply.reason.len,my_msg->first_line.u.reply.reason.s,\
	 body.len,body.s,new_header.len,new_header.s,totag.len,totag.s);
   /* t_reply_with_body un-ref-counts the transaction, so dont use it anymore*/
   if(seas_f.tmb.t_reply_with_body(c,my_msg->first_line.u.reply.statuscode,&(my_msg->first_line.u.reply.reason),&body,&new_header,&totag)<0){
      LM_ERR("Failed to t_reply\n");
      goto error;
   }
   retval=0;
   goto exit;
error:
   retval = -1;
   if(c)
      seas_f.tmb.unref_cell(c);
   if(the_param)
      shm_free(the_param);
exit:
   if(ttag)
      pkg_free(ttag);
   if(my_msg){
      free_sip_msg_lite(my_msg);
      pkg_free(my_msg);
   }
   return retval;
}

static inline struct sip_msg *parse_ac_msg(hdr_flags_t flags,char *start,int len)
{
   struct sip_msg *my_msg;
   my_msg=NULL;
   if(!(my_msg=pkg_malloc(sizeof(struct sip_msg)))){
      LM_ERR("ac_reply: out of memory!\n");
      goto error;
   }
   memset(my_msg,0,sizeof(struct sip_msg));
   my_msg->buf=start;
   my_msg->len=len;
   LM_DBG("Action Message:[%.*s]\n",len,start);
   if(0>parse_msg(start,len,my_msg)){
      LM_ERR("parse_ac_msg: parsing sip_msg");
      goto error;
   }
   if(0>parse_headers(my_msg,flags,0)){
      LM_ERR("parse_ac_msg: parsing headers\n");
      goto error;
   }
   return my_msg;
error:
   if(my_msg){
      free_sip_msg_lite(my_msg);
      pkg_free(my_msg);
   }
   return NULL;
}

/* Actions are composed as follows:
 * (the action length and type as always= 5 bytes)
 *
 * TODO performance speedup: instead of using
 * dynamically allocated memory for headers,body,totag,reason and my_msg
 * use static buffers.
 *
 */
int ac_sl_msg(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len)
{
   struct sip_msg *my_msg;
   str *uri;
   struct proxy_l *proxy;
   rr_t *my_route;
   int k,retval;
   //enum sip_protos proto;

   my_msg=NULL;
   k=0;

   proxy=0;

   if(!(my_msg = parse_ac_msg(HDR_EOH_F,action+k,len-k))){
      LM_ERR("out of memory!\n");
      goto error;
   }
   if(my_msg->first_line.type == SIP_REQUEST)
      LM_DBG("forwarding request:\"%.*s\" statelessly \n",my_msg->first_line.u.request.method.len+1+\
	    my_msg->first_line.u.request.uri.len,my_msg->first_line.u.request.method.s);
   else
      LM_DBG("forwarding reply:\"%.*s\" statelessly \n",my_msg->first_line.u.reply.status.len+1+\
	    my_msg->first_line.u.reply.reason.len,my_msg->first_line.u.reply.status.s);

   if (my_msg->route) {
      if (parse_rr(my_msg->route) < 0) {
	 LM_ERR( "Error while parsing Route body\n");
	 goto error;
      }
      my_route = (rr_t*)my_msg->route->parsed;
      uri=&(my_route->nameaddr.uri);
   }else{
      uri = GET_RURI(my_msg);
   }
   set_force_socket(my_msg, grep_sock_info(&my_msg->via1->host,
                                            my_msg->via1->port,
                                            my_msg->via1->proto) );
   /* or also could be:
      my_msg->force_send_socket=the_as->binds[processor_id].bind_address;
      not sure which is better...
      */
   /*proxy=uri2proxy(uri,PROTO_NONE);
   if (proxy==0) {
      LM_ERR("unable to create proxy from URI \n");
      goto error;
   }
   proto=proxy->proto;
   */
   //TODO my_msg->recvd
   if(0>forward_sl_request(my_msg,uri,PROTO_NONE))
      goto error;
   retval=0;
   goto exit;
error:
   retval = -1;
exit:
   if(proxy){
      free_proxy(proxy);
      pkg_free(proxy);
   }
   if(my_msg){
      free_sip_msg_lite(my_msg);
      pkg_free(my_msg);
   }
   return retval;
}

static inline void free_sip_msg_lite(struct sip_msg *my_msg)
{
   if(my_msg){
      /**should do the same as in free_sip_msg() but w/o freeing my_msg->buf*/
      if (my_msg->new_uri.s) { pkg_free(my_msg->new_uri.s); my_msg->new_uri.len=0; }
      if (my_msg->dst_uri.s) { pkg_free(my_msg->dst_uri.s); my_msg->dst_uri.len=0; }
      if (my_msg->path_vec.s) { pkg_free(my_msg->path_vec.s);my_msg->path_vec.len=0; }
      if (my_msg->headers)     free_hdr_field_lst(my_msg->headers);
      if (my_msg->add_rm)      free_lump_list(my_msg->add_rm);
      if (my_msg->body_lumps)  free_lump_list(my_msg->body_lumps);
      /* this is not in lump_struct.h, and anyhow it's not supposed to be any lumps
       * in our messages... or is it?
      if (my_msg->reply_lump)   free_reply_lump(my_msg->reply_lump);
      */
   }
}

int forward_sl_request(struct sip_msg *msg,str *uri,int proto)
{
	struct dest_info dst;
	int ret;

	ret = -1;

#ifdef USE_DNS_FAILOVER
        if ((uri2dst(NULL,&dst, msg,  uri, proto)==0) || (dst.send_sock==0))
#else
        if ((uri2dst(&dst, msg,  uri, proto)==0) || (dst.send_sock==0))
#endif 
        {
		LOG(L_ERR, "forward_sl_request: no socket found\n");
		return -1;
	}

        LM_DBG("Sending:\n%.*s.\n", (int)msg->len,msg->buf);
        if (msg_send(&dst, msg->buf,msg->len)<0){
           LM_ERR("ERROR:seas:forward_sl_request: Error sending message !!\n");
           return -1;
        }
	return ret;
}



/*Actions are composed as follows:
 * (the action length and type as always= 5 bytes)
 * 4:uac_id
 *
 * int request(str* method, str* req_uri, str* to, str* from, str* headers, str* body, transaction_cb c, void* cp)
 * TODO performance speedup: instead of using
 * dynamically allocated memory for headers,body,totag,reason and my_msg
 * use static buffers.
 *
 */
int ac_uac_req(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len)
{
   unsigned int cseq;
   char err_buf[MAX_REASON_LEN];
   struct sip_msg *my_msg;
   struct to_body *fb,*tb;
   struct cseq_body *cseqb;
   struct as_uac_param *the_param;
   dlg_t *my_dlg;
   int k,retval,uac_id,sip_error,ret,err_ret;
   long clen;
   str headers,body,fake_uri;
   uac_req_t uac_r;

   headers.s=body.s=fake_uri.s=NULL;
   my_dlg=NULL;
   my_msg=NULL;
   the_param=NULL;
   k=clen=0;

   net2hostL(uac_id,action,k);

   if(!(headers.s=pkg_malloc(MAX_HEADER))){
      LM_ERR("Out of Memory!!");
      goto error;
   }
   headers.len=0;
   LM_DBG("Action UAC Message: uac_id:%d processor_id=%d\n",uac_id,processor_id);
   if (!(my_msg = parse_ac_msg(HDR_EOH_F,action+k,len-k))) {
      LM_ERR("out of memory!\n");
      goto error;
   }
   if(my_msg->first_line.type==SIP_REPLY){
      LM_ERR("trying to create a UAC with a SIP response!!\n");
      goto error;
   }
   if(parse_headers(my_msg,HDR_EOH_F,0)==-1){
      LM_ERR("ERROR:seas:ac_uac_req:parsing headers\n");
      goto error;
   }
   if(parse_from_header(my_msg)<0){
      LM_ERR("parsing from header ! \n");
      goto error;
   }
   if(check_transaction_quadruple(my_msg)==0){
      as_action_fail_resp(uac_id,SE_UAC,"Headers missing (to,from,call-id,cseq)?",0);
      LM_ERR("Headers missing (to,from,call-id,cseq)?");
      goto error;
   }
   if(!(get_from(my_msg)) || !(get_from(my_msg)->tag_value.s) || 
	 !(get_from(my_msg)->tag_value.len)){
      as_action_fail_resp(uac_id,SE_UAC,"From tag missing",0);
      LM_ERR("From tag missing");
      goto error;
   }
   fb=my_msg->from->parsed;
   tb=my_msg->to->parsed;
   cseqb=my_msg->cseq->parsed;
   if(0!=(str2int(&cseqb->number,&cseq))){
      LM_DBG("unable to parse CSeq\n");
      goto error;
   }
   if(my_msg->first_line.u.request.method_value != METHOD_ACK &&
	 my_msg->first_line.u.request.method_value != METHOD_CANCEL) {
      /** we trick req_within */
      cseq--;
   }
   if(seas_f.tmb.new_dlg_uac(&(my_msg->callid->body),&(fb->tag_value),cseq,\
	    &(fb->uri),&(tb->uri),&my_dlg) < 0) {
      as_action_fail_resp(uac_id,SE_UAC,"Error creating new dialog",0);
      LM_ERR("Error while creating new dialog\n");
      goto error;
   }
   if(seas_f.tmb.dlg_add_extra(my_dlg,&(fb->display),&(tb->display)) < 0 ) {
      as_action_fail_resp(uac_id,SE_UAC,
         "Error adding the display names to the new dialog",0);
      LM_ERR("failed to add display names to the new dialog\n");
      goto error;
   }

   if(tb->tag_value.s && tb->tag_value.len)
      shm_str_dup(&my_dlg->id.rem_tag,&tb->tag_value);
   /**Awful hack: to be able to set our own CSeq, from_tag and call-ID we have
    * to use req_within instead of req_outside (it sets it's own CSeq,Call-ID
    * and ftag), so we have to simulate that the dialog is already in completed
    * state so...
    */
   server_signature=0;
   my_dlg->state = DLG_CONFIRMED;
   if(0>(headers.len=extract_allowed_headers(my_msg,1,-1,HDR_CONTENTLENGTH_F|HDR_ROUTE_F|HDR_TO_F|HDR_FROM_F|HDR_CALLID_F|HDR_CSEQ_F,headers.s,MAX_HEADER))) {
      LM_ERR("Unable to extract allowed headers!!\n");
      goto error;
   }
   headers.s[headers.len]=0;
   /*let's get the body*/
   if(my_msg->content_length)
      clen=(long)get_content_length(my_msg);
   if(clen!=0){
      if(!(body.s=pkg_malloc(clen))){
	 LM_ERR("Out of Memory!");
	 goto error;
      }
      memcpy(body.s,get_body(my_msg),clen);
      body.len=clen;
      body.s[clen]=0;
      LM_DBG("Trying to construct a Sip Request with: body:%d[%.*s] headers:%d[%.*s]\n",\
	    body.len,body.len,body.s,headers.len,headers.len,headers.s);
      /*t_reply_with_body un-ref-counts the transaction, so dont use it anymore*/
   }else{
      body.s=NULL;
      body.len=0;
   }
   /*Now... create the UAC !!
    * it would be great to know the hash_index and the label that have been assigned
    * to our newly created cell, but t_uac does not leave any way for us to know...
    * only that when that transaction transitions its state (ie. a response is received,
    * a timeout is reached, etc...) the callback will be called with the given parameter.
    *
    * So the only way we have to know who we are, is passing as a parameter a structure with
    * 2 pointers: one to the app_server and the other, the identifier of the UAC (uac_id).
    *
    */
   if(!(the_param=shm_malloc(sizeof(struct as_uac_param)))){
      LM_ERR("out of shared memory\n");
      goto error;
   }
   the_param->who=my_as;
   the_param->uac_id=uac_id;
   the_param->processor_id=processor_id;
   the_param->destroy_cb_set=0;

   shm_str_dup(&my_dlg->rem_target,&my_msg->first_line.u.request.uri);

   if (my_msg->route) {
      if (parse_rr(my_msg->route) < 0) {
	 LM_ERR( "Error while parsing Route body\n");
	 goto error;
      }
      /* TODO route_set should be a shm copy of my_msg->route->parsed */
      my_dlg->route_set=(rr_t*)my_msg->route->parsed;
      /** this SHOULD be:
       shm_duplicate_rr(&my_dlg->route_set,my_msg->route->parsed);
       * but it will last more...
       */
   }
   calculate_hooks(my_dlg);
   if(flags & SPIRAL_FLAG){
      memcpy(headers.s+headers.len,SPIRAL_HDR CRLF,SPIRAL_HDR_LEN + CRLF_LEN);
      headers.len+=SPIRAL_HDR_LEN+CRLF_LEN;
      headers.s[headers.len]=0;
      fake_uri.s=pkg_malloc(200);
      fake_uri.len=print_local_uri(the_as,processor_id,fake_uri.s,200);

      if(fake_uri.len<0){
	 LM_ERR("printing local uri\n");
	 goto error;
      }
      my_dlg->hooks.next_hop=&fake_uri;
   }
   /* Kamailio and OpenSIPs seem to have diverged quite a bit on flags and events
      notified to UACs. Let's see if kamailio gets it right by now, if not
      this is a TODO: check PASS_PROVISIONAL
      my_dlg->T_flags=T_NO_AUTO_ACK|T_PASS_PROVISIONAL_FLAG ;
      this is the same as (TMCB_DONT_ACK|TMCB_LOCAL_RESPONSE_OUT) in Kamailio
   */

   set_uac_req(&uac_r, &(my_msg->first_line.u.request.method), &headers,
		   &body, my_dlg,TMCB_DONT_ACK|TMCB_LOCAL_RESPONSE_OUT, uac_cb,
		   (void*)the_param);

   ret=seas_f.tmb.t_request_within(&uac_r);

   /** now undo all the fakes we have put in my_dlg*/
   /*because my_dlg->route_set should be shm but we fake it (its pkg_mem)*/
   my_dlg->route_set=(rr_t *)0;
   if (ret < 0) {
      err_ret = err2reason_phrase(ret,&sip_error,err_buf, sizeof(err_buf), "SEAS/UAC");
      LM_ERR("failed to send the [%.*s] request\n",uac_r.method->len,uac_r.method->s);
      LM_ERR("Error on request_within %s\n",err_buf );
      if(err_ret > 0) {
	 as_action_fail_resp(uac_id,ret,err_buf,0);
      }else{
	 as_action_fail_resp(uac_id,E_UNSPEC,"500 SEAS/UAC error",0);
      }
      goto error;
   }
   retval=0;
   goto exit;
error:
   retval = -1;
   if(the_param)
      shm_free(the_param);
exit:
   seas_f.tmb.free_dlg(my_dlg);
   if(headers.s)
      pkg_free(headers.s);
   if(body.s)
      pkg_free(body.s);
   if(fake_uri.s)
      pkg_free(fake_uri.s);
   if(my_msg){
      if(my_msg->headers)
	 free_hdr_field_lst(my_msg->headers);
      pkg_free(my_msg);
   }
   return retval;
}

/**
 * len MUST be >0
 */
int print_local_uri(as_p as,char processor_id,char *where,int len)
{
   int i;
   struct socket_info *si;
   str proto;
   proto.s=NULL;
   proto.len=0;
   for(i=0;i<MAX_BINDS;i++){
      if(as->bound_processor[i]==processor_id)
	 break;
   }
   if(i==MAX_BINDS){
      LM_DBG("processor ID not found\n");
      return -1;
   }
   si=as->binds[i];
   switch(si->proto){
      case PROTO_UDP:
	 proto.s="";
	 proto.len=0;
	 break;
      case PROTO_TCP:
	 proto.s=TRANSPORT_PARAM "TCP";
	 proto.len=TRANSPORT_PARAM_LEN + 3;
	 break;
      case PROTO_TLS:
	 proto.s=TRANSPORT_PARAM "TLS";
	 proto.len=TRANSPORT_PARAM_LEN + 3;
	 break;
      case PROTO_SCTP:
	 proto.s=TRANSPORT_PARAM "SCTP";
	 proto.len=TRANSPORT_PARAM_LEN + 4;
	 break;
      case PROTO_WS:
      case PROTO_WSS:
	 proto.s=TRANSPORT_PARAM "WS";
	 proto.len=TRANSPORT_PARAM_LEN + 2;
	 break;
   }
   switch(si->address.af){
      case AF_INET:
	 i=snprintf(where,len,"sip:%d.%d.%d.%d:%u%.*s",si->address.u.addr[0],si->address.u.addr[1],\
	       si->address.u.addr[2],si->address.u.addr[3],si->port_no,proto.len,proto.s);
	 break;
      case AF_INET6:
	 i=snprintf(where,len,"sip:[%x:%x:%x:%x:%x:%x:%x:%x]:%u%.*s", htons(si->address.u.addr16[0]), htons(si->address.u.addr16[1]),\
	       htons(si->address.u.addr16[2]), htons(si->address.u.addr16[3]), htons(si->address.u.addr16[4]), htons(si->address.u.addr16[5]),\
	       htons(si->address.u.addr16[6]), htons(si->address.u.addr16[7]),si->port_no,proto.len,proto.s);
	 break;
      default:
	 LM_ERR("address family unknown\n");
	 return -1;
   }
   if(i>len){
      LM_ERR("Output was truncated!!\n");
      return -1;
   }else if(i<0){
      LM_ERR("Error on snprintf\n");
      return i;
   }
   return i;
}

/* !!! COPIED FROM MODULES/TM  !!
 * This function skips name part
 * uri parsed by parse_contact must be used
 * (the uri must not contain any leading or
 *  trailing part and if angle bracket were
 *  used, right angle bracket must be the
 *  last character in the string)
 *
 * _s will be modified so it should be a tmp
 * copy
 */
void get_raw_uri(str* _s)
{
        char* aq;

        if (_s->s[_s->len - 1] == '>') {
                aq = find_not_quoted(_s, '<');
                _s->len -= aq - _s->s + 2;
                _s->s = aq + 1;
        }
}
/* !!! COPIED FROM MODULES/TM  !!
 * Calculate dialog hooks
 *
 * This is copied from modules/tm/dlg.c
 *
 * Maybe a reference to the original function in TM
 * could be reached via handlers or whatever...
 */
static inline int calculate_hooks(dlg_t* _d)
{
   str* uri;
   struct sip_uri puri;

   if (_d->route_set) {
      uri = &_d->route_set->nameaddr.uri;
      if (parse_uri(uri->s, uri->len, &puri) < 0) {
         LM_ERR( "Error while parsing URI\n");
         return -1;
      }

      if (puri.lr.s) {
         if (_d->rem_target.s) _d->hooks.request_uri = &_d->rem_target;
         else _d->hooks.request_uri = &_d->rem_uri;
	 _d->hooks.next_hop = &_d->route_set->nameaddr.uri;
	 _d->hooks.first_route = _d->route_set;
      } else {
         _d->hooks.request_uri = &_d->route_set->nameaddr.uri;
         _d->hooks.next_hop = _d->hooks.request_uri;
         _d->hooks.first_route = _d->route_set->next;
         _d->hooks.last_route = &_d->rem_target;
      }
   } else {
      if (_d->rem_target.s) _d->hooks.request_uri = &_d->rem_target;
      else _d->hooks.request_uri = &_d->rem_uri;
      _d->hooks.next_hop = _d->hooks.request_uri;
   }

   if ((_d->hooks.request_uri) && (_d->hooks.request_uri->s) && (_d->hooks.request_uri->len)) {
      _d->hooks.ru.s = _d->hooks.request_uri->s;
      _d->hooks.ru.len = _d->hooks.request_uri->len;
      _d->hooks.request_uri = &_d->hooks.ru;
      get_raw_uri(_d->hooks.request_uri);
   }
   if ((_d->hooks.next_hop) && (_d->hooks.next_hop->s) && (_d->hooks.next_hop->len)) {
      _d->hooks.nh.s = _d->hooks.next_hop->s;
      _d->hooks.nh.len = _d->hooks.next_hop->len;
      _d->hooks.next_hop = &_d->hooks.nh;
      get_raw_uri(_d->hooks.next_hop);
   }

   return 0;
}

/**
 * Strips the "<strip_top_vias>" topmost via headers.
 * Leaves only the topmost "<allow_top_routes>" Record-Route headers.
 *
 */
int extract_allowed_headers(struct sip_msg *my_msg,int strip_top_vias,int allow_top_Rroutes,hdr_flags_t forbidden_hdrs,char *headers,int headers_len)
{
   struct hdr_field *hf;
   rr_t *rb;
   struct via_body *vb;
   int len,k,rtcnt,i;

   len=0;
   rtcnt=allow_top_Rroutes;
   rb=NULL;
   vb=NULL;

   for(hf=my_msg->headers;hf;hf=hf->next){
      if(forbidden_hdrs & HDR_T2F(hf->type)){
	 LM_DBG("Skipping header (%.*s)\n",hf->name.len,hf->name.s);
	 continue;
      }else if(hf->type==HDR_VIA_T && strip_top_vias > 0){
	 /** All vias MUST be parsed !!*/
	 for(i=0,vb=hf->parsed;vb;vb=vb->next,i++);
	 if(i<=strip_top_vias){
	    LM_DBG("Stripping vias [%.*s]\n",hf->len,hf->name.s);
	    /** skip this via header*/
	    strip_top_vias-=i;
	 }else{
	    assert(i>1);
	    vb=hf->parsed;
	    while(strip_top_vias--)
	       vb=vb->next;
	    k= (hf->name.s + hf->len) - vb->name.s;
	    LM_DBG("Stripping vias [%.*s]\n",(int)(vb->name.s-hf->name.s),
		  hf->name.s);
	    if(k+VIA_LEN<headers_len){
	       memcpy(headers+len,VIA,VIA_LEN);
	       len+=VIA_LEN;
	       memcpy(headers+len,vb->name.s,k);
	       len+=k;
	    }else{
	       LM_ERR("Out Of Space !!\n");
	       goto error;
	    }
	 }
      }else if(hf->type==HDR_RECORDROUTE_T && rtcnt>=0){
	 if(rtcnt==0)
	    continue;
	 if(!hf->parsed && 0>parse_rr(hf)){
	    LM_ERR("parsing Record-Route:\"%.*s\"\n",hf->body.len,hf->body.s);
	    goto error;
	 }
	 for(i=0,rb=hf->parsed;rb;rb=rb->next,i++);
	 if(i<=rtcnt){
	    if((len+hf->len)<headers_len){
	       LM_DBG("Allowing RecordRoute [%.*s]\n",hf->len,hf->name.s);
	       memcpy(headers+len,hf->name.s,hf->len);
	       len+=hf->len;
	    }else{
	       LM_ERR("Unable to keep recordroute (not enough space left in headers) Discarding \"%.*s\" \n",hf->name.len,hf->name.s);
	       goto error;
	    }
	    /** is this dangerous ? because the rtcnt is the control variable for this conditional 'if'
	     * so if I change rtcnt value in one of the statements... what then ??? */
	    rtcnt-=i;
	 }else{
	    assert(rtcnt>0);
	    rb=hf->parsed;
	    while(--rtcnt)
	       rb=rb->next;
	    k= (((rb->nameaddr.name.s) + rb->len)-hf->name.s) ;
	    if(len+k+CRLF_LEN<headers_len){
	       memcpy(headers+len,hf->name.s,k);
	       LM_DBG("Allowing RecordRoute [%.*s\r\n]\n",k,hf->name.s);
	       len+=k;
	       memcpy(headers+len,CRLF,CRLF_LEN);
	       len+=CRLF_LEN;
	    }else{
	       LM_ERR("Out Of Space !!\n");
	       goto error;
	    }
	 }
	 if(hf->parsed){
	    free_rr((rr_t **)(void*)(&hf->parsed));
	    hf->parsed=NULL;
	 }
      }else{
	 if((len+hf->len)<headers_len){
	    memcpy(headers+len,hf->name.s,hf->len);
	    len+=hf->len;
	 }else{
	    LM_WARN("Too many headers. Discarding \"%.*s\" \n",
				hf->name.len,hf->name.s);
	 }
      }
   }/*for*/
   return len;
error:
   return -1;
}


/**
 * ERROR action responses are composed of:
 * 4: the length of the event
 * 1: the event type (AC_RES_FAIL)
 * 4: NBO of the uac-action-request identification (uac_id)
 * 4: the sip_error code in NBO.
 * 1: (unsigned) the length of the string.
 * N: the string
 *
 */
int as_action_fail_resp(int uac_id,int sip_error,char *err_buf,int i)
{
   char msg[14+MAX_REASON_LEN];
   int k, ev_len;
   k=4;
   if(i==0)
      i=strlen(err_buf);
   if(i>MAX_REASON_LEN){
      LM_ERR("Error Reason bigger than MAX_REASON_LEN\n");
      return -1;
   }
   msg[k++]=AC_RES_FAIL;
   uac_id=htonl(uac_id);
   memcpy(msg+k,&uac_id,4);
   k+=4;
   sip_error=htonl(sip_error);
   memcpy(msg+k,&sip_error,4);
   k+=4;
   msg[k++]=(char)(unsigned char)i;
   memcpy(msg+k,err_buf,i);
   k+=i;
   ev_len=htonl(k);
   memcpy(msg,&ev_len,4);
   if(write(my_as->u.as.action_fd,msg,k)<=0){
      LM_DBG("Ignoring error write\n");
   }
   return 0;
}

/*
 * This callback function should be used in order to free the parameters passed to uac_cb.
 * This callback is called when the transaction is detroyed.
 */
void uac_cleanup_cb(struct cell* t, int type, struct tmcb_params *rcvd_params)
{
	struct as_uac_param *ev_info;

	ev_info=(struct as_uac_param*)*rcvd_params->param;

	if(ev_info) {	
		shm_free(ev_info);
		*rcvd_params->param=NULL;
	}
}

void uas_e2e_ack_cb(struct cell* t, int type,struct tmcb_params *rcvd_params)
{
   struct as_uac_param *ev_info;
   int mylen;
   as_msg_p my_as_ev=NULL;
   char *buffer=NULL;

   ev_info=(struct as_uac_param*)*rcvd_params->param;

   if(!(type & TMCB_E2EACK_IN))
      return;

   if(!(my_as_ev=shm_malloc(sizeof(as_msg_t)))){
      LM_ERR("no more shared mem\n");
      goto error;
   }
   if(!(buffer=create_as_event_t(t,rcvd_params->req,ev_info->processor_id,&mylen,E2E_ACK))){
      LM_ERR("unable to create event code\n");
      goto error;
   }
   my_as_ev->as = ev_info->who;
   my_as_ev->msg = buffer;
   my_as_ev->len = mylen;
   my_as_ev->type = RES_IN;
   my_as_ev->transaction = t;
   if(write(write_pipe,&my_as_ev,sizeof(as_msg_p))<=0){
      goto error;
   }
   goto exit;
error:
   if(my_as_ev){
      shm_free(my_as_ev);
   }
   if(buffer)
      shm_free(buffer);
exit:
   return ;
}

/**
 * This function will be called from a SER process when a reply is received for
 * the transaction. The SER processes only have acces to the EventDispatcher 
 * fifo (not to the ActionDispatcher) so EventDispatcher will be the one who 
 * will send the event to the AppServer.
 */
void uac_cb(struct cell* t, int type,struct tmcb_params *rcvd_params)
{
   as_msg_p my_as_ev=0;
   int mylen,code,i;
   struct as_uac_param *ev_info;
   char *buffer;

   ev_info=(struct as_uac_param*)*rcvd_params->param;
   code=rcvd_params->code;
   buffer=0;
   if(!ev_info || !ev_info->who){
      return;
   }

   if(type == TMCB_LOCAL_COMPLETED && !ev_info->destroy_cb_set) {
      if(seas_f.tmb.register_tmcb(NULL, t, TMCB_DESTROY , uac_cleanup_cb, (void*)ev_info, 0) <= 0) {
         LM_ERR( "register_tmcb for destroy callback failed\n");
         goto error;
      }
      ev_info->destroy_cb_set = 1;
   }

   LM_DBG("reply to UAC Transaction for AS:%.*s code: %d\n",
		   ev_info->who->name.len,ev_info->who->name.s,code);
   LM_DBG("transaction %p Nr_of_outgoings:%d is_Local:%c\n",
		   t,t->nr_of_outgoings,is_local(t)?'y':'n');
   for(i=0;i<t->nr_of_outgoings;i++)
      LM_DBG("UAC[%d].last_received=%d\n",i,t->uac[i].last_received);
   if(!(my_as_ev=shm_malloc(sizeof(as_msg_t)))){
      LM_ERR("no more shared mem\n");
      goto error;
   }
   if(!(buffer=create_as_action_reply(t,rcvd_params,ev_info->uac_id,ev_info->processor_id,&mylen))){
      LM_ERR("failed to encode message\n");
      goto error;
   }
   my_as_ev->as = ev_info->who;
   my_as_ev->msg = buffer;
   my_as_ev->len = mylen;
   my_as_ev->type = RES_IN;
   my_as_ev->transaction = t;
   if(write(write_pipe,&my_as_ev,sizeof(as_msg_p))<=0){
      goto error;
   }
   goto exit;
error:
   if(my_as_ev){
      shm_free(my_as_ev);
   }
   if(buffer)
      shm_free(buffer);
exit:
   return ;
}

char* create_as_action_reply(struct cell *c,struct tmcb_params *params,int uac_id,char processor_id,int *evt_len)
{
   int i;
   unsigned int code,flags;
   unsigned short int port;
   unsigned int k,len;
   char *buffer;
   struct sip_msg *msg;
   if(!(buffer=shm_malloc(ENCODED_MSG_SIZE))){
      LM_ERR("create_as_action_reply Out Of Memory !!\n");
      return 0;
   }
   msg=0;
   *evt_len=0;
   flags=0;
   if(params->rpl==FAKED_REPLY)
      flags=FAKED_REPLY_FLAG;
   /*length*/
   k=4;
   /*type*/
   buffer[k++]=(unsigned char)RES_IN;
   /*processor id*/
   buffer[k++]=processor_id;
   /*flags (by now, not used)*/
   flags=htonl(flags);
   memcpy(buffer+k,&flags,4);
   k+=4;
   /*recv info*/
   if(!(params->rpl == FAKED_REPLY)) {
      msg=params->rpl;
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
   }else{
      /*protocol*/
      buffer[k++]=0;
      /*src ip len*/
      buffer[k++]=0;
      /*dst ip len*/
      buffer[k++]=0;
      /*skip src port and dst port*/
      buffer[k++]=0;
      buffer[k++]=0;
      buffer[k++]=0;
      buffer[k++]=0;
   }
   /*hash_index*/
   i=htonl(c->hash_index);
   memcpy(buffer+k,&i,4);
   k+=4;
   /*label*/
   i=(!strncmp(c->method.s,"CANCEL",6)) ? \
     htonl(((struct as_uac_param*)*params->param)->label) : \
	htonl(c->label);
   memcpy(buffer+k,&i,4);
   k+=4;
   /*uac_id*/
   uac_id=htonl(uac_id);
   memcpy(buffer+k,&uac_id,4);
   k+=4;
   /*code*/
   code=htonl(params->code);
   memcpy(buffer+k,&code,4);
   k+=4;
   /*length of event (hdr+payload-4), copied at the beginning*/
   if(params->rpl != FAKED_REPLY) {
      if((i=encode_msg(msg,buffer+k,ENCODED_MSG_SIZE-k))<0){
	 LM_ERR("failed to encode msg\n");
	 goto error;
      }
      k+=i;
   }
   *evt_len=k;
   k=htonl(k);
   memcpy(buffer,&k,4);
   return buffer;
error:
   return 0;
}


