/* 
 * Stats reporting code. It reports through SIG_USR1 and if loaded
 * through the SNMP module
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
 *
 */

/*!
 * \file
 * \brief Kamailio core :: Stats reporting code
 * Stats reporting code. It reports through SIG_USR1 and if loaded
 * through the SNMP module
 * \ingroup core
 * Module: \ref core
 */


#ifdef STATS
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "dprint.h"
#include "mem/shm_mem.h"
#include "sr_module.h"

/* SNMP includes */
#include "modules/snmp/snmp_handler.h"
#include "modules/snmp/sipCommonStatsMethod.h"
#include "modules/snmp/sipCommonStatusCode.h"

struct stats_s *stats;		/* per process stats structure */ 
char *stat_file = NULL;		/* set by the parser */

/* private variables */
static struct stats_s *global_stats=NULL;
static int stats_segments=-1;	/*used also to determine if we've been init'ed*/

/* adds up global statistics and puts them into passed struct.
 * -1 returned on failure */
static int collect_stats(struct stats_s *s);

/***********************8 SNMP Stuff **************************/
/* a small structure we use to pass around the functions needed by
 * all the registration functions */
struct stats_funcs {
	int (*reg_func)(const char *, struct sip_snmp_handler*);
	struct sip_snmp_handler* (*new_func)(size_t);
	void (*free_func)(struct sip_snmp_handler*);
};

/* SNMP Handler registration functions */
static int sipSummaryStatsTable_register(const struct stats_funcs *f);
static int sipMethodStatsTable_register(const struct stats_funcs *f);
static int sipStatusCodesTable_register(const struct stats_funcs *f);

/* the handlers */
static int collect_InReqs(struct sip_snmp_obj *, enum handler_op);
static int collect_OutReqs(struct sip_snmp_obj *, enum handler_op);
static int collect_InResp(struct sip_snmp_obj *, enum handler_op);
static int collect_OutResp(struct sip_snmp_obj *, enum handler_op);
static int sipStatsMethod_handler(struct sip_snmp_obj *o, enum handler_op op);
static int sipStatusCodes_handler(struct sip_snmp_obj *o, enum handler_op op);

int init_stats(int nr_of_processes)
{
	LM_DBG("initializing stats for %d processes\n", 
		nr_of_processes);


	global_stats = shm_malloc(nr_of_processes*sizeof(struct stats_s));
	if(!global_stats) {
		LM_ERR("Out of memory\n");
		return -1;
	}
	stats_segments = nr_of_processes;

	if(stats_register() == -1)
		LM_WARN("Couldn't register stats with snmp module\n");


	return 0;
}

/* sets the stats pointer for the passed process */
void setstats(int child_index)
{
	if(stats_segments == -1 || !global_stats) {
		LM_ERR("Stats not initialized. Cannot set them\n");
		stats = NULL;
		return;
	}
	if(child_index < 0 || child_index >= stats_segments) {
		stats = NULL;
		LM_ERR("Invalid index %d while setting statistics. Only have "
			"space for %d processes\n", child_index, stats_segments);
		return;
	}
	stats = global_stats+child_index;
	stats->process_index = child_index;
	/* can't use pids[] because we may be called before the corresponding
	 * slot in pids[] is initialized (chk main_loop()) */
	stats->pid = getpid();
	stats->start_time = time(NULL);
}

/* printheader is used to print pid, date and index */
int dump_statistic(FILE *fp, struct stats_s *istats, int printheader)
{
	struct tm res;
	char t[256];
	if(stats_segments == -1 || !global_stats) {
		LM_ERR("Stats \"engine\" not initialized\n");
		return -1;
	}

	if(printheader) {
		localtime_r(&istats->start_time, &res);
		strftime(t, 255, "%c", &res);
		
		fprintf(fp, "stats for process %d (pid %d) started at %s\n", 
				istats->process_index, istats->pid, t);
	}

	fprintf(fp, "received requests:\ninv: %ld\tack: %ld\tcnc: %ld\t"
		"bye: %ld\tother: %ld\n",
		istats->received_requests_inv,
		istats->received_requests_ack,
		istats->received_requests_cnc,
		istats->received_requests_bye,
		istats->received_requests_other);
	fprintf(fp, "sent requests:\n"
		"inv: %ld\tack: %ld\tcnc: %ld\tbye: %ld\tother: %ld\n",
		istats->sent_requests_inv,
		istats->sent_requests_ack,
		istats->sent_requests_cnc,
		istats->sent_requests_bye,
		istats->sent_requests_other);
	fprintf(fp, "received responses:\n"
		"1: %ld\t2: %ld\t3: %ld\t4: %ld\t5: %ld\t6: %ld\tother: %ld\t"
		"drops: %ld\n",
		istats->received_responses_1,
		istats->received_responses_2,
		istats->received_responses_3,
		istats->received_responses_4,
		istats->received_responses_5,
		istats->received_responses_6,
		istats->received_responses_other,
		istats->received_drops);
	fprintf(fp, "sent responses:\n"
		"1: %ld\t2: %ld\t3: %ld\t4: %ld\t5: %ld\t6: %ld\n",
		istats->sent_responses_1,
		istats->sent_responses_2,
		istats->sent_responses_3,
		istats->sent_responses_4,
		istats->sent_responses_5,
		istats->sent_responses_6);
	fprintf(fp, "processed requests: %ld\t\tprocessed responses: %ld\n"
		"acc req time: %ld\t\t\tacc res time: %ld\nfailed on send: %ld\n\n",
		istats->processed_requests,
		istats->processed_responses,
		istats->acc_req_time,
		istats->acc_res_time,
		istats->failed_on_send);
	return 0;
}

int dump_all_statistic()
{
	register int i;
	register struct stats_s *c;
	static struct stats_s *g = NULL;
	struct tm res;
	char t[256];
	time_t ts;
	FILE *stat_fp = NULL;

	if(stats_segments == -1 || !global_stats) {
		LM_ERR("%s: Can't dump statistics, not initialized!\n", __func__);
		return -1;
	}

	if(!stat_file) {
		LM_ERR("%s: Can't dump statistics, invalid stats file\n", __func__);
		return -1;
	}

	stat_fp = fopen(stat_file, "a");
	if(!stat_fp) {
		LM_ERR("%s: Couldn't open stats file %s: %s\n", __func__, stat_file,
				strerror(errno));
		return -1;
	}

	/* time stamp them since we're appending to the file */
	ts = time(NULL);
	localtime_r(&ts, &res);
	strftime(t, 255, "%c", &res);
	fprintf(stat_fp, "#### stats @ %s #####\n", t); 

	c = global_stats;
	for(i=0; i<stats_segments; i++) {
		if(dump_statistic(stat_fp, c, 1) == -1) {
			LM_ERR("Error dumping statistics for process %d\n", i);
			goto end;
		}
		c++;
	}

	fprintf(stat_fp, "## Global Stats ##\n");
	if(!g)
		g = calloc(1, sizeof(struct stats_s));
	if(!g) {
		LM_ERR("Couldn't dump global stats: %s\n", strerror(errno));
		goto end;
	}
	
	if(collect_stats(g) == -1) {
		LM_ERR("%s: Couldn't dump global stats\n", __func__);
		goto end;
	}
	if(dump_statistic(stat_fp, g, 0) == -1) {
		LM_ERR("Couldn't dump global stats\n");
		goto end;
	}
end:
	fprintf(stat_fp, "\n");
	fclose(stat_fp);

	return 0;
}

static int collect_stats(struct stats_s *s)
{
	register int i;
	register struct stats_s *c;
	if(!s) {
		LM_ERR("Invalid stats pointer passed\n");
		return -1;
	}
	if(!global_stats || stats_segments == -1) {
		LM_ERR("Can't collect statistics, not initialized!!\n");
		return -1;
	}

	c = global_stats;
	memset(s, '\0', sizeof(struct stats_s));
	for(i=0; i<stats_segments; i++) {
		s->received_requests_inv += c->received_requests_inv;
		s->received_requests_ack += c->received_requests_ack;
		s->received_requests_cnc += c->received_requests_cnc;
		s->received_requests_bye += c->received_requests_bye;
		s->received_requests_other += c->received_requests_other;
		s->received_responses_1 += c->received_responses_1;
		s->received_responses_2 += c->received_responses_2;
		s->received_responses_3 += c->received_responses_3;
		s->received_responses_4 += c->received_responses_4;
		s->received_responses_5 += c->received_responses_5;
		s->received_responses_6 += c->received_responses_6;
		s->received_responses_other += c->received_responses_other;
		s->received_drops += c->received_drops;
		s->sent_requests_inv += c->sent_requests_inv;
		s->sent_requests_ack += c->sent_requests_ack;
		s->sent_requests_cnc += c->sent_requests_cnc;
		s->sent_requests_bye += c->sent_requests_bye;
		s->sent_requests_other += c->sent_requests_other;
		s->sent_responses_1 += c->sent_responses_1;
		s->sent_responses_2 += c->sent_responses_2;
		s->sent_responses_3 += c->sent_responses_3;
		s->sent_responses_4 += c->sent_responses_4;
		s->sent_responses_5 += c->sent_responses_5;
		s->sent_responses_6 += c->sent_responses_6;
		s->processed_requests += c->processed_requests;
		s->processed_responses += c->processed_responses;
		s->acc_req_time += c->acc_req_time;
		s->acc_res_time += c->acc_res_time;
		s->failed_on_send += c->failed_on_send;

		c++; /* next, please... */
	}

	return 0;
}

/*************************** SNMP Stuff ***********************/

/* ##### Registration Functions ####### */

/* Registers the handlers for:
 * - sipSummaryStatsTable
 * - sipMethodStatsTable
 * - sipStatusCodesTable
 * - sipCommonStatusCodeTable
 *
 * Returns 0 if snmp module not present, -1 on error, 1 on successful
 * registration
 */

#define reg(t) \
	if(t##_register(&f) == -1) {	\
		LM_ERR("%s: Failed registering SNMP handlers\n", func);	\
		return -1;	\
	}

int stats_register()
{
	const char *func = __FUNCTION__;
	struct stats_funcs f;

	f.reg_func = (void*) find_export("snmp_register_handler", 2, 0);
	f.new_func = (void*) find_export("snmp_new_handler", 1, 0);
	f.free_func = (void*) find_export("snmp_free_handler", 1, 0);
	if(!f.reg_func || !f.new_func || !f.free_func) {
		LM_INFO("%s: Couldn't find SNMP module\n", func);
		LM_INFO("%s: Not reporting stats through SNMP\n", func);
		return 0;
	}

	reg(sipSummaryStatsTable);
	reg(sipMethodStatsTable);
	reg(sipStatusCodesTable);

	return 0;
}

/* Receives the function used to register SNMP handlers. Returns 0
 * on success, -1 on error */
/* Registers:
 * - sipSummaryInRequests
 * - sipSummaryOutRequests
 * - sipSummaryInResponses
 * - sipSummaryOutResponses
 * => sipSummaryTotalTransactions is handled by the tm module */
static int sipSummaryStatsTable_register(const struct stats_funcs *f)
{
	register struct sip_snmp_handler *h;
	register struct sip_snmp_obj *o;
	const char *func = __FUNCTION__;

	h = f->new_func(sizeof(unsigned long));
	if(!h) {
		LM_ERR("%s: Error creating handler\n", func);
		return -1;
	}
	o = h->sip_obj;

	/* this is the same for all of our objects */
	o->type = SER_COUNTER;
	*o->value.integer = 0;	/* default value. The real one is computed on
							   request */
	o->val_len = sizeof(unsigned long);

	/* sipSummaryInRequests */
	h->on_get = collect_InReqs;
	h->on_set = h->on_end = NULL;
	if(f->reg_func("sipSummaryInRequests", h) == -1) {
		LM_ERR("%s: Error registering sipSummaryInRequests\n", func);
		f->free_func(h);
		return -1;
	}

	/* sipSummaryOutRequests */
	h->on_get = collect_OutReqs;
	if(f->reg_func("sipSummaryOutRequests", h) == -1) {
		LM_ERR("%s: Error registering sipSummaryOutRequests\n", func);
		f->free_func(h);
		return -1;
	}

	/* sipSummaryInResponses */
	h->on_get = collect_InResp;
	if(f->reg_func("sipSummaryInResponses", h) == -1) {
		LM_ERR("%s: Error registering sipSummaryInResponses\n", func);
		f->free_func(h);
		return -1;
	}
	
	/* sipSummaryOutResponses */
	h->on_get = collect_OutResp;
	if(f->reg_func("sipSummaryOutResponses", h) == -1) {
		LM_ERR("%s: Error registering sipSummaryOutResponses\n", func);
		f->free_func(h);
		return -1;
	}

	f->free_func(h);

	return 0;
}

static int sipMethodStatsTable_register(const struct stats_funcs *f)
{
	const char* objs[] = {
		"sipStatsInviteIns",
		"sipStatsInviteOuts",
		"sipStatsAckIns",
		"sipStatsAckOuts",
		"sipStatsByeIns",
		"sipStatsByeOuts",
		"sipStatsCancelIns",
		"sipStatsCancelOuts"
#if 0	/* we don't know about these */
		"sipStatsOptionsIns",
		"sipStatsOptionsOuts",
		"sipStatsRegisterIns",
		"sipStatsRegisterOuts",
		"sipStatsInfoIns",
		"sipStatsInfoOuts"
#endif
	};
	int i, num = 8;
	const char *func = __FUNCTION__;
	register struct sip_snmp_handler *h;
	register struct sip_snmp_obj *o;

	h = f->new_func(sizeof(unsigned long));
	if(!h) {
		LM_ERR("%s: Error creating handler\n", func);
		return -1;
	}
	o = h->sip_obj;
	o->type = SER_COUNTER;
	*o->value.integer = 0;
	o->val_len = sizeof(unsigned long);

	h->on_get = sipStatsMethod_handler;
	h->on_set = h->on_end = NULL;

	for(i=0; i<num; i++) {
		if(f->reg_func(objs[i], h) == -1) {
			LM_ERR("%s: Error registering %s\n", func, objs[i]);
			f->free_func(h);
			return -1;
		}
	}

	f->free_func(h);

	return 0;
}

static int sipStatusCodesTable_register(const struct stats_funcs *f)
{
	const char *objs[] = {
		"sipStatsInfoClassIns",
		"sipStatsInfoClassOuts",
		"sipStatsSuccessClassIns",
		"sipStatsSuccessClassOuts",
		"sipStatsRedirClassIns",
		"sipStatsRedirClassOuts",
		"sipStatsReqFailClassIns",
		"sipStatsReqFailClassOuts",
		"sipStatsServerFailClassIns",
		"sipStatsServerFailClassOuts",
		"sipStatsGlobalFailClassIns",
		"sipStatsGlobalFailClassOuts",
		"sipStatsOtherClassesIns",
		"sipStatsOtherClassesOuts"
	};
	int i, num = 14;
	const char *func = __FUNCTION__;
	register struct sip_snmp_handler *h;
	register struct sip_snmp_obj *o;

	h = f->new_func(sizeof(unsigned long));
	if(!h) {
		LM_ERR("%s: Error creating handler\n", func);
		return -1;
	}
	o = h->sip_obj;
	o->type = SER_COUNTER;
	*o->value.integer = 0;
	o->val_len = sizeof(unsigned long);

	h->on_get = sipStatusCodes_handler;
	h->on_set = h->on_end = NULL;

	for(i=0; i<num; i++) {
		if(f->reg_func(objs[i], h) == -1) {
			LM_ERR("%s: Error registering %s\n", func, objs[i]);
			f->free_func(h);
			return -1;
		}
	}

	f->free_func(h);

	return 0;}

/* ########################## SNMP Handlers ######################### */

/*** Handlers for sipSummaryStatsTable */
static int collect_InReqs(struct sip_snmp_obj *o, enum handler_op op)
{
	register int i;
	register struct stats_s *c;
	register unsigned long t1, t2, t3, t4, t5;
	const char *func = __FUNCTION__;

	if(!global_stats || stats_segments == -1) {
		LM_ERR("%s: Can't collect stats, they have not been initialized."
			"Did you call init_stats()?\n", func);
		return -1;
	}

	if(op != SER_GET) {
		LM_ERR("%s: Invalid handler operation passed\n", func);
		return -1;
	}

	if(!o->value.integer) {
		o->value.integer = calloc(1, sizeof(unsigned long));
		if(!o->value.integer) {
			LM_ERR("%s: %s\n", func, strerror(errno));
			return -1;
		}
	}

	c = global_stats;
	t1 = t2 = t3 = t4 = t5 = 0;
	for(i=0; i<stats_segments; i++, c++) {
		t1 += c->received_requests_inv;
		t2 += c->received_requests_ack;
		t3 += c->received_requests_cnc;
		t4 += c->received_requests_bye;
		t5 += c->received_requests_other;
	}

	*o->value.integer = t1 + t2 + t3 + t4 + t5; 
	o->val_len = sizeof(unsigned long);
	o->type = SER_COUNTER;
	
	return 0;
}

static int collect_OutReqs(struct sip_snmp_obj *o, enum handler_op op)
{
	register int i;
	register struct stats_s *c;
	register unsigned long t1, t2, t3, t4, t5;
	const char *func = __FUNCTION__;

	if(!global_stats || stats_segments == -1) {
		LM_ERR("%s: Can't collect stats, they have not been initialized."
			"Did you call init_stats()?\n", func);
		return -1;
	}

	if(op != SER_GET) {
		LM_ERR("%s: Invalid handler operation passed\n", func);
		return -1;
	}

	if(!o->value.integer) {
		o->value.integer = calloc(1, sizeof(unsigned long));
		if(!o->value.integer) {
			LM_ERR("%s: %s\n", func, strerror(errno));
			return -1;
		}
	}

	c = global_stats;
	t1 = t2 = t3 = t4 = t5 = 0;
	for(i=0; i<stats_segments; i++, c++) {
		t1 += c->sent_requests_inv;
		t2 += c->sent_requests_ack;
		t3 += c->sent_requests_cnc;
		t4 += c->sent_requests_bye;
		t5 += c->sent_requests_other;
	}

	*o->value.integer = t1 + t2 + t3 + t4 + t5; 
	o->val_len = sizeof(unsigned long);
	o->type = SER_COUNTER;
	
	return 0;
}

static int collect_InResp(struct sip_snmp_obj *o, enum handler_op op)
{
	register int i;
	register struct stats_s *c;
	register unsigned long t1, t2, t3, t4, t5, t6, t7;
	const char *func = __FUNCTION__;

	if(!global_stats || stats_segments == -1) {
		LM_ERR("%s: Can't collect stats, they have not been initialized."
			"Did you call init_stats()?\n", func);
		return -1;
	}

	if(op != SER_GET) {
		LM_ERR("%s: Invalid handler operation passed\n", func);
		return -1;
	}

	if(!o->value.integer) {
		o->value.integer = calloc(1, sizeof(unsigned long));
		if(!o->value.integer) {
			LM_ERR("%s: %s\n", func, strerror(errno));
			return -1;
		}
	}

	c = global_stats;
	t1 = t2 = t3 = t4 = t5 = t6 = t7 = 0;
	for(i=0; i<stats_segments; i++, c++) {
		t1 += c->received_responses_1;
		t2 += c->received_responses_2;
		t3 += c->received_responses_3;
		t4 += c->received_responses_4;
		t5 += c->received_responses_5;
		t6 += c->received_responses_6;
		t7 += c->received_responses_other;
	}

	*o->value.integer = t1 + t2 + t3 + t4 + t5 + t6 + t7; 
	o->val_len = sizeof(unsigned long);
	o->type = SER_COUNTER;
	
	return 0;
}

static int collect_OutResp(struct sip_snmp_obj *o, enum handler_op op)
{
	register int i;
	register struct stats_s *c;
	register unsigned long t1, t2, t3, t4, t5, t6, t7;
	const char *func = __FUNCTION__;

	if(!global_stats || stats_segments == -1) {
		LM_ERR("%s: Can't collect stats, they have not been initialized\n",
			func);
		return -1;
	}

	if(op != SER_GET) {
		LM_ERR("%s: Invalid handler operation passed\n", func);
		return -1;
	}

	if(!o->value.integer) {
		o->value.integer = calloc(1, sizeof(unsigned long));
		if(!o->value.integer) {
			LM_ERR("%s: %s\n", func, strerror(errno));
			return -1;
		}
	}

	c = global_stats;
	t1 = t2 = t3 = t4 = t5 = t6 = t7 = 0;
	for(i=0; i<stats_segments; i++, c++) {
		t1 += c->sent_responses_1;
		t2 += c->sent_responses_2;
		t3 += c->sent_responses_3;
		t4 += c->sent_responses_4;
		t5 += c->sent_responses_5;
		t6 += c->sent_responses_6;
		/* XXX: Not in stats struct 
		t7 += c->sent_responses_other;
		*/
	}

	*o->value.integer = t1 + t2 + t3 + t4 + t5 + t6 + t7; 
	o->val_len = sizeof(unsigned long);
	o->type = SER_COUNTER;
	
	return 0;
}

/***** Handlers for sipMethodStatsTable ******/
/* Collects the specified stat and puts the result in total. s defines
 * the starting point in the stats array, normally global_stats */
#define collect_this_stat(stat, total, s) \
	for(i=0; i<stats_segments; i++)	\
		total += s++->stat;

static int sipStatsMethod_handler(struct sip_snmp_obj *o, enum handler_op op)
{
	register struct stats_s *c;
	register unsigned long total;
	register int i;
	const char *func = __FUNCTION__;

	if(!o) {
		LM_ERR("%s: Invalid sip SNMP object passed\n", func);
		return -1;
	}

	if(!global_stats || stats_segments == -1) {
		LM_ERR("%s: Can't collect stats, they have not been initialized\n",
			func);
		return -1;
	}

	if(op != SER_GET) {
		LM_ERR("%s: Invalid handler operation passed\n", func);
		return -1;
	}

	if(!o->value.integer) {
		o->value.integer = calloc(1, sizeof(unsigned long));
		if(!o->value.integer) {
			LM_ERR("%s: %s\n", func, strerror(errno));
			return -1;
		}
	}

	c = global_stats;
	total = 0;
	switch(o->col) {
		/* these definitions are taken from sipMethodStatsHandler */
		case COLUMN_SIPSTATSINVITEINS: 
			collect_this_stat(received_requests_inv, total, c);
			break;
		case COLUMN_SIPSTATSINVITEOUTS: 
			collect_this_stat(sent_requests_inv, total, c);
			break;
		case COLUMN_SIPSTATSACKINS: 
			collect_this_stat(received_requests_ack, total, c);
			break;
		case COLUMN_SIPSTATSACKOUTS: 
			collect_this_stat(sent_requests_ack, total, c);
			break;
		case COLUMN_SIPSTATSBYEINS: 
			collect_this_stat(received_requests_bye, total, c);
			break;
		case COLUMN_SIPSTATSBYEOUTS: 
			collect_this_stat(sent_requests_bye, total, c);
			break;
		case COLUMN_SIPSTATSCANCELINS: 
			collect_this_stat(received_requests_cnc, total, c);
			break;
		case COLUMN_SIPSTATSCANCELOUTS: 
			collect_this_stat(sent_requests_cnc, total, c);
			break;
		/* ser doesn't have notion for these. We don't
		 * register them with snmp. Here just as remainder */
#if 0
		case COLUMN_SIPSTATSOPTIONSINS:
		case COLUMN_SIPSTATSOPTIONSOUTS:
		case COLUMN_SIPSTATSREGISTERINS:
		case COLUMN_SIPSTATSREGISTEROUTS:
		case COLUMN_SIPSTATSINFOINS:
		case COLUMN_SIPSTATSINFOOUTS:
			break;
#endif
	}

	*o->value.integer = total;
	o->val_len = sizeof(unsigned long);
	o->type = SER_COUNTER;

	return 0;
}

static int sipStatusCodes_handler(struct sip_snmp_obj *o, enum handler_op op)
{
	register struct stats_s *c;
	register unsigned long total;
	register int i;
	const char *func = __FUNCTION__;

	if(!o) {
		LM_ERR("%s: Invalid sip SNMP object passed\n", func);
		return -1;
	}

	if(!global_stats || stats_segments == -1) {
		LM_ERR("%s: Can't collect stats, they have not been initialized\n",
			func);
		return -1;
	}

	if(op != SER_GET) {
		LM_ERR("%s: Invalid handler operation passed\n", func);
		return -1;
	}

	if(!o->value.integer) {
		o->value.integer = calloc(1, sizeof(unsigned long));
		if(!o->value.integer) {
			LM_ERR("%s: %s\n", func, strerror(errno));
			return -1;
		}
	}

	c = global_stats;
	total = 0;
	switch(o->col) {
		case COLUMN_SIPSTATSINFOCLASSINS:
			collect_this_stat(received_responses_1, total, c);
			break;
		case COLUMN_SIPSTATSINFOCLASSOUTS:
			collect_this_stat(sent_responses_1, total, c);
			break;
		case COLUMN_SIPSTATSSUCCESSCLASSINS:
			collect_this_stat(received_responses_2, total, c);
			break;
		case COLUMN_SIPSTATSSUCCESSCLASSOUTS:
			collect_this_stat(sent_responses_2, total, c);
			break;
		case COLUMN_SIPSTATSREDIRCLASSINS:
			collect_this_stat(received_responses_3, total, c);
			break;
		case COLUMN_SIPSTATSREDIRCLASSOUTS:
			collect_this_stat(sent_responses_3, total, c);
			break;
		case COLUMN_SIPSTATSREQFAILCLASSINS:
			collect_this_stat(received_responses_4, total, c);
			break;
		case COLUMN_SIPSTATSREQFAILCLASSOUTS:
			collect_this_stat(sent_responses_4, total, c);
			break;
		case COLUMN_SIPSTATSSERVERFAILCLASSINS:
			collect_this_stat(received_responses_5, total, c);
			break;
		case COLUMN_SIPSTATSSERVERFAILCLASSOUTS:
			collect_this_stat(sent_responses_5, total, c);
			break;
		case COLUMN_SIPSTATSGLOBALFAILCLASSINS:
			collect_this_stat(received_responses_6, total, c);
			break;
		case COLUMN_SIPSTATSGLOBALFAILCLASSOUTS:
			collect_this_stat(sent_responses_6, total, c);
			break;
		case COLUMN_SIPSTATSOTHERCLASSESINS:
			collect_this_stat(received_responses_other, total, c);
			break;
		case COLUMN_SIPSTATSOTHERCLASSESOUTS:
			/* FIXME: For some reason this is not defined in
			 * struct stats_s... */
			/* collect_this_stat(sent_responses_other, total, c); */
			total = 0;
			break;
	}

	*o->value.integer = total;
	o->val_len = sizeof(unsigned long);
	o->type = SER_COUNTER;

	return 0;
}

#endif
