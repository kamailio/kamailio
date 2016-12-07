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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "dlg_mod.h"
#include "db_dlg.h"
#include "serialize_dlg.h"
#include "../../sr_module.h"
#include "../../modules/tm/tm_load.h"
#include <cds/sstr.h>
#include "dlg_utils.h"
#include "dlg_request.h"
#include "../../locking.h"

MODULE_VERSION

/* "public" data members */

static int db_mode = 0;
static str db_url = STR_NULL;

/* internal data members */

/* data members for pregenerated tags - taken from TM */
char dialog_tags[TOTAG_VALUE_LEN];
char *dialog_tag_suffix = NULL;

struct tm_binds tmb;

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
	{"db_mode", PARAM_INT, &db_mode },
	{"db_url",  PARAM_STR, &db_url },
	{0, 0, 0}
};


struct module_exports exports = {
	"dialog",
	cmds,        /* Exported functions */
	0,           /* RPC methods */
	params,      /* Exported parameters */
	dlg_mod_init, /* module initialization function */
	0,           /* response function*/
	dlg_mod_destroy,  /* destroy function */
	0,           /* oncancel function */
	dlg_mod_child_init/* per-child init function */
};

static void init_dialog_tags()
{
	/* taken from tm, might be useful */
	init_tags(dialog_tags, &dialog_tag_suffix, "SER-DIALOG/tags", '-');
}

#include <cds/dstring.h>

gen_lock_t *dlg_mutex = NULL; 

static int init_dialog_mutex()
{
	dlg_mutex = (gen_lock_t*)shm_malloc(sizeof(*dlg_mutex));
	if (!dlg_mutex) return -1;
	lock_init(dlg_mutex);
	return 0;
}

static void destroy_dialog_mutex()
{
	if (dlg_mutex) {
		lock_destroy(dlg_mutex);
		shm_free((void*)dlg_mutex);
	}
}

static int dlg_mod_init(void)
{
	load_tm_f load_tm;
	
	if (init_dialog_mutex() < 0) {
		ERR("can't initialize mutex\n");
		return -1;
	}
	
	init_dialog_tags();

	load_tm = (load_tm_f)find_export("load_tm", NO_SCRIPT, 0);
	if (!load_tm) {
		LOG(L_ERR, "dlg_mod_init(): Can't import tm\n");
		return -1;
	}
	if (load_tm(&tmb) < 0) {
		LOG(L_ERR, "dlg_mod_init(): Can't import tm functions\n");
		return -1;
	}

	return 0;
}

static int dlg_mod_child_init(int _rank)
{
	return 0;
}

static void dlg_mod_destroy(void)
{
	destroy_dialog_mutex();
}

