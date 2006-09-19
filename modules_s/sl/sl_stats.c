/*
 *
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
#include "../../config.h"
#include "../../pt.h"
#include "sl_stats.h"
#include <strings.h>
#include <stdio.h>


static struct sl_stats** sl_stats;


static void add_sl_stats( struct sl_stats *t, struct sl_stats *i)
{
	enum reply_type rt;

	for (rt=0; rt<RT_END; rt++) 
		t->err[rt]+=i->err[rt];
	t->failures+=i->failures;
}


static const char* rpc_stats_doc[2] = {
	"Print reply statistics.",
	0
};

static void rpc_stats(rpc_t* rpc, void* c)
{
	void* st;
	struct sl_stats total;
	int p;
	int procs_no;

	memset(&total, 0, sizeof(struct sl_stats));
	if (dont_fork) {
		add_sl_stats(&total, &(*sl_stats)[0]);
	} else{
		procs_no=get_max_procs();
		for (p=0; p < procs_no; p++)
			add_sl_stats(&total, &(*sl_stats)[p]);
	}

	if (rpc->add(c, "{", &st) < 0) return;
	
	rpc->struct_add(st, "ddd", 
			"200", total.err[RT_200],
			"202", total.err[RT_202],
			"2xx", total.err[RT_2xx]);

	rpc->struct_add(st, "ddd",
			"300", total.err[RT_300],
			"301", total.err[RT_301],
			"302", total.err[RT_302],
			"3xx", total.err[RT_3xx]);

	rpc->struct_add(st, "dddddddd",
			"400", total.err[RT_400],
			"401", total.err[RT_401],
			"403", total.err[RT_403],
			"404", total.err[RT_404],
			"407", total.err[RT_407],
			"408", total.err[RT_408],
			"483", total.err[RT_483],
			"4xx", total.err[RT_4xx]);

	rpc->struct_add(st, "dd",
			"500", total.err[RT_500],
			"5xx", total.err[RT_5xx]);

	rpc->struct_add(st, "d", "6xx", total.err[RT_6xx]);
	rpc->struct_add(st, "d", "xxx", total.err[RT_xxx]);
}


void sl_stats_destroy(void)
{
	if (!sl_stats) return;
	if (*sl_stats) shm_free(*sl_stats);
	shm_free(sl_stats);
}

int init_sl_stats(void) 
{
	sl_stats = (struct sl_stats**)shm_malloc(sizeof(struct sl_stats*));
	if (!sl_stats) {
		ERR("Unable to allocated shared memory for sl statistics\n");
		return -1;
	}
	*sl_stats = 0;
	return 0;
}


int init_sl_stats_child(void)
{
	int len;

	len = sizeof(struct sl_stats) * get_max_procs();
	*sl_stats = shm_malloc(len);
	if (*sl_stats == 0) {
		ERR("No shmem\n");
		shm_free(sl_stats);
		return -1;
	}
	memset(*sl_stats, 0, len);
	return 0;
}


void update_sl_failures( void )
{
	(*sl_stats)[process_no].failures++;
}

void update_sl_stats( int code ) 
{

	struct sl_stats *my_stats;

	my_stats=&(*sl_stats)[process_no];

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

rpc_export_t sl_rpc[] = {
	{"sl.stats", rpc_stats, rpc_stats_doc, 0},
	{0, 0, 0, 0}
};
