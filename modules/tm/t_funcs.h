#ifndef _T_FUNCS_H
#define _T_FUNCS_H

#include "../../msg_parser.h"

struct s_table;
struct timer;
struct entry;
struct cell;

#include "timer.h"
#include "lock.h"
#include "sip_msg.h"

#define sh_malloc( size )     malloc(size)
#define sh_free( ptr )           free(ptr)
#define get_cseq( p_msg)    ((struct cseq_body*)p_msg->cseq->parsed)



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
