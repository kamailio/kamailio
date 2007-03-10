/*
 * $Id$
 *
 * Usrloc module interface
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
 * History:
 * ---------
 * 2003-01-27 timer activity printing #ifdef-ed to EXTRA_DEBUG (jiri)
 * 2003-03-11 New module interface (janakj)
 * 2003-03-12 added replication and state columns (nils)
 * 2003-03-16 flags export parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-21 failed fifo init stops init process (jiri)
 * 2004-03-17 generic callbacks added (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 */

#include <stdio.h>
#include "ul_mod.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../timer.h"     /* register_timer */
#include "../../globals.h"   /* is_main */
#include "../../ut.h"        /* str_init */
#include "dlist.h"           /* register_udomain */
#include "udomain.h"         /* {insert,delete,get,release}_urecord */
#include "urecord.h"         /* {insert,delete,get}_ucontact */
#include "ucontact.h"        /* update_ucontact */
#include "ul_unixsock.h"
#include "ul_mi.h"
#include "ul_callback.h"
#include "notify.h"
#include "usrloc.h"

MODULE_VERSION

#define USER_COL       "username"
#define DOMAIN_COL     "domain"
#define CONTACT_COL    "contact"
#define EXPIRES_COL    "expires"
#define Q_COL          "q"
#define CALLID_COL     "callid"
#define CSEQ_COL       "cseq"
#define FLAGS_COL      "flags"
#define CFLAGS_COL     "cflags"
#define USER_AGENT_COL "user_agent"
#define RECEIVED_COL   "received"
#define PATH_COL       "path"
#define SOCK_COL       "socket"
#define METHODS_COL    "methods"
#define LAST_MOD_COL   "last_modified"

static int mod_init(void);                          /* Module initialization function */
static void destroy(void);                          /* Module destroy function */
static void timer(unsigned int ticks, void* param); /* Timer handler */
static int child_init(int rank);                    /* Per-child init function */
static int mi_child_init();

extern int bind_usrloc(usrloc_api_t* api);
extern int ul_locks_no;
/*
 * Module parameters and their default values
 */

/* Name of column containing usernames */
str user_col        = str_init(USER_COL);
/* Name of column containing domains */
str domain_col      = str_init(DOMAIN_COL);
/* Name of column containing contact addresses */
str contact_col     = str_init(CONTACT_COL);
/* Name of column containing expires values */
str expires_col     = str_init(EXPIRES_COL);
/* Name of column containing q values */
str q_col           = str_init(Q_COL);
/* Name of column containing callid string */
str callid_col      = str_init(CALLID_COL);
/* Name of column containing cseq values */
str cseq_col        = str_init(CSEQ_COL);
/* Name of column containing internal flags */
str flags_col       = str_init(FLAGS_COL);
/* Name of column containing contact flags */
str cflags_col      = str_init(CFLAGS_COL);
/* Name of column containing user agent string */
str user_agent_col  = str_init(USER_AGENT_COL);
/* Name of column containing transport info of REGISTER */
str received_col    = str_init(RECEIVED_COL);
/* Name of column containing the Path header */
str path_col        = str_init(PATH_COL);
/* Name of column containing the received socket */
str sock_col        = str_init(SOCK_COL);
/* Name of column containing the supported methods */
str methods_col     = str_init(METHODS_COL);
/* Name of column containing the last modified date */
str last_mod_col     = str_init(LAST_MOD_COL);

/* Database URL */
str db_url          = str_init(DEFAULT_DB_URL);
/* Timer interval in seconds */
int timer_interval  = 60;
/* Database sync scheme: 0-no db, 1-write through, 2-write back, 3-only db */
int db_mode         = 0;
/* Whether usrloc should use domain part of aor */
int use_domain      = 0;
/* By default do not enable timestamp ordering */
int desc_time_order = 0;

/* number of rows to fetch from result */
int ul_fetch_rows = 2000;
int ul_hash_size = 9;

/* flag */
unsigned int nat_bflag = (unsigned int)-1;
unsigned int init_flag = 0;

db_con_t* ul_dbh = 0; /* Database connection handle */
db_func_t ul_dbf;



/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"ul_register_udomain",   (cmd_function)register_udomain,   1, 0, 0},
	{"ul_insert_urecord",     (cmd_function)insert_urecord,     1, 0, 0},
	{"ul_delete_urecord",     (cmd_function)delete_urecord,     1, 0, 0},
	{"ul_get_urecord",        (cmd_function)get_urecord,        1, 0, 0},
	{"ul_lock_udomain",       (cmd_function)lock_udomain,       1, 0, 0},
	{"ul_unlock_udomain",     (cmd_function)unlock_udomain,     1, 0, 0},
	{"ul_release_urecord",    (cmd_function)release_urecord,    1, 0, 0},
	{"ul_insert_ucontact",    (cmd_function)insert_ucontact,    1, 0, 0},
	{"ul_delete_ucontact",    (cmd_function)delete_ucontact,    1, 0, 0},
	{"ul_get_ucontact",       (cmd_function)get_ucontact,       1, 0, 0},
	{"ul_get_all_ucontacts",  (cmd_function)get_all_ucontacts,  1, 0, 0},
	{"ul_update_ucontact",    (cmd_function)update_ucontact,    1, 0, 0},
	{"ul_register_watcher",   (cmd_function)register_watcher,   1, 0, 0},
	{"ul_unregister_watcher", (cmd_function)unregister_watcher, 1, 0, 0},
	{"ul_bind_usrloc",        (cmd_function)bind_usrloc,        1, 0, 0},
	{"ul_register_ulcb",      (cmd_function)register_ulcb,      1, 0, 0},
	{"ul_get_num_users",      (cmd_function)get_number_of_users,1, 0, 0},
	{0, 0, 0, 0, 0}
};


/* 
 * Exported parameters 
 */
static param_export_t params[] = {
	{"user_column",       STR_PARAM, &user_col.s      },
	{"domain_column",     STR_PARAM, &domain_col.s    },
	{"contact_column",    STR_PARAM, &contact_col.s   },
	{"expires_column",    STR_PARAM, &expires_col.s   },
	{"q_column",          STR_PARAM, &q_col.s         },
	{"callid_column",     STR_PARAM, &callid_col.s    },
	{"cseq_column",       STR_PARAM, &cseq_col.s      },
	{"flags_column",      STR_PARAM, &flags_col.s     },
	{"cflags_column",     STR_PARAM, &cflags_col.s    },
	{"db_url",            STR_PARAM, &db_url.s        },
	{"timer_interval",    INT_PARAM, &timer_interval  },
	{"db_mode",           INT_PARAM, &db_mode         },
	{"use_domain",        INT_PARAM, &use_domain      },
	{"desc_time_order",   INT_PARAM, &desc_time_order },
	{"user_agent_column", STR_PARAM, &user_agent_col.s},
	{"received_column",   STR_PARAM, &received_col.s  },
	{"path_column",       STR_PARAM, &path_col.s      },
	{"socket_column",     STR_PARAM, &sock_col.s      },
	{"methods_column",    STR_PARAM, &methods_col.s   },
	{"matching_mode",     INT_PARAM, &matching_mode   },
	{"cseq_delay",        INT_PARAM, &cseq_delay      },
	{"fetch_rows",        INT_PARAM, &ul_fetch_rows   },
	{"hash_size",         INT_PARAM, &ul_hash_size    },
	{"nat_bflag",         INT_PARAM, &nat_bflag       },
	{0, 0, 0}
};


stat_export_t mod_stats[] = {
	{"registered_users" ,  STAT_IS_FUNC, (stat_var**)get_number_of_users  },
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ MI_USRLOC_RM,           mi_usrloc_rm_aor,       0,                 0,
				mi_child_init },
	{ MI_USRLOC_RM_CONTACT,   mi_usrloc_rm_contact,   0,                 0,
				mi_child_init },
	{ MI_USRLOC_DUMP,         mi_usrloc_dump,         0,                 0,
				0             },
	{ MI_USRLOC_FLUSH,        mi_usrloc_flush,        MI_NO_INPUT_FLAG,  0,
				mi_child_init },
	{ MI_USRLOC_ADD,          mi_usrloc_add,          0,                 0,
				mi_child_init },
	{ MI_USRLOC_SHOW_CONTACT, mi_usrloc_show_contact, 0,                 0,
				mi_child_init },
	{ 0, 0, 0, 0, 0}
};


struct module_exports exports = {
	"usrloc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Export parameters */
	mod_stats,  /* exported statistics */
	mi_cmds,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	mod_init,   /* Module initialization function */
	0,          /* Response function */
	destroy,    /* Destroy function */
	child_init  /* Child initialization function */
};


/*
 * Module initialization function
 */
static int mod_init(void)
{
	DBG("usrloc - initializing\n");

	/* Compute the lengths of string parameters */
	user_col.len = strlen(user_col.s);
	domain_col.len = strlen(domain_col.s);
	contact_col.len = strlen(contact_col.s);
	expires_col.len = strlen(expires_col.s);
	q_col.len = strlen(q_col.s);
	callid_col.len = strlen(callid_col.s);
	cseq_col.len = strlen(cseq_col.s);
	flags_col.len = strlen(flags_col.s);
	cflags_col.len = strlen(cflags_col.s);
	user_agent_col.len = strlen(user_agent_col.s);
	received_col.len = strlen(received_col.s);
	path_col.len = strlen(path_col.s);
	sock_col.len = strlen(sock_col.s);
	methods_col.len = strlen(methods_col.s);
	last_mod_col.len = strlen(last_mod_col.s);
	db_url.len = strlen(db_url.s);

	if(ul_hash_size<=1)
		ul_hash_size = 512;
	else
		ul_hash_size = 1<<ul_hash_size;
	ul_locks_no = ul_hash_size;

	/* check matching mode */
	switch (matching_mode) {
		case CONTACT_ONLY:
		case CONTACT_CALLID:
			break;
		default:
			LOG(L_ERR,"ERROR:usrloc:mod_init: invalid matching mode %d\n",
				matching_mode);
	}

	if(ul_init_locks()!=0)
	{
		LOG(L_ERR, "ERROR: usrloc: locks array initialization failed\n");
		return -1;
	}

	/* Register cache timer */
	register_timer( timer, 0, timer_interval);

	if (init_ul_unixsock() < 0) {
		LOG(L_ERR, "ERROR: usrloc/unixsock initialization failed\n");
		return -1;
	}

	/* init the callbacks list */
	if ( init_ulcb_list() < 0) {
		LOG(L_ERR, "ERROR: usrloc/callbacks initialization failed\n");
		return -1;
	}

	/* Shall we use database ? */
	if (db_mode != NO_DB) { /* Yes */
		if (bind_dbmod(db_url.s, &ul_dbf) < 0) { /* Find database module */
			LOG(L_ERR, "ERROR: mod_init(): Can't bind database module\n");
			return -1;
		}
		if (!DB_CAPABILITY(ul_dbf, DB_CAP_ALL)) {
			LOG(L_ERR, "usrloc:mod_init: Database module does not implement"
						" all functions needed by the module\n");
			return -1;
		}
		if(ul_fetch_rows<=0) {
			LOG(L_ERR, "usrloc:mod_init: invalid fetch_rows number '%d'\n",
					ul_fetch_rows);
			return -1;
		}
	}

	if (nat_bflag==(unsigned int)-1) {
		nat_bflag = 0;
	} else if ( nat_bflag>=8*sizeof(nat_bflag) ) {
		LOG(L_ERR,"ERROR:usrloc:mod_init: bflag index (%d) too big!\n",
			nat_bflag);
		return -1;
	} else {
		nat_bflag = 1<<nat_bflag;
	}

	init_flag = 1;

	return 0;
}


static int child_init(int _rank)
{
	dlist_t* ptr;

	/* Shall we use database ? */
	if (db_mode != NO_DB) { /* Yes */
		ul_dbh = ul_dbf.init(db_url.s); /* Get a new database connection */
		if (!ul_dbh) {
			LOG(L_ERR, "ERROR:ul:child_init(%d): "
					"Error while connecting database\n", _rank);
			return -1;
		}
		/* _rank==1 is used even when fork is disabled */
		if (_rank==1 && db_mode!= DB_ONLY) {
			/* if cache is used, populate domains from DB */
			for( ptr=root ; ptr ; ptr=ptr->next) {
				if (preload_udomain(ul_dbh, ptr->d) < 0) {
					LOG(L_ERR,
						"ERROR:ul:child_init(%d): Error while preloading "
						"domain '%.*s'\n", _rank, ptr->name.len,
						ZSW(ptr->name.s));
					return -1;
				}
			}
		}
	}

	return 0;
}


/* */
static int mi_child_init()
{
	static int done = 0;

	if (done)
		return 0;

	if (db_mode != NO_DB) {
		ul_dbh = ul_dbf.init(db_url.s);
		if (!ul_dbh) {
			LOG(L_ERR, "ERROR:ul:mi_child_init: "
					"Error while connecting database\n");
			return -1;
		}
	}
	done = 1;

	return 0;
}


/*
 * Module destroy function
 */
static void destroy(void)
{
	/* Parent only, synchronize the world
	* and then nuke it */
	if (is_main) {
		if (ul_dbh && synchronize_all_udomains() != 0) {
			LOG(L_ERR, "timer(): Error while flushing cache\n");
		}
		free_all_udomains();
		ul_destroy_locks();
	}
	
	/* All processes close database connection */
	if (ul_dbh) ul_dbf.close(ul_dbh);

	/* free callbacks list */
	destroy_ulcb_list();
}


/*
 * Timer handler
 */
static void timer(unsigned int ticks, void* param)
{
	if (synchronize_all_udomains() != 0) {
		LOG(L_ERR, "timer(): Error while synchronizing cache\n");
	}
}
