/*
 * Accounting module
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
#include "../tm/tm_load.h"

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

static void acc_onreply( struct cell* t,  struct sip_msg *msg );
static void acc_onack( struct cell* t,  struct sip_msg *msg );


struct module_exports exports= {
	"acc",

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

	mod_init, 	/* initialization module */
	0,			/* response function */
	0,			/* destroy function */
	0,			/* oncancel function */
	0			/* per-child init function */
};



static int mod_init( void )
{

	load_tm_f	load_tm;

	fprintf( stderr, "acc - initializing\n");

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", 1))) {
		LOG(L_ERR, "ERROR: acc: mod_init: can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) return -1;

	/* register callbacks */
	if (tmb.register_tmcb( TMCB_REPLY, acc_onreply ) <= 0) 
		return -1;
	if (tmb.register_tmcb( TMCB_E2EACK, acc_onack ) <=0 )
		return -1;

	return 0;
}


static void acc_onreply( struct cell* t, struct sip_msg *msg ) 
{

	unsigned int status_code;
	struct sip_msg *rq;

	status_code =  msg->REPLY_STATUS;
	rq = t->uas.request;

	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag )==-1) return;
	/* early media is reported only if explicitely demanded, 
	   other provisional responses are always dropped  */
	if (status_code < 200 && ! (early_media && status_code==183)) 
		return;
	/* negative transactions reported only if explicitely demanded */
	if (!failed_transactions && msg->REPLY_STATUS >=300) return;

	/* anything else, i.e., 2xx, always reported */
	acc_reply_report(t, msg);
}


static void acc_onack( struct cell* t , struct sip_msg *msg )
{
	struct sip_msg *rq;

	rq = t->uas.request;
	/* only for those guys who insist on seeing ACKs as well */
	if (!report_ack) return;
	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag )==-1) return;

	acc_ack_report(t, msg);
}

