/*
 *
 * $Id$
 *
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



#include <stdio.h>
#include "t_stats.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../fifo_server.h"

struct t_stats *cur_stats, *acc_stats;


/* we don't worry about locking data during reads (unlike
   setting values which always happens from some locks) */
  
int print_stats(  FILE *f )
{
	fprintf(f, "Current:\n");
	fprintf(f, "# of transactions: %d, ", 
		cur_stats->transactions );
	fprintf(f, "local: %d, ",
		cur_stats->client_transactions );
	fprintf(f, "waiting: %d" CLEANUP_EOL ,
		cur_stats->waiting );

	fprintf(f, "Total:\n");
	fprintf(f, "# of transactions: %d,",
		acc_stats->transactions );
	fprintf(f, " local: %d,",
		acc_stats->client_transactions );
	fprintf(f, " waiting: %d" CLEANUP_EOL ,
		acc_stats->waiting );

	fprintf(f, "Replied localy: %d" CLEANUP_EOL ,
		acc_stats->replied_localy );
	fprintf(f, "Completion status 6xx: %d,",
		acc_stats->completed_6xx );
	fprintf(f, " 5xx: %d,",
		acc_stats->completed_5xx );
	fprintf(f, " 4xx: %d,",
		acc_stats->completed_4xx );
	fprintf(f, " 3xx: %d,",
		acc_stats->completed_3xx );
	fprintf(f, "2xx: %d" CLEANUP_EOL ,
		acc_stats->completed_2xx );
	
	return 1;
}

int static fifo_stats( FILE *pipe, char *response_file )
{
	FILE *file;

	if (response_file==0 || *response_file==0 ) {
		LOG(L_ERR, "ERROR: fifo_stats: null file\n");
		return -1;
	}

	file=open_reply_pipe(response_file );
	if (file==NULL) {
		LOG(L_ERR, "ERROR: fifo_stats: file %s bad: %s\n",
			response_file, strerror(errno) );
		return -1;
	}
	print_stats( file );
	fclose(file);
	
	return 1;

}

int init_tm_stats(void)
{
	cur_stats=shm_malloc(sizeof(struct t_stats));
	if (cur_stats==0) {
		LOG(L_ERR, "ERROR: init_stats: no mem for stats\n");
		return -1;
	}
	acc_stats=shm_malloc(sizeof(struct t_stats));
	if (acc_stats==0) {
		LOG(L_ERR, "ERROR: init_stats: no mem for stats\n");
		shm_free(cur_stats);
		return -1;
	}

	if (register_fifo_cmd(fifo_stats, "t_stats", 0)<0) {
		LOG(L_CRIT, "cannot register fifo stats\n");
		return -1;
	}

	memset(cur_stats, 0, sizeof(struct t_stats) );
	memset(acc_stats, 0, sizeof(struct t_stats) );
	return 1;
}
