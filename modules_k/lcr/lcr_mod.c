/*
 * $Id$
 *
 * Least Cost Routing module (also implements sequential forking)
 *
 * Copyright (C) 2005 Juha Heinanen
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <regex.h>
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
#include "mi.h"

MODULE_VERSION

/*
 * Version of gw and lcr tables required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define GW_TABLE_VERSION 7
#define LCR_TABLE_VERSION 2

/* usr_avp flag for sequential forking */
#define Q_FLAG      (1<<2)

static void destroy(void);       /* Module destroy function */
static int mi_child_init(void);
static int mod_init(void);       /* Module initialization function */
static int fixstringloadgws(void **param, int param_count);

int reload_gws ( void );

#define GW_TABLE "gw"

#define GW_NAME_COL "gw_name"

#define GRP_ID_COL "grp_id"

#define IP_ADDR_COL "ip_addr"

#define PORT_COL "port"

#define URI_SCHEME_COL "uri_scheme"

#define TRANSPORT_COL "transport"

#define STRIP_COL "strip"

#define TAG_COL "tag"

#define FLAGS_COL "flags"

#define LCR_TABLE "lcr"

#define PREFIX_COL "prefix"

#define FROM_URI_COL "from_uri"

#define PRIORITY_COL "priority"

#define MAX_NO_OF_GWS 32
#define MAX_NO_OF_LCRS 256
#define MAX_PREFIX_LEN 256
#define MAX_TAG_LEN 16
#define MAX_FROM_URI_LEN 256

/* Default module parameter values */
#define DEF_FR_INV_TIMER 90
#define DEF_FR_INV_TIMER_NEXT 30
#define DEF_PREFIX_MODE 0

/*
 * Type definitions
 */

typedef enum sip_protos uri_transport;

struct gw_info {
    unsigned int ip_addr;
    unsigned int port;
    unsigned int grp_id;
    uri_type scheme;
    uri_transport transport;
    unsigned int strip;
    char tag[MAX_TAG_LEN + 1];
    unsigned short tag_len;
    unsigned int flags;
};

struct lcr_info {
    char prefix[MAX_PREFIX_LEN + 1];
    unsigned short prefix_len;
    char from_uri[MAX_FROM_URI_LEN + 1];
    unsigned short from_uri_len;
    unsigned int grp_id;
    unsigned short priority;
    unsigned short end_record;
};

struct prefix_regex {
	regex_t re;
	short int valid;
};

struct from_uri_regex {
    regex_t re;
    short int valid;
};

struct mi {
    int gw_index;
    int route_index;
    int randomizer;
};


/*
 * Database variables
 */
static db_con_t* db_handle = 0;   /* Database connection handle */
static db_func_t lcr_dbf;

/*
 * Module parameter variables
 */

/* database */
static str db_url           = str_init(DEFAULT_RODB_URL);
static str gw_table         = str_init(GW_TABLE);
static str gw_name_col      = str_init(GW_NAME_COL);
static str grp_id_col       = str_init(GRP_ID_COL);
static str ip_addr_col      = str_init(IP_ADDR_COL);
static str port_col         = str_init(PORT_COL);
static str uri_scheme_col   = str_init(URI_SCHEME_COL);
static str transport_col    = str_init(TRANSPORT_COL);
static str strip_col        = str_init(STRIP_COL);
static str tag_col          = str_init(TAG_COL);
static str flags_col        = str_init(FLAGS_COL);
static str lcr_table        = str_init(LCR_TABLE);
static str prefix_col       = str_init(PREFIX_COL);
static str from_uri_col     = str_init(FROM_URI_COL);
static str priority_col     = str_init(PRIORITY_COL);

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

/* prefix mode */
int prefix_mode_param = DEF_PREFIX_MODE;

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

struct lcr_info **lcrs;  /* Pointer to current lcr table pointer */
struct lcr_info *lcrs_1; /* Pointer to lcr table 1 */
struct lcr_info *lcrs_2; /* Pointer to lcr table 2 */

unsigned int *lcrs_ws_reload_counter;
unsigned int reload_counter;

struct prefix_regex prefix_reg[MAX_NO_OF_LCRS];
struct from_uri_regex from_uri_reg[MAX_NO_OF_LCRS];

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
	 fixstringloadgws, 0, REQUEST_ROUTE},
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
	{"port_column",              STR_PARAM, &port_col.s     },
	{"uri_scheme_column",        STR_PARAM, &uri_scheme_col.s },
	{"transport_column",         STR_PARAM, &transport_col.s },
	{"strip_column",             STR_PARAM, &strip_col.s    },
	{"tag_column",               STR_PARAM, &tag_col.s      },
	{"flags_column",             STR_PARAM, &flags_col.s    },
	{"lcr_table",                STR_PARAM, &lcr_table.s    },
	{"prefix_column",            STR_PARAM, &prefix_col.s   },
	{"from_uri_column",          STR_PARAM, &from_uri_col.s },
	{"priority_column",          STR_PARAM, &priority_col.s },
	{"fr_inv_timer_avp",         STR_PARAM,	&fr_inv_timer_avp_param },
	{"gw_uri_avp",               STR_PARAM, &gw_uri_avp_param },
	{"ruri_user_avp",            STR_PARAM, &ruri_user_avp_param },
	{"contact_avp",              STR_PARAM, &contact_avp_param },
	{"rpid_avp",                 STR_PARAM, &rpid_avp_param },
	{"flags_avp",                STR_PARAM, &flags_avp_param },
	{"fr_inv_timer",             INT_PARAM, &fr_inv_timer },
	{"fr_inv_timer_next",        INT_PARAM,	&fr_inv_timer_next },
	{"prefix_mode",              INT_PARAM, &prefix_mode_param },
	{0, 0, 0}
};


/*
 * Exported MI functions
 */
static mi_export_t mi_cmds[] = {
	{ MI_LCR_RELOAD,  mi_lcr_reload,   MI_NO_INPUT_FLAG,  0,  mi_child_init },
	{ MI_LCR_DUMP,    mi_lcr_dump,     MI_NO_INPUT_FLAG,  0,  0 },
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
		LM_CRIT("Null lcr_dbf\n");
		goto error;
	}
	db_handle=lcr_dbf.init(db_url);
	if (db_handle==0){
		LM_ERR("Unable to connect to the database\n");
		goto error;
	}
	return 0;
error:
	return -1;
}



static int lcr_db_bind(const str* db_url)
{
    if (db_bind_mod(db_url, &lcr_dbf)<0){
	LM_ERR("Unable to bind to the database module\n");
	return -1;
    }

    if (!DB_CAPABILITY(lcr_dbf, DB_CAP_QUERY)) {
	LM_ERR("Database module does not implement 'query' function\n");
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
	int i;
    pv_spec_t avp_spec;
    str s;
    unsigned short avp_flags;

    LM_DBG("Initializing\n");

    /* Update length of module variables */
    db_url.len = strlen(db_url.s);
    gw_table.len = strlen(gw_table.s);
    gw_name_col.len = strlen(gw_name_col.s);
    grp_id_col.len = strlen(grp_id_col.s);
    ip_addr_col.len = strlen(ip_addr_col.s);
    port_col.len = strlen(port_col.s);
    uri_scheme_col.len = strlen(uri_scheme_col.s);
    transport_col.len = strlen(transport_col.s);
    strip_col.len = strlen(strip_col.s);
    tag_col.len = strlen(tag_col.s);
    flags_col.len = strlen(flags_col.s);
    lcr_table.len = strlen(lcr_table.s);
    prefix_col.len = strlen(prefix_col.s);
    from_uri_col.len = strlen(from_uri_col.s);
    priority_col.len = strlen(priority_col.s);

    /* Bind database */
    if (lcr_db_bind(&db_url)) {
	LM_ERR("No database module found\n");
	return -1;
    }

    /* Check value of prefix_mode */
    if ((prefix_mode_param != 0) && (prefix_mode_param != 1)) {
	LM_ERR("Invalid prefix_mode value <%d>\n", prefix_mode_param);
	return -1;
    }

    /* Process AVP params */
    if (fr_inv_timer_avp_param && *fr_inv_timer_avp_param) {
	s.s = fr_inv_timer_avp_param; s.len = strlen(s.s);
	if (pv_parse_spec(&s, &avp_spec)==0
	    || avp_spec.type!=PVT_AVP) {
	    LM_ERR("Malformed or non AVP definition <%s>\n",
		   fr_inv_timer_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &fr_inv_timer_avp, &avp_flags)!=0) {
	    LM_ERR("Invalid AVP definition <%s>\n", fr_inv_timer_avp_param);
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
	    LM_ERR("Malformed or non AVP definition <%s>\n", gw_uri_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &gw_uri_avp, &avp_flags)!=0) {
	    LM_ERR("Invalid AVP definition <%s>\n", gw_uri_avp_param);
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
	    LM_ERR("Malformed or non AVP definition <%s>\n",
		   ruri_user_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &ruri_user_avp, &avp_flags)!=0) {
	    LM_ERR("Invalid AVP definition <%s>\n", ruri_user_avp_param);
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
	    LM_ERR("Malformed or non AVP definition <%s>\n",
		   contact_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &contact_avp, &avp_flags)!=0) {
	    LM_ERR("Invalid AVP definition <%s>\n", contact_avp_param);
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
	    LM_ERR("Malformed or non AVP definition <%s>\n", rpid_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &rpid_avp, &avp_flags)!=0) {
	    LM_ERR("Invalid AVP definition <%s>\n", rpid_avp_param);
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
	    LM_ERR("Malformed or non AVP definition <%s>\n", flags_avp_param);
	    return -1;
	}
	
	if(pv_get_avp_name(0, &(avp_spec.pvp), &flags_avp, &avp_flags)!=0) {
	    LM_ERR("Invalid AVP definition <%s>\n", flags_avp_param);
	    return -1;
	}
	flags_avp_type = avp_flags;
    } else {
	LM_ERR("AVP flags_avp has not been defined\n");
	return -1;
    }

    /* Check table version */
	db_con_t* dbh;
	if (lcr_dbf.init==0){
		LM_CRIT("Unbound database\n");
		return -1;
	}
	dbh=lcr_dbf.init(&db_url);
	if (dbh==0){
		LM_ERR("Unable to open database connection\n");
		return -1;
	}
	if((db_check_table_version(&lcr_dbf, dbh, &gw_table, GW_TABLE_VERSION) < 0) ||
		(db_check_table_version(&lcr_dbf, dbh, &lcr_table, LCR_TABLE_VERSION) < 0)) {
			LM_ERR("error during table version check.\n");
			lcr_dbf.close(dbh);
			goto err;
    }
	lcr_dbf.close(dbh);

    /* Initializing gw tables and gw table pointer variable */
    gws_1 = (struct gw_info *)shm_malloc(sizeof(struct gw_info) *
					 (MAX_NO_OF_GWS + 1));
    if (gws_1 == 0) {
	LM_ERR("No memory for gw table\n");
	goto err;
    }
    gws_2 = (struct gw_info *)shm_malloc(sizeof(struct gw_info) *
					 (MAX_NO_OF_GWS + 1));
    if (gws_2 == 0) {
	LM_ERR("No memory for gw table\n");
	goto err;
    }
    for (i = 0; i < MAX_NO_OF_GWS + 1; i++) {
	gws_1[i].ip_addr = gws_2[i].ip_addr = 0;
    }
    gws = (struct gw_info **)shm_malloc(sizeof(struct gw_info *));
    if (gws == 0) {
	LM_ERR("No memory for gw table pointer\n");
    }
    *gws = gws_1;

    /* Initializing lcr tables and lcr table pointer variable */
    lcrs_1 = (struct lcr_info *)shm_malloc(sizeof(struct lcr_info) *
					   (MAX_NO_OF_LCRS + 1));
    if (lcrs_1 == 0) {
	LM_ERR("No memory for lcr table\n");
	goto err;
    }
    lcrs_2 = (struct lcr_info *)shm_malloc(sizeof(struct lcr_info) *
					   (MAX_NO_OF_LCRS + 1));
    if (lcrs_2 == 0) {
	LM_ERR("No memory for lcr table\n");
	goto err;
    }
    for (i = 0; i < MAX_NO_OF_LCRS + 1; i++) {
	lcrs_1[i].end_record = lcrs_2[i].end_record = 0;
    }
    lcrs = (struct lcr_info **)shm_malloc(sizeof(struct lcr_info *));
    if (lcrs == 0) {
	LM_ERR("No memory for lcr table pointer\n");
	goto err;
    }
    *lcrs = lcrs_1;

    lcrs_ws_reload_counter = (unsigned int *)shm_malloc(sizeof(unsigned int));
    if (lcrs_ws_reload_counter == 0) {
	LM_ERR("No memory for reload counter\n");
	goto err;
    }
    *lcrs_ws_reload_counter = reload_counter = 0;

    memset(prefix_reg, 0, sizeof(struct prefix_regex) * MAX_NO_OF_LCRS);
    memset(from_uri_reg, 0, sizeof(struct from_uri_regex) * MAX_NO_OF_LCRS);

    /* First reload */
    if (reload_gws() == -1) {
	LM_CRIT("Failed to reload gateways and routes\n");
	goto err;
    }

    return 0;

err:
    return -1;
}


static void destroy(void)
{
	lcr_db_close();
}

/*
 * Sort lcr records by prefix_len and priority.
 */
static int comp_lcrs(const void *m1, const void *m2)
{
    int result = -1;

    struct mi *mi1 = (struct mi *) m1;
    struct mi *mi2 = (struct mi *) m2;

    struct lcr_info lcr_record1 = (*lcrs)[mi1->route_index];
    struct lcr_info lcr_record2 = (*lcrs)[mi2->route_index];

    if (prefix_mode_param == 0) {
        /* Sort by prefix. */
        if (lcr_record1.prefix_len > lcr_record2.prefix_len) {
	    result = 1;
        } else if (lcr_record1.prefix_len == lcr_record2.prefix_len) {
	    /* Sort by priority. */
	    if (lcr_record1.priority < lcr_record2.priority) {
	        result = 1;
	    } else if (lcr_record1.priority == lcr_record2.priority) {
	        /* Nothing to do. */
	        result = 0;
	    }
        }
    } else {
        if (lcr_record1.priority < lcr_record2.priority) {
	    result = 1;
	} else if (lcr_record1.priority == lcr_record2.priority) {
	    /* Nothing to do. */
	    result = 0;
	}
    }

    return result;
}

/*
 * Sort lcr records by rand table.
 */
static int rand_lcrs(const void *m1, const void *m2)
{
    int result = -1;

    struct mi mi1 = *((struct mi *) m1);
    struct mi mi2 = *((struct mi *) m2);

    if (mi1.randomizer > mi2.randomizer) {
	result = 1;
    } else if (mi1.randomizer == mi2.randomizer) {
	result = 0;
    }

    return result;
}

/*
 * regcomp each prefix.
 */
static int load_prefix_regex(void)
{
    int i, status, result = 0;

    for (i = 0; i < MAX_NO_OF_LCRS; i++) {
	if ((*lcrs)[i].end_record != 0) {
	    break;
	}
	if (prefix_reg[i].valid) {
	    regfree(&(prefix_reg[i].re));
	    prefix_reg[i].valid = 0;
	}
	memset(&(prefix_reg[i].re), 0, sizeof(regex_t));
	if ((status=regcomp(&(prefix_reg[i].re),(*lcrs)[i].prefix,0))!=0){
	    LM_ERR("bad prefix re <%s>, regcomp returned %d (check regex.h)\n",
		(*lcrs)[i].prefix, status);
	    result = -1;
	    break;
	}
	prefix_reg[i].valid = 1;
    }

    return result;
}

/*
 * regcomp each from_uri.
 */
static int load_from_uri_regex(void)
{
    int i, status, result = 0;

    for (i = 0; i < MAX_NO_OF_LCRS; i++) {
	if ((*lcrs)[i].end_record != 0) {
	    break;
	}
	if (from_uri_reg[i].valid) {
	    regfree(&(from_uri_reg[i].re));
	    from_uri_reg[i].valid = 0;
	}
	memset(&(from_uri_reg[i].re), 0, sizeof(regex_t));
	if ((status=regcomp(&(from_uri_reg[i].re),(*lcrs)[i].from_uri,0))!=0){
	    LM_ERR("Bad from_uri re <%s>, regcomp returned %d (check regex.h)\n",
	    	(*lcrs)[i].from_uri, status);
	    result = -1;
	    break;
	}
	from_uri_reg[i].valid = 1;
    }

    if (result != -1) {
	reload_counter = *lcrs_ws_reload_counter;
    }
    return result;
}

static int load_all_regex(void)
{
	int result =0;

	if (prefix_mode_param != 0) {
		result = load_prefix_regex();
	}

	if (result == 0) {
		result = load_from_uri_regex();
	} else {
		LM_ERR("Unable to load prefix regex\n");
	}

	if (result == 0) {
		reload_counter = *lcrs_ws_reload_counter;
	} else {
		LM_ERR("Unable to load from_uri regex\n");
	}

	return result;
}

/*
 * Reload gws to unused gw table and lcrs to unused lcr table, and, when done
 * make unused gw and lcr table the one in use.
 */
int reload_gws(void)
{
    unsigned int i, port, strip, tag_len, prefix_len, from_uri_len,
    grp_id, priority;
    struct in_addr ip_addr;
    unsigned int flags;
    uri_type scheme;
    uri_transport transport;
    db_con_t* dbh;
    char *tag, *prefix, *from_uri;
    db_res_t* res = NULL;
    db_row_t* row;
    db_key_t gw_cols[8];
    db_key_t lcr_cols[4];

    gw_cols[0] = &ip_addr_col;
    gw_cols[1] = &port_col;
    gw_cols[2] = &uri_scheme_col;
    gw_cols[3] = &transport_col;
    gw_cols[4] = &strip_col;
    gw_cols[5] = &tag_col;
    /* FIXME: is this ok if we have different names for grp_id
       in the two tables? (ge vw lcr) */
    gw_cols[6] = &grp_id_col;
    gw_cols[7] = &flags_col;

    lcr_cols[0] = &prefix_col;
    lcr_cols[1] = &from_uri_col;
    /* FIXME: is this ok if we have different names for grp_id
       in the two tables? (ge vw lcr) */
    lcr_cols[2] = &grp_id_col;
    lcr_cols[3] = &priority_col;

    if (lcr_dbf.init==0){
	LM_CRIT("Unbound database\n");
	return -1;
    }
    dbh=lcr_dbf.init(&db_url);
    if (dbh==0){
	LM_ERR("Unable to open database connection\n");
	return -1;
    }

    if (lcr_dbf.use_table(dbh, &gw_table) < 0) {
	LM_ERR("Error while trying to use gw table\n");
	return -1;
    }

    if (lcr_dbf.query(dbh, NULL, 0, NULL, gw_cols, 0, 8, 0, &res) < 0) {
	    LM_ERR("Failed to query gw data\n");
	    lcr_dbf.close(dbh);
	    return -1;
    }

    if (RES_ROW_N(res) + 1 > MAX_NO_OF_GWS) {
	    LM_ERR("Too many gateways\n");
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
    }

    for (i = 0; i < RES_ROW_N(res); i++) {
	row = RES_ROWS(res) + i;
	if (!((VAL_TYPE(ROW_VALUES(row)) == DB_STRING) &&
	      !VAL_NULL(ROW_VALUES(row)) &&
	      inet_aton((char *)VAL_STRING(ROW_VALUES(row)), &ip_addr) != 0)) {
	    LM_ERR("Invalid IP address of gw <%s>\n",
		   (char *)VAL_STRING(ROW_VALUES(row)));
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
	}
	if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
	    port = 0;
	} else {
	    port = (unsigned int)VAL_INT(ROW_VALUES(row) + 1);
	}
	if (port > 65536) {
	    LM_ERR("Port of gw is too large <%u>\n", port);
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
	}
	if (VAL_NULL(ROW_VALUES(row) + 2) == 1) {
	    scheme = SIP_URI_T;
	} else {
	    scheme = (uri_type)VAL_INT(ROW_VALUES(row) + 2);
	    if ((scheme != SIP_URI_T) && (scheme != SIPS_URI_T)) {
		LM_ERR("Unknown or unsupported URI scheme <%u>\n",
		       (unsigned int)scheme);
		lcr_dbf.free_result(dbh, res);
		lcr_dbf.close(dbh);
		return -1;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 3) == 1) {
	    transport = PROTO_NONE;
	} else {
	    transport = (uri_transport)VAL_INT(ROW_VALUES(row) + 3);
	    if ((transport != PROTO_UDP) && (transport != PROTO_TCP) &&
		(transport != PROTO_TLS) && (transport != PROTO_SCTP)) {
		LM_ERR("Unknown or unsupported transport <%u>\n",
		       (unsigned int)transport);
		lcr_dbf.free_result(dbh, res);
		lcr_dbf.close(dbh);
		return -1;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 4) == 1) {
	    strip = 0;
	} else {
	    strip = (unsigned int)VAL_INT(ROW_VALUES(row) + 4);
	}
	if (VAL_NULL(ROW_VALUES(row) + 5) == 1) {
	    tag_len = 0;
	    tag = (char *)0;
	} else {
	    tag = (char *)VAL_STRING(ROW_VALUES(row) + 5);
	    tag_len = strlen(tag);
	    if (tag_len > MAX_TAG_LEN) {
		LM_ERR("Too long gw tag <%u>\n", tag_len);
		lcr_dbf.free_result(dbh, res);
		lcr_dbf.close(dbh);
		return -1;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 6) == 1) {
	    grp_id = 0;
	} else {
	    grp_id = VAL_INT(ROW_VALUES(row) + 6);
	}
	if (!VAL_NULL(ROW_VALUES(row) + 7) &&
	    (VAL_TYPE(ROW_VALUES(row) + 7) == DB_INT)) {
	    flags = (unsigned int)VAL_INT(ROW_VALUES(row) + 7);
	} else {
	    LM_ERR("Attribute flags is NULL or non-int\n");
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
	}
	if (*gws == gws_1) {
	    gws_2[i].ip_addr = (unsigned int)ip_addr.s_addr;
	    gws_2[i].port = port;
	    gws_2[i].grp_id = grp_id;
	    gws_2[i].scheme = scheme;
	    gws_2[i].transport = transport;
	    gws_2[i].flags = flags;
	    gws_2[i].strip = strip;
	    gws_2[i].tag_len = tag_len;
	    if (tag_len)
		memcpy(&(gws_2[i].tag[0]), tag, tag_len);
	} else {
	    gws_1[i].ip_addr = (unsigned int)ip_addr.s_addr;
	    gws_1[i].port = port;
	    gws_1[i].grp_id = grp_id;
	    gws_1[i].scheme = scheme;
	    gws_1[i].transport = transport;
	    gws_1[i].flags = flags;
	    gws_1[i].strip = strip;
	    gws_1[i].tag_len = tag_len;
	    if (tag_len)
		memcpy(&(gws_1[i].tag[0]), tag, tag_len);
	}
    }

    lcr_dbf.free_result(dbh, res);

    if (*gws == gws_1) {
	gws_2[i].ip_addr = 0;
	*gws = gws_2;
    } else {
	gws_1[i].ip_addr = 0;
	*gws = gws_1;
    }


    if (lcr_dbf.use_table(dbh, &lcr_table) < 0) {
	LM_ERR("Error while trying to use lcr table\n");
	return -1;
    }

    if (lcr_dbf.query(dbh, NULL, 0, NULL, lcr_cols, 0, 4, 0, &res) < 0) {
	LM_ERR("Failed to query lcr data\n");
	lcr_dbf.close(dbh);
	return -1;
    }

    if (RES_ROW_N(res) + 1 > MAX_NO_OF_LCRS) {
	LM_ERR("Too many lcr entries <%d>\n", RES_ROW_N(res));
	lcr_dbf.free_result(dbh, res);
	lcr_dbf.close(dbh);
	return -1;
    }
    for (i = 0; i < RES_ROW_N(res); i++) {
	row = RES_ROWS(res) + i;
	if (VAL_NULL(ROW_VALUES(row)) == 1) {
	    prefix_len = 0;
	    prefix = 0;
	} else {
	    prefix = (char *)VAL_STRING(ROW_VALUES(row));
	    prefix_len = strlen(prefix);
	    if (prefix_len > MAX_PREFIX_LEN) {
		LM_ERR("Too long lcr prefix <%u>\n", prefix_len);
		lcr_dbf.free_result(dbh, res);
		lcr_dbf.close(dbh);
		return -1;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
	    from_uri_len = 0;
	    from_uri = 0;
	} else {
	    from_uri = (char *)VAL_STRING(ROW_VALUES(row) + 1);
	    from_uri_len = strlen(from_uri);
	    if (from_uri_len > MAX_FROM_URI_LEN) {
		LM_ERR("Too long from_uri <%u>\n", from_uri_len);
		lcr_dbf.free_result(dbh, res);
		lcr_dbf.close(dbh);
		return -1;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 2) == 1) {
	    LM_ERR("Route grp_id is NULL\n");
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
	}
	grp_id = (unsigned int)VAL_INT(ROW_VALUES(row) + 2);
	if (VAL_NULL(ROW_VALUES(row) + 3) == 1) {
	    LM_ERR("Route priority is NULL\n");
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
	}
	priority = (unsigned int)VAL_INT(ROW_VALUES(row) + 3);

	if (*lcrs == lcrs_1) {
	    lcrs_2[i].prefix_len = prefix_len;
	    if (prefix_len)
		memcpy(&(lcrs_2[i].prefix[0]), prefix, prefix_len);
	    lcrs_2[i].from_uri_len = from_uri_len;
	    if (from_uri_len) {
		memcpy(&(lcrs_2[i].from_uri[0]), from_uri, from_uri_len);
		lcrs_2[i].from_uri[from_uri_len] = '\0';
	    }
	    lcrs_2[i].grp_id = grp_id;
	    lcrs_2[i].priority = priority;
	    lcrs_2[i].end_record = 0;
	} else {
	    lcrs_1[i].prefix_len = prefix_len;
	    if (prefix_len)
		memcpy(&(lcrs_1[i].prefix[0]), prefix, prefix_len);
	    lcrs_1[i].from_uri_len = from_uri_len;
	    if (from_uri_len) {
		memcpy(&(lcrs_1[i].from_uri[0]), from_uri, from_uri_len);
		lcrs_1[i].from_uri[from_uri_len] = '\0';
	    }
	    lcrs_1[i].grp_id = grp_id;
	    lcrs_1[i].priority = priority;
	    lcrs_1[i].end_record = 0;
	}
    }

    lcr_dbf.free_result(dbh, res);
    lcr_dbf.close(dbh);

    if (*lcrs == lcrs_1) {
	lcrs_2[i].end_record = 1;
	*lcrs = lcrs_2;
    } else {
	lcrs_1[i].end_record = 1;
	*lcrs = lcrs_1;
    }

    (*lcrs_ws_reload_counter)++;
    if (0 != load_all_regex()) {

	return -1;
    }

    return 1;
}


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

    for (i = 0; i < MAX_NO_OF_GWS; i++) {

	if ((*gws)[i].ip_addr == 0) 
	    break;

	node= add_mi_node_child(rpl,0 ,"GW", 2, 0, 0);
	if(node == NULL)
	    return -1;

	p = int2str((unsigned long)(*gws)[i].grp_id, &len );
	attr = add_mi_attr(node, MI_DUP_VALUE, "GRP_ID", 6, p, len );
	if(attr == NULL)
	    return -1;

	transport = (*gws)[i].transport;
	if (transport == PROTO_UDP)
	    transp= ";transport=udp";
	else  if (transport == PROTO_TCP)
	    transp= ";transport=tcp";
	else  if (transport == PROTO_TLS)
	    transp= ";transport=tls";
	else  if (transport == PROTO_SCTP)
	    transp= ";transport=sctp";
	else
	    transp= "";

	address.af = AF_INET;
	address.len = 4;
	address.u.addr32[0] = (*gws)[i].ip_addr;
	attr= addf_mi_attr(node,0 ,"URI", 3,"%s:%s:%d%s",
			   ((*gws)[i].scheme == SIP_URI_T)?"sip":"sips",
			   ip_addr2a(&address),
			   ((*gws)[i].port == 0)?5060:(*gws)[i].port,transp);
	if(attr == NULL)
	    return -1;

	p = int2str((unsigned long)(*gws)[i].strip, &len );
	attr = add_mi_attr(node, MI_DUP_VALUE, "STRIP", 5, p, len);
	if(attr == NULL)
	    return -1;

	attr = add_mi_attr(node, MI_DUP_VALUE, "TAG", 3,
			   (*gws)[i].tag, (*gws)[i].tag_len );
	if(attr == NULL)
	    return -1;

	p = int2str((unsigned long)(*gws)[i].flags, &len );
	attr = add_mi_attr(node, MI_DUP_VALUE, "FLAGS", 5, p, len);
	if(attr == NULL)
	    return -1;
    }

    for (i = 0; i < MAX_NO_OF_LCRS; i++) {
	if ((*lcrs)[i].end_record != 0)
	    break;

	node= add_mi_node_child(rpl, 0, "RULE", 4, 0, 0);
	attr = add_mi_attr(node, 0, "PREFIX", 6, (*lcrs)[i].prefix,
			   (*lcrs)[i].prefix_len );
	if(attr== 0)
	    return -1;

	attr = add_mi_attr(node, 0, "FROM_URI", 8, (*lcrs)[i].from_uri,
			   (*lcrs)[i].from_uri_len );
	if(attr== 0)
	    return -1;
	
	p = int2str((unsigned long)(*lcrs)[i].grp_id, &len );
	attr = add_mi_attr(node, MI_DUP_VALUE, "GRP_ID", 6, p, len );
	if(attr == NULL)
	    return -1;

	p = int2str((unsigned long)(*lcrs)[i].priority, &len );
	attr = add_mi_attr(node, MI_DUP_VALUE, "PRIORITY", 8, p, len );
	if(attr == NULL)
	    return -1;

    }

    return 0;
}


/*
 * Load info of matching GWs from database to gw_uri AVPs
 */
static int do_load_gws(struct sip_msg* _m, str *_from_uri, int _grp_id)
{
    str ruri_user, from_uri, value;
    char from_uri_str[MAX_FROM_URI_LEN + 1];
    char ruri[MAX_URI_SIZE];
    unsigned int i, j, k, index, addr, port, strip, gw_index,
	duplicated_gw, flags, have_rpid_avp;
    uri_type scheme;
    uri_transport transport;
    struct ip_addr address;
    str addr_str, port_str;
    char *at, *tag, *strip_string, *flags_string;
    struct usr_avp *avp;
    int_str val;
    struct mi matched_gws[MAX_NO_OF_GWS + 1];
    unsigned short tag_len, prefix_len, priority;
    int randomizer_start, randomizer_end, randomizer_flag,
	strip_len, flags_len;
    struct lcr_info lcr_rec;

    /* Find Request-URI user */
    if (parse_sip_msg_uri(_m) < 0) {
	    LM_ERR("Error while parsing R-URI\n");
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
		LM_ERR("Error while parsing headers\n");
		return -1;
	    }
	    if (!_m->from) {
		LM_ERR("From header field not found\n");
		return -1;
	    }
	    if ((!(_m->from)->parsed) && (parse_from_header(_m) < 0)) {
		LM_ERR("Error while parsing From header\n");
		return -1;
	    }
	    from_uri = get_from(_m)->uri;
	}
    }
    if (from_uri.len <= MAX_FROM_URI_LEN) {
	strncpy(from_uri_str, from_uri.s, from_uri.len);
	from_uri_str[from_uri.len] = '\0';
    } else {
	LM_ERR("From URI is too long <%u>\n", from_uri.len);
	return -1;
    }

    /*
     * Check if the gws and lcrs were reloaded
     */
	if (reload_counter != *lcrs_ws_reload_counter) {
		if (load_all_regex() != 0) {
		    return -1;
		}
	}

    /*
     * Let's match the gws:
     *  1. prefix matching
     *  2. from_uri matching
     *  3. _grp_id matching
     *
     * Note: A gateway must be in the list _only_ once.
     */
    gw_index = 0;
    duplicated_gw = 0;
    for (i = 0; i < MAX_NO_OF_LCRS; i++) {
	lcr_rec = (*lcrs)[i];
	if (lcr_rec.end_record != 0) {
	    break;
	}
	if ( ((prefix_mode_param == 0) && (lcr_rec.prefix_len <= ruri_user.len) &&
	      (strncmp(lcr_rec.prefix, ruri_user.s, lcr_rec.prefix_len)==0)) ||
	     ( (prefix_mode_param != 0) && ( (lcr_rec.prefix_len == 0) ||
					(prefix_reg[i].valid &&
					 (regexec(&(prefix_reg[i].re), ruri_user.s, 0,
						  (regmatch_t *)NULL, 0) == 0)) ) ) ) {
	    /* 1. Prefix matching is done */
	    if ((lcr_rec.from_uri_len == 0) ||
		(from_uri_reg[i].valid &&
		 (regexec(&(from_uri_reg[i].re), from_uri_str, 0,
			  (regmatch_t *)NULL, 0) == 0))) {
		/* 2. from_uri matching is done */
		for (j = 0; j < MAX_NO_OF_GWS; j++) {
		    if ((*gws)[j].ip_addr == 0) {
			break;
		    }
		    if (lcr_rec.grp_id == (*gws)[j].grp_id &&
			(_grp_id < 0 || (*gws)[j].grp_id == _grp_id)) {
			/* 3. _grp_id matching is done */
			for (k = 0; k < gw_index; k++) {
			    if ((*gws)[j].ip_addr ==
				(*gws)[matched_gws[k].gw_index].ip_addr) {
				/* Found the same gw in the list  */
				/* Let's keep the one with higher */
				/* match on prefix len            */
				LM_DBG("Duplicate gw for index"
				       " %d [%d,%d] and current [%d,%d] \n",
				       k, matched_gws[k].route_index,
				       matched_gws[k].route_index, i, j);
				duplicated_gw = 1;
				if (lcr_rec.prefix_len >
				    (*lcrs)[matched_gws[k].route_index].prefix_len) {
				    /* Replace the old entry with the new one */
				    LM_DBG("Replace [%d,%d]"
					   " with [%d,%d] on index %d:"
					   " prefix reason %d>%d\n",
					   matched_gws[k].route_index,
					   matched_gws[k].gw_index, i, j, k,
					   lcr_rec.prefix_len,
					   (*lcrs)[matched_gws[k].route_index].prefix_len);
				    matched_gws[k].route_index = i;
				    matched_gws[k].gw_index = j;
				    /* Stop searching in the matched_gws list */
				    break;
				} else if (lcr_rec.prefix_len ==
					   (*lcrs)[matched_gws[k].route_index].prefix_len) {
				    if (lcr_rec.priority >
					(*lcrs)[matched_gws[k].route_index].priority) {
					/* Replace the old entry with the new one */
					LM_DBG("Replace [%d,%d] with"
					       " [%d,%d] on index %d:"
					       " priority reason %d>%d\n",
					       matched_gws[k].route_index,
					       matched_gws[k].gw_index,
					       i, j, k, lcr_rec.priority,
					       (*lcrs)[matched_gws[k].route_index].priority);
					matched_gws[k].route_index = i;
					matched_gws[k].gw_index = j;
					/* Stop searching in the matched_gws list */
					break;
				    }
				}
			    }
			}
			if (duplicated_gw == 0) {
			    /* This is a new gw */
			    matched_gws[gw_index].route_index = i;
			    matched_gws[gw_index].gw_index = j;
			    LM_DBG("Added matched_gws[%d]=[%d,%d]\n",
				   gw_index, i, j);
			    gw_index++;
			} else {
			    duplicated_gw = 0;
			}
		    }
		}
	    }
	}
    }
    matched_gws[gw_index].route_index = -1;
    matched_gws[gw_index].gw_index = -1;

    /*
     * Sort the gateways based on:
     *  1. prefix len
     *  2. priority
     */
    qsort(matched_gws, gw_index, sizeof(struct mi), comp_lcrs);
	randomizer_start = 0;

    /* Randomizing the gateways with same prefix_len and same priority */
    randomizer_flag = 0;
    prefix_len = (*lcrs)[matched_gws[0].route_index].prefix_len;
    priority = (*lcrs)[matched_gws[0].route_index].priority;
    for (i = 1; i < gw_index; i++) {
 	if ( prefix_len == (*lcrs)[matched_gws[i].route_index].prefix_len &&
 	     priority == (*lcrs)[matched_gws[i].route_index].priority) {
	    /* we have a match */
	    if (randomizer_flag == 0) {
		randomizer_flag = 1;
		randomizer_start = i - 1;
	    }
	    matched_gws[i - 1].randomizer = rand();
 	}
	else {
	    if (randomizer_flag == 1) {
		randomizer_end = i - 1;
		randomizer_flag = 0;
		qsort(&matched_gws[randomizer_start],
		      randomizer_end - randomizer_start + 1,
		      sizeof(struct mi), rand_lcrs);
	    }
	    prefix_len = (*lcrs)[matched_gws[i].route_index].prefix_len;
	    priority = (*lcrs)[matched_gws[i].route_index].priority;
	}
    }
    if (randomizer_flag == 1) {
	randomizer_end = gw_index - 1;
	matched_gws[i - 1].randomizer = rand();
	qsort(&matched_gws[randomizer_start],
	      randomizer_end - randomizer_start + 1,
	      sizeof(struct mi), rand_lcrs);
    }

    for (i = 0; i < MAX_NO_OF_GWS; i++) {
	index = matched_gws[i].gw_index;
	if (index == -1) {
	    break;
	}
      	addr = (*gws)[index].ip_addr;
	port = (*gws)[index].port;
	scheme = (*gws)[index].scheme;
	transport = (*gws)[index].transport;
	flags = (*gws)[index].flags;
	strip = (*gws)[index].strip;
	if (strip > ruri_user.len) {
	    LM_ERR("Strip count of gw is too large <%u>\n", strip);
	    goto skip;
	}
	tag_len = (*gws)[index].tag_len;
	tag = (*gws)[index].tag;
	if (6 + tag_len + 40 /* flags + strip */ + 1 + 15 + 1 + 5 + 1 + 14 >
	    MAX_URI_SIZE) {
	    LM_ERR("Request URI would be too long\n");
	    goto skip;
	}
	at = (char *)&(ruri[0]);
	flags_string = int2str(flags, &flags_len);
	memcpy(at, flags_string, flags_len);
	at = at + flags_len;
	if (scheme == SIP_URI_T) {
	    memcpy(at, "sip:", 4); at = at + 4;
	} else if (scheme == SIPS_URI_T) {
	    memcpy(at, "sips:", 5); at = at + 5;
	} else {
	    LM_ERR("Unknown or unsupported URI scheme <%u>\n",
		   (unsigned int)scheme);
	    goto skip;
	}
	if (tag_len) {
	    memcpy(at, tag, tag_len); at = at + tag_len;
	}
	/* Add strip in this form |number.
	 * For example: |3 means strip first 3 characters.
         */
	*at = '|'; at = at + 1;
	strip_string = int2str(strip, &strip_len);
	memcpy(at, strip_string, strip_len);
	at = at + strip_len;
	*at = '@'; at = at + 1;
	address.af = AF_INET;
	address.len = 4;
	address.u.addr32[0] = addr;
	addr_str.s = ip_addr2a(&address);
	addr_str.len = strlen(addr_str.s);
	memcpy(at, addr_str.s, addr_str.len); at = at + addr_str.len;
	if (port != 0) {
	    if (port > 65536) {
		LM_ERR("Port of GW is too large <%u>\n", port);
		goto skip;
	    }
	    *at = ':'; at = at + 1;
	    port_str.s = int2str(port, &port_str.len);
	    memcpy(at, port_str.s, port_str.len); at = at + port_str.len;
	}
	if (transport != PROTO_NONE) {
	    memcpy(at, ";transport=", 11); at = at + 11;
	    if (transport == PROTO_UDP) {
		memcpy(at, "udp", 3); at = at + 3;
	    } else if (transport == PROTO_TCP) {
		memcpy(at, "tcp", 3); at = at + 3;
	    } else if (transport == PROTO_TLS) {
		memcpy(at, "tls", 3); at = at + 3;
	    } else if (transport == PROTO_SCTP) {
		memcpy(at, "sctp", 4); at = at + 4;
	    } else {
		LM_ERR("Unknown or unsupported transport <%u>\n",
		       (unsigned int)transport);
		goto skip;
	    }
	}
	value.s = (char *)&(ruri[0]);
	value.len = at - value.s;
	val.s = value;
	add_avp(gw_uri_avp_type|AVP_VAL_STR, gw_uri_avp, val);
	LM_DBG("Added gw_uri_avp <%.*s>\n", value.len, value.s);
    skip:
	continue;
    }

    return 1;
}

/*
 * Load info of matching GWs from database to gw_uri AVPs
 * taking into account the given group id.  Caller URI is taken
 * from request.
 */
static int load_gws_from_grp(struct sip_msg* _m, char* _s1, char* _s2)
{
	str grp_s;
	unsigned int grp_id;
	
	if(((pv_elem_p)_s1)->spec.getf!=NULL)
	{
		if(pv_printf_s(_m, (pv_elem_p)_s1, &grp_s)!=0)
			return -1;
		if(str2int(&grp_s, &grp_id)!=0)
			return -1;
	} else {
		grp_id = ((pv_elem_p)_s1)->spec.pvp.pvn.u.isname.name.n;
	}
	if (grp_id > 0) return do_load_gws(_m, (str *)0, (int)grp_id);
	else return -1;
}


/*
 * Load info of matching GWs from database to gw_uri AVPs.
 * Caller URI is taken from request.
 */
static int load_gws_0(struct sip_msg* _m, char* _s1, char* _s2)
{
    return do_load_gws(_m, (str *)0, -1);
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
 	    return do_load_gws(_m, &(pv_val.rs), -1);
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
 * If called from request route block, rewrites scheme, host, port, and
 * transport parts of R-URI based on first gw_uri AVP value, which is then
 * destroyed.  Also saves R-URI user to ruri_user AVP for later use in
 * failure route block.
 * If called from failure route block, appends a new branch to request
 * where scheme, host, port, and transport of URI are taken from the first
 * gw_uri AVP value, which is then destroyed.  URI user is taken from
 * ruri_user AVP value saved earlier.
 * Returns 1 upon success and -1 upon failure.
 */
static int next_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
    int_str gw_uri_val, ruri_user_val, val;
    struct action act;
    struct usr_avp *gu_avp, *ru_avp;
    int rval;
    str new_ruri;
    char *at, *at_char, *strip_char, *endptr;
    unsigned int strip;

    gu_avp = search_first_avp(gw_uri_avp_type, gw_uri_avp, &gw_uri_val, 0);
    if (!gu_avp) return -1;

    /* Set flags_avp from integer at the beginning of of gw_uri */
    val.n = (int)strtoul(gw_uri_val.s.s, &at, 0);
    add_avp(flags_avp_type, flags_avp, val);
    LM_DBG("Added flags_avp <%u>\n", (unsigned int)val.n);

    gw_uri_val.s.len = gw_uri_val.s.len - (at - gw_uri_val.s.s);
    gw_uri_val.s.s = at;

    if (route_type == REQUEST_ROUTE) {

	/* Create new Request-URI taking URI user from current Request-URI
	   and other parts of from gw_uri AVP. */
	if (parse_sip_msg_uri(_m) < 0) {
	    LM_ERR("Parsing of R-URI failed\n");
	    return -1;
	}
	new_ruri.s = pkg_malloc(gw_uri_val.s.len + _m->parsed_uri.user.len);
	if (!new_ruri.s) {
	    LM_ERR("No memory for new R-URI\n");
	    return -1;
	}
	at_char = memchr(gw_uri_val.s.s, '@', gw_uri_val.s.len);
	if (!at_char) {
	    pkg_free(new_ruri.s);
	    LM_ERR("No @ in gateway URI <%.*s>\n",
		   gw_uri_val.s.len, gw_uri_val.s.s);
	    return -1;
	}
	strip_char = memchr(gw_uri_val.s.s, '|', gw_uri_val.s.len);
	if (!strip_char || strip_char + 1 >= at_char) {
	    pkg_free(new_ruri.s);
	    LM_ERR("No strip character | and at least one "
		   "character before @ in gateway URI <%.*s>\n",
		   gw_uri_val.s.len, gw_uri_val.s.s);
	    return -1;
	}
	at = new_ruri.s;
	memcpy(at, gw_uri_val.s.s, strip_char - gw_uri_val.s.s);
	at = at + (strip_char - gw_uri_val.s.s);
	strip = strtol(strip_char + 1, &endptr, 10);
	if (endptr != at_char) {
	    pkg_free(new_ruri.s);
	    LM_ERR("Non-digit char between | and @ chars in gw URI <%.*s>\n",
		   gw_uri_val.s.len, gw_uri_val.s.s);
	    return -1;
	}
	if (_m->parsed_uri.user.len - strip > 0) {
	    memcpy(at, _m->parsed_uri.user.s + strip,
		   _m->parsed_uri.user.len - strip);
	    at = at + _m->parsed_uri.user.len - strip;
	}
	if (*(at - 1) != ':') {
	    memcpy(at, at_char, gw_uri_val.s.len - (at_char - gw_uri_val.s.s));
	    at = at + gw_uri_val.s.len - (at_char - gw_uri_val.s.s);
	} else {
	    memcpy(at, at_char + 1, gw_uri_val.s.len -
		   (at_char + 1 - gw_uri_val.s.s));
	    at = at + gw_uri_val.s.len - (at_char + 1 - gw_uri_val.s.s);
	}
	*at = '\0';
	/* Save Request-URI user for use in FAILURE_ROUTE */
	val.s = _m->parsed_uri.user;
	add_avp(ruri_user_avp_type|AVP_VAL_STR, ruri_user_avp, val);
	LM_DBG("Added ruri_user_avp <%.*s>\n", val.s.len, val.s.s);
	/* Rewrite Request URI */
	act.type = SET_URI_T;
	act.elem[0].type = STRING_ST;
	act.elem[0].u.string = new_ruri.s;
	rval = do_action(&act, _m);
	pkg_free(new_ruri.s);
	destroy_avp(gu_avp);
	if (rval != 1) {
	    LM_ERR("Calling do_action failed with return value <%d>\n", rval);
	    return -1;
	}
	return 1;

    } else if (route_type == FAILURE_ROUTE) {

	/* Create new Request-URI taking URI user from ruri_user AVP
	   and other parts of from gateway URI AVP. */
	ru_avp = search_first_avp(ruri_user_avp_type, ruri_user_avp,
				  &ruri_user_val, 0);
	if (!ru_avp) {
	    LM_ERR("No ruri_user AVP\n");
	    return -1;
	}
	new_ruri.s = pkg_malloc(gw_uri_val.s.len + _m->parsed_uri.user.len);
	if (!new_ruri.s) {
	    LM_ERR("No memory for new R-URI.\n");
	    return -1;
	}
	at_char = memchr(gw_uri_val.s.s, '@', gw_uri_val.s.len);
	if (!at_char) {
	    pkg_free(new_ruri.s);
	    LM_ERR("No @ in gateway URI <%.*s>\n",
		   gw_uri_val.s.len, gw_uri_val.s.s);
	    return -1;
	}
	strip_char = memchr(gw_uri_val.s.s, '|', gw_uri_val.s.len);
	if (!strip_char || strip_char + 1 >= at_char) {
	    pkg_free(new_ruri.s);
	    LM_ERR("No strip char | and at least one "
		   "char before @ in gateway URI <%.*s>\n",
		   gw_uri_val.s.len, gw_uri_val.s.s);
	    return -1;
	}
	at = new_ruri.s;
	memcpy(at, gw_uri_val.s.s, strip_char - gw_uri_val.s.s);
	at = at + (strip_char - gw_uri_val.s.s);
	strip = strtol(strip_char + 1, &endptr, 10);
	if (endptr != at_char) {
	    pkg_free(new_ruri.s);
	    LM_ERR("Non-digit char between | and @ chars in gw URI <%.*s>\n",
		   gw_uri_val.s.len, gw_uri_val.s.s);
	    return -1;
	}
	if (ruri_user_val.s.len - strip > 0) {
	    memcpy(at, ruri_user_val.s.s + strip,
		   ruri_user_val.s.len - strip);
	    at = at + ruri_user_val.s.len - strip;
	}
	if (*(at - 1) != ':') {
	    memcpy(at, at_char, gw_uri_val.s.len - (at_char - gw_uri_val.s.s));
	    at = at + gw_uri_val.s.len - (at_char - gw_uri_val.s.s);
	} else {
	    memcpy(at, at_char + 1, gw_uri_val.s.len -
		   (at_char + 1 - gw_uri_val.s.s));
	    at = at + gw_uri_val.s.len - (at_char + 1 - gw_uri_val.s.s);
	}
	new_ruri.len = at - new_ruri.s;
	act.type = APPEND_BRANCH_T;
	act.elem[0].type = STRING_ST;
	act.elem[0].u.s = new_ruri;
	act.elem[1].type = NUMBER_ST;
	act.elem[1].u.number = 0;
	rval = do_action(&act, _m);
	pkg_free(new_ruri.s);
	destroy_avp(gu_avp);
	if (rval != 1) {
	    LM_ERR("Calling do_action failed with return value <%d>\n", rval);
	    return -1;
	}
	return 1;
    }

    /* unsupported route type */
    LM_ERR("Unsupported route type\n");
    return -1;
}


/*
 * Checks if request comes from a gateway
 */
static int do_from_gw(struct sip_msg* _m, pv_spec_t *addr_sp, int grp_id)
{
    int i;
    unsigned int src_addr;
    pv_value_t pv_val;
    struct ip_addr *ip;
    int_str val;

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
			LM_ERR("IP address PV empty value\n");
			return -1;
		}
	} else {
		src_addr = _m->rcv.src_ip.u.addr32[0];
	}

    for (i = 0; i < MAX_NO_OF_GWS; i++) {
	if ((*gws)[i].ip_addr == 0) {
	    return -1;
	}
	if ((*gws)[i].ip_addr == src_addr && 
	    (grp_id < 0 || (*gws)[i].grp_id == grp_id)) {
	    LM_DBG("Request came from gw\n");
	    val.n = (int)(*gws)[i].flags;
	    add_avp(flags_avp_type, flags_avp, val);
	    LM_DBG("Added flags_avp <%u>\n", (unsigned int)val.n);
	    return 1;
	}
    }

    LM_DBG("Request did not come from gw\n");
    return -1;
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
    unsigned int i;

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

    for (i = 0; i < MAX_NO_OF_GWS; i++) {
	if ((*gws)[i].ip_addr == 0) {
	    return -1;
	}
	if ((*gws)[i].ip_addr == addr.s_addr && 
		(grp_id < 0 || (*gws)[i].grp_id == grp_id)) {
	    return 1;
	}
    }

    return -1;
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
	LM_ERR("No memory left for branch info\n");
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
	    LM_ERR("Failed to convert socket to str\n");
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
			      struct socket_info **sock, unsigned int *flags)
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
	    LM_ERR("Parsing of socket info <%.*s> failed\n",  s.len, s.s);
	    return 0;
	}
	*sock = grep_sock_info(&host, (unsigned short)port,
			       (unsigned short)proto);
	if (*sock == 0) {
	    LM_ERR("Invalid socket <%.*s>\n", s.len, s.s);
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
	    LM_ERR("Failed to decode flags <%.*s>\n", s.len, s.s);
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
	LM_DBG("Nothing to do - no branches!\n");
	return 1;
    }

    ruri = GET_RURI(msg);
    if (!ruri) {
	LM_ERR("No Request-URI found\n");
	return -1;
    }
    ruri_q = get_ruri_q();

    for(idx = 0; (uri.s = get_branch(idx, &uri.len, &q, 0, 0, 0, 0)) != 0;
	idx++) {
	if (q != ruri_q) {
	    goto rest;
	}
    }
    LM_DBG("Nothing to do - all contacts have same q!\n");
    return 1;

rest:
    /* Insert Request-URI branch to contact list */
    contacts = (struct contact *)pkg_malloc(sizeof(struct contact));
    if (!contacts) {
	LM_ERR("No memory for contact info\n");
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
	    LM_ERR("No memory for contact info\n");
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
	    LM_ERR("Encoding of branch info failed\n");
	    free_contact_list(contacts);
	    if (branch_info.s) pkg_free(branch_info.s);
	    return -1;
	}
	val.s = branch_info;
	add_avp(contact_avp_type|AVP_VAL_STR|(curr->q_flag),
		contact_avp, val);
	pkg_free(branch_info.s);
	LM_DBG("Loaded contact <%.*s> with q_flag <%d>\n",
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
	    LM_DBG("No AVPs -- we are done!\n");
	    return 1;
	}

	LM_DBG("Next contact is <%s>\n", val.s.s);

	if (decode_branch_info(val.s.s, &uri, &dst, &path, &sock, &flags)
	    == 0) {
	    LM_ERR("Decoding of branch info <%.*s> failed\n",
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
		LM_ERR("Setting of fr_inv_timer_avp failed\n");
		return -1;
	    }
	    return 1;
	}

	/* Append branches until out of branches or Q_FLAG is set */
	prev = avp;
	while ((avp = search_next_avp(avp, &val))) {
	    destroy_avp(prev);

	    LM_DBG("Next contact is <%s>\n", val.s.s);

	    if (decode_branch_info(val.s.s, &uri, &dst, &path, &sock, &flags)
		== 0) {
		LM_ERR("Decoding of branch info <%.*s> failed\n",
		       val.s.len, val.s.s);
		destroy_avp(avp);
		return -1;
	    }

	    if (append_branch(msg, &uri, &dst, &path, 0, flags, sock) != 1) {
		LM_ERR("Appending branch failed\n");
		destroy_avp(avp);
		return -1;
	    }

	    if (avp->flags & Q_FLAG) {
		destroy_avp(avp);
		val.n = fr_inv_timer_next;
		if (add_avp(fr_inv_timer_avp_type, fr_inv_timer_avp, val)
		    != 0) {
		    LM_ERR("Setting of fr_inv_timer_avp failed\n");
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
		LM_ERR("Decoding of branch info <%.*s> failed\n",
		       val.s.len, val.s.s);
		destroy_avp(avp);
		return -1;
	    }
	    
	    if (append_branch(msg, &uri, &dst, &path, 0, flags, sock) != 1) {
		LM_ERR("Appending branch failed\n");
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
	    LM_ERR("Setting of fr_inv_timer_avp failed\n");
	    return -1;
	}
	
    } else {
	/* unsupported route type */
	LM_ERR("Unsupported route type\n");
	return -1;
    }

    return 1;
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
	    LM_ERR("No param <%d>!\n", param_count);
	    return -1;
	}
	
	if(pv_parse_format(&s,&model)<0 || model==NULL) {
	    LM_ERR("Wrong format <%s> for param <%d>!\n", s.s, param_count);
	    return -1;
	}
	if(model->spec.getf==NULL) {
	    if(param_count==1) {
		if(str2int(&s, (unsigned int*)&model->spec.pvp.pvn.u.isname.name.n)!=0) {
		    LM_ERR("Wrong value <%s> for param <%d>!\n",
			   s.s, param_count);
		    return -1;
		}
	    }
	}
	*param = (void*)model;
    }

    return 0;
}
