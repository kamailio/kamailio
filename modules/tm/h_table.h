/*
 * $Id$
 */


#ifndef _H_TABLE_H
#define _H_TABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "../../msg_parser.h"
#include "../../types.h"
#include "config.h"

struct s_table;
struct entry;
struct cell;
struct timer;

#include "sh_malloc.h"

#include "timer.h"
#include "lock.h"
#include "sip_msg.h"


#define T_UNDEFINED  ( (struct cell*) -1 )
#define T_NULL       ( (struct cell*) 0 )


#define NO_CANCEL       ( (struct retrans_buff*) 0 )
#define EXTERNAL_CANCEL ( (struct retrans_buff*) -1)

#define STATUS_LOCAL_CANCEL -1
#define STATUS_REQUEST       0


typedef struct retrans_buff
{
	char *retr_buffer;
	int   bufflen;

	struct sockaddr_in to;
	size_t tolen;

	/* a message can be linked just to retransmission and FR list */
	struct timer_link retr_timer;
	struct timer_link fr_timer;

	/*the cell that containes this retrans_buff*/
	struct cell* my_T;
	unsigned int branch;

	enum lists retr_list;

	/* set to status code if the buffer is a reply, 
	0 if request or -1 if local CANCEL */
	int status;

}retrans_buff_type;


/* transaction context */

typedef struct cell
{
	/* linking data */
	struct cell*     next_cell;
	struct cell*     prev_cell;

	/* indicates which process is currently processing this transaction */
	process_bm_t  ref_bitmap;
	/* tells in which hash table entry the cell lives */
	unsigned int  hash_index;
	/* sequence number within hash collision slot */
	unsigned int  label;

	/* bindings to wait and delete timer */
	struct timer_link wait_tl;
	struct timer_link dele_tl;

	/*the transaction that is canceled (usefull only for CANCEL req)*/
	struct cell *T_canceled;

	/* useful data */
	/* UA Server */
	struct sip_msg       *inbound_request;
	struct retrans_buff   outbound_response;
	unsigned int          status;
	str                  *tag;
	unsigned int          inbound_request_isACKed;
	int                   relaied_reply_branch;
	int                   nr_of_outgoings;
	/* UA Clients */
	struct retrans_buff   *outbound_request[ MAX_FORK ];
	struct sip_msg        *inbound_response[ MAX_FORK ];
	struct retrans_buff   *outbound_ack[ MAX_FORK ];
	struct retrans_buff   *outbound_cancel[ MAX_FORK ];

	/* protection against concurrent reply processing */
	ser_lock_t   reply_mutex;
	/* protection against concurrent ACK processing */
	ser_lock_t	ack_mutex;
#ifdef WAIT
	/* protection against reentering WAIT state */
	ser_lock_t	wait_mutex;
	/* has the transaction been put on wait status ? */
	int on_wait;
#endif

	/* this is where destination is stored for picked branch;
	good if a need to forward ACK later on */
	struct sockaddr_in ack_to;
#ifndef	USE_SYNONIM
	/* MD5checksum */
	char md5[32];
#endif

#ifdef	EXTRA_DEBUG
	/* scheduled for deletion ? */
	short damocles;
#endif

}cell_type;



/* double-linked list of cells with hash synonyms */
typedef struct entry
{
   struct cell*       first_cell;
   struct cell*       last_cell;
   /* currently highest sequence number in a synonym list */
   unsigned int    next_label;
   /* sync mutex */
   ser_lock_t                 mutex;
}entry_type;



/* transaction table */
struct s_table
{
   /* table of hash entries; each of them is a list of synonyms  */
   struct entry   entrys[ TABLE_ENTRIES ];
   /* table of timer lists */
   struct timer   timers[ NR_OF_TIMER_LISTS ];
};


struct s_table* init_hash_table();
void free_hash_table( struct s_table* hash_table );
void free_cell( struct cell* dead_cell );
struct cell*  build_cell( struct sip_msg* p_msg );
void remove_from_hash_table( struct s_table *hash_table, struct cell * p_cell );
void insert_into_hash_table( struct s_table *hash_table, struct cell * p_cell );
void insert_into_hash_table_unsafe( struct s_table *hash_table, struct cell * p_cell );

#endif
