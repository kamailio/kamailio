/*
 * $Id$
 *
 */

#ifndef _TM_CONFIG_H
#define _TM_CONFIG_H

/* always use a power of 2 for hash table size */
#define T_TABLE_POWER    12
#define TABLE_ENTRIES    (1 << (T_TABLE_POWER))

/* maximum number of forks per transaction */
enum fork_list { MAX_FORK=4 , NO_RPL_BRANCH ,NR_OF_CLIENTS};
enum fork_type { DEFAULT, NO_RESPONSE };

/* maximumum length of localy generated acknowledgement */
#define MAX_ACK_LEN   1024

/* FINAL_RESPONSE_TIMER ... tells how long should the transaction engine
   wait if no final response comes back*/
#define FR_TIME_OUT       30
#define INV_FR_TIME_OUT   120

/* WAIT timer ... tells how long state should persist in memory after
   a transaction was finalized*/
#define WT_TIME_OUT       5

/* DELETE timer ... tells how long should the transaction persist in memory
   after it was removed from the hash table and before it will be deleted */
#define DEL_TIME_OUT      2
 
/* retransmission timers */
#define RETR_T1           1
#define RETR_T2           4

/* when first reply is sent, this additional space is allocated so that
   one does not have to reallocate share memory when the message is
   replaced by a subsequent, longer message
*/
#define REPLY_OVERBUFFER_LEN 160
#define TAG_OVERBUFFER_LEN 32

/* character which separates individual parts of MPLS-ized branch */
#ifdef BRUT_HACK
#	define BRANCH_SEPARATOR 'X'
#else
#	define BRANCH_SEPARATOR '.'
#endif

#endif
