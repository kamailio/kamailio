/*
 * Copyright (C) 2007 iptelorg GmbH
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

#include "../../sr_module.h"
#include "../../cfg/cfg.h"
#include "../../cfg/cfg_ctx.h"
#include "../../lib/srdb2/db.h"
#include "../../error.h"
#include "../../ut.h"
#include <unistd.h>

MODULE_VERSION

#define MODULE_NAME "cfg_db"

static pid_t db_pid = -1;
static db_ctx_t* db_cntx = NULL;
static cfg_ctx_t *cfg_ctx = NULL;

static char *db_url = DEFAULT_DB_URL; /* Database URL */

static char *transl_tbl = "cfg_transl";
static char *transl_group_name_fld = "group_name";
static char *transl_cfg_table_fld = "cfg_table";
static char *transl_cfg_table_group_name_field_fld = "cfg_table_group_name_field";
static char *transl_cfg_table_name_field_fld = "cfg_table_name_field";
static char *transl_cfg_table_value_field_fld = "cfg_table_value_field";
		
static char *def_cfg_table = "cfg_var";
static char *def_cfg_table_group_name_field = "group_name";
static char *def_cfg_table_name_field = "name";
static char *def_cfg_table_value_field = "value";

static char *custom_tbl = "cfg_custom";
static char *custom_tbl_group_name_fld = "group_name";
static char *custom_tbl_name_fld = "name";
static char *custom_tbl_value_type_fld = "value_type";
static char *custom_tbl_min_value_fld = "min_value";
static char *custom_tbl_max_value_fld = "max_value";
static char *custom_tbl_decription_fld = "description";

/* returns -2..error happend in the past, do not try connect again, -1 .. error, 0..OK */
static int connect_db() {
	if (db_pid != getpid()) {
		db_pid = getpid();
		db_cntx = db_ctx(MODULE_NAME);
		if (db_cntx == NULL) {
			ERR(MODULE_NAME": Error while initializing database layer\n");
			return -1;
		}	
		if (db_add_db(db_cntx, db_url) < 0) {
			ERR(MODULE_NAME": Error adding database '%s'\n", db_url);
			db_ctx_free(db_cntx);
			db_cntx = NULL;
			return -1;
		}		
		if (db_connect(db_cntx) < 0) {
			ERR(MODULE_NAME": Error connecting database '%s'\n", db_url);
			db_ctx_free(db_cntx);
			db_cntx = NULL;
			return -1;
		}
	}
	if (!db_cntx) return -2; /* database has not been connected */
	return 0;
}

static int exec_transl(str *group_name, db_cmd_t **cmd, db_res_t **res) {
	db_fld_t cols[5];
	db_fld_t params[2];

	memset(cols, 0, sizeof(cols));
	cols[0].name = transl_cfg_table_fld;
	cols[0].type = DB_CSTR;
	cols[1].name = transl_cfg_table_group_name_field_fld;
	cols[1].type = DB_CSTR;
	cols[2].name = transl_cfg_table_name_field_fld;
	cols[2].type = DB_CSTR;
	cols[3].name = transl_cfg_table_value_field_fld;
	cols[3].type = DB_CSTR;

	memset(params, 0, sizeof(params));
	params[0].name = transl_group_name_fld;
	params[0].type = DB_STR;
	params[0].op = DB_EQ;
		

	DBG(MODULE_NAME": exec_transl('%.*s', ...)\n", group_name->len, group_name->s);
	*cmd = db_cmd(DB_GET, db_cntx, transl_tbl, cols, params, NULL);
	if (!*cmd) {
		ERR(MODULE_NAME": Error preparing query '%s'\n", transl_tbl);
		return -1;		
	}
	(*cmd)->match[0].flags &= ~DB_NULL;
	(*cmd)->match[0].v.lstr = *group_name;

	// FIXME: proprietary code!
	db_setopt(*cmd, "key", "pKey");
	db_setopt(*cmd, "key_omit", 1);
	
	if (db_exec(res, *cmd) < 0) {
		ERR(MODULE_NAME": Error executing query '%s'\n", transl_tbl);
		db_cmd_free(*cmd);
		return -1;
	}
	return 0;
}

#define GETCSTR(fld,def) \
	((((fld).flags & DB_NULL) || (strlen(fld.v.cstr)== 0))?def:fld.v.cstr)
	
/* translate name using translation table, returns 0..not found, 1..success, -1..error */
static int find_cfg_var(str *group_name, char *def_name, db_res_t *transl_res) {
	
	db_rec_t *transl_rec;
	int ret = -1;

	DBG(MODULE_NAME": find_cfg_var('%.*s', '%s', ...)\n", group_name->len, group_name->s, def_name);
	transl_rec = db_first(transl_res);
	/* iterate through each candidate where cfg def may be found */
	while (transl_rec) {

		static db_cmd_t* cmd;
		db_rec_t *rec;
		db_res_t *res;
		db_fld_t params[3], cols[2];
		
		memset(cols, 0, sizeof(cols));
		cols[0].name = GETCSTR(transl_rec->fld[3], def_cfg_table_value_field);
		cols[0].type = DB_NONE;
		
		memset(params, 0, sizeof(params));
		params[0].name = GETCSTR(transl_rec->fld[1], def_cfg_table_group_name_field);
		params[0].type = DB_STR;
		params[0].op = DB_EQ;
		params[1].name = GETCSTR(transl_rec->fld[2], def_cfg_table_name_field);
		params[1].type = DB_CSTR;
		params[1].op = DB_EQ;

		DBG(MODULE_NAME": exec_transl: looking in '%s'\n", GETCSTR(transl_rec->fld[0], def_cfg_table));
		cmd = db_cmd(DB_GET, db_cntx, GETCSTR(transl_rec->fld[0], def_cfg_table), cols, params, NULL);
		if (!cmd) {
			ERR(MODULE_NAME": Error preparing query '%s'\n", transl_tbl);
			return -1;		
		}
		cmd->match[0].flags &= ~DB_NULL;
		cmd->match[0].v.lstr = *group_name;
		cmd->match[1].flags &= ~DB_NULL;
		cmd->match[1].v.cstr = def_name;
									
		// FIXME: proprietary code!
		db_setopt(cmd, "key", "bySerGroup");
		db_setopt(cmd, "key_omit", 0);
		
		if (db_exec(&res, cmd) < 0) {
			ERR(MODULE_NAME": Error executing query '%s'\n", transl_tbl);
			db_cmd_free(cmd);
			return -1;
		}
	
		rec = db_first(res);
		if (rec) { /* var found in config table */
			str def_name_s;
			def_name_s.s = def_name;
			def_name_s.len = strlen(def_name);
			DBG(MODULE_NAME": exec_transl: found record, type:%d\n", rec->fld[0].type);
			/* read and set cfg var */
			switch (rec->fld[0].type) {
				case DB_STR:
					if (cfg_set_now(cfg_ctx, group_name, NULL /* group id */, &def_name_s, &rec->fld[0].v.lstr, CFG_VAR_STR) < 0) goto err;
					break;
				case DB_CSTR:					
					if (cfg_set_now_string(cfg_ctx, group_name, NULL /* group id */, &def_name_s, rec->fld[0].v.cstr) < 0) goto err;
					break;
				case DB_INT:
					if (cfg_set_now_int(cfg_ctx, group_name, NULL /* group id */, &def_name_s, rec->fld[0].v.int4) < 0) goto err;
					break;
				default:
					ERR(MODULE_NAME": unexpected field type (%d), table:'%s', field:'%s'\n", 
						rec->fld[0].type, 
						GETCSTR(transl_rec->fld[0], def_cfg_table), 
						GETCSTR(transl_rec->fld[3], def_cfg_table_value_field)
					);
					goto err;
			}
			ret = 1;
		err:
			db_res_free(res);
			db_cmd_free(cmd);
			return ret;
		}
		db_res_free(res);
		db_cmd_free(cmd);
		
		transl_rec = db_next(transl_res);	
	}
	return 0;
}

/* callback function called once per cfg_group identified by group_name */
static void on_declare(str *group_name, cfg_def_t *definition) {
	static db_cmd_t* cmd;
	db_res_t *res;
	cfg_def_t *def;
	int ret;
	str asterisk_s = STR_STATIC_INIT("*");
	DBG(MODULE_NAME": on_declare('%.*s')\n", group_name->len, group_name->s);
	if (connect_db() < 0) return;
	
	for (def=definition; def->name; def++) {
		/* for each definition lookup config tables */
		if (exec_transl(group_name, &cmd, &res) < 0) return;
		ret = find_cfg_var(group_name, def->name, res);
		db_res_free(res);
		db_cmd_free(cmd);
		if (ret > 0) continue;

		/* not found then try default '*' translations */
		if (exec_transl(&asterisk_s, &cmd, &res) < 0) return;
		find_cfg_var(group_name, def->name, res);
		db_res_free(res);
		db_cmd_free(cmd);
	}
}

#define CSTRDUP(dest, fld) { \
	if (((fld).flags & DB_NULL) == 0) { \
		int n; \
		n = strlen((fld).v.cstr); \
		if (n > 0) {\
			(dest) = pkg_malloc(n+1); \
			if (!(dest)) return E_OUT_OF_MEM; \
			memcpy((dest), (fld).v.cstr, n+1); \
		} \
	} \
}

/* module initialization function */
static int mod_init(void) {
	static str default_s = STR_STATIC_INIT("<default>");
	db_cmd_t *cmd;			
	db_res_t *res;
	db_rec_t *rec;
	db_fld_t cols[7];

	DBG(MODULE_NAME": mod_init: initializing\n");

	/* get default values from translation table */
	if (connect_db() < 0) return E_CFG;

	DBG(MODULE_NAME": mod_init: getting default values from translation table\n");
	if (exec_transl(&default_s, &cmd, &res) < 0) return E_CFG;
	rec = db_first(res);
	if (rec) {
		CSTRDUP(def_cfg_table, rec->fld[0]);
		CSTRDUP(def_cfg_table_group_name_field, rec->fld[1]);
		CSTRDUP(def_cfg_table_name_field, rec->fld[2]);
		CSTRDUP(def_cfg_table_value_field, rec->fld[3]);
	}
//	db_rec_free(rec);  // ---> causes next db_cmd is aborted !!!
	db_res_free(res);
	db_cmd_free(cmd);
	
	DBG(MODULE_NAME": mod_init: default values: table='%s', group_name_field='%s', name_field='%s', value_field='%s'\n",
			def_cfg_table, def_cfg_table_group_name_field, def_cfg_table_name_field, def_cfg_table_value_field);

	/* get custom parameters from database */
	DBG(MODULE_NAME": mod_init: getting custom parameters from '%s'\n", custom_tbl);
	memset(cols, 0, sizeof(cols));
	cols[0].name = custom_tbl_group_name_fld;
	cols[0].type = DB_CSTR;
	cols[1].name = custom_tbl_name_fld;
	cols[1].type = DB_CSTR;
	cols[2].name = custom_tbl_value_type_fld;
	cols[2].type = DB_CSTR;
	cols[3].name = custom_tbl_min_value_fld;
	cols[3].type = DB_INT;
	cols[4].name = custom_tbl_max_value_fld;
	cols[4].type = DB_INT;
	cols[5].name = custom_tbl_decription_fld;
	cols[5].type = DB_CSTR;
	
	cmd = db_cmd(DB_GET, db_cntx, custom_tbl, cols, NULL, NULL);
	if (!cmd) {
		ERR(MODULE_NAME": Error preparing query '%s'\n", custom_tbl);
		return E_CFG;		
	}
								
	if (db_exec(&res, cmd) < 0) {
		ERR(MODULE_NAME": Error executing query '%s'\n", custom_tbl);
		db_cmd_free(cmd);
		return E_CFG;
	}
	rec = db_first(res);
	while (rec) {
		DBG(MODULE_NAME": custom parameter '%s.%s' type:%s\n", rec->fld[0].v.cstr, rec->fld[1].v.cstr, rec->fld[2].v.cstr);
		if (((rec->fld[0].flags & DB_NULL) || strlen(rec->fld[0].v.cstr) == 0) ||
			((rec->fld[1].flags & DB_NULL) || strlen(rec->fld[1].v.cstr) == 0) ||
			((rec->fld[2].flags & DB_NULL) || strlen(rec->fld[2].v.cstr) == 0)) {
			ERR(MODULE_NAME": empty group_name,name or type value in table '%s'\n", custom_tbl);
			return E_CFG;
		}
		switch (rec->fld[2].v.cstr[0]) {
			case 'i':
			case 'I':
				if (cfg_declare_int(rec->fld[0].v.cstr, rec->fld[1].v.cstr, 0, rec->fld[3].v.int4, rec->fld[4].v.int4, rec->fld[5].v.cstr) < 0) {
					ERR(MODULE_NAME": Error declaring cfg int '%s.%s'\n", rec->fld[0].v.cstr, rec->fld[1].v.cstr);
					return E_CFG;
				}
				break;
			case 's':
			case 'S':
				if (cfg_declare_str(rec->fld[0].v.cstr, rec->fld[1].v.cstr, "", rec->fld[5].v.cstr) < 0) {	
					ERR(MODULE_NAME": Error declaring cfg str '%s.%s'\n", rec->fld[0].v.cstr, rec->fld[1].v.cstr);
					return E_CFG;
				}
				break;
			default:
				ERR(MODULE_NAME": bad custom value type '%s'\n", rec->fld[2].v.cstr);
				return E_CFG;
		}
		
		rec = db_next(res);
	}
	db_res_free(res);
	db_cmd_free(cmd);

	/* register into config framework */
	DBG(MODULE_NAME": mod_init: registering cfg callback\n");
	if (cfg_register_ctx(&cfg_ctx, on_declare) < 0) {
		ERR(MODULE_NAME": failed to register cfg context\n");
		return -1;
	}

	return 0;
}

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",    PARAM_STRING, &db_url},
	{"transl_tbl", PARAM_STRING, &transl_tbl},
/*	{"transl_group_name_fld", PARAM_STRING, &transl_group_name_fld},
	{"transl_cfg_table_fld", PARAM_STRING, &transl_cfg_table_fld},
	{"transl_cfg_table_group_name_field_fld", PARAM_STRING, &transl_cfg_table_group_name_field_fld},
	{"transl_cfg_table_name_field_fld", PARAM_STRING, &transl_cfg_table_name_field_fld},
	{"transl_cfg_table_value_field_fld", PARAM_STRING, &transl_cfg_table_value_field_fld},
*/
	{"custom_tbl", PARAM_STRING, &custom_tbl},

	{0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
	MODULE_NAME,
	0,			/* Exported functions */
	0,			/* RPC methods */
	params,		/* Exported parameters */
	mod_init,	/* module initialization function */
	0,			/* response function */
	0,			/* destroy function */
	0,			/* oncancel function */
	0			/* child initialization function */
};

