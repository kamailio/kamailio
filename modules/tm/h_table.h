#ifndef _H_TABLE_H
#define _H_TABLE_H

#include <stdio.h>
#include <stdlib.h>
#ifdef THREADS
#include <pthread.h>
#endif
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/socket.h>

#include "../../msg_parser.h"

struct s_table;
struct timer;
struct entry;
struct cell;

#include "timer.h"
#include "lock.h"
#include "msg_cloner.h"

#define sh_malloc( size )     malloc(size)
#define sh_free( ptr )           free(ptr)
#define get_cseq( p_msg)    ((struct cseq_body*)p_msg->cseq->parsed)


/* always use a power of 2 for hash table size */
#define TABLE_ENTRIES  256
#define MAX_FORK           20

/* all you need to put a cell in a timer list:
   links to neighbours and timer value         */
typedef struct timer_link
{
   struct cell *timer_next_cell, *timer_prev_cell;
   unsigned int time_out;
}timer_link_type ;


/* timer list: includes head, tail and protection semaphore */

typedef struct  timer
{
   struct cell*    first_cell;
   struct cell*    last_cell;
   /* int                  sem; */
   ser_lock_t   mutex;
} timer_type;


typedef struct retrans_buff
{
   unsigned int type;
   char               *buffer;
   int                  bufflen;
   unsigned int dest_ip;
   unsigned int dest_port;

   struct sockaddr *to;
   socklen_t tolen;

}retrans_buff_type;


typedef struct cell
{
   /* linking data */
   struct cell*     next_cell;
   struct cell*     prev_cell;

   /*sync data */
   ser_lock_t   mutex;
   int       ref_counter;

   /* cell payload data */
   union {
	/* transactions */
	struct {
   		/* tells in which hash table entry the cell lives */
   		unsigned int  hash_index;
   		/* sequence number within hash collision slot */
   		unsigned int  label;

   		/* bindings to all timer links in which a cell may be located */
   		struct timer_link tl[NR_OF_TIMER_LISTS];

   		/* usefull data */
   		/* incoming request and its response*/
   		struct sip_msg         *inbound_request;
   		struct retrans_buff   *inbound_response;
   		unsigned int             status;
   		str*                             tag;
   		/* array of outgoing requests and its responses */
   		int                               nr_of_outgoings;
   		struct retrans_buff   *outbound_request[ MAX_FORK ];
   		struct sip_msg          *outbound_response[ MAX_FORK ];
	} transaction;
	/* retransmission buffer */
	struct {
		struct retrans_buffer *retr_buffer;
		/* a message can be linked just to one retransmission list */
		struct timer_link retransmission_timer_list;
	} retransmission;
   }; /* cell payload */
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


/* hash table */
struct s_table
{
   /* table of hash entries; each of them is a list of synonyms  */
   struct entry*   entrys;
   /* table of timer lists */
   struct timer*   timers;
   /* retransmission lists */
   struct timer*   retr_timers;
#ifdef THREADS
   pthread_t         timer_thread_id;
#endif
   /* current time */
  unsigned int   time;
};


void free_cell( struct cell* dead_cell );
struct s_table* init_hash_table();

void ref_transaction( struct cell* p_cell);
void unref_transaction( struct cell* p_cell);

void free_hash_table( struct s_table* hash_table );


/* function returns:
 *       0 - a new transaction was created 
 *      -1 - retransmission
 *      -2 - error
 */
int  t_add_transaction( struct s_table* hash_table , struct sip_msg* p_msg );


/* function returns:
 *       0 - transaction wasn't found
 *       1 - transaction found
 */
int  t_lookup_request( struct s_table* hash_table , struct sip_msg* p_msg );


/* function returns:
 *       0 - transaction wasn't found
 *       T - transaction found
 */
struct cell* t_lookupOriginalT(  struct s_table* hash_table , struct sip_msg* p_msg );


/* function returns:
 *       0 - forward successfull
 *      -1 - error during forward
 */
int t_forward( struct s_table* hash_table , struct sip_msg* p_msg , unsigned int dst_ip , unsigned int dst_port);



/*  This function is called whenever a reply for our module is received; we need to register
  *  this function on module initialization;
  *  Returns :   1 - core router stops
  *                    0 - core router relay statelessly
  */
int t_on_reply_received( struct s_table  *hash_table , struct sip_msg  *p_msg ) ;



/* Retransmits the last sent inbound reply.
  */
int t_retransmit_reply( struct s_table * , struct sip_msg *  );


#endif
