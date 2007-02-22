/**
 * $Id$
 *
 * dispatcher module -- stateless load balancing
 *
 * Copyright (C) 2004-2006 FhG Fokus
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

/**
 * History
 * -------
 * 2004-07-31  first version, by dcm
 *
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

#include "dispatch.h"

MODULE_VERSION

/** parameters */
char *dslistfile = CFG_DIR"dispatcher.list";
int  force_dst = 0;
int ds_flags   = 0;
static int ds_hash_no = 0; /* hash function number */

/** module functions */
static int mod_init(void);
static int child_init(int);

static int w_ds_select_dst(struct sip_msg*, char*, char*);
static int w_ds_select_new(struct sip_msg*, char*, char*);

void destroy(void);

static cmd_export_t cmds[]={
	{"ds_select_dst", w_ds_select_dst, 2, fixup_int_12, REQUEST_ROUTE},
	{"ds_select_new", w_ds_select_new, 2, fixup_int_12, REQUEST_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"list_file",      PARAM_STRING, &dslistfile},
	{"force_dst",      PARAM_INT,    &force_dst},
	{"flags",          PARAM_INT,    &ds_flags},
	{"hash",           PARAM_INT,    &ds_hash_no},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"dispatcher",
	cmds,
	0,          /* RPC methods */
	params,

	mod_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) destroy,
	0,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	DBG("DISPATCHER: initializing ...\n");

	if(ds_load_list(dslistfile)!=0)
	{
		LOG(L_ERR, "DISPATCHER:mod_init:ERROR -- couldn't load list file\n");
		return -1;
	}
	if (ds_set_hash_f(ds_hash_no)!=0){
		LOG(L_WARN, "WARNING: dispatcher: hash algorithm %d not defined, using"
					" the default\n", ds_hash_no);
	}
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
void destroy(void)
{
	DBG("DISPATCHER: destroy module ...\n");
	ds_destroy_list();
}

