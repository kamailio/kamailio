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
	if (process_euac_notify(m) == 0) return 1;
	else return -1;
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

struct tm_binds tmb;
dlg_func_t dlg_func;

int events_mod_init(void)
{
    load_tm_f load_tm;
	bind_dlg_mod_f bind_dlg;

	DEBUG_LOG("presence_b2b module initialization\n");

	/* ??? if other module uses this libraries it might be a problem ??? */
	xmlInitParser();

	DEBUG_LOG(" ... common libraries\n");
	cds_initialize();
	qsa_initialize();

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		ERR("Can't import tm!\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm(&tmb)==-1) {
		ERR("load_tm() failed\n");
		return -1;
	}

	bind_dlg = (bind_dlg_mod_f)find_export("bind_dlg_mod", -1, 0);
	if (!bind_dlg) {
		LOG(L_ERR, "Can't import dlg\n");
		return -1;
	}
	if (bind_dlg(&dlg_func) != 0) {
		return -1;
	}

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
	cds_cleanup();

	/* ??? if other module uses this libraries it might be a problem ??? */
/*	xmlCleanupParser(); */
	DEBUG_LOG("presence_b2b module cleanup finished\n");
}

