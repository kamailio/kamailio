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
#include "../tm/h_table.h"
#include "../../parser/msg_parser.h"

#include "acc_mod.h"
#include "acc.h"
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

/* flag which is needed for reporting missed calls */
int missed_flag = 2;

/* report e2e ACKs too */
int report_ack = 1;

/* ------------- Callback handlers --------------- */

static void acc_onreply( struct cell* t,  struct sip_msg *msg,
	int code, void *param );
static void acc_onack( struct cell* t,  struct sip_msg *msg,
	int code, void *param );
static void acc_onreq( struct cell* t, struct sip_msg *msg,
	int code, void *param ) ;


struct module_exports exports= {
	"acc",

	/* exported functions */
	( char*[] ) { "acc_request" },
	( cmd_function[] ) { acc_request },
	( int[] ) { 1 /* acc_missed */},
	( fixup_function[]) { 0 /* acc_missed */},
	1, /* number of exported functions */

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
		"report_ack",
		"missed_flag"
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
		&report_ack,
		&missed_flag
	},

	10,			/* number of variables */

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
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT))) {
		LOG(L_ERR, "ERROR: acc: mod_init: can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) return -1;

	/* register callbacks */
	if (tmb.register_tmcb( TMCB_REPLY, acc_onreply, 0 /* empty param */ ) <= 0) 
		return -1;
	if (tmb.register_tmcb( TMCB_E2EACK, acc_onack, 0 /* empty param */ ) <=0 )
		return -1;
	if (tmb.register_tmcb( TMCB_REQUEST_OUT, acc_onreq, 0 /* empty param */ ) <=0 )
		return -1;

	return 0;
}


static void acc_onreq( struct cell* t, struct sip_msg *msg,
	int code, void *param )
{
	/* disable C timer for accounted calls */
	if (isflagset( msg, acc_flag)==1 || 
				(t->is_invite && isflagset( msg, missed_flag))) {
#		ifdef EXTRA_DEBUG
		DBG("DEBUG: noisy_timer set for accounting\n");
#		endif
		t->noisy_ctimer=1;
	}
}

static void acc_onreply( struct cell* t, struct sip_msg *reply,
	int code, void *param ) 
{

	struct sip_msg *rq;

	rq = t->uas.request;

	if (t->is_invite && missed_flag && isflagset( rq, missed_flag)==1
		&& ((code>=400 && code<500) || code>=600))
		acc_missed_report( t, reply, code);


	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag )==-1) return;
	/* early media is reported only if explicitely demanded, 
	   other provisional responses are always dropped  */
	if (code < 200 && ! (early_media && code==183)) 
		return;
	/* negative transactions reported only if explicitely demanded */
	if (!failed_transactions && code >=300) return;

	/* anything else, i.e., 2xx, always reported */
	acc_reply_report(t, reply);
}


static void acc_onack( struct cell* t , struct sip_msg *ack,
	int code, void *param )
{
	struct sip_msg *rq;

	rq = t->uas.request;
	/* only for those guys who insist on seeing ACKs as well */
	if (!report_ack) return;
	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag )==-1) return;

	acc_ack_report(t, ack);
}

