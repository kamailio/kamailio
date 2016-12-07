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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#define SERVER_ID_COL  "server_id"

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
str server_id_col   = STR_STATIC_INIT(SERVER_ID_COL);  /* Name of column containing server id */
str db_url          = STR_STATIC_INIT(DEFAULT_DB_URL); /* Database URL */

int timer_interval  = 60;             /* Timer interval in seconds */
int db_mode         = 0;              /* Database sync scheme: 0-no db, 1-write through, 2-write back */
int desc_time_order = 0;              /* By default do not enable timestamp ordering */
int db_skip_delete  = 0;              /* Enable/disable contact deletion in database */


db_ctx_t* db = NULL;
db_cmd_t** del_contact = NULL;
db_cmd_t** ins_contact = NULL;
int cmd_n = 0, cur_cmd = 0;

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
	{"reg_avp_column",    PARAM_STRING, &avp_column  },	
	{"reg_avp_flag",      PARAM_STRING, &reg_avp_flag_name },
	{"db_skip_delete",    PARAM_INT, &db_skip_delete},
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

	set_reg_avpflag_name(reg_avp_flag_name);

	return 0;
}


static int build_db_cmds(void)
{
	db_fld_t del_contact_match[] = {
		{.name = uid_col.s, .type = DB_STR},
		{.name = contact_col.s, .type = DB_STR},
		{.name = NULL},
	};
	
	db_fld_t ins_contact_values[] = {
		{.name = uid_col.s,        .type = DB_STR},
		{.name = contact_col.s,    .type = DB_STR},
		{.name = expires_col.s,    .type = DB_DATETIME},
		{.name = q_col.s,          .type = DB_DOUBLE},
		{.name = callid_col.s,     .type = DB_STR},
		{.name = cseq_col.s,       .type = DB_INT},
		{.name = flags_col.s,      .type = DB_BITMAP},
		{.name = user_agent_col.s, .type = DB_STR},
		{.name = received_col.s,   .type = DB_STR},
		{.name = instance_col.s,   .type = DB_STR},
		{.name = aor_col.s,        .type = DB_STR},
		{.name = server_id_col.s,  .type = DB_INT},
		{.name = avp_column,       .type = DB_STR}, /* Must be the last element in the array */
		{.name = NULL},
	};
	
	dlist_t* ptr;
	int i;

	INFO("usrloc: build_db_cmds()\n");
	for(cmd_n = 0, ptr = root; ptr; cmd_n++, ptr = ptr->next);

	del_contact = pkg_malloc(cmd_n);
	if (del_contact == NULL) {
		ERR("No memory left\n");
		return -1;
	}
	memset(del_contact, '\0', sizeof(del_contact) * cmd_n);

	ins_contact = pkg_malloc(cmd_n);
	if (ins_contact == NULL) {
		ERR("No memory left\n");
		return -1;
	}
	memset(ins_contact, '\0', sizeof(ins_contact) * cmd_n);

	INFO("usrloc: building del_contact queries()\n");
	for(i = 0, ptr = root; ptr; ptr = ptr->next, i++) {
		del_contact[i] = db_cmd(DB_DEL, db, ptr->name.s, NULL, del_contact_match, NULL);
		if (del_contact[i] == NULL) return -1;
	}

	INFO("usrloc: building inst_contact queries()\n");
	for(i = 0, ptr = root; ptr; ptr = ptr->next, i++) {
		ins_contact[i] = db_cmd(DB_PUT, db, ptr->name.s, NULL, NULL, ins_contact_values);
		if (ins_contact[i] == NULL) return -1;
	}
	return 0;
}



static int child_init(int _rank)
{
	INFO("usrloc: child_init( rank: %d)\n", _rank);
	if (_rank==PROC_INIT || _rank==PROC_MAIN || _rank==PROC_TCP_MAIN) {
		INFO("usrloc: do nothing for the init, main or tcp_main processes\n");
		return 0; /* do nothing for the main or tcp_main processes */
	}
	
	INFO("usrloc: db_mode = %d\n", db_mode);
	     /* Shall we use database ? */
	if ( db_mode != NO_DB) { /* Yes */
		db = db_ctx("usrloc");
		if (db == NULL) {
			ERR("Error while initializing database layer\n");
			return -1;
		}

		if (db_add_db(db, db_url.s) < 0) return -1;
		if (db_connect(db) < 0) return -1;
		if (build_db_cmds() < 0) return -1;
	}
	INFO("usrloc: child_init( rank: %d), done OK\n", _rank);

	return 0;
}



/*
 * Module destroy function
 */
static void destroy(void)
{
	int i;

	/* Parent only, synchronize the world
	* and then nuke it */
	if (is_main) {
		if (db && synchronize_all_udomains() != 0) {
			LOG(L_ERR, "destroy(): Error while flushing cache\n");
		}
		free_all_udomains();
	}

	if (del_contact) {
		for(i = 0; i < cmd_n; i++) {
			if (del_contact[i]) db_cmd_free(del_contact[i]);
		}
		pkg_free(del_contact);
	}

	if (ins_contact) {
		for(i = 0; i < cmd_n; i++) {
			if (ins_contact[i]) db_cmd_free(ins_contact[i]);
		}
		pkg_free(ins_contact);
	}

	if (db) db_ctx_free(db);

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
