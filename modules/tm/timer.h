/*
 * $Id$
 */

#ifndef _TIMER_H
#define _TIMER_H

/* timer timestamp value indicating a timer has been 
   deactived and shall not be executed
*/
#define TIMER_DELETED	1



/* identifiers of timer lists;*/
/* fixed-timer retransmission lists (benefit: fixed timer$
   length allows for appending new items to the list as$
   opposed to inserting them which is costly */
enum lists
{
	FR_TIMER_LIST, FR_INV_TIMER_LIST,
	WT_TIMER_LIST,
	DELETE_LIST,
	RT_T1_TO_1, RT_T1_TO_2, RT_T1_TO_3, RT_T2,
	NR_OF_TIMER_LISTS
};



#define is_in_timer_list2(_tl) ( (_tl)->timer_list )

extern int timer_group[NR_OF_TIMER_LISTS];
extern unsigned int timer_id2timeout[NR_OF_TIMER_LISTS];

struct timer;

#include "lock.h"
#include "t_funcs.h"


/* all you need to put a cell in a timer list
   links to neighbours and timer value */
typedef struct timer_link
{
	struct timer_link *next_tl;
	struct timer_link *prev_tl;
	volatile unsigned int       time_out;
	void              *payload;
	struct timer      *timer_list;
	enum timer_groups  tg;
}timer_link_type ;


/* timer list: includes head, tail and protection semaphore */
typedef struct  timer
{
	struct timer_link  first_tl;
	struct timer_link  last_tl;
	ser_lock_t*        mutex;
	enum lists         id;
} timer_type;



void init_timer_list( struct s_table* hash_table, enum lists list_id);
void reset_timer_list( struct s_table* hash_table, enum lists list_id);
void remove_timer_unsafe(  struct timer_link* tl ) ;
void add_timer_unsafe( struct timer*, struct timer_link*, unsigned int);
struct timer_link  *check_and_split_time_list( struct timer*, int);

void reset_timer( struct s_table *hash_table,
	struct timer_link* tl );
/* determine timer length and put on a correct timer list */
void set_timer( struct s_table *hash_table,
	struct timer_link *new_tl, enum lists list_id );
/* similar to set_timer, except it allows only one-time
   timer setting and all later attempts are ignored */
void set_1timer( struct s_table *hash_table,
	struct timer_link *new_tl, enum lists list_id );

#endif
