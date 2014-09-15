/*
 * $Id$
 *
 * Usrloc module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * ---------
 * 2003-01-27 timer activity printing #ifdef-ed to EXTRA_DEBUG (jiri)
 * 2003-03-11 New module interface (janakj)
 * 2003-03-12 added replication and state columns (nils)
 * 2003-03-16 flags export parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-21 failed fifo init stops init process (jiri)
 * 2004-03-17 generic callbacks added (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 */

/*! \file
 *  \brief USRLOC - Usrloc module interface
 *  \ingroup usrloc
 *
 * - Module \ref usrloc
 */

/*!
 * \defgroup usrloc Usrloc :: User location module
 * \brief User location module
 *
 * The module keeps a user location table and provides access
 * to the table to other modules. The module exports no functions
 * that could be used directly from scripts, all access is done
 * over a API. A main user of this API is the registrar module.
 * \see registrar
 */

#include <stdio.h>
#include "ul_mod.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../rpc_lookup.h"
#include "../../timer.h"     /* register_timer */
#include "../../timer_proc.h" /* register_sync_timer */
#include "../../globals.h"   /* is_main */
#include "../../ut.h"        /* str_init */
#include "../../lib/srutils/sruid.h"
#include "dlist.h"           /* register_udomain */
#include "udomain.h"         /* {insert,delete,get,release}_urecord */
#include "urecord.h"         /* {insert,delete,get}_ucontact */
#include "ucontact.h"        /* update_ucontact */
#include "ul_mi.h"
#include "ul_rpc.h"
#include "ul_callback.h"
#include "usrloc.h"

MODULE_VERSION

#define RUID_COL       "ruid"
#define USER_COL       "username"
#define DOMAIN_COL     "domain"
#define CONTACT_COL    "contact"
#define EXPIRES_COL    "expires"
#define Q_COL          "q"
#define CALLID_COL     "callid"
#define CSEQ_COL       "cseq"
#define FLAGS_COL      "flags"
#define CFLAGS_COL     "cflags"
#define USER_AGENT_COL "user_agent"
#define RECEIVED_COL   "received"
#define PATH_COL       "path"
#define SOCK_COL       "socket"
#define METHODS_COL    "methods"
#define INSTANCE_COL   "instance"
#define REG_ID_COL     "reg_id"
#define LAST_MOD_COL   "last_modified"

#define ULATTRS_USER_COL       "username"
#define ULATTRS_DOMAIN_COL     "domain"
#define ULATTRS_RUID_COL       "ruid"
#define ULATTRS_ANAME_COL      "aname"
#define ULATTRS_ATYPE_COL      "atype"
#define ULATTRS_AVALUE_COL     "avalue"
#define ULATTRS_LAST_MOD_COL   "last_modified"

static int mod_init(void);                          /*!< Module initialization function */
static void destroy(void);                          /*!< Module destroy function */
static void ul_core_timer(unsigned int ticks, void* param);  /*!< Core timer handler */
static void ul_local_timer(unsigned int ticks, void* param); /*!< Local timer handler */
static int child_init(int rank);                    /*!< Per-child init function */
static int mi_child_init(void);

#define UL_PRELOAD_SIZE	8
static char* ul_preload_list[UL_PRELOAD_SIZE];
static int ul_preload_index = 0;
static int ul_preload_param(modparam_t type, void* val);

extern int bind_usrloc(usrloc_api_t* api);
extern int ul_locks_no;
int ul_db_update_as_insert = 0;
int ul_timer_procs = 0;
int ul_db_check_update = 0;
int ul_keepalive_timeout = 0;

int ul_db_ops_ruid = 1;
int ul_expires_type = 0;
int ul_db_raw_fetch_type = 0;

str ul_xavp_contact_name = {0};

/* sruid to get internal uid for mi/rpc commands */
sruid_t _ul_sruid;

/*
 * Module parameters and their default values
 */

str ruid_col        = str_init(RUID_COL); 		/*!< Name of column containing record unique id */
str user_col        = str_init(USER_COL); 		/*!< Name of column containing usernames */
str domain_col      = str_init(DOMAIN_COL); 	/*!< Name of column containing domains */
str contact_col     = str_init(CONTACT_COL);	/*!< Name of column containing contact addresses */
str expires_col     = str_init(EXPIRES_COL);	/*!< Name of column containing expires values */
str q_col           = str_init(Q_COL);			/*!< Name of column containing q values */
str callid_col      = str_init(CALLID_COL);		/*!< Name of column containing callid string */
str cseq_col        = str_init(CSEQ_COL);		/*!< Name of column containing cseq values */
str flags_col       = str_init(FLAGS_COL);		/*!< Name of column containing internal flags */
str cflags_col      = str_init(CFLAGS_COL);		/*!< Name of column containing contact flags */
str user_agent_col  = str_init(USER_AGENT_COL);	/*!< Name of column containing user agent string */
str received_col    = str_init(RECEIVED_COL);	/*!< Name of column containing transport info of REGISTER */
str path_col        = str_init(PATH_COL);		/*!< Name of column containing the Path header */
str sock_col        = str_init(SOCK_COL);		/*!< Name of column containing the received socket */
str methods_col     = str_init(METHODS_COL);	/*!< Name of column containing the supported methods */
str instance_col    = str_init(INSTANCE_COL);	/*!< Name of column containing the SIP instance value */
str reg_id_col      = str_init(REG_ID_COL);		/*!< Name of column containing the reg-id value */
str last_mod_col    = str_init(LAST_MOD_COL);	/*!< Name of column containing the last modified date */

str ulattrs_user_col   = str_init(ULATTRS_USER_COL);   /*!< Name of column containing username */
str ulattrs_domain_col = str_init(ULATTRS_DOMAIN_COL); /*!< Name of column containing domain */
str ulattrs_ruid_col   = str_init(ULATTRS_RUID_COL);   /*!< Name of column containing record unique id */
str ulattrs_aname_col  = str_init(ULATTRS_ANAME_COL);  /*!< Name of column containing attribute name */
str ulattrs_atype_col  = str_init(ULATTRS_ATYPE_COL);  /*!< Name of column containing attribute type */
str ulattrs_avalue_col = str_init(ULATTRS_AVALUE_COL); /*!< Name of column containing attribute value */
str ulattrs_last_mod_col = str_init(ULATTRS_LAST_MOD_COL);	/*!< Name of column containing the last modified date */

str db_url          = str_init(DEFAULT_DB_URL);	/*!< Database URL */
int timer_interval  = 60;				/*!< Timer interval in seconds */
int db_mode         = 0;				/*!< Database sync scheme: 0-no db, 1-write through, 2-write back, 3-only db */
int use_domain      = 0;				/*!< Whether usrloc should use domain part of aor */
int desc_time_order = 0;				/*!< By default do not enable timestamp ordering */
int handle_lost_tcp = 0;				/*!< By default do not remove contacts before expiration time */

int ul_fetch_rows = 2000;				/*!< number of rows to fetch from result */
int ul_hash_size = 9;
int ul_db_insert_null = 0;

/* flags */
unsigned int nat_bflag = (unsigned int)-1;
unsigned int init_flag = 0;

db1_con_t* ul_dbh = 0; /* Database connection handle */
db_func_t ul_dbf;



/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"ul_bind_usrloc",        (cmd_function)bind_usrloc,        1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters 
 */
static param_export_t params[] = {
	{"ruid_column",         PARAM_STR, &ruid_col      },
	{"user_column",         PARAM_STR, &user_col      },
	{"domain_column",       PARAM_STR, &domain_col    },
	{"contact_column",      PARAM_STR, &contact_col   },
	{"expires_column",      PARAM_STR, &expires_col   },
	{"q_column",            PARAM_STR, &q_col         },
	{"callid_column",       PARAM_STR, &callid_col    },
	{"cseq_column",         PARAM_STR, &cseq_col      },
	{"flags_column",        PARAM_STR, &flags_col     },
	{"cflags_column",       PARAM_STR, &cflags_col    },
	{"db_url",              PARAM_STR, &db_url        },
	{"timer_interval",      INT_PARAM, &timer_interval  },
	{"db_mode",             INT_PARAM, &db_mode         },
	{"use_domain",          INT_PARAM, &use_domain      },
	{"desc_time_order",     INT_PARAM, &desc_time_order },
	{"user_agent_column",   PARAM_STR, &user_agent_col},
	{"received_column",     PARAM_STR, &received_col  },
	{"path_column",         PARAM_STR, &path_col      },
	{"socket_column",       PARAM_STR, &sock_col      },
	{"methods_column",      PARAM_STR, &methods_col   },
	{"instance_column",     PARAM_STR, &instance_col  },
	{"reg_id_column",       PARAM_STR, &reg_id_col    },
	{"matching_mode",       INT_PARAM, &matching_mode   },
	{"cseq_delay",          INT_PARAM, &cseq_delay      },
	{"fetch_rows",          INT_PARAM, &ul_fetch_rows   },
	{"hash_size",           INT_PARAM, &ul_hash_size    },
	{"nat_bflag",           INT_PARAM, &nat_bflag       },
	{"handle_lost_tcp",     INT_PARAM, &handle_lost_tcp },
	{"preload",             PARAM_STRING|USE_FUNC_PARAM, (void*)ul_preload_param},
	{"db_update_as_insert", INT_PARAM, &ul_db_update_as_insert},
	{"timer_procs",         INT_PARAM, &ul_timer_procs},
	{"db_check_update",     INT_PARAM, &ul_db_check_update},
	{"xavp_contact",        PARAM_STR, &ul_xavp_contact_name},
	{"db_ops_ruid",         INT_PARAM, &ul_db_ops_ruid},
	{"expires_type",        PARAM_INT, &ul_expires_type},
	{"db_raw_fetch_type",   PARAM_INT, &ul_db_raw_fetch_type},
	{"db_insert_null",      PARAM_INT, &ul_db_insert_null},
	{0, 0, 0}
};


stat_export_t mod_stats[] = {
	{"registered_users" ,  STAT_IS_FUNC, (stat_var**)get_number_of_users  },
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ MI_USRLOC_RM,           mi_usrloc_rm_aor,       0,                 0,
				mi_child_init },
	{ MI_USRLOC_RM_CONTACT,   mi_usrloc_rm_contact,   0,                 0,
				mi_child_init },
	{ MI_USRLOC_DUMP,         mi_usrloc_dump,         0,                 0,
				0             },
	{ MI_USRLOC_FLUSH,        mi_usrloc_flush,        MI_NO_INPUT_FLAG,  0,
				mi_child_init },
	{ MI_USRLOC_ADD,          mi_usrloc_add,          0,                 0,
				mi_child_init },
	{ MI_USRLOC_SHOW_CONTACT, mi_usrloc_show_contact, 0,                 0,
				mi_child_init },
	{ 0, 0, 0, 0, 0}
};


struct module_exports exports = {
	"usrloc",
	DEFAULT_DLFLAGS, /*!< dlopen flags */
	cmds,       /*!< Exported functions */
	params,     /*!< Export parameters */
	mod_stats,  /*!< exported statistics */
	mi_cmds,    /*!< exported MI functions */
	0,          /*!< exported pseudo-variables */
	0,          /*!< extra processes */
	mod_init,   /*!< Module initialization function */
	0,          /*!< Response function */
	destroy,    /*!< Destroy function */
	child_init  /*!< Child initialization function */
};


/*! \brief
 * Module initialization function
 */
static int mod_init(void)
{
	int i;
	udomain_t* d;

	if(sruid_init(&_ul_sruid, '-', "ulcx", SRUID_INC)<0)
		return -1;

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif
	
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if (rpc_register_array(ul_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(ul_hash_size<=1)
		ul_hash_size = 512;
	else
		ul_hash_size = 1<<ul_hash_size;
	ul_locks_no = ul_hash_size;

	/* check matching mode */
	switch (matching_mode) {
		case CONTACT_ONLY:
		case CONTACT_CALLID:
		case CONTACT_PATH:
			break;
		default:
			LM_ERR("invalid matching mode %d\n", matching_mode);
	}

	if(ul_init_locks()!=0)
	{
		LM_ERR("locks array initialization failed\n");
		return -1;
	}

	/* Register cache timer */
	if(ul_timer_procs<=0)
	{
		if (timer_interval > 0)
			register_timer(ul_core_timer, 0, timer_interval);
	}
	else
		register_sync_timers(ul_timer_procs);

	/* init the callbacks list */
	if ( init_ulcb_list() < 0) {
		LM_ERR("usrloc/callbacks initialization failed\n");
		return -1;
	}

	/* Shall we use database ? */
	if (db_mode != NO_DB) { /* Yes */
		if (db_bind_mod(&db_url, &ul_dbf) < 0) { /* Find database module */
			LM_ERR("failed to bind database module\n");
			return -1;
		}
		if (!DB_CAPABILITY(ul_dbf, DB_CAP_ALL)) {
			LM_ERR("database module does not implement all functions"
					" needed by the module\n");
			return -1;
		}
		if(ul_fetch_rows<=0) {
			LM_ERR("invalid fetch_rows number '%d'\n", ul_fetch_rows);
			return -1;
		}
	}

	if (nat_bflag==(unsigned int)-1) {
		nat_bflag = 0;
	} else if ( nat_bflag>=8*sizeof(nat_bflag) ) {
		LM_ERR("bflag index (%d) too big!\n", nat_bflag);
		return -1;
	} else {
		nat_bflag = 1<<nat_bflag;
	}

	for(i=0; i<ul_preload_index; i++) {
		if(register_udomain((const char*)ul_preload_list[i], &d)<0) {
			LM_ERR("cannot register preloaded table %s\n", ul_preload_list[i]);
			return -1;
		}
	}

	if (handle_lost_tcp && db_mode == DB_ONLY)
		LM_WARN("handle_lost_tcp option makes nothing in DB_ONLY mode\n");

	init_flag = 1;

	return 0;
}


static int child_init(int _rank)
{
	dlist_t* ptr;
	int i;

	if(sruid_init(&_ul_sruid, '-', "ulcx", SRUID_INC)<0)
		return -1;

	if(_rank==PROC_MAIN && ul_timer_procs>0)
	{
		for(i=0; i<ul_timer_procs; i++)
		{
			if(fork_sync_timer(PROC_TIMER, "USRLOC Timer", 1 /*socks flag*/,
					ul_local_timer, (void*)(long)i, timer_interval /*sec*/)<0) {
				LM_ERR("failed to start timer routine as process\n");
				return -1; /* error */
			}
		}
	}

	/* connecting to DB ? */
	switch (db_mode) {
		case NO_DB:
			return 0;
		case DB_ONLY:
		case WRITE_THROUGH:
			/* connect to db only from SIP workers, TIMER and MAIN processes */
			if (_rank<=0 && _rank!=PROC_TIMER && _rank!=PROC_MAIN)
				return 0;
			break;
		case WRITE_BACK:
			/* connect to db only from TIMER (for flush), from MAIN (for
			 * final flush() and from child 1 for preload */
			if (_rank!=PROC_TIMER && _rank!=PROC_MAIN && _rank!=PROC_SIPINIT)
				return 0;
			break;
		case DB_READONLY:
			/* connect to db only from child 1 for preload */
			if(_rank!=PROC_SIPINIT)
				return 0;
			break;
	}

	ul_dbh = ul_dbf.init(&db_url); /* Get a database connection per child */
	if (!ul_dbh) {
		LM_ERR("child(%d): failed to connect to database\n", _rank);
		return -1;
	}
	/* _rank==PROC_SIPINIT is used even when fork is disabled */
	if (_rank==PROC_SIPINIT && db_mode!=DB_ONLY) {
		/* if cache is used, populate domains from DB */
		for( ptr=root ; ptr ; ptr=ptr->next) {
			if (preload_udomain(ul_dbh, ptr->d) < 0) {
				LM_ERR("child(%d): failed to preload domain '%.*s'\n",
						_rank, ptr->name.len, ZSW(ptr->name.s));
				return -1;
			}
			uldb_preload_attrs(ptr->d);
		}
	}

	return 0;
}


/* */
static int mi_child_init(void)
{
	static int done = 0;

	if (done)
		return 0;

	if (db_mode != NO_DB) {
		ul_dbh = ul_dbf.init(&db_url);
		if (!ul_dbh) {
			LM_ERR("failed to connect to database\n");
			return -1;
		}
	}

	if(sruid_init(&_ul_sruid, '-', "ulcx", SRUID_INC)<0)
		return -1;
	done = 1;

	return 0;
}


/*! \brief
 * Module destroy function
 */
static void destroy(void)
{
	/* we need to sync DB in order to flush the cache */
	if (ul_dbh) {
		ul_unlock_locks();
		if (synchronize_all_udomains(0, 1) != 0) {
			LM_ERR("flushing cache failed\n");
		}
		ul_dbf.close(ul_dbh);
	}

	free_all_udomains();
	ul_destroy_locks();

	/* free callbacks list */
	destroy_ulcb_list();
}


/*! \brief
 * Core timer handler
 */
static void ul_core_timer(unsigned int ticks, void* param)
{
	if (synchronize_all_udomains(0, 1) != 0) {
		LM_ERR("synchronizing cache failed\n");
	}
}

/*! \brief
 * Local timer handler
 */
static void ul_local_timer(unsigned int ticks, void* param)
{
	if (synchronize_all_udomains((int)(long)param, ul_timer_procs) != 0) {
		LM_ERR("synchronizing cache failed\n");
	}
}

/*! \brief
 * preload module parameter handler
 */
static int ul_preload_param(modparam_t type, void* val)
{
	if(val==NULL)
	{
		LM_ERR("invalid parameter\n");
		goto error;
	}
	if(ul_preload_index>=UL_PRELOAD_SIZE)
	{
		LM_ERR("too many preloaded tables\n");
		goto error;
	}
	ul_preload_list[ul_preload_index] = (char*)val;
	ul_preload_index++;
	return 0;
error:
	return -1;
}
