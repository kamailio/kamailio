#include "events_mod.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include <libxml/parser.h>
#include <cds/memory.h>
#include <cds/ptr_vector.h>
#include <cds/logger.h>
#include <cds/cds.h>
#include <presence/qsa.h>

#include "qsa_events.h"
#include "rpc.h"

#include <time.h>
#include "events_uac.h"
#include "euac_funcs.h"

MODULE_VERSION

int events_mod_init(void);
void events_mod_destroy(void);
int events_child_init(int _rank);

static int handle_notify(struct sip_msg* m)
{
	int res;
	
	PROF_START(b2b_handle_notify)
		
	if (process_euac_notify(m) == 0) res = 1;
	else res = -1;
	
	PROF_STOP(b2b_handle_notify)

	return res;
}


/** Exported functions */
static cmd_export_t cmds[]={
	{"handle_notify", (cmd_function)handle_notify, 0, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};

static int handle_presence_subscriptions = 1;

/** Exported parameters */
static param_export_t params[]={
	{"default_expiration", PARAM_INT, &subscribe_time },
	{"on_error_retry_time", PARAM_INT, &resubscribe_timeout_on_err },
	{"presence_route", PARAM_STR, &presence_route },
	{"additional_presence_headers", PARAM_STR, &presence_headers },
	{"wait_for_term_notify", PARAM_INT, &waiting_for_notify_time },
	{"resubscribe_delta", PARAM_INT, &resubscribe_delta },
	{"min_resubscribe_time", PARAM_INT, &min_resubscribe_time },
	{"handle_presence_subscriptions", PARAM_INT, &handle_presence_subscriptions },

	{"presence_outbound_proxy", PARAM_STR, &presence_outbound_proxy },	
	{"max_subscribe_delay", PARAM_INT, &max_subscribe_delay }, /* for randomized sent of SUBSCRIBE requests */
	{0, 0, 0}
};

struct module_exports exports = {
	"presence_b2b",
	cmds,        /* Exported functions */
	events_rpc_methods,           /* RPC methods */
	params,      /* Exported parameters */
	events_mod_init, /* module initialization function */
	0,           /* response function*/
	events_mod_destroy,	/* pa_destroy,  / * destroy function */
	0,           /* oncancel function */
	events_child_init	/* per-child init function */
};

int events_mod_init(void)
{
	DEBUG_LOG("presence_b2b module initialization\n");

	/* ??? if other module uses this libraries it might be a problem ??? */
	xmlInitParser();

	DEBUG_LOG(" ... common libraries\n");
	qsa_initialize();

	if (events_uac_init() != 0) {
		return -1;
	}

	if (!handle_presence_subscriptions) {
		WARN("NOT handling presence subscriptions\n");
	}

	if (events_qsa_interface_init(handle_presence_subscriptions) != 0) return -1;

	return 0;
}

int events_child_init(int _rank)
{
	return 0;
}

void events_mod_destroy(void)
{
	/*int i, cnt;
	char *s;*/

	DEBUG_LOG("presence_b2b module cleanup\n");

	DEBUG_LOG(" ... events UAC\n");
	events_uac_destroy();

	DEBUG_LOG(" ... qsa interface\n");
	events_qsa_interface_destroy();

	DEBUG_LOG(" ... common libs\n");
	qsa_cleanup();

	/* ??? if other module uses this libraries it might be a problem ??? */
/*	xmlCleanupParser(); */
	DEBUG_LOG("presence_b2b module cleanup finished\n");
}

