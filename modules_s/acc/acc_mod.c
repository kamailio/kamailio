/*
 * Accounting module
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
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
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
static void destroy(void);
static int child_init(int rank);

db_con_t* db_handle;   /* Database connection handle */

/* ----- Parameter variables ----------- */

/* name of user id (==digest uid) column */
char *uid_column="uid";

#ifdef SQL_ACC

     /* Database url */
char *db_url;

     /* name of database table, default=="acc" */
char *db_table_acc="acc";

     /* names of columns in table acc*/
char* acc_sip_from_col      = "sip_from";
char* acc_sip_to_col        = "sip_to";
char* acc_sip_status_col    = "sip_status";
char* acc_sip_method_col    = "sip_method";
char* acc_i_uri_col         = "i_uri";
char* acc_o_uri_col         = "o_uri";
char* acc_sip_callid_col    = "sip_callid";
char* acc_user_col          = "user";
char* acc_time_col          = "time";

     /* name of missed calls table, default=="missed_calls" */
char *db_table_mc="missed_calls";

     /* names of columns in table missed calls*/
char* mc_sip_from_col      = "sip_from";
char* mc_sip_to_col        = "sip_to";
char* mc_sip_status_col    = "sip_status";
char* mc_sip_method_col    = "sip_method";
char* mc_i_uri_col         = "i_uri";
char* mc_o_uri_col         = "o_uri";
char* mc_sip_callid_col    = "sip_callid";
char* mc_user_col          = "user";
char* mc_time_col          = "time";

#endif

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

/* log to syslog too*/
int usesyslog = 1;

/* ------------- Callback handlers --------------- */

static void acc_onreply( struct cell* t,  struct sip_msg *msg,
	int code, void *param );
static void acc_onack( struct cell* t,  struct sip_msg *msg,
	int code, void *param );
static void acc_onreq( struct cell* t, struct sip_msg *msg,
	int code, void *param ) ;
static void on_missed(struct cell *t, struct sip_msg *reply,
	int code, void *param );


static cmd_export_t cmds[] = {
	{"acc_request", acc_request, 1, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[] = {
#ifdef SQL_ACC
        {"db_table_acc",          STR_PARAM, &db_table_acc         }, 
	{"db_table_missed_calls", STR_PARAM, &db_table_missed_calls},
	{"db_url",                STR_PARAM, &db_url               },
        {"acc_sip_from_column",   STR_PARAM, &acc_sip_from_column  },
        {"acc_sip_to_column",     STR_PARAM, &acc_sip_status_column},
        {"acc_sip_status_column", STR_PARAM, &acc_sip_status_column},
        {"acc_sip_method_column", STR_PARAM, &acc_sip_method_column},
        {"acc_i_uri_column",      STR_PARAM, &acc_i_uri_column     },
        {"acc_o_uri_column",      STR_PARAM, &acc_o_uri_column     },
        {"acc_sip_callid_column", STR_PARAM, &acc_sip_callid_column},
        {"acc_user_column",       STR_PARAM, &acc_user_column      },
        {"acc_time_column",       STR_PARAM, &acc_time_column      },
        {"mc_sip_from_column",    STR_PARAM, &mc_sip_from_column   },
        {"mc_sip_to_column",      STR_PARAM, &mc_sip_to_column     },
        {"mc_sip_status_column",  STR_PARAM, &mc_sip_status_column },
        {"mc_sip_method_column",  STR_PARAM, &mc_sip_method_column },
        {"mc_i_uri_column",       STR_PARAM, &mc_i_uri_column      },
        {"mc_o_uri_column",       STR_PARAM, &mc_o_uri_column      },
        {"mc_sip_callid_column",  STR_PARAM, &mc_sip_callid_column },
        {"mc_user_column",        STR_PARAM, &mc_user_column       },
        {"mc_time_column",        STR_PARAM, &mc_time_column       },
#endif
	{"uid_column",            STR_PARAM, &uid_column           },
	{"log_level",             INT_PARAM, &log_level            },
	{"early_media",           INT_PARAM, &early_media          },
	{"failed_transactions",   INT_PARAM, &failed_transactions  },
	{"acc_flag",              INT_PARAM, &acc_flag             },
	{"report_ack",            INT_PARAM, &report_ack           },
        {"missed_flag",           INT_PARAM, &missed_flag          },
	{"usesyslog",             INT_PARAM, &usesyslog            }
};


struct module_exports exports= {
	"acc",
	cmds,       /* exported functions */
	params,     /* exported params */
	mod_init,   /* initialization module */
	0,	    /* response function */
	destroy,    /* destroy function */
	0,	    /* oncancel function */
	child_init  /* per-child init function */
};



static int mod_init( void )
{

	load_tm_f	load_tm;

	fprintf( stderr, "acc - initializing\n");

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "ERROR: acc: mod_init: can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) return -1;

	/* register callbacks */

	/*  report on completed transactions */
	if (tmb.register_tmcb( TMCB_RESPONSE_OUT, acc_onreply, 0 /* empty param */ ) <= 0)
		return -1;
	/* account e2e acks if configured to do so */
	if (tmb.register_tmcb( TMCB_E2EACK_IN, acc_onack, 0 /* empty param */ ) <=0 )
		return -1;
	/* disable silent c-timer for registered calls */
	if (tmb.register_tmcb( TMCB_REQUEST_IN, acc_onreq, 0 /* empty param */ ) <=0 )
		return -1;
	/* report on missed calls */
	if (tmb.register_tmcb( TMCB_ON_FAILURE, on_missed, 0 /* empty param */ ) <=0 )
		return -1;

#ifdef SQL_ACC
    if (db_url == NULL) {
        LOG(L_ERR, "ERROR: acc:init_mod(): Use db_url parameter\n");
		return -1;
	}
	db_handle = db_init(db_url);
	if (!db_handle) {
        LOG(L_ERR, "acc:init_child(): Unable to connect database\n");
		return -1;
	} else {
		DBG("DEbug: mod_init(acc): db opened \n");
	}
#endif

	return 0;
}

static int child_init(int rank)
{
#ifdef SQL_ACC
	/* close parent's DB connection */
	db_close(db_handle);
	db_handle = db_init(db_url);
	if (!db_handle) {
        LOG(L_ERR, "acc:init_child(): Unable to connect database\n");
		return -1;
	}
#endif
	return 0;
}

static void destroy(void)
{
#ifdef SQL_ACC
    if (db_handle) db_close(db_handle);
#endif
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

static void on_missed(struct cell *t, struct sip_msg *reply,
	int code, void *param )
{
	struct sip_msg *rq;

	rq = t->uas.request;

	if (t->is_invite
			&& missed_flag
			&& isflagset( rq, missed_flag)==1
			&& code>=300 )
	{
		acc_missed_report( t, reply, code);
		/* don't come here again on next failed branch */
		resetflag(rq, missed_flag );
	}
}

static void acc_onreply( struct cell* t, struct sip_msg *reply,
	int code, void *param )
{

	struct sip_msg *rq;

	rq = t->uas.request;

	/* acc_onreply is bound to TMCB_REPLY which may be called
	   from _reply, like when FR hits; we should not miss this
	   event for missed calls either
	*/
	on_missed(t, reply, code, param );

	/* if acc enabled for flagged transaction, check if flag matches */
	if (acc_flag && isflagset( rq, acc_flag )==-1) return;
	/* early media is reported only if explicitely demanded,
	   other provisional responses are always dropped  */
	if (code < 200 && ! (early_media && code==183))
		return;
	/* negative transactions reported only if explicitely demanded */
	if (!failed_transactions && code >=300) return;

	/* anything else, i.e., 2xx, always reported */
	acc_reply_report(t, reply, code);
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

