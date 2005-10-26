#include "rls_mod.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include <libxml/parser.h>
#include <curl/curl.h>
#include <cds/memory.h>
#include <cds/ptr_vector.h>
#include <cds/logger.h>

#include "rl_subscription.h"
#include "rls_handler.h"

#include "time_event_manager.h"
#include <time.h>

MODULE_VERSION

int rls_mod_init(void);
void rls_mod_destroy(void);
int rls_child_init(int _rank);
static int rls_subscribe_fixup(void** param, int param_no);

/** Exported functions */
static cmd_export_t cmds[]={
	/* {"handle_r_subscription", handle_r_subscription, 0, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE}, */
	{"handle_rls_subscription", (cmd_function)handle_rls_subscription, 2, 
		rls_subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};

/** Exported parameters */
static param_export_t params[]={
	{"min_expiration", INT_PARAM, &rls_min_expiration }, 
	{"max_expiration", INT_PARAM, &rls_max_expiration }, 
	{"default_expiration", INT_PARAM, &rls_default_expiration }, 
	{0, 0, 0}
};

struct module_exports exports = {
	"rls",
	cmds,        /* Exported functions */
	params,      /* Exported parameters */
	rls_mod_init, /* module initialization function */
	0,           /* response function*/
	rls_mod_destroy,	/* pa_destroy,  / * destroy function */
	0,           /* oncancel function */
	rls_child_init	/* per-child init function */
};

struct tm_binds tmb;
	
int rls_min_expiration = 60;
int rls_max_expiration = 7200;
int rls_default_expiration = 3761;

/* FIXME: settings of other xcap parameters (auth, ssl, ...) */

/* internal data members */
static ptr_vector_t *xcap_servers = NULL;

int rls_mod_init(void)
{
    load_tm_f load_tm;

	if (time_event_management_init() != 0) {
		LOG(L_ERR, "rls_mod_init(): Can't initialize time event management!\n");
		return -1;
	}
	
	if (subscription_management_init() != 0) {
		LOG(L_ERR, "rls_mod_init(): Can't initialize time event management!\n");
		return -1;
	}
	
	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "rls_mod_init(): Can't import tm!\n");
		return -1;
	}

	/* let the auto-loading function load all TM stuff */
	if (load_tm(&tmb)==-1) {
		LOG(L_ERR, "rls_mod_init(): load_tm() failed\n");
		return -1;
	}
	
	if (rls_init() != 0) {
		return -1;
	}

	if (vs_init() != 0) {
		return -1;
	}

	xcap_servers = (ptr_vector_t*)shm_malloc(sizeof(ptr_vector_t));
	if (!xcap_servers) {
		LOG(L_ERR, "rls_mod_init(): can't allocate memory for XCAP servers vector\n");
		return -1;
	}

	/* ??? if other module uses this libraries it might be a problem ??? */
	xmlInitParser();
	curl_global_init(CURL_GLOBAL_ALL);
	
	return 0;
}

int rls_child_init(int _rank)
{
	return 0;
}

void rls_mod_destroy(void)
{
	int i, cnt;
	char *s;

	/* destroy used XCAP servers */
	if (xcap_servers) {
		cnt = ptr_vector_size(xcap_servers);
		for (i = 0; i < cnt; i++) {
			s = ptr_vector_get(xcap_servers, i);
			if (s) shm_free(s);
		}
		ptr_vector_destroy(xcap_servers);
		shm_free(xcap_servers);
		xcap_servers = NULL;
	}
	
	rls_destroy();
	
	/* ??? if other module uses this libraries it might be a problem ??? */
/*	xmlCleanupParser();
	curl_global_cleanup(); */
}

static int rls_subscribe_fixup(void** param, int param_no)
{
	char *xcap_server = NULL;
	int send_errors = 0;
	
	if (param_no == 1) {
		if (!param) {
			LOG(L_ERR, "rls_subscribe_fixup(): XCAP server address not set!\n");
			return E_UNSPEC;
		}
		xcap_server = zt_strdup((char *)*param);
		if (!xcap_server) {
			LOG(L_ERR, "rls_subscribe_fixup(): Can't set XCAP server address!\n");
			return E_UNSPEC;
		}
		
		/* store not only the root string? (create a structure rather?) */
		ptr_vector_add(xcap_servers, xcap_server);
		
		TRACE_LOG("rls_subscribe_fixup(): XCAP server is %s\n", xcap_server);
		*param = (void*)xcap_server;
	}
	if (param_no == 2) {
		if (param) {
			if (*param) send_errors = atoi(*param);
		}
		TRACE_LOG("rls_subscribe_fixup(): send errors: %d\n", send_errors);
		*param = (void*)send_errors;
	}
	return 0;
}

