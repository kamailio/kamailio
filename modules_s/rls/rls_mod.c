#include "rls_mod.h"
#include "fifo.h"
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

/* authorization parameters */
char *auth_type_str = NULL; /* type of authorization: none,implicit,xcap */
char *auth_xcap_root = NULL;	/* must be set if xcap authorization */
int db_mode = 0; /* 0 -> no DB, 1 -> write through */
char *db_url = NULL;

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
	{"auth", STR_PARAM, &auth_type_str }, /* type of authorization: none, implicit, xcap, ... */
	{"auth_xcap_root", STR_PARAM, &auth_xcap_root }, /* xcap root settings - must be set for xcap auth */
	{"db_mode", INT_PARAM, &db_mode },
	{"db_url", STR_PARAM, &db_url },
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
dlg_func_t dlg_func;
int use_db = 0;

int rls_min_expiration = 60;
int rls_max_expiration = 7200;
int rls_default_expiration = 3761;
rls_auth_params_t rls_auth_params;	/* structure filled according to parameters (common for all XCAP servers now) */

/* TODO: settings of other xcap parameters (auth, ssl, ...) */

/* internal data members */
static ptr_vector_t *xcap_servers = NULL;
db_con_t* rls_db = NULL; /* database connection handle */
db_func_t rls_dbf;	/* database functions */

static int set_auth_params(rls_auth_params_t *dst, const char *auth_type_str, char *xcap_root)
{
	dst->xcap_root = NULL;
	if (!auth_type_str) {
		LOG(L_ERR, "no subscription authorization type given, using \'implicit\'!\n");
		dst->type = rls_auth_none;
		return 0;
	}
	if (strcmp(auth_type_str, "xcap") == 0) {
		if (!xcap_root) {
			LOG(L_ERR, "XCAP authorization selected, but no auth_xcap_root given!\n");
			return -1;
		}
		dst->xcap_root = xcap_root;
		if (!(*dst->xcap_root)) {
			LOG(L_ERR, "XCAP authorization selected, but empty auth_xcap_root given!\n");
			return -1;
		}
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

int rls_mod_init(void)
{
    load_tm_f load_tm;
	bind_dlg_mod_f bind_dlg;

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

	xcap_servers = (ptr_vector_t*)shm_malloc(sizeof(ptr_vector_t));
	if (!xcap_servers) {
		LOG(L_ERR, "rls_mod_init(): can't allocate memory for XCAP servers vector\n");
		return -1;
	}
	ptr_vector_init(xcap_servers, 8);

	/* set authorization type according to requested "auth type name"
	 * and other (type specific) parameters */
	if (set_auth_params(&rls_auth_params, auth_type_str, auth_xcap_root) != 0) return -1;


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

	rls_fifo_register();
	
	/* ??? if other module uses this libraries it might be a problem ??? */
	xmlInitParser();
	curl_global_init(CURL_GLOBAL_ALL);
	
	return 0;
}

int rls_child_init(int _rank)
{
	rls_db = NULL;
	if (use_db) {
		if (rls_dbf.init) rls_db = rls_dbf.init(db_url);
		if (!rls_db) {
			LOG(L_ERR, "ERROR: rls_child_init(%d): "
					"Error while connecting database\n", _rank);
			return -1;
		}
		if (_rank == 0) {
			rls_lock();
			db_load_rls();
			rls_unlock();
		}
	}

	return 0;
}

void rls_mod_destroy(void)
{
	int i, cnt;
	char *s;

	DEBUG_LOG("rls_mod_destroy()\n");
	
	DEBUG_LOG(" ... xcap servers\n");
	/* destroy used XCAP servers */
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
		shm_free(xcap_servers);
		xcap_servers = NULL;
	}
	
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
	
	/* ??? if other module uses this libraries it might be a problem ??? */
/*	xmlCleanupParser();
	curl_global_cleanup();*/
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
		
		DEBUG_LOG("rls_subscribe_fixup(): XCAP server is %s (%p)\n", xcap_server, xcap_server);
		*param = (void*)xcap_server;
	}
	if (param_no == 2) {
		if (param) {
			if (*param) send_errors = atoi(*param);
		}
		DEBUG_LOG("rls_subscribe_fixup(): send errors: %d\n", send_errors);
		*param = (void*)send_errors;
	}
	return 0;
}

