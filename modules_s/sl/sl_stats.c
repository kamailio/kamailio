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


#include "../../mem/shm_mem.h"
#include "../../globals.h"
#include "../../fifo_server.h"
#include "../../config.h"
#include "../../pt.h"
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

	fprintf(reply_file, "200: %ld 202: %ld 2xx: %ld" CLEANUP_EOL,
		total.err[RT_200], total.err[RT_202], total.err[RT_2xx]);

	fprintf(reply_file, "300: %ld 301: %ld 302: %ld"
		" 3xx: %ld" CLEANUP_EOL,
		total.err[RT_300], total.err[RT_301], total.err[RT_302], 
		total.err[RT_3xx] );

	fprintf(reply_file, "400: %ld 401: %ld 403: %ld"
		" 404: %ld 407: %ld 408: %ld"
		" 483: %ld 4xx: %ld" CLEANUP_EOL,
		total.err[RT_400], total.err[RT_401], total.err[RT_403], 
		total.err[RT_404], total.err[RT_407], total.err[RT_408],
		total.err[RT_483], total.err[RT_4xx]);

	fprintf(reply_file, "500: %ld 5xx: %ld" CLEANUP_EOL,
		total.err[RT_500], total.err[RT_5xx] );

	fprintf(reply_file, "6xx: %ld" CLEANUP_EOL,
		total.err[RT_6xx] );

	fprintf(reply_file, "xxx: %ld" CLEANUP_EOL,
		total.err[RT_xxx] );

	fprintf(reply_file, "failures: %ld" CLEANUP_EOL,
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
	fputs( "200 ok\n", reply_file);
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

	len=sizeof(struct sl_stats)*process_count();
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
	sl_stats[ process_no ].failures++;
}

void update_sl_stats( int code ) 
{

	struct sl_stats *my_stats;

	my_stats=&sl_stats[ process_no ];

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
