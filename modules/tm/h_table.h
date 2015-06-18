/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**  TM :: hash table, flags and other general defines.
 * @file 
 * @ingroup tm
 */


#ifndef _H_TABLE_H
#define _H_TABLE_H

#include "defs.h"
#include "t_stats.h"

#define TM_DEL_UNREF
/* uncomment the next define if you wish to keep hash statistics*/
/*
#define TM_HASH_STATS
*/
/* use hash stats always in debug mode */
#ifdef EXTRA_DEBUG
#ifndef TM_HASH_STATS
#define TM_HASH_STATS
#endif
#endif


#include "../../clist.h"
#include "../../parser/msg_parser.h"
#include "../../types.h"
#include "../../md5utils.h"
#include "../../usr_avp.h"
#ifdef WITH_XAVP
#include "../../xavp.h"
#endif
#include "../../timer.h"
#include "../../flags.h"
#include "../../atomic_ops.h"
#include "../../hash_func.h"
#include "config.h"

/* if TM_DIFF_RT_TIMEOUT is defined, different retransmissions timeouts
 * can be used for each transaction, at a small memory cost
 * (extra 4 bytes/transaction) */
#define TM_DIFF_RT_TIMEOUT


struct s_table;
struct entry;
struct cell;
struct timer;
struct retr_buf;
struct ua_client;
struct async_state;

#include "../../mem/shm_mem.h"
#include "lock.h"
#include "sip_msg.h"
#include "t_hooks.h"
#ifdef USE_DNS_FAILOVER
#include "../../dns_cache.h"
#endif


#define LOCK_HASH(_h) lock_hash((_h))
#define UNLOCK_HASH(_h) unlock_hash((_h))

void lock_hash(int i);
void unlock_hash(int i);


#define NO_CANCEL       ( (char*) 0 )
#define EXTERNAL_CANCEL ( (char*) -1)

#define TYPE_LOCAL_ACK    -2
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
   REQ_RLSD means that a transaction was put on wait explicitly
            from t_release_transaction
   REQ_EXIST means that this request is a retransmission which does not
            affect transactional state
   REQ_ERR_DELAYED mean that tm wants to send  reply(ser_error) but it
            delayed it to end-of-script to allow it to be overriden.
            If this is set and all of the above flag are not => send reply
            on end of script. If any of the above flags is set, do not
            send (especially REQ_RPLD and REQ_RLSD).
*/
enum kill_reason { REQ_FWDED=1, REQ_RPLD=2, REQ_RLSD=4, REQ_EXIST=8,
				   REQ_ERR_DELAYED=16 };


/* #define F_RB_T_ACTIVE		0x01  (obsolete) fr or retr active */
#define F_RB_T2				0x02
#define F_RB_RETR_DISABLED	0x04 /* retransmission disabled */
#define F_RB_FR_INV	0x08 /* timer switched to FR_INV */
#define F_RB_TIMEOUT	0x10 /* timeout */
#define F_RB_REPLIED	0x20 /* reply received */
#define F_RB_CANCELED	0x40 /* rb/branch canceled */
#define F_RB_DEL_TIMER	0x80 /* timer should be deleted if active */
#define F_RB_NH_LOOSE	0x100 /* next hop is a loose router */
#define F_RB_NH_STRICT	0x200 /* next hop is a strict router */
/* must detect when neither loose nor strict flag is set -> two flags.
 * alternatively, 1x flag for strict/loose and 1x for loose|strict set/not */
#define F_RB_RELAYREPLY	0x400 /* branch under relay reply condition */


/* if canceled or intended to be canceled, return true */
#define uac_dont_fork(uac)	((uac)->local_cancel.buffer)


typedef struct retr_buf
{
	short activ_type;
	/* set to status code if the buffer is a reply,
	0 if request or -1 if local CANCEL */
	volatile unsigned short flags; /* DISABLED, T2 */
	volatile unsigned char t_active; /* timer active */
	unsigned short branch; /* no more then 65k branches :-) */
	int buffer_len;
	char *buffer;
	/*the cell that contains this retrans_buff*/
	struct cell* my_T;
	struct timer_ln timer;
	struct dest_info dst;
	ticks_t retr_expire;
	ticks_t fr_expire; /* ticks value after which fr. will fire */
}retr_buf_type;



/* User Agent Server content */

typedef struct ua_server
{
	struct sip_msg   *request;
	char             *end_request;
	struct retr_buf  response;
	/* keep to-tags for local 200 replies for INVITE -- 
	 * we need them for dialog-wise matching of ACKs;
	 * the pointer shows to shmem-ed reply */
	str				 local_totag;
#ifdef CANCEL_REASON_SUPPORT
	struct cancel_reason* cancel_reas; /* pointer to cancel reason, used
										  for e2e cancels */
#endif /* CANCEL_REASON_SUPPORT */
	unsigned int     status;
}ua_server_type;



/* User Agent Client content */

#define TM_UAC_FLAGS
#ifdef TM_UAC_FLAGS
/* UAC internal flags */
#define TM_UAC_FLAG_RR	1	/* Record-Route applied */
#define TM_UAC_FLAG_R2	2	/* 2nd Record-Route applied */
#define TM_UAC_FLAG_FB	4	/* Mark first entry in new branch set */
#endif

typedef struct ua_client
{
	/* if we store a reply (branch picking), this is where it is */
	struct sip_msg  *reply;
	char *end_reply;	/* pointer to end of sip_msg so we know the shm blocked used in clone...(used in async replies) */
	struct retr_buf  request;
	/* we maintain a separate copy of cancel rather than
	   reuse the structure for original request; the 
	   original request is no longer needed but its delayed
	   timer may fire and interfere with whoever tries to
	   rewrite it
	*/
	struct retr_buf local_cancel;
	/* pointer to retransmission buffer where uri is printed;
	   good for generating ACK/CANCEL */
#ifdef USE_DNS_FAILOVER
	struct dns_srv_handle dns_h;
#endif
	str uri;
	str path;
	str instance;
	str ruid;
	str location_ua;
	/* if we don't store, we at least want to know the status */
	int             last_received;

#ifdef TM_UAC_FLAGS
	/* internal flags per tm uac */
	unsigned int flags;
#endif
	/* per branch flags */
	flag_t branch_flags;
	/* internal processing code - (mapping over sip warning codes)
	 * - storing the code giving a clue of what happened internally */
	int icode;
#ifdef WITH_AS_SUPPORT
	/**
	 * Resent for every rcvd 2xx reply.
	 * This member's as an alternative to passing the reply to the AS, 
	 * every time a reply for local request is rcvd.
	 * Member can not be union'ed with local_cancel, since CANCEL can happen
	 * concurrently with a 2xx reply (to generate an ACK).
	 */
	struct retr_buf *local_ack;
#endif
	/* the route to take if no final positive reply arrived */
	unsigned short on_failure;
	/* the route to take for all failure replies */
	unsigned short on_branch_failure;
	/* the onreply_route to be processed if registered to do so */
	unsigned short on_reply;
	/* unused - keep the structure aligned to 32b */
	unsigned short on_unused;
}ua_client_type;


struct totag_elem {
	struct totag_elem *next;
	str tag;
	volatile int acked;
};

/* structure for storing transaction state prior to suspending of async transactions */
typedef struct async_state {
	unsigned int backup_route;
	unsigned int backup_branch;
	unsigned int blind_uac;
	unsigned int ruri_new;
} async_state_type;

/* transaction's flags */
/* is the transaction's request an INVITE? */
#define T_IS_INVITE_FLAG     (1<<0)
/* is this a transaction generated by local request? */
#define T_IS_LOCAL_FLAG      (1<<1)
/* set to one if you want to disallow silent transaction
   dropping when C timer hits */
#define T_NOISY_CTIMER_FLAG  (1<<2)
/* transaction canceled
 * WARNING: this flag can be set outside reply lock from e2e_cancel().
 * If a future flag could be affected by a race w/ e2e_cancel() the code
 * should be changed.*/
#define T_CANCELED           (1<<3)
/* 6xx received => stop forking */
#define T_6xx            (1<<4) 

#define T_IN_AGONY (1<<5) /* set if waiting to die (delete timer)
                             TODO: replace it with del on unref */
#define T_AUTO_INV_100 (1<<6) /* send an 100 reply automatically  to inv. */
#ifdef WITH_AS_SUPPORT
	/* don't generate automatically an ACK for local transaction */
#	define T_NO_AUTO_ACK	(1<<7)
#endif

#define T_DISABLE_6xx (1<<8) /* treat 6xx as a normal reply */
#define T_DISABLE_FAILOVER (1<<9) /* don't perform dns failover */
#ifdef CANCEL_REASON_SUPPORT
#define T_NO_E2E_CANCEL_REASON (1<<10) /* don't propagate CANCEL Reason */
#endif /* CANCEL_REASON_SUPPORT */
#define T_DONT_FORK   (T_CANCELED|T_6xx)

#ifdef WITH_AS_SUPPORT
	/* provisional replies must trigger callbacks for local transaction */
#	define T_PASS_PROVISIONAL_FLAG (1<<11)
#	define pass_provisional(_t_)	((_t_)->flags&T_PASS_PROVISIONAL_FLAG)
#endif
#define T_ASYNC_CONTINUE (1<<12) /* Is this transaction in a continuation after being suspended */

#define T_DISABLE_INTERNAL_REPLY (1<<13) /* don't send internal negative reply */
#define T_ADMIN_REPLY (1<<14) /* t reply sent by admin (e.g., from cfg script) */
#define T_ASYNC_SUSPENDED (1<<15)

/* unsigned short should be enough for a retr. timer: max. 65535 ms =>
 * max retr. = 65 s which should be enough and saves us 2*2 bytes */
typedef unsigned short retr_timeout_t;

/**
 * extra data from SIP message context to transaction storage
 */
typedef struct tm_xdata
{
	/* lists with avps */
	struct usr_avp *uri_avps_from;
	struct usr_avp *uri_avps_to;
	struct usr_avp *user_avps_from;
	struct usr_avp *user_avps_to;
	struct usr_avp *domain_avps_from;
	struct usr_avp *domain_avps_to;
#ifdef WITH_XAVP
	sr_xavp_t *xavps_list;
#endif
} tm_xdata_t;


/**
 * links to extra data from SIP message context to transaction storage
 */
typedef struct tm_xlinks
{
	/* links to lists with avps */
	struct usr_avp **uri_avps_from;
	struct usr_avp **uri_avps_to;
	struct usr_avp **user_avps_from;
	struct usr_avp **user_avps_to;
	struct usr_avp **domain_avps_from;
	struct usr_avp **domain_avps_to;
#ifdef WITH_XAVP
	sr_xavp_t **xavps_list;
#endif
} tm_xlinks_t;


/* transaction context */

typedef struct cell
{
	/* linking data */
	/* WARNING: don't move or change order of next_c or prev_c
	 * or breakage will occur */
	struct cell*     next_c;
	struct cell*     prev_c;
	/* tells in which hash table entry the cell lives */
	unsigned int  hash_index;
	/* sequence number within hash collision slot */
	unsigned int  label;
	/* different information about the transaction */
	unsigned short flags;
	/* number of forks */
	short nr_of_outgoings;

#ifdef TM_DEL_UNREF
	/* every time the transaction/cell is referenced from somewhere this
	 * ref_count should be increased (via REF()) and every time the reference
	 * is removed the ref_count should be decreased (via UNREF()).
	 * This includes adding the cell to the hash table (REF() before adding)
	 * and removing it from the hash table (UNREF_FREE() after unlinking).
	 * Exception: it does not include starting/stopping timers (timers are 
	 * forced-stopped every time when ref_count reaches 0)
	 * If the cell is no longer referenced (ref_count==0 after an UNREF),
	 * it will be automatically deleted by the UNREF() operation.
	 */
	atomic_t ref_count;
#else 
	/* how many processes are currently processing this transaction ;
	   note that only processes working on a request/reply belonging
	   to a transaction increase ref_count -- timers don't, since we
	   rely on transaction state machine to clean-up all but wait timer
	   when entering WAIT state and the wait timer is the only place
	   from which a transaction can be deleted (if ref_count==0); good
	   for protecting from conditions in which wait_timer hits and
	   tries to delete a transaction whereas at the same time 
	   a delayed message belonging to the transaction is received */
	volatile unsigned int ref_count;
#endif

	/* needed for generating local ACK/CANCEL for local
	   transactions; all but cseq_n include the entire
	   header field value, cseq_n only Cseq number; with
	   local transactions, pointers point to outbound buffer,
	   with proxied transactions to inbound request */
	str from, callid, cseq_n, to;
	/* method shortcut -- for local transactions, pointer to
	   outbound buffer, for proxies transactions pointer to
	   original message; needed for reply matching */
	str method;

	/* head of callback list */
	struct tmcb_head_list tmcb_hl;

	/* bindings to wait and delete timer */
	struct timer_ln wait_timer; /* used also for delete */

	/* UA Server */
	struct ua_server  uas;
	/* UA Clients */
	struct ua_client  *uac;
	
	/* store transaction state to be used for async transactions */
	struct async_state async_backup;
	
	/* to-tags of 200/INVITEs which were received from downstream and 
	 * forwarded or passed to UAC; note that there can be arbitrarily 
	 * many due to downstream forking; */
	struct totag_elem *fwded_totags;

	     /* lists with avps */
	struct usr_avp *uri_avps_from;
	struct usr_avp *uri_avps_to;
	struct usr_avp *user_avps_from;
	struct usr_avp *user_avps_to;
	struct usr_avp *domain_avps_from;
	struct usr_avp *domain_avps_to;
#ifdef WITH_XAVP
	sr_xavp_t *xavps_list;
#endif

	/* protection against concurrent reply processing */
	ser_lock_t reply_mutex;
	/* pid of the process that holds the reply lock */
	atomic_t reply_locker_pid;
	/* recursive reply lock count */
	int reply_rec_lock_level;

#ifdef ENABLE_ASYNC_MUTEX
	/* protect against concurrent async continues */
	ser_lock_t   async_mutex;
#endif

	ticks_t fr_timeout;     /* final response interval for retr_bufs */
	ticks_t fr_inv_timeout; /* final inv. response interval for retr_bufs */
#ifdef TM_DIFF_RT_TIMEOUT
	retr_timeout_t rt_t1_timeout_ms; /* start retr. interval for retr_bufs */
	retr_timeout_t rt_t2_timeout_ms; /* maximum retr. interval for retr_bufs */
#endif
	ticks_t end_of_life; /* maximum lifetime */

	/* nr of replied branch; 0..sr_dst_max_branches=branch value,
	 * -1 no reply, -2 local reply */
	short relayed_reply_branch;

	/* the route to take if no final positive reply arrived */
	unsigned short on_failure;
	/* the route to take for all failure replies */
	unsigned short on_branch_failure;
	/* the onreply_route to be processed if registered to do so */
	unsigned short on_reply;
	 /* The route to take for each downstream branch separately */
	unsigned short on_branch;
	 /* branch route backup for late branch add (t_append_branch) */
	unsigned short on_branch_delayed;

	/* place holder for MD5checksum, MD5_LEN bytes are extra alloc'ed */
	char md5[0];

} tm_cell_t;


#if 0
/* warning: padding too much => big size increase */
#define ENTRY_PAD_TO  128 /* should be a multiple of cacheline size for 
                             best performance*/
#define ENTRY_PAD_BYTES	 \
	(ENTRY_PAD_TO-2*sizeof(struct cell*)+sizeof(ser_lock_t)+sizeof(int)+ \
	 				2*sizeof(long))
#else
#define ENTRY_PAD_BYTES 0
#endif

/* double-linked list of cells with hash synonyms */
typedef struct entry
{
	/* WARNING: don't move or change order of next_c or prev_c
	 * or breakage will occur */
	struct cell*    next_c; 
	struct cell*    prev_c;
	/* sync mutex */
	ser_lock_t      mutex;
	atomic_t locker_pid; /* pid of the process that holds the lock */
	int rec_lock_level; /* recursive lock count */
	/* currently highest sequence number in a synonym list */
	unsigned int    next_label;
#ifdef TM_HASH_STATS
	unsigned long acc_entries;
	unsigned long cur_entries;
#endif
	char _pad[ENTRY_PAD_BYTES];
}entry_type;



/* transaction table */
struct s_table
{
	/* table of hash entries; each of them is a list of synonyms  */
	struct entry   entries[ TABLE_ENTRIES ];
};

/* pointer to the big table where all the transaction data
   lives */
extern struct s_table*  _tm_table; /* private internal stuff, don't touch
									  directly */

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
#define has_noisy_ctimer(_t_)    ((_t_)->flags&T_NOISY_CTIMER_FLAG)
#define was_cancelled(_t_)       ((_t_)->flags&T_CANCELED)
#define no_new_branches(_t_)     ((_t_)->flags&T_6xx)


void reset_kr(void);
void set_kr( enum kill_reason kr );
enum kill_reason get_kr(void);

#define get_tm_table() (_tm_table)

typedef struct s_table* (*tm_get_table_f)(void);
struct s_table* tm_get_table(void);

struct s_table* init_hash_table(void);
void   free_hash_table(void);
void   free_cell( struct cell* dead_cell );
struct cell*  build_cell( struct sip_msg* p_msg );

#ifdef TM_HASH_STATS
unsigned int transaction_count( void );
#endif


/*  Takes an already created cell and links it into hash table on the
 *  appropriate entry. */
inline static void insert_into_hash_table_unsafe( struct cell * p_cell,
													unsigned int hash )
{
	p_cell->label = _tm_table->entries[hash].next_label++;
#ifdef EXTRA_DEBUG
	DEBUG("cell label: %u\n", p_cell->label);
#endif
	p_cell->hash_index=hash;
	/* insert at the beginning */
	clist_insert(&_tm_table->entries[hash], p_cell, next_c, prev_c);

	/* update stats */
#ifdef TM_HASH_STATS
	_tm_table->entries[hash].cur_entries++;
	_tm_table->entries[hash].acc_entries++;
#endif
	t_stats_new( is_local(p_cell) );
}



/*  Un-link a  cell from hash_table, but the cell itself is not released */
inline static void remove_from_hash_table_unsafe( struct cell * p_cell)
{
	clist_rm(p_cell, next_c, prev_c);
#	ifdef EXTRA_DEBUG
#ifdef TM_HASH_STATS
	if (_tm_table->entries[p_cell->hash_index].cur_entries==0){
		LOG(L_CRIT, "BUG: bad things happened: cur_entries=0\n");
		abort();
	}
#endif
#	endif
#ifdef TM_HASH_STATS
	_tm_table->entries[p_cell->hash_index].cur_entries--;
#endif
	t_stats_deleted( is_local(p_cell) );
}

/**
 * backup xdata from/to msg context to local var and use T lists
 */
void tm_xdata_swap(tm_cell_t *t, tm_xlinks_t *xd, int mode);

void tm_xdata_replace(tm_xdata_t *newxd, tm_xlinks_t *bakxd);

#endif


