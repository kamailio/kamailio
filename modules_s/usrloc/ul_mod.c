/*
 * $Id$
 *
 * Usrloc module interface
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
 *
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
 * 2005-02-25 incoming socket is saved in ucontact record (bogdan)
 */

#include <stdio.h>
#include "ul_mod.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../timer.h"     /* register_timer */
#include "../../globals.h"   /* is_main */
#include "dlist.h"           /* register_udomain */
#include "udomain.h"         /* {insert,delete,get,release}_urecord */
#include "urecord.h"         /* {insert,delete,get}_ucontact */
#include "ucontact.h"        /* update_ucontact */
#include "ul_callback.h"
#include "notify.h"
#include "ul_rpc.h"
#include "usrloc.h"
#include "reg_avps.h"

MODULE_VERSION

#define UID_COL        "uid"
#define CONTACT_COL    "contact"
#define EXPIRES_COL    "expires"
#define Q_COL          "q"
#define CALLID_COL     "callid"
#define CSEQ_COL       "cseq"
#define METHOD_COL     "method"
#define STATE_COL      "state"
#define FLAGS_COL      "flags"
#define USER_AGENT_COL "user_agent"
#define RECEIVED_COL   "received"
#define INSTANCE_COL   "instance"
#define AOR_COL        "aor"

static int mod_init(void);                          /* Module initialization function */
static void destroy(void);                          /* Module destroy function */
static void timer(unsigned int ticks, void* param); /* Timer handler */
static int child_init(int rank);                    /* Per-child init function */

extern int bind_usrloc(usrloc_api_t* api);

/*
 * Module parameters and their default values
 */
str uid_col         = STR_STATIC_INIT(UID_COL);        /* Name of column containing usernames */
str contact_col     = STR_STATIC_INIT(CONTACT_COL);    /* Name of column containing contact addresses */
str expires_col     = STR_STATIC_INIT(EXPIRES_COL);    /* Name of column containing expires values */
str q_col           = STR_STATIC_INIT(Q_COL);          /* Name of column containing q values */
str callid_col      = STR_STATIC_INIT(CALLID_COL);     /* Name of column containing callid string */
str cseq_col        = STR_STATIC_INIT(CSEQ_COL);       /* Name of column containing cseq values */
str method_col      = STR_STATIC_INIT(METHOD_COL);     /* Name of column containing supported method */
str state_col       = STR_STATIC_INIT(STATE_COL);      /* Name of column containing contact state */
str flags_col       = STR_STATIC_INIT(FLAGS_COL);      /* Name of column containing flags */
str user_agent_col  = STR_STATIC_INIT(USER_AGENT_COL); /* Name of column containing user agent string */
str received_col    = STR_STATIC_INIT(RECEIVED_COL);   /* Name of column containing transport info of REGISTER */
str instance_col    = STR_STATIC_INIT(INSTANCE_COL);   /* Name of column containing sip-instance parameter */
str aor_col         = STR_STATIC_INIT(AOR_COL);        /* Name of column containing address of record */
str db_url          = STR_STATIC_INIT(DEFAULT_DB_URL); /* Database URL */

int timer_interval  = 60;             /* Timer interval in seconds */
int db_mode         = 0;              /* Database sync scheme: 0-no db, 1-write through, 2-write back */
int desc_time_order = 0;              /* By default do not enable timestamp ordering */


db_con_t* ul_dbh = 0; /* Database connection handle */
db_func_t ul_dbf;

static char *reg_avp_flag_name = NULL;

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
	{"ul_get_ucontact_by_inst", (cmd_function)get_ucontact_by_instance, 1, 0, 0},
	{"ul_get_all_ucontacts",  (cmd_function)get_all_ucontacts,  1, 0, 0},
	{"ul_update_ucontact",    (cmd_function)update_ucontact,    1, 0, 0},
	{"ul_register_watcher",   (cmd_function)register_watcher,   1, 0, 0},
	{"ul_unregister_watcher", (cmd_function)unregister_watcher, 1, 0, 0},
	{"ul_bind_usrloc",        (cmd_function)bind_usrloc,        1, 0, 0},
	{"ul_register_ulcb",      (cmd_function)register_ulcb,      1, 0, 0},
	{"read_reg_avps",         read_reg_avps,                    2, read_reg_avps_fixup, REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE }, 
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"uid_column",        PARAM_STR, &uid_col        },
	{"contact_column",    PARAM_STR, &contact_col    },
	{"expires_column",    PARAM_STR, &expires_col    },
	{"q_column",          PARAM_STR, &q_col          },
	{"callid_column",     PARAM_STR, &callid_col     },
	{"cseq_column",       PARAM_STR, &cseq_col       },
	{"method_column",     PARAM_STR, &method_col     },
	{"flags_column",      PARAM_STR, &flags_col      },
	{"db_url",            PARAM_STR, &db_url         },
	{"timer_interval",    PARAM_INT, &timer_interval },
	{"db_mode",           PARAM_INT, &db_mode        },
	{"desc_time_order",   PARAM_INT, &desc_time_order},
	{"user_agent_column", PARAM_STR, &user_agent_col },
	{"received_column",   PARAM_STR, &received_col   },
	{"instance_column",   PARAM_STR, &instance_col   },
	{"aor_column",        PARAM_STR, &aor_col        },
	{"reg_avp_table",     PARAM_STRING, &reg_avp_table  },
	{"reg_avp_column",       PARAM_STRING, &serialized_reg_avp_column  },	
	{"regavp_uid_column", PARAM_STRING, &regavp_uid_column  },
	{"regavp_contact_column", PARAM_STRING, &regavp_contact_column  },
	{"regavp_name_column",    PARAM_STRING, &regavp_name_column  },
	{"regavp_value_column",   PARAM_STRING, &regavp_value_column  },
	{"regavp_type_column",    PARAM_STRING, &regavp_type_column  },
	{"regavp_flags_column",   PARAM_STRING, &regavp_flags_column  },
	{"reg_avp_flag",          PARAM_STRING, &reg_avp_flag_name },
	{0, 0, 0}
};


struct module_exports exports = {
	"usrloc",
	cmds,       /* Exported functions */
	ul_rpc,     /* RPC methods */
	params,     /* Export parameters */
	mod_init,   /* Module initialization function */
	0,          /* Response function */
	destroy,    /* Destroy function */
	0,          /* OnCancel function */
	child_init  /* Child initialization function */
};


/*
 * Module initialization function
 */
static int mod_init(void)
{
	DBG("usrloc - initializing\n");

	if ((db_mode < 0) || (db_mode >= UL_DB_MAX)) {
	    ERR("Invalid database mode '%d'\n", db_mode);
	    return -1;
	}

	     /* Register cache timer */
	register_timer(timer, 0, timer_interval);

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
	}

	set_reg_avpflag_name(reg_avp_flag_name);

	return 0;
}


static int child_init(int _rank)
{
	if (_rank==PROC_MAIN || _rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main or tcp_main processes */
	     /* Shall we use database ? */
	if ( db_mode != NO_DB) { /* Yes */
		ul_dbh = ul_dbf.init(db_url.s); /* Get a new database connection */
		if (!ul_dbh) {
			LOG(L_ERR, "ERROR: child_init(%d): "
			    "Error while connecting database\n", _rank);
			return -1;
		}
	}

	return 0;
}


/*
 * Module destroy function
 */
static void destroy(void)
{
	/* Parent only, synchronize the world
	* and then nuke it */
	if (is_main && ul_dbh) {
		if (synchronize_all_udomains() != 0) {
			LOG(L_ERR, "timer(): Error while flushing cache\n");
		}
		free_all_udomains();
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
