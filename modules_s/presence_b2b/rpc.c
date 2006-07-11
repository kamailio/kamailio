#include "rpc.h"
#include "../../dprint.h"

#include "events_uac.h"
#include "euac_internals.h"
#include <unistd.h>

/*
#define rpc_lf(rpc, c)	rpc->add(c, "s","")
	rpc->printf(c, "    %.*s contact=\'%.*s\' exp=%u status=%d published=%d (id=%.*s)", 
				FMT_STR(t->id), FMT_STR(t->contact), t->expires - time(NULL),
				(int)t->state, t->is_published, FMT_STR(t->published_id));
	rpc_lf(rpc, c);
*/

#if 0

/* method for UAC testing */
static void test1(rpc_t *rpc, void *c)
{
	/*events_uac_t *uac = NULL;
	str presence = STR_STATIC_INIT("presence");
	str to = STR_STATIC_INIT("<sip:ms2@test-domain.com>");
	str from = STR_STATIC_INIT("<sip:b2b@test-domain.com>");
	str route = STR_STATIC_INIT("<sip:vencore.sip-server.net:5060;lr>");
	
	rpc->add(c, "s", "test called");
	rpc->send(c);

	 uac = create_events_uac(&to, &from, &presence, NULL, NULL, NULL, &route);

	sleep(5);
	destroy_events_uac(uac); */
}

static void test2(rpc_t *rpc, void *c)
{
	/* void *st; */
	int i;
	
	/*rpc->add(c, "s", "test called");
	rpc->send(c);*/
/*
	if (rpc->add(c, "{", &st) < 0) return;
	rpc->struct_add(st, "d", "watcher_t", sizeof(watcher_t));
	rpc->struct_add(st, "d", "dlg_t", sizeof(dlg_t));
	rpc->struct_add(st, "d", "presentity_t", sizeof(presentity_t));
	rpc->send(c);
*/

	for (i = 0; i < 1000; i++) {
		rpc->printf(c, "element %d with very long text\n", i);
		rpc->add(c, "s","");
	}
	rpc->send(c);
}

#endif

static void test(rpc_t *rpc, void *c)
{
	/* void *st; */
	int i, sum;
	char *x;
/*	int sizes[] = { 785, -1 }; */
	int sizes[] = { 4, 24, 9, 4, 12, 12, 16, 12, 11, 4, -1 };
	
	sum = 0;
	for (i = 0; sizes[i] >= 0; i++) {
		sum += sizes[i];
		x = (char*) shm_malloc(sizes[i]);
		if (!x) rpc->fault(c, 500, "allocation error");
	}
	
	rpc->add(c, "sd", "allocated bytes", sum);
	rpc->add(c, "sd", "allocated blocks", i);
	rpc->send(c);
}

/* Trace method */

#define rpc_lf(rpc, c)	do { } while (0)
/* #define rpc_lf(rpc, c)	rpc->add(c, "s","") */
#define rpc_printf(rpc, c, buf, args...) sprintf(buf, ##args); rpc->add(c, "s", buf);

static void trace(rpc_t *rpc, void *c)
{
	int i = 0;
	events_uac_t *uac;
	char tmp[2048];	
	int detailed = 0;

	if (rpc->scan(c, "d", &detailed) <= 0)
		detailed = 0;
	
	rpc->fault(c, 200, "OK");
	
	rpc->printf(c, "%s", "Presence B2BUA Trace:");
	rpc_lf(rpc, c);

	if (euac_internals) {
		if (detailed) {
			uac = euac_internals->first_uac;
			while (uac) {
				rpc_printf(rpc, c, tmp, "[%s]: %d, refcnt: %d, timer started: %d", 
						uac->id, uac->status, 
						uac->ref_cntr.cntr, uac->timer_started); 
				rpc_lf(rpc, c);
				uac = uac->next;
				i++;
			}
		}
		rpc_printf(rpc, c, tmp, "EUAC count: %d", i); rpc_lf(rpc, c);
		rpc_printf(rpc, c, tmp,  "create_cnt: %d", euac_internals->create_cnt); 
		rpc_lf(rpc, c);
		rpc_printf(rpc, c, tmp, "destroy_cnt: %d", euac_internals->destroy_cnt); 
		rpc_lf(rpc, c);
	}
	else {
		rpc->printf(c, "euac_internals not set!");
	}

	rpc->send(c);
}
/* ----- exported data structure with methods ----- */

static const char* test_doc[] = {
	"Testing events.",  /* Documentation string */
	0                   /* Method signature(s) */
};

static const char* trace_doc[] = {
	"Trace events.",  /* Documentation string */
	0                   /* Method signature(s) */
};

/* 
 * RPC Methods exported by this module 
 */

rpc_export_t events_rpc_methods[] = {
	{"presence_b2b.test", test, test_doc, 0},
	{"presence_b2b.trace", trace, trace_doc, 0},
	{0, 0, 0, 0}
};

