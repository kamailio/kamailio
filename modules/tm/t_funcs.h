#ifndef _T_FUNCS_H
#define _T_FUNCS_H

#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include "../../msg_parser.h"
#include "../../globals.h"
#include "../../udp_server.h"
#include "../../msg_translator.h"
#include "../../mem.h"

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



int tm_startup();
int tm_shutdown();


/* function returns:
 *       0 - a new transaction was created
 *      -1 - retransmission
 *      -2 - error
 */
int  t_add_transaction( struct sip_msg* p_msg );


/* function returns:
 *       0 - transaction wasn't found
 *       1 - transaction found
 */
int  t_lookup_request( struct sip_msg* p_msg );


/* function returns:
 *       0 - forward successfull
 *      -1 - error during forward
 */
int t_forward( struct sip_msg* p_msg , unsigned int dst_ip , unsigned int dst_port);



/*  This function is called whenever a reply for our module is received; we need to register
  *  this function on module initialization;
  *  Returns :   1 - core router stops
  *                    0 - core router relay statelessly
  */
int t_on_reply_received( struct sip_msg  *p_msg ) ;




/*
*/
int t_put_on_wait(  struct sip_msg  *p_msg  );




/* Retransmits the last sent inbound reply.
  */
int t_retransmit_reply( struct sip_msg *  );




/* Force a new response into inbound response buffer.
  */
int t_send_reply( struct sip_msg * , unsigned int , char *  );






struct cell* t_lookupOriginalT(  struct s_table* hash_table , struct sip_msg* p_msg );
int t_reply_matching( struct s_table* , struct sip_msg* , struct cell** , unsigned int*  );
int t_store_incoming_reply( struct cell* , unsigned int , struct sip_msg* );
int t_relay_reply( struct cell* , unsigned int , struct sip_msg* );
int t_check( struct s_table* , struct sip_msg*  );
int t_all_final( struct cell * );
int t_build_and_send_ACK( struct cell *Trans , unsigned int brach );
int relay_lowest_reply_upstream( struct cell *Trans , struct sip_msg *p_msg );
int push_reply_from_uac_to_uas( struct sip_msg * , unsigned int );
int t_cancel_branch(unsigned int branch); //TO DO
int add_branch_label( struct cell *Trans, struct sip_msg *p_msg , int branch );

void retransmission_handler( void *);
void final_response_handler( void *);
void wait_handler( void *);
void delete_handler( void *);




#endif
