/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
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
 */

#include "defs.h"


#ifndef _H_TABLE_H
#define _H_TABLE_H

#include <stdio.h>
#include <stdlib.h>

#include "../../parser/msg_parser.h"
#include "../../types.h"
#include "../../md5utils.h"
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

/* to be able to assess whether a script writer forgot to
   release a transaction and leave it for ever in memory,
   we mark it with operations done over it; if none of these
   flags is set and script is being left, it is a sign of
   script error and we need to release on writer's
   behalf

   REQ_FWDED means there is a UAC with final response timer
             ticking. If it hits, transaction will be completed.
   REQ_RPLD means that a transaction has been replied -- either
            it implies going to wait state, or for invite transactions
            FR timer is ticking until ACK arrives
   REQ_RLSD means that a transaction was put on wait explicitely
            from t_release_transaction
   REQ_EXIST means that this request is a retransmission which does not
            affect transactional state
         
*/
enum kill_reason { REQ_FWDED=1, REQ_RPLD=2, REQ_RLSD=4, REQ_EXIST=8 };

typedef struct retr_buf
{
	int activ_type;
	/* set to status code if the buffer is a reply,
	0 if request or -1 if local CANCEL */

	char *buffer;
	int   buffer_len;
	
	struct dest_info dst;

	/* a message can be linked just to retransmission and FR list */
	struct timer_link retr_timer;
	struct timer_link fr_timer;
	enum lists retr_list;

	/*the cell that containes this retrans_buff*/
	struct cell* my_T;
	unsigned int branch;

}retr_buf_type;



/* User Agent Server content */

typedef struct ua_server
{
	struct sip_msg   *request;
	struct retr_buf  response;
	unsigned int     status;
	/* keep to-tags for local 200 replies for INVITE -- 
	 * we need them for dialog-wise matching of ACKs;
	 * the pointer shows to shmem-ed reply */
	str				 local_totag;
}ua_server_type;



/* User Agent Client content */

typedef struct ua_client
{
	struct retr_buf  request;
	/* we maintain a separate copy of cancel rather than
	   reuse the strructure for original request; the 
	   original request is no longer needed but its delayed
	   timer may fire and interfere with whoever tries to
	   rewrite it
	*/
	struct retr_buf local_cancel;
	/* pointer to retransmission buffer where uri is printed;
	   good for generating ACK/CANCEL */
	str              uri;
	/* if we store a reply (branch picking), this is where it is */
	struct sip_msg 	*reply;
	/* if we don't store, we at least want to know the status */
	int	last_received;

}ua_client_type;


struct totag_elem {
	str tag;
	short acked;
	struct totag_elem *next;
};

/* transaction context */

typedef struct cell
{
	/* linking data */
	struct cell*     next_cell;
	struct cell*     prev_cell;

	/* needed for generating local ACK/CANCEL for local
	   transactions; all but cseq_n include the entire
	   header field value, cseq_n only Cseq number; with
	   local transactions, pointers point to outbound buffer,
	   with proxied transactions to inbound request */
	str from, callid, cseq_n, to;
	/* a short-cut for remember whether this transaction needs
	   INVITE-special handling (e.g., CANCEL, ACK, FR...)  */
	short is_invite;
	/* method shortcut -- for local transactions, pointer to
	   outbound buffer, for proxies transactions pointer to
	   original message; needed for reply matching */
	str method;

	/* head of callback list */
	struct tmcb_head_list tmcb_hl;

	/* how many processes are currently processing this transaction ;
	   note that only processes working on a request/reply belonging
	   to a transaction increase ref_count -- timers don't, since we
	   rely on transaction state machine to clean-up all but wait timer
	   when entering WAIT state and the wait timer is the only place
	   from which a transaction can be deleted (if ref_count==0); good
	   for protecting from conditions in which wait_timer hits and
	   tries to delete a transaction whereas at the same time 
	   a delayed message belonging to the transaction is received
	*/
	volatile unsigned int ref_count;
	/* tells in which hash table entry the cell lives */
	unsigned int  hash_index;
	/* sequence number within hash collision slot */
	unsigned int  label;

	/* bindings to wait and delete timer */
	struct timer_link wait_tl;
	struct timer_link dele_tl;

	/* number of forks */
	int nr_of_outgoings;
	/* nr of replied branch; 0..MAX_BRANCHES=branch value,
	 * -1 no reply, -2 local reply */
	int relaied_reply_branch;
	/* UA Server */
	struct ua_server  uas;
	/* UA Clients */
	struct ua_client  uac[ MAX_BRANCHES ];

	/* protection against concurrent reply processing */
	ser_lock_t   reply_mutex;

	/* the route to take if no final positive reply arrived */
	unsigned int on_negative;
	/* the onreply_route to be processed if registered to do so */
	unsigned int on_reply;
	/* set to one if you want to disallow silent transaction
	   dropping when C timer hits
	*/
	int noisy_ctimer;
	/* is it a local UAC transaction ? */
	int local;

	/* MD5checksum  (meaningful only if syn_branch=0 */
	char md5[MD5_LEN];

#ifdef	EXTRA_DEBUG
	/* scheduled for deletion ? */
	short damocles;
#endif
	/* has the transaction been scheduled to die? */
/*	enum kill_reason kr; */

	/* to-tags of 200/INVITEs which were received from downstream and 
	 * forwarded or passed to UAC; note that there can be arbitrarily 
	 * many due to downstream forking; */
	struct totag_elem *fwded_totags;
}cell_type;



/* double-linked list of cells with hash synonyms */
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



/* transaction table */
struct s_table
{
	/* table of hash entries; each of them is a list of synonyms  */
	struct entry   entrys[ TABLE_ENTRIES ];
};


void set_kr( enum kill_reason kr );
enum kill_reason get_kr();

struct s_table* get_tm_table();
struct s_table* init_hash_table();
void   free_hash_table( );
void   free_cell( struct cell* dead_cell );
struct cell*  build_cell( struct sip_msg* p_msg );
void   remove_from_hash_table_unsafe( struct cell * p_cell);
#ifdef OBSOLETED
void   insert_into_hash_table( struct cell * p_cell, unsigned int _hash);
#endif
void   insert_into_hash_table_unsafe( struct cell * p_cell, unsigned int _hash );

unsigned int transaction_count( void );

int fifo_hash( FILE *stream, char *response_file );

#endif


