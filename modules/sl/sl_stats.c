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

#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "../../globals.h"
#include "../../config.h"
#include "../../pt.h"
#include "sl_stats.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include <strings.h>
#include <stdio.h>


static struct sl_stats** sl_stats;


static void add_sl_stats( struct sl_stats *t, struct sl_stats *i)
{
	enum reply_type rt;

	for (rt=0; rt<RT_END; rt++) { 
		t->err[rt]+=i->err[rt];
		t->all_replies+=i->err[rt];
	}
	t->failures+=i->failures;
	t->filtered_acks+=i->filtered_acks;
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

void update_sl_err_replies( void )
{
	(*sl_stats)[process_no].err_replies++;
}

void update_sl_filtered_acks( void )
{
	(*sl_stats)[process_no].filtered_acks++;
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

#ifdef STATISTICS

/* k statistics */

unsigned long sl_stats_RT_1xx(void);
unsigned long sl_stats_RT_200(void);
unsigned long sl_stats_RT_202(void);
unsigned long sl_stats_RT_2xx(void);
unsigned long sl_stats_RT_300(void);
unsigned long sl_stats_RT_301(void);
unsigned long sl_stats_RT_302(void);
unsigned long sl_stats_RT_3xx(void);
unsigned long sl_stats_RT_400(void);
unsigned long sl_stats_RT_401(void);
unsigned long sl_stats_RT_403(void);
unsigned long sl_stats_RT_404(void);
unsigned long sl_stats_RT_407(void);
unsigned long sl_stats_RT_408(void);
unsigned long sl_stats_RT_483(void);
unsigned long sl_stats_RT_4xx(void);
unsigned long sl_stats_RT_500(void);
unsigned long sl_stats_RT_5xx(void);
unsigned long sl_stats_RT_6xx(void);
unsigned long sl_stats_RT_xxx(void);

unsigned long sl_stats_sent_rpls(void);
unsigned long sl_stats_sent_err_rpls(void);
unsigned long sl_stats_failures(void);
unsigned long sl_stats_rcv_acks(void);

static stat_export_t mod_stats[] = {
	{"1xx_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_1xx    },
	{"200_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_200    },
	{"202_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_202    },
	{"2xx_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_2xx    },
	{"300_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_300    },
	{"301_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_301    },
	{"302_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_302    },
	{"3xx_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_3xx    },
	{"400_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_400    },
	{"401_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_401    },
	{"403_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_403    },
	{"404_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_404    },
	{"407_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_407    },
	{"408_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_408    },
	{"483_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_483    },
	{"4xx_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_4xx    },
	{"500_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_500    },
	{"5xx_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_5xx    },
	{"6xx_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_6xx    },
	{"xxx_replies" ,       STAT_IS_FUNC,
		(stat_var**)sl_stats_RT_xxx    },

	{"sent_replies" ,      STAT_IS_FUNC,
		(stat_var**)sl_stats_sent_rpls      },
	{"sent_err_replies" ,  STAT_IS_FUNC,
		(stat_var**)sl_stats_sent_err_rpls  },
	{"failures" ,          STAT_IS_FUNC,
		(stat_var**)sl_stats_failures       },
	{"received_ACKs" ,     STAT_IS_FUNC,
		(stat_var**)sl_stats_rcv_acks       },
	{0,0,0}
};


static struct sl_stats _sl_stats_total;
static ticks_t _sl_stats_tm = 0;

static void sl_stats_update(void)
{
	int p;
	int procs_no;
	ticks_t t;

	t = get_ticks();
	if(t==_sl_stats_tm)
		return;
	_sl_stats_tm = t;

	memset(&_sl_stats_total, 0, sizeof(struct sl_stats));
	if (dont_fork) {
		add_sl_stats(&_sl_stats_total, &(*sl_stats)[0]);
	} else{
		procs_no=get_max_procs();
		for (p=0; p < procs_no; p++)
			add_sl_stats(&_sl_stats_total, &(*sl_stats)[p]);
	}
}

unsigned long sl_stats_tx_1xx_rpls(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_1xx];
}

unsigned long sl_stats_RT_1xx(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_1xx];
}

unsigned long sl_stats_RT_200(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_200];
}

unsigned long sl_stats_RT_202(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_202];
}

unsigned long sl_stats_RT_2xx(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_2xx];
}

unsigned long sl_stats_RT_300(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_300];
}

unsigned long sl_stats_RT_301(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_301];
}

unsigned long sl_stats_RT_302(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_302];
}

unsigned long sl_stats_RT_3xx(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_3xx];
}

unsigned long sl_stats_RT_400(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_400];
}

unsigned long sl_stats_RT_401(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_401];
}

unsigned long sl_stats_RT_403(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_403];
}

unsigned long sl_stats_RT_404(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_404];
}

unsigned long sl_stats_RT_407(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_407];
}

unsigned long sl_stats_RT_408(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_408];
}

unsigned long sl_stats_RT_483(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_483];
}

unsigned long sl_stats_RT_4xx(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_4xx];
}

unsigned long sl_stats_RT_500(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_500];
}

unsigned long sl_stats_RT_5xx(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_5xx];
}

unsigned long sl_stats_RT_6xx(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_6xx];
}

unsigned long sl_stats_RT_xxx(void)
{
	sl_stats_update();
	return _sl_stats_total.err[RT_xxx];
}


unsigned long sl_stats_sent_rpls(void)
{
	sl_stats_update();
	return _sl_stats_total.all_replies;
}

unsigned long sl_stats_sent_err_rpls(void)
{
	sl_stats_update();
	return _sl_stats_total.err_replies;
}

unsigned long sl_stats_failures(void)
{
	sl_stats_update();
	return _sl_stats_total.failures;
}

unsigned long sl_stats_rcv_acks(void)
{
	sl_stats_update();
	return _sl_stats_total.filtered_acks;
}

#endif

int sl_register_kstats(void)
{
#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats("sl", mod_stats)!=0 ) {
		LM_ERR("failed to register statistics\n");
		return -1;
	}
#endif
	return 0;
}

