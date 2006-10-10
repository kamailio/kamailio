/*
 * $Id$
 *
 * Least Cost Routing module (also implements sequential forking)
 *
 * Copyright (C) 2005 Juha Heinanen
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
#include "../../mi/mi.h"
#include "../mysql/dbase.h"
#include "fifo.h"
#include "mi.h"

MODULE_VERSION

/*
 * Version of gw and lcr tables required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define GW_TABLE_VERSION 3
#define LCR_TABLE_VERSION 2

/* usr_avp flag for sequential forking */
#define Q_FLAG      (1<<2)

static void destroy(void);       /* Module destroy function */
static int child_init(int rank); /* Per-child initialization function */
static int mi_child_init(void);
static int mod_init(void);       /* Module initialization function */
static int fixstring2int(void **param, int param_count);

int reload_gws ( void );

#define GW_TABLE "gw"

#define GW_NAME_COL "gw_name"

#define IP_ADDR_COL "ip_addr"

#define PORT_COL "port"

#define URI_SCHEME_COL "uri_scheme"

#define TRANSPORT_COL "transport"

#define GRP_ID_COL "grp_id"

#define LCR_TABLE "lcr"

#define STRIP_COL "strip"

#define PREFIX_COL "prefix"

#define FROM_URI_COL "from_uri"

#define PRIORITY_COL "priority"

#define MAX_QUERY_SIZE 512
#define MAX_NO_OF_GWS 32
#define MAX_NO_OF_LCRS 256
#define MAX_PREFIX_LEN 16
#define MAX_FROM_URI_LEN 128

/* Default avp names */
#define DEF_GW_URI_AVP "1400"
#define DEF_CONTACT_AVP "1401"
#define DEF_RURI_USER_AVP "1402"
#define DEF_FR_INV_TIMER_AVP "fr_inv_timer_avp"
#define DEF_FR_INV_TIMER 90
#define DEF_FR_INV_TIMER_NEXT 30
#define DEF_RPID_AVP "rpid"
#define DEF_DB_MODE 1

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
    char prefix[MAX_PREFIX_LEN];
    unsigned short prefix_len;
};

struct lcr_info {
    char prefix[MAX_PREFIX_LEN];
    unsigned short prefix_len;
    char from_uri[MAX_FROM_URI_LEN + 1];
    unsigned short from_uri_len;
    unsigned int grp_id;
    unsigned short priority;
    unsigned short end_record;
};

struct from_uri_regex {
    regex_t re;
    short int valid;
};

struct mi {
    unsigned int gw_index;
    unsigned int route_index;
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
static str db_url    = str_init(DEFAULT_RODB_URL);
str gw_table         = str_init(GW_TABLE);
str gw_name_col      = str_init(GW_NAME_COL);
str ip_addr_col      = str_init(IP_ADDR_COL);
str port_col         = str_init(PORT_COL);
str uri_scheme_col   = str_init(URI_SCHEME_COL);
str transport_col    = str_init(TRANSPORT_COL);
str grp_id_col       = str_init(GRP_ID_COL);
str lcr_table        = str_init(LCR_TABLE);
str strip_col        = str_init(STRIP_COL);
str prefix_col       = str_init(PREFIX_COL);
str from_uri_col     = str_init(FROM_URI_COL);
str priority_col     = str_init(PRIORITY_COL);

str gw_uri_avp       = str_init(DEF_GW_URI_AVP);
str ruri_user_avp    = str_init(DEF_RURI_USER_AVP);
str contact_avp      = str_init(DEF_CONTACT_AVP);
str inv_timer_avp    = str_init(DEF_FR_INV_TIMER_AVP);
int inv_timer        = DEF_FR_INV_TIMER;
int inv_timer_next   = DEF_FR_INV_TIMER_NEXT;
str rpid_avp         = str_init(DEF_RPID_AVP);
int db_mode          = DEF_DB_MODE;

/*
 * Other module types and variables
 */

struct contact {
    str uri;
    qvalue_t q;
    unsigned short q_flag;
    struct contact *next;
};

int_str gw_uri_name, ruri_user_name, contact_name, rpid_name, inv_timer_name;
unsigned short gw_uri_avp_name_str;
unsigned short ruri_user_avp_name_str;
unsigned short contact_avp_name_str;
unsigned short rpid_avp_name_str;

struct gw_info **gws;	/* Pointer to current gw table pointer */
struct gw_info *gws_1;	/* Pointer to gw table 1 */
struct gw_info *gws_2;	/* Pointer to gw table 2 */

struct lcr_info **lcrs;  /* Pointer to current lcr table pointer */
struct lcr_info *lcrs_1; /* Pointer to lcr table 1 */
struct lcr_info *lcrs_2; /* Pointer to lcr table 2 */

unsigned int *lcrs_ws_reload_counter;
unsigned int reload_counter;

struct from_uri_regex from_uri_reg[MAX_NO_OF_LCRS];

/*
 * Module functions that are defined later
 */
int load_gws(struct sip_msg* _m, char* _s1, char* _s2);
int load_gws_grp(struct sip_msg* _m, char* _s1, char* _s2);
int next_gw(struct sip_msg* _m, char* _s1, char* _s2);
int from_gw(struct sip_msg* _m, char* _s1, char* _s2);
int from_gw_grp(struct sip_msg* _m, char* _s1, char* _s2);
int to_gw(struct sip_msg* _m, char* _s1, char* _s2);
int to_gw_grp(struct sip_msg* _m, char* _s1, char* _s2);
int load_contacts (struct sip_msg*, char*, char*);
int next_contacts (struct sip_msg*, char*, char*);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"load_gws",      load_gws,      0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{"load_gws",      load_gws_grp,  1, fixstring2int,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{"next_gw",       next_gw,       0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{"from_gw",       from_gw,       0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
	{"from_gw",       from_gw_grp,   1, fixstring2int,
		REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
	{"to_gw",         to_gw,         0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{"to_gw",         to_gw_grp,     1, fixstring2int,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{"load_contacts", load_contacts, 0, 0,
		REQUEST_ROUTE},
	{"next_contacts", next_contacts, 0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",                   STR_PARAM, &db_url.s       },
	{"gw_table",                 STR_PARAM, &gw_table.s     },
	{"gw_name_column",           STR_PARAM, &gw_name_col.s  },
	{"ip_addr_column",           STR_PARAM, &ip_addr_col.s  },
	{"port_column",              STR_PARAM, &port_col.s     },
	{"uri_scheme_column",        STR_PARAM, &uri_scheme_col.s },
	{"transport_column",         STR_PARAM, &transport_col.s },
	{"grp_id_column",            STR_PARAM, &grp_id_col.s   },
	{"lcr_table",                STR_PARAM, &lcr_table.s    },
	{"strip_column",             STR_PARAM, &strip_col.s    },
	{"prefix_column",            STR_PARAM, &prefix_col.s   },
	{"from_uri_column",          STR_PARAM, &from_uri_col.s },
	{"priority_column",          STR_PARAM, &priority_col.s },
	{"gw_uri_avp",               STR_PARAM, &gw_uri_avp.s   },
	{"ruri_user_avp",            STR_PARAM, &ruri_user_avp.s },
	{"contact_avp",              STR_PARAM, &contact_avp.s  },
	{"fr_inv_timer_avp",         STR_PARAM, &inv_timer_avp.s },
	{"fr_inv_timer",             INT_PARAM, &inv_timer      },
	{"fr_inv_timer_next",        INT_PARAM, &inv_timer_next },
	{"rpid_avp",                 STR_PARAM, &rpid_avp.s     },
	{"db_mode",                  INT_PARAM, &db_mode        },
	{0, 0, 0}
};


/*
 * Exported MI functions
 */
static mi_export_t mi_cmds[] = {
	{ MI_LCR_RELOAD,  mi_lcr_reload,   0,  mi_child_init },
	{ MI_LCR_DUMP,    mi_lcr_dump,     0,  0 },
	{ 0, 0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"lcr", 
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	0,         /* exported statistics */
	mi_cmds,   /* exported MI functions */
	mod_init,  /* module initialization function */
	0,         /* response function */
	destroy,   /* destroy function */
	child_init /* child initialization function */
};


int lcr_db_init(char* db_url)
{
	if (lcr_dbf.init==0){
		LOG(L_CRIT, "BUG: lcr_db_bind: null dbf\n");
		goto error;
	}
	db_handle=lcr_dbf.init(db_url);
	if (db_handle==0){
		LOG(L_ERR, "ERROR: lcr_db_bind: unable to connect to the database\n");
		goto error;
	}
	return 0;
error:
	return -1;
}



int lcr_db_bind(char* db_url)
{
	if (bind_dbmod(db_url, &lcr_dbf)<0){
		LOG(L_ERR, "ERROR: lcr_db_bind: unable to bind to the database"
				" module\n");
		return -1;
	}

	if (!DB_CAPABILITY(lcr_dbf, DB_CAP_QUERY)) {
		LOG(L_ERR, "ERROR: lcr_db_bind: Database module does not "
		    "implement 'query' function\n");
		return -1;
	}

	if (!DB_CAPABILITY(lcr_dbf, DB_CAP_RAW_QUERY)) {
	    LOG(L_ERR, "ERROR: lcr_db_bind: Database module does not "
		"implement raw 'query' function\n");
	    return -1;
	}

	return 0;
}


void lcr_db_close()
{
	if (db_handle && lcr_dbf.close){
		lcr_dbf.close(db_handle);
		db_handle=0;
	}
}


int lcr_db_ver(char* db_url, str* name)
{
	db_con_t* dbh;
	int ver;

	if (lcr_dbf.init==0){
		LOG(L_CRIT, "BUG: lcr_db_ver: unbound database\n");
		return -1;
	}
	dbh=lcr_dbf.init(db_url);
	if (dbh==0){
		LOG(L_ERR, "ERROR: lcr_db_ver: unable to open database connection\n");
		return -1;
	}
	ver=table_version(&lcr_dbf, dbh, name);
	lcr_dbf.close(dbh);
	return ver;
}


/*
 * Module initialization function callee in each child separately
 */
static int child_init(int rank)
{
	/* don't do anything for non-worker process */
	if (rank<1 && rank!=PROC_FIFO)
		return 0;

	if (lcr_db_init(db_url.s) < 0) {
		LOG(L_ERR, "ERROR: lcr:child_init():"
		    " Unable to connect to the database\n");
		return -1;
	}

	return 0;
}


static int mi_child_init()
{
	return lcr_db_init(db_url.s);
}


/*
 * Module initialization function that is called before the main process forks
 */
static int mod_init(void)
{
	int ver, i;
	unsigned int par;

	DBG("lcr - initializing\n");

	/* Bind database */
	if (lcr_db_bind(db_url.s)) {
		LOG(L_ERR, "ERROR: lcr:mod_init(): No database module found\n");
		return -1;
	}

	/* Update length of module variables */
	db_url.len = strlen(db_url.s);
	gw_table.len = strlen(gw_table.s);
	gw_name_col.len = strlen(gw_name_col.s);
	ip_addr_col.len = strlen(ip_addr_col.s);
	port_col.len = strlen(port_col.s);
	uri_scheme_col.len = strlen(uri_scheme_col.s);
	transport_col.len = strlen(transport_col.s);
	grp_id_col.len = strlen(grp_id_col.s);
	lcr_table.len = strlen(lcr_table.s);
	strip_col.len = strlen(strip_col.s);
	prefix_col.len = strlen(prefix_col.s);
	from_uri_col.len = strlen(from_uri_col.s);
	priority_col.len = strlen(priority_col.s);
	gw_uri_avp.len = strlen(gw_uri_avp.s);
	ruri_user_avp.len = strlen(ruri_user_avp.s);
	contact_avp.len = strlen(contact_avp.s);
	inv_timer_avp.len = strlen(inv_timer_avp.s);
	rpid_avp.len = strlen(rpid_avp.s);

	/* Check table version */
	ver = lcr_db_ver(db_url.s, &gw_table);
	if (ver < 0) {
		LOG(L_ERR, "ERROR: lcr:mod_init():"
				" Error while querying table version\n");
		goto err;
	} else if (ver < GW_TABLE_VERSION) {
		LOG(L_ERR, "ERROR: lcr:mod_init(): Invalid table version"
				" of gw table\n");
		goto err;
	}		

	/* Check table version */
	ver = lcr_db_ver(db_url.s, &lcr_table);
	if (ver < 0) {
		LOG(L_ERR, "ERROR: lcr:mod_init():"
				" Error while querying table version\n");
		goto err;
	} else if (ver < LCR_TABLE_VERSION) {
		LOG(L_ERR, "ERROR: lcr:mod_init(): Invalid table version of"
				" lcr table (use openser_mysql.sh reinstall)\n");
		goto err;
	}		

	/* Initialize fifo interface */
	(void)init_lcr_fifo();

	/* Initializing gw tables and gw table pointer variable */
	gws_1 = (struct gw_info *)shm_malloc(sizeof(struct gw_info) * (MAX_NO_OF_GWS + 1));
	if (gws_1 == 0) {
	    LOG(L_ERR, "ERROR: lcr: mod_init(): "
		"No memory for gw table\n");
	    goto err;
	}
	gws_2 = (struct gw_info *)shm_malloc(sizeof(struct gw_info) * (MAX_NO_OF_GWS + 1));
	if (gws_2 == 0) {
	    LOG(L_ERR, "ERROR: lcr: mod_init(): "
		"No memory for gw table\n");
	    goto err;
	}
	for (i = 0; i < MAX_NO_OF_GWS + 1; i++) {
		gws_1[i].ip_addr = gws_2[i].ip_addr = 0;
	}
	gws = (struct gw_info **)shm_malloc(sizeof(struct gw_info *));
	if (gws == 0) {
	    LOG(L_ERR, "ERROR: lcr: mod_init(): "
		"No memory for gw table pointer\n");
	}
	*gws = gws_1;

	/* Initializing lcr tables and lcr table pointer variable */
	lcrs_1 = (struct lcr_info *)shm_malloc(sizeof(struct lcr_info) *
			(MAX_NO_OF_LCRS + 1));
	if (lcrs_1 == 0) {
		LOG(L_ERR, "ERROR: lcr: mod_init(): "
			"No memory for lcr table\n");
		goto err;
	}
	lcrs_2 = (struct lcr_info *)shm_malloc(sizeof(struct lcr_info) *
			(MAX_NO_OF_LCRS + 1));
	if (lcrs_2 == 0) {
		LOG(L_ERR, "ERROR: lcr: mod_init(): "
			"No memory for lcr table\n");
		goto err;
	}
	for (i = 0; i < MAX_NO_OF_LCRS + 1; i++) {
		lcrs_1[i].end_record = lcrs_2[i].end_record = 0;
	}
	lcrs = (struct lcr_info **)shm_malloc(sizeof(struct lcr_info *));
	if (lcrs == 0) {
		LOG(L_ERR, "ERROR: lcr: mod_init(): "
			"No memory for lcr table pointer\n");
		goto err;
	}
	*lcrs = lcrs_1;

	lcrs_ws_reload_counter = (unsigned int *)shm_malloc(sizeof(unsigned int));
	if (lcrs_ws_reload_counter == 0) {
		LOG(L_ERR, "ERROR: lcr: mod_init(): "
			"No memory for counter\n");
		goto err;
	}
	*lcrs_ws_reload_counter = reload_counter = 0;

	memset(from_uri_reg, 0, sizeof(struct from_uri_regex) * MAX_NO_OF_LCRS);

	/* First reload */
	if (reload_gws() == -1) {
		LOG(L_CRIT, "ERROR: lcr:mod_init():"
		    " failed to reload gateways and routes\n");
		goto err;
	}

	/* Assign parameter names */
	if (str2int(&gw_uri_avp, &par) == 0) {
	    gw_uri_name.n = par;
	    gw_uri_avp_name_str = 0;
	} else {
	    gw_uri_name.s = gw_uri_avp;
	    gw_uri_avp_name_str = AVP_NAME_STR;
	}
	if (str2int(&ruri_user_avp, &par) == 0) {
	    ruri_user_name.n = par;
	    ruri_user_avp_name_str = 0;
	} else {
	    ruri_user_name.s = ruri_user_avp;
	    ruri_user_avp_name_str = AVP_NAME_STR;
	}
	if (str2int(&contact_avp, &par) == 0) {
	    contact_name.n = par;
	    contact_avp_name_str = 0;
	} else {
	    contact_name.s = contact_avp;
	    contact_avp_name_str = AVP_NAME_STR;
	}
	if (str2int(&rpid_avp, &par) == 0) {
	    rpid_name.n = par;
	    rpid_avp_name_str = 0;
	} else {
	    rpid_name.s = rpid_avp;
	    rpid_avp_name_str = AVP_NAME_STR;
	}
	inv_timer_name.s = inv_timer_avp;

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

	/* Sort by prefix. */
	if (lcr_record1.prefix_len > lcr_record2.prefix_len) {
		result = 1;
	}
	else if (lcr_record1.prefix_len == lcr_record2.prefix_len) {
		/* Sort by priority. */
		if (lcr_record1.priority < lcr_record2.priority) {
			result = 1;
		}
		else if (lcr_record1.priority == lcr_record2.priority) {
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
 * regcomp each from_uri.
 */
int load_from_uri_regex()
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
			LOG(L_ERR, "ERROR:lcr:load_regex: bad from_uri re %s\n", 
					(*lcrs)[i].from_uri);
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


/*
 * Reload gws to unused gw table and lcrs to unused lcr table, and, when done
 * make unused gw and lcr table the one in use.
 */
int reload_gws ( void )
{
    int i;
    unsigned int ip_addr, port, strip, prefix_len, from_uri_len, grp_id, priority;
    uri_type scheme;
    uri_transport transport;
    db_con_t* dbh;
    char *prefix, *from_uri;
    db_res_t* res = NULL;
    db_row_t* row;
    db_key_t gw_cols[7];
    db_key_t lcr_cols[4];

    gw_cols[0] = ip_addr_col.s;
    gw_cols[1] = port_col.s;
    gw_cols[2] = uri_scheme_col.s;
    gw_cols[3] = transport_col.s;
    gw_cols[4] = strip_col.s;
    gw_cols[5] = prefix_col.s;
    /* FIXME: is this ok if we have different names for grp_id
       in the two tables? (ge vw lcr) */
    gw_cols[6] = grp_id_col.s;

    lcr_cols[0] = prefix_col.s;
    lcr_cols[1] = from_uri_col.s;
    /* FIXME: is this ok if we have different names for grp_id
       in the two tables? (ge vw lcr) */
    lcr_cols[2] = grp_id_col.s;
    lcr_cols[3] = priority_col.s;

    if (lcr_dbf.init==0){
	    LOG(L_CRIT, "ERROR: lcr_db_ver: unbound database\n");
	    return -1;
    }
    dbh=lcr_dbf.init(db_url.s);
    if (dbh==0){
	    LOG(L_ERR, "ERROR: reload_gws: unable to open database connection\n");
	    return -1;
    }

    if (lcr_dbf.use_table(dbh, gw_table.s) < 0) {
	    LOG(L_ERR, "lcr_reload_gws(): Error while trying to use gw table\n");
	    return -1;
    }

    if (lcr_dbf.query(dbh, NULL, 0, NULL, gw_cols, 0, 7, 0, &res) < 0) {
	    LOG(L_ERR, "lcr_reload_gws(): Failed to query gw data\n");
	    lcr_dbf.close(dbh);
	    return -1;
    }

    if (RES_ROW_N(res) + 1 > MAX_NO_OF_GWS) {
	    LOG(L_ERR, "reload_gws(): Too many gateways\n");
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
    }
    
    for (i = 0; i < RES_ROW_N(res); i++) {
	row = RES_ROWS(res) + i;
	if (VAL_NULL(ROW_VALUES(row)) == 1) {
		LOG(L_ERR, "reload_gws(): IP address of GW is NULL\n");
		lcr_dbf.free_result(dbh, res);
		lcr_dbf.close(dbh);
		return -1;
	}
      	ip_addr = (unsigned int)VAL_INT(ROW_VALUES(row));
	if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
		port = 0;
	} else {
		port = (unsigned int)VAL_INT(ROW_VALUES(row) + 1);
	}
	if (port > 65536) {
	    LOG(L_ERR, "reload_gws(): Port of GW is too large: %u\n", port);
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
	}
	if (VAL_NULL(ROW_VALUES(row) + 2) == 1) {
	    scheme = SIP_URI_T;
	} else {
	    scheme = (uri_type)VAL_INT(ROW_VALUES(row) + 2);
	    if ((scheme != SIP_URI_T) && (scheme != SIPS_URI_T)) {
		LOG(L_ERR, "reload_gws(): Unknown or unsupported URI scheme: %u\n", (unsigned int)scheme);
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
		(transport != PROTO_TLS)) {
		LOG(L_ERR, "reload_gws(): Unknown or unsupported transport: %u\n", (unsigned int)transport);
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
	    prefix_len = 0;
	    prefix = (char *)0;
	} else {
	    prefix = (char *)VAL_STRING(ROW_VALUES(row) + 5);
	    prefix_len = strlen(prefix);
	    if (prefix_len > MAX_PREFIX_LEN) {
		LOG(L_ERR, "reload_gws(): too long gw prefix\n");
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
	if (*gws == gws_1) {
		gws_2[i].ip_addr = ip_addr;
		gws_2[i].port = port;
		gws_2[i].grp_id = grp_id;
		gws_2[i].scheme = scheme;
		gws_2[i].transport = transport;
		gws_2[i].strip = strip;
		gws_2[i].prefix_len = prefix_len;
		if (prefix_len)
		    memcpy(&(gws_2[i].prefix[0]), prefix, prefix_len);
	} else {
		gws_1[i].ip_addr = ip_addr;
		gws_1[i].port = port;
		gws_1[i].grp_id = grp_id;
		gws_1[i].scheme = scheme;
		gws_1[i].transport = transport;
		gws_1[i].strip = strip;
		gws_1[i].prefix_len = prefix_len;
		if (prefix_len)
		    memcpy(&(gws_1[i].prefix[0]), prefix, prefix_len);
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


    if (lcr_dbf.use_table(dbh, lcr_table.s) < 0) {
	    LOG(L_ERR, "lcr_reload_gws(): Error while trying to use lcr table\n");
	    return -1;
    }

    if (lcr_dbf.query(dbh, NULL, 0, NULL, lcr_cols, 0, 4, 0, &res) < 0) {
	    LOG(L_ERR, "lcr_reload_gws(): Failed to query lcr data\n");
	    lcr_dbf.close(dbh);
	    return -1;
    }

    if (RES_ROW_N(res) + 1 > MAX_NO_OF_LCRS) {
	    LOG(L_ERR, "reload_gws(): Too many lcr entries\n");
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
    }
    for (i = 0; i < RES_ROW_N(res); i++) {
	row = RES_ROWS(res) + i;
	if (VAL_NULL(ROW_VALUES(row)) == 1) {
	    prefix_len = 0;
	    prefix = 0;
	}
	else {
	    prefix = (char *)VAL_STRING(ROW_VALUES(row));
	    prefix_len = strlen(prefix);
	    if (prefix_len > MAX_PREFIX_LEN) {
	      LOG(L_ERR, "reload_gws(): too long lcr prefix\n");
	      lcr_dbf.free_result(dbh, res);
	      lcr_dbf.close(dbh);
	      return -1;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
	    from_uri_len = 0;
		from_uri = 0;
	}
	else {
	    from_uri = (char *)VAL_STRING(ROW_VALUES(row) + 1);
	    from_uri_len = strlen(from_uri);
	    if (from_uri_len > MAX_FROM_URI_LEN) {
		LOG(L_ERR, "reload_gws(): too long from_uri\n");
		lcr_dbf.free_result(dbh, res);
		lcr_dbf.close(dbh);
		return -1;
	    }
	}
	if (VAL_NULL(ROW_VALUES(row) + 2) == 1) {
	    LOG(L_ERR, "reload_gws(): route grp_id is NULL\n");
	    lcr_dbf.free_result(dbh, res);
	    lcr_dbf.close(dbh);
	    return -1;
	}
	grp_id = (unsigned int)VAL_INT(ROW_VALUES(row) + 2);
	if (VAL_NULL(ROW_VALUES(row) + 3) == 1) {
	    LOG(L_ERR, "reload_gws(): route priority is NULL\n");
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
    if (0 != load_from_uri_regex()) {
	return -1;
    }

    return 1;
}


/* Print gateways stored in current gw table */
void print_gws (FILE *reply_file)
{
        unsigned int i, prefix_len;
	uri_transport transport;

	for (i = 0; i < MAX_NO_OF_GWS; i++) {
		if ((*gws)[i].ip_addr == 0) {
			break;
		}
		fprintf(reply_file, "%d => ", i);
		fprintf(reply_file, "%d:", (*gws)[i].grp_id);
		if ((*gws)[i].scheme == SIP_URI_T) {
		    fprintf(reply_file, "sip:");
		} else {
		    fprintf(reply_file, "sips:");
		}
		if ((*gws)[i].port == 0) {
			fprintf(reply_file, "%d.%d.%d.%d",
				((*gws)[i].ip_addr << 24) >> 24,
				(((*gws)[i].ip_addr >> 8) << 24) >> 24,
				(((*gws)[i].ip_addr >> 16) << 24) >> 24,
				(*gws)[i].ip_addr >> 24);
		} else {
			fprintf(reply_file, "%d.%d.%d.%d:%d",
				((*gws)[i].ip_addr << 24) >> 24,
				(((*gws)[i].ip_addr >> 8) << 24) >> 24,
				(((*gws)[i].ip_addr >> 16) << 24) >> 24,
				(*gws)[i].ip_addr >> 24,
				(*gws)[i].port);
		}
                transport = (*gws)[i].transport;
                if (transport == PROTO_UDP) {
		    fprintf(reply_file, ":udp");
                } else  if (transport == PROTO_TCP) {
		    fprintf(reply_file, ":tcp");
                } else  if (transport == PROTO_TLS) {
		    fprintf(reply_file, ":tls");
		} else {
		    fprintf(reply_file, ":");
		}
		fprintf(reply_file, ":%d", (*gws)[i].strip);
		prefix_len = (*gws)[i].prefix_len;
		if (prefix_len) {
			fprintf(reply_file, ":%.*s\n",
				(int)prefix_len, (*gws)[i].prefix);
		} else {
		    fprintf(reply_file, ":\n");
		}
	}
	for (i = 0; i < MAX_NO_OF_LCRS; i++) {
	    if ((*lcrs)[i].end_record != 0) {
		break;
	    }
	    fprintf(reply_file, "%d => ", i);
	    fprintf(reply_file, "%.*s",	(*lcrs)[i].prefix_len, (*lcrs)[i].prefix);
	    fprintf(reply_file, ":%.*s", (*lcrs)[i].from_uri_len, (*lcrs)[i].from_uri);
	    fprintf(reply_file, ":%u", (*lcrs)[i].grp_id);
	    fprintf(reply_file, ":%u\n", (*lcrs)[i].priority);
	}
}


int mi_print_gws (struct mi_node* rpl)
{
	unsigned int i;
	struct mi_attr* attr;
	uri_transport transport;
	char *transp;
	struct mi_node* node;
	char* p;
	int len;

	for (i = 0; i < MAX_NO_OF_GWS; i++) {

		if ((*gws)[i].ip_addr == 0) 
			break;

		node= addf_mi_node_child(rpl,0 ,"GW", 2, 0, 0);
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
		else
			transp= "";

		attr= addf_mi_attr(node,0 ,"URI", 3,"%s:%d.%d.%d.%d:%d%s",
				((*gws)[i].scheme == SIP_URI_T)?"sip":"sips",
				((*gws)[i].ip_addr << 24) >> 24,
				(((*gws)[i].ip_addr >> 8) << 24) >> 24,
				(((*gws)[i].ip_addr >> 16) << 24) >> 24,
				(*gws)[i].ip_addr >> 24,
				((*gws)[i].port == 0)?5060:(*gws)[i].port,transp
				);
		if(attr == NULL)
			return -1;

		p = int2str((unsigned long)(*gws)[i].prefix, &len );
		attr = add_mi_attr(node, MI_DUP_VALUE, "PREFIX", 6, p, len );
		if(attr == NULL)
			return -1;
	}

	for (i = 0; i < MAX_NO_OF_LCRS; i++) {
		if ((*lcrs)[i].end_record != 0)
			break;

		node= addf_mi_node_child(rpl, 0, "RULE", 4, 0, 0);
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
static int do_load_gws(struct sip_msg* _m, int grp_id)
{
    db_res_t* res = NULL;
    db_row_t *row, *r;
    unsigned int q_len;

    str ruri_user, from_uri, value;
    char from_uri_str[MAX_FROM_URI_LEN + 1];
    char query[MAX_QUERY_SIZE];
    char ruri[MAX_URI_SIZE];
    unsigned int i, j, k, index;
    unsigned int addr, port;
    unsigned int strip, gw_index, duplicated_gw;
    uri_type scheme;
    uri_transport transport;
    struct ip_addr address;
    str addr_str, port_str;
    char *at, *prefix;
    int_str val;
    struct mi matched_gws[MAX_NO_OF_GWS + 1];
    unsigned short strip_len, prefix_len, priority;
    int randomizer_start, randomizer_end, randomizer_flag;
    struct lcr_info lcr_rec;

    /* Find Request-URI user */
    if (parse_sip_msg_uri(_m) < 0) {
	    LOG(L_ERR, "load_gws(): Error while parsing R-URI\n");
	    return -1;
    }
    ruri_user = _m->parsed_uri.user;

   /* Look for Caller RPID or From URI */
    if (search_first_avp(rpid_avp_name_str, rpid_name, &val, 0) &&
	val.s.s && val.s.len) {
	/* Get URI user from RPID */
	from_uri.len = val.s.len;
	from_uri.s = val.s.s;
    } else {
	/* Get URI from From URI */
	if ((!_m->from) && (parse_headers(_m, HDR_FROM_F, 0) == -1)) {
	    LOG(L_ERR, "load_gws(): Error while parsing message\n");
	    return -1;
	}
	if (!_m->from) {
	    LOG(L_ERR, "load_gws(): FROM header field not found\n");
	    return -1;
	}
	if ((!(_m->from)->parsed) && (parse_from_header(_m) < 0)) {
	    LOG(L_ERR, "load_gws(): Error while parsing From body\n");
	    return -1;
	}
	from_uri = get_from(_m)->uri;
    }
    if (from_uri.len < MAX_FROM_URI_LEN) {
	strncpy(from_uri_str, from_uri.s, from_uri.len);
	from_uri_str[from_uri.len] = '\0';
    } else {
	LOG(L_ERR, "load_gws(): from_uri to large\n");
	return -1;
    }

    /*
     * Check if the gws and lcrs were reloaded
     */
    if (reload_counter != *lcrs_ws_reload_counter) {
	if (load_from_uri_regex() != 0) {
	    return -1;
	}
    }

	if (db_mode == 0) {
		if(grp_id >= 0) {
			q_len = snprintf(query, MAX_QUERY_SIZE, "SELECT %.*s.%.*s, %.*s.%.*s, %.*s.%.*s, %.*s.%.*s, %.*s.%.*s, %.*s.%.*s FROM %.*s, %.*s WHERE %.*s.%.*s = %d AND '%.*s' LIKE %.*s.%.*s AND '%.*s' LIKE CONCAT(%.*s.%.*s, '%%') AND %.*s.%.*s = %.*s.%.*s ORDER BY CHAR_LENGTH(%.*s.%.*s), %.*s.%.*s DESC, RAND()",
				gw_table.len, gw_table.s, ip_addr_col.len, ip_addr_col.s,
				gw_table.len, gw_table.s, port_col.len, port_col.s,
				gw_table.len, gw_table.s, uri_scheme_col.len, uri_scheme_col.s,
				gw_table.len, gw_table.s, transport_col.len, transport_col.s,
				gw_table.len, gw_table.s, strip_col.len, strip_col.s,
				gw_table.len, gw_table.s, prefix_col.len, prefix_col.s,
				gw_table.len, gw_table.s, lcr_table.len, lcr_table.s,
				lcr_table.len, lcr_table.s, grp_id_col.len, grp_id_col.s, grp_id,
				from_uri.len, from_uri.s,
				lcr_table.len, lcr_table.s, from_uri_col.len, from_uri_col.s,
				ruri_user.len, ruri_user.s,
				lcr_table.len, lcr_table.s, prefix_col.len, prefix_col.s,
				lcr_table.len, lcr_table.s, grp_id_col.len,  grp_id_col.s,
				gw_table.len, gw_table.s, grp_id_col.len, grp_id_col.s,
				lcr_table.len, lcr_table.s, prefix_col.len, prefix_col.s,
				lcr_table.len, lcr_table.s, priority_col.len, priority_col.s);
		} else {
			q_len = snprintf(query, MAX_QUERY_SIZE, "SELECT %.*s.%.*s, %.*s.%.*s, %.*s.%.*s, %.*s.%.*s, %.*s.%.*s, %.*s.%.*s FROM %.*s, %.*s WHERE '%.*s' LIKE %.*s.%.*s AND '%.*s' LIKE CONCAT(%.*s.%.*s, '%%') AND %.*s.%.*s = %.*s.%.*s ORDER BY CHAR_LENGTH(%.*s.%.*s), %.*s.%.*s DESC, RAND()",
				gw_table.len, gw_table.s, ip_addr_col.len, ip_addr_col.s,
				gw_table.len, gw_table.s, port_col.len, port_col.s,
				gw_table.len, gw_table.s, uri_scheme_col.len, uri_scheme_col.s,
				gw_table.len, gw_table.s, transport_col.len, transport_col.s,
				gw_table.len, gw_table.s, strip_col.len, strip_col.s,
				gw_table.len, gw_table.s, prefix_col.len, prefix_col.s,
				gw_table.len, gw_table.s, lcr_table.len, lcr_table.s,
				from_uri.len, from_uri.s,
				lcr_table.len, lcr_table.s, from_uri_col.len, from_uri_col.s,
				ruri_user.len, ruri_user.s,
				lcr_table.len, lcr_table.s, prefix_col.len, prefix_col.s,
				lcr_table.len, lcr_table.s, grp_id_col.len,  grp_id_col.s,
				gw_table.len, gw_table.s, grp_id_col.len, grp_id_col.s,
				lcr_table.len, lcr_table.s, prefix_col.len, prefix_col.s,
				lcr_table.len, lcr_table.s, priority_col.len, priority_col.s);
		}
		if (q_len >= MAX_QUERY_SIZE) {
			LOG(L_ERR, "load_gws(): Too long database query\n");
			return -1;
		}
		if (lcr_dbf.raw_query(db_handle, query, &res) < 0) {
			LOG(L_ERR, "load_gws(): Failed to query accept data\n");
			return -1;
		}

		for (i = 0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;
			if (VAL_NULL(ROW_VALUES(row)) == 1) {
				LOG(L_ERR, "load_gws(): Gateway IP address is NULL\n");
				goto skip1;
			}
			addr = (unsigned int)VAL_INT(ROW_VALUES(row));
			for (j = i + 1; j < RES_ROW_N(res); j++) {
				r = RES_ROWS(res) + j;
				if (addr == (unsigned int)VAL_INT(ROW_VALUES(r)))
					goto skip1;
			}
			if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
				port = 0;
			} else {
				port = (unsigned int)VAL_INT(ROW_VALUES(row) + 1);
			}
			if (VAL_NULL(ROW_VALUES(row) + 2) == 1) {
				scheme = SIP_URI_T;
			} else {
				scheme = (uri_type)VAL_INT(ROW_VALUES(row) + 2);
			}
			if (VAL_NULL(ROW_VALUES(row) + 3) == 1) {
				transport = PROTO_NONE;
			} else {
				transport = (uri_transport)VAL_INT(ROW_VALUES(row) + 3);
			}
			if (VAL_NULL(ROW_VALUES(row) + 4) == 1) {
				strip = 0;
				strip_len = 1;
			} else {
				strip = VAL_INT(ROW_VALUES(row) + 4);
				if (strip<10)
					strip_len = 1;
				else if (strip < 100)
					strip_len = 2;
				else
					strip_len = 3;
			}
			if (VAL_NULL(ROW_VALUES(row) + 5) == 1) {
				prefix_len = 0;
				prefix = (char *)0;
			} else {
				prefix = (char *)VAL_STRING(ROW_VALUES(row) + 5);
				prefix_len = strlen(prefix);
			}
			if (5 + prefix_len + 1 + strip_len + 1 + 15 + 1 + 5 + 1 + 14 >
			MAX_URI_SIZE) {
				LOG(L_ERR, "load_gws(): Request URI would be too long\n");
				goto skip1;
			}
			at = (char *)&(ruri[0]);
			if (scheme == SIP_URI_T) {
				memcpy(at, "sip:", 4); at = at + 4;
			} else if (scheme == SIPS_URI_T) {
				memcpy(at, "sips:", 5); at = at + 5;
			} else {
				LOG(L_ERR, "load_gws(): Unknown or unsupported URI "
					"scheme: %u\n", (unsigned int)scheme);
				goto skip1;
			}
			if (prefix_len) {
				memcpy(at, prefix, prefix_len);
				at = at + prefix_len;
			}
			/* Add strip in this form |number. For example: |3 means 
			 * strip first 3 characters */
			*at = '|'; at = at + 1;
			sprintf(at,"%d", strip);
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
					LOG(L_ERR, "load_gws(): Port of GW is too large: %u\n",
						port);
					goto skip1;
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
				} else {
					LOG(L_ERR, "load_gws(): Unknown or unsupported "
						"transport: %u\n", (unsigned int)transport);
					goto skip1;
				}
			}
			value.s = (char *)&(ruri[0]);
			value.len = at - value.s;
			val.s = value;
			add_avp(gw_uri_avp_name_str|AVP_VAL_STR, gw_uri_name, val);
			DBG("load_gws(): DEBUG: Added gw_uri_avp <%.*s>\n",
				value.len, value.s);
skip1:
			continue;
		} /* end for */

		lcr_dbf.free_result(db_handle, res);
		return 1;
	} else {
		/* CACHE MODE */
		/*
		 * Let's match the gws:
		 *  1. prefix matching
		 *  2. from_uri matching
		 *  3. grp_id matching
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
	if ((lcr_rec.prefix_len <= ruri_user.len) &&
	    (strncmp(lcr_rec.prefix, ruri_user.s, lcr_rec.prefix_len)==0)) {
	    /* 1. Prefix matching is done */
	    if ((lcr_rec.from_uri_len == 0) ||
		(from_uri_reg[i].valid && (regexec(&(from_uri_reg[i].re), from_uri_str, 0, (regmatch_t *)NULL, 0) == 0))) {
		/* 2. from_uri matching is done */
		for (j = 0; j < MAX_NO_OF_GWS; j++) {
		    if ((*gws)[j].ip_addr == 0) {
			break;
		    }
		    if (lcr_rec.grp_id == (*gws)[j].grp_id && (grp_id < 0 || (*gws)[j].grp_id == grp_id)) {
			/* 3. grp_id matching is done */
			for (k = 0; k < gw_index; k++) {
			    if ((*gws)[j].ip_addr ==
				(*gws)[matched_gws[k].gw_index].ip_addr) {
				/* Found the same gw in the list  */
				/* Let's keep the one with higher */
				/* match on prefix len            */
				DBG("DEBUG:lcr:load_gws: duplicate gw for index"
				    " %d [%d,%d] and current [%d,%d] \n",
				    k, matched_gws[k].route_index,
				    matched_gws[k].route_index, i, j);
				duplicated_gw = 1;
				if (lcr_rec.prefix_len >
				    (*lcrs)[matched_gws[k].route_index].prefix_len) {
				    /* Replace the old entry with the new one */
				    DBG("DEBUG:lcr:load_gws: replace[%d,%d]"
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
					DBG("DEBUG:lcr:load_gws: replace[%d,%d] with"
					    " [%d,%d] on index %d:"
					    " priority reason %d>%d\n",
					    matched_gws[k].route_index,
					    matched_gws[k].gw_index, i, j, k,
					    lcr_rec.priority,
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
			    DBG("DEBUG:lcr:load_gws: add matched_gws[%d]=[%d,%d]\n",
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
	strip = (*gws)[index].strip;
	if (strip<10) strip_len = 1;
	else
	    if (strip < 100)
		strip_len = 2;
	    else
		strip_len = 3;
	prefix_len = (*gws)[index].prefix_len;
	prefix = (*gws)[index].prefix;

	if (5 + prefix_len + 1 + strip_len + 1 + 15 + 1 + 5 + 1 + 14 >
	    MAX_URI_SIZE) {
	    LOG(L_ERR, "load_gws(): Request URI would be too long\n");
	    goto skip;
	}
	at = (char *)&(ruri[0]);
	if (scheme == SIP_URI_T) {
	    memcpy(at, "sip:", 4); at = at + 4;
	} else if (scheme == SIPS_URI_T) {
	    memcpy(at, "sips:", 5); at = at + 5;
	} else {
	    LOG(L_ERR, "load_gws(): Unknown or unsupported URI scheme: %u\n",
		(unsigned int)scheme);
	    goto skip;
	}
	if (prefix_len) {
	    memcpy(at, prefix, prefix_len); at = at + prefix_len;
	}
	//Add strip in this form |number. For example: |3 means strip first 3 characters
	*at = '|'; at = at + 1;
	sprintf(at,"%d", strip);
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
		LOG(L_ERR, "load_gws(): Port of GW is too large: %u\n", port);
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
	    } else {
		LOG(L_ERR, "load_gws(): Unknown or unsupported transport: %u\n",
		    (unsigned int)transport);
		goto skip;
	    }
	}
	value.s = (char *)&(ruri[0]);
	value.len = at - value.s;
	val.s = value;
	add_avp(gw_uri_avp_name_str|AVP_VAL_STR, gw_uri_name, val);
	DBG("load_gws(): DEBUG: Added gw_uri_avp <%.*s>\n",
	    value.len, value.s);
    skip:
	continue;
    }

    return 1;
    }
}

/*
 * Load info of matching GWs from database to gw_uri AVPs
 * taking into account the given group id.
 */
int load_gws_grp(struct sip_msg* _m, char* _s1, char* _s2)
{
    int grp_id;

    grp_id = (int)(long)_s1;
    return do_load_gws(_m, grp_id);
}

/*
 * Load info of matching GWs from database to gw_uri AVPs
 * ignoring the group id.
 */
int load_gws(struct sip_msg* _m, char* _s1, char* _s2)
{
    return do_load_gws(_m, -1);
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
int next_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
    int_str gw_uri_val, ruri_user_val, val;
    struct action act;
    int rval;
    struct usr_avp *gw_uri_avp, *ruri_user_avp;
    str new_ruri;
    char *at, *at_char;
    char *strip_char;
    unsigned int strip;

    gw_uri_avp = search_first_avp(gw_uri_avp_name_str,
				  gw_uri_name, &gw_uri_val, 0);
    if (!gw_uri_avp) return -1;

    if (route_type == REQUEST_ROUTE) {
	/* Create new Request-URI taking URI user from current Request-URI
	   and other parts of from gw_uri AVP. */
	if (parse_sip_msg_uri(_m) < 0) {
	    LOG(L_ERR, "next_gw(): Parsing of R-URI failed.\n");
	    return -1;
	}
	new_ruri.len = gw_uri_val.s.len + _m->parsed_uri.user.len + 1;
	new_ruri.s = pkg_malloc(new_ruri.len);
	if (!new_ruri.s) {
	    LOG(L_ERR, "next_gw(): No memory for new R-URI.\n");
	    return -1;
	}
	at_char = memchr(gw_uri_val.s.s, '@', gw_uri_val.s.len);
	if (!at_char) {
	    pkg_free(new_ruri.s);
	    LOG(L_ERR, "next_gw(): No @ in gateway URI.\n");
	    return -1;
	}
	strip_char = memchr(gw_uri_val.s.s, '|', gw_uri_val.s.len);
	if (!strip_char || strip_char > at_char) {
	    pkg_free(new_ruri.s);
	    LOG(L_ERR, "next_gw(): No strip character | "
		"before @ in gateway URI.\n");
	    return -1;
	}
	at = new_ruri.s;
	memcpy(at, gw_uri_val.s.s, strip_char - gw_uri_val.s.s);
	sscanf(strip_char+1,"%d",&strip);
	at = at + (strip_char - gw_uri_val.s.s);
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
	add_avp(ruri_user_avp_name_str|AVP_VAL_STR, ruri_user_name, val);
	DBG("load_gws(): DEBUG: Added ruri_user_avp <%.*s>\n",
	    val.s.len, val.s.s);
	/* Rewrite Request URI */
	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = new_ruri.s;
	rval = do_action(&act, _m);
	pkg_free(new_ruri.s);
	destroy_avp(gw_uri_avp);
	if (rval != 1) {
	    LOG(L_ERR, "next_gw(): ERROR: do_action failed with return "
		"value <%d>\n", rval);
	    return -1;
	}
	return 1;
    } else if (route_type == FAILURE_ROUTE) {
	/* Create new Request-URI taking URI user from ruri_user AVP
	   and other parts of from gateway URI AVP. */
	ruri_user_avp = search_first_avp(ruri_user_avp_name_str,
					 ruri_user_name, &ruri_user_val, 0);
	if (!ruri_user_avp) {
	    LOG(L_ERR, "next_gw(): No ruri_user AVP\n");
	    return -1;
	}
	new_ruri.len = gw_uri_val.s.len + ruri_user_val.s.len + 1;
	new_ruri.s = pkg_malloc(new_ruri.len);
	if (!new_ruri.s) {
	    LOG(L_ERR, "next_gw(): No memory for new R-URI.\n");
	    return -1;
	}
	at_char = memchr(gw_uri_val.s.s, '@', gw_uri_val.s.len);
	if (!at_char) {
	    pkg_free(new_ruri.s);
	    LOG(L_ERR, "next_gw(): No @ in gateway URI.\n");
	    return -1;
	}
	strip_char = memchr(gw_uri_val.s.s, '|', gw_uri_val.s.len);
	if (!strip_char || strip_char > at_char) {
	    pkg_free(new_ruri.s);
	    LOG(L_ERR, "next_gw(): No strip character | "
		"before @ in gateway URI.\n");
	    return -1;
	}
	at = new_ruri.s;
	memcpy(at, gw_uri_val.s.s, strip_char - gw_uri_val.s.s);
	sscanf(strip_char+1,"%d",&strip);
	at = at + (strip_char - gw_uri_val.s.s);
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
	*at = '\0';
	act.type = APPEND_BRANCH_T;
	act.p1_type = STRING_ST;
	act.p1.string = new_ruri.s;
	act.p2_type = NUMBER_ST;
	act.p2.number = 0;
	rval = do_action(&act, _m);
	pkg_free(new_ruri.s);
	destroy_avp(gw_uri_avp);
	if (rval != 1) {
	    LOG(L_ERR, "next_gw(): ERROR: do_action failed with return "
		"value <%d>\n", rval);
	    return -1;
	}
	return 1;
    }
    /* unsupported route type */
    return -1;
}


/*
 * Checks if request comes from a gateway
 */
static int do_from_gw(struct sip_msg* _m, int grp_id)
{
    int i;
    unsigned int src_addr;

    src_addr = _m->rcv.src_ip.u.addr32[0];

    for (i = 0; i < MAX_NO_OF_GWS; i++) {
	    if ((*gws)[i].ip_addr == 0) {
		    return -1;
	    }
	    if ((*gws)[i].ip_addr == src_addr && 
		    (grp_id < 0 || (*gws)[i].grp_id == grp_id)) {
		    return 1;
	    }
    }

    return -1;
}


/*
 * Checks if request comes from a gateway, taking
 * into account the group id.
 */
int from_gw_grp(struct sip_msg* _m, char* _s1, char* _s2)
{
	int grp_id;

	grp_id = (int)(long)_s1;
	return do_from_gw(_m, grp_id);
}

/*
 * Checks if request comes from a gateway, ignoring
 * the group id.
 */
int from_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
	return do_from_gw(_m, -1);
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
	LOG(L_ERR, "LCR: to_gw: ERROR while parsing the R-URI\n");
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
int to_gw_grp(struct sip_msg* _m, char* _s1, char* _s2)
{
	int grp_id;

	grp_id = (int)(long)_s1;
	return do_to_gw(_m, grp_id);
}


/*
 * Checks if in-dialog request goes to gateway, ignoring
 * the group id.
 */
int to_gw(struct sip_msg* _m, char* _s1, char* _s2)
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


/* 
 * Loads contacts in destination set into "lcr_contact" AVP in reverse
 * priority order and associated each contact with Q_FLAG telling if
 * contact is the last one in its priority class.  Finally, removes
 * all branches from destination set.
 */
int load_contacts(struct sip_msg* msg, char* key, char* value)
{
	str branch, *ruri;
	qvalue_t q, ruri_q;
	struct contact *contacts, *next, *prev, *curr;
	int_str val;
	int idx;

	/* Check if anything needs to be done */
	if (nr_branches == 0) {
	    DBG("load_contacts(): DEBUG: Nothing to do - no branches!\n");
	    return 1;
	}

	ruri = GET_RURI(msg);
	if (!ruri) {
	    LOG(L_ERR, "ERROR: load_contacts(): No Request-URI found\n");
	    return -1;
	}
	ruri_q = get_ruri_q();

	for( idx=0 ; (branch.s=get_branch(idx,&branch.len,&q,0,0,0,0))!=0 ; idx++ ) {
	    if (q != ruri_q) {
		goto rest;
	    }
	}
	DBG("load_contacts(): DEBUG: Nothing to do - all same q!\n");
	return 1;

rest:
	/* Insert Request-URI to contact list */
	contacts = (struct contact *)pkg_malloc(sizeof(struct contact));
	if (!contacts) {
	    LOG(L_ERR, "ERROR: load_contacts(): No memory for Request-URI\n");
	    return -1;
	}
	contacts->uri.s = ruri->s;
	contacts->uri.len = ruri->len;
	contacts->q = ruri_q;
	contacts->next = (struct contact *)0;

	/* Insert branch URIs to contact list in increasing q order */
	for( idx=0 ; (branch.s=get_branch(idx,&branch.len,&q,0,0,0,0))!=0 ; idx++ ) {
	    next = (struct contact *)pkg_malloc(sizeof(struct contact));
	    if (!next) {
		LOG(L_ERR, "ERROR: load_contacts(): No memory for branch URI\n");
		free_contact_list(contacts);
		return -1;
	    }
	    next->uri = branch;
	    next->q = q;
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
	    val.s = curr->uri;
	    add_avp(contact_avp_name_str|AVP_VAL_STR|(curr->q_flag),
		    contact_name, val);
	    DBG("load_contacts(): DEBUG: Loaded <%s>, q_flag <%d>\n",
		val.s.s, curr->q_flag);	    
	    curr = curr->next;
	}

	/* Clear all branches */
	clear_branches();

	/* Free contacts list */
	free_contact_list(contacts);

	return 1;
}


/*
 * Adds to request a destination set that includes all highest priority
 * class contacts in "lcr_contact" AVP.   If called from a route block,
 * rewrites the request uri with first contact and adds the remaining
 * contacts as branches.  If called from failure route block, adds all
 * contacts as brances.  Removes added contacts from "lcr_contact" AVP.
 */
int next_contacts(struct sip_msg* msg, char* key, char* value)
{
    struct usr_avp *avp, *prev;
    int_str val;
    struct action act;
    int rval;

	if ( route_type == REQUEST_ROUTE) {
		/* Find first lcr_contact_avp value */
		avp = search_first_avp(contact_avp_name_str, contact_name, &val, 0);
		if (!avp) {
			DBG("next_contacts(): DEBUG: No AVPs -- we are done!\n");
			return 1;
		}

		/* Set Request-URI */
		act.type = SET_URI_T;
		act.p1_type = STRING_ST;
		act.p1.string = val.s.s;
		rval = do_action(&act, msg);
		if (rval != 1) {
			destroy_avp(avp);
			return rval;
		}
		DBG("next_contacts(): DEBUG: R-URI is <%s>\n", val.s.s);
		if (avp->flags & Q_FLAG) {
			destroy_avp(avp);
			/* Set fr_inv_timer */
			val.n = inv_timer_next;
			if (add_avp(AVP_NAME_STR, inv_timer_name, val) != 0) {
				LOG(L_ERR, "next_contacts(): ERROR: setting of "
					"fr_inv_timer_avp failed\n");
				return -1;
			}
			return 1;
		}
		/* Append branches until out of branches or Q_FLAG is set */
		prev = avp;
		while ((avp = search_next_avp(avp, &val))) {
			destroy_avp(prev);
			act.type = APPEND_BRANCH_T;
			act.p1_type = STRING_ST;
			act.p1.string = val.s.s;
			act.p2_type = NUMBER_ST;
			act.p2.number = 0;
			rval = do_action(&act, msg);
			if (rval != 1) {
				destroy_avp(avp);
				LOG(L_ERR, "next_contacts(): ERROR: do_action failed "
					"with return value <%d>\n", rval);
				return -1;
			}
			DBG("next_contacts(): DEBUG: Branch is <%s>\n", val.s.s);
			if (avp->flags & Q_FLAG) {
				destroy_avp(avp);
				val.n = inv_timer_next;
				if (add_avp(AVP_NAME_STR, inv_timer_name, val) != 0) {
					LOG(L_ERR, "next_contacts(): ERROR: setting of "
						"fr_inv_timer_avp failed\n");
					return -1;
				}
				return 1;
			}
			prev = avp;
		}

	} else if ( route_type == FAILURE_ROUTE) {
		avp = search_first_avp(contact_avp_name_str, contact_name, &val, 0);
		if (!avp) return -1;

		prev = avp;
		do {
			act.type = APPEND_BRANCH_T;
			act.p1_type = STRING_ST;
			act.p1.string = val.s.s;
			act.p2_type = NUMBER_ST;
			act.p2.number = 0;
			rval = do_action(&act, msg);
			if (rval != 1) {
				destroy_avp(avp);
				return rval;
			}
			DBG("next_contacts(): DEBUG: New branch is <%s>\n", val.s.s);
			if (avp->flags & Q_FLAG) {
				destroy_avp(avp);
				return 1;
			}
			prev = avp;
			avp = search_next_avp(avp, &val);
			destroy_avp(prev);
		} while (avp);

		/* Restore fr_inv_timer */
		val.n = inv_timer;
		if (add_avp(AVP_NAME_STR, inv_timer_name, val) != 0) {
			LOG(L_ERR, "next_contacts(): ERROR: setting of "
				"fr_inv_timer_avp failed\n");
			return -1;
		}

	} else {
		/* unsupported rout type */
		return -1;
	}

	return 1;
}

/* 
 * Convert string parameter to integer for functions that expect an integer.
 * Taken from mediaproxy module.
 */
static int fixstring2int(void **param, int param_count)
{
	unsigned long number;
	int err;

	if (param_count == 1) {
		number = str2s(*param, strlen(*param), &err);
		if (err == 0) {
			pkg_free(*param);
			*param = (void*)number;
			return 0;
		} else {
			LOG(L_ERR, "lcr/fixstring2int(): ERROR: bad number `%s'\n",
				(char*)(*param));
			return E_CFG;
		}
	}
	return 0;
}

