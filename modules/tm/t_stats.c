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

int print_stats(  FILE *f )
{
	time_t now;

	time(&now);

	fprintf(f, "Time:\n----------------\n");
	fprintf(f, "Now: %s", ctime(&now));
	fprintf(f, "Up since: %s", ctime(&acc_stats->up_since));
	fprintf(f, "Up time: %.f [sec]\n", difftime(now, acc_stats->up_since));
	fprintf(f, "\nCurrent values:\n----------------\n");
	fprintf(f, "# of transactions: %d\n", 
		cur_stats->transactions );
	fprintf(f, "    - local: %d\n",
		cur_stats->client_transactions );
	fprintf(f, "    - waiting: %d\n",
		cur_stats->waiting );

	fprintf(f, "\nCummulative values:\n----------------\n");
	fprintf(f, "# of transactions: %d\n",	
		acc_stats->transactions );
	fprintf(f, "    - local: %d\n",
		acc_stats->client_transactions );
	fprintf(f, "    - waiting: %d\n",
		acc_stats->waiting );

	fprintf(f, "Replied localy: %d\n",
		acc_stats->replied_localy );
	fprintf(f, "Completion status 6xx: %d\n",
		acc_stats->completed_6xx );
	fprintf(f, "Completion status 5xx: %d\n",
		acc_stats->completed_5xx );
	fprintf(f, "Completion status 4xx: %d\n",
		acc_stats->completed_4xx );
	fprintf(f, "Completion status 3xx: %d\n",
		acc_stats->completed_3xx );
	fprintf(f, "Completion status 2xx: %d\n",
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

int init_stats(void)
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
	time(&acc_stats->up_since);
	return 1;
}
