/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../usr_avp.h"
#include "../../atomic_ops.h"

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

extern int tm_error; /* delayed tm error */
extern struct msgid_var user_cell_set_flags;
extern struct msgid_var user_cell_reset_flags;

extern int fr_inv_timer_avp_type;
extern int_str fr_inv_timer_avp;
extern str contacts_avp;
extern str contact_flows_avp;

/* default names for timer's AVPs  */
#define FR_TIMER_AVP      "callee_fr_timer"
#define FR_INV_TIMER_AVP  "callee_fr_inv_timer"


/* send a private buffer: utilize a retransmission structure
   but take a separate buffer not referred by it; healthy
   for reducing time spend in REPLIES locks
*/


/* send a buffer -- 'PR' means private, i.e., it is assumed noone
   else can affect the buffer during sending time
*/
#ifdef EXTRA_DEBUG
int send_pr_buffer( struct retr_buf *rb,
	void *buf, int len, char* file, const char *function, int line );
#define SEND_PR_BUFFER(_rb,_bf,_le ) \
	send_pr_buffer( (_rb), (_bf), (_le), __FILE__,  __FUNCTION__, __LINE__ )
#else
int send_pr_buffer( struct retr_buf *rb, void *buf, int len);
#define SEND_PR_BUFFER(_rb,_bf,_le ) \
	send_pr_buffer( (_rb), (_bf), (_le))
#endif

#define SEND_BUFFER( _rb ) \
	SEND_PR_BUFFER( (_rb) , (_rb)->buffer , (_rb)->buffer_len )



#ifdef TM_DEL_UNREF

#define UNREF_FREE(_T_cell) \
	do{\
		if (atomic_dec_and_test(&(_T_cell)->ref_count)){ \
			unlink_timers((_T_cell)); \
			free_cell((_T_cell)); \
		}else \
			t_stats_delayed_free(); \
	}while(0)

#define UNREF_NOSTATS(_T_cell) \
	do{\
		if (atomic_dec_and_test(&(_T_cell)->ref_count)){ \
			unlink_timers((_T_cell)); \
			free_cell((_T_cell)); \
		}\
	}while(0)

#define UNREF_UNSAFE(_T_cell) UNREF_NOSTATS(_T_cell)
/* all the version are safe when using atomic ops */
#define UNREF(_T_cell) UNREF_UNSAFE(_T_cell); 
#define REF(_T_cell) (atomic_inc(&(_T_cell)->ref_count))
#define REF_UNSAFE(_T_cell)  REF(_T_cell)
#define INIT_REF(_T_cell, v) atomic_set(&(_T_cell)->ref_count, v)

#else

#define UNREF_UNSAFE(_T_cell) ((_T_cell)->ref_count--)
#define UNREF(_T_cell) do{ \
	LOCK_HASH( (_T_cell)->hash_index ); \
	UNREF_UNSAFE(_T_cell); \
	UNLOCK_HASH( (_T_cell)->hash_index ); }while(0)
#define REF_UNSAFE(_T_cell) ((_T_cell)->ref_count++)
#define INIT_REF_UNSAFE(_T_cell) ((_T_cell)->ref_count=1)
#define IS_REFFED_UNSAFE(_T_cell) ((_T_cell)->ref_count!=0)

#endif
/*
 * Parse and fixup the fr_*_timer AVP specs
 */
int init_avp_params(char *fr_timer_param, char *fr_inv_timer_param);


typedef void (*unref_cell_f)(struct cell *t);
void unref_cell(struct cell *t);
/*
 * Get the FR_{INV}_TIMER from corresponding AVP
 */
int fr_avp2timer(unsigned int* timer);
int fr_inv_avp2timer(unsigned int* timer);


#ifdef TIMER_DEBUG
#define start_retr(rb) \
	_set_fr_retr((rb), \
				((rb)->dst.proto==PROTO_UDP) ? RT_T1_TIMEOUT_MS(rb) : \
												(unsigned)(-1), \
				__FILE__, __FUNCTION__, __LINE__)

#define force_retr(rb) \
	_set_fr_retr((rb), RT_T1_TIMEOUT_MS(rb), __FILE__, __FUNCTION__, __LINE__)

#else
#define start_retr(rb) \
	_set_fr_retr((rb), \
				((rb)->dst.proto==PROTO_UDP) ? RT_T1_TIMEOUT_MS(rb) : \
												(unsigned)(-1))

#define force_retr(rb) \
	_set_fr_retr((rb), RT_T1_TIMEOUT_MS(rb))

#endif




void tm_shutdown(void);


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


int t_relay_to( struct sip_msg  *p_msg ,
	struct proxy_l *proxy, int proto, int replicate ) ;

int kill_transaction( struct cell *trans, int error );
int kill_transaction_unsafe( struct cell *trans, int error );
#endif

