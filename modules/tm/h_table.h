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

struct s_table;
struct entry;
struct cell;
struct timer;

#include "sh_malloc.h"

#include "timer.h"
#include "lock.h"
#include "sip_msg.h"



/* always use a power of 2 for hash table size */
#define TABLE_ENTRIES  256
#define MAX_FORK           20


/* timer list: includes head, tail and protection semaphore */
typedef struct  timer
{
   struct timer_link *first_tl;
   struct timer_link *last_tl;
   ser_lock_t             mutex;
   void                      (*timeout_handler)(void*);
} timer_type;




typedef struct retrans_buff
{
   char               *retr_buffer;
   int                  bufflen;

   struct sockaddr_in to;
   /* changed in favour of Solaris to size_t
   socklen_t tolen;
   */
   size_t tolen;

   /* a message can be linked just to retransmission and FR list */
   struct timer_link tl[2];
   unsigned int timeout_ceiling;
   unsigned int timeout_value;

   /*the cell that containes this retrans_buff*/
   struct cell* my_T;
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
   /* tells in which hash table entry the cell lives */
   unsigned int  hash_index;
   /* sequence number within hash collision slot */
   unsigned int  label;

   /* bindings to wait and delete timer */
   struct timer_link wait_tl;
   struct timer_link dele_tl;

   /*the transaction that is canceled (usefull only for CANCEL req)*/
   struct cell *T_canceled;
   struct cell *T_canceler;

   /* usefull data */
   /* UA Server */
   struct sip_msg         *inbound_request;
   struct retrans_buff   *outbound_response;
   unsigned int             status;
   str*                             tag;
   /* array of outgoing requests and its responses */
   int                               nr_of_outgoings;
   /* UA Clients */
   struct retrans_buff   *outbound_request[ MAX_FORK ];
   struct sip_msg          *inbound_response[ MAX_FORK ];
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
   struct entry   entrys[ TABLE_ENTRIES ];
   /* table of timer lists */
   struct timer   timers[ NR_OF_TIMER_LISTS ];
   /* current time */
   unsigned int   time;
   /* timer process pid*/
   unsigned int   timer_pid;
};





struct s_table* init_hash_table();
void                  free_hash_table( struct s_table* hash_table );

void             free_cell( struct cell* dead_cell );
struct cell*  build_cell( struct sip_msg* p_msg );

void remove_from_hash_table( struct s_table *hash_table,  struct cell * p_cell );
void    insert_into_hash_table( struct s_table *hash_table,  struct cell * p_cell );

void      ref_cell( struct cell* p_cell);
void unref_cell( struct cell* p_cell);



#endif
