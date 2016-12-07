/*
 * Presence Agent, module interface
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * 2003-03-11  updated to the new module exports interface (andrei)
 * 2003-03-16  flags export parameter added (janakj)
 * 2004-06-08  updated to the new DB api (andrei)
 */


#include <signal.h>

#include "../../lib/srdb2/db.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "subscribe.h"
#include "publish.h"
#include "dlist.h"
#include "pa_mod.h"
#include "watcher.h"
#include "rpc.h"
#include "qsa_interface.h"
#include "async_auth.h"

#include <cds/logger.h>
#include <cds/cds.h>
#include <presence/qsa.h>

#include "status_query.h"
#include "offline_winfo.h"
#include "message.h"

MODULE_VERSION

static int pa_mod_init(void);  /* Module initialization function */
static int pa_child_init(int _rank);  /* Module child init function */
static void pa_destroy(void);  /* Module destroy function */
static int subscribe_fixup(void** param, int param_no); /* domain name -> domain pointer */
static void timer(unsigned int ticks, void* param); /* Delete timer for all domains */

int default_expires = 3600;  /* Default expires value if not present in the message (for SUBSCRIBE and PUBLISH) */
int max_subscription_expiration = 3600;  /* max expires value for subscribe */
int max_publish_expiration = 3600;  /* max expires value for subscribe */
int timer_interval = 10;     /* Expiration timer interval in seconds */
double default_priority = 0.0; /* Default priority of presence tuple */
static int default_priority_percentage = 0; /* expressed as percentage because config file grammar does not support floats */
int watcherinfo_notify = 1; /* send watcherinfo notifications */

/** TM bind */
struct tm_binds tmb;

dlg_func_t dlg_func;

fill_xcap_params_func fill_xcap_params = NULL;

/** database */
db_con_t* pa_db = NULL; /* Database connection handle */
db_func_t pa_dbf;

int use_db = 1;
str db_url = STR_NULL;
int use_place_table = 0;
#ifdef HAVE_LOCATION_PACKAGE
str pa_domain = STR_NULL;
#endif /* HAVE_LOCATION_PACKAGE */

/* DB tables */

char *presentity_table = "presentity";
char *presentity_contact_table = "presentity_contact";
char *presentity_notes_table = "presentity_notes";
char *extension_elements_table = "presentity_extensions";
char *tuple_extensions_table = "tuple_extensions";
char *tuple_notes_table = "tuple_notes";
char *watcherinfo_table = "watcherinfo";
char *place_table = "place";
char *offline_winfo_table = "offline_winfo";

/* columns in DB tables */

char *col_uri = "uri";
char *col_pdomain = "pdomain";
char *col_uid = "uid";
char *col_pres_id = "pres_id"; /* common for more tables */
char *col_xcap_params = "xcap_params";
char *col_tupleid = "tupleid"; /* common for more tables */
char *col_basic = "basic";
char *col_contact = "contact";
char *col_etag = "etag"; /* common for more tables */
char *col_published_id = "published_id";
char *col_priority = "priority";
char *col_expires = "expires"; /* common for more tables */
char *col_dbid = "dbid"; /* common for more tables */
char *col_note = "note";
char *col_lang = "lang";
char *col_element = "element"; /* common for more tables */
char *col_status_extension = "status_extension";

char *col_s_id = "s_id";
char *col_w_uri = "w_uri";
char *col_package = "package";
char *col_status = "status";
char *col_display_name = "display_name";
char *col_accepts = "accepts";
char *col_event = "event";
char *col_dialog = "dialog";
char *col_server_contact = "server_contact";
char *col_doc_index = "doc_index";

char *col_watcher = "watcher";
char *col_events = "events";
char *col_domain = "domain";
char *col_created_on = "created_on";
char *col_expires_on = "expires_on";

/* authorization parameters */
char *auth_type_str = NULL; /* type of authorization */
char *winfo_auth_type_str = "implicit"; /* type of authorization */
auth_params_t pa_auth_params;	/* structure filled according to parameters */
auth_params_t winfo_auth_params;	/* structure for watcherinfo filled according to parameters */
str pres_rules_file = STR_NULL; /* filename for XCAP queries */

int use_bsearch = 0;
int use_location_package = 0;

/* use callbacks to usrloc/??? - if 0 only pusblished information is used */
int use_callbacks = 1;
int use_offline_winfo = 0;
int offline_winfo_timer_interval = 3600;

int subscribe_to_users = 0;
str pa_subscription_uri = STR_NULL;

/* ignore 408 response on NOTIFY messages (don't destroy the subscription in the case of it if set */
int ignore_408_on_notify = 0;

/* locally generated NOTIFY is terget refresh transaction */
int notify_is_refresh = 1;

/*
 * Exported functions
 */
static cmd_export_t cmds[]={
	{"handle_subscription",   handle_subscription,   1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"handle_publish",        handle_publish,        1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	
	{"target_online",         target_online,         1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"check_subscription_status", check_subscription_status,   1, check_subscription_status_fix, REQUEST_ROUTE | FAILURE_ROUTE},
	{"store_winfo",           store_offline_winfo,   1, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"dump_stored_winfo",     dump_offline_winfo,    2, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	
	/* TODO: move into XCAP module? */
	{"authorize_message",     authorize_message,    1, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	
	/* FIXME: are these functions used to something by somebody */

/*
 *
	{"pua_exists",            pua_exists,            1, subscribe_fixup, REQUEST_ROUTE                },
	{"pa_handle_registration", pa_handle_registration,   1, subscribe_fixup, REQUEST_ROUTE },
	{"existing_subscription", existing_subscription, 1, subscribe_fixup, REQUEST_ROUTE                },
 	{"mangle_pidf",           mangle_pidf,           0, NULL, REQUEST_ROUTE | FAILURE_ROUTE},
	{"mangle_message_cpim",   mangle_message_cpim,   0, NULL,            REQUEST_ROUTE | FAILURE_ROUTE},*/

	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[]={
	{"default_expires",      PARAM_INT,    &default_expires      },
	{"max_subscription_expiration", PARAM_INT, &max_subscription_expiration },
	{"max_publish_expiration", PARAM_INT, &max_publish_expiration },
	
	{"auth",                 PARAM_STRING, &auth_type_str }, /* type of authorization: none, implicit, xcap, ... */
	{"winfo_auth",           PARAM_STRING, &winfo_auth_type_str }, /* type of authorization: none, implicit, xcap, ... */
	
	{"use_db",               PARAM_INT,    &use_db               },
	{"use_callbacks", PARAM_INT, &use_callbacks  }, /* use callbacks to usrloc/jabber ? */
	{"accept_internal_subscriptions", PARAM_INT, &accept_internal_subscriptions },
	{"watcherinfo_notify",   PARAM_INT, &watcherinfo_notify   }, /* accept winfo subscriptions ? */
	
	{"use_offline_winfo", PARAM_INT, &use_offline_winfo  }, /* use DB for offline winfo */
	{"offline_winfo_expiration", PARAM_INT, &offline_winfo_expiration }, /* how long hold information in DB */
	{"offline_winfo_timer", PARAM_INT, &offline_winfo_timer_interval }, /* basic ticks of "offline winfo" timer */

	{"db_url",               PARAM_STR,    &db_url               },
	{"pres_rules_file",      PARAM_STR,    &pres_rules_file },
	
	{"ignore_408_on_notify", PARAM_INT, &ignore_408_on_notify }, /* ignore 408 responses on NOTIFY */
	{"timer_interval",       PARAM_INT,    &timer_interval       },
	{"default_priority_percentage", PARAM_INT,    &default_priority_percentage  },
	
	{"auth_rules_refresh_time", PARAM_INT, &auth_rules_refresh_time },
	{"async_auth_queries", PARAM_INT, &async_auth_queries },
	{"max_auth_requests_per_tick", PARAM_INT, &max_auth_requests_per_tick },
	
	/* experimental, undocumented */
	{"subscribe_to_users",   PARAM_INT,    &subscribe_to_users },
	{"pa_subscription_uri",  PARAM_STR,    &pa_subscription_uri },
	
	/* undocumented still (TODO) */
	{"presentity_table",     PARAM_STRING, &presentity_table     },
	{"presentity_contact_table", PARAM_STRING, &presentity_contact_table     },
	{"watcherinfo_table",    PARAM_STRING, &watcherinfo_table    },
	{"offline_winfo_table", PARAM_STRING, &offline_winfo_table }, /* table with offline winfo */

	/* deprecated (undocumented, remove) */
	{"place_table",          PARAM_STRING, &place_table          },
	{"use_place_table",      PARAM_INT,    &use_place_table      },
	{"use_bsearch",          PARAM_INT,    &use_bsearch          },
	{"use_location_package", PARAM_INT,    &use_location_package },

	/* db columns, undocumented */

	/* DB: columns for table "offline_winfo" */

	{"uid_column", PARAM_STRING, &col_uid },
	{"watcher_column", PARAM_STRING, &col_watcher },
	{"events_column", PARAM_STRING, &col_events },
	{"domain_column", PARAM_STRING, &col_domain },
	{"status_column", PARAM_STRING, &col_status },
	{"created_on_column", PARAM_STRING, &col_created_on },
	{"expires_on_column", PARAM_STRING, &col_expires_on },
	{"dbid_column", PARAM_STRING, &col_dbid },

	/* DB: columns for table "presentity_contact" */

	{"pres_id_column", PARAM_STRING, &col_pres_id },
	{"basic_column", PARAM_STRING, &col_basic },
	{"expires_column", PARAM_STRING, &col_expires },
	{"priority_column", PARAM_STRING, &col_priority },
	{"contact_column", PARAM_STRING, &col_contact },
	{"tupleid_column", PARAM_STRING, &col_tupleid },
	{"etag_column", PARAM_STRING, &col_etag },
	{"published_id_column", PARAM_STRING, &col_published_id },

	/* DB: columns for table "presentity_extensions" */

	/*{"dbid_column", PARAM_STRING, &col_dbid },*/
	/*{"pres_id_column", PARAM_STRING, &col_pres_id },*/
	/*{"etag_column", PARAM_STRING, &col_etag },*/
	{"element_column", PARAM_STRING, &col_element },
	/*{"expires_column", PARAM_STRING, &col_expires },*/

	/* DB: columns for table "presentity_notes" */

	/*{"dbid_column", PARAM_STRING, &col_dbid },*/
	/*{"pres_id_column", PARAM_STRING, &col_pres_id },*/
	/*{"etag_column", PARAM_STRING, &col_etag },*/
	{"note_column", PARAM_STRING, &col_note },
	{"lang_column", PARAM_STRING, &col_lang },
	/*{"expires_column", PARAM_STRING, &col_expires },*/

	/* DB: columns for table "presentity" */

	/*{"pres_id_column", PARAM_STRING, &col_pres_id },*/
	{"uri_column", PARAM_STRING, &col_uri },
	/*{"uid_column", PARAM_STRING, &col_uid },*/
	{"pdomain_column", PARAM_STRING, &col_pdomain },
	{"xcap_params_column", PARAM_STRING, &col_xcap_params },

	/* DB: columns for table "tuple_extensions" */

	/*{"pres_id_column", PARAM_STRING, &col_pres_id },*/
	/*{"tupleid_column", PARAM_STRING, &col_tupleid },*/
	/*{"element_column", PARAM_STRING, &col_element },*/
	{"status_extension_column", PARAM_STRING, &col_status_extension },

	/* DB: columns for table "tuple_notes" */

	/*{"pres_id_column", PARAM_STRING, &col_pres_id },*/
	/*{"tupleid_column", PARAM_STRING, &col_tupleid },*/
	/*{"note_column", PARAM_STRING, &col_note },*/
	/*{"lang_column", PARAM_STRING, &col_lang },*/

	/* DB: columns for table "watcherinfo" */

	{"w_uri_column", PARAM_STRING, &col_w_uri },
	{"display_name_column", PARAM_STRING, &col_display_name },
	{"s_id_column", PARAM_STRING, &col_s_id },
	{"package_column", PARAM_STRING, &col_package },
	/*{"status_column", PARAM_STRING, &col_status },*/
	{"event_column", PARAM_STRING, &col_event },
	/*{"expires_column", PARAM_STRING, &col_expires },*/
	{"accepts_column", PARAM_STRING, &col_accepts },
	/*{"pres_id_column", PARAM_STRING, &col_pres_id },*/
	{"server_contact_column", PARAM_STRING, &col_server_contact },
	{"dialog_column", PARAM_STRING, &col_dialog },
	{"doc_index_column", PARAM_STRING, &col_doc_index },

	{"notify_is_refresh", PARAM_INT, &notify_is_refresh },

	{0, 0, 0}
};

struct module_exports exports = {
	"pa",
	cmds,           /* Exported functions */
	pa_rpc_methods, /* RPC methods */
	params,         /* Exported parameters */
	pa_mod_init,    /* module initialization function */
	0,              /* response function*/
	pa_destroy,     /* destroy function */
	0,              /* oncancel function */
	pa_child_init   /* per-child init function */
};

char* decode_mime_type(char *start, char *end, unsigned int *mime_type);

static void test_mimetype_parser(void)
{
	static struct mimetype_test {
		char *string;
		int parsed;
	} mimetype_tests[] = {
		{ "application/cpim", MIMETYPE(APPLICATION,CPIM) },
		{ "text/plain", MIMETYPE(TEXT,PLAIN) },
		{ "message/cpim", MIMETYPE(MESSAGE,CPIM) },
		{ "application/sdp", MIMETYPE(APPLICATION,SDP) },
		{ "application/cpl+xml", MIMETYPE(APPLICATION,CPLXML) },
		{ "application/pidf+xml", MIMETYPE(APPLICATION,PIDFXML) },
		{ "application/rlmi+xml", MIMETYPE(APPLICATION,RLMIXML) },
		{ "multipart/related", MIMETYPE(MULTIPART,RELATED) },
		{ "application/lpidf+xml", MIMETYPE(APPLICATION,LPIDFXML) },
		{ "application/xpidf+xml", MIMETYPE(APPLICATION,XPIDFXML) },
		{ "application/watcherinfo+xml", MIMETYPE(APPLICATION,WATCHERINFOXML) },
		{ "application/external-body", MIMETYPE(APPLICATION,EXTERNAL_BODY) },
		{ "application/*", MIMETYPE(APPLICATION,ALL) },
		{ "text/xml+msrtc.pidf", MIMETYPE(TEXT,XML_MSRTC_PIDF) },
		{ "application/cpim-pidf+xml", MIMETYPE(APPLICATION,CPIM_PIDFXML) },
		{ NULL, 0 }
	};
	struct mimetype_test *mt = &mimetype_tests[0];
	LOG(L_DBG, "Presence Agent - testing mimetype parser\n");
	while (mt->string) {
		unsigned int pmt;
		LOG(L_DBG, "Presence Agent - parsing mimetype %s\n", mt->string);
		decode_mime_type(mt->string, mt->string+strlen(mt->string), &pmt);
		if (pmt != mt->parsed) {
			LOG(L_ERR, "Parsed mimetype %s got %x expected %x\n",
			    mt->string, pmt, mt->parsed);
		}

		mt++;
	}
}

static int set_auth_params(auth_params_t *dst, const char *auth_type_str,
		const char *log_str)
{
	if (!auth_type_str) {
		LOG(L_ERR, "no subscription authorization type for %s given, using \'implicit\'!\n", log_str);
		dst->type = auth_none;
		return 0;
	}
	if (strcmp(auth_type_str, "xcap") == 0) {
		dst->type = auth_xcap;
		return 0;
	}
	if (strcmp(auth_type_str, "none") == 0) {
		dst->type = auth_none;
		LOG(L_WARN, "using \'none\' subscription authorization for %s!\n", log_str);
		return 0;
	}
	if (strcmp(auth_type_str, "implicit") == 0) {
		dst->type = auth_implicit;
		return 0;
	}

	LOG(L_ERR, "Can't resolve subscription authorization for %s type: \'%s\'."
			" Use one of: none, implicit, xcap.\n", log_str, auth_type_str);
	return -1;
}

static int pa_mod_init(void)
{
	load_tm_f load_tm;
	bind_dlg_mod_f bind_dlg;

	/* SER_PROFILE_INIT - init in child (each process needs it) */
	
	test_mimetype_parser();
	DBG("Presence Agent - initializing\n");

	DBG(" ... common libraries\n");
	qsa_initialize();

	if (subscribe_to_users) {
		if (is_str_empty(&pa_subscription_uri)) {
			ERR("pa_subscription_uri must be set if set subscribe_to_users\n");
			return -1;
		}
		if (accept_internal_subscriptions) {
			ERR("impossible to have set accept_internal_subscriptions together with subscribe_to_users\n");
			return -1;
		}
	}
	
	/* set authorization type according to requested "auth type name"
	 * and other (type specific) parameters */
	if (set_auth_params(&pa_auth_params, auth_type_str, "presence") != 0) return -1;

	/* set authorization type for watcherinfo
	 * according to requested "auth type name"
	 * and other (type specific) parameters */
	if (set_auth_params(&winfo_auth_params, winfo_auth_type_str, "watcher info") != 0) return -1;

	     /* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "Can't import tm\n");
		return -1;
	}
	     /* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) {
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

	/* Register cache timer */
	register_timer(timer, 0, timer_interval);
	
	/* register offline winfo timer */
	if (use_offline_winfo)
		register_timer(offline_winfo_timer, 0, offline_winfo_timer_interval);

#ifdef HAVE_LOCATION_PACKAGE
	if (pa_domain.len == 0) {
		LOG(L_ERR, "pa_mod_init(): pa_domain must be specified\n");
		return -1;
	}
	LOG(L_DBG, "pa_mod: pa_mod=%s\n", ZSW(pa_domain.s));
#endif /* HAVE_LOCATION_PACKAGE */

	LOG(L_DBG, "pa_mod: use_db=%d us_offline_winfo=%d db_url.s=%s\n",
	    use_db, use_offline_winfo, ZSW(db_url.s));

	if (use_db || use_offline_winfo) {
		if (!db_url.len) {
			LOG(L_ERR, "pa_mod_init(): no db_url specified but DB has to be used "
					"(use_db=%d us_offline_winfo=%d)\n", use_db, use_offline_winfo);
			return -1;
		}
		if (bind_dbmod(db_url.s, &pa_dbf) < 0) { /* Find database module */
			LOG(L_ERR, "pa_mod_init(): Can't bind database module via url %s\n", db_url.s);
			return -1;
		}

		if (!DB_CAPABILITY(pa_dbf, DB_CAP_ALL)) {
			LOG(L_ERR, "pa_mod_init(): Database module does not implement all functions needed by the module\n");
			return -1;
		}
	}

	default_priority = ((double)default_priority_percentage) / 100.0;

	if (pa_qsa_interface_init() != 0) {
		LOG(L_CRIT, "pa_mod_init(): QSA interface initialization failed!\n");
		return -1;
	}

	fill_xcap_params = (fill_xcap_params_func)find_export("fill_xcap_params", 0, -1);

	if (async_auth_timer_init() < 0) {
		ERR("can't init async timer\n");
		return -1;
	}
	
	LOG(L_DBG, "pa_mod_init done\n");
	return 0;
}

/* can be called after pa_mod_init and creates new PA DB connection */
db_con_t* create_pa_db_connection()
{
	if (!(use_db || use_offline_winfo)) return NULL;
	if (!pa_dbf.init) return NULL;

	return pa_dbf.init(db_url.s);
}

void close_pa_db_connection(db_con_t* db)
{
	if (db && pa_dbf.close) pa_dbf.close(db);
}

static int pa_child_init(int _rank)
{
	/* Shall we use database ? */
	if (use_db || use_offline_winfo) { /* Yes */
		if (_rank==PROC_INIT || _rank==PROC_MAIN || _rank==PROC_TCP_MAIN)
			return 0; /* do nothing for the main or tcp_main processes */
		pa_db = create_pa_db_connection();
		if (!pa_db) {
			LOG(L_ERR, "ERROR: pa_child_init(%d): "
					"Error while connecting database\n", _rank);
			return -1;
		}
	}

#ifdef DO_TRACE
	SER_PROFILE_INIT
#endif

	return 0;
}

static void pa_destroy(void)
{
	DBG("PA module cleanup\n");

	DBG("destroying PA module\n");
	DBG(" ... qsa interface\n");
	pa_qsa_interface_destroy();

	DBG(" ... pdomains\n");
	free_all_pdomains();
	if ((use_db || use_offline_winfo) && pa_db) {
		DBG(" ... closing db connection\n");
		close_pa_db_connection(pa_db);
	}
	pa_db = NULL;

	DBG(" ... cleaning common libs\n");
	qsa_cleanup();
}


/*
 * Convert char* parameter to pdomain_t* pointer
 */
static int subscribe_fixup(void** param, int param_no)
{
	pdomain_t* d;

	if (param_no == 1) {
		LOG(L_DBG, "subscribe_fixup: pdomain name is %s\n", (char*)*param);
		if (register_pdomain((char*)*param, &d) < 0) {
			LOG(L_ERR, "subscribe_fixup(): Error while registering domain\n");
			return E_UNSPEC;
		}

		*param = (void*)d;
	}
	return 0;
}


/*
 * Timer handler
 */
static void timer(unsigned int ticks, void* param)
{
	if (timer_all_pdomains() != 0) {
		LOG(L_ERR, "timer(): Error while synchronizing domains\n");
	}
}
