/*
 * $Id$
 */


#ifndef _T_FUNCS_H
#define _T_FUNCS_H

#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>

#include "../../parser/msg_parser.h"
#include "../../globals.h"
#include "../../udp_server.h"
#include "../../msg_translator.h"
#include "../../timer.h"
#include "../../forward.h"
#include "../../mem/mem.h"
#include "../../md5utils.h"
#include "../../ip_addr.h"

#include "config.h"
#include "lock.h"
#include "timer.h"
#include "sh_malloc.h"
#include "sip_msg.h"


struct s_table;
struct timer;
struct entry;
struct cell;

extern struct cell      *T;
extern unsigned int     global_msg_id;
extern struct s_table*  hash_table;



#define LOCK_REPLIES(_t) lock(&(_t)->reply_mutex )
#define UNLOCK_REPLIES(_t) unlock(&(_t)->reply_mutex )
#define LOCK_ACK(_t) lock(&(_t)->ack_mutex )
#define UNLOCK_ACK(_t) unlock(&(_t)->ack_mutex )
#define LOCK_WAIT(_t) lock(&(_t)->wait_mutex )
#define UNLOCK_WAIT(_t) unlock(&(_t)->wait_mutex )


/* send a private buffer: utilize a retransmission structure
   but take a separate buffer not refered by it; healthy
   for reducing time spend in REPLIES locks
*/

inline static int send_pr_buffer( struct retr_buf *rb,
	void *buf, int len, char *function, int line )
{
	if (buf && len && rb )
		return udp_send( rb->send_sock, buf, 
			len, &rb->to,  sizeof(union sockaddr_union) ) ;
	else {
		LOG(L_CRIT, "ERROR: sending an empty buffer from %s (%d)\n",
			function, line );
		return -1;
	}
}

#define SEND_PR_BUFFER(_rb,_bf,_le ) \
	send_pr_buffer( (_rb), (_bf), (_le),  __FUNCTION__, __LINE__ )

/*
#define SEND_PR_BUFFER(_rb,_bf,_le ) \
	( ((_bf) && (_le) && (_bf)) ? \
	udp_send( (_bf), (_le), &((_rb)->to), sizeof(union sockaddr_union) ) : \
	log_send_error( __FUNCTION__, __LINE__ ) )
*/

/* just for understanding of authors of the following macros, who did not
   include 'PR' in macro names though they use 'PR' macro: PR stands for
   PRIVATE and indicates usage of memory buffers in PRIVATE memory space,
   where -- as opposed to SHARED memory space -- no concurrent memory
   access can occur and thus no locking is needed ! -jiri
*/
#define SEND_ACK_BUFFER( _rb ) \
	SEND_PR_BUFFER( (_rb) , (_rb)->ack , (_rb)->ack_len )

#define SEND_CANCEL_BUFFER( _rb ) \
	SEND_PR_BUFFER( (_rb) , (_rb)->cancel , (_rb)->cancel_len )

#define SEND_BUFFER( _rb ) \
	SEND_PR_BUFFER( (_rb) , (_rb)->buffer , (_rb)->buffer_len )


/*
  macros for reference bitmap (lock-less process non-exclusive ownership) 
*/
#define T_IS_REFED(_T_cell) ((_T_cell)->ref_bitmap)
#define T_REFCOUNTER(_T_cell) \
	( { int _i=0; \
		process_bm_t _b=(_T_cell)->ref_bitmap; \
		while (_b) { \
			if ( (_b) & 1 ) _i++; \
			(_b) >>= 1; \
		} ;\
		(_i); \
	 } )
		

#ifdef EXTRA_DEBUG
#define T_IS_REFED_BYSELF(_T_cell) ((_T_cell)->ref_bitmap & process_bit)
#	define DBG_REF(_action, _t) DBG("DEBUG: XXXXX %s (%s:%d): T=%p , ref (bm=%x, cnt=%d)\n",\
			(_action), __FUNCTION__, __LINE__, (_t),(_t)->ref_bitmap, T_REFCOUNTER(_t));
#	define T_UNREF(_T_cell) \
	( { \
		DBG_REF("unref", (_T_cell)); \
		if (!T_IS_REFED_BYSELF(_T_cell)) { \
			DBG("ERROR: unrefering unrefered transaction %p from %s , %s : %d\n", \
				(_T_cell), __FUNCTION__, __FILE__, __LINE__ ); \
			abort(); \
		} \
		(_T_cell)->ref_bitmap &= ~process_bit; \
	} )

#	define T_REF(_T_cell) \
	( { \
		DBG_REF("ref", (_T_cell));	 \
		if (T_IS_REFED_BYSELF(_T_cell)) { \
			DBG("ERROR: refering already refered transaction %p from %s,%s :"\
				" %d\n",(_T_cell), __FUNCTION__, __FILE__, __LINE__ ); \
			abort(); \
		} \
		(_T_cell)->ref_bitmap |= process_bit; \
	} )
#else
#	define T_UNREF(_T_cell) ({ (_T_cell)->ref_bitmap &= ~process_bit; })
#	define T_REF(_T_cell) ({ (_T_cell)->ref_bitmap |= process_bit; })
#endif



/*
enum addifnew_status { AIN_ERROR, AIN_RETR, AIN_NEW, AIN_NEWACK,
	AIN_OLDACK, AIN_RTRACK } ;
*/


int   tm_startup();
void tm_shutdown();


/* function returns:
 *       1 - a new transaction was created
 *      -1 - error, including retransmission
 */
int  t_add_transaction( struct sip_msg* p_msg  );




/* function returns:
 *      -1 - transaction wasn't found
 *       1 - transaction found
 */
int t_check( struct sip_msg* , int *branch , int* is_cancel);




/* Forwards the inbound request to a given IP and port.  Returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
/* v6; -jiri
int t_forward( struct sip_msg* p_msg , unsigned int dst_ip ,
										unsigned int dst_port);
*/




/* Forwards the inbound request to dest. from via.  Returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
/* v6; -jiri
int t_forward_uri( struct sip_msg* p_msg  );
*/




/* This function is called whenever a reply for our module is received;
 * we need to register this function on module initialization;
 * Returns :   0 - core router stops
 *             1 - core router relay statelessly
 */
int t_on_reply( struct sip_msg  *p_msg ) ;




/* This function is called whenever a request for our module is received;
 * we need to register this function on module initialization;
 * Returns :   0 - core router stops
 *             1 - core router relay statelessly
 */
int t_on_request_received( struct sip_msg  *p_msg , unsigned int ip, unsigned int port) ;




/* This function is called whenever a request for our module is received;
 * we need to register this function on module initialization;
 * Returns :   0 - core router stops
 *             1 - core router relay statelessly
 */
int t_on_request_received_uri( struct sip_msg  *p_msg ) ;




/* returns 1 if everything was OK or -1 for error
 */
int t_release_transaction( struct sip_msg* );




/* Retransmits the last sent inbound reply.
 * Returns  -1 - error
 *           1 - OK
 */
int t_retransmit_reply( /* struct sip_msg * */  );




/* Force a new response into inbound response buffer.
 * returns 1 if everything was OK or -1 for erro
 */
int t_send_reply( struct sip_msg * , unsigned int , char *  , unsigned int);




/* releases T-context */
int t_unref( /* struct sip_msg* p_msg */ );



/* v6; -jiri
int t_forward_nonack( struct sip_msg* p_msg , unsigned int dest_ip_param ,
	unsigned int dest_port_param );
int t_forward_ack( struct sip_msg* p_msg , unsigned int dest_ip_param ,
	unsigned int dest_port_param );
*/
int t_forward_nonack( struct sip_msg* p_msg, struct proxy_l * p );
int t_forward_ack( struct sip_msg* p_msg );


int forward_serial_branch(struct cell* Trans,int branch);
struct cell* t_lookupOriginalT(  struct s_table* hash_table,
	struct sip_msg* p_msg );
int t_reply_matching( struct sip_msg* , int* ,  int* );
int t_store_incoming_reply( struct cell* , unsigned int , struct sip_msg* );
int t_lookup_request( struct sip_msg* p_msg , int leave_new_locked );
int t_all_final( struct cell * );
int t_build_and_send_ACK( struct cell *Trans , unsigned int brach ,
	struct sip_msg* rpl);
int t_should_relay_response( struct cell *Trans, int new_code, int branch,
	int *should_store );
int t_update_timers_after_sending_reply( struct retr_buf *rb );
int t_put_on_wait(  struct cell  *Trans  );
int relay_lowest_reply_upstream( struct cell *Trans , struct sip_msg *p_msg );
int add_branch_label( struct cell *Trans, struct sip_msg *p_msg , int branch );
int get_ip_and_port_from_uri( str* uri , unsigned int *param_ip,
	unsigned int *param_port);
int t_build_and_send_CANCEL(struct cell *Trans, unsigned int branch);
char *build_ack( struct sip_msg* rpl, struct cell *trans, int branch ,
	int *ret_len);

int t_addifnew( struct sip_msg* p_msg );

void timer_routine(unsigned int, void*);




inline int static attach_ack(  struct cell *t, int branch,
									char *ack, int ack_len )
{
	LOCK_ACK( t );
	if (t->uac[branch].request.ack) {
		UNLOCK_ACK(t);
		shm_free( ack );
		LOG(L_WARN, "attach_ack: Warning: ACK already sent out\n");
		return 0;
	}
	t->uac[branch].request.ack = ack;
	t->uac[branch].request.ack_len = ack_len;
	UNLOCK_ACK( t );
	return 1;
}




/* remove from timer list */
static inline void reset_timer( struct s_table *hash_table,
													struct timer_link* tl )
{
	/* lock(timer_group_lock[ tl->tg ]); */
	/* hack to work arround this timer group thing*/
	lock(hash_table->timers[timer_group[tl->tg]].mutex);
	remove_timer_unsafe( tl );
	unlock(hash_table->timers[timer_group[tl->tg]].mutex);
	/*unlock(timer_group_lock[ tl->tg ]);*/
}




/* determine timer length and put on a correct timer list */
static inline void set_timer( struct s_table *hash_table,
							struct timer_link *new_tl, enum lists list_id )
{
	unsigned int timeout;
	struct timer* list;


	if (list_id<FR_TIMER_LIST || list_id>=NR_OF_TIMER_LISTS) {
		LOG(L_CRIT, "ERROR: set_timer: unkown list: %d\n", list_id);
#ifdef EXTRA_DEBUG
		abort();
#endif
		return;
	}
	timeout = timer_id2timeout[ list_id ];
	list= &(hash_table->timers[ list_id ]);

	lock(list->mutex);
	/* make sure I'm not already on a list */
	remove_timer_unsafe( new_tl );
	add_timer_unsafe( list, new_tl, get_ticks()+timeout);
	unlock(list->mutex);
}




static inline void reset_retr_timers( struct s_table *h_table,
													struct cell *p_cell )
{
	int ijk;

	/* lock the first timer list of the FR group -- all other
	   lists share the same lock*/
	lock(hash_table->timers[RT_T1_TO_1].mutex);
	remove_timer_unsafe( & p_cell->uas.response.retr_timer );
	for( ijk=0 ; ijk<(p_cell)->nr_of_outgoings ; ijk++ )  {
		remove_timer_unsafe( & p_cell->uac[ijk].request.retr_timer );
	}
	unlock(hash_table->timers[RT_T1_TO_1].mutex);

	lock(hash_table->timers[FR_TIMER_LIST].mutex);
	remove_timer_unsafe( & p_cell->uas.response.fr_timer );
	for( ijk=0 ; ijk<(p_cell)->nr_of_outgoings ; ijk++ )  {
		remove_timer_unsafe( & p_cell->uac[ijk].request.fr_timer );
	}
	unlock(hash_table->timers[FR_TIMER_LIST].mutex);
	DBG("DEBUG:stop_RETR_and_FR_timers : timers stopped\n");
}

void delete_cell( struct cell *p_cell );

int t_newtran( struct sip_msg* p_msg );

#endif

