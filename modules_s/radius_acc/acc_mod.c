/*
 * Radius Accounting module
 *
 * $Id$
 */

#include <stdio.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../tm/t_hooks.h"
#include "../tm/tm_load.h"
#include "../../parser/msg_parser.h"

#include "acc_mod.h"
#include "acc.h"
#include "../tm/tm_load.h"
#include <radiusclient.h>

/* Defines for radiusclient library */
#define RADLOG           "/home/ssi/work/client/radtest.txt"
#define CONFIG_FILE 	 "/home/ssi/work/client/radiusclient.conf"

struct tm_binds tmb;

static int mod_init( void );

/* ----- Parameter variables ----------- */

/* Flag if we are using a database or log facilities, default is log */       
int use_db = 0;

/* Database url */
char *db_url;

/* name of user id (==digest uid) column */
char *uid_column="uid";

/* name of database table, default=="acc" */
char *db_table="acc";

/* noisiness level logging facilities are used */
int log_level=L_NOTICE;

/* should early media replies (183) be logged ? default==no */
int early_media = 0;

/* should failed replies (>=3xx) be logged ? default==no */
int failed_transactions = 0;

/* flag which is needed for reporting: 0=any, 1 .. MAX_FLAG flag number */
int acc_flag = 1;

/* report e2e ACKs too */
int report_ack = 1;

/* ------------- Callback handlers --------------- */

static void rad_acc_onreply( struct cell* t,  struct sip_msg *msg );
static void rad_acc_onack( struct cell* t,  struct sip_msg *msg );


struct module_exports exports= {
	"radius_acc",

	/* exported functions */
	( char*[] ) { },
	( cmd_function[] ) { },
	( int[] ) { },
	( fixup_function[]) { },
	0, /* number of exported functions */

	/* exported variables */
	(char *[]) { /* variable names */
		"use_database",
		"db_table",
		"db_url",
		"uid_column",
		"log_level",
		"early_media",
		"failed_transactions",
		"acc_flag",
		"report_ack"
	},

	(modparam_t[]) { /* variable types */
		INT_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},

	(void *[]) { /* variable pointers */
		&use_db,
		&db_table,
		&db_url,
		&uid_column,
		&log_level,
		&early_media,
		&failed_transactions,
		&acc_flag,
		&report_ack
	},

	9,			/* number of variables */
	mod_init, 	        /* initialization module */
	0,			/* response function */
	0,			/* destroy function */
	0,			/* oncancel function */
	0			/* per-child init function */
};



/*
 * Initialize the module by registering the call-back
 * methods for reply and ack
 * returns -1 on failure
 */
static int mod_init( void )
{

	load_tm_f	load_tm;

	printf("Radius Accounting Init\n");
	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", 1))) {
		LOG(L_ERR, "ERROR: acc: mod_init: can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) return -1;

	/* register callbacks */
	if (tmb.register_tmcb( TMCB_REPLY, rad_acc_onreply ) <= 0) 
		return -1;
	if (tmb.register_tmcb( TMCB_E2EACK, rad_acc_onack ) <=0 )
		return -1;

	/* Read the configuration file */
  	if (rc_read_config(CONFIG_FILE) != 0) {
    	DBG("Error: acc: mod_init: opening configuration file \n");
    	return(-1);
  	}
  
	/* Read the dictionaray file from the configuration file loaded above */
	if (rc_read_dictionary(rc_conf_str("dictionary")) != 0) {
    	DBG("Error: acc: mod_init: opening dictionary file \n");
    	return(-1);
  	}

	/* Open log file */
  	rc_openlog(RADLOG);

	return 0;
}


/*
 * Function that gets called on reply
 * params: struct cell* t The callback structure
 *         struct sip_msg *msg The sip message.
 * returns: nothing
 */
static void rad_acc_onreply( struct cell* t, struct sip_msg *msg ) 
{

	unsigned int status_code;
	struct sip_msg *rq;
	int result;
	
	status_code =  msg->REPLY_STATUS;
	rq = t->uas.request;
	result = 0;
	
	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag ) == -1) return;
	/* early media is reported only if explicitely demanded, 
	   other provisional responses are always dropped  */
	
	if (status_code < 200 && ! (early_media && status_code== 183)) 
		return;

	/* negative transactions reported only if explicitely demanded */
	if (!failed_transactions && msg->REPLY_STATUS >=300) return;

	/* anything else, i.e., 2xx, always reported */
	result = radius_log_reply(t, msg);

}

/*
 * Function that gets called on reply
 * params: struct cell* t The callback structure
 *         struct sip_msg *msg The sip message.
 * returns: nothing
 */
static void rad_acc_onack( struct cell* t , struct sip_msg *msg )
{
  	struct sip_msg *rq;
	rq = t->uas.request;

	if (!report_ack) return;
	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag )==-1) return;

	radius_log_ack(t, msg);
}









