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
#include "dlist.h"           /* register_udomain */
#include "udomain.h"         /* {insert,delete,get,release}_urecord */
#include "urecord.h"         /* {insert,delete,get}_ucontact */
#include "ucontact.h"        /* update_ucontact */


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
char* db_url         = "sql://janakj:heslo@localhost/ser"; /* Database URL */
int   timer_interval = 60;                                 /* Timer interval in seconds */
int   write_through  = 0;                                  /* Enable or disable write-through cache scheme */
int   use_db         = 1;                                  /* Specifies whether to use database for persistent storage */


db_con_t* db; /* Database connection handle */


struct module_exports exports = {
	"usrloc",
	(char*[]) {
		"~ul_register_domain", /* Create a new domain */

		"~ul_insert_record",   /* Insert record into a domain */
		"~ul_delete_record",   /* Delete record from a domain */
		"~ul_get_record",      /* Get record from a domain */
		"~ul_release_record",  /* Release record obtained using get_record */

		"~ul_new_record",      /* Create new record structure */
		"~ul_free_record",     /* Release a record structure */
		"~ul_insert_contact",  /* Insert a new contact into a record */
		"~ul_delete_contact",  /* Remove a contact from a record */
		"~ul_get_contact",     /* Return pointer to a contact */

		"~ul_update_contact"   /* Update a contact */
	},
	(cmd_function[]) {
		(cmd_function)register_udomain,

		(cmd_function)insert_urecord,
		(cmd_function)delete_urecord,
		(cmd_function)get_urecord,
		(cmd_function)release_urecord,
		
		(cmd_function)new_urecord,
		(cmd_function)free_urecord,
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
		"write_through",
		"use_db"
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
		&write_through,
		&use_db
	},
	11,          /* Number of parameters */

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

	register_timer(timer, NULL, timer_interval);
	
	if (use_db) {
		if (bind_dbmod() < 0) {
			LOG(L_ERR, "mod_init(): Can't bind database module\n");
			return -1;
		}
		
		DBG("mod_init(): Opening database connection for parent\n");
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


static int child_init(int rank)
{
	if (use_db) {
		db = db_init(db_url);
		if (!db) {
			LOG(L_ERR, "child_init(%d): Error while connecting database\n", rank);
			return -1;
		} else {
			if (rank == 0) {
				     /* Preload_all_domains must be called
				      * after the whole config file is parsed and
				      * all domains are created, so we call it from
				      * the first child, because fork is called after
				      * the cfg file was parsed
				      */
				if (preload_all_udomains() != 0) {
					LOG(L_ERR, "Error while preloading domains\n");
					return -1;
				}				
			}
		}
	}
	return 0;
}


/*
 * Module destroy function
 */
static void destroy(void)
{
	free_all_udomains();
}


/*
 * Timer handler
 */
static void timer(unsigned int ticks, void* param)
{
	LOG(L_ERR, "Running timer\n");
	if (timer_handler() != 0) {
		LOG(L_ERR, "timer(): Error while running timer\n");
	}
	LOG(L_ERR, "Timer done\n");
}
