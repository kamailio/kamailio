/*
 *
 * $Id$
 *
 */

#include "../../mem/shm_mem.h"
#include "../../globals.h"
#include "../../fifo_server.h"
#include "../../config.h"
#include "sl_stats.h"
#include <strings.h>
#include <stdio.h>


static struct sl_stats *sl_stats;


static void add_sl_stats( struct sl_stats *t, struct sl_stats *i)
{
	enum reply_type rt;

	for (rt=0; rt<RT_END; rt++) 
		t->err[rt]+=i->err[rt];
	t->failures+=i->failures;
}


static int print_sl_stats(FILE *reply_file)
{
	struct sl_stats total;
	int b, p;

	memset(&total, 0, sizeof(struct sl_stats));
	if (dont_fork) {
		add_sl_stats(&total, &sl_stats[0]);
	} else for (b=0; b<sock_no; b++)
		for (p=0; p<children_no; p++) 
			add_sl_stats(&total, &sl_stats[b*children_no+p]);

	fprintf(reply_file, "200: %d 202: %d 2xx: %d" CLEANUP_EOL,
		total.err[RT_200], total.err[RT_202], total.err[RT_2xx]);

	fprintf(reply_file, "300: %d 301: %d 302: %d"
		" 3xx: %d" CLEANUP_EOL,
		total.err[RT_300], total.err[RT_301], total.err[RT_302], 
		total.err[RT_3xx] );

	fprintf(reply_file, "400: %d 401: %d 403: %d"
		" 404: %d 407: %d 408: %d"
		" 483: %d 4xx: %d" CLEANUP_EOL,
		total.err[RT_400], total.err[RT_401], total.err[RT_403], 
		total.err[RT_404], total.err[RT_407], total.err[RT_408],
		total.err[RT_483], total.err[RT_4xx]);

	fprintf(reply_file, "500: %d 5xx: %d" CLEANUP_EOL,
		total.err[RT_500], total.err[RT_5xx] );

	fprintf(reply_file, "6xx: %d" CLEANUP_EOL,
		total.err[RT_6xx] );

	fprintf(reply_file, "xxx: %d" CLEANUP_EOL,
		total.err[RT_xxx] );

	fprintf(reply_file, "failures: %d" CLEANUP_EOL,
		total.failures );

	return 1;
}

int static sl_stats_cmd( FILE *pipe, char *response_file )
{
	FILE *reply_file;

	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "ERROR: sl_stats: file not opened\n");
		return -1;
	}
	print_sl_stats( reply_file );
	fclose(reply_file);
	return 1;
}

void sl_stats_destroy()
{
	shm_free(sl_stats);
}

int init_sl_stats( void ) 
{
	int len;

	len=sizeof(struct sl_stats)*(1+(dont_fork ? 1 : children_no*sock_no));
	sl_stats=shm_malloc(len);
	if (sl_stats==0) {
		LOG(L_ERR, "ERROR: init_sl_stats: no shmem\n");
		return -1;
	}
	memset(sl_stats, 0, len);
	if (register_fifo_cmd(sl_stats_cmd, "sl_stats", 0)<0) {
		LOG(L_CRIT, "cannot register sl_stats\n");
		return -1;
	}
	return 1;
}

void update_sl_failures( void )
{
	struct sl_stats *my_stats;

	my_stats=&sl_stats[ bind_idx * children_no + process_no ];
	my_stats->failures++;
}

void update_sl_stats( int code ) 
{

	struct sl_stats *my_stats;

	my_stats=&sl_stats[ bind_idx * children_no + process_no ];

	if (code >=700 || code <200 ) {
		my_stats->err[RT_xxx]++;
	} else if (code>=600) {
		my_stats->err[RT_6xx]++;
	} else if (code>=500) {
		switch(code) {
			case 500:	my_stats->err[RT_500]++;
						break;
			default:	my_stats->err[RT_5xx]++;
						break;
		}
	} else if (code>=400) {
		switch(code) {
			case 400:	my_stats->err[RT_400]++;
						break;
			case 401:	my_stats->err[RT_401]++;
						break;
			case 403:	my_stats->err[RT_403]++;
						break;
			case 404:	my_stats->err[RT_404]++;
						break;
			case 407:	my_stats->err[RT_407]++;
						break;
			case 408:	my_stats->err[RT_408]++;
						break;
			case 483:	my_stats->err[RT_483]++;
						break;
			default:	my_stats->err[RT_4xx]++;
						break;
		}
	} else if (code>=300) {
		switch(code) {
			case 300:	my_stats->err[RT_300]++;
						break;
			case 301:	my_stats->err[RT_301]++;
						break;
			case 302:	my_stats->err[RT_302]++;
						break;
			default:	my_stats->err[RT_3xx]++;
						break;
		}
	} else { /* 2xx */
		switch(code) {
			case 200:	my_stats->err[RT_200]++;
						break;
			case 202:	my_stats->err[RT_202]++;
						break;
			default:	my_stats->err[RT_2xx]++;
						break;
		}
	}
		
}
