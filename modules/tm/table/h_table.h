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

struct s_table;
struct timer;
struct cell;

#include "timer.h"

#define sh_malloc( size )  malloc(size)
#define sh_free( ptr )        free(ptr)

#define TABLE_ENTRIES  256
#define SEM_KEY            6688

/* all you need to put a cell in a timer list:
   links to neighbours and timer value
*/
struct timer_link
{
	struct cell *timer_next_cell, *timer_prev_cell;
	unsigned int time_out;
} ;

/* timer list: includes head, tail and protection semaphore */
typedef struct  timer
{
   struct cell*    first_cell;
   struct cell*    last_cell;
   int                  sem;
} timer_type;



typedef struct cell
{
   /* linking data */
   struct cell*     next_cell;
   struct cell*     prev_cell;

   /* tells in which hash table entry the cell lives */
   int                   hash_index;
   /* textual encoding of entry index in hash-table;
      used for looking up transactions for replies */
   char*               via_label;
   /* sequence number within hash collision slot; ascending
      across the synonym list */
   int                   label;

   /*sync data */
   int      sem;
   int      ref_counter;

   /* bindings to all timer links in which a cell may be located */
   struct timer_link tl[NR_OF_TIMER_LISTS];

   /* usefull data */
   int      status;
   int      to_length;
   int      req_tag_length;
   int      res_tag_length;
   int      from_length;
   int      cseq_nr_length;
   int      cseq_method_length;
   int      call_id_length;
   int      incoming_req_uri_length;
   char*  to;
   char*  req_tag;
   char*  res_tag;
   char*  from;
   char*  cseq_nr;
   char*  cseq_method;
   char*  call_id;
   char*  incoming_req_uri;
   char*  outgoing_req_uri;
}cell_type;


/* double-linked list of cells with hash synonyms */
typedef struct entry
{
   struct cell*  first_cell;
   struct cell*  last_cell;
   /* currently highest sequence number in a synonym list */
   int                next_label;
   /* semaphore */
   int                sem;
}entry_type;

/* hash table */
struct s_table
{
   /* table of hash entries; each of them is a list of synonyms  */
   struct entry*   entrys;
   /* table of timer lists */
   struct timer*   timers;
   /* pointers to items which had ref-count>0 on the first
      delete attempt -> garbage'm later
   */
/* delete-list put among other lists too;
   struct cell*      first_del_hooker;
   struct cell*      last_del_hooker;
*/
#ifdef THREADS
   pthread_t         timer_thread_id;
#endif
   /* current time */
  unsigned int   time;
};


struct s_table* init_hash_table();
void free_hash_table( struct s_table* hash_table );
struct cell* add_Transaction(
	struct s_table* hash_table, char* incoming_req_uri, char* from, char* to,
	char* tag, char* call_id, char* cseq_nr ,char* cseq_method );
struct cell* lookup_for_Transaction_by_req(
	struct s_table* hash_table, char* from, char* to, char* tag,
	char* call_id , char* cseq_nr ,char* cseq_method );
struct cell* lookup_for_Transaction_by_ACK(
	struct s_table* hash_table, char* from, char* to, char* tag,
	char* call_id, char* cseq_nr );
struct cell* lookup_for_Transaction_by_CANCEL(
	struct s_table* hash_table,char *req_uri, char* from, char* to,
	char* tag, char* call_id, char* cseq_nr );
struct cell* lookup_for_Transaction_by_res(
	struct s_table* hash_table, char* label, char* from, char* to,
	char* tag, char* call_id, char* cseq_nr ,char* cseq_method );
void unref_Cell( struct cell* p_cell);

void free_cell( struct cell* dead_cell );


#endif
