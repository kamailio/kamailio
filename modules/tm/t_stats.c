/*
 *
 * $Id$
 *
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

	file=fopen(response_file, "w" );
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
