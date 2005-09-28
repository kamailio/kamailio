/*
 * Presence Agent, module interface
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 2003-03-11  updated to the new module exports interface (andrei)
 * 2003-03-16  flags export parameter added (janakj)
 * 2004-06-08  updated to the new DB api (andrei)
 */


#include <signal.h>

#include "../../fifo_server.h"
#include "../../db/db.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "subscribe.h"
#include "publish.h"
#include "dlist.h"
#include "unixsock.h"
#include "location.h"
#include "pa_mod.h"
#include "pidf.h"
#include "watcher.h"

MODULE_VERSION

static int pa_mod_init(void);  /* Module initialization function */
static int pa_child_init(int _rank);  /* Module child init function */
static void pa_destroy(void);  /* Module destroy function */
static int subscribe_fixup(void** param, int param_no); /* domain name -> domain pointer */
static void timer(unsigned int ticks, void* param); /* Delete timer for all domains */

int default_expires = 3600;  /* Default expires value if not present in the message */
int timer_interval = 10;     /* Expiration timer interval in seconds */
double default_priority = 0.5; /* Default priority of presence tuple */
static int default_priority_percentage = 50; /* expressed as percentage because config file grammar does not support floats */
int watcherinfo_notify = 1; /* send watcherinfo notifications */

/** TM bind */
struct tm_binds tmb;

/** database */
db_con_t* pa_db; /* Database connection handle */
db_func_t pa_dbf;

int use_db = 1;
str db_url;
int use_place_table = 0;
#ifdef HAVE_LOCATION_PACKAGE
str pa_domain;
#endif /* HAVE_LOCATION_PACKAGE */
char *presentity_table = "presentity";
char *presentity_contact_table = "presentity_contact";
char *watcherinfo_table = "watcherinfo";
char *place_table = "place";
int use_bsearch = 0;
int use_location_package = 0;
int new_watcher_pending = 0;
int callback_update_db = 1;
int callback_lock_pdomain = 1;
int new_tuple_on_publish = 1;
int pa_pidf_priority = 1;

/*
 * Exported functions
 */
static cmd_export_t cmds[]={
	{"pa_handle_registration", pa_handle_registration,   1, subscribe_fixup, REQUEST_ROUTE },
	{"handle_subscription",   handle_subscription,   1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"handle_publish",        handle_publish,        1, subscribe_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"existing_subscription", existing_subscription, 1, subscribe_fixup, REQUEST_ROUTE                },
	{"pua_exists",            pua_exists,            1, subscribe_fixup, REQUEST_ROUTE                },
	{"mangle_pidf",           mangle_pidf,           0, NULL, REQUEST_ROUTE | FAILURE_ROUTE},
	{"mangle_message_cpim",   mangle_message_cpim,   0, NULL,            REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[]={
	{"default_expires",      INT_PARAM, &default_expires      },
	{"default_priority_percentage",     INT_PARAM, &default_priority_percentage  },
	{"timer_interval",       INT_PARAM, &timer_interval       },
	{"use_db",               INT_PARAM, &use_db               },
	{"use_place_table",      INT_PARAM, &use_place_table      },
	{"use_bsearch",          INT_PARAM, &use_bsearch          },
	{"use_location_package", INT_PARAM, &use_location_package },
	{"db_url",               STR_PARAM, &db_url.s             },
#ifdef HAVE_LOCATION_PACKAGE
	{"pa_domain",            STR_PARAM, &pa_domain.s          },
#endif /* HAVE_LOCATION_PACKAGE */
	{"presentity_table",     STR_PARAM, &presentity_table     },
	{"presentity_contact_table", STR_PARAM, &presentity_contact_table     },
	{"watcherinfo_table",    STR_PARAM, &watcherinfo_table    },
	{"place_table",          STR_PARAM, &place_table          },
	{"new_watcher_pending",  INT_PARAM, &new_watcher_pending  },
	{"callback_update_db",   INT_PARAM, &callback_update_db   },
	{"callback_lock_pdomain", INT_PARAM, &callback_lock_pdomain },
	{"new_tuple_on_publish", INT_PARAM, &new_tuple_on_publish  },
	{"pidf_priority",        INT_PARAM, &pa_pidf_priority  },
	{"watcherinfo_notify",   INT_PARAM, &watcherinfo_notify   },
	{0, 0, 0}
};


struct module_exports exports = {
	"pa", 
	cmds,        /* Exported functions */
	params,      /* Exported parameters */
	pa_mod_init, /* module initialization function */
	0,           /* response function*/
	pa_destroy,  /* destroy function */
	0,           /* oncancel function */
	pa_child_init/* per-child init function */
};


void pa_sig_handler(int s) 
{
	DBG("PA:pa_worker:%d: SIGNAL received=%d\n **************", getpid(), s);
}

static struct mimetype_test {
	const char *string;
	int parsed;
} mimetype_tests[] = {
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
#ifdef SUBTYPE_XML_MSRTC_PIDF
	{ "text/xml+msrtcp.pidf", MIMETYPE(TEXT,XML_MSRTC_PIDF) },
#endif
	{ NULL, 0 }
};

char* decode_mime_type(char *start, char *end, unsigned int *mime_type);

static void test_mimetype_parser(void)
{
	struct mimetype_test *mt = &mimetype_tests[0];
	DBG("Presence Agent - testing mimetype parser\n");
	while (mt->string) {
		unsigned int pmt;
		LOG(L_DBG, "Presence Agent - parsing mimetype %s\n", mt->string);
		decode_mime_type((char*)mt->string,
			(char*)(mt->string+strlen(mt->string)), &pmt);
		if (pmt != mt->parsed) {
			LOG(L_ERR, "Parsed mimetype %s got %x expected %x\n",
			    mt->string, pmt, mt->parsed);
		}
		mt++;
	}
}

static int pa_mod_init(void)
{
	DBG("Presence Agent - initializing\n");

	test_mimetype_parser();

	/* load the TM API */
	if (load_tm_api(&tmb)!=0) {
		LOG(L_ERR, "ERROR:acc:mod_init: can't load TM API\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_pa_publish, "pa_publish", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo pa_publish\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_pa_presence, "pa_presence", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo pa_presence\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_pa_location, "pa_location", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo pa_location\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_pa_location_contact, "pa_location_contact", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo pa_location_contact\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_pa_watcherinfo, "pa_watcherinfo", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo pa_watcherinfo\n");
		return -1;
	}

	if (init_unixsock_iface() < 0) {
		LOG(L_ERR, "pa_mod_init: Error while initializing unix socket interface\n");
		return -1;
	}

	     /* Register cache timer */
	register_timer(timer, 0, timer_interval);

	LOG(L_CRIT, "db_url=%p\n", db_url.s);
	LOG(L_CRIT, "db_url=%s\n", ZSW(db_url.s));
	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	LOG(L_CRIT, "db_url.len=%d\n", db_url.len);
#ifdef HAVE_LOCATION_PACKAGE
	if (pa_domain.len == 0) {
		LOG(L_ERR, "pa_mod_init(): pa_domain must be specified\n");
		return -1;
	}
	LOG(L_CRIT, "pa_mod: pa_mod=%s\n", ZSW(pa_domain.s));
#endif /* HAVE_LOCATION_PACKAGE */

	LOG(L_CRIT, "pa_mod: use_db=%d db_url.s=%s\n", 
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

	LOG(L_CRIT, "pa_mod_init done\n");
	return 0;
}


static int pa_child_init(int _rank)
{
 	     /* Shall we use database ? */
	if (use_db) { /* Yes */
		pa_db = NULL;
		pa_db = pa_dbf.init(db_url.s); /* Initialize a new separate 
										  connection */
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
	free_all_pdomains();
	if (use_db && (pa_dbf.close != NULL) && (pa_db != NULL)) {
		pa_dbf.close(pa_db);
	}
}


/*
 * Convert char* parameter to pdomain_t* pointer
 */
static int subscribe_fixup(void** param, int param_no)
{
	pdomain_t* d;

	if (param_no == 1) {
		LOG(L_ERR, "subscribe_fixup: pdomain name is %s\n", (char*)*param);
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
