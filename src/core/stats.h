/*
 *
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


#ifndef stats_h
#define stats_h

#include <ctype.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


#define _update_request( method, dir )			\
	do{ if (stat_file!=NULL) switch( method ) {	\
          	case METHOD_INVITE: stats->dir##_requests_inv++; break;	\
          	case METHOD_ACK: stats->dir##_requests_ack++; break;		\
          	case METHOD_CANCEL: stats->dir##_requests_cnc++; break;	\
          	case METHOD_BYE: stats->dir##_requests_bye++; break;		\
		case METHOD_INFO: stats->dir##_requests_info++; break;		\
          	case METHOD_OTHER: stats->dir##_requests_other++; break;	\
		default: LM_ERR("unknown method in rq stats (%s)\n", #dir);	\
		}	\
	}while(0)


/*
#define update_received_request( method ) _update_request( method, received )
#define update_sent_request( method ) _update_request( method, sent )

#define update_received_response( statusclass ) \
									_update_response( statusclass, received )
#define update_sent_response( statusclass ) \
									_update_response( statusclass, sent )
#define update_received_drops	{  stats->received_drops++; }
#define update_fail_on_send	{  stats->failed_on_send++; }
*/

#define         _statusline(class, dir )\
						case class: stats->dir##_responses_##class++; break;

/* FIXME: Don't have case for _other (see received_responses_other) */
#define _update_response( statusclass, dir )		\
        do{ if (stat_file!=NULL)                          \
                switch( statusclass ) {                 \
                        _statusline(1, dir)                   \
                        _statusline(2, dir)                   \
                        _statusline(3, dir)                   \
                        _statusline(4, dir)                   \
                        _statusline(5, dir)                   \
                        _statusline(6, dir)                   \
                        default: LM_INFO("unusual status code received in stats (%s)\n", #dir); \
                }       \
        }while(0)

#ifdef STATS
#	define STATS_RX_REQUEST(method) _update_request(method, received)
#	define STATS_TX_REQUEST(method) _update_request(method, sent )
#	define STATS_RX_RESPONSE(class) _update_response( class, received )
#	define STATS_TX_RESPONSE(class) _update_response( class, sent )
#	define STATS_RX_DROPS {  stats->received_drops++; }
#	define STATS_TX_DROPS {  stats->failed_on_send++; }
#else
#	define STATS_RX_REQUEST(method)
#	define STATS_TX_REQUEST(method)
#	define STATS_RX_RESPONSE(class) 
#	define STATS_TX_RESPONSE(class) 
#	define STATS_RX_DROPS 
#	define STATS_TX_DROPS 
#endif

#ifdef STATS


struct stats_s {

	unsigned int	process_index;
	pid_t		pid;
	time_t		start_time;

	unsigned long 

	/* received packets */

	received_requests_inv, 		/* received_requests */
	received_requests_ack,
	received_requests_cnc,
	received_requests_bye,
	received_requests_other,

	received_responses_1, 		/* received_responses */
	received_responses_2,
	received_responses_3,
	received_responses_4,
	received_responses_5,
	received_responses_6,
	received_responses_other,

	received_drops,	/* all messages we received and did not process
					   successfully; reasons include SIP sanity checks 
					   (missing Vias, neither request nor response, 
					   failed parsing), ser errors (malloc, action
					   failure)
					*/

	/* sent */

	/* sent_requests */
	sent_requests_inv,
	sent_requests_ack,
	sent_requests_cnc,
	sent_requests_bye,
	sent_requests_other,

	/* sent responses */
	sent_responses_1,
	sent_responses_2,
	sent_responses_3,
	sent_responses_4,
	sent_responses_5,
	sent_responses_6,
	/* FIXME: Don't want sent_responses_other?? */

	processed_requests,
	processed_responses,
	acc_req_time,
	acc_res_time,

	failed_on_send;			
};

extern struct stats_s *stats;
extern char *stat_file;

int init_stats( int nr_of_processes );
void setstats( int child_index );
int dump_all_statistic();
int dump_statistic(FILE *fp, struct stats_s *istats, int printheader);
/* Registers handlers with SNMP module */
int stats_register(); 

#endif
#endif
