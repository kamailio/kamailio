/*$Id$
 *
 * debug and test ser module
 *
 *
 * Copyright (C) 2007 iptelorg GmbH
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
 *  2007-02-27  created by andrei
 */




#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <stdlib.h>

MODULE_VERSION

static int dbg_msleep(struct sip_msg*, char*,char*);
static int dbg_abort(struct sip_msg*, char*,char*);
static int dbg_pkg_status(struct sip_msg*, char*,char*);
static int dbg_shm_status(struct sip_msg*, char*,char*);
static int mod_init(void);


static cmd_export_t cmds[]={
	{"dbg_msleep", dbg_msleep, 1, fixup_int_1, 
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"dbg_abort", dbg_abort, 0, 0, 
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"dbg_pkg_status", dbg_pkg_status, 0, 0, 
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"dbg_shm_status", dbg_shm_status, 0, 0, 
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{0,0,0}
};

struct module_exports exports = {
	"dbg",
	cmds,
	0,        /* RPC methods */
	params,

	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	LOG(L_WARN, "WARNING: dbg module loaded\n");
	return 0;
}


static int dbg_msleep(struct sip_msg* msg, char* val, char* foo)
{
	int ms;
	fparam_t* p;
	p=(fparam_t*)val;
	ms=p->v.i;
	LOG(L_CRIT, "dbg: sleeping %d ms\n", ms);
	usleep(ms*1000);
	return 1;
}



static int dbg_abort(struct sip_msg* msg, char* foo, char* bar)
{
	LOG(L_CRIT, "dbg: dbg_abort called\n");
	abort();
	return 0;
}



static int dbg_pkg_status(struct sip_msg* msg, char* foo, char* bar)
{
	pkg_status();
	return 1;
}



static int dbg_shm_status(struct sip_msg* msg, char* foo, char* bar)
{
	shm_status();
	return 1;
}


