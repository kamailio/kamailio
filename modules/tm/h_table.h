#ifndef _H_TABLE_H
#define _H_TABLE_H

#include <stdio.h>
#include <stdlib.h>
#ifdef THREADS
#include <pthread.h>
#endif
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "../../msg_parser.h"

struct s_table;
struct timer;
struct entry;
struct cell;

#include "timer.h"
#include "lock.h"


#define sh_malloc( size )  malloc(size)
#define sh_free( ptr )        free(ptr)

#define TABLE_ENTRIES  256
#define MAX_FORK           20


extern  struct cell      *T;
extern  unsigned int  global_msg_id;

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
   int                  sem;
   lock_t   mutex;
} timer_type;


typedef struct retrans_buff
{
   char                *buffer;
   int                  bufflen;
   unsigned int dest_ip;
   unsigned int dest_port;
}retrans_buff_type;


typedef struct cell
{
   /* linking data */
   struct cell*     next_cell;
   struct cell*     prev_cell;

   /* tells in which hash table entry the cell lives */
   unsigned int  hash_index;
   /* sequence number within hash collision slot */
   unsigned int  label;

   /*sync data */
   lock_t   mutex;
   int       ref_counter;

   /* bindings to all timer links in which a cell may be located */
   struct timer_link tl[NR_OF_TIMER_LISTS];

   /* usefull data */
   /* incoming request and its response*/
   struct sip_msg         *inbound_request;
   struct retrans_buff   *inbound_response;
   unsigned int             status;
   /* array of outgoing requests and its responses */
   int                               nr_of_outgoings;
   struct retrans_buff   *outbound_request[ MAX_FORK ];
   struct sip_msg          *outbound_response[ MAX_FORK ];

}cell_type;


/* double-linked list of cells with hash synonyms */
typedef struct entry
{
   struct cell*       first_cell;
   struct cell*       last_cell;
   /* currently highest sequence number in a synonym list */
   unsigned int    next_label;
   /* sync mutex */
   lock_t                 mutex;
}entry_type;


/* hash table */
struct s_table
{
   /* table of hash entries; each of them is a list of synonyms  */
   struct entry*   entrys;
   /* table of timer lists */
   struct timer*   timers;
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
 *       0 - a new transaction was created -> the proxy core don't have to release the p_msg structure
 *      -1 - retransmission
 *      -2 - error
 */
int                t_add_transaction( struct s_table* hash_table , struct sip_msg* p_msg );


/* function returns:
 *       0 - transaction wasn't found
 *       1 - transaction found
 */
int  t_lookup_request( struct s_table* hash_table , struct sip_msg* p_msg );




#endif
