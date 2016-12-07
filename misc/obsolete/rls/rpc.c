#include "rpc.h"
#include "rl_subscription.h"
#include "rls_data.h"
#include "../../dprint.h"

#include <unistd.h>

/*
#define rpc_lf(rpc, c)	rpc->add(c, "s","")
	rpc->printf(c, "    %.*s contact=\'%.*s\' exp=%u status=%d published=%d (id=%.*s)", 
				FMT_STR(t->id), FMT_STR(t->contact), t->expires - time(NULL),
				(int)t->state, t->is_published, FMT_STR(t->published_id));
	rpc_lf(rpc, c);
*/

/* #define rpc_lf(rpc, c)	rpc->add(c, "s","") */
#define rpc_lf(rpc, c)	do { } while (0)

static void trace_vs(rpc_t *rpc, void *c, virtual_subscription_t *vs, int details)
{
	rpc->printf(c, " Virtual subscriptions:");
	rpc_lf(rpc, c);
	
	rpc->printf(c, " -> URI = %.*s", FMT_STR(vs->uri));
	rpc_lf(rpc, c);
	rpc->printf(c, " -> status = %d", vs->status);
	rpc_lf(rpc, c);

	if (details > 0) {
		rpc->printf(c, " -> document = %.*s", FMT_STR(vs->state_document));
		rpc_lf(rpc, c);
	}

	rpc_lf(rpc, c);
}

static void rls_trace_subscription(rpc_t *rpc, void *c, rl_subscription_t *s, int details)
{
	virtual_subscription_t *vs;
	int cnt, i;
	
	switch (s->type) {
		case rls_internal_subscription:
			rpc->printf(c, "URI = %.*s", FMT_STR(*s->u.internal.record_id));
			rpc_lf(rpc, c);
			break;
		case rls_external_subscription:
			rpc->printf(c, "URI = %.*s", FMT_STR(s->u.external.record_id));
			rpc_lf(rpc, c);
			break;
	}

	cnt = ptr_vector_size(&s->vs);
	for (i = 0; i < cnt; i++) {
		vs = ptr_vector_get(&s->vs, i);
		if (!vs) continue;
		if (details > 0) trace_vs(rpc, c, vs, details - 1);
	}
	
	rpc_lf(rpc, c);
}

static void rls_trace(rpc_t *rpc, void *c)
{
	int i = 0;
	subscription_data_t *s;
	rl_subscription_t *rs;
	int details = 0;
	
	if (rpc->scan(c, "d", &details) <= 0)
		details = 0;
	rpc->fault(c, 200, "OK");
	
	rpc->add(c, "s", "RLS Trace:");

	if (!rls) {
		rpc->printf(c, "problems");
		rpc->send(c);
		return;
	}
	
	s = rls_manager->first;
	while (s) {
		i++;
		rs = (rl_subscription_t*)(s->usr_data);
		if (details > 0) rls_trace_subscription(rpc, c, rs, details);
		s = s->next;
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
