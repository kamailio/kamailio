/*
 * Radius Accounting module
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */
/* History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
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
#include <radiusclient.h>

/* Defines for radiusclient library */
#define CONFIG_FILE 	 "etc/radiusclient.conf"

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

static void rad_acc_onreply( struct cell* t,  struct sip_msg *msg,
								int code, void *param );

static void rad_acc_onack( struct cell* t,  struct sip_msg *msg,
								int code, void *param );

static void rad_acc_onreq( struct cell* t, struct sip_msg *msg,
								int code, void *param ) ;


static  cmd_exports_t cmds[]={
	{"rad_acc_request", rad_acc_request, 1, 0 },
	{0,0,0,0}
};

static param_exports_t params[]={
	{"use_database",        INT_PARAM,   &use_db      },
	{"db_table",            STR_PARAM,   &db_table    },
	{"db_url",              STR_PARAM,   &db_url      },
	{"uid_column",          STR_PARAM,   &uid_column  },
	{"log_level",           INT_PARAM,   &log_level   },
	{"early_media",         INT_PARAM,   &early_media },
	{"failed_transactions", INT_PARAM,   &failed_transactions },
	{"acc_flag",            INT_PARAM,   &acc_flag    },
	{"report_ack",          INT_PARAM,   &report_ack  },
	{"missed_flag",         INT_PARAM,   &missed_flag },
	{0,0,0}
};


struct module_exports exports= {
	"radius_acc",
	
	cmds,
	/* exported variables */
	params,
	
	mod_init, 	/* initialization module */
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

	fprintf( stderr, "radius_acc - initializing\n");

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT))) {
		LOG(L_ERR, "ERROR: radius_acc: mod_init: can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) return -1;

	/* register callbacks */
	if (tmb.register_tmcb( TMCB_REPLY_OUT, rad_acc_onreply, 
								0 /* empty param */ ) <= 0) 
		return -1;
	if (tmb.register_tmcb( TMCB_E2EACK, rad_acc_onack, 
								0 /* empty param */ ) <=0 )
		return -1;
	if (tmb.register_tmcb( TMCB_REQUEST_OUT, rad_acc_onreq, 
								0 /* empty param */ ) <=0 )
		return -1;

	/* Read the configuration file */
  	if (rc_read_config(CONFIG_FILE) != 0) {
    	LOG(L_ERR,"Error:radius_acc: mod_init: opening configuration file \n");
    	return(-1);
  	}
  
	/* Read the dictionaray file from the configuration file loaded above */
	if (rc_read_dictionary(rc_conf_str("dictionary")) != 0) {
    	DBG("Error: acc: mod_init: opening dictionary file \n");
    	return(-1);
  	}
	
	return 0;
}



static void rad_acc_onreq( struct cell* t, struct sip_msg *msg, int code, 
								void *param )
{
	/* disable C timer for accounted calls */
	if (isflagset( msg, acc_flag) == 1 || 
				(t->is_invite && isflagset( msg, missed_flag))) {
#		ifdef EXTRA_DEBUG
		DBG("DEBUG: noisy_timer set for accounting\n");
#		endif
		t->noisy_ctimer = 1;
	}
}


/*
 * Function that gets called on reply
 * params: struct cell* t The callback structure
 *         struct sip_msg *msg The sip message.
 *         int code The status code
 *         void* param Passed parameter
 * returns: nothing
 */
static void rad_acc_onreply( struct cell* t, struct sip_msg *reply, int code, 
								void *param ) 
{

	struct sip_msg *rq;

	rq = t->uas.request;

	if (t->is_invite && missed_flag && isflagset( rq, missed_flag)==1
			&& ((code>=400 && code<500) || code>=600))
				rad_acc_missed_report( t, reply, code);


	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag )==-1) return;
	/* early media is reported only if explicitely demanded, 
	   other provisional responses are always dropped  */
	if (code < 200 && ! (early_media && code==183)) 
		return;
	/* negative transactions reported only if explicitely demanded */
	if (!failed_transactions && code >=300) return;

	/* anything else, i.e., 2xx, always reported */
	radius_log_reply(t, reply);

}

/*
 * Function that gets called on reply
 * params: struct cell* t The callback structure
 *         struct sip_msg *msg The sip message.
 *         int code The status code
 *         void* param Passed parameter
 * returns: nothing
 */
static void rad_acc_onack( struct cell* t , struct sip_msg *ack, int code, 
								void *param )
{
  	struct sip_msg *rq;
	rq = t->uas.request;

	if (!report_ack) return;
	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag )==-1) return;

	radius_log_ack(t, ack);
}









