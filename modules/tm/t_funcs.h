/*
 * $Id$
 */


#ifndef _T_FUNCS_H
#define _T_FUNCS_H

#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>

#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../globals.h"
#include "../../udp_server.h"
#include "../../msg_translator.h"
#include "../../timer.h"
#include "../../forward.h"
#include "../../mem/mem.h"
#include "../../md5utils.h"
#include "../../ip_addr.h"
#include "../../parser/parse_uri.h"

#include "config.h"
#include "lock.h"
#include "timer.h"
#include "sip_msg.h"
#include "h_table.h"
#include "ut.h"


struct s_table;
struct timer;
struct entry;
struct cell;

extern struct s_table*  hash_table;
extern int noisy_ctimer;


#define LOCK_HASH(_h) lock(&(hash_table->entrys[(_h)].mutex))
#define UNLOCK_HASH(_h) unlock(&(hash_table->entrys[(_h)].mutex))

#ifdef _OBSOLETED
#define LOCK_ACK(_t) lock(&(_t)->ack_mutex )
#define UNLOCK_ACK(_t) unlock(&(_t)->ack_mutex )
#endif

#ifdef _XWAIT
	#define LOCK_WAIT(_t) lock(&(_t)->wait_mutex )
	#define UNLOCK_WAIT(_t) unlock(&(_t)->wait_mutex )
#else
	#define LOCK_WAIT(_t)
	#define UNLOCK_WAIT(_t)
#endif

/* send a private buffer: utilize a retransmission structure
   but take a separate buffer not refered by it; healthy
   for reducing time spend in REPLIES locks
*/

int send_pr_buffer( struct retr_buf *rb,
	void *buf, int len, char *function, int line );

/* send a buffer -- 'PR' means private, i.e., it is assumed noone
   else can affect the buffer during sending time
*/
#define SEND_PR_BUFFER(_rb,_bf,_le ) \
	send_pr_buffer( (_rb), (_bf), (_le),  __FUNCTION__, __LINE__ )

#define SEND_BUFFER( _rb ) \
	SEND_PR_BUFFER( (_rb) , (_rb)->buffer , (_rb)->buffer_len )


#define UNREF_UNSAFE(_T_cell) ({  (_T_cell)->ref_count--; })
#define UNREF(_T_cell) ({ \
	LOCK_HASH( (_T_cell)->hash_index ); \
	UNREF_UNSAFE(_T_cell); \
	UNLOCK_HASH( (_T_cell)->hash_index ); })
#define REF_UNSAFE(_T_cell) ({  (_T_cell)->ref_count++; })
#define REF(_T_cell) ({ \
	LOCK_HASH( (_T_cell)->hash_index ); \
	REF_UNSAFE(_T_cell); \
	UNLOCK_HASH( (_T_cell)->hash_index ); })
#define INIT_REF_UNSAFE(_T_cell) (_T_cell)->ref_count=1
#define IS_REFFED_UNSAFE(_T_cell) ((_T_cell)->ref_count!=0)


int   tm_startup();
void tm_shutdown();


/* function returns:
 *       1 - a new transaction was created
 *      -1 - error, including retransmission
 */
int  t_add_transaction( struct sip_msg* p_msg  );


/* returns 1 if everything was OK or -1 for error
 */
int t_release_transaction( struct cell *trans );


/* int forward_serial_branch(struct cell* Trans,int branch); */
int t_put_on_wait(  struct cell  *Trans  );
int get_ip_and_port_from_uri( str* uri , unsigned int *param_ip,
	unsigned int *param_port);


void timer_routine(unsigned int, void*);

int t_newtran( struct sip_msg* p_msg );

void put_on_wait(  struct cell  *Trans  );

void start_retr( struct retr_buf *rb );

void cleanup_localcancel_timers( struct cell *t );

int t_relay_to( struct sip_msg  *p_msg ,
	struct proxy_l *proxy, int replicate ) ;


#endif

