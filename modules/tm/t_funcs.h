/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 /* History:
  * --------
  *  2003-02-18  updated various function prototypes (andrei)
  *  2003-03-10  removed ifdef _OBSO & made redefined all the *UNREF* macros
  *               in a non-gcc specific way (andrei)
  *  2003-03-13  now send_pr_buffer will be called w/ function/line info
  *               only when compiling w/ -DEXTRA_DEBUG (andrei)
  *  2003-03-31  200 for INVITE/UAS resent even for UDP (jiri) 
  */



#ifndef _T_FUNCS_H
#define _T_FUNCS_H

#include "defs.h"


#include <errno.h>
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

extern int noisy_ctimer;


/* send a private buffer: utilize a retransmission structure
   but take a separate buffer not refered by it; healthy
   for reducing time spend in REPLIES locks
*/


/* send a buffer -- 'PR' means private, i.e., it is assumed noone
   else can affect the buffer during sending time
*/
#ifdef EXTRA_DEBUG
int send_pr_buffer( struct retr_buf *rb,
	void *buf, int len, char* file, char *function, int line );
#define SEND_PR_BUFFER(_rb,_bf,_le ) \
	send_pr_buffer( (_rb), (_bf), (_le), __FILE__,  __FUNCTION__, __LINE__ )
#else
int send_pr_buffer( struct retr_buf *rb, void *buf, int len);
#define SEND_PR_BUFFER(_rb,_bf,_le ) \
	send_pr_buffer( (_rb), (_bf), (_le))
#endif

#define SEND_BUFFER( _rb ) \
	SEND_PR_BUFFER( (_rb) , (_rb)->buffer , (_rb)->buffer_len )


#define UNREF_UNSAFE(_T_cell) ((_T_cell)->ref_count--)
#define UNREF(_T_cell) do{ \
	LOCK_HASH( (_T_cell)->hash_index ); \
	UNREF_UNSAFE(_T_cell); \
	UNLOCK_HASH( (_T_cell)->hash_index ); }while(0)
#define REF_UNSAFE(_T_cell) ((_T_cell)->ref_count++)
#define INIT_REF_UNSAFE(_T_cell) ((_T_cell)->ref_count=1)
#define IS_REFFED_UNSAFE(_T_cell) ((_T_cell)->ref_count!=0)


static void inline _set_fr_retr( struct retr_buf *rb, int retr )
{
	if (retr) {
		rb->retr_list=RT_T1_TO_1;
		set_timer( &rb->retr_timer, RT_T1_TO_1 );
	}
	set_timer(&rb->fr_timer, FR_TIMER_LIST);
}

static void inline start_retr(struct retr_buf *rb)
{
	_set_fr_retr(rb, rb->dst.proto==PROTO_UDP);
}

static void inline force_retr(struct retr_buf *rb)
{
	_set_fr_retr(rb, 1);
}


void tm_shutdown();


/* function returns:
 *       1 - a new transaction was created
 *      -1 - error, including retransmission
 */
int  t_add_transaction( struct sip_msg* p_msg  );


/* returns 1 if everything was OK or -1 for error */
int t_release_transaction( struct cell *trans );


int get_ip_and_port_from_uri( str* uri , unsigned int *param_ip,
	unsigned int *param_port);


void put_on_wait(  struct cell  *Trans  );

#ifdef _OBSOLETED
void start_retr( struct retr_buf *rb );
#endif

void cleanup_localcancel_timers( struct cell *t );

int t_relay_to( struct sip_msg  *p_msg ,
	struct proxy_l *proxy, int proto, int replicate ) ;

int t_write_req(struct sip_msg* msg,
	char* vm_fifo, char* action);

int init_twrite_lines();

#endif

