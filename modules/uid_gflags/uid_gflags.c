/*
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
 * Copyright (C) 2004 FhG FOKUS
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
 */
/*
 * TODO
 * - flag range checking
 * - named flags (takes a protected name list)
 */


#include <stdio.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../lib/srdb2/db.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../usr_avp.h"
#include "../../rpc.h"
#include "../../config.h"


MODULE_VERSION

static int set_gflag(struct sip_msg*, char *, char *);
static int reset_gflag(struct sip_msg*, char *, char *);
static int is_gflag(struct sip_msg*, char *, char *);
static int flush_gflags(struct sip_msg*, char*, char*);

static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);
static int reload_global_attributes(void);

static int initial = 0;
static unsigned int *gflags;

static char* db_url = DEFAULT_DB_URL;
static int   load_global_attrs = 0;
static char* attr_table = "uid_global_attrs";
static char* attr_name = "name";
static char* attr_type = "type";
static char* attr_value = "value";
static char* attr_flags = "flags";

static db_ctx_t* db = NULL;
static db_cmd_t* load_attrs_cmd = NULL, *save_gflags_cmd = NULL;

static avp_list_t** active_global_avps;
static avp_list_t *avps_1;
static avp_list_t *avps_2;
static rpc_export_t rpc_methods[];

static cmd_export_t cmds[]={
	{"set_ugflag",   set_gflag,   1, fixup_int_1, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"reset_ugflag", reset_gflag, 1, fixup_int_1, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"is_ugflag",    is_gflag,    1, fixup_int_1, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"flush_ugflags", flush_gflags, 0, 0,         REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"initial",            PARAM_INT,    &initial          },
	{"db_url",             PARAM_STRING, &db_url           },
	{"load_global_attrs",  PARAM_INT,    &load_global_attrs},
	{"global_attrs_table", PARAM_STRING, &attr_table       },
	{"global_attrs_name",  PARAM_STRING, &attr_name        },
	{"global_attrs_type",  PARAM_STRING, &attr_type        },
	{"global_attrs_value", PARAM_STRING, &attr_value       },
	{"global_attrs_flags", PARAM_STRING, &attr_flags       },
	{0, 0, 0}
};

struct module_exports exports = {
	"uid_gflags",
	cmds,
	rpc_methods, /* RPC methods */
	params,
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};


static int init_db(void)
{
	db_fld_t attr_res[] = {
		{.name = attr_name,  DB_STR},
		{.name = attr_type,  DB_INT},
		{.name = attr_value, DB_STR},
		{.name = attr_flags, DB_BITMAP},
		{.name = NULL}
	};

	db_fld_t values[] = {
		{.name = attr_name,  DB_CSTR},
		{.name = attr_type,  DB_INT},
		{.name = attr_value, DB_STR},
		{.name = attr_flags, DB_BITMAP},
		{.name = NULL}
	};

	db = db_ctx("gflags");
	if (db == NULL) {
		ERR("Error while initializing database layer\n");
		return -1;
	}
	if (db_add_db(db, db_url) < 0) return -1;
	if (db_connect(db) < 0) return -1;
	
	/* SELECT name, type, value, flags FROM global_attrs */
	load_attrs_cmd = db_cmd(DB_GET, db, attr_table, attr_res, NULL, NULL);
	if (load_attrs_cmd == NULL) {
		ERR("Error while building db query to load global attributes\n");
		return -1;
	}

	save_gflags_cmd = db_cmd(DB_PUT, db, attr_table, NULL, NULL, values);
	if (save_gflags_cmd == NULL) {
		ERR("Error while building db query to save global flags\n");
		return -1;
	}

	return 0;
}


static int set_gflag(struct sip_msg *bar, char *flag_par, char *foo)
{
	unsigned long int flag;

	if (!flag_par || ((fparam_t*)flag_par)->type != FPARAM_INT) {
	  LOG(L_ERR, "gflags:set_gflag: Invalid parameter\n");
	  return -1;
	}
	
	flag=((fparam_t*)flag_par)->v.i; 

	(*gflags) |= 1 << flag;
	return 1;
}

static int reset_gflag(struct sip_msg *bar, char *flag_par, char *foo)
{
	unsigned long int flag;

	if (!flag_par || ((fparam_t*)flag_par)->type != FPARAM_INT) {
	  LOG(L_ERR, "gflags:reset_gflag: Invalid parameter\n");
	  return -1;
	}
	
	flag=((fparam_t*)flag_par)->v.i; 
	(*gflags) &= ~ (1 << flag);
	return 1;
}

static int is_gflag(struct sip_msg *bar, char *flag_par, char *foo)
{
	unsigned long int flag;

	if (!flag_par || ((fparam_t*)flag_par)->type != FPARAM_INT) {
	  LOG(L_ERR, "gflags:is_gflag: Invalid parameter\n");
	  return -1;
	}
	
	flag=((fparam_t*)flag_par)->v.i; 
	return ( (*gflags) & (1<<flag)) ? 1 : -1;
}


/*
 * Load attributes from global_attrs table
 */
static int load_attrs(avp_list_t* global_avps)
{
	int_str name, v;
	db_res_t* res;
	db_rec_t* rec;
	str avp_val;
	unsigned short flags;

	if (db_exec(&res, load_attrs_cmd) < 0) return -1;

	rec = db_first(res);

	while(rec) {
		if (rec->fld[0].flags & DB_NULL ||
			rec->fld[1].flags & DB_NULL ||
			rec->fld[3].flags & DB_NULL) {
			LOG(L_ERR, "gflags:load_attrs: Skipping row containing NULL entries\n");
			goto skip;
		}

		if ((rec->fld[3].v.int4 & SRDB_LOAD_SER) == 0) goto skip;

		name.s = rec->fld[0].v.lstr;

		     /* Test for NULL value */
		if (rec->fld[2].flags & DB_NULL) {
			avp_val.s = 0;
			avp_val.len = 0;
		} else {
			avp_val = rec->fld[2].v.lstr;
		}

		flags = AVP_CLASS_GLOBAL | AVP_NAME_STR;
		if (rec->fld[1].v.int4 == AVP_VAL_STR) {
			/* String AVP */
			v.s = avp_val;
			flags |= AVP_VAL_STR;
		} else {
			/* Integer AVP */
			str2int(&avp_val, (unsigned*)&v.n);
			if (rec->fld[0].v.lstr.len == (sizeof(AVP_GFLAGS) - 1) &&
				!strncmp(rec->fld[0].v.lstr.s, AVP_GFLAGS, sizeof(AVP_GFLAGS) - 1)) {
				/* Restore gflags */
				*gflags = v.n;
			}
		}
		
		if (add_avp_list(global_avps, flags, name, v) < 0) {
			LOG(L_ERR, "gflags:load_attrs: Error while adding global attribute %.*s, skipping\n",
			    rec->fld[0].v.lstr.len, ZSW(rec->fld[0].v.lstr.s));
			goto skip;
		}

	skip:
		rec = db_next(res);
	}

	db_res_free(res);
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

	avps_1 = shm_malloc(sizeof(*avps_1));
	if (!avps_1) {
		ERR("can't allocate memory\n");
		return -1;
	}
	*avps_1 = NULL;
	avps_2 = shm_malloc(sizeof(*avps_2));
	if (!avps_2) {
		ERR("can't allocate memory\n");
		return -1;
	}
	*avps_2 = NULL;
	active_global_avps = &avps_1;

	if (load_global_attrs) {
		if (init_db() < 0) {
			shm_free(gflags);
			return -1;
		}
		
		if (load_attrs(*active_global_avps) < 0) {
			db_cmd_free(load_attrs_cmd);
			db_cmd_free(save_gflags_cmd);
			db_ctx_free(db);
			return -1;
		}
		
		set_avp_list(AVP_CLASS_GLOBAL, *active_global_avps);
		
		db_cmd_free(load_attrs_cmd);
		db_cmd_free(save_gflags_cmd);
		db_ctx_free(db);

		load_attrs_cmd = NULL;
		save_gflags_cmd = NULL;
		db = NULL;
	}

	return 0;
}

static int child_init(int rank)
{
	if (load_global_attrs) {
		if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
			return 0; /* do nothing for the main or tcp_main processes */

		if (init_db() < 0) return -1;
	}
	return 0;
}


static void mod_destroy(void)
{
	if (avps_1 != 0) {
		destroy_avp_list(avps_1);
	}
	if (avps_2 != 0) {
		destroy_avp_list(avps_2);
	}
	active_global_avps = 0;

	if (load_attrs_cmd) db_cmd_free(load_attrs_cmd);
	if (save_gflags_cmd) db_cmd_free(save_gflags_cmd);
	if (db) db_ctx_free(db);
}


int save_gflags(unsigned int flags)
{
	str fl;

	if (!load_global_attrs) {
		LOG(L_ERR, "gflags:save_gflags: You must enable load_global_attrs to make flush_gflag work\n");
		return -1;
	}

	fl.s = int2str(flags, &fl.len);

	save_gflags_cmd->vals[0].v.cstr = AVP_GFLAGS;
	save_gflags_cmd->vals[1].v.int4 = 0;
	save_gflags_cmd->vals[2].v.lstr = fl;
	save_gflags_cmd->vals[3].v.bitmap = SRDB_LOAD_SER;

	if (db_exec(NULL, save_gflags_cmd) < 0) {
		LOG(L_ERR, "gflags:save_gflag: Unable to store new value\n");
		return -1;
	}

	DBG("gflags:save_gflags: Successfuly stored in database\n");
	return 0;
}

static int reload_global_attributes(void)
{
	avp_list_t**  new_global_avps;
  
  /* Choose new global AVP list and free its old contents */
  if (active_global_avps == &avps_1) {
  	destroy_avp_list(avps_2);
		new_global_avps = &avps_2;
	} 
	else {
		destroy_avp_list(avps_1);
		new_global_avps = &avps_1;
	}
    
  if (load_attrs(*new_global_avps) < 0) {
  	goto error;
  }
  
  active_global_avps = new_global_avps;
  set_avp_list(AVP_CLASS_GLOBAL, *active_global_avps);

  return 0;
    
error:
	destroy_avp_list(*new_global_avps);
  return -1;
}

/*
 * Flush the state of global flags into database
 */
static int flush_gflags(struct sip_msg* msg, char* s1, char* s2)
{
	if (save_gflags(*gflags) < 0)  return -1;
	else return 1;
}


static const char* rpc_set_doc[] = {
	"Load a CPL script to the server.", /* Documentation string */
	0                                   /* Method signature(s) */
};

static void rpc_set(rpc_t* rpc, void* c)
{
        int flag;

	if (rpc->scan(c, "d", &flag) < 1) {
		rpc->fault(c, 400, "Flag number expected");
		return;
	}
	if (flag < 0 || flag > 31) {
		rpc->fault(c, 400, "Flag number %d out of range", &flag);
	}
	(*gflags) |= 1 << flag;
}


static const char* rpc_is_set_doc[] = {
	"Load a CPL script to the server.", /* Documentation string */
	0                                   /* Method signature(s) */
};

static void rpc_is_set(rpc_t* rpc, void* c)
{
        int flag;

	if (rpc->scan(c, "d", &flag) < 1) {
		rpc->fault(c, 400, "Flag number expected");
		return;
	}
	if (flag < 0 || flag > 31) {
		rpc->fault(c, 400, "Flag number %d out of range", &flag);
	}
	rpc->add(c, "b", (*gflags) & (1 << flag));
}


static const char* rpc_reset_doc[] = {
	"Load a CPL script to the server.", /* Documentation string */
	0                                   /* Method signature(s) */
};

static void rpc_reset(rpc_t* rpc, void* c)
{
        int flag;

	if (rpc->scan(c, "d", &flag) < 1) {
		rpc->fault(c, 400, "Flag number expected");
		return;
	}
	if (flag < 0 || flag > 31) {
		rpc->fault(c, 400, "Flag number %d out of range", &flag);
	}
	(*gflags) &= ~ (1 << flag);
}


static const char* rpc_flush_doc[] = {
	"Load a CPL script to the server.", /* Documentation string */
	0                                   /* Method signature(s) */
};

static void rpc_flush(rpc_t* rpc, void* c)
{
	if (flush_gflags(0, 0, 0) < 0) {
		rpc->fault(c, 400, "Error while saving flags to database");
	}
}


static const char* rpc_dump_doc[] = {
	"Load a CPL script to the server.", /* Documentation string */
	0                                   /* Method signature(s) */
};

static void rpc_dump(rpc_t* rpc, void* c)
{
        int i;
	for(i = 0; i < 32; i++) {
		rpc->add(c, "b", (*gflags >> i) & 1);
	}
}

static const char* rpc_reload_doc[2] = {
	"Reload global attributes from database",
	0
};

/*
 * Fifo function to reload domain table
 */
static void rpc_reload(rpc_t* rpc, void* ctx)
{
	if (reload_global_attributes() < 0) {
		LOG(L_ERR, "ERROR: Reloading of global_attrs table has failed\n");
		rpc->fault(ctx, 400, "Reloading of global attributes failed");
	}
	else {
		/* reload is successful */
		LOG(L_INFO, "INFO: global_attrs table reloaded\n");
	}
}

/*
 * RPC Methods exported by this module
 */
static rpc_export_t rpc_methods[] = {
	{"gflags.set",    rpc_set,    rpc_set_doc,    0},
	{"gflags.is_set", rpc_is_set, rpc_is_set_doc, 0},
	{"gflags.reset",  rpc_reset,  rpc_reset_doc,  0},
	{"gflags.flush",  rpc_flush,  rpc_flush_doc,  0},
	{"gflags.dump",   rpc_dump,   rpc_dump_doc,   0},
	{"global.reload", rpc_reload, rpc_reload_doc, 0},
	{0, 0, 0, 0}
};
