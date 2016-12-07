/**
 * $Id$
 *
 * dispatcher module -- stateless load balancing
 *
 * Copyright (C) 2004-2006 FhG Fokus
 * Copyright (C) 2005-2008 Hendrik Scholz <hendrik.scholz@freenet-ag.de>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"

#include "dispatcher.h"
#include "ds_rpc.h"

MODULE_VERSION

/** parameters */
char *dslistfile = CFG_DIR"dispatcher.list";
int  force_dst = 0;
int ds_flags   = 0;
char *ds_activelist;
char ***ds_setp_a, ***ds_setp_b;
int **ds_setlen_a, **ds_setlen_b;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int w_ds_select_dst(struct sip_msg*, char*, char*);
static int w_ds_select_new(struct sip_msg*, char*, char*);

static void destroy(void);

static cmd_export_t cmds[]={
	{"ds_select_dst", w_ds_select_dst, 2, fixup_var_int_12, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_select_new", w_ds_select_new, 2, fixup_var_int_12, REQUEST_ROUTE|FAILURE_ROUTE},
	{0,0,0,0,0}
};

static param_export_t params[]={
	{"list_file",      PARAM_STRING, &dslistfile},
	{"force_dst",      PARAM_INT,    &force_dst},
	{"flags",          PARAM_INT,    &ds_flags},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"dispatcher",
	cmds,
	rpc_methods,	/* RPC methods */
	params,
	mod_init,	/* module initialization function */
	(response_function) 0,
	(destroy_function) destroy,
	0,
	child_init	/* per-child init function */
};

/******************************************************************************
 * init module function
 *
 * - init memory
 * - clear both 'memory pages'
 * - load dispatcher lists from file
 * - activate list
 *
 ******************************************************************************/
static int mod_init(void)
{
	DBG("DISPATCHER: initializing ...\n");

	if(ds_init_memory() != 0) {
		LOG(L_ERR, "DISPATCHER:mod_init:ERROR -- memory allocation error\n");
		return -1;
	}

	/* clean both lists */
	ds_clean_list();
	DS_SWITCH_ACTIVE_LIST
	ds_clean_list();

	if(ds_load_list(dslistfile)!=0)
	{
		LOG(L_ERR, "DISPATCHER:mod_init:ERROR -- couldn't load list file\n");
		return -1;
	}
	/* switch active list since we had the offline one prepared */
	DS_SWITCH_ACTIVE_LIST

	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	DBG("DISPATCHER:init_child #%d / pid <%d>\n", rank, getpid());
	return 0;
}

/**
 *
 */
static int w_ds_select_dst(struct sip_msg* msg, char* set, char* alg)
{
	if(msg==NULL)
		return -1;

	return ds_select_dst(msg, set, alg);
}

/**
 *
 */
static int w_ds_select_new(struct sip_msg* msg, char* set, char* alg)
{
	if(msg==NULL)
		return -1;

	return ds_select_new(msg, set, alg);
}

/**
 * destroy function
 */
static void destroy(void)
{
	DBG("DISPATCHER: destroy module ...\n");
	ds_destroy_lists();
}
