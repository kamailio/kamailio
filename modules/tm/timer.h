#ifndef _TIMER_H
#define _TIMER_H

enum lists { FR_TIMER_LIST, WT_TIMER_LIST, DELETE_LIST, NR_OF_TIMER_LISTS };

#include "h_table.h"

/* FINAL_RESPONSE_TIMER ... tells how long should the transaction engine
   wait if no final response comes back
*/
#define FR_TIME_OUT     16
/* WAIT timer ... tells how long state should persist in memory after
   a transaction was finalized
*/
#define WT_TIME_OUT      32

void put_in_tail_of_timer_list( struct s_table* hash_table, struct cell* p_cell,
                  int list_id, unsigned int time_out );
void remove_timer( struct s_table* hash_table, struct cell* p_cell,
                  int list_id);
void remove_from_hash_table( struct s_table *hash_table,  struct cell * p_cell );
void remove_timer_from_head( struct s_table* hash_table, struct cell* p_cell, int list_id );
void del_Transaction( struct s_table *hash_table , struct cell * p_cell );
void start_FR_timer( struct s_table* hash_table, struct cell* p_cell );
void start_WT_timer( struct s_table* hash_table, struct cell* p_cell );
void * timer_routine(void * attr);

#endif
