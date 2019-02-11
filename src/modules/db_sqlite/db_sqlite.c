/*
 * $Id$
 *
 * SQlite module interface
 *
 * Copyright (C) 2010 Timo Teräs
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

#include <sys/time.h>
#include <sqlite3.h>

#include "../../core/sr_module.h"
#include "../../lib/srdb1/db_query.h"
#include "../../lib/srdb1/db.h"
#include "../../core/parser/parse_param.h"
#include "dbase.h"
#include "db_sqlite.h"

MODULE_VERSION

static int sqlite_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table		= db_sqlite_use_table;
	dbb->init		= db_sqlite_init;
	dbb->close		= db_sqlite_close;
	dbb->free_result	= db_sqlite_free_result;
	dbb->query		= db_sqlite_query;
	dbb->insert		= db_sqlite_insert;
	dbb->delete		= db_sqlite_delete;
	dbb->update		= db_sqlite_update;
	dbb->raw_query		= db_sqlite_raw_query;

	return 0;
}

static db_param_list_t *db_param_list = NULL;

static void db_param_list_add(db_param_list_t *e) {
	if (!db_param_list) {
		db_param_list = e;
		LM_DBG("adding database params [%s]\n", e->database.s);
		clist_init(db_param_list, next, prev);
	} else {
		LM_DBG("append database params [%s]\n", e->database.s);
		clist_append(db_param_list, e, next, prev);
	}
}

static void db_param_list_destroy(db_param_list_t *e) {
	if (!e)
		return;
	if (e->database.s)
		pkg_free(e->database.s);
	if (e->journal_mode.s)
		pkg_free(e->journal_mode.s);
	pkg_free(e);
	e = NULL;
}

static db_param_list_t *db_param_list_new(const char *db_filename) {
	db_param_list_t *e = pkg_malloc(sizeof(db_param_list_t));
	if (!e)
		return NULL;
	memset(e, 0, sizeof(db_param_list_t));

	e->database.len = strlen(db_filename);
	e->database.s = pkg_malloc(e->database.len+1);
	if (!e->database.s) goto error;
	strcpy(e->database.s, db_filename);

	db_param_list_add(e);
	return e;
error:
	db_param_list_destroy(e);
	return NULL;
}

db_param_list_t *db_param_list_search(str db_filename) {
	db_param_list_t *e;
	if (!db_param_list) {
		return NULL;
	}
	if (strncmp(db_filename.s, db_param_list->database.s, db_filename.len) == 0) {
		return db_param_list;
	}
	clist_foreach(db_param_list, e, next){
		if (strncmp(db_filename.s, e->database.s, db_filename.len) == 0) {
			return e;
		}
	}
	return NULL;
}

static int db_set_journal_mode_entry(str db_filename, str journal_mode) {
	if(!db_filename.s || !journal_mode.s)
		return -1;
	db_param_list_t *e = db_param_list_search(db_filename);
	if (!e)
		e = db_param_list_new(db_filename.s);
	if (!e) {
		LM_ERR("can't create a new db_param for [%s]\n", db_filename.s);
		return -1;
	}
	e->journal_mode.s = pkg_malloc(journal_mode.len+1);
	if (!e->journal_mode.s) goto error;
	strncpy(e->journal_mode.s, journal_mode.s, journal_mode.len);
	e->journal_mode.len = journal_mode.len;
	e->journal_mode.s[e->journal_mode.len] = '\0';
	return 1;
	error:
		db_param_list_destroy(e);
		return -1;
}

int db_set_journal_mode(modparam_t type, void *val) {
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	str s;

	if (val==NULL)
		return -1;

	s.s = (char*)val;
	s.len = strlen(s.s);
	if (s.len<=0)
		return -1;
	if (s.s[s.len-1]==';')
		s.len--;

	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0)
		goto error;
	// PRAGMA schema.journal_mode = DELETE | TRUNCATE | PERSIST | MEMORY | WAL | OFF
	for (pit = params_list; pit; pit=pit->next) {
		LM_DBG("[param][%.*s]\n", pit->name.len, pit->name.s);
		if ( pit->body.len==3 && strncasecmp(pit->body.s,"WAL", 3) ) {
			db_set_journal_mode_entry(pit->name, pit->body);
		} else if ( pit->body.len==6 && strncasecmp(pit->body.s,"DELETE", 6) ) {
			db_set_journal_mode_entry(pit->name, pit->body);
		} else if ( pit->body.len==8 && strncasecmp(pit->body.s,"TRUNCATE", 8) ) {
			db_set_journal_mode_entry(pit->name, pit->body);
		} else if ( pit->body.len==7 && strncasecmp(pit->body.s,"PERSIST", 7) ) {
			db_set_journal_mode_entry(pit->name, pit->body);
		} else if ( pit->body.len==6 && strncasecmp(pit->body.s,"MEMORY", 6) ) {
			db_set_journal_mode_entry(pit->name, pit->body);
		} else if ( pit->body.len==3 && strncasecmp(pit->body.s,"OFF", 3) ) {
			db_set_journal_mode_entry(pit->name, pit->body);
		}
	}

	if(params_list!=NULL)
		free_params(params_list);
	return 1;
error:
	if(params_list!=NULL)
		free_params(params_list);
	return -1;
}

int db_set_readonly(modparam_t type, void *val) {
	if(val==NULL)
		return -1;
	str db_name = str_init((char*) val);
	db_param_list_t *e = db_param_list_search(db_name);
	if (!e)
		e = db_param_list_new(db_name.s);
	if (!e) {
		LM_ERR("can't create a new db_param for [%s]\n", (char*) val);
		return -1;
	}
	e->readonly = 1;
	return 1;
}

static param_export_t params[] = {
	{"db_set_readonly",  PARAM_STRING|USE_FUNC_PARAM, (void*)db_set_readonly},
	{"db_set_journal_mode",  PARAM_STRING|USE_FUNC_PARAM, (void*)db_set_journal_mode},
	{0,0,0}
};

static cmd_export_t cmds[] = {
	{"db_bind_api", (cmd_function)sqlite_bind_api, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(db_api_init()<0)
		return -1;
	return 0;
}

static int mod_init(void)
{
	sqlite3_initialize();

	LM_INFO("SQlite library version %s (compiled using %s)\n",
		sqlite3_libversion(),
		SQLITE_VERSION);
	return 0;
}


static void mod_destroy(void)
{
	LM_INFO("SQlite terminate\n");

	sqlite3_shutdown();
}

struct module_exports exports = {
	"db_sqlite",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* module commands */
	params,			/* module parameters */
	0,			/* exported·RPC·methods· */
	0,			/* exported pseudo-variables */
	0,			/* response function */
	mod_init,		/* module initialization function */
	0,			/* per-child init function */
	mod_destroy		/* destroy function */
};
