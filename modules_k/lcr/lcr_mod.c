/*
 * $Id$
 *
 * Least Cost Routing module (also implements sequential forking)
 *
 * Copyright (C) 2005-2008 Juha Heinanen
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
#include "../../db/db.h"
#include "../../usr_avp.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/msg_parser.h"
#include "../../action.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "../../ip_addr.h"
#include "../../resolve.h"
#include "../../mi/mi.h"
#include "../../mod_fix.h"
#include "../../socket_info.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "hash.h"
#include "mi.h"

MODULE_VERSION

/*
 * Version of gw and lcr tables required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define GW_TABLE_VERSION 8
#define LCR_TABLE_VERSION 2

/* usr_avp flag for sequential forking */
#define Q_FLAG      (1<<2)

static void destroy(void);       /* Module destroy function */
static int mi_child_init(void);
static int mod_init(void);       /* Module initialization function */
static void free_shared_memory(void);
static int fixstringloadgws(void **param, int param_count);

#define GW_TABLE "gw"

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

#define LCR_TABLE "lcr"

#define PREFIX_COL "prefix"

#define FROM_URI_COL "from_uri"

#define PRIORITY_COL "priority"

#define MAX_NO_OF_GWS 128
#define MAX_TAG_LEN 16
#define MAX_HOST_LEN 64
#define MAX_USER_LEN 64

/* Default module parameter values */
#define DEF_FR_INV_TIMER 90
#define DEF_FR_INV_TIMER_NEXT 30
#define DEF_LCR_HASH_SIZE 128
#define DEF_FETCH_ROWS 2000

/*
 * Type definitions
 */

typedef enum sip_protos uri_transport;

struct gw_info {
    unsigned int ip_addr;
    char hostname[MAX_HOST_LEN];
    unsigned short hostname_len;
    unsigned int port;
    unsigned int grp_id;
    uri_type scheme;
    uri_transport transport;
    unsigned int strip;
    char tag[MAX_TAG_LEN + 1];
    unsigned short tag_len;
    unsigned short weight;
    unsigned int flags;
    unsigned int next;  /* index of next gw in the same group */
};

struct gw_grp {
    unsigned int grp_id;
    unsigned int first;   /* index to first gw of group in gw table */
};

struct matched_gw_info {
    unsigned short gw_index;
    unsigned short prefix_len;
    unsigned short priority;
    unsigned int weight;
};

/*
 * Database variables
 */
static db_con_t* db_handle = 0;   /* Database connection handle */
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
static str lcr_table        = str_init(LCR_TABLE);
static str prefix_col       = str_init(PREFIX_COL);
static str from_uri_col     = str_init(FROM_URI_COL);
static str priority_col     = str_init(PRIORITY_COL);

/* number of rows to fetch at a shot */
static int fetch_rows_param = DEF_FETCH_ROWS;

/* timer */
int fr_inv_timer      = DEF_FR_INV_TIMER;
int fr_inv_timer_next = DEF_FR_INV_TIMER_NEXT;

/* avps */
static char *fr_inv_timer_avp_param = NULL;
static char *gw_uri_avp_param = NULL;
static char *ruri_user_avp_param = NULL;
static char *contact_avp_param = NULL;
static char *rpid_avp_param = NULL;
static char *flags_avp_param = NULL;

/* size of prefix hash table */
unsigned int lcr_hash_size_param = DEF_LCR_HASH_SIZE;

/*
 * Other module types and variables
 */

struct contact {
    str uri;
    qvalue_t q;
    str dst_uri;
    str path;
    unsigned int flags;
    struct socket_info* sock;
    unsigned short q_flag;
    struct contact *next;
};

static int     fr_inv_timer_avp_type;
static int_str fr_inv_timer_avp;
static int     gw_uri_avp_type;
static int_str gw_uri_avp;
static int     ruri_user_avp_type;
static int_str ruri_user_avp;
static int     contact_avp_type;
static int_str contact_avp;
static int     rpid_avp_type;
static int_str rpid_avp;
static int     flags_avp_type;
static int_str flags_avp;

struct gw_info **gws;	/* Pointer to current gw table pointer */
struct gw_info *gws_1;	/* Pointer to gw table 1 */
struct gw_info *gws_2;	/* Pointer to gw table 2 */

struct lcr_info ***lcrs;  /* Pointer to current lcr hash table pointer */
struct lcr_info **lcrs_1; /* Pointer to lcr hash table 1 */
struct lcr_info **lcrs_2; /* Pointer to lcr hash table 2 */


/*
 * Module functions that are defined later
 */
static int load_gws_0(struct sip_msg* _m, char* _s1, char* _s2);
static int load_gws_1(struct sip_msg* _m, char* _s1, char* _s2);
static int load_gws_from_grp(struct sip_msg* _m, char* _s1, char* _s2);
static int next_gw(struct sip_msg* _m, char* _s1, char* _s2);
static int from_gw_0(struct sip_msg* _m, char* _s1, char* _s2);
static int from_gw_1(struct sip_msg* _m, char* _s1, char* _s2);
static int from_gw_grp(struct sip_msg* _m, char* _s1, char* _s2);
static int to_gw(struct sip_msg* _m, char* _s1, char* _s2);
static int to_gw_grp(struct sip_msg* _m, char* _s1, char* _s2);
static int load_contacts (struct sip_msg*, char*, char*);
static int next_contacts (struct sip_msg*, char*, char*);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"load_gws", (cmd_function)load_gws_0, 0, 0, 0, REQUEST_ROUTE},
    {"load_gws", (cmd_function)load_gws_1, 1, fixup_pvar_null,
     fixup_free_pvar_null, REQUEST_ROUTE},
    {"load_gws_from_grp", (cmd_function)load_gws_from_grp, 1,
     fixstringloadgws, fixup_free_pvar_null, REQUEST_ROUTE},
    {"next_gw", (cmd_function)next_gw, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"from_gw", (cmd_function)from_gw_0, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"from_gw", (cmd_function)from_gw_1, 1, fixup_pvar_null,
     fixup_free_pvar_null, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"from_gw_grp", (cmd_function)from_gw_grp, 1, fixup_uint_null, 0,
     REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
    {"to_gw", (cmd_function)to_gw, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"to_gw", (cmd_function)to_gw_grp, 1, fixup_uint_null, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"load_contacts", (cmd_function)load_contacts, 0, 0, 0,
     REQUEST_ROUTE},
    {"next_contacts", (cmd_function)next_contacts, 0, 0, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"db_url",                   STR_PARAM, &db_url.s       },
    {"gw_table",                 STR_PARAM, &gw_table.s     },
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
    {"fr_inv_timer_avp",         STR_PARAM, &fr_inv_timer_avp_param },
    {"gw_uri_avp",               STR_PARAM, &gw_uri_avp_param },
    {"ruri_user_avp",            STR_PARAM, &ruri_user_avp_param },
    {"contact_avp",              STR_PARAM, &contact_avp_param },
    {"rpid_avp",                 STR_PARAM, &rpid_avp_param },
    {"flags_avp",                STR_PARAM, &flags_avp_param },
    {"fr_inv_timer",             INT_PARAM, &fr_inv_timer },
    {"fr_inv_timer_next",        INT_PARAM, &fr_inv_timer_next },
    {"lcr_hash_size",            INT_PARAM, &lcr_hash_size_param },
    {"fetch_rows",               INT_PARAM, &fetch_rows_param },
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
	0          /* child initialization function */
};


static int lcr_db_init(const str* db_url)
{	
	if (lcr_dbf.init==0){
		LM_CRIT("null lcr_dbf\n");
		goto error;
	}
	db_handle=lcr_dbf.init(db_url);
	if (db_handle==0){
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
	if (db_handle && lcr_dbf.close){
		lcr_dbf.close(db_handle);
		db_handle=0;
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

    /* Update length of module variables */
    db_url.len = strlen(db_url.s);
    gw_table.len = strlen(gw_table.s);
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

    /* Process AVP params */
    if (fr_inv_timer_avp_param && *fr_inv_timer_avp_param) {
	s.s = fr_inv_timer_avp_param; s.len = strlen(s.s);
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n",
		   fr_inv_timer_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &fr_inv_timer_avp, &avp_flags)!=0) {
	    LM_ERR("invalid AVP definition <%s>\n", fr_inv_timer_avp_param);
	    return -1;
	}
	fr_inv_timer_avp_type = avp_flags;
    } else {
	LM_ERR("AVP fr_inv_timer_avp has not been defined\n");
	return -1;
    }

    if (gw_uri_avp_param && *gw_uri_avp_param) {
	s.s = gw_uri_avp_param; s.len = strlen(s.s);
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n", gw_uri_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &gw_uri_avp, &avp_flags)!=0) {
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
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &ruri_user_avp, &avp_flags)!=0) {
	    LM_ERR("invalid AVP definition <%s>\n", ruri_user_avp_param);
	    return -1;
	}
	ruri_user_avp_type = avp_flags;
    } else {
	LM_ERR("AVP ruri_user_avp has not been defined\n");
	return -1;
    }

    if (contact_avp_param && *contact_avp_param) {
	s.s = contact_avp_param; s.len = strlen(s.s);
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n",
		   contact_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &contact_avp, &avp_flags)!=0) {
	    LM_ERR("invalid AVP definition <%s>\n", contact_avp_param);
	    return -1;
	}
	contact_avp_type = avp_flags;
    } else {
	LM_ERR("AVP contact_avp has not been defined\n");
	return -1;
    }

    if (rpid_avp_param && *rpid_avp_param) {
	s.s = rpid_avp_param; s.len = strlen(s.s);
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n", rpid_avp_param);
 	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &rpid_avp, &avp_flags)!=0) {
	    LM_ERR("invalid AVP definition <%s>\n", rpid_avp_param);
	    return -1;
	}
	rpid_avp_type = avp_flags;
    } else {
	LM_ERR("AVP rpid_avp has not been defined\n");
	return -1;
    }

    if (flags_avp_param && *flags_avp_param) {
	s.s = flags_avp_param; s.len = strlen(s.s);
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("malformed or non AVP definition <%s>\n", flags_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &flags_avp, &avp_flags)!=0) {
	    LM_ERR("invalid AVP definition <%s>\n", flags_avp_param);
	    return -1;
	}
	flags_avp_type = avp_flags;
    } else {
	LM_ERR("AVP flags_avp has not been defined\n");
	return -1;
    }

    if (fetch_rows_param < 1) {
	LM_ERR("invalid fetch_rows module parameter value <%d>\n",
	       fetch_rows_param);
	return -1;
    }

    /* Check table version */
    db_con_t* dbh;
    if (lcr_dbf.init==0){
	LM_CRIT("unbound database\n");
	return -1;
    }
    dbh=lcr_dbf.init(&db_url);
    if (dbh==0){
	LM_ERR("unable to open database connection\n");
	return -1;
    }
    if((db_check_table_version(&lcr_dbf, dbh, &gw_table, GW_TABLE_VERSION)
	< 0) ||
       (db_check_table_version(&lcr_dbf, dbh, &lcr_table, LCR_TABLE_VERSION)
	< 0)) { 
	LM_ERR("error during table version check\n");
	lcr_dbf.close(dbh);
	goto err;
    }
    lcr_dbf.close(dbh);

    /* Reset all shm pointers */
    gws_1 = gws_2 = (struct gw_info *)NULL;
    gws = (struct gw_info **)NULL;
    lcrs_1 = lcrs_2 = (struct lcr_info **)NULL;
    lcrs = (struct lcr_info ***)NULL;
    reload_lock = (gen_lock_t *)NULL;

    /* Initializing gw tables and gw table pointer variable */
    /* ip_addr of first entry contains the number of gws in table */
    gws_1 = (struct gw_info *)shm_malloc(sizeof(struct gw_info) *
					 (MAX_NO_OF_GWS + 1));
    if (gws_1 == 0) {
	LM_ERR("no memory for gw table\n");
	goto err;
    }
    gws_2 = (struct gw_info *)shm_malloc(sizeof(struct gw_info) *
					 (MAX_NO_OF_GWS + 1));
    if (gws_2 == 0) {
	LM_ERR("no memory for gw table\n");
	goto err;
    }
    gws = (struct gw_info **)shm_malloc(sizeof(struct gw_info *));
    if (gws == 0) {
	LM_ERR("no memory for gw table pointer\n");
    }
    gws_1[0].ip_addr = 0;    /* Number of gateways in table */
    *gws = gws_1;

    /* Initializing lcr hash tables and hash table pointer variable */
    /* Last entry in hash table contains list of different prefix lengths */
    lcrs_1 = (struct lcr_info **)
	shm_malloc(sizeof(struct lcr_info *) * (lcr_hash_size_param + 1));
    if (lcrs_1 == 0) {
	LM_ERR("no memory for lcr hash table\n");
	goto err;
    }
    memset(lcrs_1, 0, sizeof(struct lcr_info *) * (lcr_hash_size_param + 1));
    lcrs_2 = (struct lcr_info **)
	shm_malloc(sizeof(struct lcr_info *) * (lcr_hash_size_param + 1));
    if (lcrs_1 == 0) {
	LM_ERR("no memory for lcr hash table\n");
	goto err;
    }
    memset(lcrs_2, 0, sizeof(struct lcr_info *) * (lcr_hash_size_param + 1));
    lcrs = (struct lcr_info ***)shm_malloc(sizeof(struct lcr_info *));
    if (lcrs == 0) {
	LM_ERR("no memory for lcr hash table pointer\n");
	goto err;
    }
    *lcrs = lcrs_1;

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
    if (reload_gws_and_lcrs() == -1) {
	lock_release(reload_lock);
	LM_CRIT("failed to reload gateways and routes\n");
	goto err;
    }
    lock_release(reload_lock);

    return 0;

err:
    free_shared_memory();
    return -1;
}


static void destroy(void)
{
    lcr_db_close();

    free_shared_memory();
}

/* Free shared memory */
static void free_shared_memory(void)
{
    if (gws_1) {
        shm_free(gws_1);
    }
    if (gws_2) {
        shm_free(gws_2);
    }
    if (gws) {
        shm_free(gws);
    }
    if (lcrs_1) {
        lcr_hash_table_contents_free(lcrs_1);
        shm_free(lcrs_1);
    }
    if (lcrs_2) {
        lcr_hash_table_contents_free(lcrs_2);
        shm_free(lcrs_2);
    }
    if (lcrs) {
        shm_free(lcrs);
    }
    if (reload_lock) {
	lock_destroy(reload_lock);
	lock_dealloc(reload_lock);
    }
}
   

/* 
 * Convert string parameter to integer for functions that expect an integer.
 * Taken from sl module.
 */
static int fixstringloadgws(void **param, int param_count)
{
    pv_elem_t *model=NULL;
    str s;

    /* convert to str */
    s.s = (char*)*param;
    s.len = strlen(s.s);

    model=NULL;
    if (param_count==1) {
	if(s.len==0) {
	    LM_ERR("no param <%d>!\n", param_count);
	    return -1;
	}
	
	if(pv_parse_format(&s,&model)<0 || model==NULL) {
	    LM_ERR("wrong format <%s> for param <%d>!\n", s.s, param_count);
	    return -1;
	}
	if(model->spec.getf==NULL) {
	    if(param_count==1) {
		if(str2int(&s, (unsigned int*)&model->spec.pvp.pvn.u.isname.name.n)!=0) {
		    LM_ERR("wrong value <%s> for param <%d>!\n",
			   s.s, param_count);
		    return -1;
		}
	    }
	}
	*param = (void*)model;
    }

    return 0;
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
		     unsigned int tag_len, unsigned short weight)
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
int reload_gws_and_lcrs(void)
{
    unsigned int i, n, port, strip, tag_len, prefix_len, from_uri_len,
	grp_id,	grp_cnt, priority, flags, first_gw, weight, gw_cnt,
	hostname_len;
    struct in_addr ip_addr;
    uri_type scheme;
    uri_transport transport;
    db_con_t* dbh;
    char *ip_string, *hostname, *tag, *prefix, *from_uri;
    db_res_t* res = NULL;
    db_row_t* row;
    db_key_t gw_cols[10];
    db_key_t lcr_cols[4];
    pcre *from_uri_re;
    struct gw_grp gw_grps[MAX_NO_OF_GWS];

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

    lcr_cols[0] = &prefix_col;
    lcr_cols[1] = &from_uri_col;
    lcr_cols[2] = &grp_id_col;
    lcr_cols[3] = &priority_col;

    /* Reload gws */

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

    if (lcr_dbf.query(dbh, NULL, 0, NULL, gw_cols, 0, 10, 0, &res) < 0) {
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
	    (VAL_TYPE(ROW_VALUES(row)) != DB_STRING)) {
	    LM_ERR("gw ip address at row <%u> is null or not string\n", i);
	    goto gw_err;
	}
	ip_string = (char *)VAL_STRING(ROW_VALUES(row));
	if (inet_aton(ip_string, &ip_addr) == 0) {
	    LM_ERR("gateway ip address <%s> at row <%u> is invalid\n",
		   ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
	    port = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 1) != DB_INT) {
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
	if (VAL_NULL(ROW_VALUES(row) + 2) == 1) {
	    scheme = SIP_URI_T;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 2) != DB_INT) {
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
	if (VAL_NULL(ROW_VALUES(row) + 3) == 1) {
	    transport = PROTO_NONE;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 3) != DB_INT) {
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
	if (VAL_NULL(ROW_VALUES(row) + 4) == 1) {
	    strip = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 4) != DB_INT) {
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
	if (VAL_NULL(ROW_VALUES(row) + 5) == 1) {
	    tag_len = 0;
	    tag = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 5) != DB_STRING) {
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
	if (VAL_NULL(ROW_VALUES(row) + 6) == 1) {
	    grp_id = 0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 6) != DB_INT) {
		LM_ERR("grp_id of gw <%s> at row <%u> is not int\n",
		       ip_string, i);
		goto gw_err;
	    }
	    grp_id = VAL_INT(ROW_VALUES(row) + 6);
	}
	if (!VAL_NULL(ROW_VALUES(row) + 7) &&
	    (VAL_TYPE(ROW_VALUES(row) + 7) == DB_INT)) {
	    flags = (unsigned int)VAL_INT(ROW_VALUES(row) + 7);
	} else {
	    LM_ERR("flags of gw <%s> at row <%u> is NULL or not int\n",
		   ip_string, i);
	    goto gw_err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 8) == 1) {
	    weight = 1;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 8) != DB_INT) {
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
	if (VAL_NULL(ROW_VALUES(row) + 9) == 1) {
	    hostname_len = 0;
	    hostname = (char *)0;
	} else {
	    if (VAL_TYPE(ROW_VALUES(row) + 9) != DB_STRING) {
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
	
	if (*gws == gws_1) {
	    if (!insert_gw(gws_2, i + 1, (unsigned int)ip_addr.s_addr, 
			   hostname, hostname_len, grp_id,
			   ip_string, port, scheme, transport, flags, strip,
			   tag, tag_len, weight)) {
		goto gw_err;
	    }
	} else {
	    if (!insert_gw(gws_1, i + 1, (unsigned int)ip_addr.s_addr,
			   hostname, hostname_len, grp_id,
			   ip_string, port, scheme, transport, flags, strip,
			   tag, tag_len, weight)) {
		goto gw_err;
	    }
	}
    }

    lcr_dbf.free_result(dbh, res);
    res = NULL;
    
    gw_cnt = i;

    if (*gws == gws_1) {
	qsort(&(gws_2[1]), gw_cnt, sizeof(struct gw_info), comp_gw_grps);
	gws_2[0].ip_addr = gw_cnt;
	link_gw_grps(gws_2, gw_grps, &grp_cnt);
    } else {
	qsort(&(gws_1[1]), gw_cnt, sizeof(struct gw_info), comp_gw_grps);
	gws_1[0].ip_addr = gw_cnt;
	link_gw_grps(gws_1, gw_grps, &grp_cnt);
    }

    for (i = 0; i < grp_cnt; i++) {
	LM_DBG("gw_grps[%d].grp_id <%d>, gw_grps[%d].first <%d>\n",
		i, gw_grps[i].grp_id, i, gw_grps[i].first);
    }

    /* Reload lcrs */

    if (*lcrs == lcrs_1) {
	lcr_hash_table_contents_free(lcrs_2);
    } else {
	lcr_hash_table_contents_free(lcrs_1);
    }

    if (lcr_dbf.use_table(dbh, &lcr_table) < 0) {
	LM_ERR("error while trying to use lcr table\n");
	return -1;
    }

    if (DB_CAPABILITY(lcr_dbf, DB_CAP_FETCH)) {
	if (lcr_dbf.query(dbh, 0, 0, 0, lcr_cols, 0, 4, 0, 0) < 0) {
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
	if (lcr_dbf.query(dbh, 0, 0, 0, lcr_cols, 0, 4, 0, &res) < 0) {
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
		if (VAL_TYPE(ROW_VALUES(row)) != DB_STRING) {
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
		if (VAL_TYPE(ROW_VALUES(row) + 1) != DB_STRING) {
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
		(VAL_TYPE(ROW_VALUES(row) + 2) != DB_INT)) {
		LM_ERR("lcr grp_id at row <%u> is null or not int\n", i);
		goto lcr_err;
	    }
	    grp_id = (unsigned int)VAL_INT(ROW_VALUES(row) + 2);
	    first_gw = find_first_gw(gw_grps, grp_cnt, grp_id);
	    if (first_gw == 0) {
		LM_ERR("gw grp_id <%u> of prefix <%.*s> has no gateways\n",
		       grp_id, prefix_len, prefix);
		goto lcr_err;
	    }
	    if ((VAL_NULL(ROW_VALUES(row) + 3) == 1) ||
		(VAL_TYPE(ROW_VALUES(row) + 3) != DB_INT)) {
		LM_ERR("lcr priority at row <%u> is null or not int\n", i);
		goto lcr_err;
	    }
	    priority = (unsigned int)VAL_INT(ROW_VALUES(row) + 3);

	    if (*lcrs == lcrs_1) {
		if (!lcr_hash_table_insert(lcrs_2, prefix_len, prefix,
					   from_uri_len, from_uri, from_uri_re,
					   grp_id, first_gw, priority) ||
		    !prefix_len_insert(lcrs_2, prefix_len)) {
		    lcr_hash_table_contents_free(lcrs_2);
		    goto lcr_err;
		}
	    } else {
		if (!lcr_hash_table_insert(lcrs_1, prefix_len, prefix,
					   from_uri_len, from_uri, from_uri_re,
					   grp_id, first_gw, priority) ||
		    !prefix_len_insert(lcrs_1, prefix_len)) {
		    lcr_hash_table_contents_free(lcrs_1);
		    goto lcr_err;
		}
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

    /* Switch current gw and lcr tables */
    if (*gws == gws_1) {
	*gws = gws_2;
    } else {
	*gws = gws_1;
    }
    if (*lcrs == lcrs_1) {
	*lcrs = lcrs_2;
    } else {
	*lcrs = lcrs_1;
    }

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
    unsigned int i;
    struct mi_attr* attr;
    uri_transport transport;
    char *transp;
    struct mi_node* node;
    struct ip_addr address;
    char* p;
    int len;

    for (i = 1; i <= (*gws)[0].ip_addr; i++) {

	node = add_mi_node_child(rpl,0 ,"GW", 2, 0, 0);
	if (node == NULL) goto err;

	p = int2str((unsigned long)(*gws)[i].grp_id, &len );
	attr = add_mi_attr(node, MI_DUP_VALUE, "GRP_ID", 6, p, len );
	if (attr == NULL) goto err;

	address.af = AF_INET;
	address.len = 4;
	address.u.addr32[0] = (*gws)[i].ip_addr;
	attr = addf_mi_attr(node, 0, "IP_ADDR", 6, "%s", ip_addr2a(&address));
	if (attr == NULL) goto err;

	attr = add_mi_attr(node, MI_DUP_VALUE, "HOSTNAME", 8,
			   (*gws)[i].hostname, (*gws)[i].hostname_len );
	if (attr == NULL) goto err;

	if ((*gws)[i].port > 0) {
	    p = int2str((unsigned long)(*gws)[i].port, &len );
	    attr = add_mi_attr(node, MI_DUP_VALUE, "PORT", 4, p, len);
	} else {
	    attr = add_mi_attr(node, MI_DUP_VALUE, "PORT", 4, (char *)0, 0);
	}	    
	if (attr == NULL) goto err;

	if ((*gws)[i].scheme == SIP_URI_T) {
	    attr = add_mi_attr(node, MI_DUP_VALUE, "SCHEME", 6, "sip", 3);
	} else {
	    attr = add_mi_attr(node, MI_DUP_VALUE, "SCHEME", 6, "sips", 4);
	}
	if (attr == NULL) goto err;

	transport = (*gws)[i].transport;
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

	p = int2str((unsigned long)(*gws)[i].strip, &len );
	attr = add_mi_attr(node, MI_DUP_VALUE, "STRIP", 5, p, len);
	if (attr == NULL) goto err;

	attr = add_mi_attr(node, MI_DUP_VALUE, "TAG", 3,
			   (*gws)[i].tag, (*gws)[i].tag_len );
	if (attr == NULL) goto err;

	p = int2str((unsigned long)(*gws)[i].weight, &len);
	attr = add_mi_attr(node, MI_DUP_VALUE, "WEIGHT", 6, p, len);
	if (attr == NULL) goto err;

	p = int2str((unsigned long)(*gws)[i].flags, &len);
	attr = add_mi_attr(node, MI_DUP_VALUE, "FLAGS", 5, p, len);
	if (attr == NULL) goto err;
    }

    return 0;

 err:
    return -1;
}

/* Print lcrs from lcrs table */
int mi_print_lcrs(struct mi_node* rpl)
{
    unsigned int i;
    struct mi_attr* attr;
    struct mi_node* node;
    char* p;
    int len;
    struct lcr_info *lcr_rec;

    for (i = 0; i < lcr_hash_size_param; i++) {

	lcr_rec = (*lcrs)[i];

	while (lcr_rec) {

	    node = add_mi_node_child(rpl, 0, "RULE", 4, 0, 0);
	    if (node == NULL) goto err;

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

    lcr_rec = (*lcrs)[lcr_hash_size_param];

    while (lcr_rec) {

	node = add_mi_node_child(rpl, 0, "PREFIX_LENS", 11, 0, 0);
	if (node == NULL) goto err;

	p = int2str((unsigned long)lcr_rec->prefix_len, &len );
	attr = add_mi_attr(node, MI_DUP_VALUE, "PREFIX_LEN", 10, p, len);
	if (attr == NULL) goto err;

	lcr_rec = lcr_rec->next;
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
    memcpy(at, string, len);
    at = at + len;
    *at = '|'; at = at + 1;
    /* strip */
    string = int2str(strip, &len);
    memcpy(at, string, len);
    at = at + len;
    *at = '|'; at = at + 1;
    /* tag */
    memcpy(at, tag, tag_len);
    at = at + tag_len;
    *at = '|'; at = at + 1;
    /* ip_addr */
    string = int2str(ip_addr, &len);
    memcpy(at, string, len);
    at = at + len;
    *at = '|'; at = at + 1;
    /* hostname */
    memcpy(at, hostname, hostname_len);
    at = at + hostname_len;
    *at = '|'; at = at + 1;
    /* port */
    string = int2str(port, &len);
    memcpy(at, string, len);
    at = at + len;
    *at = '|'; at = at + 1;
    /* transport */
    string = int2str(transport, &len);
    memcpy(at, string, len);
    at = at + len;
    *at = '|'; at = at + 1;
    /* flags */
    string = int2str(flags, &len);
    memcpy(at, string, len);
    at = at + len;
    return at - value;
}

inline int decode_avp_value(char *value, str *scheme, unsigned int *strip,
			    str *tag, str *addr, str *hostname,
			    str *port, str *transport, unsigned int *flags)
{
    str s;
    unsigned int u;
    char *sep;
    struct ip_addr a;

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
    str2int(&s, &u);
    a.af = AF_INET;
    a.len = 4;
    a.u.addr32[0] = u;
    addr->s = ip_addr2a(&a);
    addr->len = strlen(addr->s);
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
    

/* Add gateways in matched_gws array into AVPs */
void add_gws_into_avps(struct matched_gw_info *matched_gws,
		       unsigned int gw_cnt, str *ruri_user)
{
    unsigned int i, index, strip, hostname_len;
    int tag_len;
    str value;
    char encoded_value[MAX_URI_LEN];
    int_str val;

    for (i = 0; i < gw_cnt; i++) {
	index = matched_gws[i].gw_index;
      	hostname_len = (*gws)[index].hostname_len;
	strip = (*gws)[index].strip;
	if (strip > ruri_user->len) {
	    LM_ERR("strip count of gw is too large <%u>\n", strip);
	    goto skip;
	}
	tag_len = (*gws)[index].tag_len;
	if (5 /* scheme */ + 4 /* strip */ + tag_len + 1 /* @ */ +
	    ((hostname_len > 15)?hostname_len:15) + 6 /* port */ +
	    15 /* transport */ + 10 /* flags */ + 7 /* separators */
	    > MAX_URI_LEN) {
	    LM_ERR("too long AVP value\n");
	    goto skip;
	}
	value.len = 
	    encode_avp_value(encoded_value, (*gws)[index].scheme, strip,
			     (*gws)[index].tag, tag_len, (*gws)[index].ip_addr,
			     (*gws)[index].hostname, hostname_len,
			     (*gws)[index].port, (*gws)[index].transport,
			     (*gws)[index].flags);
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
 * Load info of matching GWs to gw_uri AVPs
 */
static int do_load_gws(struct sip_msg* _m, str *_from_uri)
{
    str ruri_user, from_uri;
    unsigned int j, k, gw_index, have_rpid_avp, gw_count;
    struct usr_avp *avp;
    int_str val;
    struct matched_gw_info matched_gws[MAX_NO_OF_GWS + 1];
    struct lcr_info *lcr_rec, *pl;

    /* Find Request-URI user */
    if (parse_sip_msg_uri(_m) < 0) {
	    LM_ERR("error while parsing R-URI\n");
	    return -1;
    }
    ruri_user = _m->parsed_uri.user;

    if (_from_uri) {
	/* take caller uri from _from_uri argument */
	from_uri = *_from_uri;
    } else {
	/* take caller uri from RPID or From URI */
	have_rpid_avp = 0;
	avp = search_first_avp(rpid_avp_type, rpid_avp, &val, 0);
	if (avp != NULL) {
	    /* Get URI user from RPID if not empty */
	    if (avp->flags & AVP_VAL_STR) {
		if (val.s.s && val.s.len) {
		    from_uri = val.s;
		    have_rpid_avp = 1;
		}
	    } else {
		from_uri.s = int2str(val.n, &from_uri.len);
		have_rpid_avp = 1;
	    }
	}
	if (!have_rpid_avp) {
	    /* Get URI from From URI */
	    if ((!_m->from) && (parse_headers(_m, HDR_FROM_F, 0) == -1)) {
		LM_ERR("error while parsing headers\n");
		return -1;
	    }
	    if (!_m->from) {
		LM_ERR("from header field not found\n");
		return -1;
	    }
	    if ((!(_m->from)->parsed) && (parse_from_header(_m) < 0)) {
		LM_ERR("error while parsing From header\n");
		return -1;
	    }
	    from_uri = get_from(_m)->uri;
	}
    }

    /*
     * Find lcr entries that match based on prefix and from_uri and collect
     * gateways of matching entries into matched_gws array so that each
     * gateway appears in the array only once.
     */

    pl = (*lcrs)[lcr_hash_size_param];
    gw_index = 0;
    gw_count = (*gws)[0].ip_addr;

    while (pl) {
	if (ruri_user.len < pl->prefix_len) {
	    pl = pl->next;
	    continue;
	}
	lcr_rec = lcr_hash_table_lookup(*lcrs, pl->prefix_len, ruri_user.s);
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
			for (k = 0; k < gw_index; k++) {
			    if ((*gws)[j].ip_addr ==
				(*gws)[matched_gws[k].gw_index].ip_addr)
				/* Skip already existing gw */
				goto gw_found;
			}
			/* This is a new gw */
			matched_gws[gw_index].gw_index = j;
			matched_gws[gw_index].prefix_len = pl->prefix_len;
			matched_gws[gw_index].priority = lcr_rec->priority;
			matched_gws[gw_index].weight = (*gws)[j].weight *
			    (rand() >> 8);
			LM_DBG("added matched_gws[%d]=[%u, %u, %u, %u]\n",
			       gw_index, j, pl->prefix_len, lcr_rec->priority,
			       matched_gws[gw_index].weight);
			gw_index++;
		    gw_found:
			j = (*gws)[j].next;
		    }
		}
	    }
	    lcr_rec = lcr_rec->next;
	}
	pl = pl->next;
    }

    /* Sort gateways based on prefix_len, priority, and randomized weight */
    qsort(matched_gws, gw_index, sizeof(struct matched_gw_info), comp_matched);

    /* Add gateways into AVPs */
    add_gws_into_avps(matched_gws, gw_index, &ruri_user);

    return 1;
}


/*
 * Load info of matching GWs from database to gw_uri AVPs.
 * Caller URI is taken from request.
 */
static int load_gws_0(struct sip_msg* _m, char* _s1, char* _s2)
{
    return do_load_gws(_m, (str *)0);
}


/*
 * Load info of matching GWs from database to gw_uri AVPs.
 * Caller URI is taken from pseudo variable argument.
 */
static int load_gws_1(struct sip_msg* _m, char* _sp, char* _s2)
{
    pv_spec_t *sp;
    pv_value_t pv_val;
    sp = (pv_spec_t *)_sp;

    if (sp && (pv_get_spec_value(_m, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
		LM_DBG("missing from uri\n");
		return -1;
	    }
 	    return do_load_gws(_m, &(pv_val.rs));
	} else {
	   LM_DBG("pseudo variable value is not string\n");
	   return -1;
	}
    } else {
	LM_DBG("cannot get pseudo variable value\n");
	return -1;
    }
}


/*
 * Load info of matching GWs from database to gw_uri AVPs taking into
 * account the given group id.
 */
static int load_gws_from_grp(struct sip_msg* _m, char* _s1, char* _s2)
{
    str grp_s, ruri_user;
    unsigned int i, grp_id, gw_index, gw_cnt, next_index;
    struct matched_gw_info matched_gws[MAX_NO_OF_GWS + 1];

    /* Get grp_id from parameter */
    if(((pv_elem_p)_s1)->spec.getf != NULL) {
	if (pv_printf_s(_m, (pv_elem_p)_s1, &grp_s) != 0) {
	    LM_ERR("cannot print grp_id parameter value as string\n");
	    return -1;
	}
	if (str2int(&grp_s, &grp_id) != 0) {
	    LM_ERR("cannot convert grp_id string <%.*s> to int\n",
		   grp_s.len, grp_s.s);
	    return -1;
	}
    } else {
	grp_id = ((pv_elem_p)_s1)->spec.pvp.pvn.u.isname.name.n;
    }

    /* Find Request-URI user */
    if (parse_sip_msg_uri(_m) < 0) {
	LM_ERR("error while parsing R-URI\n");
	return -1;
    }

    ruri_user = _m->parsed_uri.user;

    /* Find gws of the given group */
    LM_DBG("finding gateways of grp_id <%d>\n", grp_id);
    gw_cnt = (*gws)[0].ip_addr;
    gw_index = 0;
    for (i = 1; i <= gw_cnt; i++) {
	if ((*gws)[i].grp_id == grp_id) {
	    next_index = i;
	    while (next_index) {
		matched_gws[gw_index].gw_index = next_index;
		matched_gws[gw_index].prefix_len = 0;
		matched_gws[gw_index].priority = 1;
		matched_gws[gw_index].weight = rand();
		LM_DBG("added matched_gws[%d]=[%u, %u, %u, %u]\n",
			gw_index, next_index, 0, 1, 
			matched_gws[gw_index].weight);
		gw_index++;
		next_index = (*gws)[next_index].next;
	    }
	    break;
	}
    }

    /* Sort gateways based on random number stored in weight field */
    qsort(matched_gws, gw_index, sizeof(struct matched_gw_info), comp_matched);

    /* Add gateways into AVPs */
    add_gws_into_avps(matched_gws, gw_index, &ruri_user);

    return 1;
}


static int generate_uris(char *r_uri, str *r_uri_user, unsigned int *r_uri_len,
			 char *dst_uri, unsigned int *dst_uri_len,
			 unsigned int *flags)
{
    int_str gw_uri_val;
    struct usr_avp *gu_avp;
    str scheme, tag, addr, hostname, port, transport;
    char *at;
    unsigned int strip;
    
    gu_avp = search_first_avp(gw_uri_avp_type, gw_uri_avp, &gw_uri_val, 0);

    if (!gu_avp) return 0; /* No more gateways left */

    decode_avp_value(gw_uri_val.s.s, &scheme, &strip, &tag, &addr,
		     &hostname, &port, &transport, flags);

    if (scheme.len + r_uri_user->len - strip + tag.len + addr.len +
	1 /* @ */ + ((hostname.len > 15)?hostname.len:15) + 1 /* : */ +
	port.len + transport.len + 1 /* null */ > MAX_URI_LEN) {
	LM_ERR("too long Request URI or DST URI\n");
	return 0;
    }

    at = r_uri;
    
    memcpy(at, scheme.s, scheme.len);
    at = at + scheme.len;
	
    memcpy(at, tag.s, tag.len);
    at = at + tag.len;
	
    if (strip > r_uri_user->len) {
	LM_ERR("strip count <%u> is largen that R-URI user <%.*s>\n",
	       strip, r_uri_user->len, r_uri_user->s);
	return 0;
    }
    memcpy(at, r_uri_user->s + strip, r_uri_user->len - strip);
    at = at + r_uri_user->len - strip;

    *at = '@';
    at = at + 1;
	
    if (hostname.len == 0) {
	memcpy(at, addr.s, addr.len);
	at = at + addr.len;
	if (port.len > 0) {
	    *at = ':';
	    at = at + 1;
	    memcpy(at, port.s, port.len);
	    at = at + port.len;
	}
	if (transport.len > 0) {
	    memcpy(at, transport.s, transport.len);
	    at = at + transport.len;
	}
	*at = '\0';
	*r_uri_len = at - r_uri;
	*dst_uri_len = 0;
    } else {
	memcpy(at, hostname.s, hostname.len);
	at = at + hostname.len;
	*at = '\0';
	*r_uri_len = at - r_uri;
	at = dst_uri;
	memcpy(at, scheme.s, scheme.len);
	at = at + scheme.len;
	memcpy(at, addr.s, addr.len);
	at = at + addr.len;
	if (port.len > 0) {
	    *at = ':';
	    at = at + 1;
	    memcpy(at, port.s, port.len);
	    at = at + port.len;
	}
	if (transport.len > 0) {
	    memcpy(at, transport.s, transport.len);
	    at = at + transport.len;
	}
	*at = '\0';
	*dst_uri_len = at - dst_uri;
    }

    destroy_avp(gu_avp);
	
    LM_DBG("r_uri <%.*s>, dst_uri <%.*s>\n",
	   *r_uri_len, r_uri, *dst_uri_len, dst_uri);

    return 1;
}


/*
 * When called first time, rewrites scheme, host, port, and
 * transport parts of R-URI based on first gw_uri_avp value, which is then
 * destroyed.  Saves R-URI user to ruri_user_avp for later use.
 *
 * On subsequence calls (determined by existence of ruri_user_avp value),
 * appends a new branch to request, where scheme, host, port, and transport
 * of URI are taken from the first gw_uri_avp value, which is then destroyed.
 * URI user is taken from ruri_user_avp value saved earlier.
 *
 * Returns 1 upon success and -1 upon failure.
 */
static int next_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
    int_str ruri_user_val, val;
    struct action act;
    struct usr_avp *ru_avp;
    int rval;
    str uri_str;
    unsigned int flags, r_uri_len, dst_uri_len;
    char r_uri[MAX_URI_LEN], dst_uri[MAX_URI_LEN];

    ru_avp = search_first_avp(ruri_user_avp_type, ruri_user_avp,
			      &ruri_user_val, 0);

    if (ru_avp == NULL) { /* First invocation */

	/* Re-write Request-URI by taking URI user from current Request-URI
	   and other parts of from gw_uri_avp. */

	if (parse_sip_msg_uri(_m) < 0) {
	    LM_ERR("parsing of R-URI failed\n");
	    return -1;
	}

	if (generate_uris(r_uri, &(_m->parsed_uri.user), &r_uri_len, dst_uri,
			  &dst_uri_len, &flags) == 0) {
	    return -1;
	}

	/* Save Request-URI user for use in subsequent invocations */
	val.s = _m->parsed_uri.user;
	add_avp(ruri_user_avp_type|AVP_VAL_STR, ruri_user_avp, val);
	LM_DBG("added ruri_user_avp <%.*s>\n", val.s.len, val.s.s);

	/* Rewrite Request URI */
	act.type = SET_URI_T;
	act.elem[0].type = STRING_ST;
	act.elem[0].u.string = r_uri;
	rval = do_action(&act, _m);
	if (rval != 1) {
	    LM_ERR("calling do_action failed with return value <%d>\n", rval);
	    return -1;
	}

    } else {  /* Subsequent invocation */

	/* Append a new branch to the transaction by taking URI user from
	   ruri_user_avp and other parts of from gw_uri_avp. */

	if (generate_uris(r_uri, &(ruri_user_val.s), &r_uri_len, dst_uri,
			  &dst_uri_len, &flags) == 0) {
	    return -1;
	}

	/* Append new branch */
	uri_str.s = r_uri;
	uri_str.len = r_uri_len;
	act.type = APPEND_BRANCH_T;
	act.elem[0].type = STRING_ST;
	act.elem[0].u.s = uri_str;
	act.elem[1].type = NUMBER_ST;
	act.elem[1].u.number = 0;
	rval = do_action(&act, _m);
	if (rval != 1) {
	    LM_ERR("calling do_action failed with return value <%d>\n", rval);
	    return -1;
	}
    }
    
    /* Set DST URI */
    if (dst_uri_len > 0) {
	uri_str.s = dst_uri;
	uri_str.len = dst_uri_len;
	act.type = SET_DSTURI_T;
	act.elem[0].type = STRING_ST;
	act.elem[0].u.s = uri_str;
	act.next = 0;
	rval = do_action(&act, _m);
	if (rval != 1) {
	    LM_ERR("calling do_action failed with return value <%d>\n", rval);
	    return -1;
	}
    }

    /* Set flags_avp */
    val.n = flags;
    add_avp(flags_avp_type, flags_avp, val);
    LM_DBG("added flags_avp <%u>\n", (unsigned int)val.n);

    return 1;
}


/*
 * Checks if request comes from a gateway
 */
static int do_from_gw(struct sip_msg* _m, pv_spec_t *addr_sp, int grp_id)
{
    unsigned int src_addr;
    pv_value_t pv_val;
    struct ip_addr *ip;
    int_str val;
    struct gw_info gw, *res;

    if (addr_sp && (pv_get_spec_value(_m, addr_sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_INT) {
	    src_addr = pv_val.ri;
	} else if (pv_val.flags & PV_VAL_STR) {
	    if ( (ip=str2ip( &pv_val.rs)) == NULL) {
		LM_ERR("failed to convert IP address string to in_addr\n");
		return -1;
	    } else {
		src_addr = ip->u.addr32[0];
	    }
	} else {
	    LM_ERR("IP address pvar has empty value\n");
	    return -1;
	}
    } else {
	src_addr = _m->rcv.src_ip.u.addr32[0];
    }

    if (grp_id < 0) {
	res = (struct gw_info *)bsearch(&src_addr, &((*gws)[1]),
					(*gws)[0].ip_addr,
					sizeof(struct gw_info), comp_gws);
    } else {
	gw.ip_addr = src_addr;
	gw.grp_id = grp_id;
	res = (struct gw_info *)bsearch(&gw, &((*gws)[1]),
					(*gws)[0].ip_addr,
					sizeof(struct gw_info), comp_gw_grps);
    }

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
 * Checks if request comes from a gateway, taking source address from request
 * and taking into account the group id.
 */
static int from_gw_grp(struct sip_msg* _m, char* _grp_id, char* _s2)
{
    return do_from_gw(_m, (pv_spec_t *)0, (int)(long)_grp_id);
}


/*
 * Checks if request comes from a gateway, taking src_address from request
 * and ignoring group id.
 */
static int from_gw_0(struct sip_msg* _m, char* _s1, char* _s2)
{
    return do_from_gw(_m, (pv_spec_t *)0, -1);
}


/*
 * Checks if request comes from a gateway, taking source address from pw
 * and ignoring group id.
 */
static int from_gw_1(struct sip_msg* _m, char* _addr_sp, char* _s2)
{
    return do_from_gw(_m, (pv_spec_t *)_addr_sp, -1);
}


/*
 * Checks if in-dialog request goes to gateway
 */
static int do_to_gw(struct sip_msg* _m, int grp_id)
{
    char host[16];
    struct in_addr addr;
    struct gw_info gw, *res;

    if((_m->parsed_uri_ok == 0) && (parse_sip_msg_uri(_m) < 0)) {
	LM_ERR("Error while parsing the R-URI\n");
	return -1;
    }

    if (_m->parsed_uri.host.len > 15) {
	return -1;
    }
    memcpy(host, _m->parsed_uri.host.s, _m->parsed_uri.host.len);
    host[_m->parsed_uri.host.len] = 0;

    if (!inet_aton(host, &addr)) {
	return -1;
    }

    if (grp_id < 0) {
	res = (struct gw_info *)bsearch(&(addr.s_addr), &((*gws)[1]),
					(*gws)[0].ip_addr,
					sizeof(struct gw_info), comp_gws);
    } else {
	gw.ip_addr = addr.s_addr;
	gw.grp_id = grp_id;
	res = (struct gw_info *)bsearch(&gw, &((*gws)[1]),
					(*gws)[0].ip_addr,
					sizeof(struct gw_info), comp_gw_grps);
    }

    if (res == NULL) {
	LM_DBG("request does not go to gw\n");
	return -1;
    } else {
	LM_DBG("request goes to gw\n");
	return 1;
    }
}


/*
 * Checks if in-dialog request goes to gateway, taking
 * into account the group id.
 */
static int to_gw_grp(struct sip_msg* _m, char* _s1, char* _s2)
{
    int grp_id;

    grp_id = (int)(long)_s1;
    return do_to_gw(_m, grp_id);
}


/*
 * Checks if in-dialog request goes to gateway, ignoring
 * the group id.
 */
static int to_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
    return do_to_gw(_m, -1);
}


/* 
 * Frees contact list used by load_contacts function
 */
static inline void free_contact_list(struct contact *curr) {
    struct contact *prev;
    while (curr) {
	prev = curr;
	curr = curr->next;
	pkg_free(prev);
    }
}

/* Encode branch info from contact struct to str */
static inline int encode_branch_info(str *info, struct contact *con)
{
    char *at, *s;
    int len;

    info->len = con->uri.len + con->dst_uri.len +
	con->path.len + MAX_SOCKET_STR + INT2STR_MAX_LEN + 5;
    info->s = pkg_malloc(info->len);
    if (!info->s) {
	LM_ERR("no memory left for branch info\n");
	return 0;
    }
    at = info->s;
    memcpy(at, con->uri.s, con->uri.len);
    at = at + con->uri.len;
    *at = '\n';
    at++;
    memcpy(at, con->dst_uri.s, con->dst_uri.len);
    at = at + con->dst_uri.len;
    *at = '\n';
    at++;
    memcpy(at, con->path.s, con->path.len);
    at = at + con->path.len;
    *at = '\n';
    at++;
    if (con->sock) {
	len = MAX_SOCKET_STR;
	if (!socket2str(con->sock, at, &len)) {
	    LM_ERR("failed to convert socket to str\n");
	    return 0;
	}
    } else {
	len = 0;
    }
    at = at + len;
    *at = '\n';
    at++;
    s = int2str(con->flags, &len);
    memcpy(at, s, len);
    at = at + len;
    *at = '\n';
    info->len = at - info->s + 1;

    return 1;
}


/* Encode branch info from str */
static inline int decode_branch_info(char *info, str *uri, str *dst, str *path,
				     struct socket_info **sock,
				     unsigned int *flags)
{
    str s, host;
    int port, proto;
    char *pos, *at;

    pos = strchr(info, '\n');
    uri->len = pos - info;
    if (uri->len) {
	uri->s = info;
    } else {
	uri->s = 0;
    }
    at = pos + 1;

    pos = strchr(at, '\n');
    dst->len = pos - at;
    if (dst->len) {
	dst->s = at;
    } else {
	dst->s = 0;
    }
    at = pos + 1;

    pos = strchr(at, '\n');
    path->len = pos - at;
    if (path->len) {
	path->s = at;
    } else {
	path->s = 0;
    }
    at = pos + 1;

    pos = strchr(at, '\n');
    s.len = pos - at;
    if (s.len) {
	s.s = at;
	if (parse_phostport(s.s, s.len, &host.s, &host.len,
			    &port, &proto) != 0) {
	    LM_ERR("parsing of socket info <%.*s> failed\n",  s.len, s.s);
	    return 0;
	}
	*sock = grep_sock_info(&host, (unsigned short)port,
			       (unsigned short)proto);
	if (*sock == 0) {
	    LM_ERR("invalid socket <%.*s>\n", s.len, s.s);
	    return 0;
	}
    } else {
	*sock = 0;
    }
    at = pos + 1;

    pos = strchr(at, '\n');
    s.len = pos - at;
    if (s.len) {
	s.s = at;
	if (str2int(&s, flags) != 0) {
	    LM_ERR("failed to decode flags <%.*s>\n", s.len, s.s);
	    return 0;
	}
    } else {
	*flags = 0;
    }

    return 1;
}


/* 
 * Loads contacts in destination set into "lcr_contact" AVP in reverse
 * priority order and associated each contact with Q_FLAG telling if
 * contact is the last one in its priority class.  Finally, removes
 * all branches from destination set.
 */
static int load_contacts(struct sip_msg* msg, char* key, char* value)
{
    str uri, dst_uri, path, branch_info, *ruri;
    qvalue_t q, ruri_q;
    struct contact *contacts, *next, *prev, *curr;
    int_str val;
    int idx;
    struct socket_info* sock;
    unsigned int flags;

    /* Check if anything needs to be done */
    if (nr_branches == 0) {
	LM_DBG("nothing to do - no branches!\n");
	return 1;
    }

    ruri = GET_RURI(msg);
    if (!ruri) {
	LM_ERR("no Request-URI found\n");
	return -1;
    }
    ruri_q = get_ruri_q();

    for(idx = 0; (uri.s = get_branch(idx, &uri.len, &q, 0, 0, 0, 0)) != 0;
	idx++) {
	if (q != ruri_q) {
	    goto rest;
	}
    }
    LM_DBG("nothing to do - all contacts have same q!\n");
    return 1;

rest:
    /* Insert Request-URI branch to contact list */
    contacts = (struct contact *)pkg_malloc(sizeof(struct contact));
    if (!contacts) {
	LM_ERR("no memory for contact info\n");
	return -1;
    }
    contacts->uri.s = ruri->s;
    contacts->uri.len = ruri->len;
    contacts->q = ruri_q;
    contacts->dst_uri = msg->dst_uri;
    contacts->sock = msg->force_send_socket;
    contacts->flags = getb0flags();
    contacts->path = msg->path_vec;
    contacts->next = (struct contact *)0;

    /* Insert branches to contact list in increasing q order */
    for(idx = 0;
	(uri.s = get_branch(idx,&uri.len,&q,&dst_uri,&path,&flags,&sock))
	    != 0;
	idx++ ) {
	next = (struct contact *)pkg_malloc(sizeof(struct contact));
	if (!next) {
	    LM_ERR("no memory for contact info\n");
	    free_contact_list(contacts);
	    return -1;
	}
	next->uri = uri;
	next->q = q;
	next->dst_uri = dst_uri;
	next->path = path;
	next->flags = flags;
	next->sock = sock;
	next->next = (struct contact *)0;
	prev = (struct contact *)0;
	curr = contacts;
	while (curr && (curr->q < q)) {
	    prev = curr;
	    curr = curr->next;
	}
	if (!curr) {
	    next->next = (struct contact *)0;
	    prev->next = next;
	} else {
	    next->next = curr;
	    if (prev) {
		prev->next = next;
	    } else {
		contacts = next;
	    }
	}		    
    }

    /* Assign values for q_flags */
    curr = contacts;
    curr->q_flag = 0;
    while (curr->next) {
	if (curr->q < curr->next->q) {
	    curr->next->q_flag = Q_FLAG;
	} else {
	    curr->next->q_flag = 0;
	}
	curr = curr->next;
    }

    /* Add contacts to "contacts" AVP */
    curr = contacts;
    while (curr) {
	if (encode_branch_info(&branch_info, curr) == 0) {
	    LM_ERR("encoding of branch info failed\n");
	    free_contact_list(contacts);
	    if (branch_info.s) pkg_free(branch_info.s);
	    return -1;
	}
	val.s = branch_info;
	add_avp(contact_avp_type|AVP_VAL_STR|(curr->q_flag),
		contact_avp, val);
	pkg_free(branch_info.s);
	LM_DBG("loaded contact <%.*s> with q_flag <%d>\n",
	       val.s.len, val.s.s, curr->q_flag);
	curr = curr->next;
    }

    /* Clear all branches */
    clear_branches();

    /* Free contact list */
    free_contact_list(contacts);

    return 1;
}


/*
 * Adds to request a destination set that includes all highest priority
 * class contacts in "lcr_contact" AVP.   If called from a route block,
 * rewrites the request uri with first contact and adds the remaining
 * contacts as branches.  If called from failure route block, adds all
 * contacts as branches.  Removes added contacts from "lcr_contact" AVP.
 */
static int next_contacts(struct sip_msg* msg, char* key, char* value)
{
    struct usr_avp *avp, *prev;
    int_str val;
    str uri, dst, path;
    struct socket_info *sock;
    unsigned int flags;

    if (route_type == REQUEST_ROUTE) {
	/* Find first lcr_contact_avp value */
	avp = search_first_avp(contact_avp_type, contact_avp, &val, 0);
	if (!avp) {
	    LM_DBG("no AVPs - we are done!\n");
	    return 1;
	}

	LM_DBG("next contact is <%s>\n", val.s.s);

	if (decode_branch_info(val.s.s, &uri, &dst, &path, &sock, &flags)
	    == 0) {
	    LM_ERR("decoding of branch info <%.*s> failed\n",
		   val.s.len, val.s.s);
	    destroy_avp(avp);
	    return -1;
	}

	rewrite_uri(msg, &uri);
	set_dst_uri(msg, &dst);
	set_path_vector(msg, &path);
	msg->force_send_socket = sock;
	setb0flags(flags);

	if (avp->flags & Q_FLAG) {
	    destroy_avp(avp);
	    /* Set fr_inv_timer */
	    val.n = fr_inv_timer_next;
	    if (add_avp(fr_inv_timer_avp_type, fr_inv_timer_avp, val) != 0) {
		LM_ERR("setting of fr_inv_timer_avp failed\n");
		return -1;
	    }
	    return 1;
	}

	/* Append branches until out of branches or Q_FLAG is set */
	prev = avp;
	while ((avp = search_next_avp(avp, &val))) {
	    destroy_avp(prev);

	    LM_DBG("next contact is <%s>\n", val.s.s);

	    if (decode_branch_info(val.s.s, &uri, &dst, &path, &sock, &flags)
		== 0) {
		LM_ERR("decoding of branch info <%.*s> failed\n",
		       val.s.len, val.s.s);
		destroy_avp(avp);
		return -1;
	    }

	    if (append_branch(msg, &uri, &dst, &path, 0, flags, sock) != 1) {
		LM_ERR("appending branch failed\n");
		destroy_avp(avp);
		return -1;
	    }

	    if (avp->flags & Q_FLAG) {
		destroy_avp(avp);
		val.n = fr_inv_timer_next;
		if (add_avp(fr_inv_timer_avp_type, fr_inv_timer_avp, val)
		    != 0) {
		    LM_ERR("setting of fr_inv_timer_avp failed\n");
		    return -1;
		}
		return 1;
	    }
	    prev = avp;
	}
	
    } else if ( route_type == FAILURE_ROUTE) {

	avp = search_first_avp(contact_avp_type, contact_avp, &val, 0);
	if (!avp) return -1;

	prev = avp;
	do {

	    LM_DBG("next contact is <%s>\n", val.s.s);

	    if (decode_branch_info(val.s.s, &uri, &dst, &path, &sock, &flags)
		== 0) {
		LM_ERR("decoding of branch info <%.*s> failed\n",
		       val.s.len, val.s.s);
		destroy_avp(avp);
		return -1;
	    }
	    
	    if (append_branch(msg, &uri, &dst, &path, 0, flags, sock) != 1) {
		LM_ERR("appending branch failed\n");
		destroy_avp(avp);
		return -1;
	    }

	    if (avp->flags & Q_FLAG) {
		destroy_avp(avp);
		return 1;
	    }

	    prev = avp;
	    avp = search_next_avp(avp, &val);
	    destroy_avp(prev);

	} while (avp);

	/* Restore fr_inv_timer */
	val.n = fr_inv_timer;
	if (add_avp(fr_inv_timer_avp_type, fr_inv_timer_avp, val) != 0) {
	    LM_ERR("setting of fr_inv_timer_avp failed\n");
	    return -1;
	}
	
    } else {
	/* Unsupported route type */
	LM_ERR("unsupported route type\n");
	return -1;
    }

    return 1;
}
