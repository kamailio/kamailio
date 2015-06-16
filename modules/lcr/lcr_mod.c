/*
 * Least Cost Routing module
 *
 * Copyright (C) 2005-2014 Juha Heinanen
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
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
 *  2005-02-14: Introduced lcr module (jh)
 *  2005-02-20: Added sequential forking functions (jh)
 *  2005-02-25: Added support for int AVP names, combined addr and port
 *              AVPs (jh)
 *  2005-07-28: Added support for gw URI scheme and transport, 
 *              backport from ser (kd)
 *  2005-08-20: Added support for gw prefixes (jh)
 *  2005-09-03: Request-URI user part can be modified between load_gws()
 *              and first next_gw() calls.
 *  2008-10-10: Database values are now checked and from/to_gw functions
 *              execute in O(logN) time.
 *  2008-11-26: Added timer based check of gateways (shurik)
 *  2009-05-12  added RPC support (andrei)
 *  2009-06-21  Added support for more than one lcr instance and
                gw defunct capability (jh)
 */
/*!
 * \file
 * \brief SIP-router LCR :: Module interface
 * \ingroup lcr
 * Module: \ref lcr
 */

/*! \defgroup lcr SIP-router Least Cost Routing Module
 *
 * The Least Cost Routing (LCR) module implements capability to serially
 * forward a request to one or more gateways so that the order in which
 * the gateways is tried is based on admin defined "least cost" rules.

 * The LCR module supports many independent LCR instances (gateways and
 * least cost rules). Each such instance has its own LCR identifier.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pcre.h>
#include "../../locking.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../usr_avp.h"
#include "../../parser/parse_from.h"
#include "../../parser/msg_parser.h"
#include "../../action.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "../../ip_addr.h"
#include "../../resolve.h"
#include "../../mod_fix.h"
#include "../../socket_info.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "hash.h"
#include "lcr_rpc.h"
#include "../../rpc_lookup.h"
#include "../../modules/tm/tm_load.h"

MODULE_VERSION

/*
 * versions of database tables required by the module.
 */
#define LCR_RULE_TABLE_VERSION 2
#define LCR_RULE_TARGET_TABLE_VERSION 1
#define LCR_GW_TABLE_VERSION 3

/* database defaults */

#define LCR_RULE_TABLE "lcr_rule"
#define LCR_RULE_TARGET_TABLE "lcr_rule_target"
#define LCR_GW_TABLE "lcr_gw"

#define ID_COL "id"
#define LCR_ID_COL "lcr_id"
#define PREFIX_COL "prefix"
#define FROM_URI_COL "from_uri"
#define REQUEST_URI_COL "request_uri"
#define STOPPER_COL "stopper"
#define ENABLED_COL "enabled"
#define RULE_ID_COL "rule_id"
#define PRIORITY_COL "priority"
#define GW_ID_COL "gw_id"
#define WEIGHT_COL "weight"
#define GW_NAME_COL "gw_name"
#define IP_ADDR_COL "ip_addr"
#define PORT_COL "port"
#define URI_SCHEME_COL "uri_scheme"
#define TRANSPORT_COL "transport"
#define PARAMS_COL "params"
#define HOSTNAME_COL "hostname"
#define STRIP_COL "strip"
#define PREFIX_COL "prefix"
#define TAG_COL "tag"
#define FLAGS_COL "flags"
#define DEFUNCT_COL "defunct"

/* Default module parameter values */
#define DEF_LCR_COUNT 1
#define DEF_LCR_RULE_HASH_SIZE 128
#define DEF_LCR_GW_COUNT 128
#define DEF_FETCH_ROWS 1024

/*
 * Type definitions
 */

struct matched_gw_info {
    unsigned short gw_index;
    unsigned short prefix_len;
    unsigned short priority;
    unsigned int weight;
    unsigned short duplicate;
};

/*
 * Database variables
 */
static db1_con_t* dbh = 0;   /* Database connection handle */
static db_func_t lcr_dbf;

/*
 * Locking variables
 */
gen_lock_t *reload_lock;

/*
 * Module parameter variables
 */

/* database variables */
static str db_url           = str_init(DEFAULT_RODB_URL);
static str lcr_rule_table   = str_init(LCR_RULE_TABLE);
static str lcr_rule_target_table = str_init(LCR_RULE_TARGET_TABLE);
static str lcr_gw_table     = str_init(LCR_GW_TABLE);
static str id_col           = str_init(ID_COL);
static str lcr_id_col       = str_init(LCR_ID_COL);
static str prefix_col       = str_init(PREFIX_COL);
static str from_uri_col     = str_init(FROM_URI_COL);
static str request_uri_col  = str_init(REQUEST_URI_COL);
static str stopper_col      = str_init(STOPPER_COL);
static str enabled_col      = str_init(ENABLED_COL);
static str rule_id_col      = str_init(RULE_ID_COL);
static str priority_col     = str_init(PRIORITY_COL);
static str gw_id_col        = str_init(GW_ID_COL);
static str weight_col       = str_init(WEIGHT_COL);
static str gw_name_col      = str_init(GW_NAME_COL);
static str ip_addr_col      = str_init(IP_ADDR_COL);
static str port_col         = str_init(PORT_COL);
static str uri_scheme_col   = str_init(URI_SCHEME_COL);
static str transport_col    = str_init(TRANSPORT_COL);
static str params_col       = str_init(PARAMS_COL);
static str hostname_col     = str_init(HOSTNAME_COL);
static str strip_col        = str_init(STRIP_COL);
static str tag_col          = str_init(TAG_COL);
static str flags_col        = str_init(FLAGS_COL);
static str defunct_col      = str_init(DEFUNCT_COL);

/* number of rows to fetch at a shot */
static int fetch_rows_param = DEF_FETCH_ROWS;

/* avps */
static char *gw_uri_avp_param = NULL;
static char *ruri_user_avp_param = NULL;
static char *tag_avp_param = NULL;
static char *flags_avp_param = NULL;
static char *defunct_gw_avp_param = NULL;
static char *lcr_id_avp_param = NULL;

/* max number of lcr instances */
unsigned int lcr_count_param = DEF_LCR_COUNT;

/* size of lcr rules hash table */
unsigned int lcr_rule_hash_size_param = DEF_LCR_RULE_HASH_SIZE;

/* max no of gws */
unsigned int lcr_gw_count_param = DEF_LCR_GW_COUNT;

/* can gws be defuncted */
static unsigned int defunct_capability_param = 0;

/* dont strip or tag param */
static int dont_strip_or_prefix_flag_param = -1;

/* ping related params */
unsigned int ping_interval_param = 0;
unsigned int ping_inactivate_threshold_param = 1;
str ping_valid_reply_codes_param = {"", 0};
str ping_socket_param = {"", 0};
str ping_from_param = {"sip:pinger@localhost", 20};

/*
 * Other module types and variables
 */

static int     gw_uri_avp_type;
static int_str gw_uri_avp;
static int     ruri_user_avp_type;
static int_str ruri_user_avp;
static int     tag_avp_type;
static int_str tag_avp;
static int     flags_avp_type;
static int_str flags_avp;
static int     defunct_gw_avp_type;
static int_str defunct_gw_avp;
static int     lcr_id_avp_type;
static int_str lcr_id_avp;

/* Pointer to rule hash table pointer table */
struct rule_info ***rule_pt = (struct rule_info ***)NULL;

/* Pointer to gw table pointer table */
struct gw_info **gw_pt = (struct gw_info **)NULL;

/* Pointer to rule_id info hash table */
struct rule_id_info **rule_id_hash_table = (struct rule_id_info **)NULL;

/* Pinging related vars */
struct tm_binds tmb;
void ping_timer(unsigned int ticks, void* param);
unsigned int ping_valid_reply_codes[MAX_NO_OF_REPLY_CODES];
str ping_method = {"OPTIONS", 7};
unsigned int ping_rc_count = 0;

/*
 * Functions that are defined later
 */
static void destroy(void);
static int mod_init(void);
static int child_init(int rank);
static void free_shared_memory(void);

static int load_gws(struct sip_msg* _m, int argc, action_u_t argv[]);
static int next_gw(struct sip_msg* _m, char* _s1, char* _s2);
static int inactivate_gw(struct sip_msg* _m, char* _s1, char* _s2);
static int defunct_gw(struct sip_msg* _m, char* _s1, char* _s2);
static int from_gw_1(struct sip_msg* _m, char* _s1, char* _s2);
static int from_gw_3(struct sip_msg* _m, char* _s1, char* _s2, char* _s3);
static int from_any_gw_0(struct sip_msg* _m, char* _s1, char* _s2);
static int from_any_gw_2(struct sip_msg* _m, char* _s1, char* _s2);
static int to_gw_1(struct sip_msg* _m, char* _s1, char* _s2);
static int to_gw_3(struct sip_msg* _m, char* _s1, char* _s2, char* _s3);
static int to_any_gw_0(struct sip_msg* _m, char* _s1, char* _s2);
static int to_any_gw_2(struct sip_msg* _m, char* _s1, char* _s2);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"load_gws", (cmd_function)load_gws, VAR_PARAM_NO, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"next_gw", (cmd_function)next_gw, 0, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"defunct_gw", (cmd_function)defunct_gw, 1, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"inactivate_gw", (cmd_function)inactivate_gw, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"from_gw", (cmd_function)from_gw_1, 1, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"from_gw", (cmd_function)from_gw_3, 3, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"from_any_gw", (cmd_function)from_any_gw_0, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"from_any_gw", (cmd_function)from_any_gw_2, 2, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_gw", (cmd_function)to_gw_1, 1, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_gw", (cmd_function)to_gw_3, 3, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_any_gw", (cmd_function)to_any_gw_0, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_any_gw", (cmd_function)to_any_gw_2, 2, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"db_url",                   PARAM_STR, &db_url},
    {"lcr_rule_table",           PARAM_STR, &lcr_rule_table},
    {"lcr_rule_target_table",    PARAM_STR, &lcr_rule_target_table},
    {"lcr_gw_table",             PARAM_STR, &lcr_gw_table},
    {"lcr_id_column",            PARAM_STR, &lcr_id_col},
    {"id_column",                PARAM_STR, &id_col},
    {"prefix_column",            PARAM_STR, &prefix_col},
    {"from_uri_column",          PARAM_STR, &from_uri_col},
    {"request_uri_column",       PARAM_STR, &request_uri_col},
    {"stopper_column",           PARAM_STR, &stopper_col},
    {"enabled_column",           PARAM_STR, &enabled_col},
    {"rule_id_column",           PARAM_STR, &rule_id_col},
    {"priority_column",          PARAM_STR, &priority_col},
    {"gw_id_column",             PARAM_STR, &gw_id_col},
    {"weight_column",            PARAM_STR, &weight_col},
    {"gw_name_column",           PARAM_STR, &gw_name_col},
    {"ip_addr_column",           PARAM_STR, &ip_addr_col},
    {"port_column",              PARAM_STR, &port_col},
    {"uri_scheme_column",        PARAM_STR, &uri_scheme_col},
    {"transport_column",         PARAM_STR, &transport_col},
    {"params_column",            PARAM_STR, &params_col},
    {"hostname_column",          PARAM_STR, &hostname_col},
    {"strip_column",             PARAM_STR, &strip_col},
    {"prefix_column",            PARAM_STR, &prefix_col},
    {"tag_column",               PARAM_STR, &tag_col},
    {"flags_column",             PARAM_STR, &flags_col},
    {"defunct_column",           PARAM_STR, &defunct_col},
    {"gw_uri_avp",               PARAM_STRING, &gw_uri_avp_param},
    {"ruri_user_avp",            PARAM_STRING, &ruri_user_avp_param},
    {"tag_avp",                  PARAM_STRING, &tag_avp_param},
    {"flags_avp",                PARAM_STRING, &flags_avp_param},
    {"defunct_capability",       INT_PARAM, &defunct_capability_param},
    {"defunct_gw_avp",           PARAM_STRING, &defunct_gw_avp_param},
    {"lcr_id_avp",               PARAM_STRING, &lcr_id_avp_param},
    {"lcr_count",                INT_PARAM, &lcr_count_param},
    {"lcr_rule_hash_size",       INT_PARAM, &lcr_rule_hash_size_param},
    {"lcr_gw_count",             INT_PARAM, &lcr_gw_count_param},
    {"dont_strip_or_prefix_flag",INT_PARAM, &dont_strip_or_prefix_flag_param},
    {"fetch_rows",               INT_PARAM, &fetch_rows_param},
    {"ping_interval",            INT_PARAM, &ping_interval_param},
    {"ping_inactivate_threshold",  INT_PARAM, &ping_inactivate_threshold_param},
    {"ping_valid_reply_codes",   PARAM_STR, &ping_valid_reply_codes_param},
    {"ping_from",                PARAM_STR, &ping_from_param},
    {"ping_socket",              PARAM_STR, &ping_socket_param},
    {0, 0, 0}
};

/*
 * Module interface
 */
struct module_exports exports = {
	"lcr", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	0,         /* exported statistics */
	0,         /* exported MI functions */
	0,         /* exported pseudo-variables */
	0,         /* extra processes */
	mod_init,  /* module initialization function */
	0,         /* response function */
	destroy,   /* destroy function */
	child_init /* child initialization function */
};

static int lcr_db_init(const str* db_url)
{	
	if (lcr_dbf.init==0){
		LM_CRIT("null lcr_dbf\n");
		goto error;
	}
	if (dbh) {
	    LM_ERR("database is already connected\n");
	    goto error;
	}
	dbh=lcr_dbf.init(db_url);
	if (dbh==0){
		LM_ERR("unable to connect to the database\n");
		goto error;
	}
	return 0;
error:
	return -1;
}



static int lcr_db_bind(const str* db_url)
{
    if (db_bind_mod(db_url, &lcr_dbf)<0){
	LM_ERR("unable to bind to the database module\n");
	return -1;
    }

    if (!DB_CAPABILITY(lcr_dbf, DB_CAP_QUERY)) {
	LM_ERR("database module does not implement 'query' function\n");
	return -1;
    }

    return 0;
}


static void lcr_db_close(void)
{
	if (dbh && lcr_dbf.close){
		lcr_dbf.close(dbh);
		dbh=0;
	}
}


/*
 * Module initialization function that is called before the main process forks
 */
static int mod_init(void)
{
    pv_spec_t *avp_spec;
    str s;
    unsigned short avp_flags;
    unsigned int i;
    char *at, *past, *sep;

    /* Register RPC commands */
    if (rpc_register_array(lcr_rpc)!=0) {
	LM_ERR("failed to register RPC commands\n");
	return -1;
    }

    /* Bind database */
    if (lcr_db_bind(&db_url)) {
	LM_ERR("no database module found\n");
	return -1;
    }

    /* Check values of some params */
    if (lcr_count_param < 1) {
	LM_ERR("invalid lcr_count module parameter value <%d>\n",
	       lcr_count_param);
	return -1;
    }
    if (lcr_rule_hash_size_param < 1) {
	LM_ERR("invalid lcr_rule_hash_size module parameter value <%d>\n",
	       lcr_rule_hash_size_param);
	return -1;
    }
    if (lcr_gw_count_param < 1) {
	LM_ERR("invalid lcr_gw_count module parameter value <%d>\n",
	       lcr_gw_count_param);
	return -1;
    }
    if ((dont_strip_or_prefix_flag_param != -1) &&
	!flag_in_range(dont_strip_or_prefix_flag_param)) {
	LM_ERR("invalid dont_strip_or_prefix_flag value <%d>\n",
	       dont_strip_or_prefix_flag_param);
	return -1;
    }

    /* Process AVP params */

    if (gw_uri_avp_param && *gw_uri_avp_param) {
	s.s = gw_uri_avp_param; s.len = strlen(s.s);
    avp_spec = pv_cache_get(&s);
	if (avp_spec==NULL|| avp_spec->type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n", gw_uri_avp_param);
	    return -1;
	}

	if (pv_get_avp_name(0, &(avp_spec->pvp), &gw_uri_avp, &avp_flags) != 0) {
	    LM_ERR("invalid AVP definition <%s>\n", gw_uri_avp_param);
	    return -1;
	}
	gw_uri_avp_type = avp_flags;
    } else {
	LM_ERR("AVP gw_uri_avp has not been defined\n");
	return -1;
    }

    if (ruri_user_avp_param && *ruri_user_avp_param) {
	s.s = ruri_user_avp_param; s.len = strlen(s.s);
    avp_spec = pv_cache_get(&s);
	if (avp_spec==NULL || avp_spec->type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n",
		   ruri_user_avp_param);
	    return -1;
	}

	if (pv_get_avp_name(0, &(avp_spec->pvp), &ruri_user_avp, &avp_flags)
	    != 0) {
	    LM_ERR("invalid AVP definition <%s>\n", ruri_user_avp_param);
	    return -1;
	}
	ruri_user_avp_type = avp_flags;
    } else {
	LM_ERR("AVP ruri_user_avp has not been defined\n");
	return -1;
    }

    if (tag_avp_param) {
	s.s = tag_avp_param; s.len = strlen(s.s);
    avp_spec = pv_cache_get(&s);
	if (avp_spec==NULL || (avp_spec->type!=PVT_AVP)) {
	    LM_ERR("malformed or non AVP definition <%s>\n", tag_avp_param);
	    return -1;
	}
	if (pv_get_avp_name(0, &(avp_spec->pvp), &tag_avp, &avp_flags) != 0) {
	    LM_ERR("invalid AVP definition <%s>\n", tag_avp_param);
	    return -1;
	}
	tag_avp_type = avp_flags | AVP_VAL_STR;
    }

    if (flags_avp_param) {
	s.s = flags_avp_param; s.len = strlen(s.s);
    avp_spec = pv_cache_get(&s);
	if (avp_spec==NULL || (avp_spec->type != PVT_AVP)) {
	    LM_ERR("malformed or non AVP definition <%s>\n", flags_avp_param);
	    return -1;
	}
	if (pv_get_avp_name(0, &(avp_spec->pvp), &flags_avp, &avp_flags) != 0) {
	    LM_ERR("invalid AVP definition <%s>\n", flags_avp_param);
	    return -1;
	}
	flags_avp_type = avp_flags;
    }

    if ((ping_interval_param != 0) && (ping_interval_param < 10)) {
	LM_ERR("invalid ping_interval value '%u'\n", ping_interval_param);
	return -1;
    }

    if ((defunct_capability_param > 0) || (ping_interval_param > 0)) {
	if (defunct_gw_avp_param && *defunct_gw_avp_param) {
	    s.s = defunct_gw_avp_param; s.len = strlen(s.s);
	    avp_spec = pv_cache_get(&s);
	    if (avp_spec==NULL || (avp_spec->type != PVT_AVP)) {
		LM_ERR("malformed or non AVP definition <%s>\n",
		       defunct_gw_avp_param);
		return -1;
	    }
	    if (pv_get_avp_name(0, &(avp_spec->pvp), &defunct_gw_avp,
				&avp_flags) != 0) {
		LM_ERR("invalid AVP definition <%s>\n", defunct_gw_avp_param);
		return -1;
	    }
	    defunct_gw_avp_type = avp_flags;
	} else {
	    LM_ERR("AVP defunct_gw_avp has not been defined\n");
	    return -1;
	}
	if (lcr_id_avp_param && *lcr_id_avp_param) {
	    s.s = lcr_id_avp_param; s.len = strlen(s.s);
	    avp_spec = pv_cache_get(&s);
	    if (avp_spec==NULL || (avp_spec->type != PVT_AVP)) {
		LM_ERR("malformed or non AVP definition <%s>\n",
		       lcr_id_avp_param);
		return -1;
	    }
	    if (pv_get_avp_name(0, &(avp_spec->pvp), &lcr_id_avp,
				&avp_flags) != 0) {
		LM_ERR("invalid AVP definition <%s>\n", lcr_id_avp_param);
		return -1;
	    }
	    lcr_id_avp_type = avp_flags;
	} else {
	    LM_ERR("AVP lcr_id_avp has not been defined\n");
	    return -1;
	}
	if (ping_interval_param > 0) {
	    if (load_tm_api(&tmb) == -1) {
		LM_ERR("could not bind tm api\n");
		return -1;
	    }
	    if ((ping_inactivate_threshold_param < 1) ||
		(ping_inactivate_threshold_param > 32)) {
		LM_ERR("invalid ping_inactivate_threshold value '%u'\n",
		       ping_inactivate_threshold_param);
		return -1;
	    }
	    /* ping reply codes */
	    at = ping_valid_reply_codes_param.s;
	    past = ping_valid_reply_codes_param.s +
		ping_valid_reply_codes_param.len;
	    while (at < past) {
		sep = index(at, ',');
		s.s = at;
		if (sep == NULL) {
		    s.len = past - at;
		    at = past;
		} else {
		    s.len = sep - at;
		    at = sep + 1;
		}
		if (ping_rc_count > MAX_NO_OF_REPLY_CODES - 1) {
		    LM_ERR("more than %u ping reply codes\n",
			   MAX_NO_OF_REPLY_CODES);
		    goto err;
		}
		if ((str2int(&s, &(ping_valid_reply_codes[ping_rc_count]))
		     != 0) ||
		    (ping_valid_reply_codes[ping_rc_count] < 100) ||
		    (ping_valid_reply_codes[ping_rc_count] > 999)) {
		    LM_ERR("invalid ping_valid_reply_codes code '%.*s'\n",
			   s.len, s.s);
		    return -1;
		}
		ping_rc_count++;
	    }
	    register_timer(ping_timer, NULL, ping_interval_param);
	}
    }

    if (fetch_rows_param < 1) {
	LM_ERR("invalid fetch_rows module parameter value <%d>\n",
	       fetch_rows_param);
	return -1;
    }

    /* Check table version */
    if (lcr_db_init(&db_url) < 0) {
	LM_ERR("unable to open database connection\n");
	return -1;
    }
    if ((db_check_table_version(&lcr_dbf, dbh, &lcr_rule_table,
				LCR_RULE_TABLE_VERSION) < 0) ||
	(db_check_table_version(&lcr_dbf, dbh, &lcr_rule_target_table,
				LCR_RULE_TARGET_TABLE_VERSION) < 0) ||
	(db_check_table_version(&lcr_dbf, dbh, &lcr_gw_table,
				LCR_GW_TABLE_VERSION) < 0)) {
	LM_ERR("error during table version check\n");
	lcr_db_close();
	goto err;
    }
    lcr_db_close();

    /* rule shared memory */

    /* rule hash table pointer table */
    /* pointer at index 0 points to temp rule hash table */
    rule_pt = (struct rule_info ***)shm_malloc(sizeof(struct rule_info **) *
					       (lcr_count_param + 1));
    if (rule_pt == 0) {
	LM_ERR("no memory for rule hash table pointer table\n");
	goto err;
    }
    memset(rule_pt, 0, sizeof(struct rule_info **) * (lcr_count_param + 1));

    /* rules hash tables */
    /* last entry in hash table contains list of different prefix lengths */
    for (i = 0; i <= lcr_count_param; i++) {
	rule_pt[i] = (struct rule_info **)
	    shm_malloc(sizeof(struct rule_info *) *
		       (lcr_rule_hash_size_param + 1));
	if (rule_pt[i] == 0) {
	    LM_ERR("no memory for rules hash table\n");
	    goto err;
	}
	memset(rule_pt[i], 0, sizeof(struct rule_info *) *
	       (lcr_rule_hash_size_param + 1));
    }
    /* gw shared memory */

    /* gw table pointer table */
    /* pointer at index 0 points to temp rule hash table */
    gw_pt = (struct gw_info **)shm_malloc(sizeof(struct gw_info *) *
					  (lcr_count_param + 1));
    if (gw_pt == 0) {
	LM_ERR("no memory for gw table pointer table\n");
	goto err;
    }
    memset(gw_pt, 0, sizeof(struct gw_info *) * (lcr_count_param + 1));

    /* gw tables themselves */
    /* ordered by ip_addr for from_gw/to_gw functions */
    /* in each table i, (gw_pt[i])[0].ip_addr contains number of
       gateways in the table and (gw_pt[i])[0].port has value 1
       if some gateways in the table have null ip addr. */
    for (i = 0; i <= lcr_count_param; i++) {
	gw_pt[i] = (struct gw_info *)shm_malloc(sizeof(struct gw_info) *
						(lcr_gw_count_param + 1));
	if (gw_pt[i] == 0) {
	    LM_ERR("no memory for gw table\n");
	    goto err;
	}
	memset(gw_pt[i], 0, sizeof(struct gw_info *) *
	       (lcr_gw_count_param + 1));
    }

    /* Allocate and initialize locks */
    reload_lock = lock_alloc();
    if (reload_lock == NULL) {
	LM_ERR("cannot allocate reload_lock\n");
	goto err;
    }
    if (lock_init(reload_lock) == NULL) {
	LM_ERR("cannot init reload_lock\n");
	goto err;
    }

    /* First reload */
    lock_get(reload_lock);
    if (reload_tables() < 0) {
	lock_release(reload_lock);
	LM_CRIT("failed to reload lcr tables\n");
	goto err;
    }
    lock_release(reload_lock);

    return 0;

err:
    free_shared_memory();
    return -1;
}


/* Module initialization function called in each child separately */
static int child_init(int rank)
{
    return 0;
}


static void destroy(void)
{
    lcr_db_close();

    free_shared_memory();
}


/* Free shared memory */
static void free_shared_memory(void)
{
    int i;

    for (i = 0; i <= lcr_count_param; i++) {
	if (rule_pt && rule_pt[i]) {
	    rule_hash_table_contents_free(rule_pt[i]);
	    shm_free(rule_pt[i]);
	    rule_pt[i] = 0;
	}
    }
    if (rule_pt) {
	shm_free(rule_pt);
	rule_pt = 0;
    }
    for (i = 0; i <= lcr_count_param; i++) {
	if (gw_pt && gw_pt[i]) {
	    shm_free(gw_pt[i]);
	    gw_pt[i] = 0;
	}
    }
    if (gw_pt) {
	shm_free(gw_pt);
	gw_pt = 0;
    }
    if (reload_lock) {
	lock_destroy(reload_lock);
	lock_dealloc(reload_lock);
	reload_lock=0;
    }
}
   
/*
 * Compare matched gateways based on prefix_len, priority, and randomized
 * weight.
 */
static int comp_matched(const void *m1, const void *m2)
{
    struct matched_gw_info *mi1 = (struct matched_gw_info *) m1;
    struct matched_gw_info *mi2 = (struct matched_gw_info *) m2;

    /* Sort by prefix_len */
    if (mi1->prefix_len > mi2->prefix_len) return 1;
    if (mi1->prefix_len == mi2->prefix_len) {
	/* Sort by priority */
	if (mi1->priority < mi2->priority) return 1;
	if (mi1->priority == mi2->priority) {
	    /* Sort by randomized weigth */
	    if (mi1->weight > mi2->weight) return 1;
	    if (mi1->weight == mi2->weight) return 0;
	    return -1;
	}
	return -1;
    }
    return -1;
}


/* Compile pattern into shared memory and return pointer to it. */
static pcre *reg_ex_comp(const char *pattern)
{
    pcre *re, *result;
    const char *error;
    int rc, err_offset;
    size_t size;

    re = pcre_compile(pattern, 0, &error, &err_offset, NULL);
    if (re == NULL) {
	LM_ERR("pcre compilation of '%s' failed at offset %d: %s\n",
	       pattern, err_offset, error);
	return (pcre *)0;
    }
    rc = pcre_fullinfo(re, NULL, PCRE_INFO_SIZE, &size);
    if (rc != 0) {
	LM_ERR("pcre_fullinfo on compiled pattern '%s' yielded error: %d\n",
	       pattern, rc);
	return (pcre *)0;
    }
    result = (pcre *)shm_malloc(size);
    if (result == NULL) {
	pcre_free(re);
	LM_ERR("not enough shared memory for compiled PCRE pattern\n");
	return (pcre *)0;
    }
    memcpy(result, re, size);
    pcre_free(re);
    return result;
}


/*
 * Compare gateways based on their IP address
 */
static int comp_gws(const void *_g1, const void *_g2)
{
    struct gw_info *g1 = (struct gw_info *)_g1;
    struct gw_info *g2 = (struct gw_info *)_g2;

    if (g1->ip_addr.af < g2->ip_addr.af)   return -1;
    if (g1->ip_addr.af > g2->ip_addr.af)   return  1;
    if (g1->ip_addr.len < g2->ip_addr.len) return -1;
    if (g1->ip_addr.len > g2->ip_addr.len) return  1;
    return memcmp(g1->ip_addr.u.addr, g2->ip_addr.u.addr, g1->ip_addr.len);
}


/* 
 * Insert gw info into index i or gws table
 */
static int insert_gw(struct gw_info *gws, unsigned int i, unsigned int gw_id,
		     char *gw_name, unsigned int gw_name_len,
		     char *scheme, unsigned int scheme_len,
		     struct ip_addr *ip_addr, unsigned int port,
		     uri_transport transport_code, 
		     char *transport, unsigned int transport_len,
		     char *params, unsigned int params_len,
		     char *hostname, unsigned int hostname_len,
		     char *ip_string, unsigned int strip, char *prefix,
		     unsigned int prefix_len, char *tag, unsigned int tag_len,
		     unsigned int flags, unsigned int defunct_until)
{
    char *at, *string;
    int len;

    gws[i].gw_id = gw_id;
    if (gw_name_len) memcpy(&(gws[i].gw_name[0]), gw_name, gw_name_len);
    gws[i].gw_name_len = gw_name_len;
    memcpy(&(gws[i].scheme[0]), scheme, scheme_len);
    gws[i].scheme_len = scheme_len;
    gws[i].ip_addr = *ip_addr;
    gws[i].port = port;
    gws[i].transport_code = transport_code;
    if (transport_len) memcpy(&(gws[i].transport[0]), transport, transport_len);
    gws[i].transport_len = transport_len;
    if (params_len) memcpy(&(gws[i].params[0]), params, params_len);
    gws[i].params_len = params_len;
    if (hostname_len) memcpy(&(gws[i].hostname[0]), hostname, hostname_len);
    gws[i].hostname_len = hostname_len;
    gws[i].strip = strip;
    gws[i].prefix_len = prefix_len;
    if (prefix_len) memcpy(&(gws[i].prefix[0]), prefix, prefix_len);
    gws[i].tag_len = tag_len;
    if (tag_len) memcpy(&(gws[i].tag[0]), tag, tag_len);
    gws[i].flags = flags;
    gws[i].defunct_until = defunct_until;
    gws[i].state = 0;
    at = &(gws[i].uri[0]);
    append_str(at, scheme, scheme_len);
    if (ip_addr->af != 0) {
	string = ip_addr2a(ip_addr);
	len = strlen(string);
	if (ip_addr->af == AF_INET6) {
	    append_chr(at, '[');
	    append_str(at, string, len);
	    append_chr(at, ']');
	} else {
	    append_str(at, string, len);
	}
    } else {
	append_str(at, &(hostname[0]), hostname_len);
    }
    if (port > 0) {
	append_chr(at, ':');
	string = int2str(port, &len);
	append_str(at, string, len);
    }
    if (transport_len > 0) {
	append_str(at, transport, transport_len);
    }
    gws[i].uri_len = at - &(gws[i].uri[0]);
    LM_DBG("inserted gw <%u, %.*s, %.*s> at index %u\n", gw_id,
	   gw_name_len, gw_name, gws[i].uri_len, gws[i].uri, i);
    return 1;
}


/*
 * Insert prefix_len into list pointed by last rule hash table entry 
 * if not there already. Keep list in decending prefix_len order.
 */
static int prefix_len_insert(struct rule_info **table,
			     unsigned short prefix_len)
{
    struct rule_info *lcr_rec, **previous, *this;
    
    previous = &(table[lcr_rule_hash_size_param]);
    this = table[lcr_rule_hash_size_param];

    while (this) {
	if (this->prefix_len == prefix_len)
	    return 1;
	if (this->prefix_len < prefix_len) {
	    lcr_rec = shm_malloc(sizeof(struct rule_info));
	    if (lcr_rec == NULL) {
		LM_ERR("no shared memory for rule_info\n");
		return 0;
	    }
	    memset(lcr_rec, 0, sizeof(struct rule_info));
	    lcr_rec->prefix_len = prefix_len;
	    lcr_rec->next = this;
	    *previous = lcr_rec;
	    return 1;
	}
	previous = &(this->next);
	this = this->next;
    }

    lcr_rec = shm_malloc(sizeof(struct rule_info));
    if (lcr_rec == NULL) {
	LM_ERR("no shared memory for rule_info\n");
	return 0;
    }
    memset(lcr_rec, 0, sizeof(struct rule_info));
    lcr_rec->prefix_len = prefix_len;
    lcr_rec->next = NULL;
    *previous = lcr_rec;
    return 1;
}

static int insert_gws(db1_res_t *res, struct gw_info *gws,
		      unsigned int *null_gw_ip_addr,
		      unsigned int *gw_cnt)
{
    unsigned int i, gw_id, defunct_until, gw_name_len, port, params_len,
	hostname_len, strip, prefix_len, tag_len, flags, scheme_len,
	transport_len;
    char *gw_name, *params, *hostname, *prefix, *tag, *scheme, *transport;
    uri_transport transport_code;
    db_row_t* row;
    struct in_addr in_addr;
    struct ip_addr ip_addr, *ip_p;
    str ip_string;
    
    for (i = 0; i < RES_ROW_N(res); i++) {
	row = RES_ROWS(res) + i;
	if ((VAL_NULL(ROW_VALUES(row) + 12) == 1) ||
	    (VAL_TYPE(ROW_VALUES(row) + 12) != DB1_INT)) {
	    LM_ERR("lcr_gw id at row <%u> is null or not int\n", i);
	    return 0;
	}
	gw_id = (unsigned int)VAL_INT(ROW_VALUES(row) + 12);
	if (VAL_NULL(ROW_VALUES(row) + 11)) {
	    defunct_until = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 11) != DB1_INT) {
		LM_ERR("lcr_gw defunct at row <%u> is not int\n", i);
		return 0;
	    }
	    defunct_until = (unsigned int)VAL_INT(ROW_VALUES(row) + 11);
	    if (defunct_until > 4294967294UL) {
		LM_DBG("skipping disabled gw <%u>\n", gw_id);
		continue;
	    }
	}
	if (!VAL_NULL(ROW_VALUES(row)) &&
	    (VAL_TYPE(ROW_VALUES(row)) != DB1_STRING)) {
	    LM_ERR("lcr_gw gw_name at row <%u> is not null or string\n", i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row))) {
	    gw_name_len = 0;
	    gw_name = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row)) != DB1_STRING) {
		LM_ERR("lcr_gw gw_name at row <%u> is not string\n", i);
		return 0;
	    }
	    gw_name = (char *)VAL_STRING(ROW_VALUES(row));
	    gw_name_len = strlen(gw_name);
	}
	if (gw_name_len > MAX_NAME_LEN) {
	    LM_ERR("lcr_gw gw_name <%u> at row <%u> it too long\n",
		   gw_name_len, i);
	    return 0;
	}
	if (!VAL_NULL(ROW_VALUES(row) + 1) &&
	    (VAL_TYPE(ROW_VALUES(row) + 1) != DB1_STRING)) {
	    LM_ERR("lcr_gw ip_addr at row <%u> is not null or string\n",
		   i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row) + 1) ||
	    (strlen((char *)VAL_STRING(ROW_VALUES(row) + 1)) == 0)) {
	    ip_string.s = (char *)0;
	    ip_addr.af = 0;
	    ip_addr.len = 0;
	    *null_gw_ip_addr = 1;
	} else {
	    ip_string.s = (char *)VAL_STRING(ROW_VALUES(row) + 1);
	    ip_string.len = strlen(ip_string.s);
	    if ((ip_p = str2ip(&ip_string))) {
		/* 123.123.123.123 */
		ip_addr = *ip_p;
	    }
	    else if ((ip_p = str2ip6(&ip_string))) {
		/* fe80::123:4567:89ab:cdef and [fe80::123:4567:89ab:cdef] */
		ip_addr = *ip_p;
	    }
	    else if (inet_aton(ip_string.s, &in_addr) == 0) {
		/* backwards compatibility for integer or hex notations */
		ip_addr.u.addr32[0] = in_addr.s_addr;
		ip_addr.af = AF_INET;
		ip_addr.len = 4;
	    }
	    else {
		LM_ERR("lcr_gw ip_addr <%s> at row <%u> is invalid\n",
		       ip_string.s, i);
		return 0;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 2)) {
	    port = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 2) != DB1_INT) {
		LM_ERR("lcr_gw port at row <%u> is not int\n", i);
		return 0;
	    }
	    port = (unsigned int)VAL_INT(ROW_VALUES(row) + 2);
	}
	if (port > 65536) {
	    LM_ERR("lcr_gw port <%d> at row <%u> is too large\n", port, i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row) + 3)) {
	    scheme = "sip:";
	    scheme_len = 4;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 3) != DB1_INT) {
		LM_ERR("lcr_gw uri scheme at row <%u> is not int\n", i);
		return 0;
	    }
	    switch (VAL_INT(ROW_VALUES(row) + 3)) {
	    case SIP_URI_T:
		scheme = "sip:";
		scheme_len = 4;
		break;
	    case SIPS_URI_T:
		scheme = "sips:";
		scheme_len = 5;
		break;
	    default:
		LM_ERR("lcr_gw has unknown or unsupported URI scheme <%u> at "
		       "row <%u>\n", VAL_INT(ROW_VALUES(row) + 3), i);
		return 0;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 4)) {
	    transport_code = PROTO_NONE;
	    transport = "";
	    transport_len = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 4) != DB1_INT) {
		LM_ERR("lcr_gw transport at row <%u> is not int\n", i);
		return 0;
	    }
	    transport_code = (uri_transport)VAL_INT(ROW_VALUES(row) + 4);
	    switch (transport_code) {
	    case PROTO_UDP:
		transport = ";transport=udp";
		transport_len = 14;
		break;
	    case PROTO_TCP:
		transport = ";transport=tcp";
		transport_len = 14;
		break;
	    case PROTO_TLS:
		transport = ";transport=tls";
		transport_len = 14;
		break;
	    case PROTO_SCTP:
		transport = ";transport=sctp";
		transport_len = 15;
		break;
	    default:
		LM_ERR("lcr_gw has unknown or unsupported transport <%u> at "
		       " row <%u>\n", transport_code, i);
		return 0;
	    }
	}
	if ((VAL_INT(ROW_VALUES(row) + 3) == SIPS_URI_T) &&
	    (transport_code == PROTO_UDP)) {
	    LM_ERR("lcr_gw has wrong transport <%u> for SIPS URI "
		   "scheme at row <%u>\n", transport_code, i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row) + 5)) {
	    params_len = 0;
	    params = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 5) != DB1_STRING) {
		LM_ERR("lcr_gw params at row <%u> is not string\n", i);
		return 0;
	    }
	    params = (char *)VAL_STRING(ROW_VALUES(row) + 5);
	    params_len = strlen(params);
	    if ((params_len > 0) && (params[0] != ';')) {
		LM_ERR("lcr_gw params at row <%u> does not start "
		       "with ';'\n", i);
		return 0;
	    }
	}
	if (params_len > MAX_PARAMS_LEN) {
	    LM_ERR("lcr_gw params length <%u> at row <%u> it too large\n",
		   params_len, i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row) + 6)) {
	    if (ip_string.s == 0) {
		LM_ERR("lcr_gw gw ip_addr and hostname are both null "
		       "at row <%u>\n", i);
		return 0;
	    }
	    hostname_len = 0;
	    hostname = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 6) != DB1_STRING) {
		LM_ERR("hostname at row <%u> is not string\n", i);
		return 0;
	    }
	    hostname = (char *)VAL_STRING(ROW_VALUES(row) + 6);
	    hostname_len = strlen(hostname);
	}
	if (hostname_len > MAX_HOST_LEN) {
	    LM_ERR("lcr_gw hostname at row <%u> it too long\n", i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row) + 7)) {
	    strip = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 7) != DB1_INT) {
		LM_ERR("lcr_gw strip count at row <%u> is not int\n", i);
		return 0;
	    }
	    strip = (unsigned int)VAL_INT(ROW_VALUES(row) + 7);
	}
	if (strip > MAX_USER_LEN) {
	    LM_ERR("lcr_gw strip count <%u> at row <%u> it too large\n",
		   strip, i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row) + 8)) {
	    prefix_len = 0;
	    prefix = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 8) != DB1_STRING) {
		LM_ERR("lcr_gw prefix at row <%u> is not string\n", i);
		return 0;
	    }
	    prefix = (char *)VAL_STRING(ROW_VALUES(row) + 8);
	    prefix_len = strlen(prefix);
	}
	if (prefix_len > MAX_PREFIX_LEN) {
	    LM_ERR("lcr_gw prefix at row <%u> it too long\n", i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row) + 9)) {
	    tag_len = 0;
	    tag = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 9) != DB1_STRING) {
		LM_ERR("lcr_gw tag at row <%u> is not string\n", i);
		return 0;
	    }
	    tag = (char *)VAL_STRING(ROW_VALUES(row) + 9);
	    tag_len = strlen(tag);
	}
	if (tag_len > MAX_TAG_LEN) {
	    LM_ERR("lcr_gw tag at row <%u> it too long\n", i);
	    return 0;
	}
	if (VAL_NULL(ROW_VALUES(row) + 10)) {
	    flags = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 10) != DB1_INT) {
		LM_ERR("lcr_gw flags at row <%u> is not int\n", i);
		return 0;
	    }
	    flags = (unsigned int)VAL_INT(ROW_VALUES(row) + 10);
	}
	(*gw_cnt)++;
	if (!insert_gw(gws, *gw_cnt, gw_id, gw_name, gw_name_len,
		       scheme, scheme_len, &ip_addr, port,
		       transport_code, transport, transport_len,
		       params, params_len, hostname,
		       hostname_len, ip_string.s, strip, prefix, prefix_len,
		       tag, tag_len, flags, defunct_until)) {
	    return 0;
	}
    }
    return 1;
}


/*
 * Reload gws to unused gw table, rules to unused lcr hash table, and
 * prefix lens to a new prefix_len list.  When done, make these tables
 * and list the current ones.
 */
int reload_tables()
{
    unsigned int i, n, lcr_id, rule_id, gw_id, from_uri_len, request_uri_len,
	stopper, prefix_len, enabled, gw_cnt, null_gw_ip_addr, priority,
	weight, tmp;
    char *prefix, *from_uri, *request_uri;
    db1_res_t* res = NULL;
    db_row_t* row;
    db_key_t key_cols[1];
    db_op_t op[1];
    db_val_t vals[1];
    db_key_t gw_cols[13];
    db_key_t rule_cols[6];
    db_key_t target_cols[4];
    pcre *from_uri_re, *request_uri_re;
    struct gw_info *gws, *gw_pt_tmp;
    struct rule_info **rules, **rule_pt_tmp;

    key_cols[0] = &lcr_id_col;
    op[0] = OP_EQ;
    VAL_TYPE(vals) = DB1_INT;
    VAL_NULL(vals) = 0;

    rule_cols[0] = &id_col;
    rule_cols[1] = &prefix_col;
    rule_cols[2] = &from_uri_col;
    rule_cols[3] = &stopper_col;
    rule_cols[4] = &enabled_col;
    rule_cols[5] = &request_uri_col;
	
    gw_cols[0] = &gw_name_col;
    gw_cols[1] = &ip_addr_col;
    gw_cols[2] = &port_col;
    gw_cols[3] = &uri_scheme_col;
    gw_cols[4] = &transport_col;
    gw_cols[5] = &params_col;
    gw_cols[6] = &hostname_col;
    gw_cols[7] = &strip_col;
    gw_cols[8] = &prefix_col;
    gw_cols[9] = &tag_col;
    gw_cols[10] = &flags_col;
    gw_cols[11] = &defunct_col;
    gw_cols[12] = &id_col;

    target_cols[0] = &rule_id_col;
    target_cols[1] = &gw_id_col;
    target_cols[2] = &priority_col;
    target_cols[3] = &weight_col;

    request_uri_re = from_uri_re = 0;

    if (lcr_db_init(&db_url) < 0) {
	LM_ERR("unable to open database connection\n");
	return -1;
    }

    rule_id_hash_table = pkg_malloc(sizeof(struct rule_id_info *) *
				    lcr_rule_hash_size_param);
    if (!rule_id_hash_table) {
	LM_ERR("no pkg memory for rule_id hash table\n");
	goto err;
    }
    memset(rule_id_hash_table, 0, sizeof(struct rule_id_info *) *
	   lcr_rule_hash_size_param);

    for (lcr_id = 1; lcr_id <= lcr_count_param; lcr_id++) {

	/* Reload rules */

	rules = rule_pt[0];
	rule_hash_table_contents_free(rules);
	rule_id_hash_table_contents_free();
	
	if (lcr_dbf.use_table(dbh, &lcr_rule_table) < 0) {
	    LM_ERR("error while trying to use lcr_rule table\n");
	    goto err;
	}

	VAL_INT(vals) = lcr_id;
	if (DB_CAPABILITY(lcr_dbf, DB_CAP_FETCH)) {
	    if (lcr_dbf.query(dbh, key_cols, op, vals, rule_cols, 1, 6, 0, 0)
		< 0) {
		LM_ERR("db query on lcr_rule table failed\n");
		goto err;
	    }
	    if (lcr_dbf.fetch_result(dbh, &res, fetch_rows_param) < 0) {
		LM_ERR("failed to fetch rows from lcr_rule table\n");
		goto err;
	    }
	} else {
	    if (lcr_dbf.query(dbh, key_cols, op, vals, rule_cols, 1, 6, 0, &res)
		< 0) {
		LM_ERR("db query on lcr_rule table failed\n");
		goto err;
	    }
	}

	n = 0;
	request_uri_re = from_uri_re = 0;
    
	do {
	    LM_DBG("loading, cycle %d with <%d> rows\n", n++, RES_ROW_N(res));
	    for (i = 0; i < RES_ROW_N(res); i++) {

		request_uri_re = from_uri_re = 0;
		row = RES_ROWS(res) + i;

		if ((VAL_NULL(ROW_VALUES(row)) == 1) ||
		    (VAL_TYPE(ROW_VALUES(row)) != DB1_INT)) {
		    LM_ERR("lcr rule id at row <%u> is null or not int\n", i);
		    goto err;
		}
		rule_id = (unsigned int)VAL_INT(ROW_VALUES(row));

		enabled = (unsigned int)VAL_INT(ROW_VALUES(row) + 4);
		if ((enabled != 0) && (enabled != 1)) {
		    LM_ERR("lcr rule <%u> enabled is not 0 or 1\n", rule_id);
		    goto err;
		}
		if (enabled == 0) {
		    LM_DBG("skipping disabled lcr rule <%u>\n", rule_id);
		    continue;
		}

		if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
		    prefix_len = 0;
		    prefix = 0;
		} else {
		    if (VAL_TYPE(ROW_VALUES(row) + 1) != DB1_STRING) {
			LM_ERR("lcr rule <%u> prefix is not string\n", rule_id);
			goto err;
		    }
		    prefix = (char *)VAL_STRING(ROW_VALUES(row) + 1);
		    prefix_len = strlen(prefix);
		}
		if (prefix_len > MAX_PREFIX_LEN) {
		    LM_ERR("lcr rule <%u> prefix is too long\n", rule_id);
		    goto err;
		}

		if ((VAL_NULL(ROW_VALUES(row) + 3) == 1) ||
		    (VAL_TYPE(ROW_VALUES(row) + 3) != DB1_INT)) {
		    LM_ERR("lcr rule <%u> stopper is NULL or not int\n",
			   rule_id);
		    goto err;
		}
		stopper = (unsigned int)VAL_INT(ROW_VALUES(row) + 3);
		if ((stopper != 0) && (stopper != 1)) {
		    LM_ERR("lcr rule <%u> stopper is not 0 or 1\n", rule_id);
		    goto err;
		}

		if ((VAL_NULL(ROW_VALUES(row) + 4) == 1) ||
		    (VAL_TYPE(ROW_VALUES(row) + 4) != DB1_INT)) {
		    LM_ERR("lcr rule <%u> enabled is NULL or not int\n",
			   rule_id);
		    goto err;
		}

		if (VAL_NULL(ROW_VALUES(row) + 2) == 1) {
		    from_uri_len = 0;
		    from_uri = 0;
		} else {
		    if (VAL_TYPE(ROW_VALUES(row) + 2) != DB1_STRING) {
			LM_ERR("lcr rule <%u> from_uri is not string\n",
			       rule_id);
			goto err;
		    }
		    from_uri = (char *)VAL_STRING(ROW_VALUES(row) + 2);
		    from_uri_len = strlen(from_uri);
		}
		if (from_uri_len > MAX_URI_LEN) {
		    LM_ERR("lcr rule <%u> from_uri is too long\n", rule_id);
		    goto err;
		}
		if (from_uri_len > 0) {
		    from_uri_re = reg_ex_comp(from_uri);
		    if (from_uri_re == 0) {
			LM_ERR("failed to compile lcr rule <%u> from_uri "
			       "<%s>\n", rule_id, from_uri);
			goto err;
		    }
		} else {
		    from_uri_re = 0;
		}

		if (VAL_NULL(ROW_VALUES(row) + 5) == 1) {
		    request_uri_len = 0;
		    request_uri = 0;
		} else {
		    if (VAL_TYPE(ROW_VALUES(row) + 5) != DB1_STRING) {
			LM_ERR("lcr rule <%u> request_uri is not string\n",
			       rule_id);
			goto err;
		    }
		    request_uri = (char *)VAL_STRING(ROW_VALUES(row) + 5);
		    request_uri_len = strlen(request_uri);
		}
		if (request_uri_len > MAX_URI_LEN) {
		    LM_ERR("lcr rule <%u> request_uri is too long\n", rule_id);
		    goto err;
		}
		if (request_uri_len > 0) {
		    request_uri_re = reg_ex_comp(request_uri);
		    if (request_uri_re == 0) {
			LM_ERR("failed to compile lcr rule <%u> request_uri "
			       "<%s>\n", rule_id, request_uri);
			goto err;
		    }
		} else {
		    request_uri_re = 0;
		}

		if (!rule_hash_table_insert(rules, lcr_id, rule_id, prefix_len,
					    prefix, from_uri_len, from_uri,
					    from_uri_re, request_uri_len,
					    request_uri, request_uri_re, stopper) ||
		    !prefix_len_insert(rules, prefix_len)) {
		    goto err;
		}
	    }

	    if (DB_CAPABILITY(lcr_dbf, DB_CAP_FETCH)) {
		if (lcr_dbf.fetch_result(dbh, &res, fetch_rows_param) < 0) {
		    LM_ERR("fetching of rows from lcr_rule table failed\n");
		    goto err;
		}
	    } else {
		break;
	    }

	} while (RES_ROW_N(res) > 0);

	lcr_dbf.free_result(dbh, res);
	res = NULL;

	/* Reload gws */

	gws = gw_pt[0];

	if (lcr_dbf.use_table(dbh, &lcr_gw_table) < 0) {
	    LM_ERR("error while trying to use lcr_gw table\n");
	    goto err;
	}

	VAL_INT(vals) = lcr_id;
	if (lcr_dbf.query(dbh, key_cols, op, vals, gw_cols, 1, 13, 0, &res)
	    < 0) {
	    LM_ERR("failed to query gw data\n");
	    goto err;
	}

	if (RES_ROW_N(res) + 1 > lcr_gw_count_param) {
	    LM_ERR("too many gateways\n");
	    goto err;
	}

	null_gw_ip_addr = gw_cnt = 0;

	if (!insert_gws(res, gws, &null_gw_ip_addr, &gw_cnt)) goto err;

	lcr_dbf.free_result(dbh, res);
	res = NULL;

	VAL_INT(vals) = 0;
	if (lcr_dbf.query(dbh, key_cols, op, vals, gw_cols, 1, 13, 0, &res)
	    < 0) {
	    LM_ERR("failed to query gw data\n");
	    goto err;
	}

	if (RES_ROW_N(res) + 1 + gw_cnt > lcr_gw_count_param) {
	    LM_ERR("too many gateways\n");
	    goto err;
	}

	if (!insert_gws(res, gws, &null_gw_ip_addr, &gw_cnt)) goto err;

	lcr_dbf.free_result(dbh, res);
	res = NULL;

	qsort(&(gws[1]), gw_cnt, sizeof(struct gw_info), comp_gws);
	gws[0].ip_addr.u.addr32[0] = gw_cnt;
	gws[0].port = null_gw_ip_addr;

	/* Reload targets */

	if (lcr_dbf.use_table(dbh, &lcr_rule_target_table) < 0) {
	    LM_ERR("error while trying to use lcr_rule_target table\n");
	    goto err;
	}

	VAL_INT(vals) = lcr_id;
	if (DB_CAPABILITY(lcr_dbf, DB_CAP_FETCH)) {
	    if (lcr_dbf.query(dbh, key_cols, op, vals, target_cols, 1, 4, 0, 0)
		< 0) {
		LM_ERR("db query on lcr_rule_target table failed\n");
		goto err;
	    }
	    if (lcr_dbf.fetch_result(dbh, &res, fetch_rows_param) < 0) {
		LM_ERR("failed to fetch rows from lcr_rule_target table\n");
		goto err;
	    }
	} else {
	    if (lcr_dbf.query(dbh, key_cols, op, vals, target_cols, 1, 4, 0,
			      &res) < 0) {
		LM_ERR("db query on lcr_rule_target table failed\n");
		goto err;
	    }
	}

	n = 0;
	do {
	    LM_DBG("loading, cycle %d with <%d> rows\n", n++, RES_ROW_N(res));
	    for (i = 0; i < RES_ROW_N(res); i++) {
		row = RES_ROWS(res) + i;
		if ((VAL_NULL(ROW_VALUES(row)) == 1) ||
		    (VAL_TYPE(ROW_VALUES(row)) != DB1_INT)) {
		    LM_ERR("lcr_rule_target rule_id at row <%u> is null "
			   "or not int\n", i);
		    goto err;
		}
		rule_id = (unsigned int)VAL_INT(ROW_VALUES(row));
		if ((VAL_NULL(ROW_VALUES(row) + 1) == 1) ||
		    (VAL_TYPE(ROW_VALUES(row) + 1) != DB1_INT)) {
		    LM_ERR("lcr_rule_target gw_id at row <%u> is null "
			   "or not int\n", i);
		    goto err;
		}
		gw_id = (unsigned int)VAL_INT(ROW_VALUES(row) + 1);
		if ((VAL_NULL(ROW_VALUES(row) + 2) == 1) ||
		    (VAL_TYPE(ROW_VALUES(row) + 2) != DB1_INT)) {
		    LM_ERR("lcr_rule_target priority at row <%u> is null "
			   "or not int\n", i);
		    goto err;
		}
		priority = (unsigned int)VAL_INT(ROW_VALUES(row) + 2);
		if (priority > 255) {
		    LM_ERR("lcr_rule_target priority value at row <%u> is "
			   "not 0-255\n", i);
		    goto err;
		}
		if ((VAL_NULL(ROW_VALUES(row) + 3) == 1) ||
		    (VAL_TYPE(ROW_VALUES(row) + 3) != DB1_INT)) {
		    LM_ERR("lcr_rule_target weight at row <%u> is null "
			   "or not int\n", i);
		    goto err;
		}
		weight = (unsigned int)VAL_INT(ROW_VALUES(row) + 3);
		if ((weight < 1) || (weight > 254)) {
		    LM_ERR("lcr_rule_target weight value at row <%u> is "
			   "not 1-254\n", i);
		    goto err;
		}
		tmp = rule_hash_table_insert_target(rules, gws, rule_id, gw_id,
						    priority, weight);
		if (tmp == 2) {
		    LM_INFO("skipping disabled <gw/rule> = <%u/%u>\n",
			    gw_id, rule_id);
		} else if (tmp != 1) {
		    LM_ERR("could not insert target to rule <%u>\n", rule_id);
		    goto err;
		}
	    }
	    if (DB_CAPABILITY(lcr_dbf, DB_CAP_FETCH)) {
		if (lcr_dbf.fetch_result(dbh, &res, fetch_rows_param) < 0) {
		    LM_ERR("fetching of rows from lcr_rule_target table "
			   "failed\n");
		    goto err;
		}
	    } else {
		break;
	    }
	} while (RES_ROW_N(res) > 0);

	lcr_dbf.free_result(dbh, res);
	res = NULL;

	/* Swap tables */
	rule_pt_tmp = rule_pt[lcr_id];
	gw_pt_tmp = gw_pt[lcr_id];
	rule_pt[lcr_id] = rules;
	gw_pt[lcr_id] = gws;
	rule_pt[0] = rule_pt_tmp;
	gw_pt[0] = gw_pt_tmp;
    }

    lcr_db_close();
    rule_id_hash_table_contents_free();
    if (rule_id_hash_table) pkg_free(rule_id_hash_table);
    return 1;

 err:
    lcr_dbf.free_result(dbh, res);
    lcr_db_close();
    rule_id_hash_table_contents_free();
    if (rule_id_hash_table) pkg_free(rule_id_hash_table);
    return -1;
}


static inline int encode_avp_value(char *value, unsigned int gw_index,
			    char *scheme, unsigned int scheme_len,
			    unsigned int strip,
			    char *prefix, unsigned int prefix_len,
			    char *tag, unsigned int tag_len,
			    struct ip_addr *ip_addr, char *hostname, 
			    unsigned int hostname_len, unsigned int port,
			    char *params, unsigned int params_len,
			    char *transport, unsigned int transport_len,
			    unsigned int flags)
{
    char *at, *string;
    int len;

    at = value;

    /* gw index */
    string = int2str(gw_index, &len);
    append_str(at, string, len);
    append_chr(at, '|');
    /* scheme */
    append_str(at, scheme, scheme_len);
    append_chr(at, '|');
    /* strip */
    string = int2str(strip, &len);
    append_str(at, string, len);
    append_chr(at, '|');
    /* prefix */
    append_str(at, prefix, prefix_len);
    append_chr(at, '|');
    /* tag */
    append_str(at, tag, tag_len);
    append_chr(at, '|');
    /* ip_addr */
    if (ip_addr->af == AF_INET && ip_addr->u.addr32[0] > 0) {
	string = int2str(ip_addr->u.addr32[0], &len);
	append_str(at, string, len);
    }
    else if (ip_addr->af == AF_INET6 && !ip_addr_any(ip_addr)) {
	append_chr(at, '[');
	at += ip6tosbuf(ip_addr->u.addr, at, MAX_URI_LEN - (at - value));
	append_chr(at, ']');
    }
    append_chr(at, '|');
    /* hostname */
    append_str(at, hostname, hostname_len);
    append_chr(at, '|');
    /* port */
    if (port > 0) {
	string = int2str(port, &len);
	append_str(at, string, len);
    }
    append_chr(at, '|');
    /* params */
    append_str(at, params, params_len);
    append_chr(at, '|');
    /* transport */
    append_str(at, transport, transport_len);
    append_chr(at, '|');
    /* flags */
    string = int2str(flags, &len);
    append_str(at, string, len);
    return at - value;
}

static inline int decode_avp_value(char *value, unsigned int *gw_index, str *scheme,
			    unsigned int *strip, str *prefix, str *tag,
			    struct ip_addr *addr, str *hostname, str *port,
			    str *params, str *transport, unsigned int *flags)
{
    unsigned int u;
    str s;
    char *sep;
    struct ip_addr *ip;

    /* gw index */
    s.s = value;
    sep = index(s.s, '|');
    if (sep == NULL) {
	LM_ERR("index was not found in AVP value\n");
	return 0;
    }
    s.len = sep - s.s;
    str2int(&s, gw_index);
    /* scheme */
    scheme->s = sep + 1;
    sep = index(scheme->s, '|');
    if (sep == NULL) {
	LM_ERR("scheme was not found in AVP value\n");
	return 0;
    }
    scheme->len = sep - scheme->s;
    /* strip */
    s.s = sep + 1;
    sep = index(s.s, '|');
    if (sep == NULL) {
	LM_ERR("strip was not found in AVP value\n");
	return 0;
    }
    s.len = sep - s.s;
    str2int(&s, strip);
    /* prefix */
    prefix->s = sep + 1;
    sep = index(prefix->s, '|');
    if (sep == NULL) {
	LM_ERR("prefix was not found in AVP value\n");
	return 0;
    }
    prefix->len = sep - prefix->s;
    /* tag */
    tag->s = sep + 1;
    sep = index(tag->s, '|');
    if (sep == NULL) {
	LM_ERR("tag was not found in AVP value\n");
	return 0;
    }
    tag->len = sep - tag->s;
    /* addr */
    s.s = sep + 1;
    sep = index(s.s, '|');
    if (sep == NULL) {
	LM_ERR("ip_addr was not found in AVP value\n");
	return 0;
    }
    s.len = sep - s.s;
    if (s.len > 0) {
	if ((ip = str2ip(&s)) != NULL)
	    *addr = *ip;
	else if ((ip = str2ip6(&s)) != NULL)
	    *addr = *ip;
	else {
	    str2int(&s, &u);
	    addr->af = AF_INET;
	    addr->len = 4;
	    addr->u.addr32[0] = u;
	}
    } else {
	addr->af = 0;
    }
    /* hostname */
    hostname->s = sep + 1;
    sep = index(hostname->s, '|');
    if (sep == NULL) {
	LM_ERR("hostname was not found in AVP value\n");
	return 0;
    }
    hostname->len = sep - hostname->s;
    /* port */
    port->s = sep + 1;
    sep = index(port->s, '|');
    if (sep == NULL) {
	LM_ERR("port was not found in AVP value\n");
	return 0;
    }
    port->len = sep - port->s;
    /* params */
    params->s = sep + 1;
    sep = index(params->s, '|');
    if (sep == NULL) {
	LM_ERR("params was not found in AVP value\n");
	return 0;
    }
    params->len = sep - params->s;
    /* transport */
    transport->s = sep + 1;
    sep = index(transport->s, '|');
    if (sep == NULL) {
	LM_ERR("transport was not found in AVP value\n");
	return 0;
    }
    transport->len = sep - transport->s;
    /* flags */
    s.s = sep + 1;
    s.len = strlen(s.s);
    str2int(&s, flags);

    return 1;
}
    

/* Add gateways in matched_gws array into gw_uri_avps */
void add_gws_into_avps(struct gw_info *gws, struct matched_gw_info *matched_gws,
		       unsigned int gw_cnt, str *ruri_user)
{
    unsigned int i, index, strip, hostname_len, params_len;
    int prefix_len, tag_len;
    str value;
    char encoded_value[MAX_URI_LEN];
    int_str val;

    delete_avp(gw_uri_avp_type|AVP_VAL_STR, gw_uri_avp);

    for (i = 0; i < gw_cnt; i++) {
	if (matched_gws[i].duplicate == 1) continue;
	index = matched_gws[i].gw_index;
      	hostname_len = gws[index].hostname_len;
      	params_len = gws[index].params_len;
	strip = gws[index].strip;
	if (strip > ruri_user->len) {
	    LM_ERR("strip count of gw is too large <%u>\n", strip);
	    goto skip;
	}
	prefix_len = gws[index].prefix_len;
	tag_len = gws[index].tag_len;
	if (5 /* gw_index */ + 5 /* scheme */ + 4 /* strip */ + prefix_len +
	    tag_len + 1 /* @ */ +
	    ((hostname_len > IP6_MAX_STR_SIZE+2)?hostname_len:IP6_MAX_STR_SIZE+2) + 6 /* port */ +
	    params_len /* params */ + 15 /* transport */ + 10 /* flags */ +
	    7 /* separators */ > MAX_URI_LEN) {
	    LM_ERR("too long AVP value\n");
	    goto skip;
	}
	value.len = 
	    encode_avp_value(encoded_value, index,
			     gws[index].scheme, gws[index].scheme_len,
			     strip, gws[index].prefix, prefix_len,
			     gws[index].tag, tag_len,
			     &gws[index].ip_addr,
			     gws[index].hostname, hostname_len,
			     gws[index].port, gws[index].params, params_len,
			     gws[index].transport, gws[index].transport_len,
			     gws[index].flags);
	value.s = (char *)&(encoded_value[0]);
	val.s = value;
	add_avp(gw_uri_avp_type|AVP_VAL_STR, gw_uri_avp, val);

	LM_DBG("added gw_uri_avp <%.*s> with weight <%u>\n",
	       value.len, value.s, matched_gws[i].weight);
    skip:
	continue;
    }
}


/*
 * Load info of matching GWs into gw_uri_avps
 */
static int load_gws(struct sip_msg* _m, int argc, action_u_t argv[])
{
    str ruri_user, from_uri, *request_uri;
    int i, j, lcr_id;
    unsigned int gw_index, now, dex;
    int_str val;
    struct matched_gw_info matched_gws[MAX_NO_OF_GWS + 1];
    struct rule_info **rules, *rule, *pl;
    struct gw_info *gws;
    struct target *t;
    char* tmp;

    /* Get and check parameter values */
    if (argc < 1) {
	LM_ERR("lcr_id parameter is missing\n");
	return -1;
    }
    lcr_id = strtol(argv[0].u.string, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == argv[0].u.string)) {
	LM_ERR("invalid lcr_id parameter %s\n", argv[0].u.string);
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count_param)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }
    if (argc > 1) {
	ruri_user = argv[1].u.str;
    } else {
	if ((parse_sip_msg_uri(_m) < 0) || (!_m->parsed_uri.user.s)) {
	    LM_ERR("error while parsing R-URI\n");
	    return -1;
	}
	ruri_user = _m->parsed_uri.user;
    }
    if (argc > 2) {
	from_uri = argv[2].u.str;
    } else {
	from_uri.len = 0;
	from_uri.s = (char *)0;
    }
    if (argc > 3) {
	LM_ERR("too many parameters\n");
	return -1;
    }
    LM_DBG("load_gws(%u, %.*s, %.*s)\n", lcr_id, ruri_user.len, ruri_user.s,
	   from_uri.len, from_uri.s);

    request_uri = GET_RURI(_m);

    /* Use rules and gws with index lcr_id */
    rules = rule_pt[lcr_id];
    gws = gw_pt[lcr_id];

    /*
     * Find lcr entries that match based on prefix and from_uri and collect
     * gateways of matching entries into matched_gws array so that each
     * gateway appears in the array only once.
     */

    pl = rules[lcr_rule_hash_size_param];
    gw_index = 0;

    if (defunct_capability_param > 0) {
	delete_avp(defunct_gw_avp_type, defunct_gw_avp);
    }

    now = time((time_t *)NULL);

    /* check prefixes in from longest to shortest */
    while (pl) {
	if (ruri_user.len < pl->prefix_len) {
	    pl = pl->next;
	    continue;
	}
	rule = rule_hash_table_lookup(rules, pl->prefix_len, ruri_user.s);
	while (rule) {
	    /* Match prefix */
	    if ((rule->prefix_len != pl->prefix_len) ||
		(strncmp(rule->prefix, ruri_user.s, pl->prefix_len)))
		    goto next;

	    /* Match from uri */
	    if ((rule->from_uri_len != 0) &&
		(pcre_exec(rule->from_uri_re, NULL, from_uri.s,
			   from_uri.len, 0, 0, NULL, 0) < 0)) {
		LM_DBG("from uri <%.*s> did not match to from regex <%.*s>\n",
		       from_uri.len, from_uri.s, rule->from_uri_len,
		       rule->from_uri);
		goto next;
	    }

	    /* Match request uri */
	    if ((rule->request_uri_len != 0) &&
		(pcre_exec(rule->request_uri_re, NULL, request_uri->s,
			   request_uri->len, 0, 0, NULL, 0) < 0)) {
		LM_DBG("request uri <%.*s> did not match to request regex <%.*s>\n",
		       request_uri->len, request_uri->s, rule->request_uri_len,
		       rule->request_uri);
		goto next;
	    }

	    /* Load gws associated with this rule */
	    t = rule->targets;
	    while (t) {
		/* If this gw is defunct or inactive, skip it */
		if ((gws[t->gw_index].defunct_until > now) ||
		    (gws[t->gw_index].state == GW_INACTIVE))
		    goto skip_gw;
		matched_gws[gw_index].gw_index = t->gw_index;
		matched_gws[gw_index].prefix_len = pl->prefix_len;
		matched_gws[gw_index].priority = t->priority;
		matched_gws[gw_index].weight = t->weight *
		    (rand() >> 8);
		matched_gws[gw_index].duplicate = 0;
		LM_DBG("added matched_gws[%d]=[%u, %u, %u, %u]\n",
		       gw_index, t->gw_index, pl->prefix_len,
		       t->priority, matched_gws[gw_index].weight);
		gw_index++;
	    skip_gw:
		t = t->next;
	    }
	    /* Do not look further if this matching rule was stopper */
	    if (rule->stopper == 1) goto done;

next:
	    rule = rule->next;
	}
	pl = pl->next;
    }

 done:
    /* Sort gateways in reverse order based on prefix_len, priority,
       and randomized weight */
    qsort(matched_gws, gw_index, sizeof(struct matched_gw_info), comp_matched);

    /* Remove duplicate gws */
    for (i = gw_index - 1; i >= 0; i--) {
	if (matched_gws[i].duplicate == 1) continue;
	dex = matched_gws[i].gw_index;
	for (j = i - 1; j >= 0; j--) {
	    if (matched_gws[j].gw_index == dex) {
		matched_gws[j].duplicate = 1;
	    }
	}
    }

    /* Add gateways into gw_uris_avp */
    add_gws_into_avps(gws, matched_gws, gw_index, &ruri_user);

    /* Add lcr_id into AVP */
    if ((defunct_capability_param > 0) || (ping_interval_param > 0)) {
	delete_avp(lcr_id_avp_type, lcr_id_avp);
	val.n = lcr_id;
	add_avp(lcr_id_avp_type, lcr_id_avp, val);
    }
    
    if (gw_index > 0) {
	return 1;
    } else {
	return 2;
    }
}


/* Generate Request-URI and Destination URI */
static int generate_uris(struct sip_msg* _m, char *r_uri, str *r_uri_user,
			 unsigned int *r_uri_len, char *dst_uri,
			 unsigned int *dst_uri_len, struct ip_addr *addr,
			 unsigned int *gw_index, unsigned int *flags,
			 str *tag)
{
    int_str gw_uri_val;
    struct usr_avp *gu_avp;
    str scheme, prefix, hostname, port, params, transport, addr_str,
	tmp_tag;
    char *at;
    unsigned int strip;
    
    gu_avp = search_first_avp(gw_uri_avp_type, gw_uri_avp, &gw_uri_val, 0);

    if (!gu_avp) return 0; /* No more gateways left */

    decode_avp_value(gw_uri_val.s.s, gw_index, &scheme, &strip, &prefix,
		     &tmp_tag, addr, &hostname, &port, &params, &transport,
		     flags);

    if (addr->af != 0) {
	addr_str.s = ip_addr2a(addr);
	addr_str.len = strlen(addr_str.s);
    } else {
	addr_str.len = 0;
    }
    
    if (scheme.len + r_uri_user->len - strip + prefix.len + 1 /* @ */ +
	((hostname.len > IP6_MAX_STR_SIZE+2)?hostname.len:IP6_MAX_STR_SIZE+2) + 1 /* : */ +
	port.len + params.len + transport.len + 1 /* null */ > MAX_URI_LEN) {
	LM_ERR("too long Request URI or DST URI\n");
	return -1;
    }

    if ((dont_strip_or_prefix_flag_param != -1) &&
	isflagset(_m, dont_strip_or_prefix_flag_param)) {
	strip = 0;
	prefix.len = 0;
    }

    at = r_uri;
    
    append_str(at, scheme.s, scheme.len);
    append_str(at, prefix.s, prefix.len);
    if (strip > r_uri_user->len) {
	LM_ERR("strip count <%u> is larger than R-URI user <%.*s>\n",
	       strip, r_uri_user->len, r_uri_user->s);
	return -1;
    }
    append_str(at, r_uri_user->s + strip, r_uri_user->len - strip);

    append_chr(at, '@');

    if ((addr_str.len > 0) && (hostname.len > 0)) {
	/* both ip_addr and hostname specified:
	   place hostname in r-uri and ip_addr in dst-uri */
	append_str(at, hostname.s, hostname.len);
	if (params.len > 0) {
	    append_str(at, params.s, params.len);
	}
	*at = '\0';
	*r_uri_len = at - r_uri;
	at = dst_uri;
	append_str(at, scheme.s, scheme.len);
	if (addr->af == AF_INET6)
	    append_chr(at, '[');
	append_str(at, addr_str.s, addr_str.len);
	if (addr->af == AF_INET6)
	    append_chr(at, ']');
	if (port.len > 0) {
	    append_chr(at, ':');
	    append_str(at, port.s, port.len);
	}
	if (transport.len > 0) {
	    append_str(at, transport.s, transport.len);
	}
	*at = '\0';
	*dst_uri_len = at - dst_uri;
    } else {
	/* either ip_addr or hostname specified:
	   place the given one in r-uri and leave dst-uri empty */
	if (addr_str.len > 0) {
	    if (addr->af == AF_INET6)
		append_chr(at, '[');
	    append_str(at, addr_str.s, addr_str.len);
	    if (addr->af == AF_INET6)
		append_chr(at, ']');
	} else {
	    append_str(at, hostname.s, hostname.len);
	}
	if (port.len > 0) {
	    append_chr(at, ':');
	    append_str(at, port.s, port.len);
	}
	if (transport.len > 0) {
	    append_str(at, transport.s, transport.len);
	}
	if (params.len > 0) {
	    append_str(at, params.s, params.len);
	}
	*at = '\0';
	*r_uri_len = at - r_uri;
	*dst_uri_len = 0;
    }

    memcpy(tag->s, tmp_tag.s, tmp_tag.len);
    tag->len = tmp_tag.len;

    destroy_avp(gu_avp);
	
    LM_DBG("r_uri <%.*s>, dst_uri <%.*s>\n",
	   (int)*r_uri_len, r_uri, (int)*dst_uri_len, dst_uri);

    return 1;
}


/*
 * Defunct current gw until time given as argument has passed.
 */
static int defunct_gw(struct sip_msg* _m, char *_defunct_period, char *_s2)
{
    int_str lcr_id_val, index_val;
    struct gw_info *gws;
    char *tmp;
    unsigned int gw_index, defunct_until;
    int defunct_period;

    /* Check defunct gw capability */
    if (defunct_capability_param == 0) {
	LM_ERR("no defunct gw capability, activate by setting "
	       "defunct_capability module param\n");
	return -1;
    }

    /* Get and check parameter value */
    defunct_period = strtol(_defunct_period, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _defunct_period)) {
	LM_ERR("invalid defunct_period parameter %s\n", _defunct_period);
	return -1;
    }
    if (defunct_period < 0) {
	LM_ERR("invalid defunct_period param value %d\n", defunct_period);
	return -1;
    }

    /* Get AVP values */
    if (search_first_avp(lcr_id_avp_type, lcr_id_avp, &lcr_id_val, 0)
	== NULL) {
	LM_ERR("lcr_id_avp was not found\n");
	return -1;
    }
    gws = gw_pt[lcr_id_val.n];
    if (search_first_avp(defunct_gw_avp_type, defunct_gw_avp,
			 &index_val, 0) == NULL) {
	LM_ERR("defunct_gw_avp was not found\n");
	return -1;
    }
    gw_index = index_val.n;
    if ((gw_index < 1) || (gw_index > gws[0].ip_addr.u.addr32[0])) {
	LM_ERR("gw index <%u> is out of bounds\n", gw_index);
	return -1;
    }
    
    defunct_until = time((time_t *)NULL) + defunct_period;
    LM_DBG("defuncting gw with name <%.*s> until <%u>\n",
	   gws[gw_index].gw_name_len, gws[gw_index].gw_name, defunct_until);
    gws[gw_index].defunct_until = defunct_until;

    return 1;
}


/*
 * Inactivate current gw (provided that inactivate threshold has been reached)
 */
static int inactivate_gw(struct sip_msg* _m, char *_defunct_period, char *_s2)
{
    int_str lcr_id_val, index_val;
    struct gw_info *gws;
    unsigned int gw_index;

    /* Check inactivate gw capability */
    if (ping_interval_param == 0)  {
	LM_ERR("no inactivate gw capability, activate by setting "
	       "ping_interval module param\n");
	return -1;
    }

    /* Get AVP values */
    if (search_first_avp(lcr_id_avp_type, lcr_id_avp, &lcr_id_val, 0)
	== NULL) {
	LM_ERR("lcr_id_avp was not found\n");
	return -1;
    }
    gws = gw_pt[lcr_id_val.n];
    if (search_first_avp(defunct_gw_avp_type, defunct_gw_avp,
			 &index_val, 0) == NULL) {
	LM_ERR("defunct_gw_avp was not found\n");
	return -1;
    }
    gw_index = index_val.n;
    if ((gw_index < 1) || (gw_index > gws[0].ip_addr.u.addr32[0])) {
	LM_ERR("gw index <%u> is out of bounds\n", gw_index);
	return -1;
    }
    
    if (gws[gw_index].state == GW_ACTIVE) {
	gws[gw_index].state = GW_PINGING + ping_inactivate_threshold_param;
    } else if (gws[gw_index].state > GW_INACTIVE) {
	gws[gw_index].state--;
    }
    if (gws[gw_index].state > GW_INACTIVE) {
	LM_DBG("failing gw '%.*s' will be inactivated after '%u' "
	       "more failure(s)\n",
	       gws[gw_index].gw_name_len, gws[gw_index].gw_name,
	       gws[gw_index].state - GW_INACTIVE);
    } else {
	LM_DBG("failing gw '%.*s' has been inactivated\n",
	       gws[gw_index].gw_name_len, gws[gw_index].gw_name);
    }

    return 1;
}


/*
 * Defunct given gw in given lcr until time period given as argument has passed.
 */
int rpc_defunct_gw(unsigned int lcr_id, unsigned int gw_id, unsigned int period)
{
    struct gw_info *gws;
    unsigned int until, i;

    if ((lcr_id < 1) || (lcr_id > lcr_count_param)) {
	LM_ERR("invalid lcr_id value <%u>\n", lcr_id);
	return 0;
    }

    until = time((time_t *)NULL) + period;

    LM_DBG("defuncting gw <lcr_id/gw_id>=<%u/%u> for %u seconds until %d\n",
	   lcr_id, gw_id, period, until);

    gws = gw_pt[lcr_id];
    for (i = 1; i <= gws[0].ip_addr.u.addr32[0]; i++) {
	if (gws[i].gw_id == gw_id) {
	    gws[i].defunct_until = until;
	    return 1;
	}
    }
    
    LM_ERR("gateway with id <%u> not found\n", gw_id);

    return 0;
}


/* Check if OPTIONS ping reply matches a SIP reply code listed in
   ping_valid_reply_codes param */
static int check_extra_codes(unsigned int code)
{
    unsigned int i;
    for (i = 0; i < ping_rc_count; i++) {
	if (ping_valid_reply_codes[i] == code) return 0;
    }
    return -1;
}


/* Callback function that is executed when OPTIONS ping reply has been
   received */
static void ping_callback(struct cell *t, int type, struct tmcb_params *ps)
{

    struct gw_info *gw;
    str uri;

    gw = (struct gw_info *)(*ps->param);

    /* SIP URI is taken from the Transaction.
     * Remove the "To: <" (s+5) and the trailing >+new-line (s - 5 (To: <)
     * - 3 (>\r\n)). */
    uri.s = t->to.s + 5;
    uri.len = t->to.len - 8;

    LM_DBG("OPTIONS %.*s finished with code <%d>\n", 
	   uri.len, uri.s, ps->code);

    if (((ps->code >= 200) && (ps->code <= 299)) ||
	(check_extra_codes(ps->code) == 0)) {
	if ((uri.len == gw->uri_len) &&
	    (strncmp(uri.s, &(gw->uri[0]), uri.len) == 0)) {
	    LM_INFO("activating gw with uri %.*s\n", uri.len, uri.s);
	    gw->state = GW_ACTIVE;
	} else {
	    LM_DBG("ignoring OPTIONS reply due to lcr.reload\n");
	}
    }

    return;
}


/* Timer process for pinging inactive gateways */
void ping_timer(unsigned int ticks, void* param)
{
    struct gw_info *gws;
    uac_req_t uac_r;
    str uri;
    unsigned int i, j;

    /* Ping each gateway that is GW_PINGING state */

    for (j = 1; j <= lcr_count_param; j++) {

	gws = gw_pt[j];

	for (i = 1; i <= gws[0].ip_addr.u.addr32[0]; i++) {

	    if (gws[i].state >= GW_PINGING) {

		uri.s = &(gws[i].uri[0]);
		uri.len = gws[i].uri_len;
		LM_DBG("pinging gw uri %.*s\n", uri.len, uri.s);

		set_uac_req(&uac_r, &ping_method, 0, 0, 0,
			    TMCB_LOCAL_COMPLETED, ping_callback,
			    (void*)&(gws[i]));
		if (ping_socket_param.len > 0) {
		    uac_r.ssock = &ping_socket_param;
		}
		
		if (tmb.t_request(&uac_r, &uri, &uri, &ping_from_param, 0)
		    < 0) {
		    LM_ERR("unable to ping [%.*s]\n", uri.len, uri.s);
		}
	    }
	}
    }
}


/*
 * When called first time, rewrites scheme, host, port, and
 * transport parts of R-URI based on first gw_uri_avp value, which is then
 * destroyed.  Saves R-URI user to ruri_user_avp for later use.
 *
 * On other calls, rewrites R-URI, where scheme, host, port,
 * and transport of URI are taken from the first gw_uri_avp value, 
 * which is then destroyed. URI user is taken either from ruri_user_avp
 * value saved earlier.
 *
 * Returns 1 upon success and -1 upon failure.
 */
static int next_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
    int_str ruri_user_val, val;
    struct usr_avp *ru_avp;
    int rval;
    str uri_str, tag_str;
    char tag[MAX_TAG_LEN];
    unsigned int flags, r_uri_len, dst_uri_len, gw_index;
    char r_uri[MAX_URI_LEN], dst_uri[MAX_URI_LEN];
    struct ip_addr addr;

    tag_str.s = &(tag[0]);

    ru_avp = search_first_avp(ruri_user_avp_type, ruri_user_avp,
			      &ruri_user_val, 0);

    if (ru_avp == NULL) {
	
	/* First invocation either in route or failure route block.
	 * Take Request-URI user from Request-URI and generate Request
         * and Destination URIs. */

	if (parse_sip_msg_uri(_m) < 0) {
	    LM_ERR("parsing of R-URI failed\n");
	    return -1;
	}
	if (!generate_uris(_m, r_uri, &(_m->parsed_uri.user), &r_uri_len,
			   dst_uri, &dst_uri_len, &addr, &gw_index, &flags,
			   &tag_str)) {
	    return -1;
	}

	/* Save Request-URI user into uri_user_avp for use in subsequent
         * invocations. */

	val.s = _m->parsed_uri.user;
	add_avp(ruri_user_avp_type|AVP_VAL_STR, ruri_user_avp, val);
	LM_DBG("added ruri_user_avp <%.*s>\n", val.s.len, val.s.s);

    } else {

	/* Subsequent invocation either in route or failure route block.
	 * Take Request-URI user from ruri_user_avp and generate Request
         * and Destination URIs. */

	if (!generate_uris(_m, r_uri, &(ruri_user_val.s), &r_uri_len, dst_uri,
			   &dst_uri_len, &addr, &gw_index, &flags, &tag_str)) {
	    return -1;
	}
    }

    /* Rewrite Request URI */
    uri_str.s = r_uri;
    uri_str.len = r_uri_len;
    rewrite_uri(_m, &uri_str);
    
    /* Set Destination URI if not empty */
    if (dst_uri_len > 0) {
	uri_str.s = dst_uri;
	uri_str.len = dst_uri_len;
	LM_DBG("setting du to <%.*s>\n", uri_str.len, uri_str.s);
	rval = set_dst_uri(_m, &uri_str);
	if (rval != 0) {
	    LM_ERR("calling do_action failed with return value <%d>\n", rval);
	    return -1;
	}
	
    }

    /* Set flags_avp */
    if (flags_avp_param) {
	val.n = flags;
	add_avp(flags_avp_type, flags_avp, val);
	LM_DBG("added flags_avp <%u>\n", (unsigned int)val.n);
    }

    /* Set tag_avp */
    if (tag_avp_param) {
	val.s = tag_str;
	add_avp(tag_avp_type, tag_avp, val);
	LM_DBG("added tag_avp <%.*s>\n", val.s.len, val.s.s);
    }

    /* Add index of selected gw to defunct gw AVP */
    if ((defunct_capability_param > 0) || (ping_interval_param > 0)) {
	delete_avp(defunct_gw_avp_type, defunct_gw_avp);
	val.n = gw_index;
	add_avp(defunct_gw_avp_type, defunct_gw_avp, val);
	LM_DBG("added defunct_gw_avp <%u>\n", addr.u.addr32[0]);
    }
    
    return 1;
}


/*
 * Checks if request comes from ip address of a gateway
 */
static int do_from_gw(struct sip_msg* _m, unsigned int lcr_id,
		      struct ip_addr *src_addr, uri_transport transport)
{
    struct gw_info *res, gw, *gws;
    int_str val;
	
    gws = gw_pt[lcr_id];

    /* Skip lcr instance if some of its gws do not have ip_addr */
    if (gws[0].port != 0) {
	LM_DBG("lcr instance <%u> has gw(s) without ip_addr\n", lcr_id);
	return -1;
    }

    /* Search for gw ip address */
    gw.ip_addr = *src_addr;
    res = (struct gw_info *)bsearch(&gw, &(gws[1]), gws[0].ip_addr.u.addr32[0],
				    sizeof(struct gw_info), comp_gws);

    /* Store tag and flags and return result */
    if ((res != NULL) &&
  	((transport == PROTO_NONE) || (res->transport_code == transport))) {
	LM_DBG("request game from gw\n");
	if (tag_avp_param) {
	    val.s.s = res->tag;
	    val.s.len = res->tag_len;
	    add_avp(tag_avp_type, tag_avp, val);
	    LM_DBG("added tag_avp <%.*s>\n", val.s.len, val.s.s);
	}
	if (flags_avp_param) {
	    val.n = res->flags;
	    add_avp(flags_avp_type, flags_avp, val);
	    LM_DBG("added flags_avp <%u>\n", (unsigned int)val.n);
	}
	return 1;
    } else {
	LM_DBG("request did not come from gw\n");
	return -1;
    }
}


/*
 * Checks if request comes from ip address of a gateway taking source
 * address and transport protocol from request.
 */
static int from_gw_1(struct sip_msg* _m, char* _lcr_id, char* _s2)
{
    int lcr_id;
    char *tmp;
    uri_transport transport;

    /* Get and check parameter value */
    lcr_id = strtol(_lcr_id, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _lcr_id)) {
	LM_ERR("invalid lcr_id parameter %s\n", _lcr_id);
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count_param)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }

    /* Get transport protocol */
    transport = _m->rcv.proto;

    /* Do test */
    return do_from_gw(_m, lcr_id, &_m->rcv.src_ip, transport);
}


/*
 * Checks if request comes from ip address of a gateway taking source
 * address and transport protocol from parameters
 */
static int from_gw_3(struct sip_msg* _m, char* _lcr_id, char* _addr,
		     char* _transport)
{
    struct ip_addr src_addr;
    int lcr_id;
    char *tmp;
    struct ip_addr *ip;
    str addr_str;
    uri_transport transport;

    /* Get and check parameter values */
    lcr_id = strtol(_lcr_id, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _lcr_id)) {
	LM_ERR("invalid lcr_id parameter %s\n", _lcr_id);
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count_param)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }
    addr_str.s = _addr;
    addr_str.len = strlen(_addr);
    if ((ip = str2ip(&addr_str)) != NULL)
	src_addr = *ip;
    else if ((ip = str2ip6(&addr_str)) != NULL)
	src_addr = *ip;
    else {
	LM_ERR("addr param value %s is not an IP address\n", _addr);
	return -1;
    }
    transport = strtol(_transport, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _transport)) {
	LM_ERR("invalid transport parameter %s\n", _lcr_id);
	return -1;
    }
    if ((transport < PROTO_NONE) || (transport > PROTO_SCTP)) {
	LM_ERR("invalid transport parameter value %d\n", transport);
	return -1;
    }

    /* Do test */
    return do_from_gw(_m, lcr_id, &src_addr, transport);
}


/*
 * Checks if request comes from ip address of any gateway taking source
 * address from request.
 */
static int from_any_gw_0(struct sip_msg* _m, char* _s1, char* _s2)
{
    unsigned int i;
    uri_transport transport;

    transport = _m->rcv.proto;

    for (i = 1; i <= lcr_count_param; i++) {
	if (do_from_gw(_m, i, &_m->rcv.src_ip, transport) == 1) {
	    return i;
	}
    }
    return -1;
}


/*
 * Checks if request comes from ip address of a a gateway taking source
 * IP address and transport protocol from parameters.
 */
static int from_any_gw_2(struct sip_msg* _m, char* _addr, char* _transport)
{
    unsigned int i;
    char *tmp;
    struct ip_addr *ip, src_addr;
    str addr_str;
    uri_transport transport;

    /* Get and check parameter values */
    addr_str.s = _addr;
    addr_str.len = strlen(_addr);
    if ((ip = str2ip(&addr_str)) != NULL)
	src_addr = *ip;
    else if ((ip = str2ip6(&addr_str)) != NULL)
	src_addr = *ip;
    else {
	LM_ERR("addr param value %s is not an IP address\n", _addr);
	return -1;
    }
    transport = strtol(_transport, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _transport)) {
	LM_ERR("invalid transport parameter %s\n", _transport);
	return -1;
    }
    if ((transport < PROTO_NONE) || (transport > PROTO_SCTP)) {
	LM_ERR("invalid transport parameter value %d\n", transport);
	return -1;
    }

    /* Do test */
    for (i = 1; i <= lcr_count_param; i++) {
	if (do_from_gw(_m, i, &src_addr, transport) == 1) {
	    return i;
	}
    }
    return -1;
}


/*
 * Checks if in-dialog request goes to ip address of a gateway.
 */
static int do_to_gw(struct sip_msg* _m, unsigned int lcr_id,
		    struct ip_addr *dst_addr, uri_transport transport)
{
    struct gw_info *res, gw, *gws;

    gws = gw_pt[lcr_id];

    /* Skip lcr instance if some of its gws do not have ip addr */
    if (gws[0].port != 0) {
	LM_DBG("lcr instance <%u> has gw(s) without ip_addr\n", lcr_id);
	return -1;
    }

    /* Search for gw ip address */
    gw.ip_addr = *dst_addr;
    res = (struct gw_info *)bsearch(&gw, &(gws[1]), gws[0].ip_addr.u.addr32[0],
				    sizeof(struct gw_info), comp_gws);

    /* Return result */
    if ((res != NULL) &&
  	((transport == PROTO_NONE) || (res->transport_code == transport))) {
	LM_DBG("request goes to gw\n");
	return 1;
    } else {
	LM_DBG("request is not going to gw\n");
	return -1;
    }
}


/*
 * Checks if request goes to ip address and transport protocol of a gateway
 * taking lcr_id from parameter and destination address and transport protocol
 * from Request URI.
 */
static int to_gw_1(struct sip_msg* _m, char* _lcr_id, char* _s2)
{
    int lcr_id;
    char *tmp;
    struct ip_addr *ip, dst_addr;
    uri_transport transport;

    /* Get and check parameter value */
    lcr_id = strtol(_lcr_id, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _lcr_id)) {
	LM_ERR("invalid lcr_id parameter %s\n", _lcr_id);
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count_param)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }

    /* Get destination address and transport protocol from R-URI */
    if ((_m->parsed_uri_ok == 0) && (parse_sip_msg_uri(_m) < 0)) {
	LM_ERR("while parsing Request-URI\n");
	return -1;
    }
    if (_m->parsed_uri.host.len > IP6_MAX_STR_SIZE+2) {
	LM_DBG("request is not going to gw "
	       "(Request-URI host is not an IP address)\n");
	return -1;
    }
    if ((ip = str2ip(&(_m->parsed_uri.host))) != NULL)
	dst_addr = *ip;
    else if ((ip = str2ip6(&(_m->parsed_uri.host))) != NULL)
	dst_addr = *ip;
    else {
	LM_DBG("request is not going to gw "
	       "(Request-URI host is not an IP address)\n");
	return -1;
    }
    transport = _m->parsed_uri.proto;

    /* Do test */
    return do_to_gw(_m, lcr_id, &dst_addr, transport);
}


/*
 * Checks if request goes to ip address of a gateway taking lcr_id,
 * destination address and transport protocol from parameters.
 */
static int to_gw_3(struct sip_msg* _m, char* _lcr_id, char* _addr,
		   char* _transport)
{
    int lcr_id;
    char *tmp;
    struct ip_addr *ip, dst_addr;
    str addr_str;
    uri_transport transport;

    /* Get and check parameter values */
    lcr_id = strtol(_lcr_id, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _lcr_id)) {
	LM_ERR("invalid lcr_id parameter %s\n", _lcr_id);
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count_param)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }
    addr_str.s = _addr;
    addr_str.len = strlen(_addr);
    if ((ip = str2ip(&addr_str)) != NULL)
	dst_addr = *ip;
    else if ((ip = str2ip(&addr_str)) != NULL)
	dst_addr = *ip;
    else {
	LM_ERR("addr param value %s is not an IP address\n", _addr);
	return -1;
    }
    transport = strtol(_transport, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _transport)) {
	LM_ERR("invalid transport parameter %s\n", _transport);
	return -1;
    }
    if ((transport < PROTO_NONE) || (transport > PROTO_SCTP)) {
	LM_ERR("invalid transport parameter value %d\n", transport);
	return -1;
    }
    
    /* Do test */
    return do_to_gw(_m, lcr_id, &dst_addr, transport);
}


/*
 * Checks if request goes to ip address of any gateway taking destination
 * address and transport protocol from Request-URI.
 */
static int to_any_gw_0(struct sip_msg* _m, char* _s1, char* _s2)
{
    unsigned int i;
    struct ip_addr *ip, dst_addr;
    uri_transport transport;

    /* Get destination address and transport protocol from R-URI */
    if ((_m->parsed_uri_ok == 0) && (parse_sip_msg_uri(_m) < 0)) {
	LM_ERR("while parsing Request-URI\n");
	return -1;
    }
    if (_m->parsed_uri.host.len > IP6_MAX_STR_SIZE+2) {
	LM_DBG("request is not going to gw "
	       "(Request-URI host is not an IP address)\n");
	return -1;
    }
    if ((ip = str2ip(&(_m->parsed_uri.host))) != NULL)
	dst_addr = *ip;
    else if ((ip = str2ip6(&(_m->parsed_uri.host))) != NULL)
	dst_addr = *ip;
    else {
	LM_DBG("request is not going to gw "
	       "(Request-URI host is not an IP address)\n");
	return -1;
    }
    transport = _m->parsed_uri.proto;

    /* Do test */
    for (i = 1; i <= lcr_count_param; i++) {
	if (do_to_gw(_m, i, &dst_addr, transport) == 1) {
	    return i;
	}
    }
    return -1;
}


/*
 * Checks if request goes to ip address of any gateway taking destination
 * address and transport protocol from parameters.
 */
static int to_any_gw_2(struct sip_msg* _m, char* _addr, char* _transport)
{
    unsigned int i;
    char *tmp;
    struct ip_addr *ip, dst_addr;
    uri_transport transport;
    str addr_str;

    /* Get and check parameter values */
    addr_str.s = _addr;
    addr_str.len = strlen(_addr);
    if ((ip = str2ip(&addr_str)) != NULL)
	dst_addr = *ip;
    else if ((ip = str2ip6(&addr_str)) != NULL)
	dst_addr = *ip;
    else {
	LM_ERR("addr param value %s is not an IP address\n", _addr);
	return -1;
    }
    transport = strtol(_transport, &tmp, 10);
    if ((tmp == 0) || (*tmp) || (tmp == _transport)) {
	LM_ERR("invalid transport parameter %s\n", _transport);
	return -1;
    }
    if ((transport < PROTO_NONE) || (transport > PROTO_SCTP)) {
	LM_ERR("invalid transport parameter value %d\n", transport);
	return -1;
    }

    /* Do test */
    for (i = 1; i <= lcr_count_param; i++) {
	if (do_to_gw(_m, i, &dst_addr, transport) == 1) {
	    return i;
	}
    }
    return -1;
}
