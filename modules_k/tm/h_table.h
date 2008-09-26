/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-03-16  removed _TOTAG (jiri)
 * 2003-03-06  we keep a list of 200/INV to-tags now (jiri)
 * 2003-03-01  kr set through a function now (jiri)
 * 2003-12-04  callbacks per transaction added; completion callback
 *             merge into them as LOCAL_COMPETED (bogdan)
 * 2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 * 2004-02-13  t->is_invite, t->local, t->noisy_ctimer replaced
 *             with flags (bogdan)
 * 2004-08-23  avp support added - avp list linked in transaction (bogdan)
 * 2007-01-25  DNS failover at transaction level added (bogdan) 
 */

/*! \file
 * \brief TM :: Hash tables
 *
 * \ingroup tm
 * - Module: \ref tm
 */

#ifndef _H_TABLE_H
#define _H_TABLE_H

#include <stdio.h>
#include <stdlib.h>

#include "../../parser/msg_parser.h"
#include "../../proxy.h"
#include "../../md5utils.h"
#include "../../usr_avp.h"
#include "config.h"

struct s_table;
struct entry;
struct cell;
struct timer;
struct retr_buf;

#include "../../mem/shm_mem.h"
#include "lock.h"
#include "sip_msg.h"
#include "t_reply.h"
#include "t_hooks.h"
#include "timer.h"

#define LOCK_HASH(_h) lock_hash((_h))
#define UNLOCK_HASH(_h) unlock_hash((_h))

void lock_hash(int i);
void unlock_hash(int i);


#define NO_CANCEL       ( (char*) 0 )
#define EXTERNAL_CANCEL ( (char*) -1)

#define TYPE_LOCAL_CANCEL -1
#define TYPE_REQUEST       0

/*! \brief to be able to assess whether a script writer forgot to
   release a transaction and leave it for ever in memory,
   we mark it with operations done over it; if none of these
   flags is set and script is being left, it is a sign of
   script error and we need to release on writer's
   behalf

   - REQ_FWDED means there is a UAC with final response timer
             ticking. If it hits, transaction will be completed.
   - REQ_RPLD means that a transaction has been replied -- either
            it implies going to wait state, or for invite transactions
            FR timer is ticking until ACK arrives
   - REQ_RLSD means that a transaction was put on wait explicitly
            from t_release_transaction
   - REQ_EXIST means that this request is a retransmission which does not
            affect transactional state
*/
enum kill_reason { REQ_FWDED=1, REQ_RPLD=2, REQ_RLSD=4, REQ_EXIST=8 };

typedef void (*setkr_f)(enum kill_reason mykr);

typedef struct retr_buf
{
	int activ_type;
	/* set to status code if the buffer is a reply,
	0 if request or -1 if local CANCEL */

	str buffer;
	
	struct dest_info dst;

	/* a message can be linked just to retransmission and FR list */
	struct timer_link retr_timer;
	struct timer_link fr_timer;
	enum lists retr_list;

	/*the cell that contains this retrans_buff*/
	struct cell* my_T;
	unsigned int branch;
} retr_buf_type;



/*! \brief User Agent Server content */
typedef struct ua_server
{
	struct sip_msg   *request;
	char             *end_request;
	struct retr_buf  response;
	unsigned int     status;
	/* keep to-tags for local 200 replies for INVITE -- 
	 * we need them for dialog-wise matching of ACKs;
	 * the pointer shows to shmem-ed reply */
	str              local_totag;
}ua_server_type;



/*! \brief User Agent Client content */
typedef struct ua_client
{
	struct retr_buf  request;
	struct proxy_l   *proxy;
	/* we maintain a separate copy of cancel rather than
	   reuse the structure for original request; the 
	   original request is no longer needed but its delayed
	   timer may fire and interfere with whoever tries to
	   rewrite it */
	struct retr_buf local_cancel;
	/* pointer to retransmission buffer where uri is printed;
	   good for generating ACK/CANCEL */
	str              uri;
	/* the path vector used for this branch */
	str              path_vec;
	/* number of RR headers that were locally added for this branch */
	unsigned int     added_rr;
	/* if we store a reply (branch picking), this is where it is */
	struct sip_msg  *reply;
	/* if we don't store, we at least want to know the status */
	short            last_received;
	/* UAC specific flags */
	short            flags;
	/* script flags, specific to this branch */
	int              br_flags;
}ua_client_type;


struct totag_elem {
	str tag;
	short acked;
	struct totag_elem *next;
};



/*! \name transaction flags */
/*@{*/
#define T_IS_INVITE_FLAG        (1<<0)	/*!< is the transaction's request an INVITE? */
#define T_IS_LOCAL_FLAG         (1<<1)	/*!< is this a transaction generated by local request? */
#define T_WAS_CANCELLED_FLAG    (1<<3)	/*!< set to one if you want to disallow silent transaction
   						dropping when C timer hits */
#define T_HOPBYHOP_CANCEL_FLAG  (1<<4) 	/*!< transaction was cancelled hopbyhop */
#define T_NO_AUTOACK_FLAG       (1<<5)	/*!< ACK must not be auto generated for the local transaction */
#define T_PASS_PROVISIONAL_FLAG (1<<6)	/*!< provisional replies must trigger callbacks for local transaction */
#define T_NO_DNS_FAILOVER_FLAG  (1<<7)	/*!< do auto DNS failover  */
#define T_NO_NEW_BRANCHES_FLAG  (1<<8)	/*!< transaction must not create new branches  */

/* transaction UAC's flags */
#define T_UAC_TO_CANCEL_FLAG  (1<<0)	/*!< UAC :: is the UAC pending for CANCEL ?  */
#define T_UAC_HAS_RECV_REPLY  (1<<1)	/*!< UAC :: have the UAC received any replies?  */

/*@}*/


/*! \brief transaction context */
typedef struct cell
{
	/* linking data */
	struct cell*     next_cell;
	struct cell*     prev_cell;
	unsigned int  hash_index;	/*!< tells in which hash table entry the cell lives */
	unsigned int  label;		/*!< sequence number within hash collision slot */
	unsigned int flags; 		/*!< different information about the transaction */

	/*! \brief how many processes are currently processing this transaction? 

	   note that only processes working on a request/reply belonging
	   to a transaction increase ref_count -- timers don't, since we
	   rely on transaction state machine to clean-up all but wait timer
	   when entering WAIT state and the wait timer is the only place
	   from which a transaction can be deleted (if ref_count==0); good
	   for protecting from conditions in which wait_timer hits and
	   tries to delete a transaction whereas at the same time 
	   a delayed message belonging to the transaction is received */
	volatile unsigned int ref_count;

	/*! \brief needed for generating local ACK/CANCEL for local
	   transactions

	   all but cseq_n include the entire
	   header field value, cseq_n only Cseq number; with
	   local transactions, pointers point to outbound buffer,
	   with proxied transactions to inbound request */
	str from, callid, cseq_n, to;

	/*! \brief method shortcut 

	   for local transactions, pointer to
	   outbound buffer, for proxies transactions pointer to
	   original message; needed for reply matching */
	str method;

	struct tmcb_head_list tmcb_hl;		/*!< head of callback list */

	struct timer_link wait_tl;		/*!< binding to wait timer */
	struct timer_link dele_tl;		/*!< binding to delete timer */

	int first_branch;		/*!< first branch - when serial forking is performed, keeps the first
	 			    	      branch for each step ; it allows proper branch selection */
	int nr_of_outgoings;			/*!< number of forks */
	int relaied_reply_branch;	/*!< nr of replied branch; 0..MAX_BRANCHES=branch value,
	 					  -1 no reply, -2 local reply */
	struct ua_server  uas;			/*!< UA Server */
	struct ua_client  uac[ MAX_BRANCHES ];	/*!< UA Clients */

	ser_lock_t   reply_mutex;		/*!< protection against concurrent reply processing */

	unsigned int on_negative;		/*!< the route to take if no final positive reply arrived */
	unsigned int on_reply;			/*!< the onreply_route to be processed if registered to do so */
	unsigned int on_branch;			/*!< the branch_route to be processed separately for each branch */

	char md5[MD5_LEN];			/*!< MD5checksum  (meaningful only if syn_branch=0) */

#ifdef	EXTRA_DEBUG
	short damocles;				/*!< scheduled for deletion ? */
#endif

	/*! \brief  to-tags of 200/INVITEs which were received from downstream and 
	 * forwarded or passed to UAC; note that there can be arbitrarily 
	 * many due to downstream forking; */
	struct totag_elem *fwded_totags;

	struct usr_avp *user_avps;		/*!< list with user avp */

	void *dialog_ctx;			/*!< holders for higher contexts */
} cell_type;



/*! \brief double-linked list of cells with hash synonyms */
typedef struct entry
{
	struct cell*    first_cell;
	struct cell*    last_cell;
	/* currently highest sequence number in a synonym list */
	unsigned int    next_label;
	/* sync mutex */
	ser_lock_t      mutex;
	unsigned long acc_entries;
	unsigned long cur_entries;
}entry_type;



/*! \brief transaction table */
struct s_table
{
	/*! \brief table of hash entries; each of them is a list of synonyms  */
	struct entry   entrys[ TM_TABLE_ENTRIES ];
};


#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define get_retr_timer_payload(_tl_) \
	list_entry( _tl_, struct retr_buf, retr_timer)
#define get_fr_timer_payload(_tl_) \
	list_entry( _tl_, struct retr_buf, fr_timer)
#define get_wait_timer_payload(_tl_) \
	list_entry( _tl_, struct cell, wait_tl)
#define get_dele_timer_payload(_tl_) \
	list_entry( _tl_, struct cell, dele_tl)

#define get_T_from_reply_rb(_rb_) \
	list_entry( list_entry( _rb_, (struct ua_server), response),\
		struct cell, uas)
#define get_T_from_request_rb(_rb_, _br_) \
	list_entry( list_entry( (rb_, (struct ua_client), request) - \
		(_br_)*sizeof(struct retr_buf), struct cell, uas)
#define get_T_from_cancel_rb(_rb_, _br_) \
	list_entry( list_entry( (rb_, (struct ua_client), local_cancel) - \
		(_br_)*sizeof(struct retr_buf), struct cell, uas)

#define is_invite(_t_)           ((_t_)->flags&T_IS_INVITE_FLAG)
#define is_local(_t_)            ((_t_)->flags&T_IS_LOCAL_FLAG)
#define was_cancelled(_t_)       ((_t_)->flags&T_WAS_CANCELLED_FLAG)
#define is_hopbyhop_cancel(_t_)  ((_t_)->flags&T_HOPBYHOP_CANCEL_FLAG)
#define no_autoack(_t_)          ((_t_)->flags&T_NO_AUTOACK_FLAG)
#define pass_provisional(_t_)    ((_t_)->flags&T_PASS_PROVISIONAL_FLAG)
#define no_new_branches(_t_)     ((_t_)->flags&T_NO_NEW_BRANCHES_FLAG)


extern int syn_branch;


void reset_kr(void);
void set_kr( enum kill_reason kr );
enum kill_reason get_kr(void);

struct s_table* get_tm_table(void);
struct s_table* init_hash_table(void);
void   free_hash_table(void);
void   free_cell( struct cell* dead_cell );
struct cell*  build_cell( struct sip_msg* p_msg );
void   remove_from_hash_table_unsafe( struct cell * p_cell);
#ifdef OBSOLETED
void   insert_into_hash_table( struct cell * p_cell, unsigned int _hash);
#endif
void   insert_into_hash_table_unsafe( struct cell * p_cell, unsigned int _hash );

unsigned int transaction_count(void);

/* Unix socket variant */
int unixsock_hash(str* msg);

#endif


