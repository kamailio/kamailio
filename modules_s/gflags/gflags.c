/*$Id$
 *
 * gflags module: global flags; it keeps a bitmap of flags
 * in shared memory and may be used to change behaviour 
 * of server based on value of the flags. E.g.,
 *    if (is_gflag("1")) { t_relay_to_udp("10.0.0.1","5060"); }
 *    else { t_relay_to_udp("10.0.0.2","5060"); }
 * The benefit of this module is the value of the switch flags
 * can be manipulated by external applications such as web interface
 * or command line tools.
 *
 *
 * Copyright (C) 2004 FhG
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
 */
/*
 * History:
 * --------
 *  2004-09-09  initial module created (jiri)
 *
 * TODO
 * - flag range checking
 * - named flags (takes a protected name list)
 */


/* flag buffer size for FIFO protocool */
#define MAX_FLAG_LEN 12
/* FIFO action protocol names */
#define FIFO_SET_GFLAG "set_gflag"
#define FIFO_IS_GFLAG "is_gflag"
#define FIFO_RESET_GFLAG "reset_gflag"

#include <stdio.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../db/db.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../fifo_server.h"
#include "../../usr_avp.h"
#include "../../config.h"


MODULE_VERSION

static int set_gflag(struct sip_msg*, char *, char *);
static int reset_gflag(struct sip_msg*, char *, char *);
static int is_gflag(struct sip_msg*, char *, char *);

static int mod_init(void);
static void mod_destroy(void);

static int initial=0;
static unsigned int *gflags; 

static char* db_url = DEFAULT_RODB_URL;
static int load_global_attrs = 0;
static char* attr_table = "global_attrs";
static char* attr_name = "name";
static char* attr_type = "type";
static char* attr_value = "value";
static char* attr_flags = "flags";

static db_con_t* con;
static db_func_t db;

static avp_t* global_avps;

static cmd_export_t cmds[]={
	{"set_gflag",   set_gflag,   1, fixup_int_1, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE}, 
	{"reset_gflag", reset_gflag, 1, fixup_int_1, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE}, 
	{"is_gflag",    is_gflag,    1, fixup_int_1, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE}, 
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={ 
	{"initial",            INT_PARAM, &initial          },
	{"db_url",             STR_PARAM, &db_url           },
	{"load_global_attrs",  INT_PARAM, &load_global_attrs},
	{"global_attrs_table", STR_PARAM, &attr_table       },
	{"global_attrs_name",  STR_PARAM, &attr_name        },
	{"global_attrs_type",  STR_PARAM, &attr_type        },
	{"global_attrs_value", STR_PARAM, &attr_value       },
	{"global_attrs_flags", STR_PARAM, &attr_flags       },
	{0, 0, 0} 
};

struct module_exports exports = {
	"gflags", 
	cmds,
	params,
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0,           /* oncancel function */
	0            /* per-child init function */
};


static unsigned int read_flag(FILE *pipe, char *response_file)
{
	char flag_str[MAX_FLAG_LEN];
	int flag_len;
	unsigned int flag_nr;
	str fs;

	if (!read_line(flag_str, MAX_FLAG_LEN, pipe, &flag_len) 
			|| flag_len == 0) {
		fifo_reply(response_file, "400: gflags: invalid flag number\n");
		LOG(L_ERR, "ERROR: read_flag: invalid flag number\n");
		return 0;
	}

	fs.s=flag_str;fs.len=flag_len;
	if (str2int(&fs, &flag_nr) < 0) {
		fifo_reply(response_file, "400: gflags: invalid flag format\n");
		LOG(L_ERR, "ERROR: read_flag: invalid flag format\n");
		return 0;
	}

	return flag_nr;
}
	

static int fifo_set_gflag( FILE* pipe, char* response_file )
{

	unsigned int flag;

	flag=read_flag(pipe, response_file);
	if 	(!flag) {
		LOG(L_ERR, "ERROR: fifo_set_gflag: failed in read_flag\n");
		return 1;
	}

	(*gflags) |= 1 << flag;
	fifo_reply (response_file, "200 OK\n");
	return 1;
}

static int fifo_reset_gflag( FILE* pipe, char* response_file )
{

	unsigned int flag;

	flag=read_flag(pipe, response_file);
	if 	(!flag) {
		LOG(L_ERR, "ERROR: fifo_reset_gflag: failed in read_flag\n");
		return 1;
	}

	(*gflags) &= ~ (1 << flag);
	fifo_reply (response_file, "200 OK\n");
	return 1;
}

static int fifo_is_gflag( FILE* pipe, char* response_file )
{

	unsigned int flag;

	flag=read_flag(pipe, response_file);
	if 	(!flag) {
		LOG(L_ERR, "ERROR: fifo_reset_gflag: failed in read_flag\n");
		return 1;
	}

	fifo_reply (response_file, "200 OK\n%s\n", 
			((*gflags) & (1<<flag)) ? "TRUE" : "FALSE" );
	return 1;
}

static int set_gflag(struct sip_msg *bar, char *flag_par, char *foo) 
{
	unsigned long int flag;

	flag=*((unsigned long int*)flag_par);
	(*gflags) |= 1 << flag;
	return 1;
}
	
static int reset_gflag(struct sip_msg *bar, char *flag_par, char *foo)
{
	unsigned long int flag;

	flag=*((unsigned long int*)flag_par);
	(*gflags) &= ~ (1 << flag);
	return 1;
}

static int is_gflag(struct sip_msg *bar, char *flag_par, char *foo)
{
	unsigned long int flag;

	flag=*((unsigned long int*)flag_par);
	return ( (*gflags) & (1<<flag)) ? 1 : -1;
}
	

/*
 * Load attributes from domain_attrs table
 */
static int load_attrs(void)
{
	int_str name, v;

	str avp_name, avp_val;
	int i, type, n;
	db_key_t cols[4];
	db_res_t* res;
	db_val_t *val;
	unsigned short flags;

	if (!con) {
		LOG(L_ERR, "gflags:load_attrs: Invalid database handle\n");
		return -1;
	}
	
	cols[0] = attr_name;
	cols[1] = attr_type;
	cols[2] = attr_value;
	cols[3] = attr_flags;

	if (db.use_table(con, attr_table) < 0) {
		LOG(L_ERR, "gflags:load_attrs: Error in use_table\n");
		return -1;
	}

	if (db.query(con, 0, 0, 0, cols, 0, 4, 0, &res) < 0) {
		LOG(L_ERR, "gflags:load_attrs: Error while quering database\n");
		return -1;
	}

	n = 0;
	for(i = 0; i < res->n; i++) {
		val = res->rows[i].values;

		if (val[0].nul || val[1].nul || val[3].nul) {
			LOG(L_ERR, "gflags:load_attrs: Skipping row containing NULL entries\n");
			continue;
		}

		if ((val[3].val.int_val & DB_LOAD_SER) == 0) continue;

		n++;
		     /* Get AVP name */
		avp_name.s = (char*)val[0].val.string_val;
		avp_name.len = strlen(avp_name.s);
		name.s = &avp_name;

		     /* Get AVP type */
		type = val[1].val.int_val;

		     /* Test for NULL value */
		if (val[2].nul) {
			avp_val.s = 0;
			avp_val.len = 0;
		} else {
			avp_val.s = (char*)val[2].val.string_val;
			avp_val.len = strlen(avp_val.s);
		}

		flags = AVP_GLOBAL | AVP_NAME_STR;
		if (type == AVP_VAL_STR) {
			     /* String AVP */
			v.s = &avp_val;
			flags |= AVP_VAL_STR;
		} else {
			     /* Integer AVP */
			str2int(&avp_val, (unsigned*)&v.n);
		}

		if (add_avp(flags, name, v) < 0) {
			LOG(L_ERR, "gflags:load_attrs: Error while adding global attribute %.*s, skipping\n",
			    avp_name.len, ZSW(avp_name.s));
			continue;
		}
	}
	DBG("gflags:load_attrs: %d domain attributes found, %d loaded\n", res->n, n);
	db.free_result(con, res);
	return 0;
}


static int mod_init(void)
{
	gflags=(unsigned int *) shm_malloc(sizeof(unsigned int));
	if (!gflags) {
		LOG(L_ERR, "Error: gflags/mod_init: no shmem\n");
		return -1;
	}
	*gflags=initial;
	if (register_fifo_cmd(fifo_set_gflag, FIFO_SET_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register FIFO_SET_GFLAG\n");
		return -1;
	}
	if (register_fifo_cmd(fifo_reset_gflag, FIFO_RESET_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register FIFO_RESET_GFLAG\n");
		return -1;
	}
	if (register_fifo_cmd(fifo_is_gflag, FIFO_IS_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register FIFO_SET_GFLAG\n");
		return -1;
	}

	if (load_global_attrs) {
		if (bind_dbmod(db_url, &db) < 0) { /* Find database module */
			LOG(L_ERR, "gflags:mod_init: Can't bind database module\n");
			return -1;
		}
		if (!DB_CAPABILITY(db, DB_CAP_ALL)) {
			LOG(L_ERR, "gflags:mod_init: Database module does not implement"
			    " all functions needed by the module\n");
			return -1;
		}

		con = db.init(db_url); /* Get a new database connection */
		if (!con) {
			LOG(L_ERR, "gflags:mod_init: Error while connecting database\n");
			return -1;
		}

		if (load_attrs() < 0) {
			db.close(con);
			return -1;
		}

		set_global_avp_list(&global_avps);
		
		db.close(con);
	}

	global_avps = 0;
	return 0;
}


static void mod_destroy(void)
{
	destroy_avp_list(&global_avps);
}
