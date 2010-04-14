/*
 *
 * Least Cost Routing module
 *
 * Copyright (C) 2005-2009 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "../../lib/kcore/km_ut.h"
#include "../../usr_avp.h"
#include "../../parser/parse_from.h"
#include "../../parser/msg_parser.h"
#include "../../action.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "../../ip_addr.h"
#include "../../resolve.h"
#include "../../lib/kmi/mi.h"
#include "../../mod_fix.h"
#include "../../socket_info.h"
#include "../../modules/tm/tm_load.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "hash.h"
#include "mi.h"
#include "lcr_rpc.h" /* defines RPC_SUPPORT */
#ifdef RPC_SUPPORT
#include "../../rpc_lookup.h"
#endif /* RPC_SUPPORT */



MODULE_VERSION

/*
 * Version of gw and lcr tables required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define GW_TABLE_VERSION 10
#define LCR_TABLE_VERSION 3

static void destroy(void);       /* Module destroy function */
static int mi_child_init(void);
static int mod_init(void);       /* Module initialization function */
static int child_init(int rank); /* Per-child initialization function */
static void free_shared_memory(void);

#define GW_TABLE "gw"

#define LCR_ID_COL "lcr_id"

#define GW_NAME_COL "gw_name"

#define GRP_ID_COL "grp_id"

#define IP_ADDR_COL "ip_addr"

#define HOSTNAME_COL "hostname"

#define PORT_COL "port"

#define URI_SCHEME_COL "uri_scheme"

#define TRANSPORT_COL "transport"

#define STRIP_COL "strip"

#define TAG_COL "tag"

#define WEIGHT_COL "weight"

#define FLAGS_COL "flags"

#define DEFUNCT_COL "defunct"

#define LCR_TABLE "lcr"

#define PREFIX_COL "prefix"

#define FROM_URI_COL "from_uri"

#define PRIORITY_COL "priority"


/* Default module parameter values */
#define DEF_LCR_HASH_SIZE 128
#define DEF_FETCH_ROWS 1024

/*
 * Type definitions
 */

/* TMB Structure */
struct tm_binds tmb;

struct gw_grp {
    unsigned int grp_id;
    unsigned int first;   /* index to first gw of group in gw table */
};

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

/* database tables */
static str db_url           = str_init(DEFAULT_RODB_URL);
static str gw_table         = str_init(GW_TABLE);
static str lcr_id_col       = str_init(LCR_ID_COL);
static str gw_name_col      = str_init(GW_NAME_COL);
static str grp_id_col       = str_init(GRP_ID_COL);
static str ip_addr_col      = str_init(IP_ADDR_COL);
static str hostname_col     = str_init(HOSTNAME_COL);
static str port_col         = str_init(PORT_COL);
static str uri_scheme_col   = str_init(URI_SCHEME_COL);
static str transport_col    = str_init(TRANSPORT_COL);
static str strip_col        = str_init(STRIP_COL);
static str tag_col          = str_init(TAG_COL);
static str weight_col       = str_init(WEIGHT_COL);
static str flags_col        = str_init(FLAGS_COL);
static str defunct_col      = str_init(DEFUNCT_COL);
static str lcr_table        = str_init(LCR_TABLE);
static str prefix_col       = str_init(PREFIX_COL);
static str from_uri_col     = str_init(FROM_URI_COL);
static str priority_col     = str_init(PRIORITY_COL);

/* number of rows to fetch at a shot */
static int fetch_rows_param = DEF_FETCH_ROWS;

/* avps */
static char *gw_uri_avp_param = NULL;
static char *ruri_user_avp_param = NULL;
static char *flags_avp_param = NULL;
static char *defunct_gw_avp_param = NULL;
static char *lcr_id_avp_param = NULL;

/* size of prefix hash table */
unsigned int lcr_hash_size_param = DEF_LCR_HASH_SIZE;

/* number of lcr instances */
unsigned int lcr_count = 1;

/* can gws be defuncted */
static unsigned int defunct_capability = 0;

/*
 * Other module types and variables
 */

static int     gw_uri_avp_type;
static int_str gw_uri_avp;
static int     ruri_user_avp_type;
static int_str ruri_user_avp;
static int     flags_avp_type;
static int_str flags_avp;
static int     defunct_gw_avp_type;
static int_str defunct_gw_avp;
static int     lcr_id_avp_type;
static int_str lcr_id_avp;

/* Pointer to gw table pointer table */
struct gw_info **gwtp = (struct gw_info **)NULL;

/* Pointer to lcr hash table pointer table */
struct lcr_info ***lcrtp = (struct lcr_info ***)NULL;


/*
 * Functions that are defined later
 */
static int load_gws(struct sip_msg* _m, char* _s1, char* _s2);
static int next_gw(struct sip_msg* _m, char* _s1, char* _s2);
static int defunct_gw(struct sip_msg* _m, char* _s1, char* _s2);
static int from_gw_1(struct sip_msg* _m, char* _s1, char* _s2);
static int from_gw_2(struct sip_msg* _m, char* _s1, char* _s2);
static int from_any_gw_0(struct sip_msg* _m, char* _s1, char* _s2);
static int from_any_gw_1(struct sip_msg* _m, char* _s1, char* _s2);
static int to_gw_1(struct sip_msg* _m, char* _s1, char* _s2);
static int to_gw_2(struct sip_msg* _m, char* _s1, char* _s2);
static int to_any_gw_0(struct sip_msg* _m, char* _s1, char* _s2);
static int to_any_gw_1(struct sip_msg* _m, char* _s1, char* _s2);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"load_gws", (cmd_function)load_gws, 2, fixup_igp_pvar,
     fixup_free_igp_pvar, REQUEST_ROUTE | FAILURE_ROUTE},
    {"next_gw", (cmd_function)next_gw, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"defunct_gw", (cmd_function)defunct_gw, 1, fixup_igp_null, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"from_gw", (cmd_function)from_gw_1, 1, fixup_igp_null, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"from_gw", (cmd_function)from_gw_2, 2, fixup_igp_pvar,
     fixup_free_igp_pvar, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"from_any_gw", (cmd_function)from_any_gw_0, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"from_any_gw", (cmd_function)from_any_gw_1, 1, fixup_pvar_null,
     fixup_free_pvar_null, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_gw", (cmd_function)to_gw_1, 1, fixup_igp_null, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_gw", (cmd_function)to_gw_2, 2, fixup_igp_pvar,
     fixup_free_igp_pvar, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_any_gw", (cmd_function)to_any_gw_0, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_any_gw", (cmd_function)to_any_gw_1, 1, fixup_pvar_null,
     fixup_free_pvar_null, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"db_url",                   STR_PARAM, &db_url.s       },
    {"gw_table",                 STR_PARAM, &gw_table.s     },
    {"lcr_id_column",            STR_PARAM, &lcr_id_col.s   },
    {"gw_name_column",           STR_PARAM, &gw_name_col.s  },
    {"grp_id_column",            STR_PARAM, &grp_id_col.s   },
    {"ip_addr_column",           STR_PARAM, &ip_addr_col.s  },
    {"hostname_column",          STR_PARAM, &hostname_col.s },
    {"port_column",              STR_PARAM, &port_col.s     },
    {"uri_scheme_column",        STR_PARAM, &uri_scheme_col.s },
    {"transport_column",         STR_PARAM, &transport_col.s },
    {"strip_column",             STR_PARAM, &strip_col.s    },
    {"tag_column",               STR_PARAM, &tag_col.s      },
    {"weight_column",            STR_PARAM, &weight_col.s   },
    {"flags_column",             STR_PARAM, &flags_col.s    },
    {"lcr_table",                STR_PARAM, &lcr_table.s    },
    {"prefix_column",            STR_PARAM, &prefix_col.s   },
    {"from_uri_column",          STR_PARAM, &from_uri_col.s },
    {"priority_column",          STR_PARAM, &priority_col.s },
    {"gw_uri_avp",               STR_PARAM, &gw_uri_avp_param },
    {"ruri_user_avp",            STR_PARAM, &ruri_user_avp_param },
    {"flags_avp",                STR_PARAM, &flags_avp_param },
    {"lcr_hash_size",            INT_PARAM, &lcr_hash_size_param },
    {"lcr_count",                INT_PARAM, &lcr_count },
    {"fetch_rows",               INT_PARAM, &fetch_rows_param },
    {"defunct_capability",       INT_PARAM, &defunct_capability },
    {"defunct_gw_avp",           STR_PARAM, &defunct_gw_avp_param },
    {"lcr_id_avp",               STR_PARAM, &lcr_id_avp_param },
    {0, 0, 0}
};


/*
 * Exported MI functions
 */
static mi_export_t mi_cmds[] = {
    { MI_LCR_RELOAD, mi_lcr_reload, MI_NO_INPUT_FLAG, 0, mi_child_init },
    { MI_LCR_GW_DUMP, mi_lcr_gw_dump, MI_NO_INPUT_FLAG, 0, 0 },
    { MI_LCR_LCR_DUMP, mi_lcr_lcr_dump, MI_NO_INPUT_FLAG, 0, 0 },
    { 0, 0, 0, 0 ,0}
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
	mi_cmds,   /* exported MI functions */
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


static int mi_child_init(void)
{
	return lcr_db_init(&db_url);
}


/*
 * Module initialization function that is called before the main process forks
 */
static int mod_init(void)
{
    pv_spec_t avp_spec;
    str s;
    unsigned short avp_flags;
    unsigned int i;
    db1_con_t* dbh;

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
#ifdef RPC_SUPPORT
	if (rpc_register_array(lcr_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
#endif /* RPC_SUPPORT */


    /* Update length of module variables */
    db_url.len = strlen(db_url.s);
    gw_table.len = strlen(gw_table.s);
    lcr_id_col.len = strlen(lcr_id_col.s);
    gw_name_col.len = strlen(gw_name_col.s);
    grp_id_col.len = strlen(grp_id_col.s);
    ip_addr_col.len = strlen(ip_addr_col.s);
    hostname_col.len = strlen(hostname_col.s);
    port_col.len = strlen(port_col.s);
    uri_scheme_col.len = strlen(uri_scheme_col.s);
    transport_col.len = strlen(transport_col.s);
    strip_col.len = strlen(strip_col.s);
    tag_col.len = strlen(tag_col.s);
    weight_col.len = strlen(weight_col.s);
    flags_col.len = strlen(flags_col.s);
    lcr_table.len = strlen(lcr_table.s);
    prefix_col.len = strlen(prefix_col.s);
    from_uri_col.len = strlen(from_uri_col.s);
    priority_col.len = strlen(priority_col.s);

    /* Bind database */
    if (lcr_db_bind(&db_url)) {
	LM_ERR("no database module found\n");
	return -1;
    }

    /* Check value of prefix_hash_size */
    if (lcr_hash_size_param <= 0) {
	LM_ERR("invalid prefix_hash_size value <%d>\n", lcr_hash_size_param);
	return -1;
    }

    /* Check value of lcr_count */
    if (lcr_count < 1) {
	LM_ERR("invalid lcr_count module parameter value <%d>\n", lcr_count);
	return -1;
    }

    /* Process AVP params */

    if (gw_uri_avp_param && *gw_uri_avp_param) {
	s.s = gw_uri_avp_param; s.len = strlen(s.s);
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n", gw_uri_avp_param);
	    return -1;
	}
	
	if (pv_get_avp_name(0, &(avp_spec.pvp), &gw_uri_avp, &avp_flags) != 0) {
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
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n",
		   ruri_user_avp_param);
	    return -1;
	}
	
	if (pv_get_avp_name(0, &(avp_spec.pvp), &ruri_user_avp, &avp_flags)
	    != 0) {
	    LM_ERR("invalid AVP definition <%s>\n", ruri_user_avp_param);
	    return -1;
	}
	ruri_user_avp_type = avp_flags;
    } else {
	LM_ERR("AVP ruri_user_avp has not been defined\n");
	return -1;
    }

    if (flags_avp_param && *flags_avp_param) {
	s.s = flags_avp_param; s.len = strlen(s.s);
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n", flags_avp_param);
	    return -1;
	}
	
	if (pv_get_avp_name(0, &(avp_spec.pvp), &flags_avp, &avp_flags) != 0) {
	    LM_ERR("invalid AVP definition <%s>\n", flags_avp_param);
	    return -1;
	}
	flags_avp_type = avp_flags;
    } else {
	LM_ERR("AVP flags_avp has not been defined\n");
	return -1;
    }

    if (defunct_capability > 0) {
	if (defunct_gw_avp_param && *defunct_gw_avp_param) {
	    s.s = defunct_gw_avp_param; s.len = strlen(s.s);
	    if ((pv_parse_spec(&s, &avp_spec) == 0) ||
		(avp_spec.type != PVT_AVP)) {
		LM_ERR("malformed or non AVP definition <%s>\n",
		       defunct_gw_avp_param);
		return -1;
	    }
	    if (pv_get_avp_name(0, &(avp_spec.pvp), &defunct_gw_avp,
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
	    if ((pv_parse_spec(&s, &avp_spec) == 0) ||
		(avp_spec.type != PVT_AVP)) {
		LM_ERR("malformed or non AVP definition <%s>\n",
		       lcr_id_avp_param);
		return -1;
	    }
	    if (pv_get_avp_name(0, &(avp_spec.pvp), &lcr_id_avp,
				&avp_flags) != 0) {
		LM_ERR("invalid AVP definition <%s>\n", lcr_id_avp_param);
		return -1;
	    }
	    lcr_id_avp_type = avp_flags;
	} else {
	    LM_ERR("AVP lcr_id_avp has not been defined\n");
	    return -1;
	}
    }

    if (fetch_rows_param < 1) {
	LM_ERR("invalid fetch_rows module parameter value <%d>\n",
	       fetch_rows_param);
	return -1;
    }

    /* Check table version */
    if (lcr_dbf.init==0){
	LM_CRIT("unbound database\n");
	return -1;
    }
    dbh=lcr_dbf.init(&db_url);
    if (dbh == NULL){
	LM_ERR("unable to open database connection\n");
	return -1;
    }
    if ((db_check_table_version(&lcr_dbf, dbh, &gw_table, GW_TABLE_VERSION)
	 < 0) ||
	(db_check_table_version(&lcr_dbf, dbh, &lcr_table, LCR_TABLE_VERSION)
	 < 0)) { 
	LM_ERR("error during table version check\n");
	lcr_dbf.close(dbh);
	goto err;
    }
    lcr_dbf.close(dbh);

    /* Allocate gw related shared memory */
    /* gw table pointer table, index 0 points to temp gw table  */
    gwtp = (struct gw_info **)shm_malloc(sizeof(struct gw_info *) *
					 (lcr_count + 1));
    if (gwtp == 0) {
	LM_ERR("no memory for gw table pointer table\n");
	goto err;
    }
    memset(gwtp, 0, sizeof(struct gw_info *) * (lcr_count + 1));
    /* gw tables */
    for (i = 0; i <= lcr_count; i++) {
	gwtp[i] = (struct gw_info *)shm_malloc(sizeof(struct gw_info) *
					       (MAX_NO_OF_GWS + 2));
	if (gwtp[i] == 0) {
	    LM_ERR("no memory for gw table\n");
	    goto err;
	}
 	(gwtp[i])[0].ip_addr = 0;    /* Number of gateways in table */
    }

    /* Allocate lcr rules related shared memory */
    /* lcr hash table pointer table, index 0 points to temp lcr table */
    lcrtp = (struct lcr_info ***)shm_malloc(sizeof(struct lcr_info **) *
					    (lcr_count + 1));
    if (lcrtp == 0) {
	LM_ERR("no memory for lcr hash table pointer table\n");
	goto err;
    }
    memset(lcrtp, 0, sizeof(struct lcr_info **) * (lcr_count + 1));
    /* lcr hash tables */
    /* Last entry in hash table contains list of different prefix lengths */
    for (i = 0; i <= lcr_count; i++) {
	lcrtp[i] = (struct lcr_info **)shm_malloc(sizeof(struct lcr_info *) *
						  (lcr_hash_size_param + 1));
	if (lcrtp[i] == 0) {
	    LM_ERR("no memory for lcr hash table\n");
	    goto err;
	}
	memset(lcrtp[i], 0, sizeof(struct lcr_info *) *
	       (lcr_hash_size_param + 1));
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
    for (i = 1; i <= lcr_count; i++) {
	if (reload_gws_and_lcrs(i) < 0) {
	    lock_release(reload_lock);
	    LM_CRIT("failed to reload gateways of lcr_id %i\n", i);
	    goto err;
	}
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
#ifdef RPC_SUPPORT
	/* do nothing for the main process, tcp main process or timer */
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN ||
		rank==PROC_TIMER)
		return 0;
	/* init db for the rest of the processes:
	   - we need it for PROC_RPC and PROC_FIFO if we want db access from
	     RPC accessed via the ctl module
	   - we need it from all the ser tcp or tls processes if we want
	     db access from RPC via the xmlrpc module */
	return lcr_db_init(&db_url);
#else
	return 0;
#endif /* RPC_SUPPORT */
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
    for (i = 0; i <= lcr_count; i++) {
	if (gwtp && gwtp[i]) {
	    shm_free(gwtp[i]);
	    gwtp[i] = 0;
	}
	if (lcrtp && lcrtp[i]) {
	    lcr_hash_table_contents_free(lcrtp[i]);
	    shm_free(lcrtp[i]);
	    lcrtp[i] = 0;
	}
    }
    if (gwtp) shm_free(gwtp);
    if (lcrtp) shm_free(lcrtp);
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
    int rc, size, err_offset;

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
 * Compare gateways based on their IP address and group id
 */
static int comp_gw_grps(const void *_g1, const void *_g2)
{
    struct gw_info *g1 = (struct gw_info *)_g1;
    struct gw_info *g2 = (struct gw_info *)_g2;

    if (g1->ip_addr < g2->ip_addr) return -1;
    if (g1->ip_addr > g2->ip_addr) return 1;

    if (g1->grp_id < g2->grp_id) return -1;
    if (g1->grp_id > g2->grp_id) return 1;

    return 0;
}


/*
 * Compare gateways based on their IP address
 */
static int comp_gws(const void *_g1, const void *_g2)
{
    struct gw_info *g1 = (struct gw_info *)_g1;
    struct gw_info *g2 = (struct gw_info *)_g2;

    if (g1->ip_addr < g2->ip_addr) return -1;
    if (g1->ip_addr > g2->ip_addr) return 1;

    return 0;
}


/*
 * Check if ip_addr/grp_id of gateway is unique.
 */
static int gw_unique(const struct gw_info *gws, const unsigned int count,
		     const unsigned int ip_addr, const unsigned int grp_id)
{
    unsigned int i;

    for (i = 1; i <= count; i++) {
	if ((gws[i].ip_addr == ip_addr) &&
	    (gws[i].grp_id == grp_id))
	    return 0;
    }

    return 1;
}

static int insert_gw(struct gw_info *gws, unsigned int i, unsigned int ip_addr,
		     char *hostname, unsigned int hostname_len,
		     unsigned int grp_id, char *ip_string, unsigned int port,
		     unsigned int scheme, unsigned int transport,
		     unsigned int flags, unsigned int strip, char *tag,
		     unsigned int tag_len, unsigned short weight,
		     unsigned int defunct_until)
{
    if (gw_unique(gws, i - 1, ip_addr, grp_id) == 0) {
	LM_ERR("ip_addr/grp_id <%s/%u> of gw is not unique\n",
	       ip_string, grp_id);
	return 0;
    }
    gws[i].ip_addr = ip_addr;
    if (hostname_len) memcpy(&(gws[i].hostname[0]), hostname, hostname_len);
    gws[i].hostname_len = hostname_len;
    gws[i].ip_addr = ip_addr;
    gws[i].port = port;
    gws[i].grp_id = grp_id;
    gws[i].scheme = scheme;
    gws[i].transport = transport;
    gws[i].flags = flags;
    gws[i].strip = strip;
    gws[i].tag_len = tag_len;
    if (tag_len) memcpy(&(gws[i].tag[0]), tag, tag_len);
    gws[i].weight = weight;
    gws[i].defunct_until = defunct_until;
    gws[i].next = 0;

    return 1;
}

/*
 * Links gws that belong to same group via next field, sets gw_grps
 * array with first indexes of each group, and sets grp_cnt to number of
 * different gw groups.
 */
static void link_gw_grps(struct gw_info *gws, struct gw_grp *gw_grps,
			 unsigned int *grp_cnt)
{
    unsigned int i, j;

    *grp_cnt = 0;
    
    for (i = 1; i <= gws[0].ip_addr; i++) {
	for (j = 1; j < i; j++) {
	    if (gws[j].grp_id == gws[i].grp_id) {
		gws[i].next = gws[j].next;
		gws[j].next = i;
		goto found;
	    }
	}
	gw_grps[*grp_cnt].grp_id = gws[i].grp_id;
	gw_grps[*grp_cnt].first = i;
	*grp_cnt = *grp_cnt + 1;
    found:
	continue;
    }
}

/*
 * Return gw table index of first gw in given group or 0 if no gws in
 * the group.
 */
static int find_first_gw(struct gw_grp *gw_grps, unsigned int grp_cnt,
			 unsigned int grp_id)
{
    unsigned int i;
    
    for (i = 0; i < grp_cnt; i++) {
	if (gw_grps[i].grp_id == grp_id) {
	    return gw_grps[i].first;
	}
    }

    return 0;
}

/*
 * Insert prefix_len into list pointed by last lcr hash table entry 
 * if not there already. Keep list in decending prefix_len order.
 */
static int prefix_len_insert(struct lcr_info **table, unsigned short prefix_len)
{
    struct lcr_info *lcr_rec, **previous, *this;
    
    previous = &(table[lcr_hash_size_param]);
    this = table[lcr_hash_size_param];

    while (this) {
	if (this->prefix_len == prefix_len)
	    return 1;
	if (this->prefix_len < prefix_len) {
	    lcr_rec = shm_malloc(sizeof(struct lcr_info));
	    if (lcr_rec == NULL) {
		LM_ERR("no shared memory for lcr_info\n");
		return 0;
	    }
	    memset(lcr_rec, 0, sizeof(struct lcr_info));
	    lcr_rec->prefix_len = prefix_len;
	    lcr_rec->next = this;
	    *previous = lcr_rec;
	    return 1;
	}
	previous = &(this->next);
	this = this->next;
    }

    lcr_rec = shm_malloc(sizeof(struct lcr_info));
    if (lcr_rec == NULL) {
	LM_ERR("no shared memory for lcr_info\n");
	return 0;
    }
    memset(lcr_rec, 0, sizeof(struct lcr_info));
    lcr_rec->prefix_len = prefix_len;
    lcr_rec->next = NULL;
    *previous = lcr_rec;
    return 1;
}


/*
 * Reload gws to unused gw table, lcrs to unused lcr hash table, and
 * prefix lens to a new prefix_len list.  When done, make these tables
 * and list the current ones.
 */
int reload_gws_and_lcrs(int lcr_id)
{
    unsigned int i, n, port, strip, tag_len, prefix_len, from_uri_len,
	grp_id,	grp_cnt, priority, flags, first_gw, weight, gw_cnt,
	hostname_len, defunct_until;
    struct in_addr ip_addr;
    uri_type scheme;
    uri_transport transport;
    db1_con_t* dbh;
    char *ip_string, *hostname, *tag, *prefix, *from_uri;
    db1_res_t* res = NULL;
    db_row_t* row;
    db_key_t key_cols[1];
    db_op_t op[1];
    db_val_t vals[1];
    db_key_t gw_cols[11];
    db_key_t lcr_cols[4];
    pcre *from_uri_re;
    struct gw_grp gw_grps[MAX_NO_OF_GWS];
    struct gw_info *gws, *gwtp_tmp;
    struct lcr_info **lcrs, **lcrtp_tmp;
	
    key_cols[0] = &lcr_id_col;
    op[0] = OP_EQ;
    VAL_TYPE(vals) = DB1_INT;
    VAL_NULL(vals) = 0;
    VAL_INT(vals) = lcr_id;

    gw_cols[0] = &ip_addr_col;
    gw_cols[1] = &port_col;
    gw_cols[2] = &uri_scheme_col;
    gw_cols[3] = &transport_col;
    gw_cols[4] = &strip_col;
    gw_cols[5] = &tag_col;
    gw_cols[6] = &grp_id_col;
    gw_cols[7] = &flags_col;
    gw_cols[8] = &weight_col;
    gw_cols[9] = &hostname_col;
    gw_cols[10] = &defunct_col;

    lcr_cols[0] = &prefix_col;
    lcr_cols[1] = &from_uri_col;
    lcr_cols[2] = &grp_id_col;
    lcr_cols[3] = &priority_col;

    /* Reload gws */

    gws = gwtp[0];

    if (lcr_dbf.init == 0) {
	LM_CRIT("unbound database\n");
	return -1;
    }
    dbh = lcr_dbf.init(&db_url);
    if (dbh == 0) {
	LM_ERR("unable to open database connection\n");
	return -1;
    }

    if (lcr_dbf.use_table(dbh, &gw_table) < 0) {
	LM_ERR("error while trying to use gw table\n");
	return -1;
    }

    if (lcr_dbf.query(dbh, key_cols, op, vals, gw_cols, 1, 11, 0, &res) < 0) {
	LM_ERR("failed to query gw data\n");
	lcr_dbf.close(dbh);
	return -1;
    }

    if (RES_ROW_N(res) + 1 > MAX_NO_OF_GWS) {
	LM_ERR("too many gateways\n");
	goto gw_err;
    }

    for (i = 0; i < RES_ROW_N(res); i++) {
	row = RES_ROWS(res) + i;
	if (VAL_NULL(ROW_VALUES(row)) ||
	    (VAL_TYPE(ROW_VALUES(row)) != DB1_STRING)) {
	    LM_ERR("gw ip address at row <%u> is null or not string\n", i);
	    goto gw_err;
	}
	ip_string = (char *)VAL_STRING(ROW_VALUES(row));
	if (inet_aton(ip_string, &ip_addr) == 0) {
	    LM_ERR("gateway ip address <%s> at row <%u> is invalid\n",
		   ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 1)) {
	    port = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 1) != DB1_INT) {
		LM_ERR("port of gw <%s> at row <%u> is not int\n",
		       ip_string, i);
		goto gw_err;
	    }
	    port = (unsigned int)VAL_INT(ROW_VALUES(row) + 1);
	}
	if (port > 65536) {
	    LM_ERR("port <%d> of gw <%s> at row <%u> is too large\n",
		   port, ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 2)) {
	    scheme = SIP_URI_T;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 2) != DB1_INT) {
		LM_ERR("uri scheme of gw <%s> at row <%u> is not int\n",
		       ip_string, i);
		goto gw_err;
	    }
	    scheme = (uri_type)VAL_INT(ROW_VALUES(row) + 2);
	}
	if ((scheme != SIP_URI_T) && (scheme != SIPS_URI_T)) {
	    LM_ERR("unknown or unsupported URI scheme <%u> of gw <%s> at "
		   "row <%u>\n", (unsigned int)scheme, ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 3)) {
	    transport = PROTO_NONE;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 3) != DB1_INT) {
		LM_ERR("transport of gw <%s> at row <%u> is not int\n",
		       ip_string, i);
		goto gw_err;
	    }
	    transport = (uri_transport)VAL_INT(ROW_VALUES(row) + 3);	
	}
	if ((transport != PROTO_UDP) && (transport != PROTO_TCP) &&
	    (transport != PROTO_TLS) && (transport != PROTO_SCTP) &&
	    (transport != PROTO_NONE)) {
	    LM_ERR("unknown or unsupported transport <%u> of gw <%s> at "
		   " row <%u>\n", (unsigned int)transport, ip_string, i);
	    goto gw_err;
	}
	if ((scheme == SIPS_URI_T) && (transport == PROTO_UDP)) {
	    LM_ERR("wrong transport <%u> for SIPS URI scheme of gw <%s> at "
		   "row <%u>\n", transport, ip_string, i); 
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 4)) {
	    strip = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 4) != DB1_INT) {
		LM_ERR("strip count of gw <%s> at row <%u> is not int\n",
		       ip_string, i);
		goto gw_err;
	    }
	    strip = (unsigned int)VAL_INT(ROW_VALUES(row) + 4);
	}
	if (strip > MAX_USER_LEN) {
	    LM_ERR("strip count <%u> of gw <%s> at row <%u> it too large\n",
		   strip, ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 5)) {
	    tag_len = 0;
	    tag = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 5) != DB1_STRING) {
		LM_ERR("tag of gw <%s> at row <%u> is not string\n",
		       ip_string, i);
		goto gw_err;
	    }
	    tag = (char *)VAL_STRING(ROW_VALUES(row) + 5);
	    tag_len = strlen(tag);
	}
	if (tag_len > MAX_TAG_LEN) {
	    LM_ERR("tag length <%u> of gw <%s> at row <%u> it too large\n",
		   tag_len, ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 6)) {
	    grp_id = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 6) != DB1_INT) {
		LM_ERR("grp_id of gw <%s> at row <%u> is not int\n",
		       ip_string, i);
		goto gw_err;
	    }
	    grp_id = VAL_INT(ROW_VALUES(row) + 6);
	}
	if (!VAL_NULL(ROW_VALUES(row) + 7) &&
	    (VAL_TYPE(ROW_VALUES(row) + 7) == DB1_INT)) {
	    flags = (unsigned int)VAL_INT(ROW_VALUES(row) + 7);
	} else {
	    LM_ERR("flags of gw <%s> at row <%u> is NULL or not int\n",
		   ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 8)) {
	    weight = 1;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 8) != DB1_INT) {
		LM_ERR("weight of gw <%s> at row <%u> is not int\n",
		       ip_string, i);
		goto gw_err;
	    }
	    weight = (unsigned int)VAL_INT(ROW_VALUES(row) + 8);
	}
	if ((weight < 1) || (weight > 254)) {
	    LM_ERR("weight <%d> of gw <%s> at row <%u> is not 1-254\n",
		   weight, ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 9)) {
	    hostname_len = 0;
	    hostname = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 9) != DB1_STRING) {
		LM_ERR("hostname of gw <%s> at row <%u> is not string\n",
		       ip_string, i);
		goto gw_err;
	    }
	    hostname = (char *)VAL_STRING(ROW_VALUES(row) + 9);
	    hostname_len = strlen(hostname);
	}
	if (hostname_len > MAX_HOST_LEN) {
	    LM_ERR("hostname length <%u> of gw <%s> at row <%u> it too large\n",
		   hostname_len, ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 10)) {
	    defunct_until = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 10) != DB1_INT) {
		LM_ERR("defunct of gw <%s> at row <%u> is not int\n",
		       ip_string, i);
		goto gw_err;
	    }
	    defunct_until = (unsigned int)VAL_INT(ROW_VALUES(row) + 10);
	}
	if (!insert_gw(gws, i + 1, (unsigned int)ip_addr.s_addr, 
		       hostname, hostname_len, grp_id,
		       ip_string, port, scheme, transport, flags, strip,
		       tag, tag_len, weight, defunct_until)) {
	    goto gw_err;
	}
    }

    lcr_dbf.free_result(dbh, res);
    res = NULL;
    
    gw_cnt = i;

    qsort(&(gws[1]), gw_cnt, sizeof(struct gw_info), comp_gw_grps);
    gws[0].ip_addr = gw_cnt;
    gws[gw_cnt + 1].ip_addr = 0;
    link_gw_grps(gws, gw_grps, &grp_cnt);

    for (i = 0; i < grp_cnt; i++) {
	LM_DBG("lcr_id %u: gw_grps[%d].grp_id <%d>, gw_grps[%d].first <%d>\n",
	       lcr_id, i, gw_grps[i].grp_id, i, gw_grps[i].first);
    }

    /* Reload lcrs */

    lcrs = lcrtp[0];

    lcr_hash_table_contents_free(lcrs);

    if (lcr_dbf.use_table(dbh, &lcr_table) < 0) {
	LM_ERR("error while trying to use lcr table\n");
	return -1;
    }

    if (DB_CAPABILITY(lcr_dbf, DB_CAP_FETCH)) {
	if (lcr_dbf.query(dbh, key_cols, op, vals, lcr_cols, 1, 4, 0, 0) < 0) {
	    LM_ERR("db query on lcr table failed\n");
	    lcr_dbf.close(dbh);
	    return -1;
	}
	if (lcr_dbf.fetch_result(dbh, &res, fetch_rows_param) < 0) {
	    LM_ERR("failed to fetch rows from lcr table\n");
	    lcr_dbf.close(dbh);
	    return -1;
	}
    } else {
	if (lcr_dbf.query(dbh, key_cols, op, vals, lcr_cols, 1, 4, 0, &res)
	    < 0) {
	    LM_ERR("db query on lcr table failed\n");
	    lcr_dbf.close(dbh);
	    return -1;
	}
    }

    n = 0;
    from_uri_re = 0;
    
    do {
	LM_DBG("loading, cycle %d with <%d> rows", n++, RES_ROW_N(res));
	for (i = 0; i < RES_ROW_N(res); i++) {
	    from_uri_re = 0;
	    row = RES_ROWS(res) + i;
	    if (VAL_NULL(ROW_VALUES(row)) == 1) {
		prefix_len = 0;
		prefix = 0;
	    } else {
		if (VAL_TYPE(ROW_VALUES(row)) != DB1_STRING) {
		    LM_ERR("lcr prefix at row <%u> is not string\n", i);
		    goto lcr_err;
		}
		prefix = (char *)VAL_STRING(ROW_VALUES(row));
		prefix_len = strlen(prefix);
	    }
	    if (prefix_len > MAX_PREFIX_LEN) {
		LM_ERR("length <%u> of lcr prefix at row <%u> is too large\n",
		       prefix_len, i);
		goto lcr_err;
	    }
	    if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
		from_uri_len = 0;
		from_uri = 0;
	    } else {
		if (VAL_TYPE(ROW_VALUES(row) + 1) != DB1_STRING) {
		    LM_ERR("lcr from_uri at row <%u> is not string\n", i);
		    goto lcr_err;
		}
		from_uri = (char *)VAL_STRING(ROW_VALUES(row) + 1);
		from_uri_len = strlen(from_uri);
	    }
	    if (from_uri_len > MAX_URI_LEN) {
		LM_ERR("length <%u> of lcr from_uri at row <%u> is too large\n",
		       from_uri_len, i);
		goto lcr_err;
	    }
	    if (from_uri_len > 0) {
		from_uri_re = reg_ex_comp(from_uri);
		if (from_uri_re == 0) {
		    LM_ERR("failed to compile lcr from_uri <%s> at row <%u>\n",
			   from_uri, i);
		    goto lcr_err;
		}
	    } else {
		from_uri_re = 0;
	    }
	    if ((VAL_NULL(ROW_VALUES(row) + 2) == 1) ||
		(VAL_TYPE(ROW_VALUES(row) + 2) != DB1_INT)) {
		LM_ERR("lcr grp_id at row <%u> is null or not int\n", i);
		goto lcr_err;
	    }
	    grp_id = (unsigned int)VAL_INT(ROW_VALUES(row) + 2);
	    first_gw = find_first_gw(gw_grps, grp_cnt, grp_id);
	    if (first_gw == 0) {
		LM_ERR("gw grp_id <%u> of prefix <%.*s> has no gateways\n",
		       grp_id, (int)prefix_len, prefix);
		goto lcr_err;
	    }
	    if ((VAL_NULL(ROW_VALUES(row) + 3) == 1) ||
		(VAL_TYPE(ROW_VALUES(row) + 3) != DB1_INT)) {
		LM_ERR("lcr priority at row <%u> is null or not int\n", i);
		goto lcr_err;
	    }
	    priority = (unsigned int)VAL_INT(ROW_VALUES(row) + 3);

	    if (!lcr_hash_table_insert(lcrs, prefix_len, prefix,
				       from_uri_len, from_uri, from_uri_re,
				       grp_id, first_gw, priority) ||
		!prefix_len_insert(lcrs, prefix_len)) {
		lcr_hash_table_contents_free(lcrs);
		goto lcr_err;
	    }
	}
	if (DB_CAPABILITY(lcr_dbf, DB_CAP_FETCH)) {
	    if (lcr_dbf.fetch_result(dbh, &res, fetch_rows_param) < 0) {
		LM_ERR("fetching of rows from lcr table failed\n");
		goto lcr_err;
	    }
	} else {
	    break;
	}
    } while (RES_ROW_N(res) > 0);

    lcr_dbf.free_result(dbh, res);
    lcr_dbf.close(dbh);

    /* Swap gw and lcr hash table with index id with temporary table */  
    gwtp_tmp = gwtp[lcr_id];
    lcrtp_tmp = lcrtp[lcr_id];
    gwtp[lcr_id] = gwtp[0];
    lcrtp[lcr_id] = lcrtp[0];
    gwtp[0] = gwtp_tmp;
    lcrtp[0] = lcrtp_tmp;

    return 1;

 lcr_err:
    if (from_uri_re) shm_free(from_uri_re);

 gw_err:
    lcr_dbf.free_result(dbh, res);
    lcr_dbf.close(dbh);
    return -1;
}


/* Print gateways from gws table */
int mi_print_gws(struct mi_node* rpl)
{
    unsigned int i, j;
    struct mi_attr* attr;
    uri_transport transport;
    char *transp;
    struct mi_node* node;
    struct ip_addr address;
    char* p;
    int len;
    struct gw_info *gws;

    for (j = 1; j <= lcr_count; j++) {

	gws = gwtp[j];

	for (i = 1; i <= gws->ip_addr; i++) {

	    node = add_mi_node_child(rpl,0 ,"GW", 2, 0, 0);
	    if (node == NULL) goto err;

	    p = int2str(j, &len );
	    attr = add_mi_attr(node, MI_DUP_VALUE, "LCR_ID", 6, p, len );
	    if (attr == NULL) goto err;

	    p = int2str((unsigned long)gws[i].grp_id, &len );
	    attr = add_mi_attr(node, MI_DUP_VALUE, "GRP_ID", 6, p, len );
	    if (attr == NULL) goto err;

	    address.af = AF_INET;
	    address.len = 4;
	    address.u.addr32[0] = gws[i].ip_addr;
	    attr = addf_mi_attr(node, 0, "IP_ADDR", 6, "%s",
				ip_addr2a(&address));
	    if (attr == NULL) goto err;

	    attr = add_mi_attr(node, MI_DUP_VALUE, "HOSTNAME", 8,
			       gws[i].hostname, gws[i].hostname_len );
	    if (attr == NULL) goto err;

	    if (gws[i].port > 0) {
		p = int2str((unsigned long)gws[i].port, &len );
		attr = add_mi_attr(node, MI_DUP_VALUE, "PORT", 4, p, len);
	    } else {
		attr = add_mi_attr(node, MI_DUP_VALUE, "PORT", 4, (char *)0, 0);
	    }	    
	    if (attr == NULL) goto err;

	    if (gws[i].scheme == SIP_URI_T) {
		attr = add_mi_attr(node, MI_DUP_VALUE, "SCHEME", 6, "sip", 3);
	    } else {
		attr = add_mi_attr(node, MI_DUP_VALUE, "SCHEME", 6, "sips", 4);
	    }
	    if (attr == NULL) goto err;

	    transport = gws[i].transport;
	    switch (transport) {
	    case PROTO_UDP:
		transp= "udp";
		break;
	    case PROTO_TCP:
		transp= "tcp";
		break;
	    case PROTO_TLS:
		transp= "tls";
		break;
	    case PROTO_SCTP:
		transp= "sctp";
		break;
	    default:
		transp = "";
	    }
	    attr = add_mi_attr(node, MI_DUP_VALUE, "TRANSPORT", 9,
			       transp, strlen(transp));
	    if (attr == NULL) goto err;
	    
	    p = int2str((unsigned long)gws[i].strip, &len);
	    attr = add_mi_attr(node, MI_DUP_VALUE, "STRIP", 5, p, len);
	    if (attr == NULL) goto err;
	    
	    attr = add_mi_attr(node, MI_DUP_VALUE, "TAG", 3,
			       gws[i].tag, gws[i].tag_len);
	    if (attr == NULL) goto err;

	    p = int2str((unsigned long)gws[i].weight, &len);
	    attr = add_mi_attr(node, MI_DUP_VALUE, "WEIGHT", 6, p, len);
	    if (attr == NULL) goto err;
	    
	    p = int2str((unsigned long)gws[i].flags, &len);
	    attr = add_mi_attr(node, MI_DUP_VALUE, "FLAGS", 5, p, len);
	    if (attr == NULL) goto err;
	    
	    p = int2str((unsigned long)gws[i].defunct_until, &len);
	    attr = add_mi_attr(node, MI_DUP_VALUE, "DEFUNCT_UNTIL", 13, p, len);
	    if (attr == NULL) goto err;
	}
    }

    return 0;

 err:
    return -1;
}

/* Print lcrs from lcrs table */
int mi_print_lcrs(struct mi_node* rpl)
{
    unsigned int i, j;
    struct mi_attr* attr;
    struct mi_node* node;
    char* p;
    int len;
    struct lcr_info **lcrs, *lcr_rec;

    for (j = 1; j <= lcr_count; j++) {

	lcrs = lcrtp[j];

	for (i = 0; i < lcr_hash_size_param; i++) {

	    lcr_rec = lcrs[i];

	    while (lcr_rec) {

		node = add_mi_node_child(rpl, 0, "RULE", 4, 0, 0);
		if (node == NULL) goto err;

		p = int2str(j, &len );
		attr = add_mi_attr(node, MI_DUP_VALUE, "LCR_ID", 6, p, len );
		if (attr == NULL) goto err;

		attr = add_mi_attr(node, 0, "PREFIX", 6, lcr_rec->prefix,
				   lcr_rec->prefix_len);
		if (attr == NULL) goto err;
		
		attr = add_mi_attr(node, 0, "FROM_URI", 8, lcr_rec->from_uri,
				   lcr_rec->from_uri_len);
		if (attr == NULL) goto err;
	
		p = int2str((unsigned long)lcr_rec->grp_id, &len );
		attr = add_mi_attr(node, MI_DUP_VALUE, "GRP_ID", 6, p, len);
		if (attr == NULL) goto err;

		p = int2str((unsigned long)lcr_rec->priority, &len);
		attr = add_mi_attr(node, MI_DUP_VALUE, "PRIORITY", 8, p, len);
		if (attr == NULL) goto err;

		lcr_rec = lcr_rec->next;
	    }
	}

	lcr_rec = lcrs[lcr_hash_size_param];

	while (lcr_rec) {

	    node = add_mi_node_child(rpl, 0, "PREFIX_LENS", 11, 0, 0);
	    if (node == NULL) goto err;

	    p = int2str((unsigned long)lcr_rec->prefix_len, &len );
	    attr = add_mi_attr(node, MI_DUP_VALUE, "PREFIX_LEN", 10, p, len);
	    if (attr == NULL) goto err;

	    lcr_rec = lcr_rec->next;
	}
    }

    return 0;

 err:
    return -1;
}

inline int encode_avp_value(char *value, uri_type scheme, unsigned int strip,
			    char *tag, unsigned int tag_len,
			    unsigned int ip_addr, char *hostname,
			    unsigned int hostname_len, unsigned int port,
			    uri_transport transport, unsigned int flags)
{
    char *at, *string;
    int len;
    
    /* scheme */
    at = value;
    string = int2str(scheme, &len);
    append_str(at, string, len);
    append_chr(at, '|');
    /* strip */
    string = int2str(strip, &len);
    append_str(at, string, len);
    append_chr(at, '|');
    /* tag */
    append_str(at, tag, tag_len);
    append_chr(at, '|');
    /* ip_addr */
    string = int2str(ip_addr, &len);
    append_str(at, string, len);
    append_chr(at, '|');
    /* hostname */
    append_str(at, hostname, hostname_len);
    append_chr(at, '|');
    /* port */
    string = int2str(port, &len);
    append_str(at, string, len);
    append_chr(at, '|');
    /* transport */
    string = int2str(transport, &len);
    append_str(at, string, len);
    append_chr(at, '|');
    /* flags */
    string = int2str(flags, &len);
    append_str(at, string, len);
    return at - value;
}

inline int decode_avp_value(char *value, str *scheme, unsigned int *strip,
			    str *tag, unsigned int *addr, str *hostname,
			    str *port, str *transport, unsigned int *flags)
{
    unsigned int u;
    str s;
    char *sep;

    /* scheme */
    s.s = value;
    sep = index(s.s, '|');
    if (sep == NULL) {
	LM_ERR("scheme was not found in AVP value\n");
	return 0;
    }
    s.len = sep - s.s;
    str2int(&s, &u);
    if (u == SIP_URI_T) {
	scheme->s = "sip:";
	scheme->len = 4;
    } else {
	scheme->s = "sips:";
	scheme->len = 5;
    }
    /* strip */
    s.s = sep + 1;
    sep = index(s.s, '|');
    if (sep == NULL) {
	LM_ERR("strip was not found in AVP value\n");
	return 0;
    }
    s.len = sep - s.s;
    str2int(&s, strip);
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
    str2int(&s, addr);
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
	LM_ERR("scheme was not found in AVP value\n");
	return 0;
    }
    port->len = sep - port->s;
    /* transport */
    s.s = sep + 1;
    sep = index(s.s, '|');
    if (sep == NULL) {
	LM_ERR("transport was not found in AVP value\n");
	return 0;
    }
    s.len = sep - s.s;
    str2int(&s, &u);
    switch (u) {
    case PROTO_NONE:
    case PROTO_UDP:
	transport->s = (char *)0;
	transport->len = 0;
	break;
    case PROTO_TCP:
	transport->s = ";transport=tcp";
	transport->len = 14;
	break;
    case PROTO_TLS:
	transport->s = ";transport=tls";
	transport->len = 14;
    default:
	transport->s = ";transport=sctp";
	transport->len = 15;
	break;
    }
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
    unsigned int i, index, strip, hostname_len;
    int tag_len;
    str value;
    char encoded_value[MAX_URI_LEN];
    int_str val;

    delete_avp(gw_uri_avp_type|AVP_VAL_STR, gw_uri_avp);

    for (i = 0; i < gw_cnt; i++) {
	if (matched_gws[i].duplicate == 1) continue;
	index = matched_gws[i].gw_index;
      	hostname_len = gws[index].hostname_len;
	strip = gws[index].strip;
	if (strip > ruri_user->len) {
	    LM_ERR("strip count of gw is too large <%u>\n", strip);
	    goto skip;
	}
	tag_len = gws[index].tag_len;
	if (5 /* scheme */ + 4 /* strip */ + tag_len + 1 /* @ */ +
	    ((hostname_len > 15)?hostname_len:15) + 6 /* port */ +
	    15 /* transport */ + 10 /* flags */ + 7 /* separators */
	    > MAX_URI_LEN) {
	    LM_ERR("too long AVP value\n");
	    goto skip;
	}
	value.len = 
	    encode_avp_value(encoded_value, gws[index].scheme, strip,
			     gws[index].tag, tag_len, gws[index].ip_addr,
			     gws[index].hostname, hostname_len,
			     gws[index].port, gws[index].transport,
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
static int load_gws(struct sip_msg* _m, char *_lcr_id, char *_from_uri)
{
    str ruri_user, from_uri;
    int i, j, lcr_id;
    unsigned int gw_index, gw_count, now, ip_addr;
    int_str val;
    struct matched_gw_info matched_gws[MAX_NO_OF_GWS + 1];
    struct lcr_info **lcrs, *lcr_rec, *pl;
    struct gw_info *gws;

    /* Get and check parameter values */
    if (get_int_fparam(&lcr_id, _m, (fparam_t *)_lcr_id) != 0) {
	LM_ERR("no lcr_id param value\n");
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }

    if (get_str_fparam(&from_uri, _m, (fparam_t *)_from_uri) != 0) {
	LM_ERR("no from_uri parameter value\n");
	return -1;
    }
    if (from_uri.len == 0) {
	LM_ERR("empry from_uri param value\n");
	return -1;
    }

    /* Use gws and lcr rules with index lcr_id */
    gws = gwtp[lcr_id];
    lcrs = lcrtp[lcr_id];

    /* Find Request-URI user */
    if ((parse_sip_msg_uri(_m) < 0) || (!_m->parsed_uri.user.s)) {
	LM_ERR("error while parsing R-URI\n");
	return -1;
    }
    ruri_user = _m->parsed_uri.user;

    /*
     * Find lcr entries that match based on prefix and from_uri and collect
     * gateways of matching entries into matched_gws array so that each
     * gateway appears in the array only once.
     */

    pl = lcrs[lcr_hash_size_param];
    gw_index = 0;
    gw_count = gws[0].ip_addr;

    if (defunct_capability > 0) {
	delete_avp(defunct_gw_avp_type, defunct_gw_avp);
    }

    now = time((time_t *)NULL);

    while (pl) {
	if (ruri_user.len < pl->prefix_len) {
	    pl = pl->next;
	    continue;
	}
	lcr_rec = lcr_hash_table_lookup(lcrs, pl->prefix_len, ruri_user.s);
	while (lcr_rec) {
	    /* Match prefix */
	    if ((lcr_rec->prefix_len == pl->prefix_len) && 
		(strncmp(lcr_rec->prefix, ruri_user.s, pl->prefix_len) == 0)) {
		/* Match from uri */
		if ((lcr_rec->from_uri_len == 0) ||
		    (pcre_exec(lcr_rec->from_uri_re, NULL, from_uri.s,
			       from_uri.len, 0, 0, NULL, 0) >= 0)) {
		    /* Load unique gws of the group of this lcr entry */
		    j = lcr_rec->first_gw;
		    while (j) {
                        /* If this gw is defunct, skip it */
		        if (gws[j].defunct_until > now) goto gw_found;
			matched_gws[gw_index].gw_index = j;
			matched_gws[gw_index].prefix_len = pl->prefix_len;
			matched_gws[gw_index].priority = lcr_rec->priority;
			matched_gws[gw_index].weight = gws[j].weight *
			    (rand() >> 8);
			matched_gws[gw_index].duplicate = 0;
			LM_DBG("added matched_gws[%d]=[%u, %u, %u, %u]\n",
			       gw_index, j, pl->prefix_len, lcr_rec->priority,
			       matched_gws[gw_index].weight);
			gw_index++;
		    gw_found:
			j = gws[j].next;
		    }
		}
	    }
	    lcr_rec = lcr_rec->next;
	}
	pl = pl->next;
    }

    /* Sort gateways in reverse order based on prefix_len, priority,
       and randomized weight */
    qsort(matched_gws, gw_index, sizeof(struct matched_gw_info), comp_matched);

    /* Remove duplicate gws */
    for (i = gw_index - 1; i >= 0; i--) {
	if (matched_gws[i].duplicate == 1) continue;
	ip_addr = gws[matched_gws[i].gw_index].ip_addr;
	for (j = i - 1; j >= 0; j--) {
	    if (gws[matched_gws[j].gw_index].ip_addr == ip_addr) {
		matched_gws[j].duplicate = 1;
	    }
	}
    }

    /* Add gateways into gw_uris_avp */
    add_gws_into_avps(gws, matched_gws, gw_index, &ruri_user);

    /* Add lcr_id into AVP */
    if (defunct_capability > 0) {
	delete_avp(lcr_id_avp_type, lcr_id_avp);
	val.n = lcr_id;
	add_avp(lcr_id_avp_type, lcr_id_avp, val);
    }
    
    return 1;
}


/* Generate Request-URI and Destination URI */
static int generate_uris(char *r_uri, str *r_uri_user, unsigned int *r_uri_len,
			 char *dst_uri, unsigned int *dst_uri_len,
			 unsigned int *addr, unsigned int *flags)
{
    int_str gw_uri_val;
    struct usr_avp *gu_avp;
    str scheme, tag, hostname, port, transport, addr_str;
    char *at;
    unsigned int strip;
    struct ip_addr a;
    
    gu_avp = search_first_avp(gw_uri_avp_type, gw_uri_avp, &gw_uri_val, 0);

    if (!gu_avp) return 0; /* No more gateways left */

    decode_avp_value(gw_uri_val.s.s, &scheme, &strip, &tag, addr,
		     &hostname, &port, &transport, flags);

    a.af = AF_INET;
    a.len = 4;
    a.u.addr32[0] = *addr;
    addr_str.s = ip_addr2a(&a);
    addr_str.len = strlen(addr_str.s);
    
    if (scheme.len + r_uri_user->len - strip + tag.len + 1 /* @ */ +
	((hostname.len > 15)?hostname.len:15) + 1 /* : */ +
	port.len + transport.len + 1 /* null */ > MAX_URI_LEN) {
	LM_ERR("too long Request URI or DST URI\n");
	return -1;
    }

    at = r_uri;
    
    append_str(at, scheme.s, scheme.len);
    append_str(at, tag.s, tag.len);
	
    if (strip > r_uri_user->len) {
	LM_ERR("strip count <%u> is larger than R-URI user <%.*s>\n",
	       strip, r_uri_user->len, r_uri_user->s);
	return -1;
    }
    append_str(at, r_uri_user->s + strip, r_uri_user->len - strip);

    append_chr(at, '@');
	
    if (hostname.len == 0) {
	append_str(at, addr_str.s, addr_str.len);
	if (port.len > 0) {
	    append_chr(at, ':');
	    append_str(at, port.s, port.len);
	}
	if (transport.len > 0) {
	    append_str(at, transport.s, transport.len);
	}
	*at = '\0';
	*r_uri_len = at - r_uri;
	*dst_uri_len = 0;
    } else {
	append_str(at, hostname.s, hostname.len);
	*at = '\0';
	*r_uri_len = at - r_uri;
	at = dst_uri;
	append_str(at, scheme.s, scheme.len);
	append_str(at, addr_str.s, addr_str.len);
	if (port.len > 0) {
	    append_chr(at, ':');
	    append_str(at, port.s, port.len);
	}
	if (transport.len > 0) {
	    append_str(at, transport.s, transport.len);
	}
	*at = '\0';
	*dst_uri_len = at - dst_uri;
    }

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
    int_str lcr_id_val, ip_addr_val;
    struct gw_info *gws, *res, *gw;
    unsigned int ip_addr, defunct_until;
    int defunct_period;

    /* Check defunct gw capability */
    if (defunct_capability == 0) {
	LM_ERR("no defunct gw capability, activate by setting "
	       "defunct_capability module param\n");
	return -1;
    }

    /* Get parameter value */
    if (get_int_fparam(&defunct_period, _m, (fparam_t *)_defunct_period) != 0) {
	LM_ERR("no defunct_period param value\n");
	return -1;
    }
    if (defunct_period < 1) {
	LM_ERR("invalid defunct_period param value\n");
	return -1;
    }

    /* Get AVP values */
    if (search_first_avp(lcr_id_avp_type, lcr_id_avp, &lcr_id_val, 0)
	== NULL) {
	LM_ERR("lcr_id_avp was not found\n");
	return -1;
    }
    if (search_first_avp(defunct_gw_avp_type, defunct_gw_avp,
			 &ip_addr_val, 0) == NULL) {
	LM_ERR("defucnt_gw_avp was not found\n");
	return -1;
    }
    ip_addr = ip_addr_val.n;
    
    /* Use gws with index lcr_id */
    gws = gwtp[lcr_id_val.n];

    /* Search for gw address */
    res = (struct gw_info *)bsearch(&ip_addr, &(gws[1]), gws[0].ip_addr,
				    sizeof(struct gw_info), comp_gws);
    if (res == NULL) return 1;

    /* Defunct gw(s) */
    defunct_until = time((time_t *)NULL) + defunct_period;
    LM_DBG("defuncting gw %u in grp %u\n", res->ip_addr, res->grp_id);
    res->defunct_until = defunct_until;
    gw = res - 1;
    while (gw->ip_addr == ip_addr) {
	LM_DBG("defuncting gw %u in grp %u\n", gw->ip_addr, gw->grp_id);
	gw->defunct_until = defunct_until;
	gw = gw - 1;
    }
    gw = res + 1;
    while (gw->ip_addr == ip_addr) {
	LM_DBG("defuncting gw %u in grp %u\n", gw->ip_addr, gw->grp_id);
	gw->defunct_until = defunct_until;
	gw = gw + 1;
    }

    return 1;
}    


/*
 * When called first time in route block, rewrites scheme, host, port, and
 * transport parts of R-URI based on first gw_uri_avp value, which is then
 * destroyed.  Saves R-URI user to ruri_user_avp for later use.
 *
 * On other calls appends a new branch to request, where scheme, host, port,
 * and transport of URI are taken from the first gw_uri_avp value, 
 * which is then destroyed. URI user is taken either from R-URI (first
 * call in failure route block) or from ruri_user_avp value saved earlier.
 *
 * Returns 1 upon success and -1 upon failure.
 */
static int next_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
    int_str ruri_user_val, val;
    struct action act;
    struct run_act_ctx ra_ctx;
    struct usr_avp *ru_avp;
    int rval;
    str uri_str;
    unsigned int flags, r_uri_len, dst_uri_len, addr;
    char r_uri[MAX_URI_LEN], dst_uri[MAX_URI_LEN];

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
	if (generate_uris(r_uri, &(_m->parsed_uri.user), &r_uri_len, dst_uri,
			  &dst_uri_len, &addr, &flags) != 1) {
	    return -1;
	}

	/* Save Request-URI user into uri_user_avp for use in subsequent
         * invocations. */
	val.s = _m->parsed_uri.user;
	add_avp(ruri_user_avp_type|AVP_VAL_STR, ruri_user_avp, val);
	LM_DBG("added ruri_user_avp <%.*s>\n", val.s.len, val.s.s);

    } else {

	/* Subsequent invocation either in route or failure route block. */

	/* Take Request-URI user from ruri_user_avp and generate Request
         * and Destination URIs. */
	if (generate_uris(r_uri, &(ruri_user_val.s), &r_uri_len, dst_uri,
			  &dst_uri_len, &addr, &flags) != 1) {
	    return -1;
	}
    }

    if ((route_type == REQUEST_ROUTE) && (ru_avp == NULL)) {

	/* First invocation in route block => Rewrite Request URI. */
	memset(&act, '\0', sizeof(act));
	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = r_uri;
	init_run_actions_ctx(&ra_ctx);
	rval = do_action(&ra_ctx, &act, _m);
	if (rval != 1) {
	    LM_ERR("do_action failed with return value <%d>\n", rval);
	    return -1;
	}

    } else {
	
	/* Subsequent invocation in route block or any invocation in
         * failure route block => append new branch. */
	uri_str.s = r_uri;
	uri_str.len = r_uri_len;
	LM_DBG("appending branch <%.*s>\n", uri_str.len, uri_str.s);
	if (append_branch(_m, &uri_str, 0, 0, Q_UNSPECIFIED, 0, 0) == -1) {
	    LM_ERR("when appending branch <%.*s>\n", uri_str.len, uri_str.s);
	    return -1;
	}
    }
    
    /* Set Destination URI if not empty */
    if (dst_uri_len > 0) {
	uri_str.s = dst_uri;
	uri_str.len = dst_uri_len;
	rval = set_dst_uri(_m, &uri_str);
	if (rval != 0) {
	    LM_ERR("calling do_action failed with return value <%d>\n", rval);
	    return -1;
	}
	
    }

    /* Set flags_avp */
    val.n = flags;
    add_avp(flags_avp_type, flags_avp, val);
    LM_DBG("added flags_avp <%u>\n", (unsigned int)val.n);

    /* Add IP addr of selected gw to defunct gw AVP */
    if (defunct_capability > 0) {
	delete_avp(defunct_gw_avp_type, defunct_gw_avp);
	val.n = addr;
	add_avp(defunct_gw_avp_type, defunct_gw_avp, val);
	LM_DBG("added defunct_gw_avp <%u>", addr);
    }
    
    return 1;
}


/*
 * Checks if request comes from a gateway
 */
static int do_from_gw(struct sip_msg* _m, unsigned int lcr_id,
		      unsigned int src_addr)
{
    struct gw_info *res, *gws;
    int_str val;
	
    /* Use gws with index lcr_id */
    gws = gwtp[lcr_id];

    /* Search for gw address */
    res = (struct gw_info *)bsearch(&src_addr, &(gws[1]), gws[0].ip_addr,
				    sizeof(struct gw_info), comp_gws);

    /* Store flags and return result */
    if (res == NULL) {
	LM_DBG("request did not come from gw\n");
	return -1;
    } else {
	LM_DBG("request game from gw\n");
	val.n = res->flags;
	add_avp(flags_avp_type, flags_avp, val);
	LM_DBG("added flags_avp <%u>\n", (unsigned int)val.n);
	return 1;
    }
}


/*
 * Checks if request comes from a gateway taking src_address from reques.
 */
static int from_gw_1(struct sip_msg* _m, char* _lcr_id, char* _s2)
{
    int lcr_id;
    unsigned int src_addr;

    /* Get and check parameter value */
    if (get_int_fparam(&lcr_id, _m, (fparam_t *)_lcr_id) != 0) {
	LM_ERR("no lcr_id param value\n");
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }

    /* Get source address */
    src_addr = _m->rcv.src_ip.u.addr32[0];

    /* Do test */
    return do_from_gw(_m, lcr_id, src_addr);
}


/*
 * Checks if request comes from a gateway taking source address from param.
 */
static int from_gw_2(struct sip_msg* _m, char* _lcr_id, char* _addr)
{
    unsigned int src_addr;
    int lcr_id;
    pv_value_t pv_val;
    struct ip_addr *ip;

    /* Get and check parameter values */
    if (get_int_fparam(&lcr_id, _m, (fparam_t *)_lcr_id) != 0) {
	LM_ERR("no lcr_id param value\n");
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }

    if (_addr && (pv_get_spec_value(_m, (pv_spec_t *)_addr, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_INT) {
	    src_addr = pv_val.ri;
	} else if (pv_val.flags & PV_VAL_STR) {
	    if ((ip = str2ip(&pv_val.rs)) == NULL) {
		LM_DBG("request did not come from gw "
		       "(addr param value is not an IP address)\n");
		return -1;
	    } else {
		src_addr = ip->u.addr32[0];
	    }
	} else {
	    LM_ERR("addr param has no value\n");
	    return -1;
	}
    } else {
	LM_ERR("could not get source address from param\n");
	return -1;
    }

    /* Do test */
    return do_from_gw(_m, lcr_id, src_addr);
}


/*
 * Checks if request comes from any gateway taking source address from request.
 */
static int from_any_gw_0(struct sip_msg* _m, char* _s1, char* _s2)
{
    unsigned int src_addr, i;

    src_addr = _m->rcv.src_ip.u.addr32[0];

    for (i = 1; i <= lcr_count; i++) {
	if (do_from_gw(_m, i, src_addr) == 1) {
	    return i;
	}
    }
    return -1;
}


/*
 * Checks if request comes from a gateway taking source address from param.
 */
static int from_any_gw_1(struct sip_msg* _m, char* _addr, char* _s2)
{
    unsigned int i, src_addr;
    pv_value_t pv_val;
    struct ip_addr *ip;

    /* Get parameter value */
    if (_addr && (pv_get_spec_value(_m, (pv_spec_t *)_addr, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_INT) {
	    src_addr = pv_val.ri;
	} else if (pv_val.flags & PV_VAL_STR) {
	    if ((ip = str2ip(&pv_val.rs)) == NULL) {
		LM_DBG("request did not come from gw "
		       "(addr param value is not an IP address)\n");
		return -1;
	    } else {
		src_addr = ip->u.addr32[0];
	    }
	} else {
	    LM_ERR("addr param has no value\n");
	    return -1;
	}
    } else {
	LM_ERR("could not get source address from param\n");
	return -1;
    }

    /* Do test */
    for (i = 1; i <= lcr_count; i++) {
	if (do_from_gw(_m, i, src_addr) == 1) {
	    return i;
	}
    }
    return -1;
}


/*
 * Checks if in-dialog request goes to gateway
 */
static int do_to_gw(struct sip_msg* _m, unsigned int lcr_id,
		    unsigned int dst_addr)
{
    struct gw_info *res, *gws;

    /* Use gws with index lcr_id */
    gws = gwtp[lcr_id];

    /* Search for gw address */
    res = (struct gw_info *)bsearch(&dst_addr, &(gws[1]), gws[0].ip_addr,
				    sizeof(struct gw_info), comp_gws);

    /* Return result */
    if (res == NULL) {
	LM_DBG("request is not going to gw\n");
	return -1;
    } else {
	LM_DBG("request goes to gw\n");
	return 1;
    }
}


/*
 * Checks if request goes to a gateway taking destination address from request.
 */
static int to_gw_1(struct sip_msg* _m, char* _lcr_id, char* _s2)
{
    int lcr_id;
    unsigned int dst_addr;
    struct ip_addr *ip;

    /* Get and check parameter value */
    if (get_int_fparam(&lcr_id, _m, (fparam_t *)_lcr_id) != 0) {
	LM_ERR("no lcr_id param value\n");
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }

    /* Get destination address */
    if ((_m->parsed_uri_ok == 0) && (parse_sip_msg_uri(_m) < 0)) {
	LM_ERR("while parsing Request-URI\n");
	return -1;
    }
    if (_m->parsed_uri.host.len > 15) {
	LM_DBG("request is not going to gw "
	       "(Request-URI host is not an IP address)\n");
	return -1;
    }
    if ((ip = str2ip(&(_m->parsed_uri.host))) == NULL) {
	LM_DBG("request is not going to gw "
	       "(Request-URI host is not an IP address)\n");
	return -1;
    } else {
	dst_addr = ip->u.addr32[0];
    }

    /* Do test */
    return do_to_gw(_m, lcr_id, dst_addr);
}


/*
 * Checks if request goes to a gateway, taking destination address from param.
 */
static int to_gw_2(struct sip_msg* _m, char* _lcr_id, char* _addr)
{
    int lcr_id;
    unsigned int dst_addr;
    pv_value_t pv_val;
    struct ip_addr *ip;

    /* Get and check parameter values */
    if (get_int_fparam(&lcr_id, _m, (fparam_t *)_lcr_id) != 0) {
	LM_ERR("no lcr_id param value\n");
	return -1;
    }
    if ((lcr_id < 1) || (lcr_id > lcr_count)) {
	LM_ERR("invalid lcr_id parameter value %d\n", lcr_id);
	return -1;
    }

    if (_addr && (pv_get_spec_value(_m, (pv_spec_t *)_addr, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_INT) {
	    dst_addr = pv_val.ri;
	} else if (pv_val.flags & PV_VAL_STR) {
	    if ((ip = str2ip(&pv_val.rs)) == NULL) {
		LM_DBG("request is not going to gw "
		       "(addr param value is not an IP address)\n");
		return -1;
	    } else {
		dst_addr = ip->u.addr32[0];
	    }
	} else {
	    LM_ERR("addr param has no value\n");
	    return -1;
	}
    } else {
	LM_ERR("could not get destination address from param\n");
	return -1;
    }
    
    /* Do test */
    return do_to_gw(_m, lcr_id, dst_addr);
}


/*
 * Checks if request goes to any gateway taking dst_addr from request.
 */
static int to_any_gw_0(struct sip_msg* _m, char* _s1, char* _s2)
{
    unsigned int dst_addr, i;
    struct ip_addr *ip;

    /* Get destination address */
    if ((_m->parsed_uri_ok == 0) && (parse_sip_msg_uri(_m) < 0)) {
	LM_ERR("while parsing Request-URI\n");
	return -1;
    }
    if (_m->parsed_uri.host.len > 15) {
	LM_DBG("request is not going to gw "
	       "(Request-URI host is not an IP address)\n");
	return -1;
    }
    if ((ip = str2ip(&(_m->parsed_uri.host))) == NULL) {
	LM_DBG("request is not going to gw "
	       "(Request-URI host is not an IP address)\n");
	return -1;
    } else {
	dst_addr = ip->u.addr32[0];
    }

    for (i = 1; i <= lcr_count; i++) {
	if (do_to_gw(_m, i, dst_addr) == 1) {
	    return i;
	}
    }
    return -1;
}


/*
 * Checks if request goes to any gateway taking dst_addr from param.
 */
static int to_any_gw_1(struct sip_msg* _m, char* _addr, char* _s2)
{
    unsigned int i, dst_addr;
    pv_value_t pv_val;
    struct ip_addr *ip;

    /* Get parameter value */
    if (_addr && (pv_get_spec_value(_m, (pv_spec_t *)_addr, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_INT) {
	    dst_addr = pv_val.ri;
	} else if (pv_val.flags & PV_VAL_STR) {
	    if ((ip = str2ip(&pv_val.rs)) == NULL) {
		LM_DBG("request did go to any gw "
		       "(addr param value is not an IP address)\n");
		return -1;
	    } else {
		dst_addr = ip->u.addr32[0];
	    }
	} else {
	    LM_ERR("addr param has no value\n");
	    return -1;
	}
    } else {
	LM_ERR("could not get destination address from param\n");
	return -1;
    }

    /* Do test */
    for (i = 1; i <= lcr_count; i++) {
	if (do_to_gw(_m, i, dst_addr) == 1) {
	    return i;
	}
    }
    return -1;
}
