/*
 * $Id$
 */

#ifndef _TIMER_H
#define _TIMER_H

#include "lock.h"

/* identifiers of timer lists; 
*/
enum lists {	FR_TIMER_LIST, FR_INV_TIMER_LIST,
				WT_TIMER_LIST, DELETE_LIST, 
				/* fixed-timer retransmission lists (benefit: fixed timer
				   length allows for appending new items to the list as
					opposed to inserting them which is costly */
				RT_T1_TO_1, RT_T1_TO_2, RT_T1_TO_3, RT_T2, 
				NR_OF_TIMER_LISTS };

#define is_in_timer_list2(_tl) ( (_tl)->list )


struct timer;

/* all you need to put a cell in a timer list:
   links to neighbours and timer value         */
typedef struct timer_link
{
	struct timer_link 	*next_tl;
	struct timer_link 	*prev_tl;
	unsigned int       	time_out;
	void				*payload;
	struct timer		*list;
}timer_link_type ;


/* timer list: includes head, tail and protection semaphore */
typedef struct  timer
{
   struct timer_link first_tl;
   struct timer_link last_tl;
   ser_lock_t             mutex;
   enum lists id;
   void                      (*timeout_handler)(void*);
} timer_type;

void init_timer_list( struct s_table* hash_table, enum lists list_id);
void reset_timer_list( struct s_table* hash_table, enum lists list_id);

void add_to_tail_of_timer_list( struct timer *timer_list, 
	struct timer_link *tl, unsigned int time_out );
void remove_from_timer_list( struct timer_link *tl);
void timer_routine(unsigned int, void *);


/* deprecated -- too expensive -- use appending instead 
void insert_into_timer_list( struct s_table* hash_table , 
	struct timer_link* tl, enum lists list_id , unsigned int time_out );
*/

#endif
