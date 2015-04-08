/*
 * $Id$
 * 
 * Accounting module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2003-03-06: aligned to change in callback names (jiri)
 * 2003-03-06: fixed improper sql connection, now from 
 * 	           child_init (jiri)
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-04-04  grand acc cleanup (jiri)
 * 2003-04-06: Opens database connection in child_init only (janakj)
 * 2003-04-24  parameter validation (0 t->uas.request) added (jiri)
 * 2003-11-04  multidomain support for mysql introduced (jiri)
 * 2003-12-04  global TM callbacks switched to per transaction callbacks
 *             (bogdan)
 * 2004-06-06  db cleanup: static db_url, calls to acc_db_{bind,init,close)
 *             (andrei)
 * 2005-05-30  acc_extra patch commited (ramona)
 * 2005-06-28  multi leg call support added (bogdan)
 * 2006-01-13  detect_direction (for sequential requests) added (bogdan)
 * 2006-09-08  flexible multi leg accounting support added (bogdan)
 * 2006-09-19  final stage of a masive re-structuring and cleanup (bogdan)
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Core module interface
 *
 * - Module: \ref acc
 */

/*! \defgroup acc ACC :: The Kamailio accounting Module
 *            
 * The ACC module is used to account transactions information to
 *  different backends like syslog, SQL, RADIUS and DIAMETER (beta
 *  version).
 *            
 */ 

#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../modules/tm/tm_load.h"
#include "../../str.h"
#include "../rr/api.h"
#include "acc.h"
#include "acc_api.h"
#include "acc_mod.h"
#include "acc_extra.h"
#include "acc_logic.h"
#include "acc_cdr.h"

#ifdef RAD_ACC
#include "../../lib/kcore/radius.h"
#endif

#ifdef DIAM_ACC
#include "diam_dict.h"
#include "diam_tcp.h"
#endif

MODULE_VERSION

struct tm_binds tmb;
struct rr_binds rrb;

static int mod_init(void);
static void destroy(void);
static int child_init(int rank);


/* ----- General purpose variables ----------- */

/* what would you like to report on */

int early_media = 0;		/*!< should early media replies (183) be logged ? default==no */
int report_cancels = 0;		/*!< would you like us to report CANCELs from upstream too? */
int report_ack = 0;		/*!< report e2e ACKs too */
int detect_direction = 0;	/*!< detect and correct direction in the sequential requests */
int failed_transaction_flag = -1; /*!< should failed replies (>=3xx) be logged ? default==no */
static char *failed_filter_str = 0;  /* by default, do not filter logging of
					failed transactions */
unsigned short failed_filter[MAX_FAILED_FILTER_COUNT + 1];
static char* leg_info_str = 0;	/*!< multi call-leg support */
struct acc_extra *leg_info = 0;
int acc_prepare_flag = -1; /*!< should the request be prepared for later acc */
char *acc_time_format = "%Y-%m-%d %H:%M:%S";
int reason_from_hf = 0; /*!< assign reason from reason hf if present */

/* ----- time mode variables ------- */
/*! \name AccTimeModeVariables  Time Mode Variables */
/*@{*/

int acc_time_mode  = 0;
str acc_time_attr  = str_init("time_attr");
str acc_time_exten  = str_init("time_exten");
int _acc_clone_msg  = 1;
int _acc_cdr_on_failed = 1;

/*@}*/

/* ----- SYSLOG acc variables ----------- */
/*! \name AccSyslogVariables  Syslog Variables */
/*@{*/

int log_flag = -1;
int log_missed_flag = -1;
int log_level = L_NOTICE;	/*!< Syslog: noisiness level logging facilities are used */
int log_facility = LOG_DAEMON;	/*!< Syslog: log facility that is used */
static char * log_facility_str = 0; /*!< Syslog: log facility that is used */
static char *log_extra_str = 0; /*!< Syslog: log extra variables */
struct acc_extra *log_extra = 0; /*!< Log extra attributes */

/*@}*/

/* ----- CDR generation variables ------- */
/*! \name AccCdrVariables  CDR Variables */
/*@{*/

int cdr_enable  = 0;
int cdr_log_enable  = 1;
int cdr_start_on_confirmed = 0;
int cdr_expired_dlg_enable = 0;
static char* cdr_facility_str = 0;
static char* cdr_log_extra_str = 0;

str cdr_start_str = str_init("start_time");
str cdr_end_str = str_init("end_time");
str cdr_duration_str = str_init("duration");
/* name for db table to store dialog-based cdrs */
str acc_cdrs_table = str_init("");

/*@}*/

/* ----- RADIUS acc variables ----------- */
/*! \name AccRadiusVariables  Radius Variables */     
/*@{*/

#ifdef RAD_ACC
static char *radius_config = 0;
int radius_flag = -1;
int radius_missed_flag = -1;
static int service_type = -1;
void *rh;
/* rad extra variables */
static char *rad_extra_str = 0;
struct acc_extra *rad_extra = 0;
#endif
/*@}*/


/* ----- DIAMETER acc variables ----------- */

/*! \name AccDiamaterVariables  Radius Variables */     
/*@{*/
#ifdef DIAM_ACC
int diameter_flag = -1;
int diameter_missed_flag = -1;
static char *dia_extra_str = 0;		/*!< diameter extra variables */
struct acc_extra *dia_extra = 0;
rd_buf_t *rb;				/*!< buffer used to read from TCP connection*/
char* diameter_client_host="localhost";
int diameter_client_port=3000;
#endif

/*@}*/

/* ----- SQL acc variables ----------- */
/*! \name AccSQLVariables  Radius Variables */     
/*@{*/

#ifdef SQL_ACC
int db_flag = -1;
int db_missed_flag = -1;
static char *db_extra_str = 0;		/*!< db extra variables */
struct acc_extra *db_extra = 0;
static str db_url = {NULL, 0};		/*!< Database url */
str db_table_acc = str_init("acc");	/*!< name of database tables */
void *db_table_acc_data = NULL;
str db_table_mc = str_init("missed_calls");
void *db_table_mc_data = NULL;
/* names of columns in tables acc/missed calls*/
str acc_method_col     = str_init("method");
str acc_fromtag_col    = str_init("from_tag");
str acc_totag_col      = str_init("to_tag");
str acc_callid_col     = str_init("callid");
str acc_sipcode_col    = str_init("sip_code");
str acc_sipreason_col  = str_init("sip_reason");
str acc_time_col       = str_init("time");
int acc_db_insert_mode = 0;
#endif

/*@}*/

static int bind_acc(acc_api_t* api);
static int acc_register_engine(acc_engine_t *eng);
static int acc_init_engines(void);
static acc_engine_t *_acc_engines=NULL;
static int _acc_module_initialized = 0;

/* ------------- fixup function --------------- */
static int acc_fixup(void** param, int param_no);
static int free_acc_fixup(void** param, int param_no);


static cmd_export_t cmds[] = {
	{"acc_log_request", (cmd_function)w_acc_log_request, 1,
		acc_fixup, free_acc_fixup,
		ANY_ROUTE},
#ifdef SQL_ACC
	{"acc_db_request",  (cmd_function)w_acc_db_request,  2,
		acc_fixup, free_acc_fixup,
		ANY_ROUTE},
#endif
#ifdef RAD_ACC
	{"acc_rad_request", (cmd_function)w_acc_rad_request, 1,
		acc_fixup, free_acc_fixup,
		ANY_ROUTE},
#endif
#ifdef DIAM_ACC
	{"acc_diam_request",(cmd_function)w_acc_diam_request,1,
		acc_fixup, free_acc_fixup,
		ANY_ROUTE},
#endif
	{"bind_acc",    (cmd_function)bind_acc, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};



static param_export_t params[] = {
	{"early_media",             INT_PARAM, &early_media             },
	{"failed_transaction_flag", INT_PARAM, &failed_transaction_flag },
	{"failed_filter",           PARAM_STRING, &failed_filter_str       },
	{"report_ack",              INT_PARAM, &report_ack              },
	{"report_cancels",          INT_PARAM, &report_cancels          },
	{"multi_leg_info",          PARAM_STRING, &leg_info_str            },
	{"detect_direction",        INT_PARAM, &detect_direction        },
	{"acc_prepare_flag",        INT_PARAM, &acc_prepare_flag        },
	{"reason_from_hf",          INT_PARAM, &reason_from_hf          },
	/* syslog specific */
	{"log_flag",             INT_PARAM, &log_flag             },
	{"log_missed_flag",      INT_PARAM, &log_missed_flag      },
	{"log_level",            INT_PARAM, &log_level            },
	{"log_facility",         PARAM_STRING, &log_facility_str     },
	{"log_extra",            PARAM_STRING, &log_extra_str        },
	/* cdr specific */
	{"cdr_enable",           INT_PARAM, &cdr_enable                 },
	{"cdr_log_enable",         INT_PARAM, &cdr_log_enable           },
	{"cdr_start_on_confirmed", INT_PARAM, &cdr_start_on_confirmed   },
	{"cdr_facility",         PARAM_STRING, &cdr_facility_str           },
	{"cdr_extra",            PARAM_STRING, &cdr_log_extra_str          },
	{"cdr_start_id",	 PARAM_STR, &cdr_start_str		},
	{"cdr_end_id",		 PARAM_STR, &cdr_end_str		},
	{"cdr_duration_id",	 PARAM_STR, &cdr_duration_str	},
	{"cdr_expired_dlg_enable", INT_PARAM, &cdr_expired_dlg_enable   },
#ifdef RAD_ACC
	{"radius_config",        PARAM_STRING, &radius_config     },
	{"radius_flag",          INT_PARAM, &radius_flag          },
	{"radius_missed_flag",   INT_PARAM, &radius_missed_flag   },
	{"service_type",         INT_PARAM, &service_type         },
	{"radius_extra",         PARAM_STRING, &rad_extra_str     },
#endif
	/* DIAMETER specific */
#ifdef DIAM_ACC
	{"diameter_flag",        INT_PARAM, &diameter_flag        },
	{"diameter_missed_flag", INT_PARAM, &diameter_missed_flag },
	{"diameter_client_host", PARAM_STRING, &diameter_client_host },
	{"diameter_client_port", INT_PARAM, &diameter_client_port },
	{"diameter_extra",       PARAM_STRING, &dia_extra_str     },
#endif
	/* db-specific */
#ifdef SQL_ACC
	{"db_flag",              INT_PARAM, &db_flag            },
	{"db_missed_flag",       INT_PARAM, &db_missed_flag     },
	{"db_extra",             PARAM_STRING, &db_extra_str    },
	{"db_url",               PARAM_STR, &db_url             },
	{"db_table_acc",         PARAM_STR, &db_table_acc       },
	{"db_table_missed_calls",PARAM_STR, &db_table_mc        },
	{"acc_method_column",    PARAM_STR, &acc_method_col     },
	{"acc_from_tag_column",  PARAM_STR, &acc_fromtag_col    },
	{"acc_to_tag_column",    PARAM_STR, &acc_totag_col      },
	{"acc_callid_column",    PARAM_STR, &acc_callid_col     },
	{"acc_sip_code_column",  PARAM_STR, &acc_sipcode_col    },
	{"acc_sip_reason_column",PARAM_STR, &acc_sipreason_col  },
	{"acc_time_column",      PARAM_STR, &acc_time_col       },
	{"db_insert_mode",       INT_PARAM, &acc_db_insert_mode },
#endif
	/* time-mode-specific */
	{"time_mode",            INT_PARAM, &acc_time_mode        },
	{"time_attr",            PARAM_STR, &acc_time_attr        },
	{"time_exten",           PARAM_STR, &acc_time_exten       },
	{"cdrs_table",           PARAM_STR, &acc_cdrs_table       },
	{"time_format",          PARAM_STRING, &acc_time_format   },
	{"clone_msg",            PARAM_INT, &_acc_clone_msg       },
	{"cdr_on_failed",        PARAM_INT, &_acc_cdr_on_failed   },
	{0,0,0}
};


struct module_exports exports= {
	"acc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* exported functions */
	params,     /* exported params */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* initialization module */
	0,          /* response function */
	destroy,    /* destroy function */
	child_init  /* per-child init function */
};



/************************** FIXUP functions ****************************/


static int acc_fixup(void** param, int param_no)
{
	struct acc_param *accp;
	char *p;

	p = (char*)*param;
	if (p==0 || p[0]==0) {
		LM_ERR("first parameter is empty\n");
		return E_SCRIPT;
	}

	if (param_no == 1) {
		accp = (struct acc_param*)pkg_malloc(sizeof(struct acc_param));
		if (!accp) {
			LM_ERR("no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( accp, 0, sizeof(struct acc_param));
		accp->reason.s = p;
		accp->reason.len = strlen(p);
		if (strchr(p,PV_MARKER)!=NULL) { /* is a variable $xxxxx */
			if (pv_parse_format(&accp->reason, &accp->elem)<0)
			{
				LM_ERR("bad param 1; "
					"parse format error [%.*s]\n", accp->reason.len, accp->reason.s);
				return E_UNSPEC;
			}
		}
		else {
			if(acc_parse_code(p,accp)<0)
			{
				LM_ERR("bad param 1;"
					"parse code error\n");
				return E_UNSPEC;
			}
		}
		*param = (void*)accp;
#ifdef SQL_ACC
	} else if (param_no == 2) {
		/* only for db acc - the table name */
		if (db_url.s==0) {
			pkg_free(p);
			*param = 0;
		} else {
			return fixup_var_pve_str_12(param, 2);
		}
#endif
	}
	return 0;
}

static int free_acc_fixup(void** param, int param_no)
{
	if(*param)
	{
		pkg_free(*param);
		*param = 0;
	}
	return 0;
}



/************************** INTERFACE functions ****************************/


static int parse_failed_filter(char *s, unsigned short *failed_filter)
{
    unsigned int n;
    char *at;

    n = 0;

    while (1) {
	if (n >= MAX_FAILED_FILTER_COUNT) {
	    LM_ERR("too many elements in failed_filter\n");
	    return 0;
	}
	at = s;
	while ((*at >= '0') && (*at <= '9')) at++;
	if (at - s != 3) {
	    LM_ERR("respose code in failed_filter must have 3 digits\n");
	    return 0;
	}
	failed_filter[n] = (*s - '0') * 100 + (*(s + 1) - '0') * 10 +
	    (*(s + 2) - '0');
	if (failed_filter[n] < 300) {
	    LM_ERR("invalid respose code %u in failed_filter\n",
		   failed_filter[n]);
	    return 0;
	}
	LM_DBG("failed_filter %u = %u\n", n, failed_filter[n]);
	n++;
	failed_filter[n] = 0;
	s = at;
	if (*s == 0)
	    return 1;
	if (*s != ',') {
	    LM_ERR("response code is not followed by comma or end of string\n");
	    return 0;
	}
	s++;
    }
}

static int mod_init( void )
{
#ifdef SQL_ACC
	if (db_url.s) {
		if(db_url.len<=0) {
			db_url.s = NULL;
			db_url.len = 0;
		}
	}
	if(db_table_acc.len!=3 || strncmp(db_table_acc.s, "acc", 3)!=0)
	{
		db_table_acc_data = db_table_acc.s;
		if(fixup_var_pve_str_12(&db_table_acc_data, 1)<0)
		{
			LM_ERR("unable to parse acc table name [%.*s]\n",
					db_table_acc.len, db_table_acc.s);
			return -1;
		}
	}
	if(db_table_mc.len!=12 || strncmp(db_table_mc.s, "missed_calls", 12)!=0)
	{
		db_table_mc_data = db_table_mc.s;
		if(fixup_var_pve_str_12(&db_table_mc_data, 1)<0)
		{
			LM_ERR("unable to parse mc table name [%.*s]\n",
					db_table_mc.len, db_table_mc.s);
			return -1;
		}
	}
#endif

	if (log_facility_str) {
		int tmp = str2facility(log_facility_str);
		if (tmp != -1)
			log_facility = tmp;
		else {
			LM_ERR("invalid log facility configured");
			return -1;
		}
	}

	/* ----------- GENERIC INIT SECTION  ----------- */

	/* failed transaction handling */
	if ((failed_transaction_flag != -1) && 
		!flag_in_range(failed_transaction_flag)) {
		LM_ERR("failed_transaction_flag set to invalid value\n");
		return -1;
	}
	if (failed_filter_str) {
	    if (parse_failed_filter(failed_filter_str, failed_filter) == 0) {
		LM_ERR("failed to parse failed_filter param\n");
		return -1;
	    }
	} else {
	    failed_filter[0] = 0;
	}

	/* load the TM API */
	if (load_tm_api(&tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	/* if detect_direction is enabled, load rr also */
	if (detect_direction) {
		if (load_rr_api(&rrb)!=0) {
			LM_ERR("can't load RR API\n");
			return -1;
		}
		/* we need the append_fromtag on in RR */
		if (!rrb.append_fromtag) {
			LM_ERR("'append_fromtag' RR param is not enabled!"
				" - required by 'detect_direction'\n");
			return -1;
		}
	}

	/* listen for all incoming requests  */
	if ( tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, acc_onreq, 0, 0 ) <=0 ) {
		LM_ERR("cannot register TMCB_REQUEST_IN callback\n");
		return -1;
	}

	/* configure multi-leg accounting */
	if (leg_info_str && (leg_info=parse_acc_leg(leg_info_str))==0 ) {
		LM_ERR("failed to parse multileg_info param\n");
		return -1;
	}

	/* ----------- SYSLOG INIT SECTION ----------- */

	/* parse the extra string, if any */
	if (log_extra_str && (log_extra=parse_acc_extra(log_extra_str))==0 ) {
		LM_ERR("failed to parse log_extra param\n");
		return -1;
	}

	if ((log_flag != -1) && !flag_in_range(log_flag)) {
		LM_ERR("log_flag set to invalid value\n");
		return -1;
	}

	if ((log_missed_flag != -1) && !flag_in_range(log_missed_flag)) {
		LM_ERR("log_missed_flag set to invalid value\n");
		return -1;
	}

	acc_log_init();

	/* ----------- INIT CDR GENERATION ----------- */

	if( cdr_enable < 0 || cdr_enable > 1)
	{
		LM_ERR("cdr_enable is out of range\n");
		return -1;
	}

	if( cdr_expired_dlg_enable < 0 || cdr_expired_dlg_enable > 1)
	{
		LM_ERR("cdr_expired_dlg_enable is out of range\n");
		return -1;
	}

	if( cdr_enable)
	{
		if( !cdr_start_str.s || !cdr_end_str.s || !cdr_duration_str.s) 
		{
		      LM_ERR( "necessary cdr_parameters are not set\n");
		      return -1;
		}			
		
		if( !cdr_start_str.len || !cdr_end_str.len || !cdr_duration_str.len) 
		{
		      LM_ERR( "necessary cdr_parameters are empty\n");
		      return -1;
		}
		
		
		if( set_cdr_extra( cdr_log_extra_str) != 0)
		{
			LM_ERR( "failed to set cdr extra '%s'\n", cdr_log_extra_str);
			return -1;
		}

		if( cdr_facility_str && set_cdr_facility( cdr_facility_str) != 0)
		{
			LM_ERR( "failed to set cdr facility '%s'\n", cdr_facility_str);
			return -1;
		}
	
		if( init_cdr_generation() != 0)
		{
			LM_ERR("failed to init cdr generation\n");
			return -1;
		}
	}

	/* ------------ SQL INIT SECTION ----------- */

#ifdef SQL_ACC
	if (db_url.s && db_url.len > 0) {
		/* parse the extra string, if any */
		if (db_extra_str && (db_extra=parse_acc_extra(db_extra_str))==0 ) {
			LM_ERR("failed to parse db_extra param\n");
			return -1;
		}
		if (acc_db_init(&db_url)<0){
			LM_ERR("failed...did you load a database module?\n");
			return -1;
		}
		/* fix the flags */

		if ((db_flag != -1) && !flag_in_range(db_flag)) {
			LM_ERR("db_flag set to invalid value\n");
			return -1;
		}

		if ((db_missed_flag != -1) && !flag_in_range(db_missed_flag)) {
			LM_ERR("db_missed_flag set to invalid value\n");
			return -1;
		}
	} else {
		db_url.s = NULL;
		db_url.len = 0;
		db_flag = -1;
		db_missed_flag = -1;
	}
#endif

	/* ------------ RADIUS INIT SECTION ----------- */

#ifdef RAD_ACC
	if (radius_config && radius_config[0]) {
		/* parse the extra string, if any */
		if (rad_extra_str && (rad_extra=parse_acc_extra(rad_extra_str))==0 ) {
			LM_ERR("failed to parse rad_extra param\n");
			return -1;
		}

		/* fix the flags */
		if ((radius_flag != -1) && !flag_in_range(radius_flag)) {
			LM_ERR("radius_flag set to invalid value\n");
			return -1;
		}

		if ((radius_missed_flag != -1) && !flag_in_range(radius_missed_flag)) {
			LM_ERR("radius_missed_flag set to invalid value\n");
			return -1;
		}

		if (init_acc_rad( radius_config, service_type)!=0 ) {
			LM_ERR("failed to init radius\n");
			return -1;
		}
	} else {
		radius_config = 0;
		radius_flag = -1;
		radius_missed_flag = -1;
	}
#endif

	/* ------------ DIAMETER INIT SECTION ----------- */

#ifdef DIAM_ACC
	/* fix the flags */
	if (flag_idx2mask(&diameter_flag)<0)
		return -1;
	if (flag_idx2mask(&diameter_missed_flag)<0)
		return -1;

	/* parse the extra string, if any */
	if (dia_extra_str && (dia_extra=parse_acc_extra(dia_extra_str))==0 ) {
		LM_ERR("failed to parse dia_extra param\n");
		return -1;
	}

	if (acc_diam_init()!=0) {
		LM_ERR("failed to init diameter engine\n");
		return -1;
	}

#endif

	_acc_module_initialized = 1;
	if(acc_init_engines()<0) {
		LM_ERR("failed to init extra engines\n");
		return -1;
	}

	return 0;
}


static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

#ifdef SQL_ACC
	if(db_url.s && acc_db_init_child(&db_url)<0) {
		LM_ERR("could not open database connection");
		return -1;
	}

#endif

	/* DIAMETER */
#ifdef DIAM_ACC
	/* open TCP connection */
	LM_DBG("initializing TCP connection\n");

	sockfd = init_mytcp(diameter_client_host, diameter_client_port);
	if(sockfd==-1) 
	{
		LM_ERR("TCP connection not established\n");
		return -1;
	}

	LM_DBG("a TCP connection was established on sockfd=%d\n", sockfd);

	/* every child with its buffer */
	rb = (rd_buf_t*)pkg_malloc(sizeof(rd_buf_t));
	if(!rb)
	{
		LM_DBG("no more pkg memory\n");
		return -1;
	}
	rb->buf = 0;
#endif

	return 0;
}


static void destroy(void)
{
	if (log_extra)
		destroy_extras( log_extra);
#ifdef SQL_ACC
	acc_db_close();
	if (db_extra)
		destroy_extras( db_extra);
#endif
#ifdef RAD_ACC
	if (rad_extra)
		destroy_extras( rad_extra);
#endif
#ifdef DIAM_ACC
	close_tcp_connection(sockfd);
	if (dia_extra)
		destroy_extras( dia_extra);
#endif
}


/**
 * @brief return leg_info structure
 */
acc_extra_t* get_leg_info(void)
{
	return leg_info;
}

/**
 * @brief bind functions to ACC API structure
 */
static int bind_acc(acc_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	api->register_engine = acc_register_engine;
	api->get_leg_info    = get_leg_info;
	api->get_core_attrs  = core2strar;
	api->get_extra_attrs = extra2strar;
	api->get_leg_attrs   = legs2strar;
	api->parse_extra     = parse_acc_extra;
	api->exec            = acc_api_exec;
	return 0;
}

/**
 * @brief init an acc engine
 */
static int acc_init_engine(acc_engine_t *e)
{
	acc_init_info_t ai;

	if(_acc_module_initialized==0)
		return 0;

	if(e->flags & 1)
		return 0;

	memset(&ai, 0, sizeof(acc_init_info_t));
	ai.leg_info = leg_info;
	if(e->acc_init(&ai)<0)
	{
		LM_ERR("failed to initialize extra acc engine\n");
		return -1;
	}
	e->flags |= 1;
	return 0;
}

/**
 * @brief init registered acc engines
 */
static int acc_init_engines(void)
{
	acc_engine_t *e;
	e = _acc_engines;
	while(e) {
		if(acc_init_engine(e)<0)
			return -1;
		e = e->next;
	}
	return 0;
}

/**
 * @brief register an accounting engine
 * @return 0 on success, <0 on failure
 */
static int acc_register_engine(acc_engine_t *eng)
{
	acc_engine_t *e;

	if(eng==NULL)
		return -1;
	e = (acc_engine_t*)pkg_malloc(sizeof(acc_engine_t));
	if(e ==NULL)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	memcpy(e, eng, sizeof(acc_engine_t));

	if(acc_init_engine(e)<0)
	{
		pkg_free(e);
		return -1;
	}

	e->next = _acc_engines;
	_acc_engines = e;
	LM_DBG("new acc engine registered: %s\n", e->name);
	return 0;
}

/**
 *
 */
acc_engine_t *acc_api_get_engines(void)
{
	return _acc_engines;
}

