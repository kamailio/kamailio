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

/** module functions */
static int mod_init(void);
static int child_init(int);

static int w_ds_select_dst(struct sip_msg*, char*, char*);

void destroy(void);

static int ds_fixup(void** param, int param_no);

static cmd_export_t cmds[]={
	{"ds_select_dst", w_ds_select_dst, 2, ds_fixup, REQUEST_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"list_file",      STR_PARAM, &dslistfile},
	{"force_dst",      INT_PARAM, &force_dst},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"dispatcher",
	cmds,
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
 * destroy function
 */
void destroy(void)
{
	DBG("DISPATCHER: destroy module ...\n");
	ds_destroy_list();
}

static int ds_fixup(void** param, int param_no)
{
	int n;
	int err;
	if(param_no==1 || param_no==2)
	{
		n = str2s(*param, strlen(*param), &err);
		if (err == 0)
		{
			pkg_free(*param);
			*param=(void*)n;
		} else {
			LOG(L_ERR, "DISPATCHER:ds_fixup: Bad number <%s>\n",
			    (char*)(*param));
			return E_UNSPEC;
		}
	}
	return 0;
}

