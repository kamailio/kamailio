/*
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
#include "usrloc_mod.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/rpc_lookup.h"
#include "../../core/timer.h"     /* register_timer */
#include "../../core/timer_proc.h" /* register_sync_timer */
#include "../../core/globals.h"
#include "../../core/ut.h"        /* str_init */
#include "../../core/utils/sruid.h"
#include "dlist.h"           /* register_udomain */
#include "udomain.h"         /* {insert,delete,get,release}_urecord */
#include "urecord.h"         /* {insert,delete,get}_ucontact */
#include "ucontact.h"        /* update_ucontact */
#include "ul_rpc.h"
#include "ul_callback.h"
#include "ul_keepalive.h"
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
#define SRV_ID_COL     "server_id"
#define CON_ID_COL     "connection_id"
#define KEEPALIVE_COL  "keepalive"
#define PARTITION_COL  "partition"

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
static void ul_db_clean_timer(unsigned int ticks, void* param); /*!< DB clean timer handler */
static int child_init(int rank);                    /*!< Per-child init function */
static int ul_sip_reply_received(sip_msg_t *msg); /*!< SIP response handling */

#define UL_PRELOAD_SIZE	8
static char* ul_preload_list[UL_PRELOAD_SIZE];
static int ul_preload_index = 0;
static int ul_preload_param(modparam_t type, void* val);

extern int bind_usrloc(usrloc_api_t* api);
int ul_db_update_as_insert = 0;
int ul_timer_procs = 0;
int ul_db_check_update = 0;
int ul_keepalive_timeout = 0;

int ul_db_ops_ruid = 1;
int ul_expires_type = 0;
int ul_db_raw_fetch_type = 0;
int ul_rm_expired_delay = 0;
int ul_version_table = 1;

int ul_load_rank = PROC_SIPINIT;
str ul_xavp_contact_name = {0};

str ul_ka_from = str_init("sip:server@kamailio.org");
str ul_ka_domain = str_init("kamailio.org");
str ul_ka_method = str_init("OPTIONS");
int ul_ka_mode = 0;
int ul_ka_filter = 0;
int ul_ka_loglevel = 255;
str ul_ka_logmsg = str_init(" to-uri: [$tu] remote-addr: [$sas]");
pv_elem_t *ul_ka_logfmt = NULL;

/* sruid to get internal uid for mi/rpc commands */
sruid_t _ul_sruid;

/*
 * Module parameters and their default values
 */

str ul_ruid_col        = str_init(RUID_COL); 		/*!< Name of column containing record unique id */
str ul_user_col        = str_init(USER_COL); 		/*!< Name of column containing usernames */
str ul_domain_col      = str_init(DOMAIN_COL); 	/*!< Name of column containing domains */
str ul_contact_col     = str_init(CONTACT_COL);	/*!< Name of column containing contact addresses */
str ul_expires_col     = str_init(EXPIRES_COL);	/*!< Name of column containing expires values */
str ul_q_col           = str_init(Q_COL);			/*!< Name of column containing q values */
str ul_callid_col      = str_init(CALLID_COL);		/*!< Name of column containing callid string */
str ul_cseq_col        = str_init(CSEQ_COL);		/*!< Name of column containing cseq values */
str ul_flags_col       = str_init(FLAGS_COL);		/*!< Name of column containing internal flags */
str ul_cflags_col      = str_init(CFLAGS_COL);		/*!< Name of column containing contact flags */
str ul_user_agent_col  = str_init(USER_AGENT_COL);	/*!< Name of column containing user agent string */
str ul_received_col    = str_init(RECEIVED_COL);	/*!< Name of column containing transport info of REGISTER */
str ul_path_col        = str_init(PATH_COL);		/*!< Name of column containing the Path header */
str ul_sock_col        = str_init(SOCK_COL);		/*!< Name of column containing the received socket */
str ul_methods_col     = str_init(METHODS_COL);	/*!< Name of column containing the supported methods */
str ul_instance_col    = str_init(INSTANCE_COL);	/*!< Name of column containing the SIP instance value */
str ul_reg_id_col      = str_init(REG_ID_COL);		/*!< Name of column containing the reg-id value */
str ul_last_mod_col    = str_init(LAST_MOD_COL);	/*!< Name of column containing the last modified date */
str ul_srv_id_col      = str_init(SRV_ID_COL);		/*!< Name of column containing the server id value */
str ul_con_id_col      = str_init(CON_ID_COL);		/*!< Name of column containing the connection id value */
str ul_keepalive_col   = str_init(KEEPALIVE_COL);	/*!< Name of column containing the keepalive value */
str ul_partition_col   = str_init(PARTITION_COL);	/*!< Name of column containing the partition value */

str ulattrs_user_col   = str_init(ULATTRS_USER_COL);   /*!< Name of column containing username */
str ulattrs_domain_col = str_init(ULATTRS_DOMAIN_COL); /*!< Name of column containing domain */
str ulattrs_ruid_col   = str_init(ULATTRS_RUID_COL);   /*!< Name of column containing record unique id */
str ulattrs_aname_col  = str_init(ULATTRS_ANAME_COL);  /*!< Name of column containing attribute name */
str ulattrs_atype_col  = str_init(ULATTRS_ATYPE_COL);  /*!< Name of column containing attribute type */
str ulattrs_avalue_col = str_init(ULATTRS_AVALUE_COL); /*!< Name of column containing attribute value */
str ulattrs_last_mod_col = str_init(ULATTRS_LAST_MOD_COL);	/*!< Name of column containing the last modified date */

str ul_db_url          = str_init(DEFAULT_DB_URL);	/*!< Database URL */
int ul_timer_interval  = 60;				/*!< Timer interval in seconds */
int ul_db_mode         = 0;				/*!< Database sync scheme: 0-no db, 1-write through, 2-write back, 3-only db */
int ul_db_load         = 1;				/*!< Database load after restart: 1- true, 0- false (only the db_mode allows it) */
int ul_db_insert_update = 0;				/*!< Database : update on duplicate key instead of error */
int ul_use_domain      = 0;				/*!< Whether usrloc should use domain part of aor */
int ul_desc_time_order = 0;				/*!< By default do not enable timestamp ordering */
int ul_handle_lost_tcp = 0;				/*!< By default do not remove contacts before expiration time */
int ul_close_expired_tcp = 0;				/*!< By default do not close TCP connections for expired contacts */
int ul_skip_remote_socket = 0;				/*!< By default do not skip remote socket */
int ul_db_clean_tcp = 0;				/*!< Clean TCP/TLS/WSS contacts in DB before loading records */

int ul_fetch_rows = 2000;				/*!< number of rows to fetch from result */
int ul_hash_size = 10;
int ul_db_insert_null = 0;
int ul_db_timer_clean = 0;

/* flags */
unsigned int ul_nat_bflag = (unsigned int)-1;
unsigned int ul_init_flag = 0;

db1_con_t* ul_dbh = 0; /* Database connection handle */
db_func_t ul_dbf;

/* filter on load and during cleanup by server id */
unsigned int ul_db_srvid = 0;

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
	{"ruid_column",         PARAM_STR, &ul_ruid_col      },
	{"user_column",         PARAM_STR, &ul_user_col      },
	{"domain_column",       PARAM_STR, &ul_domain_col    },
	{"contact_column",      PARAM_STR, &ul_contact_col   },
	{"expires_column",      PARAM_STR, &ul_expires_col   },
	{"q_column",            PARAM_STR, &ul_q_col         },
	{"callid_column",       PARAM_STR, &ul_callid_col    },
	{"cseq_column",         PARAM_STR, &ul_cseq_col      },
	{"flags_column",        PARAM_STR, &ul_flags_col     },
	{"cflags_column",       PARAM_STR, &ul_cflags_col    },
	{"db_url",              PARAM_STR, &ul_db_url        },
	{"timer_interval",      INT_PARAM, &ul_timer_interval  },
	{"db_mode",             INT_PARAM, &ul_db_mode         },
	{"db_load",             INT_PARAM, &ul_db_load         },
	{"db_insert_update",    INT_PARAM, &ul_db_insert_update },
	{"use_domain",          INT_PARAM, &ul_use_domain      },
	{"desc_time_order",     INT_PARAM, &ul_desc_time_order },
	{"user_agent_column",   PARAM_STR, &ul_user_agent_col},
	{"received_column",     PARAM_STR, &ul_received_col  },
	{"path_column",         PARAM_STR, &ul_path_col      },
	{"socket_column",       PARAM_STR, &ul_sock_col      },
	{"methods_column",      PARAM_STR, &ul_methods_col   },
	{"instance_column",     PARAM_STR, &ul_instance_col  },
	{"reg_id_column",       PARAM_STR, &ul_reg_id_col    },
	{"server_id_column",    PARAM_STR, &ul_srv_id_col    },
	{"connection_id_column",PARAM_STR, &ul_con_id_col    },
	{"keepalive_column",    PARAM_STR, &ul_keepalive_col },
	{"partition_column",    PARAM_STR, &ul_partition_col },
	{"matching_mode",       INT_PARAM, &ul_matching_mode },
	{"cseq_delay",          INT_PARAM, &ul_cseq_delay      },
	{"fetch_rows",          INT_PARAM, &ul_fetch_rows   },
	{"hash_size",           INT_PARAM, &ul_hash_size    },
	{"nat_bflag",           INT_PARAM, &ul_nat_bflag       },
	{"handle_lost_tcp",     INT_PARAM, &ul_handle_lost_tcp },
	{"close_expired_tcp",   INT_PARAM, &ul_close_expired_tcp },
	{"skip_remote_socket",  INT_PARAM, &ul_skip_remote_socket },
	{"preload",             PARAM_STRING|USE_FUNC_PARAM, (void*)ul_preload_param},
	{"db_update_as_insert", INT_PARAM, &ul_db_update_as_insert},
	{"timer_procs",         INT_PARAM, &ul_timer_procs},
	{"db_check_update",     INT_PARAM, &ul_db_check_update},
	{"xavp_contact",        PARAM_STR, &ul_xavp_contact_name},
	{"db_ops_ruid",         INT_PARAM, &ul_db_ops_ruid},
	{"expires_type",        PARAM_INT, &ul_expires_type},
	{"db_raw_fetch_type",   PARAM_INT, &ul_db_raw_fetch_type},
	{"db_insert_null",      PARAM_INT, &ul_db_insert_null},
	{"server_id_filter",    PARAM_INT, &ul_db_srvid},
	{"db_timer_clean",      PARAM_INT, &ul_db_timer_clean},
	{"rm_expired_delay",    PARAM_INT, &ul_rm_expired_delay},
	{"version_table",       PARAM_INT, &ul_version_table},
	{"ka_mode",             PARAM_INT, &ul_ka_mode},
	{"ka_from",             PARAM_STR, &ul_ka_from},
	{"ka_domain",           PARAM_STR, &ul_ka_domain},
	{"ka_method",           PARAM_STR, &ul_ka_method},
	{"ka_filter",           PARAM_INT, &ul_ka_filter},
	{"ka_timeout",          PARAM_INT, &ul_keepalive_timeout},
	{"ka_loglevel",         PARAM_INT, &ul_ka_loglevel},
	{"ka_logmsg",           PARAM_STR, &ul_ka_logmsg},
	{"load_rank",           PARAM_INT, &ul_load_rank},
	{"db_clean_tcp",        PARAM_INT, &ul_db_clean_tcp},
	{0, 0, 0}
};


stat_export_t mod_stats[] = {
	{"registered_users" ,  STAT_IS_FUNC, (stat_var**)get_number_of_users  },
	{0,0,0}
};


struct module_exports exports = {
	"usrloc",        /*!< module name */
	DEFAULT_DLFLAGS, /*!< dlopen flags */
	cmds,            /*!< exported functions */
	params,          /*!< exported parameters */
	0,               /*!< exported rpc functions */
	0,               /*!< exported pseudo-variables */
	ul_sip_reply_received, /*!< response handling function */
	mod_init,        /*!< module init function */
	child_init,      /*!< child init function */
	destroy          /*!< destroy function */
};


/*! \brief
 * Module initialization function
 */
static int mod_init(void)
{
	int i;
	udomain_t* d;

	if(ul_rm_expired_delay!=0) {
		if(ul_db_mode != DB_ONLY) {
			LM_ERR("rm expired delay feature is available for db only mode\n");
			return -1;
		}
	}
	if(ul_rm_expired_delay<0) {
		LM_WARN("rm expired delay value is negative (%d) - setting it to 0\n",
				ul_rm_expired_delay);
		ul_rm_expired_delay = 0;
	}
	if(sruid_init(&_ul_sruid, '-', "ulcx", SRUID_INC)<0) {
		return -1;
	}

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats(exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	if (rpc_register_array(ul_rpc)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(ul_hash_size<=1) {
		ul_hash_size = 512;
	} else {
		ul_hash_size = 1<<ul_hash_size;
	}

	/* check matching mode */
	switch (ul_matching_mode) {
		case CONTACT_ONLY:
		case CONTACT_CALLID:
		case CONTACT_PATH:
		case CONTACT_CALLID_ONLY:
			break;
		default:
			LM_ERR("invalid matching mode %d\n", ul_matching_mode);
	}

	/* Register cache timer */
	if(ul_timer_procs<=0) {
		if (ul_timer_interval > 0) {
			register_timer(ul_core_timer, 0, ul_timer_interval);
		}
	} else {
		register_sync_timers(ul_timer_procs);
	}

	/* init the callbacks list */
	if (init_ulcb_list() < 0) {
		LM_ERR("usrloc/callbacks initialization failed\n");
		return -1;
	}

	/* Shall we use database ? */
	switch (ul_db_mode) {
		case DB_ONLY:
		case WRITE_THROUGH:
		case WRITE_BACK:
		/*
		 * register the need to be called post-fork of all children
		 * with the special rank PROC_POSTCHILDINIT
		 */
		ksr_module_set_flag(KSRMOD_FLAG_POSTCHILDINIT);
	}
	if (ul_db_mode != NO_DB) { /* Yes */
		if (db_bind_mod(&ul_db_url, &ul_dbf) < 0) { /* Find database module */
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
	if(ul_db_mode==WRITE_THROUGH || ul_db_mode==WRITE_BACK) {
		if(ul_db_timer_clean!=0) {
			if(sr_wtimer_add(ul_db_clean_timer, 0, ul_timer_interval)<0) {
				LM_ERR("failed to add db clean timer routine\n");
				return -1;
			}
		}
	}

	if (ul_nat_bflag==(unsigned int)-1) {
		ul_nat_bflag = 0;
	} else if (ul_nat_bflag>=8*sizeof(ul_nat_bflag) ) {
		LM_ERR("bflag index (%d) too big!\n", ul_nat_bflag);
		return -1;
	} else {
		ul_nat_bflag = 1<<ul_nat_bflag;
	}

	for(i=0; i<ul_preload_index; i++) {
		if(register_udomain((const char*)ul_preload_list[i], &d)<0) {
			LM_ERR("cannot register preloaded table %s\n", ul_preload_list[i]);
			return -1;
		}
	}

	if (ul_handle_lost_tcp && ul_db_mode == DB_ONLY) {
		LM_WARN("handle_lost_tcp option makes nothing in DB_ONLY mode\n");
	}

	if(ul_db_mode != DB_ONLY) {
		ul_set_xavp_contact_clone(1);
	}

	if(ul_ka_mode != ULKA_NONE) {
		/* set max partition number for timers processing of db records */
		if (ul_timer_procs > 1) {
			ul_set_max_partition((unsigned int)ul_timer_procs);
		}
		if(ul_ka_logmsg.len > 0) {
			if(pv_parse_format(&ul_ka_logmsg, &ul_ka_logfmt) < 0) {
				LM_ERR("failed parsing ka log message format\n");
				return -1;
			}
		}
	}

	ul_init_flag = 1;
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
					ul_local_timer, (void*)(long)i, ul_timer_interval /*sec*/)<0) {
				LM_ERR("failed to start timer routine as process\n");
				return -1; /* error */
			}
		}
	}

	/* connecting to DB ? */
	switch (ul_db_mode) {
		case NO_DB:
			return 0;
		case DB_ONLY:
		case WRITE_THROUGH:
			/* connect to db only from SIP workers, TIMER and MAIN processes,
			 *  and RPC processes */
			if (_rank<=0 && _rank!=PROC_TIMER && _rank!=PROC_POSTCHILDINIT
					 && _rank!=PROC_RPC)
				return 0;
			break;
		case WRITE_BACK:
			/* connect to db only from TIMER (for flush), from MAIN (for
			 * final flush() and from child 1 for preload */
			if (_rank!=PROC_TIMER && _rank!=PROC_POSTCHILDINIT && _rank!=PROC_SIPINIT)
				return 0;
			break;
		case DB_READONLY:
			/* connect to db only from child 1 for preload */
			ul_db_load=1; /* we always load from the db in this mode */
			if(_rank!=PROC_SIPINIT)
				return 0;
			break;
	}

	ul_dbh = ul_dbf.init(&ul_db_url); /* Get a database connection per child */
	if (!ul_dbh) {
		LM_ERR("child(%d): failed to connect to database\n", _rank);
		return -1;
	}
	/* _rank==PROC_SIPINIT is used even when fork is disabled */
	if (_rank==ul_load_rank && ul_db_mode!=DB_ONLY && ul_db_load) {
		/* if cache is used, populate domains from DB */
		for(ptr=_ksr_ul_root ; ptr ; ptr=ptr->next) {
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


/*! \brief
 * Module destroy function
 */
static void destroy(void)
{
	/* we need to sync DB in order to flush the cache */
	if (ul_dbh) {
		if (synchronize_all_udomains(0, 1) != 0) {
			LM_ERR("flushing cache failed\n");
		}
		ul_dbf.close(ul_dbh);
	}

	free_all_udomains();

	/* free callbacks list */
	destroy_ulcb_list();
}

/*! \brief
 * Callback to handle the SIP replies
 */
static int ul_sip_reply_received(sip_msg_t *msg)
{
	if(ul_ka_mode == 0) {
		return 1;
	}
	ul_ka_reply_received(msg);
	return 1;
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
 * DB dlean timer handler
 */
static void ul_db_clean_timer(unsigned int ticks, void* param)
{
	ul_db_clean_udomains();
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
