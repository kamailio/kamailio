#include "../../fifo_server.h"
#include "../../error.h"
#include "rls_mod.h"
#include "rl_subscription.h"
#include "qsa_rls.h"
#include "fifo.h"
#include <cds/sstr.h>
#include <time.h>

static void trace_rls(FILE *fifo, char *response_file, rl_subscription_t *s)
{
	int i, cnt;
	virtual_subscription_t *vs;

	switch (s->type) {
		case rls_external_subscription:
			fifo_reply(response_file, "to %.*s from %.*s\n", 
				FMT_STR(s->external.record_id),
				FMT_STR(s->external.subscriber));
			break;
		case rls_internal_subscription:
			if (s->internal.s)
				fifo_reply(response_file, "to %.*s from %.*s (internal)\n", 
					FMT_STR(s->internal.s->record_id),
					FMT_STR(s->internal.s->subscriber_id));
			else fifo_reply(response_file, "to ??? from ??? (internal)\n");
			break;
	}
	
	/* add all list elements */
	cnt = ptr_vector_size(&s->vs);
	for (i = 0; i < cnt; i++) {
		vs = ptr_vector_get(&s->vs, i);
		if (!vs) continue;

		fifo_reply(response_file, "  + %.*s\n", FMT_STR(vs->uri));
	}
}

static int fifo_rls_trace(FILE *fifo, char *response_file)
{
	subscription_data_t *_s;
	rl_subscription_t *s;
	internal_rl_subscription_t *is;

	fifo_reply(response_file, "Resource List Subscriptions\n");
	
	rls_lock();

	/* external subnscriptions */
	_s = rls_manager->first;
	while (_s) {
		s = (rl_subscription_t*)_s->usr_data;
		if (s)
			trace_rls(fifo, response_file, s);
		_s = _s->next;
	}

	/* internal subscriptions */
	if (rls_internal_data) is = rls_internal_data->first;
	else is = NULL;
	while (is) {
		if (is->rls) trace_rls(fifo, response_file, is->rls);
		is = is->next;
	}

	rls_unlock();
	return 0;
}

int rls_fifo_register()
{
	if (register_fifo_cmd(fifo_rls_trace, "rls_trace", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo rls_trace\n");
		return -1;
	}
	return 0;
}
