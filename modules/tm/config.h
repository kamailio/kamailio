/*
 * $Id$
 *
 */

#ifndef _TM_CONFIG_H
#define _TM_CONFIG_H

/* always use a power of 2 for hash table size */
#define T_TABLE_POWER		10
#define TABLE_ENTRIES  		(2 << (T_TABLE_POWER-1))

/* maximum number of forks per transaction */
#define MAX_FORK		2

/* maximumum length of localy generated acknowledgement */
#define MAX_ACK_LEN 		1024

/* FINAL_RESPONSE_TIMER ... tells how long should the transaction engine
   wait if no final response comes back*/
#define FR_TIME_OUT		16
#define INV_FR_TIME_OUT     	30

/* WAIT timer ... tells how long state should persist in memory after
   a transaction was finalized*/
#define WT_TIME_OUT      	5

/* DELETE timer ... tells how long should the transaction persist in memory
   after it was removed from the hash table and before it will be deleted */
#define DEL_TIME_OUT      	2
 
/* retransmission timers */
#define RETR_T1  		1
#define RETR_T2  		4

#endif
