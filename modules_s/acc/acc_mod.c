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
#include "../../msg_parser.h"

#include "acc_mod.h"
#include "tm_bind.h"

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

/* account only flagged transactions -- default==yes */
int flagged_only = 1;

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
		"flagged_only",
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
		&flagged_only,
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
	fprintf( stderr, "acc - initializing\n");
	if (bind_tm()==-1) return -1;

	if (!tmb.register_tmcb( TMCB_REPLY, acc_onreply )) 
		return -1;
	if (!tmb.register_tmcb( TMCB_E2EACK, acc_onack ))
		return -1;

	return 0;
}


static void acc_onreply( struct cell* t, struct sip_msg *msg ) 
{

	unsigned int status_code;

	status_code =  msg->REPLY_STATUS;
	/* we only report transactions labeled with "acc" */
	if (flagged_only && tmb.t_isflagset( msg, (char *) FL_ACC, NULL )==-1) return;
	/* early media is reported only if explicitely demanded */
	if (!early_media && status_code==183) return;
	/* other provisional replies are never reported */
	if (status_code < 200) return;
	/* negative transactions reported only if explicitely demanded */
	if (!failed_transactions && msg->REPLY_STATUS >=300) return;

	/* anything else, i.e., 2xx, always reported */
	acc_report(t, msg);
}


static void acc_onack( struct cell* t , struct sip_msg *msg )
{
	if (!report_ack) return;
	acc_report(t, msg);
}

