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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "../mysql/dbase.h"
#include "../../action.h"
#include "../../modules/tm/tm_load.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "fifo.h"

MODULE_VERSION

/*
 * Version of gw and lcr tables required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define GW_TABLE_VERSION 1
#define LCR_TABLE_VERSION 1

/* usr_avp flag for sequential forking */
#define Q_FLAG      (1<<4)

static void destroy(void);       /* Module destroy function */
static int child_init(int rank); /* Per-child initialization function */
static int mod_init(void);       /* Module initialization function */

int reload_gws ( void );

#define GW_TABLE "gw"
#define GW_TABLE_LEN (sizeof(GW_TABLE) - 1)

#define GW_NAME_COL "gw_name"
#define GW_NAME_COL_LEN (sizeof(GW_NAME_COL) - 1)

#define IP_ADDR_COL "ip_addr"
#define IP_ADDR_COL_LEN (sizeof(IP_ADDR_COL) - 1)

#define PORT_COL "port"
#define PORT_COL_LEN (sizeof(PORT_COL) - 1)

#define GRP_ID_COL "grp_id"
#define GRP_ID_COL_LEN (sizeof(GRP_ID_COL) - 1)

#define LCR_TABLE "lcr"
#define LCR_TABLE_LEN (sizeof(LCR_TABLE) - 1)

#define PREFIX_COL "prefix"
#define PREFIX_COL_LEN (sizeof(PREFIX_COL) - 1)

#define FROM_URI_COL "from_uri"
#define FROM_URI_COL_LEN (sizeof(FROM_URI_COL) - 1)

#define PRIORITY_COL "priority"
#define PRIORITY_COL_LEN (sizeof(PRIORITY_COL) - 1)

#define MAX_QUERY_SIZE 512
#define MAX_NO_OF_GWS 32

/* Default avp names */
#define DEF_GW_ADDR_AVP "lcr_gw_addr"
#define DEF_GW_PORT_AVP "lcr_gw_port"
#define DEF_CONTACT_AVP "lcr_contact"
#define DEF_FR_INV_TIMER_AVP "fr_inv_timer_avp"
#define DEF_FR_INV_TIMER 90
#define DEF_FR_INV_TIMER_NEXT 30

/*
 * Type definitions
 */
struct gw_info {
	unsigned int ip_addr;
	unsigned int port;
};

/*
 * Database variables
 */
static db_con_t* db_handle = 0;   /* Database connection handle */
static db_func_t lcr_dbf;

/*
 * Module parameter variables
 */
static str db_url    = {DEFAULT_RODB_URL, DEFAULT_RODB_URL_LEN};
str gw_table         = {GW_TABLE, GW_TABLE_LEN};
str gw_name_col      = {GW_NAME_COL, GW_NAME_COL_LEN};
str ip_addr_col      = {IP_ADDR_COL, IP_ADDR_COL_LEN};
str port_col         = {PORT_COL, PORT_COL_LEN};
str grp_id_col       = {GRP_ID_COL, GRP_ID_COL_LEN};
str lcr_table        = {LCR_TABLE, LCR_TABLE_LEN};
str prefix_col       = {PREFIX_COL, PREFIX_COL_LEN};
str from_uri_col     = {FROM_URI_COL, FROM_URI_COL_LEN};
str priority_col     = {PRIORITY_COL, PRIORITY_COL_LEN};
str gw_addr_avp      = {DEF_GW_ADDR_AVP, sizeof(DEF_GW_ADDR_AVP) - 1};
str gw_port_avp      = {DEF_GW_PORT_AVP, sizeof(DEF_GW_PORT_AVP) - 1};
str contact_avp      = {DEF_CONTACT_AVP, sizeof(DEF_CONTACT_AVP) - 1};
str inv_timer_avp    = {DEF_FR_INV_TIMER_AVP, sizeof(DEF_FR_INV_TIMER_AVP)
			-1 };
int inv_timer        = DEF_FR_INV_TIMER;
int inv_timer_next   = DEF_FR_INV_TIMER_NEXT;

/*
 * Other module types and variables
 */

struct contact {
    str uri;
    qvalue_t q;
    unsigned short q_flag;
    struct contact *next;
};

int_str addr_name, port_name, contact_name, inv_timer_name;

struct gw_info **gws;	/* Pointer to current gw table pointer */
struct gw_info *gws_1;	/* Pointer to gw table 1 */
struct gw_info *gws_2;	/* Pointer to gw table 2 */
struct tm_binds tmb;


/*
 * Module functions that are defined later
 */
int load_gws(struct sip_msg* _m, char* _s1, char* _s2);
int next_gw(struct sip_msg* _m, char* _s1, char* _s2);
int from_gw(struct sip_msg* _m, char* _s1, char* _s2);
int load_contacts (struct sip_msg*, char*, char*);
int next_contacts (struct sip_msg*, char*, char*);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"load_gws",      load_gws,      0, 0, REQUEST_ROUTE},
	{"next_gw",       next_gw,       0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"from_gw",       from_gw,       0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"load_contacts", load_contacts, 0, 0, REQUEST_ROUTE},
	{"next_contacts", next_contacts, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
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
	{"grp_id_column",            STR_PARAM, &grp_id_col.s   },
	{"lcr_table",                STR_PARAM, &lcr_table.s    },
	{"prefix_column",            STR_PARAM, &prefix_col.s   },
	{"from_uri_column",          STR_PARAM, &from_uri_col.s },
	{"priority_column",          STR_PARAM, &priority_col.s },
	{"gw_addr_avp",              STR_PARAM, &gw_addr_avp.s  },
	{"gw_port_avp",              STR_PARAM, &gw_port_avp.s  },
	{"contact_avp",              STR_PARAM, &contact_avp.s  },
        {"fr_inv_timer_avp",         STR_PARAM, &inv_timer_avp.s  },
        {"fr_inv_timer",             INT_PARAM, &inv_timer      },
        {"fr_inv_timer_next",        INT_PARAM, &inv_timer_next },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"lcr", 
	cmds,      /* Exported functions */
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
	if (lcr_db_init(db_url.s) < 0) {
		LOG(L_ERR, "ERROR: lcr:child_init():"
		    " Unable to connect to the database\n");
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

	DBG("lcr - initializing\n");

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

	/* Update length of module variables */
	db_url.len = strlen(db_url.s);
        gw_table.len = strlen(gw_table.s);
	gw_name_col.len = strlen(gw_name_col.s);
	ip_addr_col.len = strlen(ip_addr_col.s);
	port_col.len = strlen(port_col.s);
        grp_id_col.len = strlen(grp_id_col.s);
        lcr_table.len = strlen(lcr_table.s);
	prefix_col.len = strlen(prefix_col.s);
	from_uri_col.len = strlen(from_uri_col.s);
        priority_col.len = strlen(priority_col.s);
	gw_addr_avp.len = strlen(gw_addr_avp.s);
	gw_port_avp.len = strlen(gw_port_avp.s);
	contact_avp.len = strlen(contact_avp.s);
	inv_timer_avp.len = strlen(inv_timer_avp.s);

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
	*gws = gws_1;

	/* First reload */
        if (reload_gws() == -1) {
		LOG(L_CRIT, "ERROR: lcr:mod_init():"
		    " failed to reload gateways\n");
		goto err;
	}

	addr_name.s = &gw_addr_avp;
	port_name.s = &gw_port_avp;
	contact_name.s = &contact_avp;
	inv_timer_name.s = &inv_timer_avp;

	return 0;

err:
	return -1;
}


static void destroy(void)
{
	lcr_db_close();
}


/*
 * Reload gws to unused gw table and when done, make the unused gw table
 * the one in use.
 */
int reload_gws ( void )
{
    int q_len, i;
    unsigned int ip_addr, port;
    db_con_t* dbh;
    char query[MAX_QUERY_SIZE];
    db_res_t* res;
    db_row_t* row;

    q_len = snprintf(query, MAX_QUERY_SIZE, "SELECT %.*s, %.*s FROM %.*s",
		     ip_addr_col.len, ip_addr_col.s,
		     port_col.len, port_col.s,
		     gw_table.len, gw_table.s);

    if (q_len >= MAX_QUERY_SIZE) {
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
	if (*gws == gws_1) {
		gws_2[i].ip_addr = ip_addr;
		gws_2[i].port = port;
	} else {
		gws_1[i].ip_addr = ip_addr;
		gws_1[i].port = port;
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


/* Print gateways stored in current gw table */
void print_gws (FILE *reply_file)
{
	int i;

	for (i = 0; i < MAX_NO_OF_GWS; i++) {
		if ((*gws)[i].ip_addr == 0) {
			return;
		}
		if ((*gws)[i].port == 0) {
			fprintf(reply_file, "%d.%d.%d.%d\n",
				((*gws)[i].ip_addr << 24) >> 24,
				(((*gws)[i].ip_addr >> 8) << 24) >> 24,
				(((*gws)[i].ip_addr >> 16) << 24) >> 24,
				(*gws)[i].ip_addr >> 24);
		} else {
			fprintf(reply_file, "%d.%d.%d.%d:%d\n",
				((*gws)[i].ip_addr << 24) >> 24,
				(((*gws)[i].ip_addr >> 8) << 24) >> 24,
				(((*gws)[i].ip_addr >> 16) << 24) >> 24,
				(*gws)[i].ip_addr >> 24,
				(*gws)[i].port);
		}
	}
}


/*
 * Load GW info from database to lcr_gw_addr and lcr_gw_port AVPs
 */
int load_gws(struct sip_msg* _m, char* _s1, char* _s2)
{
    db_res_t* res;
    db_row_t *row, *r;
    int_str val;	    
    str ruri_user, from_uri;
    char query[MAX_QUERY_SIZE];
    int q_len, i, j;

    /* Find Request-URI user */
    if (parse_sip_msg_uri(_m) < 0) {
	    LOG(L_ERR, "load_gws(): Error while parsing R-URI\n");
	    return -1;
    }
    ruri_user = _m->parsed_uri.user;

    /* Look for From URI */
    if ((!_m->from) && (parse_headers(_m, HDR_FROM, 0) == -1)) {
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
    
    q_len = snprintf(query, MAX_QUERY_SIZE, "SELECT %.*s.%.*s, %.*s.%.*s FROM %.*s, %.*s WHERE '%.*s' LIKE %.*s.%.*s AND '%.*s' LIKE CONCAT(%.*s.%.*s, '%%') AND %.*s.%.*s = %.*s.%.*s ORDER BY CHAR_LENGTH(%.*s.%.*s), %.*s.%.*s DESC, RAND()",
		     gw_table.len, gw_table.s, ip_addr_col.len, ip_addr_col.s,
		     gw_table.len, gw_table.s, port_col.len, port_col.s,
		     gw_table.len, gw_table.s, lcr_table.len, lcr_table.s,
		     from_uri.len, from_uri.s,
		     lcr_table.len, lcr_table.s, from_uri_col.len, from_uri_col.s,
		     ruri_user.len, ruri_user.s,
		     lcr_table.len, lcr_table.s, prefix_col.len, prefix_col.s,
		     lcr_table.len, lcr_table.s, grp_id_col.len,  grp_id_col.s,
		     gw_table.len, gw_table.s, grp_id_col.len, grp_id_col.s,
		     lcr_table.len, lcr_table.s, prefix_col.len, prefix_col.s,
		     lcr_table.len, lcr_table.s, priority_col.len, priority_col.s);
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
		goto skip;
	}
      	val.n = (int)VAL_INT(ROW_VALUES(row));
	for (j = i + 1; j < RES_ROW_N(res); j++) {
		r =  RES_ROWS(res) + j;
		if (val.n == (int)VAL_INT(ROW_VALUES(r))) goto skip;
	}
	add_avp(AVP_NAME_STR, addr_name, val);
	DBG("DEBUG:load_gws(): Added AVP <%.*s,%x>\n",
	    addr_name.s->len, addr_name.s->s, val.n);

	if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
		val.n = 0;
	} else {
		val.n = (int)VAL_INT(ROW_VALUES(row) + 1);
	}
	add_avp(AVP_NAME_STR, port_name, val);
	DBG("DEBUG:load_gws(): Added AVP <%.*s,%d>\n",
	    port_name.s->len, port_name.s->s, val.n);
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
    int rval, port_len;
    struct ip_addr addr;
    struct usr_avp *avp;
    str uri, uri_user, addr_str;
    char *at, *port;

    avp = search_first_avp(AVP_NAME_STR, addr_name, &val);
    if (!avp) return -1;

    addr.af = AF_INET;
    addr.len = 4;
    addr.u.addr32[0] = (unsigned)val.n;
    destroy_avp(avp);

    if (*(tmb.route_mode) == MODE_REQUEST) {

	avp = search_first_avp(AVP_NAME_STR, port_name, &val);
	if (!avp) return -1;

	act.p1.string = ip_addr2a(&addr);

	if (val.n == 0) {
	    act.type = SET_HOSTPORT_T;
	    act.p1_type = STRING_ST;
	    rval = do_action(&act, _m);
	} else {
	    act.type = SET_HOST_T;
	    act.p1_type = STRING_ST;
	    rval = do_action(&act, _m);
	    if (rval != 1) return -1;
	    act.p1.string = int2str((unsigned)val.n, &port_len);
	    act.type = SET_PORT_T;
	    act.p1_type = STRING_ST;
	    rval = do_action(&act, _m);
	}

	destroy_avp(avp);

	if (rval != 1) {
	    LOG(L_ERR, "next_gw(): ERROR: do_action failed with return value <%d>\n", rval);
	    return -1;
	}

	return 1;

    } else { /* MODE_ONFAILURE */

	avp = search_first_avp(AVP_NAME_STR, port_name, &val);
	if (!avp) return -1;
	if (val.n == 0) {
	    port = "5060";
	    port_len = 4;
	} else {
	    port = int2str(val.n, &port_len);
	}
	destroy_avp(avp);

	addr_str.s = ip_addr2a(&addr);
	addr_str.len = strlen(addr_str.s);

	if (parse_sip_msg_uri(_m) < 0) {
	    LOG(L_ERR, "next_gw(): Error while parsing R-URI\n");
	    return -1;
	}

	uri_user = _m->parsed_uri.user;

	uri.len = 4 + uri_user.len + 1 + addr_str.len + 1 + port_len + 1;
	if (uri.len > MAX_URI_SIZE) {
	    LOG(L_ERR, "next_gw(): URI is too long\n");
	    return -1;
	}
	uri.s = pkg_malloc(uri.len);
	if (!uri.s) {
	    LOG(L_ERR, "next_gw(): No memory for new uri\n");
	    return -1;
	}
	
	at = uri.s;
	memcpy(at, "sip:", 4);
	at = at + 4;
	memcpy(at, uri_user.s, uri_user.len);
	at = at + uri_user.len;
	*at = '@';
	at = at + 1;
	memcpy(at, addr_str.s, addr_str.len);
	at = at + addr_str.len;
	*at = ':';
	at = at + 1;
	memcpy(at, port, port_len + 1);

	act.type = APPEND_BRANCH_T;
	act.p1_type = STRING_ST;
	act.p1.string = uri.s; 
	rval = do_action(&act, _m);

	pkg_free(uri.s);

	if (rval != 1) {
	    LOG(L_ERR, "next_gw(): ERROR: do_action failed with return value <%d>\n", rval);
	    return -1;
	}

	return -1;
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
	struct usr_avp *avp;

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
	while((branch.s = next_branch(&branch.len, &q, 0, 0))) {
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
	while((branch.s = next_branch(&branch.len, &q, 0, 0))) {
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
	    val.s = &(curr->uri);
	    add_avp(AVP_NAME_STR|AVP_VAL_STR|(curr->q_flag),
		    contact_name, val);
	    curr = curr->next;
	}

	/* Clear all branches */
	clear_branches();

	/* Free contacts list */
	free_contact_list(contacts);

	/* Print all avp_contact_avp attributes */
	avp = search_first_avp(AVP_NAME_STR|AVP_VAL_STR, contact_name, &val);
	do {
	    DBG("load_contacts(): DEBUG: Loaded <%s>, q_flag <%d>\n",
		val.s->s, avp->flags & Q_FLAG);
	    avp = search_next_avp(avp, &val);
	} while (avp);
	
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

    if (*(tmb.route_mode) == MODE_REQUEST) {
	
	/* Find first avp_contact_avp value */
	avp = search_first_avp(AVP_NAME_STR|AVP_VAL_STR, contact_name, &val);
	if (!avp) {
	    DBG("next_contacts(): DEBUG: No AVPs -- we are done!\n");
	    return 1;
	}

	/* Set Request-URI */
	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = val.s->s;
	rval = do_action(&act, msg);
	if (rval != 1) {
	    destroy_avp(avp);
	    return rval;
	}
	DBG("next_contacts(): DEBUG: R-URI is <%s>\n", val.s->s);
	if (avp->flags & Q_FLAG) {
	    destroy_avp(avp);
	    /* Set fr_inv_timer */
	    val.n = inv_timer_next;
	    if (add_avp(AVP_NAME_STR, inv_timer_name, val) != 0) {
		LOG(L_ERR, "next_contacts(): ERROR: setting of fr_inv_timer_avp failed\n");
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
	    act.p1.string = val.s->s;
	    rval = do_action(&act, msg);
	    if (rval != 1) {
		destroy_avp(avp);
		LOG(L_ERR, "next_contacts(): ERROR: do_action failed with return value <%d>\n", rval);
		return -1;
	    }
	    DBG("next_contacts(): DEBUG: Branch is <%s>\n", val.s->s);
	    if (avp->flags & Q_FLAG) {
		destroy_avp(avp);
		val.n = inv_timer_next;
		if (add_avp(AVP_NAME_STR, inv_timer_name, val) != 0) {
		    LOG(L_ERR, "next_contacts(): ERROR: setting of fr_inv_timer_avp failed\n");
		    return -1;
		}
		return 1;
	    }
	    prev = avp;
	}

    } else { /* MODE_ONFAILURE */
	
	avp = search_first_avp(AVP_NAME_STR|AVP_VAL_STR, contact_name, &val);
	if (!avp) return -1;

	prev = avp;
	do {
	    act.type = APPEND_BRANCH_T;
	    act.p1_type = STRING_ST;
	    act.p1.string = val.s->s;
	    rval = do_action(&act, msg);
	    if (rval != 1) {
		destroy_avp(avp);
		return rval;
	    }
	    DBG("next_contacts(): DEBUG: New branch is <%s>\n", val.s->s);
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
	    LOG(L_ERR, "next_contacts(): ERROR: setting of fr_inv_timer_avp failed\n");
	    return -1;
	}
    }

    return 1;
}
