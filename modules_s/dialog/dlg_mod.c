/* 
 * Copyright (C) 2005 iptelorg GmbH
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

#include "dlg_mod.h"
#include "db_dlg.h"
#include "serialize_dlg.h"
#include "../../sr_module.h"
#include "../tm/tm_load.h"
#include <cds/sstr.h>

MODULE_VERSION

/* "public" data members */

int db_mode = 0;
str db_url = { 0, 0};

/* internal data members */

db_con_t* dlg_db = NULL; /* database connection handle */
db_func_t dlg_dbf;	/* database functions */
int db_initialized = 0;
struct tm_binds tmb;

static int bind_dlg_mod(dlg_func_t *dst);
static int dlg_mod_init(void);
static void dlg_mod_destroy(void);
static int dlg_mod_child_init(int _rank);

/*
 * Exported functions
 */
static cmd_export_t cmds[]={
	{"bind_dlg_mod", (cmd_function)bind_dlg_mod, -1, 0, 0},
	{0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[]={
	{"db_mode", INT_PARAM, &db_mode },
	{"db_url", STR_PARAM, &db_url.s },
	{0, 0, 0}
};


struct module_exports exports = {
	"dialog", 
	cmds,        /* Exported functions */
	params,      /* Exported parameters */
	dlg_mod_init, /* module initialization function */
	0,           /* response function*/
	dlg_mod_destroy,  /* destroy function */
	0,           /* oncancel function */
	dlg_mod_child_init/* per-child init function */
};

static int dlg_mod_init(void)
{
	load_tm_f load_tm;

	load_tm = (load_tm_f)find_export("load_tm", NO_SCRIPT, 0);
	if (!load_tm) {
		LOG(L_ERR, "dlg_mod_init(): Can't import tm\n");
		return -1;
	}
	if (load_tm(&tmb) < 0) {
		LOG(L_ERR, "dlg_mod_init(): Can't import tm functions\n");
		return -1;
	}
	
	db_initialized = 0;
	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	if (db_mode > 0) {
		if (!db_url.len) {
			LOG(L_ERR, "dlg_mod_init(): no db_url specified but use_db=1\n");
			db_mode = 0;
		}
	}
	if (db_mode > 0) {
		if (bind_dbmod(db_url.s, &dlg_dbf) < 0) {
			LOG(L_ERR, "dlg_mod_init(): Can't bind database module via url %s\n", db_url.s);
			return -1;
		}

		if (!DB_CAPABILITY(dlg_dbf, DB_CAP_ALL)) { /* ? */
			LOG(L_ERR, "dlg_mod_init(): Database module does not implement all functions needed by the module\n");
			return -1;
		}
		db_initialized = 1;
	}
	
	return 0;
}

db_con_t* create_dlg_db_connection()
{
	if (db_mode <= 0) return NULL;
	if (!dlg_dbf.init) return NULL;
	
	return dlg_dbf.init(db_url.s);
}

void close_dlg_db_connection(db_con_t* db)
{
	if (db && dlg_dbf.close) dlg_dbf.close(db);
}

static int dlg_mod_child_init(int _rank)
{
	if (db_mode > 0) {
		dlg_db = create_dlg_db_connection();
		if (!dlg_db) {
			LOG(L_ERR, "ERROR: dlg_child_init(%d): "
					"Error while connecting database\n", _rank);
			return -1;
		}
	}

	return 0;
}

static void dlg_mod_destroy(void)
{
	if (db_mode) close_dlg_db_connection(dlg_db);
	dlg_db = NULL;
}

static int bind_dlg_mod(dlg_func_t *dst)
{
	if (!dst) return -1;
/*	dst->db_store = db_store_dlg;
	dst->db_load = db_load_dlg;*/
	dst->serialize = serialize_dlg;
	dst->dlg2str = dlg2str;
	dst->str2dlg = str2dlg;
	return 0;
}

