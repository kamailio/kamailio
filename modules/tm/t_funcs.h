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

#define T_UNDEFINED 	( (struct cell*) -1 )
#define T_NULL		( (struct cell*) 0 )

#define sh_malloc( size )     malloc(size)
#define sh_free( ptr )           free(ptr)
/* already defined in msg_parser.h
#define get_cseq( p_msg)    ((struct cseq_body*)p_msg->cseq->parsed)
*/

/* maximumum length of localy generated acknowledgement */
#define MAX_ACK_LEN 1024


int tm_startup();
int tm_shutdown();


/* function returns:
 *       1 - a new transaction was created
 *      -1 - error, including retransmission
 */
int  t_add_transaction( struct sip_msg* p_msg, char* foo, char* bar  );




/* function returns:
 *      -1 - transaction wasn't found
 *       1 - transaction found
 */
int  t_lookup_request( struct sip_msg* p_msg, char* foo, char* bar  );




/* Forwards the inbound request to a given IP and port.  Returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward( struct sip_msg* p_msg , unsigned int dst_ip ,
				unsigned int dst_port);




/* Forwards the inbound request to dest. from via.  Returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward_uri( struct sip_msg* p_msg , char* foo, char* bar );



/*  This function is called whenever a reply for our module is received; we need to register
  *  this function on module initialization;
  *  Returns :   0 - core router stops
  *                    1 - core router relay statelessly
  */
int t_on_reply_received( struct sip_msg  *p_msg ) ;




/* returns 1 if everything was OK or -1 for error
*/
int t_put_on_wait(  struct sip_msg  *p_msg  );




/* Retransmits the last sent inbound reply.
  * Returns  -1 -error
  *                1 - OK
  */
int t_retransmit_reply( struct sip_msg *, char* , char* );




/* Force a new response into inbound response buffer.
 * returns 1 if everything was OK or -1 for erro
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
int push_reply_from_uac_to_uas( struct cell* Trans , unsigned int );
int t_cancel_branch(unsigned int branch); //TO DO
int add_branch_label( struct cell *Trans, struct sip_msg *p_msg , int branch );

void retransmission_handler( void *);
void final_response_handler( void *);
void wait_handler( void *);
void delete_handler( void *);




#endif
