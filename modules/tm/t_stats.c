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
/*
 * History:
 * --------
 *  2003-06-27  tm_stats & friends freed on exit only if non-null (andrei)
 */


#include "defs.h"


#include <stdio.h>
#include "t_stats.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../fifo_server.h"
#include "../../unixsock_server.h"
#include "../../pt.h"

struct t_stats *tm_stats=0;


/* we don't worry about locking data during reads (unlike
   setting values which always happens from some locks) */
  
int print_stats(  FILE *f )
{
	unsigned long total, current, waiting, total_local;
	int i;
	int pno;

	pno=process_count();
	for(i=0, total=0, waiting=0, total_local=0; i<pno;i++) {
		total+=tm_stats->s_transactions[i];
		waiting+=tm_stats->s_waiting[i];
		total_local+=tm_stats->s_client_transactions[i];
	}
	current=total-tm_stats->deleted;
	waiting-=tm_stats->deleted;

	

	fprintf(f, "Current: %lu (%lu waiting) "
		"Total: %lu (%lu local) " CLEANUP_EOL,
		current, waiting, total, total_local);

	fprintf(f, "Replied localy: %lu" CLEANUP_EOL ,
		tm_stats->replied_localy );
	fprintf(f, "Completion status 6xx: %lu,",
		tm_stats->completed_6xx );
	fprintf(f, " 5xx: %lu,",
		tm_stats->completed_5xx );
	fprintf(f, " 4xx: %lu,",
		tm_stats->completed_4xx );
	fprintf(f, " 3xx: %lu,",
		tm_stats->completed_3xx );
	fprintf(f, "2xx: %lu" CLEANUP_EOL ,
		tm_stats->completed_2xx );
	
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
	fputs( "200 ok\n", file);
	print_stats( file );
	fclose(file);
	
	return 1;

}

int static unixsock_stats(str* cmd)
{
	unsigned long total, current, waiting, total_local;
	int i;
	int pno;

	unixsock_reply_asciiz( "200 OK\n");

	pno = process_count();
	for(i = 0, total = 0, waiting = 0, total_local = 0; i < pno; i++) {
		total += tm_stats->s_transactions[i];
		waiting += tm_stats->s_waiting[i];
		total_local += tm_stats->s_client_transactions[i];
	}
	current = total - tm_stats->deleted;
	waiting -= tm_stats->deleted;

	if (unixsock_reply_printf("Current: %lu (%lu waiting) "
				  "Total: %lu (%lu local) " CLEANUP_EOL,
				  current, waiting, total, total_local) < 0) goto err;

	if (unixsock_reply_printf("Replied localy: %lu" CLEANUP_EOL ,
				  tm_stats->replied_localy ) < 0) goto err;
	if (unixsock_reply_printf("Completion status 6xx: %lu,",
				  tm_stats->completed_6xx ) < 0) goto err;
	if (unixsock_reply_printf(" 5xx: %lu,",
				  tm_stats->completed_5xx ) < 0) goto err;
	if (unixsock_reply_printf(" 4xx: %lu,",
				  tm_stats->completed_4xx ) < 0) goto err;
	if (unixsock_reply_printf(" 3xx: %lu,",
				  tm_stats->completed_3xx ) < 0) goto err;
	if (unixsock_reply_printf("2xx: %lu" CLEANUP_EOL ,
				  tm_stats->completed_2xx ) < 0) goto err;
	
	unixsock_reply_send();
	return 0;

 err:
	unixsock_reply_reset();
	unixsock_reply_asciiz("500 Buffer too small\n");
	unixsock_reply_send();
	return -1;
}


int init_tm_stats(void)
{
	int size;

	tm_stats=shm_malloc(sizeof(struct t_stats));
	if (tm_stats==0) {
		LOG(L_ERR, "ERROR: init_tm_stats: no mem for stats\n");
		goto error0;
	}
	memset(tm_stats, 0, sizeof(struct t_stats) );

	size=sizeof(stat_counter)*process_count();
	tm_stats->s_waiting=shm_malloc(size);
	if (tm_stats->s_waiting==0) {
		LOG(L_ERR, "ERROR: init_tm_stats: no mem for stats\n");
		goto error1;
	}
	memset(tm_stats->s_waiting, 0, size );

	tm_stats->s_transactions=shm_malloc(size);
	if (tm_stats->s_transactions==0) {
		LOG(L_ERR, "ERROR: init_tm_stats: no mem for stats\n");
		goto error2;
	}
	memset(tm_stats->s_transactions, 0, size );

	tm_stats->s_client_transactions=shm_malloc(size);
	if (tm_stats->s_client_transactions==0) {
		LOG(L_ERR, "ERROR: init_tm_stats: no mem for stats\n");
		goto error3;
	}
	memset(tm_stats->s_client_transactions, 0, size );
		 
	if (register_fifo_cmd(fifo_stats, "t_stats", 0)<0) {
		LOG(L_CRIT, "cannot register fifo stats\n");
		goto error4;
	}

	if (unixsock_register_cmd("t_stats", unixsock_stats) < 0) {
		LOG(L_CRIT, "cannot register fifo stats\n");
		goto error4;
	}

	return 1;

error4:
	shm_free(tm_stats->s_client_transactions);
	tm_stats->s_client_transactions=0;
error3:
	shm_free(tm_stats->s_transactions);
	tm_stats->s_transactions=0;
error2:
	shm_free(tm_stats->s_waiting);
	tm_stats->s_waiting=0;
error1:
	shm_free(tm_stats);
error0:
	return -1;
}

void free_tm_stats()
{
	if (tm_stats!=0){
		if (tm_stats->s_client_transactions) 
			shm_free(tm_stats->s_client_transactions);
		if (tm_stats->s_transactions)
			shm_free(tm_stats->s_transactions);
		if (tm_stats->s_waiting)
			shm_free(tm_stats->s_waiting);
		shm_free(tm_stats);
	}
}
