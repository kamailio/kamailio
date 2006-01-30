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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * 2003-03-11  updated to the new module exports interface (andrei)
 * 2003-03-16  flags export parameter added (janakj)
 * 2004-06-08  updated to the new DB api (andrei)
 */


#include <signal.h>

#include "../../db/db.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "subscribe.h"
#include "publish.h"
#include "dlist.h"
#include "location.h"
#include "pa_mod.h"
#include "watcher.h"
#include "rpc.h"
#include "qsa_interface.h"

#include <cds/logger.h>
#include <cds/cds.h>
#include <presence/qsa.h>

#include "status_query.h"

MODULE_VERSION

static int pa_mod_init(void);  /* Module initialization function */
static int pa_child_init(int _rank);  /* Module child init function */
static void pa_destroy(void);  /* Module destroy function */
static int subscribe_fixup(void** param, int param_no); /* domain name -> domain pointer */
static void timer(unsigned int ticks, void* param); /* Delete timer for all domains */

int default_expires = 3600;  /* Default expires value if not present in the message (for SUBSCRIBE and PUBLISH) */
int timer_interval = 10;     /* Expiration timer interval in seconds */
double default_priority = 0.0; /* Default priority of presence tuple */
static int default_priority_percentage = 0; /* expressed as percentage because config file grammar does not support floats */
int watcherinfo_notify = 1; /* send watcherinfo notifications */

/** TM bind */
struct tm_binds tmb;

dlg_func_t dlg_func;


/** database */
db_con_t* pa_db = NULL; /* Database connection handle */
db_func_t pa_dbf;

int use_db = 1;
str db_url = STR_NULL;
int use_place_table = 0;
#ifdef HAVE_LOCATION_PACKAGE
str pa_domain = STR_NULL;
#endif /* HAVE_LOCATION_PACKAGE */
char *presentity_table = "presentity";
char *presentity_contact_table = "presentity_contact";
char *presentity_notes_table = "presentity_notes";
char *tuple_notes_table = "tuple_notes";
char *watcherinfo_table = "watcherinfo";
char *place_table = "place";

/* authorization parameters */
char *auth_type_str = NULL; /* type of authorization */
char *auth_xcap_root = NULL;	/* must be set if xcap authorization */
char *winfo_auth_type_str = "implicit"; /* type of authorization */
char *winfo_auth_xcap_root = NULL;	/* must be set if xcap authorization */
auth_params_t pa_auth_params;	/* structure filled according to parameters */
auth_params_t winfo_auth_params;	/* structure for watcherinfo filled according to parameters */

int use_bsearch = 0;
int use_location_package = 0;
int authorize_watchers = 1;
int callback_update_db = 1;
int callback_lock_pdomain = 1;
int new_tuple_on_publish = 1;
int pa_pidf_priority = 1;

/* use callbacks to usrloc/??? - if 0 only pusblished information is used */
int use_callbacks = 1;
/*
 * Exported functions
 */
static cmd_export_t cmds[]={
	{"handle_subscription",   handle_subscription,   1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"handle_publish",        handle_publish,        1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},

	/* still undocumented (TODO) */
	{"target_online",         target_online,         1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},

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
	{"default_priority_percentage", PARAM_INT,    &default_priority_percentage  },
	{"timer_interval",       PARAM_INT,    &timer_interval       },
	{"use_db",               PARAM_INT,    &use_db               },
	{"use_place_table",      PARAM_INT,    &use_place_table      },
	{"use_bsearch",          PARAM_INT,    &use_bsearch          },
	{"use_location_package", PARAM_INT,    &use_location_package },
	{"db_url",               PARAM_STR,    &db_url               },
#ifdef HAVE_LOCATION_PACKAGE
	{"pa_domain",            PARAM_STR,    &pa_domain            },
#endif /* HAVE_LOCATION_PACKAGE */
	{"presentity_table",     PARAM_STRING, &presentity_table     },
	{"presentity_contact_table", PARAM_STRING, &presentity_contact_table     },
	{"watcherinfo_table",    PARAM_STRING, &watcherinfo_table    },
	{"place_table",          PARAM_STRING, &place_table          },
	{"auth",                 PARAM_STRING, &auth_type_str }, /* type of authorization: none, implicit, xcap, ... */
	{"auth_xcap_root",       PARAM_STRING, &auth_xcap_root }, /* xcap root settings - must be set for xcap auth */
	{"winfo_auth",           PARAM_STRING, &winfo_auth_type_str }, /* type of authorization: none, implicit, xcap, ... */
	{"winfo_auth_xcap_root", PARAM_STRING, &winfo_auth_xcap_root }, /* xcap root settings - must be set for xcap auth */

	/* undocumented still (TODO) */
	{"use_callbacks", PARAM_INT, &use_callbacks  },
	{"watcherinfo_notify",   PARAM_INT, &watcherinfo_notify   },

	/* not used -> remove (TODO) */
	{"authorize_watchers",   PARAM_INT, &authorize_watchers  },
	{"callback_update_db",   PARAM_INT, &callback_update_db   },
	{"callback_lock_pdomain",PARAM_INT, &callback_lock_pdomain },
	{"pidf_priority",        PARAM_INT, &pa_pidf_priority  },
	{"new_tuple_on_publish", PARAM_INT, &new_tuple_on_publish  },
	
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


void pa_sig_handler(int s)
{
	DBG("PA:pa_worker:%d: SIGNAL received=%d\n **************", getpid(), s);
}

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
		int pmt;
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
		char *xcap_root, const char *log_str)
{
	dst->xcap_root = NULL;
	if (!auth_type_str) {
		LOG(L_ERR, "no subscription authorization type for %s given, using \'implicit\'!\n", log_str);
		dst->type = auth_none;
		return 0;
	}
	if (strcmp(auth_type_str, "xcap") == 0) {
		if (!xcap_root) {
			LOG(L_ERR, "XCAP authorization for %s selected, but no auth_xcap_root given!\n", log_str);
			return -1;
		}
		dst->xcap_root = xcap_root;
		if (!(*dst->xcap_root)) {
			LOG(L_ERR, "XCAP authorization for %s selected, but empty auth_xcap_root given!\n", log_str);
			return -1;
		}
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

	test_mimetype_parser();
	DEBUG_LOG("Presence Agent - initializing\n");

	DEBUG_LOG(" ... common libraries\n");
	cds_initialize();
	qsa_initialize();

	/* set authorization type according to requested "auth type name"
	 * and other (type specific) parameters */
	if (set_auth_params(&pa_auth_params, auth_type_str,
				auth_xcap_root, "presence") != 0) return -1;

	/* set authorization type for watcherinfo
	 * according to requested "auth type name"
	 * and other (type specific) parameters */
	if (set_auth_params(&winfo_auth_params, winfo_auth_type_str,
				winfo_auth_xcap_root, "watcher info") != 0) return -1;

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

#ifdef HAVE_LOCATION_PACKAGE
	if (pa_domain.len == 0) {
		LOG(L_ERR, "pa_mod_init(): pa_domain must be specified\n");
		return -1;
	}
	LOG(L_DBG, "pa_mod: pa_mod=%s\n", ZSW(pa_domain.s));
#endif /* HAVE_LOCATION_PACKAGE */

	LOG(L_DBG, "pa_mod: use_db=%d db_url.s=%s\n",
	    use_db, ZSW(db_url.s));
	if (use_db) {
		if (!db_url.len) {
			LOG(L_ERR, "pa_mod_init(): no db_url specified but use_db=1\n");
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

	LOG(L_DBG, "pa_mod_init done\n");
	return 0;
}

/* can be called after pa_mod_init and creates new PA DB connection */
db_con_t* create_pa_db_connection()
{
	if (!use_db) return NULL;
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
	if (use_db) { /* Yes */
		pa_db = create_pa_db_connection();
		if (!pa_db) {
			LOG(L_ERR, "ERROR: pa_child_init(%d): "
					"Error while connecting database\n", _rank);
			return -1;
		}
	}

	// signal(SIGSEGV, pa_sig_handler);

	return 0;
}

static void pa_destroy(void)
{
	DEBUG_LOG("PA module cleanup\n");
	DEBUG_LOG("destroying PA module\n");
	DEBUG_LOG(" ... qsa interface\n");
	pa_qsa_interface_destroy();

	DEBUG_LOG(" ... pdomains\n");
	free_all_pdomains();
	if (use_db && pa_db) {
		DEBUG_LOG(" ... closing db connection\n");
		close_pa_db_connection(pa_db);
	}
	pa_db = NULL;

	DEBUG_LOG(" ... cleaning common libs\n");
	qsa_cleanup();
	cds_cleanup();
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

/*
 * compare two str's
 */
int str_strcmp(const str *stra, const str *strb)
{
     int i;
     int alen = stra->len;
     int blen = strb->len;
     int minlen = (alen < blen ? alen : blen);
     for (i = 0; i < minlen; i++) {
	  const char a = stra->s[i];
	  const char b = strb->s[i];
	  if (a < b) return -1;
	  if (a > b) return 1;
     }
     if (alen < blen)
	  return -1;
     else if (blen > alen)
	  return 1;
     else
	  return 0;
}

/*
 * case-insensitive compare two str's
 */
int str_strcasecmp(const str *stra, const str *strb)
{
     int i;
     int alen = stra->len;
     int blen = strb->len;
     int minlen = (alen < blen ? alen : blen);
     for (i = 0; i < minlen; i++) {
	  const char a = tolower(stra->s[i]);
	  const char b = tolower(strb->s[i]);
	  if (a < b) return -1;
	  if (a > b) return 1;
     }
     if (alen < blen)
	  return -1;
     else if (blen > alen)
	  return 1;
     else
	  return 0;
}
