/*
 *
 * $Id$
 *
 */

#ifndef _T_STATS_H
#define _T_STATS_H

#include <time.h>

extern struct t_stats *cur_stats, *acc_stats;

struct t_stats {
	/* number of server transactions */
	unsigned int transactions;
	/* number of UAC transactions (part of transactions) */
	unsigned int client_transactions;
	/* number of transactions in wait state */
	unsigned int waiting;
	/* number of transactions which completed with this status */
	unsigned int completed_3xx, completed_4xx, completed_5xx, 
		completed_6xx, completed_2xx;
	unsigned int replied_localy;
	time_t up_since;
};

int init_stats(void);

#endif
