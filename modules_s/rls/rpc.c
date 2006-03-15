#include "rpc.h"
#include "rl_subscription.h"
#include "../../dprint.h"

#include <unistd.h>

/*
#define rpc_lf(rpc, c)	rpc->add(c, "s","")
	rpc->printf(c, "    %.*s contact=\'%.*s\' exp=%u status=%d published=%d (id=%.*s)", 
				FMT_STR(t->id), FMT_STR(t->contact), t->expires - time(NULL),
				(int)t->state, t->is_published, FMT_STR(t->published_id));
	rpc_lf(rpc, c);
*/

#define rpc_lf(rpc, c)	rpc->add(c, "s","")

static void rls_trace(rpc_t *rpc, void *c)
{
	int i = 0;
	subscription_data_t *s;
	
	rpc->add(c, "s", "RLS Trace:");

	if (rls_manager) {
		s = rls_manager->first;
		while (s) {
			s = s->next;
			i++;
		}
	}
	rpc->printf(c, "subscription count: %d", i);
	rpc_lf(rpc, c);
	
	rpc->send(c);
}

/* ----- exported data structure with methods ----- */

static const char* rls_trace_doc[] = {
	"RLS trace.",  /* Documentation string */
	0                                    /* Method signature(s) */
};

/* 
 * RPC Methods exported by this module 
 */

rpc_export_t rls_rpc_methods[] = {
	{"rls.trace",   rls_trace,     rls_trace_doc, 0},
	{0, 0, 0, 0}
};
