/*
 * Least Cost Routing module (also implements sequential forking)
 *
 * Copyright (C) 2005 Juha Heinanen
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
 *  2005-02-14: Introduced lcr module (jh)
 *  2005-02-20: Added sequential forking functions (jh)
 *  2005-02-25: Added support for int AVP names, combined addr and port
 *              AVPs (jh)
 *  2005-07-23: Added support for gw URI scheme and transport (jh)
 *  2005-08-20: Added support for gw prefixes (jh)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
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
#include "../mysql/dbase.h"
#include "../../action.h"
#include "../../modules/tm/tm_load.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "../../ip_addr.h"
#include "lcr_rpc.h"
#include "lcr_mod.h"

MODULE_VERSION

/*
 * Version of gw and lcr tables required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define GW_TABLE_VERSION 3
#define LCR_TABLE_VERSION 1

/* usr_avp flag for sequential forking */
#define DEF_Q_FLAG	"q_flag"
avp_flags_t	Q_FLAG = 0;

static void destroy(void);       /* Module destroy function */
static int child_init(int rank); /* Per-child initialization function */
static int mod_init(void);       /* Module initialization function */

int reload_gws ( void );

#define GW_TABLE "gw"

#define GW_NAME_COL "gw_name"

#define IP_ADDR_COL "ip_addr"

#define PORT_COL "port"

#define URI_SCHEME_COL "uri_scheme"

#define TRANSPORT_COL "transport"

#define GRP_ID_COL "grp_id"

#define LCR_TABLE "lcr"

#define PREFIX_COL "prefix"

#define FROM_URI_COL "from_uri"

#define PRIORITY_COL "priority"

#define LCR_MAX_QUERY_SIZE 512
#define MAX_PREFIX_LEN 16

/* Default avp names */
#define DEF_GW_URI_AVP "1400"
#define DEF_CONTACT_AVP "1401"
#define DEF_FR_INV_TIMER_AVP "$t.callee_fr_inv_timer"
#define DEF_FR_INV_TIMER 90
#define DEF_FR_INV_TIMER_NEXT 30
#define DEF_RPID_AVP "rpid"

/*
 * Database variables
 */
static db_con_t* db_handle = 0;   /* Database connection handle */
static db_func_t lcr_dbf;

/*
 * Module parameter variables
 */
static str db_url    = STR_STATIC_INIT(DEFAULT_RODB_URL);
str gw_table         = STR_STATIC_INIT(GW_TABLE);
str gw_name_col      = STR_STATIC_INIT(GW_NAME_COL);
str ip_addr_col      = STR_STATIC_INIT(IP_ADDR_COL);
str port_col         = STR_STATIC_INIT(PORT_COL);
str uri_scheme_col   = STR_STATIC_INIT(URI_SCHEME_COL);
str transport_col    = STR_STATIC_INIT(TRANSPORT_COL);
str grp_id_col       = STR_STATIC_INIT(GRP_ID_COL);
str lcr_table        = STR_STATIC_INIT(LCR_TABLE);
str prefix_col       = STR_STATIC_INIT(PREFIX_COL);
str from_uri_col     = STR_STATIC_INIT(FROM_URI_COL);
str priority_col     = STR_STATIC_INIT(PRIORITY_COL);
str gw_uri_avp       = STR_STATIC_INIT(DEF_GW_URI_AVP);
str contact_avp      = STR_STATIC_INIT(DEF_CONTACT_AVP);
str inv_timer_avp    = STR_STATIC_INIT(DEF_FR_INV_TIMER_AVP);
int inv_timer        = DEF_FR_INV_TIMER;
int inv_timer_next   = DEF_FR_INV_TIMER_NEXT;
str inv_timer_ps     = STR_STATIC_INIT("");
str inv_timer_next_ps = STR_STATIC_INIT("");
str rpid_avp         = STR_STATIC_INIT(DEF_RPID_AVP);

/*
 * Other module types and variables
 */

struct contact {
    str uri;
    qvalue_t q;
    unsigned short q_flag;
    struct contact *next;
};

int_str gw_uri_name, contact_name, rpid_name;
unsigned short gw_uri_avp_name_str;
unsigned short contact_avp_name_str;
unsigned short rpid_avp_name_str;

static avp_ident_t tm_timer_param;	/* TM module's invite timer avp */

struct gw_info **gws;	/* Pointer to current gw table pointer */
struct gw_info *gws_1;	/* Pointer to gw table 1 */
struct gw_info *gws_2;	/* Pointer to gw table 2 */
struct tm_binds tmb;

/* AVPs overwriting the module parameters */
static avp_ident_t *inv_timer_param = NULL;
static avp_ident_t *inv_timer_next_param = NULL;

/*
 * Module functions that are defined later
 */
int load_gws(struct sip_msg* _m, char* _s1, char* _s2);
int next_gw(struct sip_msg* _m, char* _s1, char* _s2);
int from_gw(struct sip_msg* _m, char* _s1, char* _s2);
int to_gw(struct sip_msg* _m, char* _s1, char* _s2);
int load_contacts (struct sip_msg*, char*, char*);
int next_contacts (struct sip_msg*, char*, char*);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"load_gws",      load_gws,      0, 0, REQUEST_ROUTE},
	{"next_gw",       next_gw,       0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"from_gw",       from_gw,       0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"to_gw",         to_gw,         0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"load_contacts", load_contacts, 0, 0, REQUEST_ROUTE},
	{"next_contacts", next_contacts, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",                   PARAM_STR, &db_url       },
	{"gw_table",                 PARAM_STR, &gw_table     },
	{"gw_name_column",           PARAM_STR, &gw_name_col  },
	{"ip_addr_column",           PARAM_STR, &ip_addr_col  },
	{"port_column",              PARAM_STR, &port_col     },
	{"uri_scheme_column",        PARAM_STR, &uri_scheme_col },
	{"transport_column",         PARAM_STR, &transport_col },
	{"grp_id_column",            PARAM_STR, &grp_id_col   },
	{"lcr_table",                PARAM_STR, &lcr_table    },
	{"prefix_column",            PARAM_STR, &prefix_col   },
	{"from_uri_column",          PARAM_STR, &from_uri_col },
	{"priority_column",          PARAM_STR, &priority_col },
	{"gw_uri_avp",               PARAM_STR, &gw_uri_avp },
	{"contact_avp",              PARAM_STR, &contact_avp  },
        {"fr_inv_timer_avp",         PARAM_STR, &inv_timer_avp  },
        {"fr_inv_timer",             PARAM_INT, &inv_timer      },
        {"fr_inv_timer_next",        PARAM_INT, &inv_timer_next },
        {"fr_inv_timer_param",       PARAM_STR, &inv_timer_ps },
        {"fr_inv_timer_next_param",  PARAM_STR, &inv_timer_next_ps },
	{"rpid_avp",                 PARAM_STR, &rpid_avp     },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"lcr",
	cmds,      /* Exported functions */
	lcr_rpc,   /* RPC methods */
	params,    /* Exported parameters */
	mod_init,  /* module initialization function */
	0,         /* response function */
	destroy,   /* destroy function */
	0,         /* oncancel function */
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
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main or tcp_main processes */
	if (lcr_db_init(db_url.s) < 0) {
		LOG(L_ERR, "ERROR: lcr:child_init():"
		    " Unable to connect to the database\n");
		return -1;
	}

	return 0;
}

/* get AVP module parameter */
static int get_avp_modparam(str *s, avp_ident_t *avp)
{
	if (!s->s || (s->len < 2)) return -1;

	if (s->s[0] != '$') {
		LOG(L_ERR, "ERROR: lcr: get_avp_modparam(): "
			"unknown AVP identifier: %.*s\n", s->len, s->s);
		return -1;
	}
	s->s++;
	s->len--;
	if (parse_avp_ident(s, avp)) {
		LOG(L_ERR, "ERROR: lcr: get_avp_modparam(): "
			"cannot parse AVP identifier: %.*s\n", s->len, s->s);
		return -1;
	}
	return 0;
}

/*
 * Module initialization function that is called before the main process forks
 */
static int mod_init(void)
{
	load_tm_f  load_tm;
	int ver, i;
	unsigned int par;

	DBG("lcr - initializing\n");

	Q_FLAG = register_avpflag(DEF_Q_FLAG);
	if (Q_FLAG == 0) {
		LOG(L_ERR, "ERROR: lcr:mod_init(): cannot regirser AVP flag: %s\n", DEF_Q_FLAG);
		return -1;
	}

	/* import the TM auto-loading function */
	if (!(load_tm = (load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
	    LOG(L_ERR, "ERROR: lcr:mod_init(): cannot import load_tm\n");
		goto err;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm(&tmb) == -1) goto err;

	/* Bind database */
	if (lcr_db_bind(db_url.s)) {
		LOG(L_ERR, "ERROR: lcr:mod_init(): No database module found\n");
		return -1;
	}

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
				" lcr table (use ser_mysql.sh reinstall)\n");
		goto err;
	}

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
	*gws = gws_1;

	/* First reload */
        if (reload_gws() == -1) {
		LOG(L_CRIT, "ERROR: lcr:mod_init():"
		    " failed to reload gateways\n");
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

	if (get_avp_modparam(&inv_timer_avp, &tm_timer_param))
		goto err;

	if (inv_timer_ps.len) {
		inv_timer_param = (avp_ident_t*)pkg_malloc(sizeof(avp_ident_t));
		if (!inv_timer_param) {
			LOG(L_ERR, "ERROR: lcr: mod_init(): not enough memory\n");
			return -1;
		}
		if (get_avp_modparam(&inv_timer_ps, inv_timer_param)) 
			goto err;
	}

	if (inv_timer_next_ps.len) {
		inv_timer_next_param = (avp_ident_t*)pkg_malloc(sizeof(avp_ident_t));
		if (!inv_timer_next_param) {
			LOG(L_ERR, "ERROR: lcr: mod_init(): not enough memory\n");
			return -1;
		}
		if (get_avp_modparam(&inv_timer_next_ps, inv_timer_next_param)) 
			goto err;
	}

	return 0;

err:
	return -1;
}


static void destroy(void)
{
	lcr_db_close();

	if (inv_timer_param) pkg_free(inv_timer_param);
	if (inv_timer_next_param) pkg_free(inv_timer_next_param);
}


/*
 * Reload gws to unused gw table and when done, make the unused gw table
 * the one in use.
 */
int reload_gws ( void )
{
    int q_len, i;
    unsigned int ip_addr, port, prefix_len;
    uri_type scheme;
    uri_transport transport;
    db_con_t* dbh;
    char query[LCR_MAX_QUERY_SIZE], *prefix;
    db_res_t* res;
    db_row_t* row;

    q_len = snprintf(query, LCR_MAX_QUERY_SIZE, "SELECT %.*s, %.*s, %.*s, %.*s, %.*s FROM %.*s",
		     ip_addr_col.len, ip_addr_col.s,
		     port_col.len, port_col.s,
		     uri_scheme_col.len, uri_scheme_col.s,
		     transport_col.len, transport_col.s,
		     prefix_col.len, prefix_col.s,
		     gw_table.len, gw_table.s);

    if (q_len >= LCR_MAX_QUERY_SIZE) {
	LOG(L_ERR, "lcr_reload_gws(): Too long database query\n");
	return -1;
    }

    if (lcr_dbf.init==0){
	    LOG(L_CRIT, "ERROR: lcr_db_ver: unbound database\n");
	    return -1;
    }
    dbh=lcr_dbf.init(db_url.s);
    if (dbh==0){
	    LOG(L_ERR, "ERROR: reload_gws: unable to open database connection\n");
	    return -1;
    }

    if (lcr_dbf.raw_query(dbh, query, &res) < 0) {
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
	    prefix_len = 0;
	    prefix = (char *)0;
	} else {
	    prefix = (char *)VAL_STRING(ROW_VALUES(row) + 4);
	    prefix_len = strlen(prefix);
	    if (prefix_len > MAX_PREFIX_LEN) {
		LOG(L_ERR, "reload_gws(): too long prefix\n");
		lcr_dbf.free_result(dbh, res);
		lcr_dbf.close(dbh);
		return -1;
	    }
	}
	if (*gws == gws_1) {
		gws_2[i].ip_addr = ip_addr;
		gws_2[i].port = port;
		gws_2[i].scheme = scheme;
		gws_2[i].transport = transport;
		gws_2[i].prefix_len = prefix_len;
		if (prefix_len)
		    memcpy(&(gws_2[i].prefix[0]), prefix, prefix_len);
	} else {
		gws_1[i].ip_addr = ip_addr;
		gws_1[i].port = port;
		gws_1[i].scheme = scheme;
		gws_1[i].transport = transport;
		gws_1[i].prefix_len = prefix_len;
		if (prefix_len)
		    memcpy(&(gws_1[i].prefix[0]), prefix, prefix_len);
	}
    }

    lcr_dbf.free_result(dbh, res);
    lcr_dbf.close(dbh);

    if (*gws == gws_1) {
	    gws_2[i].ip_addr = 0;
	    *gws = gws_2;
    } else {
	    gws_1[i].ip_addr = 0;
	    *gws = gws_1;
    }

    return 1;
}

/*
 * Load GW info from database to lcr_gw_addr_port AVPs
 */
int load_gws(struct sip_msg* _m, char* _s1, char* _s2)
{
    db_res_t* res;
    db_row_t *row, *r;
    str ruri_user, from_uri, value;
    char query[LCR_MAX_QUERY_SIZE];
    char ruri[MAX_URI_SIZE];
    unsigned int q_len, i, j, prefix_len;
    unsigned int addr, port;
    uri_type scheme;
    uri_transport transport;
    struct ip_addr address;
    str addr_str, port_str;
    char *at, *prefix;
    int_str val;

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

    q_len = snprintf(query, LCR_MAX_QUERY_SIZE, "SELECT %.*s.%.*s, %.*s.%.*s, %.*s.%.*s, %.*s.%.*s, %.*s.%.*s FROM %.*s, %.*s WHERE '%.*s' LIKE %.*s.%.*s AND '%.*s' LIKE CONCAT(%.*s.%.*s, '%%') AND %.*s.%.*s = %.*s.%.*s ORDER BY CHAR_LENGTH(%.*s.%.*s), %.*s.%.*s DESC, RAND()",
		     gw_table.len, gw_table.s, ip_addr_col.len, ip_addr_col.s,
		     gw_table.len, gw_table.s, port_col.len, port_col.s,
		     gw_table.len, gw_table.s, uri_scheme_col.len, uri_scheme_col.s,
		     gw_table.len, gw_table.s, transport_col.len, transport_col.s,
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
    if (q_len >= LCR_MAX_QUERY_SIZE) {
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
		goto skip;
	}
      	addr = (unsigned int)VAL_INT(ROW_VALUES(row));
	for (j = i + 1; j < RES_ROW_N(res); j++) {
		r = RES_ROWS(res) + j;
		if (addr == (unsigned int)VAL_INT(ROW_VALUES(r))) goto skip;
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
	    prefix_len = 0;
	    prefix = (char *)0;
	} else {
	    prefix = (char *)VAL_STRING(ROW_VALUES(row) + 4);
	    prefix_len = strlen(prefix);
	}
	if (5 + prefix_len + ruri_user.len + 1 + 15 + 1 + 5 + 1 + 14 > MAX_URI_SIZE) {
	    LOG(L_ERR, "load_gws(): Request URI would be too long\n");
	    goto skip;
	}
	at = (char *)&(ruri[0]);
	if (scheme == SIP_URI_T) {
	    memcpy(at, "sip:", 4); at = at + 4;
	} else if (scheme == SIPS_URI_T) {
	    memcpy(at, "sips:", 5); at = at + 5;
	} else {
	    LOG(L_ERR, "load_gws(): Unknown or unsupported URI scheme: %u\n", (unsigned int)scheme);
	    goto skip;
	}
	if (prefix_len) {
	    memcpy(at, prefix, prefix_len); at = at + prefix_len;
	}
	memcpy(at, ruri_user.s, ruri_user.len); at = at + ruri_user.len;
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
		LOG(L_ERR, "load_gws(): Unknown or unsupported transport: %u\n", (unsigned int)transport);
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

    lcr_dbf.free_result(db_handle, res);

    return 1;
}


/*
 * If called from route block, rewrites host:port part of R-URI with the
 * first lcr_gw_addr:lcr_gw_port AVP values, which are then destroyed.
 * If called from failure route block, appends a new branch to request,
 * where host:port part of its R-URI is replaced by the first
 * lcr_gw_addr:lcr_gw_port AVP value, which is then destroyed.
 * Returns 1 upon success and -1 upon failure.
 */
int next_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
    int_str val;
    struct action act;
    int rval;
    struct usr_avp *avp;

    avp = search_first_avp(gw_uri_avp_name_str, gw_uri_name, &val, 0);
    if (!avp) return -1;

    memset(&act, 0, sizeof(act));
    if (*(tmb.route_mode) == MODE_REQUEST) {

	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = val.s.s;
	rval = do_action(&act, _m);
	destroy_avp(avp);
	if (rval != 1) {
	    LOG(L_ERR, "next_gw(): ERROR: do_action failed with return value <%d>\n", rval);
	    return -1;
	}

	return 1;

    } else { /* MODE_ONFAILURE */

	act.type = APPEND_BRANCH_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = val.s.s;
	act.val[1].type = NUMBER_ST;
	act.val[1].u.number = 0;
	rval = do_action(&act, _m);
	destroy_avp(avp);
	if (rval != 1) {
	    LOG(L_ERR, "next_gw(): ERROR: do_action failed with return value <%d>\n", rval);
	    return -1;
	}

	return 1;
    }
}


/*
 * Checks if request comes from a gateway
 */
int from_gw(struct sip_msg* _m, char* _s1, char* _s2)
{
    int i;
    unsigned int src_addr;

    src_addr = _m->rcv.src_ip.u.addr32[0];

    for (i = 0; i < MAX_NO_OF_GWS; i++) {
	    if ((*gws)[i].ip_addr == 0) {
		    return -1;
	    }
	    if ((*gws)[i].ip_addr == src_addr) {
		    return 1;
	    }
    }

    return -1;
}


/*
 * Checks if in-dialog request goes to gateway
 */
int to_gw(struct sip_msg* _m, char* _s1, char* _s2)
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
	if ((*gws)[i].ip_addr == addr.s_addr) {
	    return 1;
	}
    }

    return -1;
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

	init_branch_iterator();
	while((branch.s = next_branch(&branch.len, &q, 0, 0, 0))) {
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
	init_branch_iterator();
	while((branch.s = next_branch(&branch.len, &q, 0, 0, 0))) {
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
 * Returns the value of the given AVP.
 * The default value is returned in case of missing AVP
 */
static int get_timer_value(avp_ident_t *avp, int def_value)
{
	struct usr_avp	*ret_avp;
	avp_value_t	avp_val;
	unsigned int	i;

	/* avp is not defined, use the default value */
	if (!avp) return def_value;

	ret_avp = search_avp_by_index(	avp->flags,
					avp->name,
					&avp_val,
					avp->index);
	/* avp is missing, use the default value */
	if (!ret_avp) return def_value;

	if (ret_avp->flags & AVP_VAL_STR) {
		if (str2int(&avp_val.s, &i)) {
			LOG(L_WARN, "get_timer_value(): WARNING: "
				"cannot convert AVP string value to int: %.*s\n",
				avp_val.s.len, avp_val.s.s);

			return def_value;
		}
		return (int)i;
	} else {
		return avp_val.n;
	}
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
    struct search_state st;
    struct usr_avp *avp, *prev;
    int_str val;
    struct action act;
    int rval;

    if (*(tmb.route_mode) == MODE_REQUEST) {

	/* Find first lcr_contact_avp value */
	avp = search_first_avp(contact_avp_name_str, contact_name, &val, &st);
	if (!avp) {
	    DBG("next_contacts(): DEBUG: No AVPs -- we are done!\n");
	    return 1;
	}

	/* Set Request-URI */
	memset(&act, 0, sizeof(act));
	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = val.s.s;
	rval = do_action(&act, msg);
	if (rval != 1) {
	    destroy_avp(avp);
	    return rval;
	}
	DBG("next_contacts(): DEBUG: R-URI is <%s>\n", val.s.s);
	if (avp->flags & Q_FLAG) {
	    destroy_avp(avp);
	    /* Set fr_inv_timer */
	    val.n = get_timer_value(inv_timer_next_param, inv_timer_next);
	    if (add_avp(tm_timer_param.flags, tm_timer_param.name, val) != 0) {
		LOG(L_ERR, "next_contacts(): ERROR: setting of fr_inv_timer_avp failed\n");
		return -1;
	    }
	    return 1;
	}

	/* Append branches until out of branches or Q_FLAG is set */
	prev = avp;
	while ((avp = search_next_avp(&st, &val))) {
	    destroy_avp(prev);
	    memset(&act, 0, sizeof(act));
	    act.type = APPEND_BRANCH_T;
	    act.val[0].type = STRING_ST;
	    act.val[0].u.string = val.s.s;
	    act.val[1].type = NUMBER_ST;
	    act.val[1].u.number = 0;
	    rval = do_action(&act, msg);
	    if (rval != 1) {
		destroy_avp(avp);
		LOG(L_ERR, "next_contacts(): ERROR: do_action failed with return value <%d>\n", rval);
		return -1;
	    }
	    DBG("next_contacts(): DEBUG: Branch is <%s>\n", val.s.s);
	    if (avp->flags & Q_FLAG) {
		destroy_avp(avp);
		val.n = get_timer_value(inv_timer_next_param, inv_timer_next);
		if (add_avp(tm_timer_param.flags, tm_timer_param.name, val) != 0) {
		    LOG(L_ERR, "next_contacts(): ERROR: setting of fr_inv_timer_avp failed\n");
		    return -1;
		}
		return 1;
	    }
	    prev = avp;
	}

    } else { /* MODE_ONFAILURE */

	avp = search_first_avp(contact_avp_name_str, contact_name, &val, &st);
	if (!avp) return -1;

	prev = avp;
	do {
	    memset(&act, 0, sizeof(act));
	    act.type = APPEND_BRANCH_T;
	    act.val[0].type = STRING_ST;
	    act.val[0].u.string = val.s.s;
	    act.val[1].type = NUMBER_ST;
	    act.val[1].u.number = 0;
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
	    avp = search_next_avp(&st, &val);
	    destroy_avp(prev);
	} while (avp);

	/* Restore fr_inv_timer */
	/* delete previous value */
	if ((avp = search_first_avp(tm_timer_param.flags, tm_timer_param.name, 0, 0))) {
		destroy_avp(avp);
	}

	/* add new value */
	val.n = get_timer_value(inv_timer_param, inv_timer);
	DBG("next_contacts(): val.n=%d!\n", val.n);

	if (add_avp(tm_timer_param.flags, tm_timer_param.name, val) != 0) {
	    LOG(L_ERR, "next_contacts(): ERROR: setting of fr_inv_timer_avp failed\n");
	    return -1;
	}
    }

    return 1;
}
