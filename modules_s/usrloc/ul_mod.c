/*
 * $Id$
 *
 * Usrloc module interface
 */

#include "ul_mod.h"
#include <stdio.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../timer.h"     /* register_timer */
#include "../../globals.h"   /* is_main */
#include "dlist.h"           /* register_udomain */
#include "udomain.h"         /* {insert,delete,get,release}_urecord */
#include "urecord.h"         /* {insert,delete,get}_ucontact */
#include "ucontact.h"        /* update_ucontact */
#include "ul_fifo.h"


static int mod_init(void);                          /* Module initialization function */
static void destroy(void);                          /* Module destroy function */
static void timer(unsigned int ticks, void* param); /* Timer handler */
static int child_init(int rank);                    /* Per-child init function */


/*
 * Module parameters and their default values
 */
char* user_col       = "user";                             /* Name of column containing usernames */
char* contact_col    = "contact";                          /* Name of column containing contact addresses */
char* expires_col    = "expires";                          /* Name of column containing expires values */
char* q_col          = "q";                                /* Name of column containing q values */
char* callid_col     = "callid";                           /* Name of column containing callid string */
char* cseq_col       = "cseq";                             /* Name of column containing cseq values */
char* method_col     = "method";                           /* Name of column containing supported method */
char* db_url         = "sql://ser:heslo@localhost/ser";    /* Database URL */
int   timer_interval = 60;                                 /* Timer interval in seconds */
int   db_mode        = 0;                                  /* Database sync scheme: 0-no db, 1-write through, 2-write back */


db_con_t* db; /* Database connection handle */


struct module_exports exports = {
	"usrloc",
	(char*[]) {
		"~ul_register_udomain", /* Create a new domain */

		"~ul_insert_urecord",   /* Insert record into a domain */
		"~ul_delete_urecord",   /* Delete record from a domain */
		"~ul_get_urecord",      /* Get record from a domain */
		"~ul_lock_udomain",     /* Lock domain */
		"~ul_unlock_udomain",   /* Unlock domain */

		"~ul_release_urecord",  /* Release record obtained using get_record */
		"~ul_insert_ucontact",  /* Insert a new contact into a record */
		"~ul_delete_ucontact",  /* Remove a contact from a record */
		"~ul_get_ucontact",     /* Return pointer to a contact */

		"~ul_update_ucontact"   /* Update a contact */
	},
	(cmd_function[]) {
		(cmd_function)register_udomain,

		(cmd_function)insert_urecord,
		(cmd_function)delete_urecord,
		(cmd_function)get_urecord,
		(cmd_function)lock_udomain,
		(cmd_function)unlock_udomain,
		
		(cmd_function)release_urecord,
		(cmd_function)insert_ucontact,
		(cmd_function)delete_ucontact,
		(cmd_function)get_ucontact,

		(cmd_function)update_ucontact
	},
	(int[]) {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
	},
	(fixup_function[]) {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	11,

	(char*[]) {
		"user_col",
		"contact_col",
		"expires_col",
		"q_col",
		"callid_col",
		"cseq_col",
		"method_col",
		"db_url",
		"timer_interval",
		"db_mode"
	},

	(modparam_t[]) {
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		INT_PARAM,
		INT_PARAM
	},

	(void*[]) {
		&user_col,
		&contact_col,
		&expires_col,
		&q_col,
		&callid_col,
		&cseq_col,
		&method_col,
		&db_url,
		&timer_interval,
		&db_mode
	},
	10,          /* Number of parameters */

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
	printf("usrloc module - initializing\n");

	     /* Register cache timer */
	register_timer(timer, NULL, timer_interval);

	     /* Initialize fifo interface */
	init_ul_fifo();

	     /* Shall we use database ? */
	if (db_mode != NO_DB) { /* Yes */
		if (bind_dbmod() < 0) { /* Find database module */
			LOG(L_ERR, "mod_init(): Can't bind database module\n");
			return -1;
		}
		
		     /* Open database connection in parent */
		db = db_init(db_url);
		if (!db) {
			LOG(L_ERR, "mod_init(): Error while connecting database\n");
			return -1;
		} else {
			LOG(L_INFO, "mod_init(): Database connection opened successfuly\n");
		}
	}

	return 0;
}


static int child_init(int _rank)
{
 	     /* Shall we use database ? */
	if (db_mode != NO_DB) { /* Yes */
		db_close(db); /* Close connection previously opened by parent */
		db = db_init(db_url); /* Initialize a new separate connection */
		if (!db) {
			LOG(L_ERR, "child_init(%d): Error while connecting database\n", _rank);
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
	      * and then nuke it
	      */
	if (is_main) {
		if (synchronize_all_udomains() != 0) {
			LOG(L_ERR, "timer(): Error while flushing cache\n");
		}
		free_all_udomains();
	}
	
	     /* All processes close database connection */
	if (db) db_close(db);
}


/*
 * Timer handler
 */
static void timer(unsigned int ticks, void* param)
{
	DBG("Running timer\n");
	if (synchronize_all_udomains() != 0) {
		LOG(L_ERR, "timer(): Error while synchronizing cache\n");
	}
	DBG("Timer done\n");
}
