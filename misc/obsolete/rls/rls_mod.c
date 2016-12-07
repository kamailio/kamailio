#include "rls_mod.h"
#include "../../sr_module.h"
#include "../../timer_ticks.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include <libxml/parser.h>
#include <cds/memory.h>
#include <cds/ptr_vector.h>
#include <cds/logger.h>
#include <cds/cds.h>
#include <presence/qsa.h>

#include "rl_subscription.h"
#include "rls_handler.h"
#include "rpc.h"
#include "uri_ops.h"

#include "time_event_manager.h"
#include <time.h>

MODULE_VERSION

int rls_mod_init(void);
void rls_mod_destroy(void);
int rls_child_init(int _rank);
static int rls_subscribe_fixup(void** param, int param_no);

/* authorization parameters */
char *auth_type_str = NULL; /* type of authorization: none,implicit,xcap */

int db_mode = 0; /* 0 -> no DB, 1 -> write through */
char *db_url = NULL;

int reduce_xcap_needs = 0;

/* internal data members */

/* static ptr_vector_t *xcap_servers = NULL; */
db_con_t* rls_db = NULL; /* database connection handle */
db_func_t rls_dbf;	/* database functions */

/* one shot timer for reloading data from DB -
they can not be reloaded in init or
child_init due to internal subscriptions
to other modules (may be not itialised
yet) */
static int init_timer_delay = 3; 

/* parameters for optimizations */
int max_notifications_at_once = 1000000;

/* timer for processing notifications from QSA */
int rls_timer_interval = 10;

/* ignore if NOTIFY times out (don't destroy subscription */
int rls_ignore_408_on_notify = 0;

/* maximum nested list level (if 0, no nested lists are possible,
 * if 1 only one nested list level is possible, if 2 it is possible to
 * have lists in lists, ..., unlimited if -1) */
int max_list_nesting_level = -1;

/** Exported functions */
static cmd_export_t cmds[]={
	/* {"handle_r_subscription", handle_r_subscription, 0, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE}, */
	{"handle_rls_subscription", (cmd_function)handle_rls_subscription, 1,
		rls_subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"is_simple_rls_target", is_simple_rls_target, 1, NULL, REQUEST_ROUTE | FAILURE_ROUTE},
	{"query_rls_services", query_rls_services, 0, NULL, REQUEST_ROUTE | FAILURE_ROUTE},
	{"query_resource_list", query_resource_list, 1, NULL, REQUEST_ROUTE | FAILURE_ROUTE},
	{"have_flat_list", have_flat_list, 0, NULL, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};

/** Exported parameters */
static param_export_t params[]={
	{"min_expiration", PARAM_INT, &rls_min_expiration },
	{"max_expiration", PARAM_INT, &rls_max_expiration },
	{"default_expiration", PARAM_INT, &rls_default_expiration },
	{"auth", PARAM_STRING, &auth_type_str }, /* type of authorization: none, implicit, xcap, ... */
	{"db_mode", PARAM_INT, &db_mode },
	{"db_url", PARAM_STRING, &db_url },
	{"reduce_xcap_needs", PARAM_INT, &reduce_xcap_needs },
	{"max_notifications_at_once", PARAM_INT, &max_notifications_at_once },
	{"timer_interval", PARAM_INT, &rls_timer_interval },
	{"max_list_nesting_level", PARAM_INT, &max_list_nesting_level },
	{"expiration_timer_period", PARAM_INT, &rls_expiration_timer_period },
	{"ignore_408_on_notify", PARAM_INT, &rls_ignore_408_on_notify },
	{"init_timer_delay", PARAM_INT, &init_timer_delay }, /* timer for delayed DB reload (due to internal subscriptions can't be reloaded from init or child init) */
	
	{0, 0, 0}
};

struct module_exports exports = {
	"rls",
	cmds,        /* Exported functions */
	rls_rpc_methods,           /* RPC methods */
	params,      /* Exported parameters */
	rls_mod_init, /* module initialization function */
	0,           /* response function*/
	rls_mod_destroy,	/* pa_destroy,  / * destroy function */
	0,           /* oncancel function */
	rls_child_init	/* per-child init function */
};

struct tm_binds tmb;
dlg_func_t dlg_func;
fill_xcap_params_func fill_xcap_params = NULL;

int use_db = 0;

int rls_min_expiration = 60;
int rls_max_expiration = 7200;
int rls_default_expiration = 3761;
int rls_expiration_timer_period = 10;

rls_auth_params_t rls_auth_params;	/* structure filled according to parameters (common for all XCAP servers now) */
char *xcap_server = NULL; /* XCAP server URI */

/* TODO: settings of other xcap parameters (auth, ssl, ...) */

static int set_auth_params(rls_auth_params_t *dst, const char *auth_type_str)
{
	if (!auth_type_str) {
		LOG(L_ERR, "no subscription authorization type given, using \'implicit\'!\n");
		dst->type = rls_auth_none;
		return 0;
	}
	if (strcmp(auth_type_str, "xcap") == 0) {
		dst->type = rls_auth_xcap;
		return 0;
	}
	if (strcmp(auth_type_str, "none") == 0) {
		dst->type = rls_auth_none;
		LOG(L_WARN, "using \'none\' rls-subscription authorization!\n");
		return 0;
	}
	if (strcmp(auth_type_str, "implicit") == 0) {
		dst->type = rls_auth_implicit;
		return 0;
	}

	LOG(L_ERR, "Can't resolve subscription authorization type: \'%s\'."
			" Use one of: none, implicit, xcap.\n", auth_type_str);
	return -1;
}

static ticks_t init_timer_cb(ticks_t ticks, struct timer_ln* tl, void* data)
{
	/* initialization (like read data from database) which can trigger
	 * database operations in other modules/... */

	if (use_db && (rls_db)) {
		INFO("reading RLS data from database\n");
		rls_lock();
		db_load_rls();
		rls_unlock();
	}

	if (data) {
		mem_free(data);
		/* ERR("freeing myself!\n"); */
	}
	
	return 0; /* one shot timer */
}

int rls_mod_init(void)
{
    load_tm_f load_tm;
	bind_dlg_mod_f bind_dlg;
	struct timer_ln *i_timer = NULL;

	DEBUG_LOG("RLS module initialization\n");

	/* ??? if other module uses this libraries it might be a problem ??? */
	xmlInitParser();

	DEBUG_LOG(" ... common libraries\n");
	qsa_initialize();

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

	bind_dlg = (bind_dlg_mod_f)find_export("bind_dlg_mod", -1, 0);
	if (!bind_dlg) {
		LOG(L_ERR, "Can't import dlg\n");
		return -1;
	}
	if (bind_dlg(&dlg_func) != 0) {
		return -1;
	}

	if (rls_init() != 0) {
		return -1;
	}

	if (vs_init() != 0) {
		return -1;
	}

/*	xcap_servers = (ptr_vector_t*)mem_alloc(sizeof(ptr_vector_t));
	if (!xcap_servers) {
		LOG(L_ERR, "rls_mod_init(): can't allocate memory for XCAP servers vector\n");
		return -1;
	}
	ptr_vector_init(xcap_servers, 8); */

	/* set authorization type according to requested "auth type name"
	 * and other (type specific) parameters */
	if (set_auth_params(&rls_auth_params, auth_type_str) != 0) return -1;

	use_db = 0;
	if (db_mode > 0) {
		int db_url_len = db_url ? strlen(db_url) : 0;
		if (!db_url_len) {
			LOG(L_ERR, "rls_mod_init(): no db_url specified but db_mode > 0\n");
			db_mode = 0;
		}
	}
	if (db_mode > 0) {
		if (bind_dbmod(db_url, &rls_dbf) < 0) {
			LOG(L_ERR, "rls_mod_init(): Can't bind database module via url %s\n", db_url);
			return -1;
		}

		if (!DB_CAPABILITY(rls_dbf, DB_CAP_ALL)) { /* ? */
			LOG(L_ERR, "rls_mod_init(): Database module does not implement all functions needed by the module\n");
			return -1;
		}
		use_db = 1;
	}

	/* once-shot timer for reloading data from DB -
	 * needed because it can trigger database operations
	 * in other modules and they mostly intialize their 
	 * database connection in child_init functions */
	
	i_timer = timer_alloc();
	if (!i_timer) {
		ERR("can't allocate memory for DB init timer\n");
		return -1;
	}
	else {
		timer_init(i_timer, init_timer_cb, i_timer, 0);
		timer_add(i_timer, S_TO_TICKS(init_timer_delay));
	}
	
	fill_xcap_params = (fill_xcap_params_func)find_export("fill_xcap_params", 0, -1);

	
	return 0;
}

int rls_child_init(int _rank)
{
	rls_db = NULL;
	if (use_db) {
		if (_rank==PROC_INIT || _rank==PROC_MAIN || _rank==PROC_TCP_MAIN)
			return 0; /* do nothing for the main or tcp_main processes */
		if (rls_dbf.init) rls_db = rls_dbf.init(db_url);
		if (!rls_db) {
			LOG(L_ERR, "ERROR: rls_child_init(%d): "
					"Error while connecting database\n", _rank);
			return -1;
		}
		/* if (_rank == 0) {
			rls_lock();
			db_load_rls();
			rls_unlock();
		} */
	}

	return 0;
}

void rls_mod_destroy(void)
{
	/*int i, cnt;
	char *s;*/

	DEBUG_LOG("RLS module cleanup\n");

	/* destroy used XCAP servers */
/*	DEBUG_LOG(" ... xcap servers\n");
	if (xcap_servers) {
		cnt = ptr_vector_size(xcap_servers);
		DEBUG_LOG(" count = %d\n", cnt);
		for (i = 0; i < cnt; i++) {
			s = ptr_vector_get(xcap_servers, i);
			if (s) {
				DEBUG_LOG(" ... freeing %s (%p)\n", s, s);
				cds_free(s);
			}
		}
		ptr_vector_destroy(xcap_servers);
		mem_free(xcap_servers);
		xcap_servers = NULL;
	} */

	DEBUG_LOG(" ... rls\n");
	rls_destroy();
	DEBUG_LOG(" ... vs\n");
	vs_destroy();

	DEBUG_LOG(" ... time event management\n");
	time_event_management_destroy();

	DEBUG_LOG(" %s: ... db\n", __func__);
	if (use_db) {
		if (rls_db && rls_dbf.close) rls_dbf.close(rls_db);
		rls_db = NULL;
	}

	DEBUG_LOG(" ... common libs\n");
	qsa_cleanup();

	/* ??? if other module uses this libraries it might be a problem ??? */
/*	xmlCleanupParser(); */
	DEBUG_LOG("RLS module cleanup finished\n");
}

static int rls_subscribe_fixup(void** param, int param_no)
{
	/* char *xcap_server = NULL; */
	long send_errors = 0;

/*	if (param_no == 1) {
		if (!param) {
			LOG(L_ERR, "rls_subscribe_fixup(): XCAP server address not set!\n");
			return E_UNSPEC;
		}
		xcap_server = zt_strdup((char *)*param);
		if (!xcap_server) {
			LOG(L_ERR, "rls_subscribe_fixup(): Can't set XCAP server address!\n");
			return E_UNSPEC;
		}

		/ * store not only the root string? (create a structure rather?) * /
		ptr_vector_add(xcap_servers, xcap_server);

		DEBUG_LOG("rls_subscribe_fixup(): XCAP server is %s (%p)\n", xcap_server, xcap_server);
		*param = (void*)xcap_server;
	} */
	if (param_no == 1) {
		if (param) {
			if (*param) send_errors = atoi(*param);
		}
		DEBUG_LOG("rls_subscribe_fixup(): send errors: %ld\n", send_errors);
		*param = (void*)send_errors;
	}
	return 0;
}

